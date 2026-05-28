#pragma once

#include <glm/glm.hpp>

namespace chess3d {

class Camera {
public:
    Camera() = default;

    void setTarget(const glm::vec3& target) { target_ = target; }
    const glm::vec3& target() const { return target_; }

    void setDistance(float d) { distance_ = clampDistance(d); }
    float distance() const { return distance_; }

    void setYaw(float yawRad) { yaw_ = yawRad; }
    float yaw() const { return yaw_; }

    void setPitch(float pitchRad);
    float pitch() const { return pitch_; }

    void setFovYDeg(float fovDeg) { fovYDeg_ = fovDeg; }
    void setNearFar(float n, float f) { nearPlane_ = n; farPlane_ = f; }

    void rotate(float dyaw, float dpitch);
    void zoom(float factor);
    void pan(const glm::vec2& screenDelta, float aspect);

    void reset();
    void setTopDown();
    void setSideView(bool whiteSide);
    void setHomeView(const glm::vec3& target, float distance, float yaw, float pitch);

    glm::vec3 position() const;
    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::vec3 up() const;

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspect) const;

private:
    float clampDistance(float d) const;

    glm::vec3 target_{0.0f, 0.0f, 0.0f};
    float distance_ = 10.0f;
    float yaw_ = glm::radians(35.0f);
    float pitch_ = glm::radians(35.0f);

    glm::vec3 homeTarget_{0.0f};
    float homeDistance_ = 10.0f;
    float homeYaw_ = glm::radians(35.0f);
    float homePitch_ = glm::radians(35.0f);

    float fovYDeg_ = 50.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 100.0f;
    float minDistance_ = 2.5f;
    float maxDistance_ = 35.0f;
    float pitchLimit_ = glm::radians(85.0f);
};

}  // namespace chess3d
