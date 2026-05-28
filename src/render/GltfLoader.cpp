#include "GltfLoader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace chess3d {

namespace {

glm::mat4 nodeLocalMatrix(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        glm::mat4 m(1.0f);
        for (int i = 0; i < 16; ++i) {
            m[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
        }
        return m;
    }
    glm::mat4 t(1.0f);
    if (node.translation.size() == 3) {
        t = glm::translate(t, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
    }
    if (node.rotation.size() == 4) {
        const glm::quat q(static_cast<float>(node.rotation[3]),
                          static_cast<float>(node.rotation[0]),
                          static_cast<float>(node.rotation[1]),
                          static_cast<float>(node.rotation[2]));
        t *= glm::mat4_cast(q);
    }
    if (node.scale.size() == 3) {
        t = glm::scale(t, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
    }
    return t;
}

const unsigned char* accessorPtr(const tinygltf::Model& model,
                                 const tinygltf::Accessor& acc,
                                 std::size_t& strideOut,
                                 std::size_t& countOut) {
    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];
    const std::size_t compSize = tinygltf::GetComponentSizeInBytes(acc.componentType);
    const std::size_t numComp = tinygltf::GetNumComponentsInType(acc.type);
    strideOut = bv.byteStride ? bv.byteStride : compSize * numComp;
    countOut = acc.count;
    return buf.data.data() + bv.byteOffset + acc.byteOffset;
}

bool buildMeshFromPrimitive(const tinygltf::Model& model,
                            const tinygltf::Primitive& prim,
                            std::vector<Vertex>& outVerts,
                            std::vector<std::uint32_t>& outIndices,
                            glm::vec3& bboxMin,
                            glm::vec3& bboxMax) {
    if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
        spdlog::warn("GltfLoader: non-triangle primitive (mode={}) skipped", prim.mode);
        return false;
    }
    auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end()) {
        spdlog::warn("GltfLoader: primitive without POSITION");
        return false;
    }

    const auto& posAcc = model.accessors[posIt->second];
    std::size_t stride = 0, count = 0;
    const unsigned char* posPtr = accessorPtr(model, posAcc, stride, count);

    const unsigned char* nrmPtr = nullptr;
    std::size_t nrmStride = 0;
    if (auto it = prim.attributes.find("NORMAL"); it != prim.attributes.end()) {
        std::size_t c = 0;
        nrmPtr = accessorPtr(model, model.accessors[it->second], nrmStride, c);
    }

    const unsigned char* uvPtr = nullptr;
    std::size_t uvStride = 0;
    if (auto it = prim.attributes.find("TEXCOORD_0"); it != prim.attributes.end()) {
        std::size_t c = 0;
        uvPtr = accessorPtr(model, model.accessors[it->second], uvStride, c);
    }

    const std::size_t baseVertex = outVerts.size();
    outVerts.reserve(baseVertex + count);
    for (std::size_t i = 0; i < count; ++i) {
        Vertex v;
        std::memcpy(&v.position, posPtr + i * stride, sizeof(glm::vec3));
        if (nrmPtr) {
            std::memcpy(&v.normal, nrmPtr + i * nrmStride, sizeof(glm::vec3));
        } else {
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        if (uvPtr) {
            std::memcpy(&v.uv, uvPtr + i * uvStride, sizeof(glm::vec2));
        } else {
            v.uv = glm::vec2(0.0f);
        }
        bboxMin = glm::min(bboxMin, v.position);
        bboxMax = glm::max(bboxMax, v.position);
        outVerts.push_back(v);
    }

    if (prim.indices < 0) {
        for (std::size_t i = 0; i < count; ++i) {
            outIndices.push_back(static_cast<std::uint32_t>(baseVertex + i));
        }
        return true;
    }
    const auto& idxAcc = model.accessors[prim.indices];
    std::size_t idxStride = 0, idxCount = 0;
    const unsigned char* idxPtr = accessorPtr(model, idxAcc, idxStride, idxCount);

    outIndices.reserve(outIndices.size() + idxCount);
    switch (idxAcc.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            for (std::size_t i = 0; i < idxCount; ++i) {
                outIndices.push_back(static_cast<std::uint32_t>(baseVertex + idxPtr[i]));
            }
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            for (std::size_t i = 0; i < idxCount; ++i) {
                std::uint16_t v;
                std::memcpy(&v, idxPtr + i * sizeof(std::uint16_t), sizeof(v));
                outIndices.push_back(static_cast<std::uint32_t>(baseVertex + v));
            }
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            for (std::size_t i = 0; i < idxCount; ++i) {
                std::uint32_t v;
                std::memcpy(&v, idxPtr + i * sizeof(std::uint32_t), sizeof(v));
                outIndices.push_back(static_cast<std::uint32_t>(baseVertex + v));
            }
            break;
        default:
            spdlog::error("GltfLoader: unsupported index type {}", idxAcc.componentType);
            return false;
    }
    return true;
}

void processNode(const tinygltf::Model& model,
                 int nodeIndex,
                 const glm::mat4& parent,
                 std::vector<GltfMesh>& out,
                 std::unordered_map<std::string, std::size_t>& byName,
                 glm::vec3& sceneMin,
                 glm::vec3& sceneMax) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) return;
    const auto& node = model.nodes[nodeIndex];
    const glm::mat4 local = nodeLocalMatrix(node);
    const glm::mat4 global = parent * local;

    if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
        const auto& mesh = model.meshes[node.mesh];

        std::vector<Vertex> verts;
        std::vector<std::uint32_t> indices;
        glm::vec3 bbMin(std::numeric_limits<float>::max());
        glm::vec3 bbMax(std::numeric_limits<float>::lowest());

        for (const auto& prim : mesh.primitives) {
            buildMeshFromPrimitive(model, prim, verts, indices, bbMin, bbMax);
        }

        if (!verts.empty() && !indices.empty()) {
            GltfMesh item;
            item.name = !node.name.empty() ? node.name : mesh.name;
            if (item.name.empty()) {
                item.name = "unnamed_" + std::to_string(out.size());
            }
            item.mesh = Mesh(std::span<const Vertex>(verts.data(), verts.size()),
                             std::span<const std::uint32_t>(indices.data(), indices.size()));
            item.bboxMin = bbMin;
            item.bboxMax = bbMax;
            item.nodeTransform = global;

            // bbox transformada (aproximação: 8 cantos)
            const glm::vec3 corners[8] = {
                {bbMin.x, bbMin.y, bbMin.z}, {bbMax.x, bbMin.y, bbMin.z},
                {bbMin.x, bbMax.y, bbMin.z}, {bbMax.x, bbMax.y, bbMin.z},
                {bbMin.x, bbMin.y, bbMax.z}, {bbMax.x, bbMin.y, bbMax.z},
                {bbMin.x, bbMax.y, bbMax.z}, {bbMax.x, bbMax.y, bbMax.z},
            };
            for (const auto& c : corners) {
                const glm::vec3 w = glm::vec3(global * glm::vec4(c, 1.0f));
                sceneMin = glm::min(sceneMin, w);
                sceneMax = glm::max(sceneMax, w);
            }

            byName[item.name] = out.size();
            out.push_back(std::move(item));
        }
    }

    for (int child : node.children) {
        processNode(model, child, global, out, byName, sceneMin, sceneMax);
    }
}

}  // namespace

bool GltfLoader::loadFromFile(const std::filesystem::path& path) {
    items_.clear();
    byName_.clear();
    sceneBBoxMin_ = glm::vec3(std::numeric_limits<float>::max());
    sceneBBoxMax_ = glm::vec3(std::numeric_limits<float>::lowest());
    ok_ = false;

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    const auto ext = path.extension().string();
    const bool isBinary = (ext == ".glb" || ext == ".GLB");
    const bool loaded = isBinary
        ? loader.LoadBinaryFromFile(&model, &err, &warn, path.string())
        : loader.LoadASCIIFromFile(&model, &err, &warn, path.string());

    if (!warn.empty()) spdlog::warn("GltfLoader: {}", warn);
    if (!err.empty())  spdlog::error("GltfLoader: {}", err);
    if (!loaded) {
        spdlog::error("GltfLoader: failed to load {}", path.string());
        return false;
    }

    const int sceneIdx = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (model.scenes.empty()) {
        spdlog::error("GltfLoader: no scenes");
        return false;
    }
    const auto& scene = model.scenes[sceneIdx];
    for (int rootNode : scene.nodes) {
        processNode(model, rootNode, glm::mat4(1.0f), items_, byName_, sceneBBoxMin_, sceneBBoxMax_);
    }

    spdlog::info("GltfLoader: loaded {} mesh nodes from {}", items_.size(), path.filename().string());
    spdlog::info("GltfLoader: scene bbox min=({:.2f}, {:.2f}, {:.2f}) max=({:.2f}, {:.2f}, {:.2f})",
                 sceneBBoxMin_.x, sceneBBoxMin_.y, sceneBBoxMin_.z,
                 sceneBBoxMax_.x, sceneBBoxMax_.y, sceneBBoxMax_.z);
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& it = items_[i];
        spdlog::info("  [{}] '{}' verts via bbox=({:.2f},{:.2f},{:.2f})..({:.2f},{:.2f},{:.2f})",
                     i, it.name,
                     it.bboxMin.x, it.bboxMin.y, it.bboxMin.z,
                     it.bboxMax.x, it.bboxMax.y, it.bboxMax.z);
    }
    ok_ = !items_.empty();
    return ok_;
}

const GltfMesh* GltfLoader::find(const std::string& name) const {
    auto it = byName_.find(name);
    return it == byName_.end() ? nullptr : &items_[it->second];
}

const GltfMesh* GltfLoader::findContaining(std::string_view substring) const {
    std::string needle(substring);
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const auto& it : items_) {
        std::string n = it.name;
        std::transform(n.begin(), n.end(), n.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (n.find(needle) != std::string::npos) return &it;
    }
    return nullptr;
}

}  // namespace chess3d
