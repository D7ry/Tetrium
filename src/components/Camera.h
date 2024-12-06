#pragma once
#include <glm/glm.hpp>

/**
 * @brief Camera are meant to be used with Vulkan Applications.
 * Takes some form of input and translates/rotates the camera position.
 *
 */
class Camera
{
  public:
    // default constructor creates a camera at origin
    Camera() : _position(0, 0, 0), _rotation(0, 0, 0) { updateViewMatrix(); }

    Camera(float x, float y, float z, float yaw, float pitch, float roll);
    Camera(glm::vec3 position, glm::vec3 rotation);
    const glm::mat4 GetViewMatrix() const;
    void ModRotation(float yaw, float pitch, float roll);
    void Move(float x, float y, float z);

    inline const glm::vec3& GetPosition() { return _position; };

    inline const glm::vec3& GetRotation() { return _rotation; };

    void SetPosition(float x, float y, float z);
    void SetRotation(float yaw, float pitch, float roll);

  private:
    glm::vec3 _position;   // world position of the camera
    glm::vec3 _rotation;   // pitch, yaw, roll
    glm::mat4 _viewMatrix; // view matrix

    // update the view matrix, must be called when `_position` or `_rotation`
    // are modified
    void updateViewMatrix();
};
