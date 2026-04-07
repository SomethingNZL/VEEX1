#include "veex/Server.h"
#include "veex/Logger.h"
#include "veex/EntityParser.h"

namespace veex {

Server::Server() : m_simTime(0.0) {}

bool Server::Init(const std::string& entityData) {
    Logger::Info("Server: Initializing...");

    m_simTime = 0.0;
    m_entities.clear();
    m_spawnPoint = glm::vec3(0.0f);

    std::vector<EntityData> parsedEnts = EntityParser::Parse(entityData);
    bool foundSpawn = false;

    for (const auto& ed : parsedEnts) {
        const std::string& classname = ed.GetString("classname");

        // 1. Worldspawn properties
        if (classname == "worldspawn") {
            if (ed.HasKey("skyname")) {
                m_skyName = ed.GetString("skyname");
                Logger::Info("Server: Sky: " + m_skyName);
            }
        }
        // 2. Info Player Start (Highest Priority)
        else if (classname == "info_player_start") {
            m_spawnPoint = ed.GetOrigin();
            foundSpawn = true;
            Logger::Info("Server: Spawn (info_player_start) -> " +
                std::to_string(m_spawnPoint.x) + " " +
                std::to_string(m_spawnPoint.y) + " " +
                std::to_string(m_spawnPoint.z));
            break; 
        }
        // 3. Team Spawns (Secondary Priority)
        else if (!foundSpawn &&
                 (classname == "info_player_counterterrorist" ||
                  classname == "info_player_terrorist")) {
            m_spawnPoint = ed.GetOrigin();
            foundSpawn = true;
            Logger::Info("Server: Spawn (" + classname + ") -> " +
                std::to_string(m_spawnPoint.x) + " " +
                std::to_string(m_spawnPoint.y) + " " +
                std::to_string(m_spawnPoint.z));
        }
    }

    if (!foundSpawn) {
        Logger::Warn("Server: No spawn entity found. Using (0, 1.5, 0).");
        m_spawnPoint = glm::vec3(0.0f, 1.5f, 0.0f);
    }

    // Server-side test entity (Now visible via the header)
    auto test = std::make_shared<TestEntity>(1);
    test->SetPosition(m_spawnPoint + glm::vec3(0.0f, 2.0f, -5.0f));
    AddEntity(test);

    Logger::Info("Server: Init complete.");
    return true;
}

void Server::Tick(double dt) {
    m_simTime += dt;
    for (auto& entity : m_entities) {
        if (entity) {
            entity->Update(static_cast<float>(dt));
        }
    }
}

void Server::AddEntity(std::shared_ptr<Entity> entity) {
    m_entities.push_back(entity);
}

const std::vector<std::shared_ptr<Entity>>& Server::GetEntities() const {
    return m_entities;
}

void Server::Shutdown() {
    m_entities.clear();
    Logger::Info("Server: Shutdown.");
}

} // namespace veex