#include "veex/Server.h"
#include "veex/Logger.h"
#include "veex/EntityParser.h"
#include "veex/BinfoParser.h"
#include "veex/FileSystem.h"
#include "veex/Config.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace veex {

// ── CONSTANTS & HELPERS ───────────────────────────────────────────────────────

static constexpr float HAMMER_TO_METERS = 1.0f / 32.0f;

static inline glm::vec3 ParseSourceVector(const std::string& s) {
    if (s.empty()) return glm::vec3(0.0f);
    std::istringstream ss(s);
    float x, y, z;
    ss >> x >> y >> z;
    // Source (X, Y, Z-Up) -> GL (X, Z, -Y-Forward)
    return glm::vec3(x * HAMMER_TO_METERS, z * HAMMER_TO_METERS, -y * HAMMER_TO_METERS);
}

static inline glm::vec3 ParseSourceAngles(const std::string& s) {
    if (s.empty()) return glm::vec3(0.0f);
    std::istringstream ss(s);
    float p, y, r;
    ss >> p >> y >> r;
    return glm::vec3(p, y, r);
}

// ── SERVER CORE ──────────────────────────────────────────────────────────────

Server::Server() 
    : m_simTime(0.0)
    , m_nextEntityID(1)
    , m_spawnPoint(0.0f)
{
}

Server::~Server() {
    Shutdown();
}

bool Server::Init(const std::string& entityData, const std::string& binfoPath, const GameInfo& game)
{
    Logger::Info("Server: Starting simulation sequence...");
    
    // 1. Parse Raw Lumps
    EntityParser parser;
    m_parsedEntities = parser.Parse(entityData);
    if (m_parsedEntities.empty()) {
        Logger::Error("Server: Entity lump is empty.");
        return false;
    }

    // 2. Load Binfo (Wiring Table)
    if (!BinfoParser::LoadFromFile(binfoPath, game, m_binfoTable)) {
        Logger::Warn("Server: Binfo load failed. Logic connections will be inactive.");
    }

    // 3. Process World properties
    ProcessWorldspawn();

    // 4. Primary Entity Spawning
    for (const auto& ed : m_parsedEntities) {
        SpawnEntity(ed);
    }

    // 5. Initial Logic Pass
    InitializeLogicSystem();

    // 6. Developer Tools
    SpawnEditorTools();

    Logger::Info("Server: Live. Entities: " + std::to_string(m_entities.size()));
    return true;
}

void Server::ProcessWorldspawn()
{
    for (const auto& ed : m_parsedEntities) {
        if (GetKV(ed, "classname") == "worldspawn") {
            m_skyName = GetKV(ed, "skyname");
            if (m_skyName.empty()) m_skyName = "sky_day01_01";
            Logger::Info("Server: Skybox set to " + m_skyName);
            return;
        }
    }
}

void Server::InitializeLogicSystem()
{
    RouteOutputs();
    for (auto& ent : m_entities) {
        if (ent->GetClassname() == "logic_auto") {
            ent->AcceptInput("OnMapSpawn", "", nullptr);
        }
    }
}

void Server::SpawnEditorTools()
{
    auto devCube = std::make_shared<Entity>(m_nextEntityID++, "dev_marker");
    devCube->SetPosition(m_spawnPoint + glm::vec3(0, 2, -5));
    devCube->SetTargetName("editor_marker_01");
    AddEntity(devCube);
}

void Server::Tick(double dt)
{
    m_simTime += dt;

    for (auto& ent : m_entities) {
        if (ent && ent->IsActive()) {
            ent->Update(static_cast<float>(dt));
        }
    }

    RouteOutputs();
    CleanupEntities();
}

void Server::RouteOutputs()
{
    for (auto& ent : m_entities) {
        if (!ent) continue;

        auto& pending = ent->GetPendingOutputs();
        if (pending.empty()) continue;

        auto itTable = m_binfoTable.find(ent->GetClassname());
        if (itTable == m_binfoTable.end()) {
            pending.clear();
            continue;
        }

        const auto& connections = itTable->second;
        for (const auto& fired : pending) {
            for (const auto& conn : connections) {
                if (conn.outputName == fired.outputName) {
                    Entity* target = ResolveTarget(conn.targetName, ent.get());
                    if (target) {
                        target->AcceptInput(conn.inputName, conn.param, ent.get());
                        
                        // FIX: Changed Logger::Trace to Logger::Info
                        Logger::Info("IO: " + ent->GetClassname() + " [" + fired.outputName + "] -> " + 
                                     target->GetClassname() + " (" + target->GetTargetName() + ") [" + conn.inputName + "]");
                    }
                }
            }
        }
        pending.clear();
    }
}

bool Server::SpawnEntity(const EntityData& ed)
{
    std::string classname = GetKV(ed, "classname");
    if (classname.empty() || classname == "worldspawn") return false;

    glm::vec3 origin = ParseSourceVector(GetKV(ed, "origin"));
    glm::vec3 angles = ParseSourceAngles(GetKV(ed, "angles"));
    std::string targetname = GetKV(ed, "targetname");

    // Use the factory if the class is registered
    std::shared_ptr<Entity> newEnt = EntityFactory::Create(classname, m_nextEntityID++);
    
    // Fallback to generic entity if not in factory
    if (!newEnt) {
        newEnt = std::make_shared<Entity>(m_nextEntityID - 1, classname);
    }

    if (classname.find("info_player_") != std::string::npos || classname == "info_player_start") {
        m_spawnPoint = origin;
    }

    if (newEnt) {
        newEnt->SetPosition(origin);
        newEnt->SetAngles(angles);
        newEnt->SetTargetName(targetname);
        
        for(auto const& [key, val] : ed.kv) {
            newEnt->SetRawKV(key, val);
        }

        AddEntity(newEnt);
        return true;
    }

    return false;
}

Entity* Server::ResolveTarget(const std::string& targetName, Entity* self) const
{
    if (targetName.empty()) return nullptr;
    if (targetName == "!self" || targetName == "!activator") return self;

    for (const auto& ent : m_entities) {
        if (ent && ent->GetTargetName() == targetName)
            return ent.get();
    }
    return nullptr;
}

void Server::AddEntity(std::shared_ptr<Entity> entity)
{
    if (entity) m_entities.push_back(std::move(entity));
}

void Server::RemoveEntity(int entityID)
{
    for (auto& ent : m_entities) {
        if (ent && ent->GetID() == entityID) {
            ent->MarkForDeletion();
            return;
        }
    }
}

void Server::CleanupEntities()
{
    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
            [](const std::shared_ptr<Entity>& e) {
                return !e || e->IsPendingDeletion();
            }),
        m_entities.end()
    );
}

std::string Server::GetKV(const EntityData& ed, const std::string& key) const
{
    auto it = ed.kv.find(key);
    return (it != ed.kv.end()) ? it->second : "";
}

const std::vector<std::shared_ptr<Entity>>& Server::GetEntities() const
{
    return m_entities;
}

void Server::Shutdown()
{
    Logger::Info("Server: Shutting down...");
    m_entities.clear();
    m_parsedEntities.clear();
    m_binfoTable.clear();
    m_simTime = 0.0;
}

std::string Server::GetSkyName() const { return m_skyName; }
glm::vec3 Server::GetSpawnPoint() const { return m_spawnPoint; }
double Server::GetSimTime() const { return m_simTime; }

} // namespace veex