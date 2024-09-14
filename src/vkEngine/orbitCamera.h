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
#include <SDL_mouse.h>
#include <SDL_events.h>

using namespace std;

class OrbitCamera : public CameraBase
{
public:
    explicit OrbitCamera(const vec3f &eye, const vec3f &target, const vec3f &up)
    {
        const auto dir = target - eye;
        const auto z = normalize(dir);
        auto x = normalize(crossProduct(z, normalize(up)));
        const auto y = normalize(crossProduct(x, normalize(z)));
        x = normalize(crossProduct(z, y));

        // _targetTranslation = inverse(glm::translate(target));
        _targetTranslation = MatrixTranslation4x4(-target[COMPONENT::X],
                                                  -target[COMPONENT::Y],
                                                  -target[COMPONENT::Z]);
        // glm::translate(glm::vec3(0.f, 0.f, -glm::length(dir)));
        _translation = MatrixTranslation4x4(0.f, 0.f, static_cast<float>(-dir.vectorLength()));
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
        _rotation = QuaternionFromRotationMatrix(rot);
        _rotation.normalize();
        rebuild();
    }

    inline vec3f viewPos() const override
    {
        auto v4f = MatrixMultiplyVector4x4(_invCamera, vec4f(std::array<float, 4>({0, 0, 0, 1})));
        return vec3f(std::array<float, 3>({v4f[COMPONENT::X], v4f[COMPONENT::Y], v4f[COMPONENT::Z]}));
    }

    mat4x4f viewTransformLH() const override
    {
        return _camera;
    }

    void handleKeyboardEvent(CameraActionType actionType, float dt) override
    {
    }

    void handleMouseCursorEvent(
        int button,
        int state,
        const vec2i &currMouseInScreenSpace,
        const vec2i &screenDimension) override
    {
        const vec2f currMouseCoordsInNdc = screenSpace2Ndc(currMouseInScreenSpace, screenDimension);
        if (_prevMouseCoordsInNdc != vec2f(-2.f))
        {
            if (state & SDL_BUTTON_LMASK)
            {
                rotate(_prevMouseCoordsInNdc, currMouseCoordsInNdc);
            }
            else if (state & SDL_BUTTON_RMASK)
            {
                auto persPrj = PerspectiveProjectionTransformLH(0.0001f, 200.0f, 0.3f,
                                                                (float)screenDimension[COMPONENT::X] /
                                                                    (float)screenDimension[COMPONENT::Y]);
                auto persPrjInverse = Inverse(persPrj);
                auto delta = currMouseCoordsInNdc - _prevMouseCoordsInNdc;
                auto deltaProjInv = MatrixMultiplyVector4x4(persPrjInverse, vec4f(std::array{delta[COMPONENT::X], delta[COMPONENT::Y], 0.f, 1.f}));
                pan(deltaProjInv);
            }
        }
        // log(Level::Info, "_prevMouseCoordsInNdc: ", _prevMouseCoordsInNdc);
        _prevMouseCoordsInNdc = currMouseCoordsInNdc;
    }

    void handleMouseClickEvent(int button, int state, int x, int y) override
    {
        // log(Level::Info, "handleMouseClickEvent: ",
        //     "button: ", button,
        //     "x: ", x,
        //     "y: ", y);
    }

    void handleMouseWheelEvent(float v) override
    {
        zoom(v);
    }

private:
    // Project the point in [-1, 1] screen space onto the arcball sphere
    quatf screen2orbit(const vec2f &pNorm)
    {
        const float dist = dotProduct(pNorm, pNorm);
        // If we're on/in the sphere return the point on it
        if (dist <= 1.f)
        {
            return quatf(0.0, pNorm[COMPONENT::X], pNorm[COMPONENT::Y], std::sqrt(1.f - dist));
        }
        else
        {
            // otherwise we project the point onto the sphere
            const auto proj = normalize(pNorm);
            return quatf(0.0, proj[COMPONENT::X], proj[COMPONENT::Y], 0.f);
        }
    }

    void pan(vec4f dt)
    {
        // z component
        const float zoom_amount = std::abs(_translation.data[3][2]);
        vec4f motion(std::array{dt[COMPONENT::X] * zoom_amount, dt[COMPONENT::Y] * zoom_amount, 0.f, 0.f});

        // Find the panning amount in the world space
        motion = MatrixMultiplyVector4x4(_invCamera, motion);
        auto motionTranslation = MatrixTranslation4x4(motion[COMPONENT::X],
                                                      motion[COMPONENT::Y],
                                                      motion[COMPONENT::Z]);
        _targetTranslation = MatrixMultiply4x4(motionTranslation, _targetTranslation);
        rebuild();
    }

    void rotate(vec2f prevMouseCoordsInNdc, vec2f currMouseCoordsInNdc)
    {
        prevMouseCoordsInNdc = clamp(prevMouseCoordsInNdc, vec2f(-1.f), vec2f(1.f));
        currMouseCoordsInNdc = clamp(currMouseCoordsInNdc, vec2f(-1.f), vec2f(1.f));

        const auto currQuat = screen2orbit(currMouseCoordsInNdc);
        const auto prevQuat = screen2orbit(prevMouseCoordsInNdc);

        auto newRot = normalize(currQuat * prevQuat * _rotation);
        _rotation = newRot;
        rebuild();
    }

    void zoom(float v)
    {
        const mat4x4f motion = MatrixTranslation4x4(0.f, 0.f, v);

        _translation = MatrixMultiply4x4(motion, _translation);

        rebuild();
    }

    void rebuild()
    {
        // log(Level::Info, "rebuild.rotation: ", _rotation);
        // log(Level::Info, "rebuild.targetTranslation: ", _targetTranslation);
        // log(Level::Info, "rebuild.translation: ", _translation);

        auto rotationMatrix = RotationMatrixFromQuaternion(_rotation);
        _camera = MatrixMultiply4x4(_translation, MatrixMultiply4x4(rotationMatrix, _targetTranslation));
        _invCamera = Inverse(_camera);

        // log(Level::Info, "_camera: ", _camera);
    }

    // We store the unmodified look at matrix along with
    // decomposed translation and rotation components
    // for pan action
    mat4x4f _targetTranslation;
    mat4x4f _translation;
    quatf _rotation{0, 0, 0, 1};
    // camera is the full camera transform,
    // inv_camera is stored as well to easily compute
    // eye position and world space rotation axes
    mat4x4f _camera, _invCamera;
};