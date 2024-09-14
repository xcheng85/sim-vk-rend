#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <misc.h>

#include <cameraBase.h>
#include <SDL_mouse.h>
#include <SDL_events.h>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <vector.h>
#include <matrix.h>
#include <quaternion.h>
#include <fp.h>

using namespace std;

class ArcballCamera : public CameraBase
{
public:
    explicit ArcballCamera(const glm::vec3 &eye, const glm::vec3 &center, const glm::vec3 &up)
    {
        {
            const glm::vec3 dir = center - eye;
            glm::vec3 z_axis = glm::normalize(dir);
            glm::vec3 x_axis = glm::normalize(glm::cross(z_axis, glm::normalize(up)));
            glm::vec3 y_axis = glm::normalize(glm::cross(x_axis, z_axis));
            x_axis = glm::normalize(glm::cross(z_axis, y_axis));

            center_translation = glm::inverse(glm::translate(center));
            translation = glm::translate(glm::vec3(0.f, 0.f, -glm::length(dir)));
            rotation = glm::normalize(glm::quat_cast(glm::transpose(glm::mat3(x_axis, y_axis, -z_axis))));

            log(Level::Info, "rotation : ", rotation.x);
            log(Level::Info, "rotation : ", rotation.y);
            log(Level::Info, "rotation : ", rotation.z);
            log(Level::Info, "rotation : ", rotation.w);

            update_camera();
        }

        {
            const glm::vec3 dir1 = center - eye;
            const vec3f dir{std::array<float, 3>({dir1.x, dir1.y, dir1.z})};
            const auto z = normalize(dir);

            const vec3f up1{std::array<float, 3>({up.x, up.y, up.z})};
            auto x = normalize(crossProduct(z, normalize(up1)));

            const auto y = normalize(crossProduct(x, normalize(z)));
            x = normalize(crossProduct(z, y));

            _center_translation = MatrixTranslation4x4(-center[COMPONENT::X],
                                                       -center[COMPONENT::Y],
                                                       -center[COMPONENT::Z]);
            _translation = MatrixTranslation4x4(0.f, 0.f, static_cast<float>(-dir.vectorLength()));

            // column-major
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
            _rotation = normalize(_rotation);
            log(Level::Info, "_rotation mat: ", _rotation);

            update_camera1();
        }
    }

    glm::vec3 viewPos() const override
    {
        return glm::vec3{inv_camera * glm::vec4{0, 0, 0, 1}};
    }

    glm::mat4 viewTransformLH() const override
    {
        return camera;
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
                rotate(
                    glm::vec2(_prevMouseCoordsInNdc[COMPONENT::X], _prevMouseCoordsInNdc[COMPONENT::Y]),
                    glm::vec2(currMouseCoordsInNdc[COMPONENT::X], currMouseCoordsInNdc[COMPONENT::Y]));
            }
            else if (state & SDL_BUTTON_RMASK)
            {
                auto proj = glm::perspective(glm::radians(65.f), (float)screenDimension[COMPONENT::X] / (float)screenDimension[COMPONENT::Y], 0.1f, 500.f);
                auto projInverse = glm::inverse(proj);
                auto delta = currMouseCoordsInNdc - _prevMouseCoordsInNdc;
                glm::vec4 dxy4 = projInverse * glm::vec4(
                                                   delta[COMPONENT::X], delta[COMPONENT::Y], 0, 1);
                pan(glm::vec2(dxy4.x, dxy4.y));
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
    glm::quat screen_to_arcball(const glm::vec2 &p)
    {
        const float dist = glm::dot(p, p);
        // If we're on/in the sphere return the point on it
        if (dist <= 1.f)
        {
            return glm::quat(0.0, p.x, p.y, std::sqrt(1.f - dist));
        }
        else
        {
            // otherwise we project the point onto the sphere
            const glm::vec2 proj = glm::normalize(p);
            return glm::quat(0.0, proj.x, proj.y, 0.f);
        }
    }

    void pan(glm::vec2 mouse_delta)
    {
        const float zoom_amount = std::abs(translation[3][2]);
        glm::vec4 motion(mouse_delta.x * zoom_amount, mouse_delta.y * zoom_amount, 0.f, 0.f);
        // Find the panning amount in the world space
        motion = inv_camera * motion;

        center_translation = glm::translate(glm::vec3(motion)) * center_translation;
        update_camera();
    }

    void rotate(glm::vec2 prev_mouse, glm::vec2 cur_mouse)
    {
        // Clamp mouse positions to stay in NDC
        cur_mouse = glm::clamp(cur_mouse, glm::vec2{-1, -1}, glm::vec2{1, 1});
        prev_mouse = glm::clamp(prev_mouse, glm::vec2{-1, -1}, glm::vec2{1, 1});

        const glm::quat mouse_cur_ball = screen_to_arcball(cur_mouse);
        const glm::quat mouse_prev_ball = screen_to_arcball(prev_mouse);

        rotation = mouse_cur_ball * mouse_prev_ball * rotation;
        update_camera();
    }

    void zoom(const float zoom_amount)
    {
        const glm::vec3 motion(0.f, 0.f, zoom_amount);

        translation = glm::translate(motion) * translation;
        update_camera();
    }

    void update_camera()
    {
        camera = translation * glm::mat4_cast(rotation) * center_translation;
        inv_camera = glm::inverse(camera);
        log(Level::Info, "camera mat: ", camera);
        log(Level::Info, "inv_camera mat: ", inv_camera);
    }

    void update_camera1()
    {
        auto rotationMatrix = RotationMatrixFromQuaternion(_rotation);
        _camera = MatrixMultiply4x4(_translation, MatrixMultiply4x4(rotationMatrix, _center_translation));
        _inv_camera = Inverse(_camera);
        log(Level::Info, "_camera mat: ", _camera);
        log(Level::Info, "_inv_camera mat: ", _inv_camera);
    }

    glm::mat4 center_translation, translation;
    glm::quat rotation;
    glm::mat4 camera, inv_camera;

    mat4x4f _center_translation, _translation;
    quatf _rotation{0, 0, 0, 1};
    mat4x4f _camera, _inv_camera;
};