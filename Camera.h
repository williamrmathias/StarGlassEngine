#pragma once

// GLM
#include <glm/glm.hpp>

// SDL
#include <SDL2/SDL_events.h>

static const glm::vec3 worldUp = glm::vec3{ 0.f, 1.f, 0.f };

class Camera
{
public:
    // default ctor
    Camera() {};

    glm::mat4 getViewMatrix() const;
    glm::vec3 getViewPosition() const;

    void processSDLEvent(SDL_Event& event, Uint32 mouseState);

    void updatePosition(float deltaTIme);

    float getSpeed() const { return speed; }
    float getSensitivity() const { return sensitivity; }

    void setSpeed(float newSpeed) { speed = newSpeed; }
    void setSensitivity(float newSensitivity) { sensitivity = newSensitivity; }

private:
    glm::vec3 position{ 0.f, 0.f, 0.f };
    glm::vec3 velocity{ 0.f, 0.f, 0.f };

    // view directions
    glm::vec3 front{ 0.f, 0.f, -1.f };
    glm::vec3 up{ 0.f, 1.f, 0.f };
    glm::vec3 right{ 1.f, 0.f, 0.f };

    // camera settings
    float speed = 2.5f;
    float sensitivity = 0.5f;;

    // euler angles
    float yaw = -90.f;
    float pitch = 0.f;

    void updateVectors();
};