#pragma once

#include "rt64/rt64.h"

#include "types.h"

struct RT64DefaultFloat {
    float value;
    bool set;
    constexpr RT64DefaultFloat(void) : value(0.0f), set(false) {}
    constexpr RT64DefaultFloat(float v) : value(v), set(true) {}
};

struct RT64DefaultInt {
    s32 value;
    bool set;
    constexpr RT64DefaultInt(void) : value(0), set(false) {}
    constexpr RT64DefaultInt(s32 v) : value(v), set(true) {}
};

struct RT64DefaultColor {
    RT64_VECTOR3 value;
    bool set;
    constexpr RT64DefaultColor(void) : value{ 0.0f, 0.0f, 0.0f }, set(false) {}
    constexpr RT64DefaultColor(s32 r, s32 g, s32 b) : value{ (float)(r), (float)(g), (float)(b) }, set(true) {}
};

struct RT64DefaultColorMix {
    RT64_VECTOR4 value;
    bool set;
    constexpr RT64DefaultColorMix(void) : value{ 0.0f, 0.0f, 0.0f, 0.0f }, set(false) {}
    constexpr RT64DefaultColorMix(s32 r, s32 g, s32 b, float w) : value{ (float)(r), (float)(g), (float)(b), w }, set(true) {}
};

struct RT64DefaultMask {
    u32 value;
    bool set;
    constexpr RT64DefaultMask(void) : value(0), set(false) {}
    constexpr RT64DefaultMask(u32 v) : value(v), set(true) {}
};

struct RT64DefaultLight {
    bool set;
    RT64_VECTOR3 position;
    RT64DefaultColor diffuseColor;
    RT64DefaultColor specularColor;
    float attenuationRadius;
    float pointRadius;
    float shadowOffset;
    float attenuationExponent;
    float flickerIntensity;
    u32 groupBits;
    u32 lightType;
    float pitch, yaw, roll;
    float scaleX, scaleY;
    u32 lightShape;
    u32 apertureEnabled;
    float aperturePitch, apertureYaw;
    u32 volumetricEnabled;
    float volumetricIntensity;
    float intensity;
};

struct RT64DefaultMaterial {
    RT64DefaultInt ignoreNormalFactor;
    RT64DefaultFloat uvDetailScale;
    RT64DefaultFloat reflectionFactor;
    RT64DefaultFloat reflectionFresnelFactor;
    RT64DefaultFloat reflectionShineFactor;
    RT64DefaultFloat refractionFactor;
    RT64DefaultColor specularColor;
    RT64DefaultFloat specularShinyness;
    RT64DefaultFloat solidAlphaMultiplier;
    RT64DefaultFloat shadowAlphaMultiplier;
    RT64DefaultInt depthBias;
    RT64DefaultInt shadowRayBias;
    RT64DefaultColor selfLightColor;
    RT64DefaultMask lightGroupMaskBits;
    RT64DefaultColorMix diffuseColorMix;
    RT64DefaultFloat diffuseIntensity;
    RT64DefaultFloat specularFactor;
    RT64DefaultFloat specularEccentricity;
    RT64DefaultFloat bumpStrength;
    RT64DefaultFloat normalStrength;
    RT64DefaultColor reflectionColor;
    RT64DefaultFloat specularIntensity;
    RT64DefaultFloat selfLightIntensity;
    u32 shadingModelSet = 2; // RT64_SHADING_MODEL_LAMBERT, RT64_SHADING_MODEL_PHONG, RT64_SHADING_MODEL_BLINN
    bool specularTintSet = false;
    u32 specularTint = 0;
    bool shadowEnabledSet = false;
    u32 shadowEnabled = 0;
    bool shadowCenterSet = false;
    u32 shadowCenter = 0;
};

struct RT64DefaultMod {
    const char *name;
    RT64DefaultMaterial materialMod;
    RT64DefaultLight lightMod;
    const char *bumpMap;
    const char *normalMap;
    const char *specularMap;
};

struct RT64DefaultScene {
    RT64DefaultColor ambientBaseColor;
    RT64DefaultColor ambientNoGIColor;
    RT64DefaultColor eyeLightDiffuseColor;
    RT64DefaultColor eyeLightSpecularColor;
    RT64_VECTOR3 skyDiffuseMultiplier;
    RT64_VECTOR3 skyHSLModifier;
    float skyYawOffset;
    float giDiffuseStrength;
    float giSkyStrength;
};

struct RT64DefaultArea {
    u32 levelNum;
    u32 areaIndex;
    RT64DefaultScene scene;
    const RT64DefaultLight *lights;
    s32 lightCount;
};

extern const RT64DefaultMod gRT64DefaultGeoLayoutMods[];
extern const s32 gRT64DefaultGeoLayoutModCount;
extern const RT64DefaultMod gRT64DefaultTextureMods[];
extern const s32 gRT64DefaultTextureModCount;
extern const RT64DefaultArea gRT64DefaultLevelLights[];
extern const s32 gRT64DefaultLevelLightCount;
