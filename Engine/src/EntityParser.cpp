#include "veex/EntityParser.h"
#include <sstream>
#include <cstring>

namespace veex {

// ---------------------------------------------------------------------------
// EntityData
// ---------------------------------------------------------------------------

glm::vec3 EntityData::GetOrigin(float kScale) const
{
    auto it = kv.find("origin");
    if (it == kv.end()) return glm::vec3(0.0f);

    // Source stores origin as "X Y Z" where Z is up.
    // We convert to GL space: (srcX, srcZ, -srcY) and apply scale.
    std::istringstream ss(it->second);
    float sx = 0, sy = 0, sz = 0;
    ss >> sx >> sy >> sz;

    return glm::vec3(sx * kScale, sz * kScale, -sy * kScale);
}

float EntityData::GetFloat(const std::string& key, float def) const
{
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    try { return std::stof(it->second); } catch (...) { return def; }
}

int EntityData::GetInt(const std::string& key, int def) const
{
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}

glm::vec3 EntityData::GetAngles() const
{
    auto it = kv.find("angles");
    if (it == kv.end()) return glm::vec3(0.0f);
    std::istringstream ss(it->second);
    float p = 0, y = 0, r = 0;
    ss >> p >> y >> r;
    return glm::vec3(p, y, r); // pitch, yaw, roll — raw degrees
}

// ---------------------------------------------------------------------------
// EntityParser  (mirrors MapEntity_ParseToken + MapEntity_ParseAllEntities)
// ---------------------------------------------------------------------------

// Equivalent to Source's MapEntity_ParseToken.
// Handles: whitespace skipping, // comments, quoted strings, single brace chars.
std::string EntityParser::NextToken(const std::string& data, size_t& pos)
{
    const size_t len = data.size();

    // Skip whitespace and // line comments
    while (pos < len) {
        // Skip whitespace
        while (pos < len && (unsigned char)data[pos] <= ' ') pos++;
        if (pos >= len) return "";

        // Skip // comments (Source uses these in entity lumps occasionally)
        if (pos + 1 < len && data[pos] == '/' && data[pos + 1] == '/') {
            while (pos < len && data[pos] != '\n') pos++;
            continue;
        }
        break;
    }

    if (pos >= len) return "";

    // Quoted string — most values and keys in the entity lump are quoted
    if (data[pos] == '"') {
        pos++; // skip opening quote
        std::string token;
        while (pos < len && data[pos] != '"') {
            token += data[pos++];
        }
        if (pos < len) pos++; // skip closing quote
        return token;
    }

    // Single brace character — { } ( ) are always returned alone
    static const char kBraceChars[] = "{}()'";
    if (std::strchr(kBraceChars, data[pos])) {
        return std::string(1, data[pos++]);
    }

    // Unquoted token (rare in entity lumps, but handle it anyway)
    std::string token;
    while (pos < len && (unsigned char)data[pos] > ' '
           && !std::strchr(kBraceChars, data[pos])) {
        token += data[pos++];
    }
    return token;
}

// Equivalent to MapEntity_ParseAllEntities + MapEntity_ParseEntity.
// Iterates the plain-text entity lump, collecting one EntityData per { }.
std::vector<EntityData> EntityParser::Parse(const std::string& data)
{
    std::vector<EntityData> entities;
    size_t pos = 0;

    while (pos < data.size()) {
        std::string token = NextToken(data, pos);
        if (token.empty()) break;

        // Each entity block starts with '{'
        if (token != "{") continue;

        EntityData ent;

        // Read key-value pairs until we hit the closing '}'
        while (pos < data.size()) {
            std::string key = NextToken(data, pos);
            if (key == "}" || key.empty()) break;

            std::string val = NextToken(data, pos);
            // Source strips everything after '#' in key names
            // (used to allow duplicate keys e.g. output#42)
            auto hash = key.find('#');
            if (hash != std::string::npos) key = key.substr(0, hash);

            ent.kv[key] = val;
        }

        entities.push_back(std::move(ent));
    }

    return entities;
}

} // namespace veex