#pragma once

#include <windows.h>

#include "rt64/rt64.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "gfx_rt64_context.hpp"

// Small perf-timer, affine-transform, and interpolation helpers shared by the game-thread and
// render-thread halves of the RT64 backend. Header-only so both sides can inline them freely.

static inline LARGE_INTEGER gfx_rt64_profile_marker(void) {
    LARGE_INTEGER marker;
    QueryPerformanceCounter(&marker);
    return marker;
}

static inline LARGE_INTEGER gfx_rt64_profile_delta(LARGE_INTEGER start, LARGE_INTEGER end) {
    LARGE_INTEGER delta;
    delta.QuadPart = end.QuadPart - start.QuadPart;
    delta.QuadPart *= 1000000;
    delta.QuadPart /= RT64.frequency.QuadPart;
    return delta;
}

static inline RT64_VECTOR3 transform_position_affine(RT64_MATRIX4 m, RT64_VECTOR3 v) {
    RT64_VECTOR3 o;
    o.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
    o.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
    o.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
    return o;
}

static inline RT64_VECTOR3 transform_direction_affine(RT64_MATRIX4 m, RT64_VECTOR3 v) {
    RT64_VECTOR3 o;
    o.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0];
    o.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1];
    o.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2];
    return o;
}

static inline float vector_length(RT64_VECTOR3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline RT64_VECTOR3 normalize_vector(RT64_VECTOR3 v) {
    float length = vector_length(v);
    if (length <= 0.0f) { return { 0.0f, 0.0f, 0.0f }; }
    return { v.x / length, v.y / length, v.z / length };
}

static inline float vector_dot_product(RT64_VECTOR3 a, RT64_VECTOR3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Mirrors Lights.hlsli's ComputePointLightBasis exactly - kept in step by hand, same as the
// RT64_LIGHT/LightInfo structs themselves. Yaw around world up, then pitch around the yawed right
// axis, then roll around the resulting forward axis. Takes degrees, the units RT64_LIGHT itself
// stores pitch/yaw/roll in; Lights.hlsli's own copy takes radians, converted by the CPU before the
// shader ever reads it (see RT64::Scene::setLights in rt64_scene.cpp).
static inline void gfx_rt64_point_light_basis(float pitchDegrees, float yawDegrees, float rollDegrees, RT64_VECTOR3 *outForward, RT64_VECTOR3 *outRight, RT64_VECTOR3 *outUp) {
    const float degToRad = (float)(M_PI) / 180.0f;
    const float pitch = pitchDegrees * degToRad;
    const float yaw = yawDegrees * degToRad;
    const float roll = rollDegrees * degToRad;
    const float sinYaw = sinf(yaw), cosYaw = cosf(yaw);
    const RT64_VECTOR3 forwardYawed = { sinYaw, 0.0f, cosYaw };
    const RT64_VECTOR3 rightYawed = { cosYaw, 0.0f, -sinYaw };
    const RT64_VECTOR3 worldUp = { 0.0f, 1.0f, 0.0f };

    const float sinPitch = sinf(pitch), cosPitch = cosf(pitch);
    const RT64_VECTOR3 forwardPitched = { (cosPitch * forwardYawed.x) + (sinPitch * worldUp.x), (cosPitch * forwardYawed.y) + (sinPitch * worldUp.y), (cosPitch * forwardYawed.z) + (sinPitch * worldUp.z) };
    const RT64_VECTOR3 upPitched = { (-sinPitch * forwardYawed.x) + (cosPitch * worldUp.x), (-sinPitch * forwardYawed.y) + (cosPitch * worldUp.y), (-sinPitch * forwardYawed.z) + (cosPitch * worldUp.z) };

    const float sinRoll = sinf(roll), cosRoll = cosf(roll);
    *outForward = forwardPitched;
    outRight->x = (cosRoll * rightYawed.x) + (sinRoll * upPitched.x);
    outRight->y = (cosRoll * rightYawed.y) + (sinRoll * upPitched.y);
    outRight->z = (cosRoll * rightYawed.z) + (sinRoll * upPitched.z);
    outUp->x = (-sinRoll * rightYawed.x) + (cosRoll * upPitched.x);
    outUp->y = (-sinRoll * rightYawed.y) + (cosRoll * upPitched.y);
    outUp->z = (-sinRoll * rightYawed.z) + (cosRoll * upPitched.z);
}

// The inverse of gfx_rt64_point_light_basis - recovers the pitch/yaw/roll that would produce the
// given (assumed orthonormal) forward/right basis - up is never needed, since roll is fully
// recoverable from where right ends up relative to the pitch/yaw-only basis alone. Needed because
// a geo-layout-attached Point Light's pitch/yaw/roll describe a direction in the actor's own local
// space, but only a full basis - not a set of Euler angles - can be carried through an arbitrary
// transform matrix (transform_direction_affine) the way srcLight.position already goes through
// transform_position_affine. This turns the transformed basis back into the same angle
// representation the shader itself expects on RT64_LIGHT.
static inline void gfx_rt64_point_light_angles(RT64_VECTOR3 forward, RT64_VECTOR3 right, float *outPitch, float *outYaw, float *outRoll) {
    // asinf/atan2f return radians; gfx_rt64_point_light_basis takes degrees and RT64_LIGHT's own
    // pitch/yaw/roll fields are documented as degrees, so every angle here has to be converted
    // before it is used or returned - this is the inverse of gfx_rt64_point_light_basis, which
    // converts the other way.
    const float radToDeg = 180.0f / (float)(M_PI);
    const float clampedForwardY = std::min(std::max(forward.y, -1.0f), 1.0f);
    *outPitch = asinf(clampedForwardY) * radToDeg;
    *outYaw = atan2f(forward.x, forward.z) * radToDeg;

    RT64_VECTOR3 rightYawed, upPitched, unusedForward;
    gfx_rt64_point_light_basis(*outPitch, *outYaw, 0.0f, &unusedForward, &rightYawed, &upPitched);
    *outRoll = atan2f(vector_dot_product(right, upPitched), vector_dot_product(right, rightYawed)) * radToDeg;
}

static inline void gfx_matrix_mul(float res[4][4], const float a[4][4], const float b[4][4]) {
    float tmp[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[i][j] = a[i][0] * b[0][j] +
                        a[i][1] * b[1][j] +
                        a[i][2] * b[2][j] +
                        a[i][3] * b[3][j];
        }
    }
    memcpy(res, tmp, sizeof(tmp));
}

static inline float gfx_rt64_lerp_float(float a, float b, float t) {
    return a + t * (b - a);
}

static inline RT64_MATRIX4 gfx_rt64_lerp_matrix(const RT64_MATRIX4 &a, const RT64_MATRIX4 &b, float t) {
    // TODO: This is just a hacky way to see some interpolated values, but it is NOT the proper way
    // to interpolate a transformation matrix. That will likely require decomposition of both the matrices.
    RT64_MATRIX4 c;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            c.m[i][j] = gfx_rt64_lerp_float(a.m[i][j], b.m[i][j], t);
        }
    }
    return c;
}

static inline bool gfx_rt64_skip_matrix_lerp(const RT64_MATRIX4 &a, const RT64_MATRIX4 &b, const float minDot) {
    static const RT64_VECTOR3 axes[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
    for (int i = 0; i < 3; i++) {
        RT64_VECTOR3 prevAxis = normalize_vector(transform_direction_affine(a, axes[i]));
        RT64_VECTOR3 newAxis = normalize_vector(transform_direction_affine(b, axes[i]));
        if (vector_dot_product(prevAxis, newAxis) < minDot) { return true; }
    }
    return false;
}
