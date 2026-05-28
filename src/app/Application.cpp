#include "Application.h"

#include "core/BoardCoords.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <filesystem>

namespace chess3d {

namespace {

constexpr GLuint kCameraBlockBinding = 0;

struct CameraBlockData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec4 cameraPos{0.0f};
};

std::filesystem::path assetPath(const char* relative) {
#ifdef CHESS3D_ASSETS_DIR
    return std::filesystem::path(CHESS3D_ASSETS_DIR) / relative;
#else
    return std::filesystem::path("assets") / relative;
#endif
}

constexpr const char* kBoardMeshName = "Cube.006";

// Nomes canônicos no .glb da Jaximus — primeira ocorrência de cada peça/cor.
constexpr const char* kWhitePawn   = "Pawn";
constexpr const char* kWhiteRook   = "Rook";
constexpr const char* kWhiteKnight = "Knight";
constexpr const char* kWhiteBishop = "Bishop";
constexpr const char* kWhiteQueen  = "Queen";
constexpr const char* kWhiteKing   = "King";
constexpr const char* kBlackPawn   = "Pawn.B";
constexpr const char* kBlackRook   = "Rook.B";
constexpr const char* kBlackKnight = "Knight.B";
constexpr const char* kBlackBishop = "Bishop.B";
constexpr const char* kBlackQueen  = "Queen.B";
constexpr const char* kBlackKing   = "King.B";

const glm::vec3 kWhiteColor(0.92f, 0.88f, 0.78f);
const glm::vec3 kBlackColor(0.10f, 0.10f, 0.12f);
const glm::vec3 kBoardLightSq(0.90f, 0.82f, 0.66f);
const glm::vec3 kBoardDarkSq(0.40f, 0.27f, 0.18f);

struct PiecePlacement {
    const char* meshName;
    glm::vec3 color;
    int file;
    int rank;
    float yawDeg;  // 0 para brancas, 180 para pretas (cavalos encarando o adversário)
};

std::array<PiecePlacement, 32> initialPlacements() {
    std::array<PiecePlacement, 32> p{};
    int idx = 0;
    const char* whiteBack[8] = {kWhiteRook, kWhiteKnight, kWhiteBishop, kWhiteQueen,
                                 kWhiteKing, kWhiteBishop, kWhiteKnight, kWhiteRook};
    const char* blackBack[8] = {kBlackRook, kBlackKnight, kBlackBishop, kBlackQueen,
                                 kBlackKing, kBlackBishop, kBlackKnight, kBlackRook};
    for (int file = 0; file < 8; ++file) {
        p[idx++] = {whiteBack[file], kWhiteColor, file, 0,   0.0f};
        p[idx++] = {kWhitePawn,      kWhiteColor, file, 1,   0.0f};
        p[idx++] = {kBlackPawn,      kBlackColor, file, 6, 180.0f};
        p[idx++] = {blackBack[file], kBlackColor, file, 7, 180.0f};
    }
    return p;
}

}  // namespace

Application::Application()
    : window_({1280, 720, "Chess3D", true, true}) {
    if (!window_.ok()) {
        spdlog::critical("Application: window not initialised");
        return;
    }

    camera_.setFovYDeg(50.0f);
    camera_.setNearFar(0.1f, 100.0f);
    camera_.setHomeView(glm::vec3(0.0f), 14.0f, glm::radians(35.0f), glm::radians(38.0f));

    input_.attach(window_.handle(), &camera_);

    litShader_ = Shader(assetPath("shaders/lit.vert"), assetPath("shaders/lit.frag"));
    if (litShader_.valid()) {
        litShader_.bindUniformBlock("CameraBlock", kCameraBlockBinding);
    }

    cubeMesh_ = Mesh::makeCube(0.6f);
    floorMesh_ = Mesh::makeQuad(8.0f);

    if (gltf_.loadFromFile(assetPath("models/chessboard.glb"))) {
        if (const auto* board = gltf_.find(kBoardMeshName)) {
            const glm::vec3 size = board->bboxMax - board->bboxMin;
            const float boardWidth = std::max(size.x, size.z);
            if (boardWidth > 0.0f) {
                gltfWorldScale_ = (8.0f * kSquareSize) / boardWidth;
            }
            gltfWorldOffset_ = glm::vec3(-0.5f * (board->bboxMin.x + board->bboxMax.x),
                                         -board->bboxMax.y,
                                         -0.5f * (board->bboxMin.z + board->bboxMax.z));
        }
        spdlog::info("GltfLoader: world scale={:.4f}, offset=({:.2f},{:.2f},{:.2f})",
                     gltfWorldScale_, gltfWorldOffset_.x, gltfWorldOffset_.y, gltfWorldOffset_.z);
    }

    glCreateBuffers(1, &cameraUbo_);
    glNamedBufferStorage(cameraUbo_, sizeof(CameraBlockData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, kCameraBlockBinding, cameraUbo_);
}

Application::~Application() {
    input_.detach();
    if (cameraUbo_) {
        glDeleteBuffers(1, &cameraUbo_);
        cameraUbo_ = 0;
    }
}

void Application::updateCameraUbo(float aspect) {
    CameraBlockData data;
    data.view = camera_.viewMatrix();
    data.projection = camera_.projectionMatrix(aspect);
    data.cameraPos = glm::vec4(camera_.position(), 1.0f);
    glNamedBufferSubData(cameraUbo_, 0, sizeof(CameraBlockData), &data);
}

void Application::renderScene(float aspect) {
    updateCameraUbo(aspect);
    if (!litShader_.valid()) return;
    litShader_.use();
    litShader_.setVec3("uLightDir", glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)));
    litShader_.setVec3("uLightColor", glm::vec3(1.0f, 0.97f, 0.9f));

    if (!gltf_.ok()) {
        const float t = static_cast<float>(glfwGetTime());
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), t * 0.6f, glm::vec3(0.2f, 1.0f, 0.1f));
        litShader_.setMat4("uModel", model);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
        litShader_.setVec3("uAlbedo", glm::vec3(0.85f, 0.55f, 0.25f));
        cubeMesh_.draw();
        return;
    }

    const glm::mat4 normalize =
        glm::scale(glm::mat4(1.0f), glm::vec3(gltfWorldScale_))
        * glm::translate(glm::mat4(1.0f), gltfWorldOffset_);

    // Tabuleiro
    if (const auto* board = gltf_.find(kBoardMeshName)) {
        litShader_.setMat4("uModel", normalize);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(normalize))));
        litShader_.setVec3("uAlbedo", glm::vec3(0.30f, 0.22f, 0.16f));
        board->mesh.draw();
    }

    // Peças na posição inicial
    for (const auto& p : initialPlacements()) {
        const auto* item = gltf_.find(p.meshName);
        if (!item) continue;
        const glm::mat4 model =
            glm::translate(glm::mat4(1.0f), squareToWorld(p.file, p.rank))
            * glm::rotate(glm::mat4(1.0f), glm::radians(p.yawDeg), glm::vec3(0.0f, 1.0f, 0.0f))
            * glm::scale(glm::mat4(1.0f), glm::vec3(gltfWorldScale_));
        litShader_.setMat4("uModel", model);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
        litShader_.setVec3("uAlbedo", p.color);
        item->mesh.draw();
    }
}

int Application::run() {
    if (!window_.ok()) return 1;
    spdlog::info("Chess3D — entering main loop (ESC quits, R reset, F top-down, 1/2 white/black side, scroll zooms, LMB rotates, RMB pans)");
    while (!window_.shouldClose()) {
        window_.pollEvents();
        if (glfwGetKey(window_.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            window_.setShouldClose(true);
        }
        glClearColor(0.07f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderScene(window_.aspect());
        window_.swap();
    }
    return 0;
}

}  // namespace chess3d
