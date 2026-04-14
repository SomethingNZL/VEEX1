#include "veex/PropStaticEntity.h"
#include "veex/Logger.h"

namespace veex {

// Entity factory registration - only one per type
VEEX_REGISTER_ENTITY("prop_static", PropStaticEntity);

bool PropStaticEntity::Spawn(const EntityData& ed) {
    m_modelPath = ed.GetString("model");
    m_skin      = ed.GetInt("skin", 0);
    m_solid     = ed.GetInt("solid", 6);
    m_scale     = ed.GetFloat("scale", 1.0f);

    if (m_modelPath.empty()) {
        Logger::Warn("prop_static id=" + std::to_string(m_id) + " has no model KV — discarding.");
        return false;
    }

    Logger::Info("prop_static: model='" + m_modelPath
        + "' skin=" + std::to_string(m_skin)
        + " solid=" + std::to_string(m_solid)
        + " scale=" + std::to_string(m_scale));

    // Model loading will be handled by the renderer
    // The renderer will check the file extension and load accordingly

    return true;
}

void PropStaticEntity::Draw() const {
    // Rendering is handled by the Renderer class
    // This method is a placeholder for future implementation
    // The actual drawing will use the Model class and proper transformation matrices
}

} // namespace veex