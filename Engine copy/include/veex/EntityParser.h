#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

namespace veex {

// Mirrors Source's per-entity keyvalue block.
// All values are stored as raw strings exactly as they appear in the BSP
// entity lump — no conversion is done at parse time.
// Conversion to typed values (vec3, float, int) happens at point of use,
// matching how CBaseEntity::KeyValue works in the Source engine.
struct EntityData {
    std::unordered_map<std::string, std::string> kv;

    // Returns true if the key exists.
    bool HasKey(const std::string& key) const {
        return kv.count(key) > 0;
    }

    // Raw string value, empty string if missing.
    const std::string& GetString(const std::string& key) const {
        static const std::string empty;
        auto it = kv.find(key);
        return it != kv.end() ? it->second : empty;
    }

    // Parse "X Y Z" in Source space and convert to GL space at kScale.
    // Source: X=right, Y=forward, Z=up
    // GL:     X=right, Y=up,     Z=-forward
    // Scale:  1 Source unit = kScale GL units  (0.03125f = 1/32)
    glm::vec3 GetOrigin(float kScale = 0.03125f) const;

    // Parse a single float.
    float GetFloat(const std::string& key, float def = 0.0f) const;

    // Parse a single int.
    int GetInt(const std::string& key, int def = 0) const;

    // Parse "P Y R" Euler angles (degrees) — stored as pitch/yaw/roll
    // in Source order. Returns raw as vec3(pitch, yaw, roll).
    glm::vec3 GetAngles() const;
};

// Mirrors Source's MapEntity_ParseAllEntities / MapEntity_ParseToken logic.
// Reads the plain-text entity lump and produces one EntityData per entity.
class EntityParser {
public:
    static std::vector<EntityData> Parse(const std::string& data);

private:
    // Equivalent to MapEntity_ParseToken: skips whitespace and comments,
    // handles quoted strings and single brace characters.
    static std::string NextToken(const std::string& data, size_t& pos);
};

} // namespace veex