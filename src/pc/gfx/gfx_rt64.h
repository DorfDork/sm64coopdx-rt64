#pragma once

#include "types.h"

struct lua_State;

struct Rt64Light {
    Vec3f position;
    Vec3f diffuseColor;
    f32 attenuationRadius;
    f32 pointRadius;
    Vec3f specularColor;
    f32 shadowOffset;
    f32 attenuationExponent;
    f32 flickerIntensity;
    u32 groupBits;
    u32 lightType;
    f32 pitch;
    f32 yaw;
    f32 roll;
    f32 scaleX;
    f32 scaleY;
    u32 lightShape;
    u32 apertureEnabled;
    f32 aperturePitch;
    f32 apertureYaw;
    u32 volumetricEnabled;
    f32 volumetricIntensity;
    f32 intensity;
};

struct Rt64SceneDesc {
    Vec3f ambientBaseColor;
    Vec3f ambientNoGIColor;
    Vec3f eyeLightDiffuseColor;
    Vec3f eyeLightSpecularColor;
    Vec3f skyDiffuseMultiplier;
    Vec3f skyHSLModifier;
    f32 skyYawOffset;
    f32 giDiffuseStrength;
    f32 giSkyStrength;
};

#define RT64_LUA_MAX_AREA_LIGHTS 128

struct Rt64AreaLighting {
    struct Rt64SceneDesc scene;
    C_ARRAY struct Rt64Light lights[RT64_LUA_MAX_AREA_LIGHTS];
    s32 lightCount;
};

#ifdef __cplusplus
extern "C" {
#endif

extern struct GfxRenderingAPI gfx_rt64_api;

void gfx_rt64_reset_lua_config(void);
bool gfx_rt64_is_ready(void);

void gfx_rt64_lua_register_level_lights(struct lua_State *L, s32 levelNum, s32 areaIndex, s32 tableIndex);
void gfx_rt64_lua_register_texture_mod(struct lua_State *L, const char *name, s32 tableIndex);
void gfx_rt64_lua_register_geo_layout_mod(struct lua_State *L, const char *name, s32 tableIndex);
struct Rt64AreaLighting *gfx_rt64_lua_get_area_lighting(s32 levelNum, s32 areaIndex);
bool gfx_rt64_lua_is_active(void);

#ifdef __cplusplus
}
#endif
