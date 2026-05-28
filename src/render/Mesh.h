#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace chess3d {

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
};

class Mesh {
public:
    Mesh() = default;
    Mesh(std::span<const Vertex> vertices, std::span<const std::uint32_t> indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw() const;
    void drawInstanced(GLsizei instanceCount) const;

    GLuint vao() const { return vao_; }
    GLsizei indexCount() const { return indexCount_; }
    bool valid() const { return vao_ != 0; }

    static Mesh makeCube(float halfSize = 0.5f);
    static Mesh makeQuad(float halfSize = 0.5f);

private:
    void release();

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei indexCount_ = 0;
};

}  // namespace chess3d
