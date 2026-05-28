#include "Shader.h"

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace chess3d {

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        spdlog::error("Shader: cannot open {}", path.string());
        return {};
    }
    std::stringstream ss;
    ss << stream.rdbuf();
    return ss.str();
}

GLuint compileStage(GLenum stage, const std::string& source, const std::filesystem::path& origin) {
    GLuint shader = glCreateShader(stage);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(static_cast<size_t>(logLen));
        glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        spdlog::error("Shader compile failed ({}): {}", origin.string(), log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

Shader::Shader(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath) {
    const std::string vSrc = readFile(vertexPath);
    const std::string fSrc = readFile(fragmentPath);
    if (vSrc.empty() || fSrc.empty()) return;

    const GLuint vs = compileStage(GL_VERTEX_SHADER, vSrc, vertexPath);
    const GLuint fs = compileStage(GL_FRAGMENT_SHADER, fSrc, fragmentPath);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    GLint status = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint logLen = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(static_cast<size_t>(logLen));
        glGetProgramInfoLog(program_, logLen, nullptr, log.data());
        spdlog::error("Shader link failed: {}", log.data());
        glDeleteProgram(program_);
        program_ = 0;
        glDeleteShader(vs);
        glDeleteShader(fs);
        return;
    }

    glDetachShader(program_, vs);
    glDetachShader(program_, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    spdlog::debug("Shader: linked program {} from {} + {}", program_,
                  vertexPath.filename().string(), fragmentPath.filename().string());
}

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

Shader::Shader(Shader&& other) noexcept
    : program_(other.program_), uniformCache_(std::move(other.uniformCache_)) {
    other.program_ = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (program_) glDeleteProgram(program_);
        program_ = other.program_;
        uniformCache_ = std::move(other.uniformCache_);
        other.program_ = 0;
    }
    return *this;
}

void Shader::use() const {
    glUseProgram(program_);
}

GLint Shader::uniformLocation(std::string_view name) {
    const std::string key(name);
    if (auto it = uniformCache_.find(key); it != uniformCache_.end()) {
        return it->second;
    }
    const GLint loc = glGetUniformLocation(program_, key.c_str());
    if (loc == -1) {
        spdlog::debug("Shader: uniform '{}' not found (or optimised out)", key);
    }
    uniformCache_.emplace(key, loc);
    return loc;
}

void Shader::setBool(std::string_view name, bool value) {
    glProgramUniform1i(program_, uniformLocation(name), value ? 1 : 0);
}

void Shader::setInt(std::string_view name, int value) {
    glProgramUniform1i(program_, uniformLocation(name), value);
}

void Shader::setFloat(std::string_view name, float value) {
    glProgramUniform1f(program_, uniformLocation(name), value);
}

void Shader::setVec2(std::string_view name, const glm::vec2& v) {
    glProgramUniform2fv(program_, uniformLocation(name), 1, glm::value_ptr(v));
}

void Shader::setVec3(std::string_view name, const glm::vec3& v) {
    glProgramUniform3fv(program_, uniformLocation(name), 1, glm::value_ptr(v));
}

void Shader::setVec4(std::string_view name, const glm::vec4& v) {
    glProgramUniform4fv(program_, uniformLocation(name), 1, glm::value_ptr(v));
}

void Shader::setMat3(std::string_view name, const glm::mat3& m) {
    glProgramUniformMatrix3fv(program_, uniformLocation(name), 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::setMat4(std::string_view name, const glm::mat4& m) {
    glProgramUniformMatrix4fv(program_, uniformLocation(name), 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::bindUniformBlock(std::string_view blockName, GLuint bindingPoint) {
    const std::string key(blockName);
    const GLuint blockIndex = glGetUniformBlockIndex(program_, key.c_str());
    if (blockIndex == GL_INVALID_INDEX) {
        spdlog::warn("Shader: uniform block '{}' not found", key);
        return;
    }
    glUniformBlockBinding(program_, blockIndex, bindingPoint);
}

}  // namespace chess3d
