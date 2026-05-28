#pragma once

#include "Mesh.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace chess3d {

struct GltfMesh {
    std::string name;
    Mesh mesh;
    glm::vec3 bboxMin{0.0f};
    glm::vec3 bboxMax{0.0f};
    glm::mat4 nodeTransform{1.0f};
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

private:
    std::vector<GltfMesh> items_;
    std::unordered_map<std::string, std::size_t> byName_;
    glm::vec3 sceneBBoxMin_{0.0f};
    glm::vec3 sceneBBoxMax_{0.0f};
    bool ok_ = false;
};

}  // namespace chess3d
