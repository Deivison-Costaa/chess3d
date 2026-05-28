#pragma once

#include "Mesh.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace chess3d {

// Um sub-mesh = um primitivo do glTF, com sua própria material e VBO/VAO.
// Necessário para suportar tabuleiros e modelos com múltiplos materiais por nó.
struct GltfSubMesh {
    Mesh mesh;
    int materialIndex = -1;
    glm::vec3 bboxMin{0.0f};
    glm::vec3 bboxMax{0.0f};
};

struct GltfMesh {
    std::string name;
    std::vector<GltfSubMesh> submeshes;
    glm::vec3 bboxMin{0.0f};
    glm::vec3 bboxMax{0.0f};
    glm::mat4 nodeTransform{1.0f};
    bool hasValidUvs = true;  // false se UV bbox é degenerada (todos vértices no mesmo ponto)
};

struct GltfImage {
    int width = 0;
    int height = 0;
    int channels = 4;
    std::vector<unsigned char> pixels;
};

struct GltfMaterial {
    std::string name;
    int baseColorImage = -1;
    int normalImage = -1;
    int metallicRoughnessImage = -1;
    glm::vec4 baseColorFactor{1.0f};
    float metallicFactor  = 1.0f;
    float roughnessFactor = 1.0f;
};

class GltfLoader {
public:
    bool loadFromFile(const std::filesystem::path& path);

    bool ok() const { return ok_; }
    const std::vector<GltfMesh>& items() const { return items_; }

    const GltfMesh* find(const std::string& name) const;
    const GltfMesh* findContaining(std::string_view substring) const;

    glm::vec3 sceneBBoxMin() const { return sceneBBoxMin_; }
    glm::vec3 sceneBBoxMax() const { return sceneBBoxMax_; }

    const std::vector<GltfImage>& images() const { return images_; }
    const std::vector<GltfMaterial>& materials() const { return materials_; }

private:
    std::vector<GltfMesh> items_;
    std::unordered_map<std::string, std::size_t> byName_;
    std::vector<GltfImage> images_;
    std::vector<GltfMaterial> materials_;
    glm::vec3 sceneBBoxMin_{0.0f};
    glm::vec3 sceneBBoxMax_{0.0f};
    bool ok_ = false;
};

}  // namespace chess3d
