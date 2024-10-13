#pragma once

#include <array>
#include <string>
#include <utility>
#include <fstream>
#include <sstream>
#include <iostream>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <vector.h>
#include <matrix.h>
#include <fp.h>
#include <misc.h>

using namespace std;

class CameraBase
{
public:
    enum CameraActionType : int
    {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT
    };

    virtual ~CameraBase()
    {
    }

    // virtual mat4x4f viewTransformLH() const = 0;
    // virtual vec3f viewPos() const = 0;
    virtual glm::mat4 viewTransformLH() const = 0;
    virtual glm::vec3 viewPos() const = 0;
    virtual void handleKeyboardEvent(CameraActionType actionType, float dt) = 0;
    virtual void handleMouseCursorEvent(
        int button,
        int state,
        const vec2i &currMouseInScreenSpace,
        const vec2i &screenDimension) = 0;
    virtual void handleMouseClickEvent(int button, int state, int x, int y) = 0;
    virtual void handleMouseWheelEvent(float v) = 0;

    // for fustrum volume
    inline float nearPlaneD() const
    {
        return _nearPlaneD;
    }
    inline float farPlaneD() const
    {
        return _farPlaneD;
    }
    inline float verticalFov() const
    {
        return _verticalFov;
    }
    inline float aspect() const
    {
        return _aspect;
    }
    virtual Fustrum fustrumPlanes() const= 0;

protected:
    vec2f _prevMouseCoordsInNdc{-2.f};

    glm::vec3 _eye;
    float _nearPlaneD{0.0f};
    float _farPlaneD{1.0f};
    float _verticalFov{65.f};
    float _aspect{1.0f};
};
