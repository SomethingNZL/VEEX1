#pragma once
// PropStaticEntity.h
// Entity type for static (non-animated) props.
// ---------------------------------------------------------------
// Supports:
//   - Model path reference
//   - Skin selection
//   - Solid type configuration
//   - Model scaling
// ---------------------------------------------------------------

#include "veex/Entity.h"
#include "veex/EntityParser.h"
#include "veex/Logger.h"
#include <string>

namespace veex {

class PropStaticEntity : public Entity {
public:
    explicit PropStaticEntity(int id) : Entity(id, "prop_static") {}
    virtual ~PropStaticEntity() = default;

    bool Spawn(const EntityData& ed) override;

    const std::string& GetModelPath() const { return m_modelPath; }
    int                GetSkin()      const { return m_skin; }
    int                GetSolid()     const { return m_solid; }
    float              GetScale()     const { return m_scale; }

    void Draw() const;

private:
    std::string m_modelPath;
    int         m_skin  = 0;
    int         m_solid = 6;
    float       m_scale = 1.0f;
};

} // namespace veex