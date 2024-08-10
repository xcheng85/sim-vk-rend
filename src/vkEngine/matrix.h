#pragma once

#include <cmath> // sin, cos
#include <cstdint>
#include <array>
#include <initializer_list>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <format>
#include "vector.h"
#include <cstring> // memset

template <typename T, size_t N, size_t Alignment>
struct mat
{
    // Directx: row-major
    // float _00, _01, _02;
    // float _10, _11, _12;
    // float _20, _21, _22;
    // OpenGL/Vulkan: column-major

    // row-major or column-major
    float data[N][N];
    mat()
    {
        memset(data, 0, sizeof(T) * N * N);
    };

    mat(const mat &) = default;
    mat &operator=(const mat &) = default;

    mat(mat &&) noexcept = default;
    mat &operator=(mat &&) noexcept = default;
    static mat identity()
    {
        mat res;
        for (size_t i = 0; i < N; ++i)
        {
            res.data[i][i] = 1;
        }
        return res;
    }
    // column-major
    mat(const std::array<T, N * N> &a)
    {
        memset(data, 0, sizeof(T) * N * N);
        int dst{0};
        int r, c;
        for (const auto v : a)
        {
            // column-major
            c = dst / N;
            r = dst % N;
            data[r][c] = v;
            ++dst;
        }
    }

    // diagnol
    mat(const T &v)
    {
        memset(data, 0, sizeof(T) * N * N);
        for (size_t i = 0; i < N; ++i)
        {
            data[i][i] = v;
        }
    }

    T operator()(size_t r, size_t c) const noexcept
    {
        return data[r][c];
    }
    T &operator()(size_t r, size_t c) noexcept
    {
        return data[r][c];
    }

#if (__cplusplus >= 202002L)
    bool operator==(const mat &) const = default;
    auto operator<=>(const mat &) const = default;
#endif
};

template <typename T, size_t N, size_t Alignment>
inline std::ostream &operator<<(std::ostream &os, const mat<T, N, Alignment> &v)
{
    for (int r = 0; r < N; ++r)
    {
        for (int c = 0; c < N; ++c)
        {
            os << v.data[r][c] << " ";
        }
        os << "\n";
    }
    return os;
}

template <typename T, size_t N, size_t Alignment>
inline T *value_ptr(mat<T, N, Alignment> &v)
{
    return &(v.data[0][0]);
}

// 4 * 4
using mat2x2f = mat<float, 2, 16>;

using mat3x3f = mat<float, 3, 16>;

// 16 * 4
using mat4x4f = mat<float, 4, 64>;

// affine transformation
// linear transformation (S + R) + translation (T)
// not commutative

// colum-major
template <typename T>
inline mat<T, 4, sizeof(T) * 16> MatrixMultiply4x4(const mat<T, 4, sizeof(T) * 16> &m1, const mat<T, 4, sizeof(T) * 16> &m2)
{
    mat<T, 4, sizeof(T) * 16> res;
    for (int r = 0; r < 4; ++r)
    {
        auto x = m1.data[r][0];
        auto y = m1.data[r][1];
        auto z = m1.data[r][2];
        auto w = m1.data[r][3];

        res.data[r][0] = (m2.data[0][0] * x) + (m2.data[1][0] * y) + (m2.data[2][0] * z) + (m2.data[3][0] * w);
        res.data[r][1] = (m2.data[0][1] * x) + (m2.data[1][1] * y) + (m2.data[2][1] * z) + (m2.data[3][1] * w);
        res.data[r][2] = (m2.data[0][2] * x) + (m2.data[1][2] * y) + (m2.data[2][2] * z) + (m2.data[3][2] * w);
        res.data[r][3] = (m2.data[0][3] * x) + (m2.data[1][3] * y) + (m2.data[2][3] * z) + (m2.data[3][3] * w);
    }

    // // colum-major
    // for (int c = 0; c < 4; ++c)
    // {
    //     auto x = m2.data[0][c];
    //     auto y = m2.data[1][c];
    //     auto z = m2.data[2][c];
    //     auto w = m2.data[3][c];

    //     res.data[0][c] = (m1.data[0][0] * x) + (m1.data[0][1] * y) + (m1.data[0][2] * z) + (m1.data[0][3] * w);
    //     res.data[1][c] = (m1.data[1][0] * x) + (m1.data[1][1] * y) + (m1.data[1][2] * z) + (m1.data[1][3] * w);
    //     res.data[2][c] = (m1.data[2][0] * x) + (m1.data[2][1] * y) + (m1.data[2][2] * z) + (m1.data[2][3] * w);
    //     res.data[3][c] = (m1.data[3][0] * x) + (m1.data[3][1] * y) + (m1.data[3][2] * z) + (m1.data[3][3] * w);
    // }

    return res;
}

template <typename T>
inline vec<T, 4, sizeof(T) * 4> MatrixMultiplyVector4x4(const mat<T, 4, sizeof(T) * 16> &m, const vec<T, 4, sizeof(T) * 4> &v)
{
    // opengl: m * v  4*4 and 4 * 1
    // directx: v * m  1 * 4 and 4*4
    vec<T, 4, sizeof(T) * 4> res;
    auto x = v.data[0];
    auto y = v.data[1];
    auto z = v.data[2];
    auto w = v.data[3];

    res.data[0] = (m.data[0][0] * x) + (m.data[0][1] * y) + (m.data[0][2] * z) + (m.data[0][3] * w);
    res.data[1] = (m.data[1][0] * x) + (m.data[1][1] * y) + (m.data[1][2] * z) + (m.data[1][3] * w);
    res.data[2] = (m.data[2][0] * x) + (m.data[2][1] * y) + (m.data[2][2] * z) + (m.data[2][3] * w);
    res.data[3] = (m.data[3][0] * x) + (m.data[3][1] * y) + (m.data[3][2] * z) + (m.data[3][3] * w);

    return res;
}

template <typename T>
inline mat<T, 4, sizeof(T) * 16> MatrixScale4x4(T sx, T sy, T sz)
{
    mat<T, 4, sizeof(T) * 16> res;
    res.data[0][0] = sx;
    res.data[1][1] = sy;
    res.data[2][2] = sz;
    res.data[3][3] = 1.0f;
    return res;
}

template <typename T>
inline mat<T, 4, sizeof(T) * 16> MatrixScale4x4(const vec<T, 3, sizeof(T) * 4> &scaleVector)
{
    return MatrixScale4x4(
            scaleVector[COMPONENT::X],
            scaleVector[COMPONENT::Y],
            scaleVector[COMPONENT::Z]);
}

template <typename T>
inline mat<T, 4, sizeof(T) * 16> MatrixTranslation4x4(T tx, T ty, T tz)
{
    mat<T, 4, sizeof(T) * 16> res;
    res.data[0][0] = 1.0f;
    res.data[1][1] = 1.0f;
    res.data[2][2] = 1.0f;
    res.data[3][0] = tx;
    res.data[3][1] = ty;
    res.data[3][2] = tz;
    res.data[3][3] = 1.0f;
    return res;
}

template <typename T>
inline mat<T, 4, sizeof(T) * 16> MatrixTranslation4x4(const vec<T, 3, sizeof(T) * 4> &translationVector)
{
    return MatrixTranslation4x4(
            translationVector[COMPONENT::X],
            translationVector[COMPONENT::Y],
            translationVector[COMPONENT::Z]);
}

// to do: a 11-degree minimax approximation for sine; 10-degree for cosine.
// opengl: counter-clock-wise
template <typename T>
inline mat<T, 4, sizeof(T) * 16> MatrixRotationX4x4(T angleInRadian)
{
    auto fSinAngle = sin(angleInRadian);
    auto fCosAngle = cos(angleInRadian);
    // page 196: Fundamentals of Computer Graphics
    // page 42: Computer Graphics Programming in OpengGL with C++, 3/E
    // column-major convention
    mat<T, 4, sizeof(T) * 16> res;
    res.data[0][0] = 1.0f;
    res.data[0][1] = 0.0f;
    res.data[0][2] = 0.0f;
    res.data[0][3] = 0.0f;

    res.data[1][0] = 0.0f;
    res.data[1][1] = fCosAngle;
    res.data[1][2] = fSinAngle;
    res.data[1][3] = 0.0f;

    res.data[2][0] = 0.0f;
    // text book typo ?
    res.data[2][1] = -fSinAngle;
    res.data[2][2] = fCosAngle;
    res.data[2][3] = 0.0f;

    res.data[3][0] = 0.0f;
    res.data[3][1] = 0.0f;
    res.data[3][2] = 0.0f;
    res.data[3][3] = 1.0f;
    return res;
}

template <typename T>
inline mat<T, 4, sizeof(T) * 16> MatrixRotationY4x4(T angleInRadian)
{
    auto fSinAngle = sin(angleInRadian);
    auto fCosAngle = cos(angleInRadian);
    // page 196: Fundamentals of Computer Graphics
    mat<T, 4, sizeof(T) * 16> res;
    res.data[0][0] = fCosAngle;
    res.data[0][1] = 0.0f;
    res.data[0][2] = -fSinAngle;
    res.data[0][3] = 0.0f;

    res.data[1][0] = 0.0f;
    res.data[1][1] = 1.f;
    res.data[1][2] = 0.0f;
    res.data[1][3] = 0.0f;

    res.data[2][0] = fSinAngle;
    res.data[2][1] = 0;
    res.data[2][2] = -fCosAngle;
    res.data[2][3] = 0.0f;

    res.data[3][0] = 0.0f;
    res.data[3][1] = 0.0f;
    res.data[3][2] = 0.0f;
    res.data[3][3] = 1.0f;
    return res;
}

template <typename T = float>
inline mat<T, 4, sizeof(T) * 16> MatrixRotationZ4x4(T angleInRadian)
{
    auto fSinAngle = sin(angleInRadian);
    auto fCosAngle = cos(angleInRadian);

    mat<T, 4, sizeof(T) * 16> res;
    res.data[0][0] = fCosAngle;
    res.data[0][1] = fSinAngle;
    res.data[0][2] = 0;
    res.data[0][3] = 0.0f;

    res.data[1][0] = -fSinAngle;
    res.data[1][1] = fCosAngle;
    res.data[1][2] = 0.0f;
    res.data[1][3] = 0.0f;

    res.data[2][0] = 0;
    res.data[2][1] = 0;
    res.data[2][2] = 1;
    res.data[2][3] = 0.0f;

    res.data[3][0] = 0.0f;
    res.data[3][1] = 0.0f;
    res.data[3][2] = 0.0f;
    res.data[3][3] = 1.0f;
    return res;
}

// approaches
// 1. concat three rotation matrix
// 2. Derive Rodrigues' Formula
// 3. quaternion

template <typename T>
inline mat<T, 4, sizeof(T) * 16> MatrixRotationAxis4x4(const vec<T, 3, sizeof(T) * 4> &axis, T angleInRadian)
{
    const auto c{cos(angleInRadian)};
    const auto s{sin(angleInRadian)};
    const auto axisNormalized = normalize(axis);
    // 1 - c
    // const auto tmp{1 - c};
    // vec<3, T, Q> temp((T(1) - c) * axis);
    mat<T, 4, sizeof(T) * 16> Rotate;
    const auto rx = axisNormalized[COMPONENT::X];
    const auto ry = axisNormalized[COMPONENT::Y];
    const auto rz = axisNormalized[COMPONENT::Z];

    // unoptimized for readability
    // transpose of rot.png (which is row vector)
    // represented as a column vector.
    // column-major: m * v
    // row-major: v * m
    Rotate.data[0][0] = c + rx * rx * (1 - c);
    Rotate.data[0][1] = ry * rx * (1 - c) + rz * s;
    Rotate.data[0][2] = rz * rx * (1 - c) - ry * s;
    Rotate.data[0][3] = 0;

    Rotate.data[1][0] = rx * ry * (1 - c) - rz * s;
    Rotate.data[1][1] = c + ry * ry * (1 - c);
    Rotate.data[1][2] = rz * ry * (1 - c) + rx * s;
    Rotate.data[1][3] = 0;

    Rotate.data[2][0] = rx * rz * (1 - c) + ry * s;
    Rotate.data[2][1] = ry * rz * (1 - c) - rx * s;
    Rotate.data[2][2] = c + rz * rz * (1 - c);
    Rotate.data[2][3] = 0;

    Rotate.data[3][0] = 0;
    Rotate.data[3][1] = 0;
    Rotate.data[3][2] = 0;
    Rotate.data[3][3] = 1;

    return Rotate;
}

// // camera/view transformation, v in mvp
// // 1. world position of camera
// // 2. world position of target point
// // 3. world camera up vector (not necessarily equal canonical world up vector)

// // basically create new orthogonal basis
// f: z in camera space
// s: x in camera space
// u: y in camera space

// left-hand
// v = View * Model * V

template <typename T = float>
inline mat<T, 4, sizeof(T) * 16> ViewTransformLH4x4(const vec<T, 3, sizeof(T) * 4> &pos,
                                                    const vec<T, 3, sizeof(T) * 4> &target,
                                                    const vec<T, 3, sizeof(T) * 4> &up)
{
    mat<T, 4, sizeof(T) * 16> m;
    // requires global version operator-
    const auto z = normalize(target - pos);
    auto x = normalize(crossProduct(up, z));

    // no need to normalize
    // s and f both normalized
    auto y{crossProduct(z, x)};

    // page 50: Computer Graphics Programming in OpengGL with C++, 3/E
    m.data[0][0] = x[COMPONENT::X];
    m.data[1][0] = x[COMPONENT::Y];
    m.data[2][0] = x[COMPONENT::Z];

    m.data[0][1] = y[COMPONENT::X];
    m.data[1][1] = y[COMPONENT::Y];
    m.data[2][1] = y[COMPONENT::Z];

    m.data[0][2] = z[COMPONENT::X];
    m.data[1][2] = z[COMPONENT::Y];
    m.data[2][2] = z[COMPONENT::Z];

    m.data[3][0] = -dotProduct(x, pos);
    m.data[3][1] = -dotProduct(y, pos);
    m.data[3][2] = -dotProduct(z, pos);
    m.data[3][3] = 1.0f;
    return m;
}

// 1. n
// 2. f
// 3. vertical fov
// 4. image aspect ratio (w/h)

// from view space to ndc
// before the w-divide, z is not in 0 1, no non-linear loss,

// after the homo-divide, z is in [0, 1]
template <typename T = float>
inline mat<T, 4, sizeof(T) * 16> PerspectiveProjectionTransformLH(T near, T far, T vfov, T aspect)
{
    // page 53: Computer Graphics Programming in OpengGL with C++, 3/E
    mat<T, 4, sizeof(T) * 16> m;
    // avoid implicit type conversion
    const T q = static_cast<T>(1) / (tan(vfov / static_cast<T>(2)));
    const T A = q / aspect;
    // difference from text book: here assum near far are positive
    // textbook assume near and far are negative
    // const T B = (near + far) / (near - far);
    const T B = (near + far) / (far - near);
    const T C = (static_cast<T>(2) * near * far) / (near - far);
    m.data[0][0] = A;
    m.data[1][1] = q;
    m.data[2][2] = B;
    // text book: -1
    m.data[2][3] = static_cast<T>(1);
    m.data[3][2] = C;
    return m;
}