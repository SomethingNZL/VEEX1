// Expand entity I/O system
void Entity::FireOutput(const std::string& outputName, const std::string& targetName) {
    auto it = m_outputs.find(outputName);
    if (it != m_outputs.end()) {
        for (const auto& action : it->second) {
            if (action.target == targetName) {
                action.callback();
            }
        }
    }
}

void Entity::AddOutput(const std::string& outputName, const std::string& targetName, std::function<void()> callback) {
    m_outputs[outputName].push_back({targetName, callback});
}