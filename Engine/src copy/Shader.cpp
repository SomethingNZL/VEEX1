#include "veex/Shader.h"
#include "veex/Logger.h"
#include <glad/gl.h>
#include <fstream>
#include <sstream>
#include <vector>

namespace veex {

Shader::Shader() = default;

Shader::~Shader() {
    if (m_program) glDeleteProgram(m_program);
}

bool Shader::LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::ifstream vStream(vertexPath), fStream(fragmentPath);
    if (!vStream.is_open() || !fStream.is_open()) {
        Logger::Error("Failed to open shader files: " + vertexPath + ", " + fragmentPath);
        return false;
    }

    std::stringstream vBuffer, fBuffer;
    vBuffer << vStream.rdbuf();
    fBuffer << fStream.rdbuf();

    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
        m_uniformLocations.clear();
    }

    unsigned int vShader, fShader;
    if (!CompileShader(vShader, vBuffer.str(), GL_VERTEX_SHADER)) return false;
    if (!CompileShader(fShader, fBuffer.str(), GL_FRAGMENT_SHADER)) {
        glDeleteShader(vShader);
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vShader);
    glAttachShader(m_program, fShader);

    if (!LinkProgram()) {
        glDeleteShader(vShader); glDeleteShader(fShader);
        glDeleteProgram(m_program); m_program = 0;
        return false;
    }

    glDeleteShader(vShader);
    glDeleteShader(fShader);
    Logger::Info("Shader loaded successfully: " + vertexPath);
    return true;
}

void Shader::Use() const { glUseProgram(m_program); }

void Shader::SetUniform(const std::string& name, int value) {
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetUniform(const std::string& name, float value) {
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetUniform(const std::string& name, float x, float y) {
    glUniform2f(GetUniformLocation(name), x, y);
}

void Shader::SetUniform(const std::string& name, float x, float y, float z) {
    glUniform3f(GetUniformLocation(name), x, y, z);
}

void Shader::SetUniform(const std::string& name, float x, float y, float z, float w) {
    glUniform4f(GetUniformLocation(name), x, y, z, w);
}

void Shader::SetUniform(const std::string& name, const float* matrix) {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, matrix);
}

bool Shader::CompileShader(unsigned int& shader, const std::string& source, unsigned int type) {
    shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        Logger::Error("Shader Compile Error: " + std::string(log));
        return false;
    }
    return true;
}

bool Shader::LinkProgram() {
    glLinkProgram(m_program);
    int success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(m_program, 512, nullptr, log);
        Logger::Error("Shader Link Error: " + std::string(log));
        return false;
    }
    return true;
}

int Shader::GetUniformLocation(const std::string& name) {
    if (m_uniformLocations.find(name) != m_uniformLocations.end())
        return m_uniformLocations[name];

    int loc = glGetUniformLocation(m_program, name.c_str());
    m_uniformLocations[name] = loc;
    return loc;
}

} // namespace veex