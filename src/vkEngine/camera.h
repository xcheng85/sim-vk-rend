#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <vector.h>
#include <matrix.h>

#include <fp.h>

using namespace std;

class Camera
{
public:
    enum CameraActionType : int
    {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT
    };

    explicit Camera(vec3f pos,
                    vec3f target,
                    vec3f worldUp,
                    float pitch,
                    float yaw)
    {
        _pos = pos;
        _target = target;
        _worldUp = worldUp;
        _pitch = pitch;
        _yaw = yaw;
        rebuild();
    }

    inline auto viewPos() const {
        return _pos;
    }

    mat4x4f viewTransformLH()
    {
        return ViewTransformLH4x4(_pos, _pos + _worldCameraFrontDir, _worldCameraUp);
    }

    void handleKeyboardEvent(CameraActionType actionType, float dt)
    {
        auto v = _speed * dt;
        if (actionType == FORWARD)
            _pos += _worldCameraFrontDir * v;
        else if (actionType == BACKWARD)
            _pos -= _worldCameraFrontDir * v;
        else if (actionType == LEFT)
            _pos -= _worldCameraRight * v;
        else if (actionType == RIGHT)
            _pos += _worldCameraRight * v;
    }

    void handleMouseCursorEvent(float dx, float dy)
    {
        dx *= _mouseSensitivity;
        dy *= _mouseSensitivity;

        _yaw += dx;
        _pitch += dy;

        // rotation occurs, rebuild the camera orthogonal basis
        rebuild();
    }

private:
    void rebuild()
    {
        // rebuild front direction in world space
        vec3f front;
        // refer to pitch_yaw.png
        front[COMPONENT::X] = cos(rad(_yaw)) * cos(rad(_pitch));
        front[COMPONENT::Y] = sin(rad(_pitch));
        front[COMPONENT::Z] = sin(rad(_yaw)) * cos(rad(_pitch));

        _worldCameraFrontDir = normalize(front);
        _worldCameraRight = normalize(crossProduct(_worldCameraFrontDir, _worldUp));
        _worldCameraUp = normalize(crossProduct(_worldCameraRight, _worldCameraFrontDir));
    }

    // three attributes to create the view transfrom matrix
    // Camera translation updates _pos
    vec3f _pos;
    vec3f _target;
    // canonical
    vec3f _worldUp;

    // for camera
    vec3f _worldCameraFrontDir;
    vec3f _worldCameraRight;
    vec3f _worldCameraUp;

    // mouse movement
    // will change the front world direction.
    // accumulated from the initial state
    float _pitch; // x
    float _yaw;   // y

    // middle scroll, vertical fov
    float _vFov;

    float _speed{1.8f};
    float _mouseSensitivity{0.02f};
};