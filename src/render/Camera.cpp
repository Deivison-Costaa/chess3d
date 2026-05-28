#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace chess3d {

void Camera::setPitch(float pitchRad) {
    pitch_ = std::clamp(pitchRad, -pitchLimit_, pitchLimit_);
}

float Camera::clampDistance(float d) const {
    return std::clamp(d, minDistance_, maxDistance_);
}

void Camera::rotate(float dyaw, float dpitch) {
    yaw_ += dyaw;
    setPitch(pitch_ + dpitch);
}

void Camera::zoom(float factor) {
    distance_ = clampDistance(distance_ * factor);
}

void Camera::pan(const glm::vec2& screenDelta, float aspect) {
    const float worldPerPixel = (2.0f * distance_ * std::tan(glm::radians(fovYDeg_ * 0.5f)));
    const glm::vec3 r = right();
    const glm::vec3 u = up();
    target_ += -r * screenDelta.x * worldPerPixel * aspect
              + u * screenDelta.y * worldPerPixel;
}

void Camera::reset() {
    target_ = homeTarget_;
    distance_ = homeDistance_;
    yaw_ = homeYaw_;
    pitch_ = homePitch_;
}

void Camera::setTopDown() {
    pitch_ = pitchLimit_;
    yaw_ = 0.0f;
}

void Camera::setSideView(bool whiteSide) {
    yaw_ = whiteSide ? 0.0f : glm::pi<float>();
    pitch_ = glm::radians(20.0f);
}

void Camera::setHomeView(const glm::vec3& target, float distance, float yaw, float pitch) {
    homeTarget_ = target;
    homeDistance_ = clampDistance(distance);
    homeYaw_ = yaw;
    homePitch_ = std::clamp(pitch, -pitchLimit_, pitchLimit_);
    reset();
}

glm::vec3 Camera::position() const {
    const float cp = std::cos(pitch_);
    const float sp = std::sin(pitch_);
    const float cy = std::cos(yaw_);
    const float sy = std::sin(yaw_);
    const glm::vec3 offset(cp * sy, sp, cp * cy);
    return target_ + offset * distance_;
}

glm::vec3 Camera::forward() const {
    return glm::normalize(target_ - position());
}

glm::vec3 Camera::right() const {
    constexpr glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    return glm::normalize(glm::cross(forward(), worldUp));
}

glm::vec3 Camera::up() const {
    return glm::normalize(glm::cross(right(), forward()));
}

glm::mat4 Camera::viewMatrix() const {
    constexpr glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    return glm::lookAt(position(), target_, worldUp);
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
    return glm::perspective(glm::radians(fovYDeg_), aspect, nearPlane_, farPlane_);
}

}  // namespace chess3d
