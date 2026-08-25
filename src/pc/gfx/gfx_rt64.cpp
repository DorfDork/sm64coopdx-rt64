#if defined(_WIN32)

extern "C" {
#include "pc/configfile.h"
#include "pc/fs/fs.h"
#include "game/area.h"
#include "game/game_init.h"
#include "game/level_update.h"
#include "game/rendering_graph_node.h"
#include "goddard/gd_math.h"
#include "gfx_cc.h"
#include "pc/mods/mod.h"
#include "pc/mods/mods.h"
#include "pc/lua/smlua_hooks.h"
#include "gfx_shader.h"
#include "gfx_rt64_lua.h"
}

#include <algorithm>
#include <cassert>
#include <set>
#include <stdint.h>
#include <windows.h>

#include <SDL2/SDL.h>

#include <stb/stb_image.h>
#include "pc/utils/xxhash64.h"

#include "gfx_rendering_api.h"
#include "gfx_rt64.h"
#include "gfx_rt64_context.hpp"
#include "gfx_rt64_serialization.hpp"
#include "gfx_rt64_geo_map.hpp"
#include "gfx_pc.h"

#define RT64_LUA_ASSERT_FIELD(luaType, realType, luaField, realField) \
    static_assert(offsetof(luaType, luaField) == offsetof(realType, realField), #luaType "::" #luaField " must match " #realType "::" #realField); \
    static_assert(sizeof(((luaType *)(0))->luaField) == sizeof(((realType *)(0))->realField), #luaType "::" #luaField " must be the same size as " #realType "::" #realField)

static_assert(sizeof(struct Rt64Light) == sizeof(RT64_LIGHT), "struct Rt64Light must be byte-identical to RT64_LIGHT");
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, position, position);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, diffuseColor, diffuseColor);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, attenuationRadius, attenuationRadius);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, pointRadius, pointRadius);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, specularColor, specularColor);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, shadowOffset, shadowOffset);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, attenuationExponent, attenuationExponent);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, flickerIntensity, flickerIntensity);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, groupBits, groupBits);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, lightType, lightType);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, pitch, pitch);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, yaw, yaw);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, roll, roll);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, scaleX, scaleX);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, scaleY, scaleY);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, lightShape, lightShape);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, apertureEnabled, apertureEnabled);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, aperturePitch, aperturePitch);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, apertureYaw, apertureYaw);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, volumetricEnabled, volumetricEnabled);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, volumetricIntensity, volumetricIntensity);
RT64_LUA_ASSERT_FIELD(struct Rt64Light, RT64_LIGHT, intensity, intensity);

static_assert(sizeof(struct Rt64SceneDesc) == sizeof(RT64_SCENE_DESC), "struct Rt64SceneDesc must be byte-identical to RT64_SCENE_DESC");
RT64_LUA_ASSERT_FIELD(struct Rt64SceneDesc, RT64_SCENE_DESC, ambientBaseColor, ambientBaseColor);
RT64_LUA_ASSERT_FIELD(struct Rt64SceneDesc, RT64_SCENE_DESC, ambientNoGIColor, ambientNoGIColor);
RT64_LUA_ASSERT_FIELD(struct Rt64SceneDesc, RT64_SCENE_DESC, eyeLightDiffuseColor, eyeLightDiffuseColor);
RT64_LUA_ASSERT_FIELD(struct Rt64SceneDesc, RT64_SCENE_DESC, eyeLightSpecularColor, eyeLightSpecularColor);
RT64_LUA_ASSERT_FIELD(struct Rt64SceneDesc, RT64_SCENE_DESC, skyDiffuseMultiplier, skyDiffuseMultiplier);
RT64_LUA_ASSERT_FIELD(struct Rt64SceneDesc, RT64_SCENE_DESC, skyHSLModifier, skyHSLModifier);
RT64_LUA_ASSERT_FIELD(struct Rt64SceneDesc, RT64_SCENE_DESC, skyYawOffset, skyYawOffset);
RT64_LUA_ASSERT_FIELD(struct Rt64SceneDesc, RT64_SCENE_DESC, giDiffuseStrength, giDiffuseStrength);
RT64_LUA_ASSERT_FIELD(struct Rt64SceneDesc, RT64_SCENE_DESC, giSkyStrength, giSkyStrength);

static_assert(sizeof(struct Rt64AreaLighting) == sizeof(AreaLighting), "struct Rt64AreaLighting must be byte-identical to AreaLighting");
static_assert(RT64_LUA_MAX_AREA_LIGHTS == MAX_LEVEL_LIGHTS, "RT64_LUA_MAX_AREA_LIGHTS must match MAX_LEVEL_LIGHTS");
RT64_LUA_ASSERT_FIELD(struct Rt64AreaLighting, AreaLighting, scene, sceneDesc);
RT64_LUA_ASSERT_FIELD(struct Rt64AreaLighting, AreaLighting, lights, lights);
RT64_LUA_ASSERT_FIELD(struct Rt64AreaLighting, AreaLighting, lightCount, lightCount);

#undef RT64_LUA_ASSERT_FIELD

extern "C" HWND gfx_window_dxgi_get_h_wnd(void);

extern "C" const char *dynos_actor_get_custom_name(const void *geoLayout);

#include "gfx_rt64_common.hpp"
#include "gfx_rt64_texture.hpp"

int gfx_rt64_get_level_index(void) {
    int levelIndex = (gPlayerSpawnInfos[0].areaIndex >= 0) ? gCurrLevelNum : 0;

    return (levelIndex >= 0 && levelIndex < MAX_LEVELS) ? levelIndex : 0;
}

int gfx_rt64_get_area_index(void) {
    int areaIndex = (gPlayerSpawnInfos[0].areaIndex >= 0) ? gCurrAreaIndex : 0;
    return (areaIndex >= 0 && areaIndex < MAX_AREAS) ? areaIndex : 0;
}

static void gfx_rt64_rapi_set_sampler_parameters(int tile, bool linear_filter, u32 cms, u32 cmt) {
    u32 textureKey = RT64.currentTextureIds[tile];
    auto &recordedTexture = RT64.textures[textureKey];
    recordedTexture.linearFilter = linear_filter;
    recordedTexture.cms = cms;
    recordedTexture.cmt = cmt;
}

static void gfx_rt64_rapi_set_depth_test(bool depth_test) {
}

static void gfx_rt64_rapi_set_depth_mask(bool depth_mask) {
}

static void gfx_rt64_rapi_set_zmode_decal(bool zmode_decal) {
}

static void gfx_rt64_rapi_set_viewport(int x, int y, int width, int height) {
    RT64.viewportRect = { x, y, width, height };
}

static void gfx_rt64_rapi_set_scissor(int x, int y, int width, int height) {
    RT64.scissorRect = { x, y, width, height };
}

static void gfx_rt64_rapi_set_use_alpha(bool use_alpha) {
}

bool gfx_rt64_use_vsync(void) {
    return RT64.useVsync && !RT64.turboMode;
}

static void gfx_rt64_rapi_set_vsync(bool enabled) {
    RT64.useVsync = enabled;
}

static void gfx_rt64_rapi_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
}

static void gfx_rt64_smooth_zero_normals(float *vbo, unsigned int vertexCount, unsigned int floatStride) {
    if (vertexCount < 3) {
        return;
    }

    bool anyZeroNormals = false;
    for (unsigned int v = 0; v < vertexCount; v++) {
        const float *n = &vbo[v * floatStride + 4];
        if ((n[0] == 0.0f) && (n[1] == 0.0f) && (n[2] == 0.0f)) {
            anyZeroNormals = true;
            break;
        }
    }

    if (!anyZeroNormals) {
        return;
    }

    auto positionKey = [&](const float *p) -> u64 {
        u32 bits[3];
        memcpy(bits, p, sizeof(bits));
        u64 key = (u64)(bits[0]);
        key = key * 0x9E3779B185EBCA87ull + bits[1];
        key = key * 0x9E3779B185EBCA87ull + bits[2];
        return key;
    };

    struct AccumSlot { u64 key; float x, y, z; u32 stamp; };
    static std::vector<AccumSlot> sTable;
    static u32 sTableStamp = 0;

    unsigned int tableSize = 1;
    while (tableSize <= vertexCount) { tableSize <<= 1; }
    if (sTable.size() < tableSize) {
        sTable.resize(tableSize);
    }

    if (++sTableStamp == 0) {
        std::fill(sTable.begin(), sTable.end(), AccumSlot{});
        sTableStamp = 1;
    }
    const u64 tableMask = (u64)(tableSize) - 1;

    auto accumAt = [&](u64 key) -> AccumSlot & {
        u64 slot = key & tableMask;
        while ((sTable[slot].stamp == sTableStamp) && (sTable[slot].key != key)) {
            slot = (slot + 1) & tableMask;
        }
        if (sTable[slot].stamp != sTableStamp) {
            sTable[slot].stamp = sTableStamp;
            sTable[slot].key = key;
            sTable[slot].x = sTable[slot].y = sTable[slot].z = 0.0f;
        }
        return sTable[slot];
    };

    static std::vector<u32> sVertexSlot;
    if (sVertexSlot.size() < vertexCount) {
        sVertexSlot.resize(vertexCount);
    }

    const unsigned int triCount = vertexCount / 3;
    for (unsigned int t = 0; t < triCount; t++) {
        const float *p0 = &vbo[(t * 3 + 0) * floatStride];
        const float *p1 = &vbo[(t * 3 + 1) * floatStride];
        const float *p2 = &vbo[(t * 3 + 2) * floatStride];
        const float ux = p1[0] - p0[0], uy = p1[1] - p0[1], uz = p1[2] - p0[2];
        const float vx = p2[0] - p0[0], vy = p2[1] - p0[1], vz = p2[2] - p0[2];
        const float nx = uy * vz - uz * vy;
        const float ny = uz * vx - ux * vz;
        const float nz = ux * vy - uy * vx;
        for (int c = 0; c < 3; c++) {
            const unsigned int v = t * 3 + c;
            AccumSlot &acc = accumAt(positionKey(&vbo[v * floatStride]));
            acc.x += nx;
            acc.y += ny;
            acc.z += nz;
            sVertexSlot[v] = (u32)(&acc - sTable.data());
        }
    }

    for (unsigned int v = 0; v < vertexCount; v++) {
        float *n = &vbo[v * floatStride + 4];
        // The ones that came with a normal keep it. Only the gaps are filled.
        if ((n[0] != 0.0f) || (n[1] != 0.0f) || (n[2] != 0.0f)) {
            continue;
        }

        const AccumSlot &acc = sTable[sVertexSlot[v]];
        const float len = sqrtf(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z);
        if (len > 1e-6f) {
            n[0] = (acc.x / len) * 127.0f;
            n[1] = (acc.y / len) * 127.0f;
            n[2] = (acc.z / len) * 127.0f;
        }
    }
}

u32 gfx_rt64_casted_shadow_group(const void *geoLayout) {
    const uintptr_t address = (uintptr_t)(geoLayout);
    return (u32)((address >> 4) * 2654435761u) | 1u;
}

static void gfx_rt64_rapi_process_mesh(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris, bool raytrace, GameDisplayList &displayList) {
    const bool useTexture = RT64.shaderProgram->usedTextures[0] || RT64.shaderProgram->usedTextures[1];
    const int numInputs = RT64.shaderProgram->numInputs;
    const bool useAlpha = RT64.shaderProgram->cc.optAlpha != 0;
    unsigned int vertexCount = 0;
    unsigned int vertexStride = 0;
    unsigned int indexCount = (unsigned int)(buf_vbo_num_tris * 3);
    void *vertexBuffer = buf_vbo;
    if (RT64.shaderProgram->hasCustomShader && !raytrace &&
        !RT64.shaderProgram->customShaderFailed.load(std::memory_order_relaxed)) {
        const unsigned int floatsPerVertex = 4 + 4 + 2 + 2 + 2 + (8 * 4) + 3 + 3;
        vertexStride = floatsPerVertex * (unsigned int)(sizeof(float));
    } else {
        const unsigned int vertexFixedStride = 16 + 12;
        vertexStride = vertexFixedStride + (useTexture ? 8 : 0) + numInputs * (useAlpha ? 16 : 12);
    }
    assert(((buf_vbo_len * 4) % vertexStride) == 0);

    vertexCount = (unsigned int)((buf_vbo_len * 4) / vertexStride);
    assert(buf_vbo_num_tris == (vertexCount / 3));

    if (raytrace) {
        gfx_rt64_smooth_zero_normals(buf_vbo, vertexCount, vertexStride / sizeof(float));
    }

    size_t vertexBufferSize = buf_vbo_len * sizeof(float);

    // Make the vector large enough to fit the required meshes.
    if (displayList.meshes.size() < (size_t)(displayList.drawCount + 1)) {
        displayList.meshes.resize(displayList.drawCount + 1);
    }

    // Try reusing the mesh that was stored in this index first.
    auto &dynMesh = displayList.meshes[displayList.drawCount];
    dynMesh.useTexture = useTexture;
    dynMesh.raytrace = raytrace;

    XXHash64 hashStream(0);
    hashStream.add(buf_vbo, vertexBufferSize);
    const u64 hash = hashStream.hash();
    if (hash == dynMesh.vertexBufferHash) {
        return;
    }

    // Free the previous vertex buffer if it's too small to fit the new vertex buffer.
    if ((dynMesh.vertexBuffer != nullptr) && ((dynMesh.vertexCount * dynMesh.vertexStride) < (vertexCount * vertexStride))) {
        free(dynMesh.vertexBuffer);
        dynMesh.vertexBuffer = nullptr;
    }

    // Only create the vertex buffer if it hasn't been assigned yet.
    if (dynMesh.vertexBuffer == nullptr) {
        dynMesh.vertexBuffer = (float *)(malloc(vertexBufferSize));
        if (dynMesh.vertexBuffer == nullptr) {
            gfx_rt64_error_message("RT64", "Failed to allocate a vertex buffer, ran out of memory.");
            abort();
        }
    }

    memcpy(dynMesh.vertexBuffer, vertexBuffer, vertexBufferSize);
    dynMesh.vertexCount = vertexCount;
    dynMesh.vertexStride = vertexStride;
    dynMesh.indexCount = indexCount;
    dynMesh.vertexBufferHash = hash;

    const unsigned int floatsPerVertex = vertexStride / sizeof(float);
    u64 posHash = 0;
    for (unsigned int pv = 0; pv < vertexCount; pv++) {
        u32 bits[3];
        memcpy(bits, &buf_vbo[pv * floatsPerVertex], sizeof(bits));
        posHash = (posHash * 0x9E3779B185EBCA87ull) ^ bits[0];
        posHash = (posHash * 0x9E3779B185EBCA87ull) ^ bits[1];
        posHash = (posHash * 0x9E3779B185EBCA87ull) ^ bits[2];
    }
    dynMesh.positionHash = posHash;
}

static void gfx_rt64_rapi_apply_mod(RT64_MATERIAL *material, u32 *bump, u32 *normal, u32 *specular, RecordedMod *mod) {
    if (mod->materialMod != nullptr) {
        RT64_ApplyMaterialAttributes(material, mod->materialMod);

        if ((mod->materialMod->enabledAttributes & RT64_ATTRIBUTE_SPECULAR_COLOR) &&
            !(mod->materialMod->enabledAttributes & RT64_ATTRIBUTE_SPECULAR_INTENSITY)) {
            material->specularIntensity = 10.0f;
        }
    }

    if (mod->bumpMapHash != 0) {
        const u32 textureKey = gfx_rt64_map_texture_key(mod->bumpMapHash);
        if (textureKey != 0) { *bump = textureKey; }
    }

    if (mod->normalMapHash != 0) {
        const u32 textureKey = gfx_rt64_map_texture_key(mod->normalMapHash);
        if (textureKey != 0) { *normal = textureKey; }
    }

    if (mod->specularMapHash != 0) {
        const u32 textureKey = gfx_rt64_map_texture_key(mod->specularMapHash);
        if (textureKey != 0) { *specular = textureKey; }
    }

    const float minSpecularShinyness = 0.001f;
    if (!(material->specularShinyness > minSpecularShinyness)) {
        material->specularShinyness = minSpecularShinyness;
    }
}

static void gfx_rt64_rapi_draw_triangles_common(RT64_MATRIX4 transform, float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris, bool double_sided, bool raytrace, u32 uid) {
    assert(RT64.shaderProgram != nullptr);

    // Retrieve the previous transform for the display list with this UID and store the current one.
    GameFrame *cpuFrame = &RT64.frames[RT64.cpuFrameIndex];

    if (RT64.cachedDisplayList == nullptr || RT64.cachedDisplayListUid != uid) {
        RT64.cachedDisplayList = &cpuFrame->displayLists[uid];
        RT64.cachedDisplayListUid = uid;
    }
    auto &displayList = *RT64.cachedDisplayList;
    RecordedMod *textureMod = nullptr;
    bool linearFilter = false;

    u32 cms = 0, cmt = 0;

    // Make the vector large enough to fit the required instances.
    if (displayList.instances.size() < (size_t)(displayList.drawCount + 1)) {
        displayList.instances.resize(displayList.drawCount + 1);
    }

    auto &displayListInstance = displayList.instances[displayList.drawCount];
    displayListInstance.light.groupBits = 0;
    displayListInstance.textures = {};
    displayListInstance.uniformBlocks.clear();
    displayListInstance.uniformBlockData.clear();

    if (RT64.shaderProgram->hasCustomShader &&
        !RT64.shaderProgram->customShaderFailed.load(std::memory_order_relaxed)) {
        struct Shader *const stages[2] = { RT64.shaderProgram->vertexShader, RT64.shaderProgram->fragmentShader };
        for (int stage = 0; stage < 2; stage++) {
            if (stages[stage] == nullptr) { continue; }

            for (int i = 0; i < stages[stage]->uniformBlockCount; i++) {
                const struct ShaderUniformBlock *block = &stages[stage]->uniformBlocks[i];
                if ((block->size == 0) || (block->buffer == nullptr)) { continue; }

                if (block->location >= RT64_MAX_SHADER_UNIFORM_BLOCKS) {
                    static bool sReported = false;
                    if (!sReported) {
                        sReported = true;
                        fprintf(stderr, "RT64: a shader declares more uniform blocks than there are constant buffer registers for (%d). The ones past that read as zero.\n",
                            RT64_MAX_SHADER_UNIFORM_BLOCKS);
                    }
                    continue;
                }

                RT64_SHADER_UNIFORM_BLOCK entry;
                entry.shaderRegister = block->location;
                entry.size = block->size;
                entry.data = nullptr;
                displayListInstance.uniformBlocks.push_back(entry);
                displayListInstance.uniformBlockData.insert(displayListInstance.uniformBlockData.end(), block->buffer, block->buffer + block->size);
            }
        }

        size_t dataOffset = 0;
        for (RT64_SHADER_UNIFORM_BLOCK &entry : displayListInstance.uniformBlocks) {
            entry.data = displayListInstance.uniformBlockData.data() + dataOffset;
            dataOffset += entry.size;
        }
    }

    RT64_INSTANCE_DESC &instDesc = displayListInstance.desc;
    instDesc.scissorRect = RT64.scissorRect;
    instDesc.viewportRect = RT64.viewportRect;
    instDesc.transform = transform;
    instDesc.material = RT64.defaultMaterial;
    instDesc.material.textureGenEnabled = RT64.textureGenEnabled ? 1 : 0;
    instDesc.material.textureGenU = RT64.textureGenU;
    instDesc.material.textureGenV = RT64.textureGenV;

    // Find all parameters associated to the texture if it's used.
    bool highlightMaterial = false;
    u64 textureHash = 0;

    if (!RT64.shaderProgram->usedTextures[0]) {
        textureHash = gfx_rt64_material_vanilla_name_hash();
        if (textureHash == 0) {
            textureHash = RT64.shaderProgram->hash ^ 0x5010D0000C0106ull;
        }
    }

    if (RT64.shaderProgram->usedTextures[0]) {
        u32 diffuseKey = RT64.currentTextureIds[0];

        auto recordedIt = RT64.textures.find(diffuseKey);
        if (recordedIt != RT64.textures.end()) {
            const RecordedTexture &recordedTexture = recordedIt->second;
            linearFilter = recordedTexture.linearFilter;
            cms = recordedTexture.cms;
            cmt = recordedTexture.cmt;
            textureHash = recordedTexture.hash;
        }
        displayListInstance.textures.diffuse = diffuseKey;

        const u64 materialNameHash = gfx_rt64_material_mod_name_hash();
        if (materialNameHash != 0) {
            textureHash = materialNameHash;
        }
    }

    displayListInstance.textureHash = textureHash;
    displayListInstance.geoLayout = RT64.graphNodeGeoLayout;

    const bool highlightGeoLayout = RT64.renderInspectorActive && RT64.pickGeoLayoutHighlight &&
        !RT64.pickGeoLayout && (RT64.publishedGeoLayout != nullptr) &&
        (RT64.graphNodeGeoLayout == RT64.publishedGeoLayout);

    if (textureHash != 0) {
        // Only use mutex access if the inspector is active.
        bool threadSafeAccess = RT64.renderInspectorActive;
        if (threadSafeAccess) {
            RT64.pickTextureMutex.lock();
            RT64.texModsMutex.lock();
        }

        // Use the texture mod for the matching texture hash.
        if (!RT64.texMods.empty()) {
            auto texModIt = RT64.texMods.find(textureHash);
            if (texModIt != RT64.texMods.end()) {
                textureMod = texModIt->second;
            }
        }

        if (threadSafeAccess) {
            // Update data for ray picking.
            if (RT64.pickTextureHighlight && (RT64.pickTextureHash != 0) && (textureHash == RT64.pickTextureHash)) {
                highlightMaterial = true;
            }

            RT64.texModsMutex.unlock();
            RT64.pickTextureMutex.unlock();
        }
    }

    if (RT64.shaderProgram->usedTextures[1]) {
        displayListInstance.textures.diffuse2 = RT64.currentTextureIds[1];
    }

    // Build material with applied mods.
    if (RT64.graphNodeMod != nullptr) {
        gfx_rt64_rapi_apply_mod(
            &instDesc.material,
            &displayListInstance.textures.bump,
            &displayListInstance.textures.normal,
            &displayListInstance.textures.specular,
            RT64.graphNodeMod);
    }

    if (textureMod != nullptr) {
        gfx_rt64_rapi_apply_mod(
            &instDesc.material,
            &displayListInstance.textures.bump,
            &displayListInstance.textures.normal,
            &displayListInstance.textures.specular,
            textureMod);

        if (textureMod->lightMod != nullptr) {
            displayListInstance.light = *textureMod->lightMod;
        }
    }

    // Apply a higlight color if the material is selected.
    if (highlightMaterial) {
        instDesc.material.diffuseColorMix = { 255.0f, 0.0f, 255.0f, 0.5f };
        instDesc.material.selfLightColor = { 255.0f, 255.0f, 255.0f };
        instDesc.material.lightGroupMaskBits = 0;
    }
    else if (highlightGeoLayout) {
        instDesc.material.diffuseColorMix = { 255.0f, 255.0f, 89.0f, 0.5f };
        instDesc.material.selfLightColor = { 255.0f, 255.0f, 255.0f };
        instDesc.material.lightGroupMaskBits = 0;
    }

    if (instDesc.material.shadowCenter != 0) {
        instDesc.material.shadowCenter = gfx_rt64_casted_shadow_group(displayListInstance.geoLayout);
    }

    // Copy the fog to the material.
    instDesc.material.fogColor = RT64.fogColor;
    instDesc.material.fogMul = RT64.fogMul;
    instDesc.material.fogOffset = RT64.fogOffset;
    instDesc.material.fogEnabled = RT64.shaderProgram->usedFog;

    // HACK: Add a depth bias based on how many instances have been drawn so far to push
    // coplanar stuff above other meshes on the anyhit sorting.
    instDesc.material.depthBias += RT64.instancesDrawn * 0.001f;

    auto &shader = displayListInstance.shader;
    shader.program = RT64.shaderProgram;
    shader.raytrace = raytrace;
    shader.filter = linearFilter ? RT64_SHADER_FILTER_LINEAR : RT64_SHADER_FILTER_POINT;
    shader.hAddr = (cms & G_TX_CLAMP) ? RT64_SHADER_ADDRESSING_CLAMP : (cms & G_TX_MIRROR) ? RT64_SHADER_ADDRESSING_MIRROR : RT64_SHADER_ADDRESSING_WRAP;
    shader.vAddr = (cmt & G_TX_CLAMP) ? RT64_SHADER_ADDRESSING_CLAMP : (cmt & G_TX_MIRROR) ? RT64_SHADER_ADDRESSING_MIRROR : RT64_SHADER_ADDRESSING_WRAP;
    shader.normalMap = (displayListInstance.textures.normal > 0);
    shader.specularMap = (displayListInstance.textures.specular > 0);
    shader.bumpMap = (displayListInstance.textures.bump > 0);

    // Mark the right instance flags.
    instDesc.flags = 0;
    if (RT64.background) {
        instDesc.flags |= RT64_INSTANCE_RASTER_BACKGROUND;
    }

    if (double_sided) {
        instDesc.flags |= RT64_INSTANCE_DISABLE_BACKFACE_CULLING;
    }

    gfx_rt64_rapi_process_mesh(buf_vbo, buf_vbo_len, buf_vbo_num_tris, raytrace, displayList);

    // Increase the counters.
    displayList.drawCount++;
    RT64.instancesDrawn++;
}

static void gfx_rt64_rapi_set_fog(u8 fog_r, u8 fog_g, u8 fog_b, s16 fog_mul, s16 fog_offset) {
    RT64.fogColor.x = (float)(fog_r);
    RT64.fogColor.y = (float)(fog_g);
    RT64.fogColor.z = (float)(fog_b);
    RT64.fogMul = fog_mul;
    RT64.fogOffset = fog_offset;
}

static void gfx_rt64_rapi_draw_triangles_ortho(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris, bool double_sided, u32 uid) {
    gfx_rt64_rapi_draw_triangles_common(RT64.identityTransform, buf_vbo, buf_vbo_len, buf_vbo_num_tris, double_sided, false, uid);
}

static void gfx_rt64_rapi_draw_triangles_persp(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris, float transform_affine[4][4], bool double_sided, u32 uid) {
    // Stop considering the orthographic projection triangles as background as soon as perspective triangles are drawn.
    if (RT64.background) {
        RT64.background = false;
    }

    RT64_MATRIX4 transform;
    memcpy(transform.m, transform_affine, sizeof(float) * 16);
    gfx_rt64_rapi_draw_triangles_common(transform, buf_vbo, buf_vbo_len, buf_vbo_num_tris, double_sided, true, uid);
}

void gfx_rt64_error_message(const char *window_title, const char *error_message) {
    MessageBox(nullptr, error_message, window_title, MB_OK | MB_ICONEXCLAMATION);
}

extern "C" struct GfxRenderingAPI *gRenderApi;

bool gfx_gfx_rt64_is_active(void) {
    return (gRenderApi == &gfx_rt64_api) && (RT64.device != nullptr);
}

bool gfx_rt64_lua_is_active(void) {
    return gfx_gfx_rt64_is_active();
}

struct Rt64AreaLighting *gfx_rt64_lua_get_area_lighting(s32 levelNum, s32 areaIndex) {
    if (!gfx_gfx_rt64_is_active()) { return nullptr; }
    if ((levelNum < 0) || (levelNum >= MAX_LEVELS) || (areaIndex < 0) || (areaIndex >= MAX_AREAS)) { return nullptr; }

    const std::lock_guard<std::mutex> lightingLock(RT64.levelAreaLightingMutex);
    AreaLighting &areaLighting = gfx_rt64_get_or_add_area_lighting((u32)(levelNum), (u32)(areaIndex));
    return reinterpret_cast<struct Rt64AreaLighting *>(&areaLighting);
}

void gfx_rt64_ensure_geo_layout_mods_loaded(void) {
    if ((gRenderApi != &gfx_rt64_api) || RT64.loadedGeoLayoutMods) {
        return;
    }

    gfx_rt64_load_geo_layout_mods();
    RT64.loadedGeoLayoutMods = true;
}

void gfx_rt64_invalidate_graph_node_mods(void) {
    RT64.graphNodeModsSynced.clear();
}

static void gfx_rt64_rapi_lua_config_save(void) {
    if (!gfx_gfx_rt64_is_active() || !RT64.renderInspectorActive) {
        return;
    }

    const std::lock_guard<std::mutex> lightingLock(RT64.levelAreaLightingMutex);
    const std::lock_guard<std::mutex> texModsLock(RT64.texModsMutex);
    gfx_rt64_save_geo_layout_mods();
    gfx_rt64_save_texture_mods();
    gfx_rt64_save_level_lights();
}

static void gfx_rt64_rapi_lua_config_reset(void) {
    const std::lock_guard<std::mutex> lightingLock(RT64.levelAreaLightingMutex);
    const std::lock_guard<std::mutex> texModsLock(RT64.texModsMutex);
    gfx_rt64_load_level_lights();
    gfx_rt64_load_texture_mods();
    gfx_rt64_load_geo_layout_mods();
    RT64.loadedGeoLayoutMods = true;

    RT64.graphNodeRootsNamed.clear();

    gfx_rt64_invalidate_graph_node_mods();
}

static void gfx_rt64_apply_config(void) {
    const std::lock_guard<std::mutex> lock(RT64.renderViewDescMutex);

    if (RT64.inspectorViewDescValid) {
        gfx_rt64_adopt_inspector_view_desc(RT64.inspectorViewDesc);
        RT64.inspectorViewDescValid = false;
    }

    RT64_VIEW_DESC desc = RT64.renderViewDesc;
    configRT64ResScale = gfx_rt64_clamp_percent(configRT64ResScale, sMinResolutionScale, sMaxResolutionScale);
    desc.resolutionScale = configRT64ResScale / 100.0f;
    desc.maxLights = configRT64MaxLights;
    desc.maxReflections = configRT64MaxReflections;
    desc.diSamples = configRT64SphereLights ? 1 : 0;
    desc.giSamples = configRT64GI ? 1 : 0;
    desc.denoiserEnabled = configRT64Denoiser;
    desc.motionBlurStrength = configRT64MotionBlurStrength / 100.0f;
    desc.upscaler = configRT64Upscaler;
    desc.upscalerMode = configRT64UpscalerMode;
    desc.upscalerSharpness = configRT64UpscalerSharpness / 100.0f;

    desc.aspectRatio = gfx_current_dimensions.aspect_ratio;

    RT64.useVsync = configWindow.vsync;

    if (memcmp(&desc, &RT64.renderViewDesc, sizeof(desc)) != 0) {
        RT64.renderViewDesc = desc;
        RT64.renderViewDescChanged = true;
    }
}

static void gfx_rt64_rapi_init(void) {
    // Setup library.
    RT64.hwnd = gfx_window_dxgi_get_h_wnd();
    RT64.lib = RT64_LoadLibrary();
    if (RT64.lib.handle == 0) {
        gfx_rt64_error_message("RT64", "Failed to load library. Please make sure rt64lib.dll and dxil.dll are placed next to the game's executable and are up to date.");
        abort();
    }

    // Start timers.
    QueryPerformanceFrequency(&RT64.frequency);
    RT64.startingTime = gfx_rt64_profile_marker();
    RT64.turboMode = false;

    // Start the game paused. Let the render thread unpause it once it's ready.
    RT64.pauseMode = true;

    gfx_rt64_load_level_lights();

    // Load the texture mods from a file.
    gfx_rt64_load_texture_mods();

    // Initialize other attributes.
    RT64.scissorRect = { 0, 0, 0, 0 };
    RT64.viewportRect = { 0, 0, 0, 0 };
    RT64.fogColor = { 0.0f, 0.0f, 0.0f };
    RT64.fogMul = RT64.fogOffset = 0;

    // Initialize the triangle list index array used by all meshes.
    unsigned int index = 0;
    while (index < MAX_BUFFERED_MODEL_SPACE * 3) {
        RT64.indexTriangleList[index] = index;
        index++;
    }

    // Build identity matrix.
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            RT64.identityTransform.m[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }

    // Build a default material.
    RT64.defaultMaterial.ignoreNormalFactor = 0.0f;
    RT64.defaultMaterial.uvDetailScale = 1.0f;
    RT64.defaultMaterial.reflectionFactor = 0.0f;
    RT64.defaultMaterial.reflectionFresnelFactor = 1.0f;
    RT64.defaultMaterial.reflectionShineFactor = 0.0f;
    RT64.defaultMaterial.refractionFactor = 0.0f;
    RT64.defaultMaterial.specularColor = { 255.0f, 255.0f, 255.0f };
    RT64.defaultMaterial.specularIntensity = 1.0f;
    RT64.defaultMaterial.specularShinyness = 5.0f;
    RT64.defaultMaterial.solidAlphaMultiplier = 1.0f;
    RT64.defaultMaterial.shadowAlphaMultiplier = 1.0f;
    RT64.defaultMaterial.diffuseColorMix = { 0.0f, 0.0f, 0.0f, 0.0f };
    RT64.defaultMaterial.depthBias = 0.0f;
    RT64.defaultMaterial.shadowRayBias = 1.0f;
    RT64.defaultMaterial.selfLightColor = { 0.0f, 0.0f, 0.0f };
    RT64.defaultMaterial.selfLightIntensity = 1.0f;
    RT64.defaultMaterial.lightGroupMaskBits = RT64_LIGHT_GROUP_MASK_ALL;
    RT64.defaultMaterial.fogColor = { 255.0f, 255.0f, 255.0f };
    RT64.defaultMaterial.fogMul = 0.0f;
    RT64.defaultMaterial.fogOffset = 0.0f;
    RT64.defaultMaterial.fogEnabled = false;
    RT64.defaultMaterial.lockMask = 0.0f;
    RT64.defaultMaterial.reflectionColor = { 255.0f, 255.0f, 255.0f };
    RT64.defaultMaterial.shadowEnabled = 1;
    RT64.defaultMaterial.shadowCenter = 0;
    RT64.defaultMaterial.specularTint = 1;
    RT64.defaultMaterial.shadingModel = RT64_SHADING_MODEL_BLINN;
    RT64.defaultMaterial.diffuseIntensity = 1.0f;
    RT64.defaultMaterial.specularFactor = 1.0f;
    RT64.defaultMaterial.specularEccentricity = 0.3f;
    RT64.defaultMaterial.bumpStrength = 1.0f;
    RT64.defaultMaterial.normalStrength = 1.0f;
    RT64.defaultMaterial.textureGenEnabled = 0;
    RT64.defaultMaterial.textureGenU = { 0.0f, 0.0f, 0.0f, 0.0f };
    RT64.defaultMaterial.textureGenV = { 0.0f, 0.0f, 0.0f, 0.0f };

    // Initialize camera.
    GameFrame *cpuFrame = &RT64.frames[RT64.cpuFrameIndex];
    cpuFrame->viewMatrix = RT64.identityTransform;
    cpuFrame->nearDist = 1.0f;
    cpuFrame->farDist = 1000.0f;
    cpuFrame->fovRadians = 0.75f;

    // Apply loaded configuration.
    gfx_rt64_apply_config();

    // Setup device.
    RT64.device = RT64.lib.CreateDevice(RT64.hwnd);
    if (RT64.device == nullptr) {
        const char *rt64Error = (RT64.lib.GetLastError != nullptr) ? RT64.lib.GetLastError() : nullptr;
        gfx_rt64_error_message("RT64", (rt64Error != nullptr) ? rt64Error : "No error message was reported.");
        gfx_rt64_error_message("RT64",
            "Failed to initialize RT64.\n\n"
            "Please make sure your GPU drivers are up to date and the Direct3D 12.1 feature level is supported.\n\n"
            "Windows 10 version 2004 or newer is also required for this feature level to work properly.\n\n"
            "If you're a mobile user, make sure that the high performance device is selected for this application on your system's settings.");

        abort();
    }

    // Create the render thread.
    RT64.renderThreadRunning = true;
    RT64.renderInspectorActive = false;
    RT64.renderThread = new std::thread(gfx_rt64_render_thread);
}

static void gfx_rt64_rapi_get_dimensions(u32 *width, u32 *height) {
    RECT rect;
    GetClientRect(RT64.hwnd, &rect);
    *width = rect.right - rect.left;
    *height = rect.bottom - rect.top;
}

static void gfx_rt64_rapi_on_resize(void) {
    u32 w = 0, h = 0;
    gfx_rt64_rapi_get_dimensions(&w, &h);
    configWindow.w = w;
    configWindow.h = h;
}

static void gfx_rt64_rapi_shutdown(void) {
    if (RT64.renderThread != nullptr) {
        RT64.renderThreadRunning = false;
        RT64.renderFrameCV.notify_all();
        RT64.renderThread->join();
        delete RT64.renderThread;
        RT64.renderThread = nullptr;
    }

    if (RT64.device != nullptr) {
        for (auto &dlPair : RT64.gpuDisplayLists) {
            GPUDisplayList &dl = dlPair.second;
            for (auto &instance : dl.instances) {
                if (instance.instance != nullptr) {
                    RT64.lib.DestroyInstance(instance.instance);
                }
            }
        }
        RT64.gpuDisplayLists.clear();

        for (auto &pair : RT64.gpuStaticMeshes) { gfx_rt64_destroy_gpu_mesh(pair.second); }
        RT64.gpuStaticMeshes.clear();

        RT64.prevTickMeshHashes.clear();
        RT64.curTickMeshHashes.clear();

        for (auto *pool : { &RT64.gpuDynamicRtMeshes, &RT64.gpuDynamicRasterMeshes }) {
            for (auto &pair : *pool) { gfx_rt64_destroy_gpu_mesh(pair.second); }
            pool->clear();
        }

        for (auto &pair : RT64.hashToTexture) {
            if (pair.second != nullptr) {
                RT64.lib.DestroyTexture(pair.second);
            }
        }
        RT64.hashToTexture.clear();
        RT64.gpuTextures.clear();

        if (RT64.blankTexture != nullptr) {
            RT64.lib.DestroyTexture(RT64.blankTexture);
            RT64.blankTexture = nullptr;
        }

        gfx_rt64_destroy_all_shaders();

        if (RT64.renderInspector != nullptr) {
            RT64.lib.DestroyInspector(RT64.renderInspector);
            RT64.renderInspector = nullptr;
        }

        if (RT64.view != nullptr) {
            RT64.lib.DestroyView(RT64.view);
            RT64.view = nullptr;
        }

        if (RT64.scene != nullptr) {
            RT64.lib.DestroyScene(RT64.scene);
            RT64.scene = nullptr;
        }

        RT64.lib.DestroyDevice(RT64.device);
        RT64.device = nullptr;
    }

}

static void gfx_rt64_rapi_start_frame(void) {
    gfx_rt64_sync_post_process_size();

    if (RT64.geoLayoutOriginsTimestamp != gGlobalTimer) {
        RT64.geoLayoutOriginsTimestamp = gGlobalTimer;
        {
            const std::lock_guard<std::mutex> pickLock(RT64.pickTextureMutex);
            memcpy(RT64.pickedGeoLayoutOrigins, RT64.buildingGeoLayoutOrigins, sizeof(RT64.pickedGeoLayoutOrigins));
            RT64.pickedGeoLayoutOriginCount = RT64.buildingGeoLayoutOriginCount;
        }
        RT64.buildingGeoLayoutOriginCount = 0;
    }

    gfx_rt64_publish_picked_geo_layout();

    RT64.frameEnded = false;

    gfx_rt64_apply_config();

    GameFrame *cpuFrame = &RT64.frames[RT64.cpuFrameIndex];

    // Reset frame view interpolation.
    cpuFrame->interpolateView = true;

    RT64.instancesDrawn = 0;
    RT64.background = true;
    RT64.graphNodeMod = nullptr;
    RT64.cachedDisplayList = nullptr;
    RT64.materialNameHashes.clear();
    RT64.materialNameHashDl = nullptr;
    RT64.materialNameHashCached = 0;
    RT64.materialModNameHashes.clear();
}

static void gfx_rt64_rapi_set_special_stage_lights(int levelIndex, int areaIndex) {
    GameFrame *cpuFrame = &RT64.frames[RT64.cpuFrameIndex];

    // Dynamic Lakitu camera light for Shifting Sand Land Pyramid.
    if ((levelIndex == LEVEL_SSL) && (areaIndex == 2)) {
        auto &dl = cpuFrame->displayLists[0];
        RT64_VECTOR3 viewPos = { cpuFrame->invViewMatrix.m[3][0], cpuFrame->invViewMatrix.m[3][1], cpuFrame->invViewMatrix.m[3][2] };
        RT64_VECTOR3 marioPos = { gMarioState->pos[0], gMarioState->pos[1], gMarioState->pos[2] };

        // Set the transform towards the back of the camera facing away from Mario.
        dl.transform = RT64.identityTransform;
        dl.transform.m[3][0] = viewPos.x + (viewPos.x - marioPos.x);
        dl.transform.m[3][1] = viewPos.y + 150.0f;
        dl.transform.m[3][2] = viewPos.z + (viewPos.z - marioPos.z);

        // Configure the rest of the light.
        auto &light = dl.light;
        light.position = { 0.0f, 0.0f, 0.0f };
        light.diffuseColor = { 255.0f, 230.0f, 128.0f };
        light.intensity = 1.0f;
        light.attenuationRadius = 4000.0f;
        light.attenuationExponent = 1.0f;
        light.pointRadius = 25.0f;
        light.specularColor = { 166.0f, 149.0f, 83.0f };
        light.shadowOffset = 1000.0f;
        light.groupBits = RT64_LIGHT_GROUP_DEFAULT;
    }
}

static void gfx_rt64_rapi_end_frame(void) {
    if (RT64.frameEnded) {
        return;
    }
    RT64.frameEnded = true;

    gfx_rt64_capture_post_process_uniforms();

    GameFrame *cpuFrame = &RT64.frames[RT64.cpuFrameIndex];

    // Add the special stage lights.
    int levelIndex = gfx_rt64_get_level_index();
    int areaIndex = gfx_rt64_get_area_index();
    gfx_rt64_rapi_set_special_stage_lights(levelIndex, areaIndex);

    {
        const std::lock_guard<std::mutex> lightingLock(RT64.levelAreaLightingMutex);

        // Update the scene's description.
        const AreaLighting &areaLighting = gfx_gfx_rt64_get_area_lighting(levelIndex, areaIndex);
        cpuFrame->sceneDesc = areaLighting.sceneDesc;
        cpuFrame->sceneDesc.skyDiffuseMultiplier.x *= RT64.skyDiffuseMultiplier.x;
        cpuFrame->sceneDesc.skyDiffuseMultiplier.y *= RT64.skyDiffuseMultiplier.y;
        cpuFrame->sceneDesc.skyDiffuseMultiplier.z *= RT64.skyDiffuseMultiplier.z;
        cpuFrame->skyTextureKey = RT64.skyTextureKey;
        cpuFrame->areaLightCount = (unsigned int)(areaLighting.lightCount);
        memcpy(cpuFrame->areaLights, areaLighting.lights, sizeof(RT64_LIGHT) * cpuFrame->areaLightCount);
    }

    for (const auto &tickLightPair : RT64.tickLights) {
        auto &displayList = cpuFrame->displayLists[tickLightPair.first];
        displayList.light = tickLightPair.second.light;
        displayList.transform = tickLightPair.second.transform;
    }

    // Clean up any unused instances or meshes from the display lists.
    auto dlIt = cpuFrame->displayLists.begin();
    while (dlIt != cpuFrame->displayLists.end()) {
        auto &dl = dlIt->second;

        // Destroy all unused instances.
        while (dl.instances.size() > (size_t)(dl.drawCount)) { dl.instances.pop_back(); }

        // Destroy all unused meshes.
        while (dl.meshes.size() > (size_t)(dl.drawCount)) {
            free(dl.meshes.back().vertexBuffer);
            dl.meshes.pop_back();
        }

        // Keep the display list if it's not empty. Erase it otherwise.
        if (!dl.instances.empty() || !dl.meshes.empty() || (dl.light.groupBits > 0)) {
            dlIt++;
        }
        else {
            dlIt = cpuFrame->displayLists.erase(dlIt);
        }
    }

    {
        std::unique_lock<std::mutex> lock(RT64.renderFrameIndexMutex);
        RT64.renderFrameCV.wait(lock, [] { return !RT64.renderThreadRunning || (RT64.gpuFrameIndex < 0); });
    }

    // Submit the current CPU frame as the next frame to draw and start writing on the next CPU frame.
    bool waitForBarrier = false;
    {
        const std::lock_guard<std::mutex> lock(RT64.renderFrameIndexMutex);
        RT64.gpuFrameIndex = RT64.cpuFrameIndex;
        RT64.cpuFrameIndex = (RT64.cpuFrameIndex + 1) % MAX_RENDER_FRAMES;
        waitForBarrier = (RT64.cpuFrameIndex == RT64.barrierFrameIndex);
    }
    RT64.renderFrameCV.notify_all();

    // Stall the thread until the barrier is lifted if we're trying to write on a frame being used by the GPU.
    if (waitForBarrier) {
        std::unique_lock<std::mutex> lock(RT64.renderFrameIndexMutex);
        RT64.renderFrameCV.wait(lock, [] { return !RT64.renderThreadRunning || (RT64.cpuFrameIndex != RT64.barrierFrameIndex); });
    }

    // Reset display lists for the next CPU frame.
    cpuFrame = &RT64.frames[RT64.cpuFrameIndex];
    for (auto &dlPair : cpuFrame->displayLists) {
        dlPair.second.light.groupBits = 0;
        dlPair.second.drawCount = 0;
    }

    // Clear variables for next frame.
    RT64.cachedDisplayList = nullptr;
}

static void gfx_rt64_rapi_finish_render(void) {
}

static const char *gfx_rt64_rapi_get_name(void) {
    return "RT64";
}

static bool gfx_rt64_rapi_is_legacy(void) {
    return false;
}

static u32 gfx_rt64_rapi_get_capabilities(void) {
    return GFX_BACKEND_MODEL_SPACE_GEOMETRY | GFX_BACKEND_GPU_VISIBILITY |
           GFX_BACKEND_OBJECT_IDENTITY | GFX_BACKEND_PRESENTS_DIRECTLY |
           GFX_BACKEND_SEPARATE_SKYBOX;
}

static void gfx_rt64_rapi_set_camera_perspective(float fov_degrees, float near_dist, float far_dist, bool can_interpolate) {
    GameFrame *cpuFrame = &RT64.frames[RT64.cpuFrameIndex];
    cpuFrame->fovRadians = (fov_degrees / 180.0f) * (float)(M_PI);

    cpuFrame->nearDist = (gProjectionVanillaNearValue > 0) ? (float)(gProjectionVanillaNearValue) : near_dist;
    cpuFrame->farDist = far_dist;
    cpuFrame->interpolateView = cpuFrame->interpolateView && can_interpolate;
}

static void gfx_rt64_rapi_set_camera_matrix(float matrix[4][4]) {
    GameFrame *cpuFrame = &RT64.frames[RT64.cpuFrameIndex];
    memcpy(&cpuFrame->viewMatrix.m, matrix, sizeof(float) * 16);
    gd_inverse_mat4f(&cpuFrame->viewMatrix.m, &cpuFrame->invViewMatrix.m);
}

static void gfx_rt64_apply_geo_layout_mod_to_graph_node(void *graphNode, RecordedMod *geoMod, bool replace) {
    std::shared_ptr<RecordedMod> &graphMod = RT64.graphNodeMods[graphNode];
    if (graphMod == nullptr) {
        graphMod = std::make_shared<RecordedMod>();
    }

    if (geoMod->materialMod != nullptr) {
        if (graphMod->materialMod == nullptr) {
            graphMod->materialMod = new RT64_MATERIAL();
            graphMod->materialMod->enabledAttributes = RT64_ATTRIBUTE_NONE;
        }

        if (replace) {
            graphMod->materialMod->enabledAttributes = RT64_ATTRIBUTE_NONE;
        }

        RT64_ApplyMaterialAttributes(graphMod->materialMod, geoMod->materialMod);
        graphMod->materialMod->enabledAttributes |= geoMod->materialMod->enabledAttributes;
    }
    else if (replace && (graphMod->materialMod != nullptr)) {
        graphMod->materialMod->enabledAttributes = RT64_ATTRIBUTE_NONE;
    }

    if (geoMod->lightMod != nullptr) {
        if (graphMod->lightMod == nullptr) {
            graphMod->lightMod = new RT64_LIGHT();
        }

        memcpy(graphMod->lightMod, geoMod->lightMod, sizeof(RT64_LIGHT));
    }
    else if (replace) {
        delete graphMod->lightMod;
        graphMod->lightMod = nullptr;
    }

    if (replace || (geoMod->bumpMapHash != 0)) {
        graphMod->bumpMapHash = geoMod->bumpMapHash;
    }

    if (replace || (geoMod->normalMapHash != 0)) {
        graphMod->normalMapHash = geoMod->normalMapHash;
    }

    if (replace || (geoMod->specularMapHash != 0)) {
        graphMod->specularMapHash = geoMod->specularMapHash;
    }
}

static void gfx_rt64_bind_named_geo_layout(const std::string &geoName, void *geoLayout);

static void gfx_rt64_rapi_register_layout_graph_node(void *geoLayout, void *graphNode) {
    if (graphNode != nullptr) {
        RT64.graphNodeMods.erase(graphNode);
        RT64.graphNodeGeoLayouts.erase(graphNode);
        RT64.graphNodeRootsNamed.erase(graphNode);
        RT64.graphNodeModsSynced.erase(graphNode);
    }

    if ((geoLayout != nullptr) && (graphNode != nullptr)) {
        RT64.graphNodeGeoLayouts[graphNode] = geoLayout;

        void *georef = (void *)((struct GraphNode *)(graphNode))->georef;
        if ((georef != geoLayout) && (RT64.geoLayoutMods.find(geoLayout) == RT64.geoLayoutMods.end())) {
            std::string geoName;
            if (georef != nullptr) {
                auto nameIt = RT64.geoLayoutNameMap.find(georef);
                if (nameIt != RT64.geoLayoutNameMap.end()) {
                    geoName = nameIt->second;
                }
            }
            else {
                const char *customName = dynos_actor_get_custom_name(geoLayout);
                if (customName != nullptr) { geoName = customName; }
            }

            if (!geoName.empty()) {
                gfx_rt64_bind_named_geo_layout(geoName, geoLayout);
            }
        }

        // Find the mod for the specified geoLayout.
        auto it = RT64.geoLayoutMods.find(geoLayout);
        RecordedMod *geoMod = (it != RT64.geoLayoutMods.end()) ? it->second : nullptr;
        if (geoMod != nullptr) {
            gfx_rt64_apply_geo_layout_mod_to_graph_node(graphNode, geoMod, false);
        }
    }
}

static void gfx_rt64_refresh_graph_node_mod(void *graphNode) {
    auto geoIt = RT64.graphNodeGeoLayouts.find(graphNode);
    if (geoIt == RT64.graphNodeGeoLayouts.end()) {
        return;
    }

    auto modIt = RT64.geoLayoutMods.find(geoIt->second);
    if (modIt == RT64.geoLayoutMods.end()) {
        return;
    }

    gfx_rt64_apply_geo_layout_mod_to_graph_node(graphNode, modIt->second, true);
}

static void *gfx_rt64_rapi_build_graph_node_mod(void *graphNode, float modelview_matrix[4][4], u32 uid) {
    if (RT64.tickLightsTimestamp != gGlobalTimer) {
        RT64.tickLightsTimestamp = gGlobalTimer;
        RT64.tickLights.clear();
    }

    const bool modIsStale = RT64.graphNodeModsSynced.insert(graphNode).second;
    if (modIsStale || RT64.renderInspectorActive) {
        gfx_rt64_refresh_graph_node_mod(graphNode);
    }

    if (RT64.renderInspectorActive) {
        if ((RT64.publishedGeoLayout != nullptr) && (RT64.buildingGeoLayoutOriginCount < RT64Context::maxPickedGeoLayoutOrigins)) {
            auto geoIt = RT64.graphNodeGeoLayouts.find(graphNode);
            if ((geoIt != RT64.graphNodeGeoLayouts.end()) && (geoIt->second == RT64.publishedGeoLayout)) {
                GameFrame *cpuFrame = &RT64.frames[RT64.cpuFrameIndex];
                RT64_MATRIX4 worldTransform;
                gfx_matrix_mul(worldTransform.m, modelview_matrix, cpuFrame->invViewMatrix.m);
                RT64.buildingGeoLayoutOrigins[RT64.buildingGeoLayoutOriginCount++] =
                    { worldTransform.m[3][0], worldTransform.m[3][1], worldTransform.m[3][2] };
            }
        }
    }

    auto graphNodeIt = RT64.graphNodeMods.find(graphNode);
    if (graphNodeIt != RT64.graphNodeMods.end()) {
        RecordedMod *graphNodeMod = graphNodeIt->second.get();
        if (graphNodeMod != nullptr) {
            if (graphNodeMod->lightMod != nullptr) {
                GameFrame *cpuFrame = &RT64.frames[RT64.cpuFrameIndex];

                RT64Context::TickLight &tickLight = RT64.tickLights[uid];
                gfx_matrix_mul(tickLight.transform.m, modelview_matrix, cpuFrame->invViewMatrix.m);
                tickLight.light = *graphNodeMod->lightMod;
            }

            return graphNodeMod;
        }
    }

    return nullptr;
}

static void gfx_rt64_rapi_set_graph_node_mod(void *graph_node_mod) {
    RT64.graphNodeMod = (RecordedMod *)(graph_node_mod);
}

static void gfx_rt64_rapi_set_texture_gen(bool enabled, const float coeffU[4], const float coeffV[4]) {
    RT64.textureGenEnabled = enabled;
    RT64.textureGenU = { coeffU[0], coeffU[1], coeffU[2], coeffU[3] };
    RT64.textureGenV = { coeffV[0], coeffV[1], coeffV[2], coeffV[3] };
}

static void gfx_rt64_rapi_register_layout_graph_node_entry(void *geoLayout, void *graphNode) {
    gfx_rt64_ensure_geo_layout_mods_loaded();
    gfx_rt64_rapi_register_layout_graph_node(geoLayout, graphNode);
}

static void gfx_rt64_rapi_inherit_graph_node_mod(void *originalGraphNode, void *replacementGraphNode) {
    if ((originalGraphNode == nullptr) || (replacementGraphNode == nullptr) || (originalGraphNode == replacementGraphNode)) {
        return;
    }

    auto modIt = RT64.graphNodeMods.find(originalGraphNode);
    if (modIt != RT64.graphNodeMods.end()) {
        std::shared_ptr<RecordedMod> mod = modIt->second;
        RT64.graphNodeMods[replacementGraphNode] = mod;
    }

    auto geoIt = RT64.graphNodeGeoLayouts.find(originalGraphNode);
    if (geoIt != RT64.graphNodeGeoLayouts.end()) {
        void *geoLayout = geoIt->second;
        RT64.graphNodeGeoLayouts[replacementGraphNode] = geoLayout;
    }
}

static void gfx_rt64_bind_named_geo_layout(const std::string &geoName, void *geoLayout) {
    RT64.geoLayoutNameMap[geoLayout] = geoName;

    auto nameIt = RT64.nameGeoLayoutMap.find(geoName);
    if (nameIt != RT64.nameGeoLayoutMap.end()) {
        auto modIt = RT64.geoLayoutMods.find(nameIt->second);
        if (modIt != RT64.geoLayoutMods.end()) {
            RT64.geoLayoutMods[geoLayout] = modIt->second;
        }
    }
    else {
        RT64.nameGeoLayoutMap[geoName] = geoLayout;
    }

    auto pendingIt = RT64.pendingGeoLayoutMods.find(geoName);
    if (pendingIt != RT64.pendingGeoLayoutMods.end()) {
        RT64.geoLayoutMods[geoLayout] = pendingIt->second;
        RT64.pendingGeoLayoutMods.erase(pendingIt);
    }

    gfx_rt64_invalidate_graph_node_mods();
}

static void *gfx_rt64_resolve_named_geo_layout(void *graphNodeRoot, void *geoLayout) {
    if ((geoLayout != nullptr) && (RT64.geoLayoutNameMap.find(geoLayout) != RT64.geoLayoutNameMap.end())) {
        return geoLayout;
    }

    if (geoLayout != nullptr) {
        const char *customName = dynos_actor_get_custom_name(geoLayout);
        if (customName != nullptr) {
            gfx_rt64_bind_named_geo_layout(customName, geoLayout);
            return geoLayout;
        }
    }

    if (graphNodeRoot != nullptr) {
        void *georef = (void *)((struct GraphNode *)(graphNodeRoot))->georef;
        if ((georef != nullptr) && (RT64.geoLayoutNameMap.find(georef) != RT64.geoLayoutNameMap.end())) {
            return georef;
        }
    }

    return geoLayout;
}

static void gfx_rt64_rapi_set_graph_node_root(void *graphNodeRoot) {
    RT64.graphNodeGeoLayout = nullptr;
    if (graphNodeRoot == nullptr) {
        return;
    }

    auto it = RT64.graphNodeGeoLayouts.find(graphNodeRoot);
    void *geoLayout = (it != RT64.graphNodeGeoLayouts.end()) ? it->second : nullptr;

    if (RT64.graphNodeRootsNamed.insert(graphNodeRoot).second) {
        void *namedGeoLayout = gfx_rt64_resolve_named_geo_layout(graphNodeRoot, geoLayout);
        if (namedGeoLayout != nullptr) {
            geoLayout = namedGeoLayout;
            RT64.graphNodeGeoLayouts[graphNodeRoot] = namedGeoLayout;
        }
    }

    RT64.graphNodeGeoLayout = geoLayout;
}

static bool gfx_rt64_rapi_set_skybox(const Texture *const *tiles, float diffuseColor[3]) {
    RT64.skyDiffuseMultiplier = { diffuseColor[0], diffuseColor[1], diffuseColor[2] };

    RT64.skyTextureKey = gfx_rt64_stitch_skybox_texture(tiles);

    return (RT64.skyTextureKey != 0);
}

static void gfx_rt64_rapi_main_loop_iter(void (*run_one_game_iter)(void)) {
    if (RT64.pauseMode) { return; }

    RT64.skyTextureKey = 0;
    RT64.skyDiffuseMultiplier = { 1.0f, 1.0f, 1.0f };

    LARGE_INTEGER gameStart = gfx_rt64_profile_marker();
    run_one_game_iter();
    LARGE_INTEGER gameEnd = gfx_rt64_profile_marker();

    if (RT64.renderInspectorActive) {
        double gameDeltaTimeMs = gfx_rt64_profile_delta(gameStart, gameEnd).QuadPart / 1000.0;

        char gameDeltaTimeMsg[64];
        snprintf(gameDeltaTimeMsg, sizeof(gameDeltaTimeMsg), "GAME: %.3f ms\n", gameDeltaTimeMs);

        char marioMessage[256] = "";
        if (gMarioState != nullptr) {
            snprintf(marioMessage, sizeof(marioMessage), "Mario pos: %.1f %.1f %.1f", gMarioState->pos[0], gMarioState->pos[1], gMarioState->pos[2]);
        }

        char levelMessage[256];
        snprintf(levelMessage, sizeof(levelMessage), "Level #%d Area #%d", gfx_rt64_get_level_index(), gfx_rt64_get_area_index());

        const std::lock_guard<std::mutex> lock(RT64.renderInspectorMutex);
        RT64.renderInspectorMessages.clear();
        RT64.renderInspectorMessages.push_back(std::string(gameDeltaTimeMsg));
        RT64.renderInspectorMessages.push_back(std::string(marioMessage));
        RT64.renderInspectorMessages.push_back(std::string(levelMessage));
        RT64.renderInspectorMessages.push_back(std::string("F2: Toggle inspectors"));
        RT64.renderInspectorMessages.push_back(std::string("F5: Save all configuration"));
    }
}

struct GfxRenderingAPI gfx_rt64_api = {
    gfx_rt64_rapi_z_is_from_0_to_1,
    gfx_rt64_rapi_unload_shader,
    gfx_rt64_rapi_load_shader,
    gfx_rt64_rapi_remove_shaders,
    gfx_rt64_rapi_create_and_load_new_shader,
    gfx_rt64_rapi_create_or_load_post_process_shader,
    gfx_rt64_rapi_lookup_shader,
    gfx_rt64_rapi_lookup_shader_using_index,
    gfx_rt64_rapi_shader_get_info,
    gfx_rt64_rapi_create_framebuffer,
    gfx_rt64_rapi_delete_framebuffer,
    gfx_rt64_rapi_set_framebuffer,
    gfx_rt64_rapi_reset_framebuffer,
    gfx_rt64_rapi_get_uniform_buffer_size,
    gfx_rt64_rapi_set_uniform_buffer,
    gfx_rt64_rapi_set_uniform,
    gfx_rt64_rapi_new_texture,
    gfx_rt64_rapi_select_texture,
    gfx_rt64_rapi_bind_texture_raw,
    gfx_rt64_rapi_upload_texture,
    gfx_rt64_rapi_set_sampler_parameters,
    gfx_rt64_rapi_set_depth_test,
    gfx_rt64_rapi_set_depth_mask,
    gfx_rt64_rapi_set_zmode_decal,
    gfx_rt64_rapi_set_viewport,
    gfx_rt64_rapi_set_scissor,
    gfx_rt64_rapi_set_use_alpha,
    gfx_rt64_rapi_set_vsync,
    gfx_rt64_rapi_draw_triangles,
    gfx_rt64_rapi_init,
    gfx_rt64_rapi_on_resize,
    gfx_rt64_rapi_start_frame,
    gfx_rt64_rapi_end_frame,
    gfx_rt64_rapi_finish_render,
    gfx_rt64_rapi_get_name,
    gfx_rt64_rapi_is_legacy,
    gfx_rt64_rapi_shutdown,

    gfx_rt64_rapi_set_fog,
    gfx_rt64_rapi_set_camera_perspective,
    gfx_rt64_rapi_set_camera_matrix,
    gfx_rt64_rapi_draw_triangles_ortho,
    gfx_rt64_rapi_draw_triangles_persp,
    gfx_rt64_rapi_set_graph_node_mod,
    gfx_rt64_rapi_set_texture_gen,
    gfx_rt64_rapi_register_layout_graph_node_entry,
    gfx_rt64_rapi_inherit_graph_node_mod,
    gfx_rt64_rapi_build_graph_node_mod,
    gfx_rt64_rapi_set_material_display_list,
    gfx_rt64_rapi_set_graph_node_root,
    gfx_rt64_rapi_lua_config_reset,
    gfx_rt64_rapi_lua_config_save,
    gfx_rt64_rapi_toggle_inspector,
    gfx_rt64_rapi_inspector_active,
    gfx_rt64_rapi_handle_window_message,
    gfx_rt64_rapi_main_loop_iter,
    gfx_rt64_rapi_get_capabilities,
    gfx_rt64_rapi_shader_uses_full_vertex_layout,
    gfx_rt64_rapi_set_skybox,
};

#endif // _WIN32
