#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <vector.h>
#include <matrix.h>
#include <quaternion.h>
#include <fp.h>
#include <misc.h>

#include <cameraBase.h>

using namespace std;

class Camera : public CameraBase
{
public:
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

        const auto dir = target - pos;
        const auto z = normalize(dir);
        auto x = normalize(crossProduct(z, normalize(worldUp)));
        const auto y = normalize(crossProduct(x, normalize(z)));
        x = normalize(crossProduct(z, y));

        const auto targetTranslation = MatrixTranslation4x4(-target[COMPONENT::X],
                                                            -target[COMPONENT::Y],
                                                            -target[COMPONENT::Z]);
        const auto translation = MatrixTranslation4x4(0.f, 0.f, static_cast<float>(-dir.vectorLength()));
        // column-major
        // glm::transpose(glm::mat3(x_axis, y_axis, -z_axis)))
        std::array<float, 9> cols{
            x[COMPONENT::X],
            x[COMPONENT::Y],
            x[COMPONENT::Z],
            y[COMPONENT::X],
            y[COMPONENT::Y],
            y[COMPONENT::Z],
            -z[COMPONENT::X],
            -z[COMPONENT::Y],
            -z[COMPONENT::Z],
        };
        mat3x3f rot(cols);
        rot.transpose();

        auto q = QuaternionFromRotationMatrix(rot);
        q.normalize();

        log(Level::Info, "QuaternionFromRotationMatrix: ", q);

        auto rotationMatrix = RotationMatrixFromQuaternion(q);
        auto camera = MatrixMultiply4x4(translation, MatrixMultiply4x4(targetTranslation, rotationMatrix));

        // inverse
        // mat2x2f m2({3, -1, 0, 2});
        // auto m1_inverse = Inverse(m1);

        // mat3x3f m2({1, 1, 0, 0, 2, 1, 0, 1, 2});
        // auto m2_inverse = Inverse(m2);

        // auto identity = MatrixMultiply3x3(m2, m2_inverse);

        mat4x4f m4({2, 1, 7, 1, 5, 4, 8, 5, 0, 2, 9, 7, 8, 6, 3, 8});
        auto m4_inverse = Inverse(m4);
        auto identity = MatrixMultiply4x4(m4, m4_inverse);
        rebuild();
    }

    inline vec3f viewPos() const override
    {
        return _pos;
    }

    mat4x4f viewTransformLH() const override
    {
        return ViewTransformLH4x4(_pos, _pos + _worldCameraFrontDir, _worldCameraUp);
    }

    void handleKeyboardEvent(CameraActionType actionType, float dt) override
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

    void handleMouseCursorEvent(int button, int state, 
    const vec2i& currMouseInScreenSpace,
    const vec2i &screenDimension) override
    {
        // if (_firstMouseCursor)
        // {
        //     _lastMouseX = currMouseX;
        //     _lastMouseY = currMouseY;
        //     _firstMouseCursor = false;
        // }

        // float dx = currMouseX - _lastMouseX;
        // float dy = _lastMouseY - currMouseY; // screen space is opposite to camera space

        // _lastMouseX = currMouseX;
        // _lastMouseY = currMouseY;

        // dx *= _mouseSensitivity;
        // dy *= _mouseSensitivity;

        // _yaw += dx;
        // _pitch += dy;

        // // rotation occurs, rebuild the camera orthogonal basis
        // rebuild();
    }

    void handleMouseClickEvent(int button, int state, int x, int y) override
    {
    }

    virtual void handleMouseWheelEvent(float v) override
    {
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

    uint32_t _lastMouseX{0};
    uint32_t _lastMouseY{0};
    uint32_t _currMouseX{0};
    uint32_t _currMouseY{0};
};