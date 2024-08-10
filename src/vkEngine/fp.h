#pragma once

#include <cmath> // sin, cos
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <format>
#include <functional>

#include "vector.h"

template <template <typename T, size_t N, size_t Alignment> class vec, typename T, size_t N, size_t Alignment>
struct functor1
{
};

// partial specialization on size_t N
template <template <typename T, size_t N, size_t Alignment> class vec, typename T, size_t Alignment>
struct functor1<vec, T, 1, Alignment>
{
    inline static vec<T, 1, Alignment> call(T (*Func)(T x), vec<T, 1, Alignment> const &v)
    {
        return vec<T, 1, Alignment>(Func(v[COMPONENT::X]));
    }
};

template <template <typename T, size_t N, size_t Alignment> class vec, size_t Alignment, typename T>
struct functor1<vec, T, 2, Alignment>
{
    inline constexpr static vec<T, 2, Alignment> call(T (*Func)(T x), vec<T, 2, Alignment> const &v)
    {
        return vec<T, 2, Alignment>(Func(v[COMPONENT::X]), Func(v[COMPONENT::Y]));
    }
};

template <template <typename T, size_t N, size_t Alignment> class vec, typename T>
struct functor1<vec, T, 3, sizeof(T) * 4>
{
    inline constexpr static vec<T, 3, sizeof(T) * 4> call(T (*Func)(T x), vec<T, 3, sizeof(T) * 4> const &v)
    {
        return vec<T, 3, sizeof(T) * 4>(std::array<T, 3>{Func(v[COMPONENT::X]), Func(v[COMPONENT::Y]), Func(v[COMPONENT::Z])});
    }
};

template <template <typename T, size_t N, size_t Alignment> class vec, typename T, size_t N, size_t Alignment>
struct functor1_lamda
{
};

template <template <typename T, size_t N, size_t Alignment> class vec, typename T, size_t Alignment>
struct functor1_lamda<vec, T, 2, Alignment>
{
    inline static vec<T, 2, Alignment> call(std::function<T(T)> Func, vec<T, 2, Alignment> const &v)
    {
        return vec<T, 2, Alignment>(std::array<T, 2>{Func(v[COMPONENT::X]), Func(v[COMPONENT::Y])});
    }
};

template <typename T, size_t N, size_t Alignment>
inline vec<T, N, Alignment> rad(const vec<T, N, Alignment> &degree)
{
    vec<T, N, Alignment> res;
    for(size_t i = 0; i < N; ++i) {
        res.data[i] = degree.data[i] * static_cast<T>(0.01745329251994329576923690768489);
    }
    return res;
}

template <typename T>
inline T rad(T degree)
{
    return rad(vec<T, 1, sizeof(T)>(std::array{degree})).data[0];
}