#include "Mesh.h"

#include <algorithm>
#include <array>
#include <utility>

namespace chess3d {

Mesh::Mesh(std::span<const Vertex> vertices, std::span<const std::uint32_t> indices) {
    if (vertices.empty() || indices.empty()) return;

    glCreateVertexArrays(1, &vao_);
    glCreateBuffers(1, &vbo_);
    glCreateBuffers(1, &ebo_);

    glNamedBufferStorage(vbo_,
                         static_cast<GLsizeiptr>(vertices.size_bytes()),
                         vertices.data(),
                         0);
    glNamedBufferStorage(ebo_,
                         static_cast<GLsizeiptr>(indices.size_bytes()),
                         indices.data(),
                         0);

    constexpr GLuint kBindingPoint = 0;
    glVertexArrayVertexBuffer(vao_, kBindingPoint, vbo_, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(vao_, ebo_);

    glEnableVertexArrayAttrib(vao_, 0);
    glVertexArrayAttribFormat(vao_, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    glVertexArrayAttribBinding(vao_, 0, kBindingPoint);

    glEnableVertexArrayAttrib(vao_, 1);
    glVertexArrayAttribFormat(vao_, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
    glVertexArrayAttribBinding(vao_, 1, kBindingPoint);

    glEnableVertexArrayAttrib(vao_, 2);
    glVertexArrayAttribFormat(vao_, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, uv));
    glVertexArrayAttribBinding(vao_, 2, kBindingPoint);

    indexCount_ = static_cast<GLsizei>(indices.size());
}

Mesh::~Mesh() {
    release();
}

Mesh::Mesh(Mesh&& other) noexcept
    : vao_(other.vao_), vbo_(other.vbo_), ebo_(other.ebo_), indexCount_(other.indexCount_) {
    other.vao_ = 0;
    other.vbo_ = 0;
    other.ebo_ = 0;
    other.indexCount_ = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        release();
        vao_ = std::exchange(other.vao_, 0u);
        vbo_ = std::exchange(other.vbo_, 0u);
        ebo_ = std::exchange(other.ebo_, 0u);
        indexCount_ = std::exchange(other.indexCount_, 0);
    }
    return *this;
}

void Mesh::release() {
    if (ebo_) { glDeleteBuffers(1, &ebo_); ebo_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    indexCount_ = 0;
}

void Mesh::draw() const {
    if (!vao_) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}

void Mesh::drawInstanced(GLsizei instanceCount) const {
    if (!vao_ || instanceCount <= 0) return;
    glBindVertexArray(vao_);
    glDrawElementsInstanced(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr, instanceCount);
}

Mesh Mesh::makeCube(float h) {
    const std::array<Vertex, 24> verts = {{
        // +Z (front)
        {{-h, -h,  h}, { 0,  0,  1}, {0, 0}},
        {{ h, -h,  h}, { 0,  0,  1}, {1, 0}},
        {{ h,  h,  h}, { 0,  0,  1}, {1, 1}},
        {{-h,  h,  h}, { 0,  0,  1}, {0, 1}},
        // -Z (back)
        {{ h, -h, -h}, { 0,  0, -1}, {0, 0}},
        {{-h, -h, -h}, { 0,  0, -1}, {1, 0}},
        {{-h,  h, -h}, { 0,  0, -1}, {1, 1}},
        {{ h,  h, -h}, { 0,  0, -1}, {0, 1}},
        // +X (right)
        {{ h, -h,  h}, { 1,  0,  0}, {0, 0}},
        {{ h, -h, -h}, { 1,  0,  0}, {1, 0}},
        {{ h,  h, -h}, { 1,  0,  0}, {1, 1}},
        {{ h,  h,  h}, { 1,  0,  0}, {0, 1}},
        // -X (left)
        {{-h, -h, -h}, {-1,  0,  0}, {0, 0}},
        {{-h, -h,  h}, {-1,  0,  0}, {1, 0}},
        {{-h,  h,  h}, {-1,  0,  0}, {1, 1}},
        {{-h,  h, -h}, {-1,  0,  0}, {0, 1}},
        // +Y (top)
        {{-h,  h,  h}, { 0,  1,  0}, {0, 0}},
        {{ h,  h,  h}, { 0,  1,  0}, {1, 0}},
        {{ h,  h, -h}, { 0,  1,  0}, {1, 1}},
        {{-h,  h, -h}, { 0,  1,  0}, {0, 1}},
        // -Y (bottom)
        {{-h, -h, -h}, { 0, -1,  0}, {0, 0}},
        {{ h, -h, -h}, { 0, -1,  0}, {1, 0}},
        {{ h, -h,  h}, { 0, -1,  0}, {1, 1}},
        {{-h, -h,  h}, { 0, -1,  0}, {0, 1}},
    }};

    std::array<std::uint32_t, 36> indices{};
    for (std::uint32_t face = 0; face < 6; ++face) {
        const std::uint32_t base = face * 4;
        indices[face * 6 + 0] = base + 0;
        indices[face * 6 + 1] = base + 1;
        indices[face * 6 + 2] = base + 2;
        indices[face * 6 + 3] = base + 0;
        indices[face * 6 + 4] = base + 2;
        indices[face * 6 + 5] = base + 3;
    }
    return Mesh(std::span<const Vertex>(verts.data(), verts.size()),
                std::span<const std::uint32_t>(indices.data(), indices.size()));
}

Mesh Mesh::makeQuad(float h) {
    const std::array<Vertex, 4> verts = {{
        {{-h, 0.0f, -h}, {0, 1, 0}, {0, 0}},
        {{ h, 0.0f, -h}, {0, 1, 0}, {1, 0}},
        {{ h, 0.0f,  h}, {0, 1, 0}, {1, 1}},
        {{-h, 0.0f,  h}, {0, 1, 0}, {0, 1}},
    }};
    const std::array<std::uint32_t, 6> indices = {0, 1, 2, 0, 2, 3};
    return Mesh(std::span<const Vertex>(verts.data(), verts.size()),
                std::span<const std::uint32_t>(indices.data(), indices.size()));
}

}  // namespace chess3d
