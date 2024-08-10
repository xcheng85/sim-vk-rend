#pragma once
#include <cstring> // memset
#include <cmath> // sin, cos
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <format>

// #pragma GCC optimize("unroll-loops")
// uname -p: x86_64
// x64 platform support SSE2 by default

// SIMD
// cross-platform
enum COMPONENT : int
{
    X = 0,
    Y,
    Z,
    W
};

template <typename T, size_t N, size_t Alignment = 16>
struct alignas(Alignment) vec
{
T data[N];
vec() {
    memset(data, 0, sizeof(T) * N);
};
vec(const vec &) = default;
vec(vec &&) noexcept = default;
vec &operator=(const vec &) = default;
vec &operator=(vec &&) noexcept = default;

vec(const std::array<T, N> &a)
{
    int dst{0};
    for (const auto &v : a)
    {
        data[dst++] = v;
    }
}

// compiler defined macro: indicate c++ version
#if (__cplusplus >= 202002L)
bool operator==(const vec &) const = default;
    auto operator<=>(const vec &) const = default;
#endif

T &operator[](COMPONENT index)
{
    return data[index];
}

T operator[](COMPONENT index) const
{
    return data[index];
}

vec &operator-(const vec &other)
{
    for (int i = 0; i < N; i++)
    {
        data[i] -= other.data[i];
    }
    return *this;
}

vec &operator+=(const vec &other)
{
    for (int i = 0; i < N; i++)
    {
        data[i] += other.data[i];
    }
    return *this;
}

vec &operator-=(const vec &other)
{
    for (int i = 0; i < N; i++)
    {
        data[i] -= other.data[i];
    }
    return *this;
}

vec &operator*=(const T &s)
{
    for (int i = 0; i < N; i++)
    {
        data[i] *= s;
    }
    return *this;
}

vec &operator/=(const T &s)
{
    for (int i = 0; i < N; i++)
    {
        data[i] /= s;
    }
    return *this;
}

double vectorLength() const noexcept
{
return sqrt(dotProduct(*this, *this));
}

void normalize() noexcept
{
auto vectorlength = vectorLength();

if (vectorlength > 0)
{
vectorlength = 1.0f / vectorlength;
}

data[0] *= vectorlength;
data[1] *= vectorlength;
data[2] *= vectorlength;
data[3] *= vectorlength;
}
};

template <typename T, size_t N, size_t Alignment>
inline vec<T, N, Alignment> operator+(const vec<T, N, Alignment> &v1, const vec<T, N, Alignment> &v2)
{
    vec<T, N, Alignment> res;
    for (int i = 0; i < N; i++)
    {
        res.data[i] = v1.data[i] + v2.data[i];
    }
    return res;
}

template <typename T, size_t N, size_t Alignment>
inline vec<T, N, Alignment> operator-(const vec<T, N, Alignment> &v1, const vec<T, N, Alignment> &v2)
{
    vec<T, N, Alignment> res;
    for (int i = 0; i < N; i++)
    {
        res.data[i] = v1.data[i] - v2.data[i];
    }
    return res;
}

template <typename T, size_t N, size_t Alignment>
inline vec<T, N, Alignment> operator*(const vec<T, N, Alignment> &v, const T &s)
{
    vec<T, N, Alignment> res;
    for (int i = 0; i < N; i++)
    {
        res.data[i] = v.data[i] * s;
    }
    return res;
}

template <typename T, size_t N, size_t Alignment>
inline vec<T, N, Alignment> normalize(const vec<T, N, Alignment> &v)
{
    vec<T, N, Alignment> res;
    auto vectorlength = v.vectorLength();
    for (size_t i{0}; i < N; ++i)
    {
        res.data[i] = v.data[i] / vectorlength;
    }
    return res;
}

template <typename T, size_t N, size_t Alignment>
inline double dotProduct(const vec<T, N, Alignment> &v1, const vec<T, N, Alignment> &v2) noexcept
{
float res{0.f};
for (int i = 0; i < N; ++i)
{
res += v1.data[i] * v2.data[i];
}
return res;
}

template <typename T>
inline vec<T, 3, sizeof(T) * 4> crossProduct(const vec<T, 3, sizeof(T) * 4> &v1, const vec<T, 3, sizeof(T) * 4> &v2) noexcept
{
// similar to cramer's rule
// [ V1.y*V2.z - V1.z*V2.y, V1.z*V2.x - V1.x*V2.z, V1.x*V2.y - V1.y*V2.x ]
return vec<T, 3, sizeof(T) * 4>(std::array{
        v1[COMPONENT::Y] * v2[COMPONENT::Z] - v1[COMPONENT::Z] * v2[COMPONENT::Y],
        v1[COMPONENT::Z] * v2[COMPONENT::X] - v1[COMPONENT::X] * v2[COMPONENT::Z],
        v1[COMPONENT::X] * v2[COMPONENT::Y] - v1[COMPONENT::Y] * v2[COMPONENT::X],
});
}

template <typename T, size_t N, size_t Alignment>
inline std::ostream &operator<<(std::ostream &os, const vec<T, N, Alignment> &v)
{
    os << "[ ";
    for (int i = 0; i < N; ++i)
    {
        os << v.data[i] << " ";
    }
    os << "]\n";
    return os;
}

// for glsl
template <typename T, size_t N, size_t Alignment>
inline T *value_ptr(vec<T, N, Alignment> &v)
{
    return &(v.data[0]);
}

// 8 byte
using vec2f = vec<float, 2, 8>;
// 12 byte --> 16
using vec3f = vec<float, 3, 16>;
// 32 bytes = 4 bytes (float) * 4
using vec4f = vec<float, 4, 16>;

using vec2d = vec<double, 2, 16>;
using vec3d = vec<double, 3, 32>;
using vec4d = vec<double, 4, 64>;

// // only square matrix have inverse
// // generic solution:
// // inverse of matrix = adjoint / determinant(A)
// // determinant(A) == 0 singular no inverse

// // quick for affine transformation (rotation + scale)

// // inverse of transformation matrix
// // inverse of orthogonal mtrics = transpose

// // generic: SVD = R1 * s * R2
// // inverse:
