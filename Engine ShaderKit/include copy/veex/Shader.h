#pragma once
#include <string>
#include <unordered_map>

namespace veex {

class Shader {
public:
    Shader();
    ~Shader();

    bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    void Use() const;

    // Generic SetUniforms
    void SetUniform(const std::string& name, int value);
    void SetUniform(const std::string& name, float value);
    void SetUniform(const std::string& name, float x, float y);
    void SetUniform(const std::string& name, float x, float y, float z);
    void SetUniform(const std::string& name, float x, float y, float z, float w);
    void SetUniform(const std::string& name, const float* matrix);

    // Convenience Aliases (To fix the Renderer.cpp errors)
    void SetInt(const std::string& name, int value) { SetUniform(name, value); }
    void SetFloat(const std::string& name, float value) { SetUniform(name, value); }
    void SetMat4(const std::string& name, const float* matrix) { SetUniform(name, matrix); }

private:
    unsigned int m_program = 0;
    std::unordered_map<std::string, int> m_uniformLocations;

    bool CompileShader(unsigned int& shader, const std::string& source, unsigned int type);
    bool LinkProgram();
    int GetUniformLocation(const std::string& name);
};

} // namespace veex