#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace chess3d {

class Shader {
public:
    Shader() = default;
    Shader(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void use() const;
    GLuint id() const { return program_; }
    bool valid() const { return program_ != 0; }

    void setBool(std::string_view name, bool value);
    void setInt(std::string_view name, int value);
    void setFloat(std::string_view name, float value);
    void setVec2(std::string_view name, const glm::vec2& v);
    void setVec3(std::string_view name, const glm::vec3& v);
    void setVec4(std::string_view name, const glm::vec4& v);
    void setMat3(std::string_view name, const glm::mat3& m);
    void setMat4(std::string_view name, const glm::mat4& m);

    void bindUniformBlock(std::string_view blockName, GLuint bindingPoint);

private:
    GLint uniformLocation(std::string_view name);

    GLuint program_ = 0;
    std::unordered_map<std::string, GLint> uniformCache_;
};

}  // namespace chess3d
