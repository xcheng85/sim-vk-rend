#pragma once

#include <cmath> // sin, cos
#include <assert.h>
#include <numbers> // std::numbers
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <format>
#include <type_traits>
#include <concepts>

#include "vector.h"
#include "matrix.h"
#include "fp.h"

template <typename T>
concept float_or_double_type = std::floating_point<T>;

// template inheritance
// quaternion has 4 components
// sizeof return bytes
template <float_or_double_type T>
struct quaternion : public vec<T, 4, sizeof(T) * 4>
{
    inline quaternion(const T &x, const T &y, const T &z, const T &w) : vec<T, 4, sizeof(T) * 4>(std::array<T, 4>{
                                                                            x, y, z, w})
    {
    }
    // delegate to other ctor
    inline quaternion(const vec<T, 3, sizeof(T) * 4> &n, const T &w)
        : quaternion(n[COMPONENT::X], n[COMPONENT::Y], n[COMPONENT::Z], w)
    {
    }
    // delegate to other ctor
    inline quaternion(const vec<T, 4, sizeof(T) * 4> &q)
        : quaternion(q[COMPONENT::X], q[COMPONENT::Y], q[COMPONENT::Z], q[COMPONENT::W])
    {
    }
    // p.702 introduction to 3d game programming with DX12
    quaternion &operator*=(const quaternion &other)
    {
        this->data[COMPONENT::W] =
            this->data[COMPONENT::W] * other.data[COMPONENT::W] - this->data[COMPONENT::X] * other.data[COMPONENT::X] - this->data[COMPONENT::Y] * other.data[COMPONENT::Y] - this->data[COMPONENT::Z] * other.data[COMPONENT::Z];

        this->data[COMPONENT::X] = this->data[COMPONENT::W] * other.data[COMPONENT::X] + this->data[COMPONENT::X] * other.data[COMPONENT::W] + this->data[COMPONENT::Y] * other.data[COMPONENT::Z] - this->data[COMPONENT::Z] * other.data[COMPONENT::Y];

        this->data[COMPONENT::Y] = this->data[COMPONENT::W] * other.data[COMPONENT::Y] + this->data[COMPONENT::Y] * other.data[COMPONENT::W] + this->data[COMPONENT::Z] * other.data[COMPONENT::X] - this->data[COMPONENT::X] * other.data[COMPONENT::Z];

        this->data[COMPONENT::Z] = this->data[COMPONENT::W] * other.data[COMPONENT::Z] + this->data[COMPONENT::Z] * other.data[COMPONENT::W] + this->data[COMPONENT::X] * other.data[COMPONENT::Y] - this->data[COMPONENT::Y] * other.data[COMPONENT::X];

        return *this;
    }
};

template <float_or_double_type T>
inline std::ostream &operator<<(std::ostream &os, const quaternion<T> &q)
{
    os << std::format("[{}, {}, {}, {}]\n", q.data[0], q.data[1], q.data[2], q.data[3]);
    return os;
}

template <float_or_double_type T>
quaternion<T> operator*(quaternion<T> const &q, quaternion<T> const &p)
{
    return quaternion<T>(q) *= p;
}

using quatf = quaternion<float>;
using quatd = quaternion<double>;

template <float_or_double_type T>
inline quaternion<T> Conjugate(const quaternion<T> &q)
{
    return quaternion<T>(-q[COMPONENT::X], -q[COMPONENT::Y], -q[COMPONENT::Z], q[COMPONENT::W]);
}

// inverse of unit quaternion = its conjunate
// invserse to rotation negative angle
template <float_or_double_type T>
inline quaternion<T> Inverse(const quaternion<T> &q)
{
    return Conjugate(q) / dotProduct(q, q);
}

// build quat from axis and rotation angles in rad
// rotate theta radian alongside axis n
// construct rotation quaternion
// (sin(theta *0.5)*n, cos(theta*0.5))

// Alignet for float: 16 any way
// Align for double: 32 any way
// sizeof(T) * 4 covers both
template <float_or_double_type T>
inline quaternion<T> QuaternionFromAxisAngle(const vec<T, 3, sizeof(T) * 4> &axis, const T &angle)
{
    return quaternion<T>(axis * static_cast<T>(sin(angle * static_cast<T>(0.5))), cos(angle * static_cast<T>(0.5)));
}

// build quat from euler angles
template <float_or_double_type T>
inline quaternion<T> QuaternionFromEulerAngles(T pitch, T yaw, T roll)
{
    quaternion<T> res;
    // https://learnopengl.com/Getting-started/Camera
    auto half{static_cast<T>(0.5)};
    vec<T, 3, sizeof(T) * 4> eulerAngle(std::array<T, 3>{pitch, yaw, roll});
    auto c = functor1<vec, T, 3, sizeof(T) * 4>::call(cos, eulerAngle * half);
    auto s = functor1<vec, T, 3, sizeof(T) * 4>::call(sin, eulerAngle * half);
    // axis
    res[COMPONENT::X] = s[COMPONENT::X] * c[COMPONENT::Y] * c[COMPONENT::Z] - c[COMPONENT::X] * s[COMPONENT::Y] * s[COMPONENT::Z];
    res[COMPONENT::Y] = c[COMPONENT::X] * s[COMPONENT::Y] * c[COMPONENT::Z] + s[COMPONENT::X] * c[COMPONENT::Y] * s[COMPONENT::Z];
    res[COMPONENT::Z] = c[COMPONENT::X] * c[COMPONENT::Y] * s[COMPONENT::Z] - s[COMPONENT::X] * s[COMPONENT::Y] * c[COMPONENT::Z];
    // angle
    res[COMPONENT::W] = c[COMPONENT::X] * c[COMPONENT::Y] * c[COMPONENT::Z] + s[COMPONENT::X] * s[COMPONENT::Y] * s[COMPONENT::Z];
    return res;
}

// deduct axis from quaternion
// page 706: Introduction to 3D Game Programming with Directx12
template <float_or_double_type T>
inline T RotationAngleFromQuaternion(const quaternion<T> &q)
{
    const auto x = q[COMPONENT::X], y = q[COMPONENT::Y], z = q[COMPONENT::Z], w = q[COMPONENT::W];
    // cos(1/2)
    if (abs(q[COMPONENT::W]) > T(0.877582561890372716130286068203503191))
    {
        // ||u|| = sin(angle/2)
        T const angle = asin(sqrt(x * x + y * y + z * z)) * static_cast<T>(2);
        if (w < static_cast<T>(0))
        {
            return static_cast<T>(std::numbers::pi) * static_cast<T>(2) - angle;
        }
        return angle;
    }
    // w = cos(angle/2)
    // angle = acos * 2
    return acos(w) * static_cast<T>(2);
}

// n = vector(x, y, z) / ||u||
// ||u|| = sqrt(1 - w * w)
template <float_or_double_type T>
inline vec<T, 3, sizeof(T) * 4> RotationAxisFromQuaternion(const quaternion<T> &q)
{
    const auto x = q[COMPONENT::X], y = q[COMPONENT::Y], z = q[COMPONENT::Z], w = q[COMPONENT::W];

    vec<T, 3, sizeof(T) * 4> res(
        std::array<T, 3>{
            x, y, z});

    const auto u = sqrt(static_cast<T>(1) - w * w);

    res /= u;
    return res;
}

// 3x3 rotation matrix to quaternion
// page 711. Introduction to 3D Game Programming with DirectX 12
template <float_or_double_type T>
inline quaternion<T> QuaternionFromRotationMatrix(const mat<T, 3, sizeof(T) * 16> &m)
{
    // find the largest diagonal element of R to divide by
    T fourXSquaredMinus1 = m.data[0][0] - m.data[1][1] - m.data[2][2];
    T fourYSquaredMinus1 = m.data[1][1] - m.data[0][0] - m.data[2][2];
    T fourZSquaredMinus1 = m.data[2][2] - m.data[0][0] - m.data[1][1];
    T fourWSquaredMinus1 = m.data[0][0] + m.data[1][1] + m.data[2][2];

    int biggestIndex = 0;
    T fourBiggestSquaredMinus1 = fourWSquaredMinus1;
    if (fourXSquaredMinus1 > fourBiggestSquaredMinus1)
    {
        fourBiggestSquaredMinus1 = fourXSquaredMinus1;
        biggestIndex = 1;
    }
    if (fourYSquaredMinus1 > fourBiggestSquaredMinus1)
    {
        fourBiggestSquaredMinus1 = fourYSquaredMinus1;
        biggestIndex = 2;
    }
    if (fourZSquaredMinus1 > fourBiggestSquaredMinus1)
    {
        fourBiggestSquaredMinus1 = fourZSquaredMinus1;
        biggestIndex = 3;
    }

    T biggestVal = sqrt(fourBiggestSquaredMinus1 + static_cast<T>(1)) * static_cast<T>(0.5);
    T mult = static_cast<T>(0.25) / biggestVal;

    switch (biggestIndex)
    {
    case 0:
        return quaternion<T>(
            (m.data[1][2] - m.data[2][1]) * mult,
            (m.data[2][0] - m.data[0][2]) * mult,
            (m.data[0][1] - m.data[1][0]) * mult,
            biggestVal);
    case 1:
        return quaternion<T>(
            biggestVal,
            (m.data[0][1] + m.data[1][0]) * mult,
            (m.data[2][0] + m.data[0][2]) * mult,
            (m.data[1][2] - m.data[2][1]) * mult);
    case 2:
        return quaternion<T>(
            (m.data[0][1] + m.data[1][0]) * mult,
            biggestVal,
            (m.data[1][2] + m.data[2][1]) * mult,
            (m.data[2][0] - m.data[0][2]) * mult);
    case 3:
        return quaternion<T>(
            (m.data[2][0] + m.data[0][2]) * mult,
            (m.data[1][2] + m.data[2][1]) * mult,
            biggestVal,
            (m.data[0][1] - m.data[1][0]) * mult);
    default:
        assert(false);
        return quaternion<T>(0, 0, 0, 1);
    }
}

// convert Unit Quaternion to matrix
// page 710. Introduction to 3D Game Programming with DirectX 12
template <float_or_double_type T>
inline mat<T, 4, sizeof(T) * 16> RotationMatrixFromQuaternion(const quaternion<T> &q)
{
    mat<T, 4, sizeof(T) * 16> res;
    T q1 = q[COMPONENT::X];
    T q2 = q[COMPONENT::Y];
    T q3 = q[COMPONENT::Z];
    T q4 = q[COMPONENT::W];

    // q1 * q1
    T q11 = q1 * q1, q22 = q2 * q2, q33 = q3 * q3;
    T q12 = q1 * q2, q34 = q3 * q4, q13 = q1 * q3;
    T q24 = q2 * q4, q23 = q2 * q3, q14 = q1 * q4;

    res.data[0][0] = T(1) - T(2) * (q22 + q33);
    res.data[0][1] = T(2) * (q12 + q34);
    res.data[0][2] = T(2) * (q13 + q24);

    res.data[1][0] = T(2) * (q12 - q34);
    res.data[1][1] = T(1) - T(2) * (q11 + q33);
    res.data[1][2] = T(2) * (q23 + q14);

    res.data[2][0] = T(2) * (q13 + q24);
    res.data[2][1] = T(2) * (q23 - q14);
    res.data[2][2] = T(1) - T(2) * (q11 + q22);

    res.data[3][3] = 1;

    return res;
}

// // dealing with rotation with unique quaternion

// // multiply a unit quaternion results in a rotatation
// // page 702 quaternion product

// // real number to quaternion
// // vector to quaternion

// // inverse and conjugate

// // polar representation of unit quaternion
// // polar representation of a conjugate unit quaternion, -theta
// // leads to axis of rotation + theta

// // eular angles to quat

// // unit quaternion to matrix

// // rotation matrix to quaternion

// // vector v ===> quaternion (v, 0)

// // quaternion multiplication formula
// in opengl
// // eular angle: pitch, yaw, roll
// // seq: roll(z) -> yaw (y) -> pitch(x)
// // concat of three rotations = multiplication of three quats
