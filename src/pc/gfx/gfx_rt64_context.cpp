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

#include "gfx_rt64_common.hpp"

RT64Context RT64;

static RT64_COMBINER_DESC gfx_rt64_combiner_desc_from_cc(struct ColorCombiner *cc);
static u64 gfx_rt64_combiner_desc_hash(const RT64_COMBINER_DESC &desc);

bool gfx_rt64_rapi_z_is_from_0_to_1(void) {
    return true;
}

void gfx_rt64_rapi_unload_shader(struct ShaderProgram *old_prg) {

}

void gfx_rt64_rapi_load_shader(struct ShaderProgram *new_prg) {
    RT64.shaderProgram = (ShaderProgramRT64 *)(new_prg);
}

struct ShaderProgram *gfx_rt64_rapi_create_and_load_new_shader(struct ColorCombiner *cc) {
    RT64_COMBINER_DESC desc = gfx_rt64_combiner_desc_from_cc(cc);
    u64 rt64Hash = gfx_rt64_combiner_desc_hash(desc);

    {
        const std::lock_guard<std::mutex> lock(RT64.shaderProgramsMutex);
        auto it = RT64.shaderPrograms.find(rt64Hash);
        if (it != RT64.shaderPrograms.end()) {
            gfx_rt64_rapi_load_shader((struct ShaderProgram *)(it->second));
            return (struct ShaderProgram *)(it->second);
        }
    }

    ShaderProgramRT64 *shaderProgram = new ShaderProgramRT64();
    shaderProgram->cc = desc;
    shaderProgram->hash = rt64Hash;

    struct CCFeatures ccf = { 0 };
    gfx_cc_get_features(cc, &ccf);
    shaderProgram->numInputs = (u8)(ccf.num_inputs);
    shaderProgram->usedTextures[0] = ccf.used_textures[0];
    shaderProgram->usedTextures[1] = ccf.used_textures[1];
    shaderProgram->usedFog = cc->cm.use_fog;

    const char *vsHookResult = nullptr;
    smlua_call_event_hooks(HOOK_ON_VERTEX_SHADER_CREATE, cc, &vsHookResult);

    const char *fsHookResult = nullptr;
    smlua_call_event_hooks(HOOK_ON_FRAGMENT_SHADER_CREATE, cc, &fsHookResult);

    if ((vsHookResult != nullptr) || (fsHookResult != nullptr)) {
        struct Shader *vertexShader = (struct Shader *)calloc(1, sizeof(struct Shader));
        struct Shader *fragmentShader = (struct Shader *)calloc(1, sizeof(struct Shader));

        if ((vertexShader != nullptr) && (fragmentShader != nullptr)) {
            gfx_generate_vertex_and_fragment_shader_from_cc(vertexShader, fragmentShader, cc, nullptr, nullptr);

            char *hlslVs = nullptr;
            char *hlslFs = nullptr;
            gfx_convert_spirv_to_hlsl(&hlslVs, vertexShader);
            gfx_convert_spirv_to_hlsl(&hlslFs, fragmentShader);

            if ((hlslVs != nullptr) && (hlslFs != nullptr)) {
                shaderProgram->hasCustomShader = true;
                shaderProgram->customVertexHLSL = hlslVs;
                shaderProgram->customFragmentHLSL = hlslFs;
                shaderProgram->vertexShader = vertexShader;
                shaderProgram->fragmentShader = fragmentShader;
                shaderProgram->customVertexInputs.clear();

                for (int i = 0; i < MAX_SHADER_INPUTS; i++) {
                    if (gShaderInputs[i].size == 0) { continue; }
                    RT64_SHADER_INPUT input;
                    input.location = (unsigned int)(vertexShader->shaderInputs[i].location);
                    input.size = (unsigned int)(vertexShader->shaderInputs[i].size);
                    // Points into the shader this program keeps for as long as it lives, which is
                    // what RT64 needs to name the attribute when it builds its hit shaders.
                    input.name = vertexShader->shaderInputs[i].name;
                    shaderProgram->customVertexInputs.push_back(input);
                }
            }
            else {
                gfx_destroy_shader(vertexShader);
                gfx_destroy_shader(fragmentShader);
            }

            free(hlslVs);
            free(hlslFs);
        }
        else {
            gfx_destroy_shader(vertexShader);
            gfx_destroy_shader(fragmentShader);
        }
    }

    {
        const std::lock_guard<std::mutex> lock(RT64.shaderProgramsMutex);
        RT64.shaderPrograms[rt64Hash] = shaderProgram;
    }

    gfx_rt64_rapi_load_shader((struct ShaderProgram *)(shaderProgram));

    return (struct ShaderProgram *)(shaderProgram);
}

struct ShaderProgram *gfx_rt64_rapi_lookup_shader(struct ColorCombiner *cc) {
    RT64_COMBINER_DESC desc = gfx_rt64_combiner_desc_from_cc(cc);
    u64 rt64Hash = gfx_rt64_combiner_desc_hash(desc);
    const std::lock_guard<std::mutex> lock(RT64.shaderProgramsMutex);
    auto it = RT64.shaderPrograms.find(rt64Hash);
    return (it != RT64.shaderPrograms.end()) ? (struct ShaderProgram *)(it->second) : nullptr;
}

static void gfx_rt64_destroy_shader_program(ShaderProgramRT64 *prg) {
    for (auto &variant : prg->shaderVariantMap) {
        if (variant.second != nullptr) {
            RT64.lib.DestroyShader(variant.second);
        }
    }

    gfx_destroy_shader(prg->vertexShader);
    gfx_destroy_shader(prg->fragmentShader);
    delete prg;
}

void gfx_rt64_destroy_all_shaders(void) {
    const std::lock_guard<std::mutex> lock(RT64.shaderProgramsMutex);
    for (auto &pair : RT64.shaderPrograms) {
        gfx_rt64_destroy_shader_program(pair.second);
    }
    RT64.shaderPrograms.clear();

    for (ShaderProgramRT64 *prg : RT64.retiredShaderPrograms) {
        gfx_rt64_destroy_shader_program(prg);
    }
    RT64.retiredShaderPrograms.clear();

    RT64.shaderProgram = nullptr;

    RT64.lastShaderProgram = nullptr;
    RT64.lastShaderVariant = nullptr;
}

static void gfx_rt64_update_post_process_shader(void);

void gfx_rt64_rapi_remove_shaders(void) {
    const std::lock_guard<std::mutex> lock(RT64.shaderProgramsMutex);

    for (auto &pair : RT64.shaderPrograms) {
        RT64.retiredShaderPrograms.push_back(pair.second);
    }
    RT64.shaderPrograms.clear();

    RT64.shaderProgram = nullptr;

    RT64.lastShaderProgram = nullptr;
    RT64.lastShaderVariant = nullptr;

    gfx_rt64_update_post_process_shader();
}

struct ShaderProgram *gfx_rt64_rapi_lookup_shader_using_index(u8 shaderIndex, u8 framePassIndex) {
    return nullptr;
}

void gfx_rt64_rapi_shader_get_info(struct ShaderProgram *prg, u8 *num_inputs, bool used_textures[2]) {
    ShaderProgramRT64 *p = (ShaderProgramRT64 *)(prg);
    *num_inputs = p->numInputs;
    used_textures[0] = p->usedTextures[0];
    used_textures[1] = p->usedTextures[1];
}

bool gfx_rt64_rapi_shader_uses_full_vertex_layout(struct ShaderProgram *prg) {
    if (prg == nullptr) { return false; }
    const ShaderProgramRT64 *p = (const ShaderProgramRT64 *)(prg);
    return p->hasCustomShader && !p->customShaderFailed.load(std::memory_order_relaxed);
}

static RT64_COMBINER_DESC gfx_rt64_combiner_desc_from_cc(struct ColorCombiner *cc) {
    RT64_COMBINER_DESC desc = {};
    memcpy(desc.rgb1, &cc->shader_commands[0], 4);
    memcpy(desc.alpha1, &cc->shader_commands[4], 4);
    desc.use2Cycle = cc->cm.use_2cycle ? 1 : 0;

    if (desc.use2Cycle) {
        memcpy(desc.rgb2, &cc->shader_commands[8], 4);
        memcpy(desc.alpha2, &cc->shader_commands[12], 4);
    } else {
        memset(desc.rgb2, SHADER_0, 4);
        memset(desc.alpha2, SHADER_0, 4);
    }

    desc.optAlpha = cc->cm.use_alpha ? 1 : 0;
    desc.optTextureEdge = (cc->cm.texture_edge && cc->cm.use_alpha) ? 1 : 0;
    desc.optNoise = (cc->cm.use_alpha && cc->cm.use_dither) ? 1 : 0;
    return desc;
}

static u64 gfx_rt64_combiner_desc_hash(const RT64_COMBINER_DESC &desc) {
    u64 h = 1469598103934665603ull;
    const u8 *bytes = (const u8 *)(&desc);
    for (size_t i = 0; i < sizeof(RT64_COMBINER_DESC); i++) {
        h ^= bytes[i];
        h *= 1099511628211ull;
    }
    return h;
}

static void gfx_rt64_update_post_process_shader(void) {
    if ((gPostProcessShaderInputs == nullptr) || (gPostProcessShaderBindings == nullptr)) { return; }

    struct Shader *vertexShader = (struct Shader *)calloc(1, sizeof(struct Shader));
    struct Shader *fragmentShader = (struct Shader *)calloc(1, sizeof(struct Shader));
    if ((vertexShader == nullptr) || (fragmentShader == nullptr)) {
        gfx_destroy_shader(vertexShader);
        gfx_destroy_shader(fragmentShader);
        return;
    }

    char *hlslFs = nullptr;
    if (gfx_generate_post_process_vertex_and_fragment_shader(vertexShader, fragmentShader, nullptr, nullptr)) {
        gfx_convert_spirv_to_hlsl(&hlslFs, fragmentShader);
    }

    {
        const std::lock_guard<std::mutex> lock(RT64.postProcessMutex);
        RT64.postProcessHLSL = (hlslFs != nullptr) ? hlslFs : "";
        RT64.postProcessOutputName.clear();
        RT64.postProcessInputNames.clear();
        RT64.postProcessInputs.clear();

        if (hlslFs != nullptr) {
            RT64.postProcessOutputName = fragmentShader->shaderOutputs[0].name;

            std::vector<RT64_SHADER_INPUT> stagedInputs;
            for (int i = 0; i < MAX_SHADER_INPUTS; i++) {
                if (fragmentShader->shaderInputs[i].name[0] == '\0') { continue; }

                unsigned int size = 0;
                for (int j = 0; j < MAX_SHADER_INPUTS; j++) {
                    if ((vertexShader->shaderInputs[j].size > 0) &&
                        (vertexShader->shaderInputs[j].location == fragmentShader->shaderInputs[i].location)) {
                        size = (unsigned int)(vertexShader->shaderInputs[j].size);
                        break;
                    }
                }

                if (size == 0) { continue; }

                RT64_SHADER_INPUT input;
                input.location = (unsigned int)(fragmentShader->shaderInputs[i].location);
                input.size = size;
                input.name = nullptr;
                stagedInputs.push_back(input);
                RT64.postProcessInputNames.push_back(fragmentShader->shaderInputs[i].name);
            }

            for (size_t i = 0; i < stagedInputs.size(); i++) {
                stagedInputs[i].name = RT64.postProcessInputNames[i].c_str();
                RT64.postProcessInputs.push_back(stagedInputs[i]);
            }
        }

        RT64.postProcessDirty.store(true, std::memory_order_release);
    }

    free(hlslFs);
    gfx_destroy_shader(vertexShader);
    gfx_destroy_shader(RT64.postProcessShader);
    RT64.postProcessShader = fragmentShader;
}

void gfx_rt64_capture_post_process_uniforms(void) {
    const std::lock_guard<std::mutex> lock(RT64.postProcessMutex);
    RT64.postProcessUniformBlocks.clear();
    RT64.postProcessUniformData.clear();

    if (RT64.postProcessShader == nullptr) { return; }

    for (int i = 0; i < RT64.postProcessShader->uniformBlockCount; i++) {
        const struct ShaderUniformBlock *block = &RT64.postProcessShader->uniformBlocks[i];
        if ((block->size == 0) || (block->buffer == nullptr)) { continue; }

        // As above - past the last register there is nowhere to put it, so say so rather than
        // letting the shader read zeroes and leaving no sign of why.
        if (block->location >= RT64_MAX_SHADER_UNIFORM_BLOCKS) {
            static bool sReported = false;
            if (!sReported) {
                sReported = true;
                fprintf(stderr, "RT64: the post process shader declares more uniform blocks than there are constant buffer registers for (%d). The ones past that read as zero.\n",
                    RT64_MAX_SHADER_UNIFORM_BLOCKS);
            }
            continue;
        }

        RT64_SHADER_UNIFORM_BLOCK entry;
        entry.shaderRegister = block->location;
        entry.size = block->size;
        entry.data = nullptr;
        RT64.postProcessUniformBlocks.push_back(entry);
        RT64.postProcessUniformData.insert(RT64.postProcessUniformData.end(), block->buffer, block->buffer + block->size);
    }

    size_t dataOffset = 0;
    for (RT64_SHADER_UNIFORM_BLOCK &entry : RT64.postProcessUniformBlocks) {
        entry.data = RT64.postProcessUniformData.data() + dataOffset;
        dataOffset += entry.size;
    }
}

struct ShaderProgram *gfx_rt64_rapi_create_or_load_post_process_shader(void) {
    gfx_rt64_update_post_process_shader();
    return nullptr;
}

void gfx_rt64_rapi_create_framebuffer(struct FramePass *framePass) {
    if (framePass == nullptr) { return; }

    u32 passWidth = 0, passHeight = 0;
    gfx_get_frame_pass_viewport_dimensions(framePass, &passWidth, &passHeight);

    const s32 width = passWidth;
    const s32 height = passHeight;
    if ((RT64.postProcessWidth == width) && (RT64.postProcessHeight == height)) { return; }

    RT64.postProcessWidth = width;
    RT64.postProcessHeight = height;
    gfx_rt64_update_post_process_shader();
}

void gfx_rt64_rapi_delete_framebuffer(struct FramePass *framePass) {
    if (framePass == nullptr) { return; }
    if ((RT64.postProcessWidth == 0) && (RT64.postProcessHeight == 0)) { return; }

    RT64.postProcessWidth = 0;
    RT64.postProcessHeight = 0;
    gfx_rt64_update_post_process_shader();
}

void gfx_rt64_rapi_set_framebuffer(struct FramePass *framePass) {
}

void gfx_rt64_rapi_reset_framebuffer(void) {
}

static struct Shader *gfx_rt64_shader_for_stage(ShaderProgramRT64 *prg, enum ShaderStage stage) {
    if (prg == nullptr) { return nullptr; }

    if (stage == SHADER_STAGE_VERTEX) {
        return prg->vertexShader;
    } else if (stage == SHADER_STAGE_FRAGMENT) {
        return prg->fragmentShader;
    }

    return nullptr;
}

size_t gfx_rt64_rapi_get_uniform_buffer_size(enum ShaderStage stage, int bufferIndex) {
    if (bufferIndex < 0 || bufferIndex >= MAX_UNIFORM_BLOCKS) { return 0; }

    struct Shader *shader = gfx_rt64_shader_for_stage(RT64.shaderProgram, stage);
    if (shader == nullptr) { return 0; }

    return shader->uniformBlocks[bufferIndex].size;
}

void gfx_rt64_rapi_set_uniform_buffer(enum ShaderStage stage, const char *name) {
    struct Shader *shader = gfx_rt64_shader_for_stage(RT64.shaderProgram, stage);
    if (shader == nullptr) { return; }

    int *destination = (stage == SHADER_STAGE_VERTEX) ? &gSelectedVertexUniformBuffer : &gSelectedFragmentUniformBuffer;
    for (int i = 0; i < MAX_UNIFORM_BLOCKS; i++) {
        struct ShaderUniformBlock *uniformBlock = &shader->uniformBlocks[i];
        if (strcmp(uniformBlock->name, name) == 0) {
            *destination = i;
        }
    }
}

static void gfx_rt64_set_uniform_for_specific_shader(struct ShaderUniformBlock *uniformBlock, const char *name, const void *data, u32 numElements) {
    for (int i = 0; i < MAX_SHADER_UNIFORMS; i++) {
        struct ShaderUniform *uniform = &uniformBlock->uniforms[i];
        if (uniform->size == 0) { break; }

        if (strcmp(uniform->name, name) == 0) {
            u8 *dst = uniformBlock->buffer + uniform->location;

            if (uniform->arrayLength > 1) {
                const u8 *src = (const u8 *)(data);
                u32 count = MIN(numElements, (u32)(uniform->arrayLength));
                for (u32 j = 0; j < count; j++) {
                    u8 *elementDst = dst + j * uniform->arrayStride;
                    const u8 *elementSrc = src + j * uniform->elementSize;
                    if (memcmp(elementDst, elementSrc, uniform->elementSize) == 0) { continue; }
                    memcpy(elementDst, elementSrc, uniform->elementSize);
                    uniformBlock->dirty = true;
                }
            } else if (memcmp(dst, data, uniform->size) != 0) {
                memcpy(dst, data, uniform->size);
                uniformBlock->dirty = true;
            }

            return;
        }
    }
}

void gfx_rt64_rapi_set_uniform(struct ShaderProgram *prg_, const char *name, UNUSED ShaderUniformType type, const void *data, u32 numElements) {
    ShaderProgramRT64 *prg = (ShaderProgramRT64 *)(prg_);
    if (prg == nullptr) {
        prg = RT64.shaderProgram;
        if (prg == nullptr) { return; }
    }

    if (gfx_shader_stage_is(SHADER_STAGE_VERTEX) && (prg->vertexShader != nullptr)) {
        gfx_rt64_set_uniform_for_specific_shader(&prg->vertexShader->uniformBlocks[gSelectedVertexUniformBuffer], name, data, numElements);
    }
    if (gfx_shader_stage_is(SHADER_STAGE_FRAGMENT) && (prg->fragmentShader != nullptr)) {
        gfx_rt64_set_uniform_for_specific_shader(&prg->fragmentShader->uniformBlocks[gSelectedFragmentUniformBuffer], name, data, numElements);
    }

    if ((RT64.postProcessShader != nullptr) && gfx_shader_stage_is(SHADER_STAGE_FRAGMENT)) {
        for (int i = 0; i < RT64.postProcessShader->uniformBlockCount; i++) {
            gfx_rt64_set_uniform_for_specific_shader(&RT64.postProcessShader->uniformBlocks[i], name, data, numElements);
        }
    }
}

void gfx_rt64_sync_post_process_size(void) {
    if (RT64.postProcessShader == nullptr) {
        gfx_rt64_update_post_process_shader();
    }

    for (s32 i = 0; i < MAX_CUSTOM_FRAME_PASSES; i++) {
        if (gFramePasses[i].active) { return; }
    }

    s32 width = 0, height = 0;
    if (gPostProcessShaderCustomWindowSize) {
        u32 dimWidth = 0, dimHeight = 0;
        gfx_get_dimensions(&dimWidth, &dimHeight);
        width = dimWidth;
        height = dimHeight;
    }

    if ((RT64.postProcessWidth == width) && (RT64.postProcessHeight == height)) { return; }

    RT64.postProcessWidth = width;
    RT64.postProcessHeight = height;
    RT64.postProcessDirty.store(true, std::memory_order_release);
}

static u16 gfx_rt64_shader_variant_key(bool raytrace, int filter, int hAddr, int vAddr, bool normalMap, bool specularMap, bool bumpMap) {
    u16 key = 0, fact = 1;
    key += raytrace ? fact : 0; fact *= 2;
    key += filter * fact; fact *= 2;
    key += hAddr * fact; fact *= 3;
    key += vAddr * fact; fact *= 3;
    key += normalMap ? fact : 0; fact *= 2;
    key += specularMap ? fact : 0; fact *= 2;
    key += bumpMap ? fact : 0; fact *= 2;
    return key;
}

static RT64_SHADER *gfx_rt64_render_thread_load_shader_variant(ShaderProgramRT64 *shaderProgram, bool raytrace, int filter, int hAddr, int vAddr, bool normalMap, bool specularMap, bool bumpMap) {
    const u16 variantKey = gfx_rt64_shader_variant_key(raytrace, filter, hAddr, vAddr, normalMap, specularMap, bumpMap);
    if ((RT64.lastShaderVariant != nullptr) && (RT64.lastShaderProgram == shaderProgram) && (RT64.lastShaderVariantKey == variantKey)) {
        return RT64.lastShaderVariant;
    }

    if (shaderProgram->shaderVariantMap[variantKey] == nullptr) {
        int flags = raytrace ? RT64_SHADER_RAYTRACE_ENABLED : RT64_SHADER_RASTER_ENABLED;
        if (normalMap) {
            flags |= RT64_SHADER_NORMAL_MAP_ENABLED;
        }

        if (specularMap) {
            flags |= RT64_SHADER_SPECULAR_MAP_ENABLED;
        }

        if (bumpMap) {
            flags |= RT64_SHADER_BUMP_MAP_ENABLED;
        }

        const bool useCustomSource = shaderProgram->hasCustomShader &&
                                     !shaderProgram->customShaderFailed.load(std::memory_order_relaxed);
        if (useCustomSource) {
            const char *fragmentOutputName = (shaderProgram->fragmentShader != nullptr) ? shaderProgram->fragmentShader->shaderOutputs[0].name : nullptr;
            RT64_SHADER *customShader = RT64.lib.CreateShaderFromSource(RT64.device, shaderProgram->cc, shaderProgram->customVertexHLSL.c_str(), shaderProgram->customFragmentHLSL.c_str(), shaderProgram->customVertexInputs.data(), (unsigned int)(shaderProgram->customVertexInputs.size()), fragmentOutputName, filter, hAddr, vAddr, flags);

            if (customShader == nullptr) {
                shaderProgram->customShaderFailed.store(true, std::memory_order_relaxed);
                const char *rt64Error = (RT64.lib.GetLastError != nullptr) ? RT64.lib.GetLastError() : nullptr;
                fprintf(stderr, "RT64: could not build a shader from custom source, using the built-in shader instead. %s\n",
                    (rt64Error != nullptr) ? rt64Error : "No error message was reported.");
                customShader = RT64.lib.CreateShader(RT64.device, shaderProgram->cc, filter, hAddr, vAddr, flags);
            }

            shaderProgram->shaderVariantMap[variantKey] = customShader;
        } else {
            shaderProgram->shaderVariantMap[variantKey] = RT64.lib.CreateShader(RT64.device, shaderProgram->cc, filter, hAddr, vAddr, flags);
        }
    }

    RT64_SHADER *shader = shaderProgram->shaderVariantMap[variantKey];
    RT64.lastShaderProgram = shaderProgram;
    RT64.lastShaderVariantKey = variantKey;
    RT64.lastShaderVariant = shader;
    return shader;
}

static void gfx_rt64_render_thread_apply_post_process_shader(void) {
    if (RT64.device == nullptr) { return; }

    {
        const std::lock_guard<std::mutex> lock(RT64.postProcessMutex);
        RT64.lib.SetPostProcessUniforms(RT64.device,
            RT64.postProcessUniformBlocks.empty() ? nullptr : RT64.postProcessUniformBlocks.data(),
            (unsigned int)(RT64.postProcessUniformBlocks.size()));
    }

    if (!RT64.postProcessDirty.load(std::memory_order_acquire)) { return; }

    const std::lock_guard<std::mutex> lock(RT64.postProcessMutex);
    if (RT64.postProcessHLSL.empty()) {
        RT64.lib.SetPostProcessShader(RT64.device, nullptr, nullptr, 0, nullptr, 0, 0);
    }
    else {
        RT64.lib.SetPostProcessShader(RT64.device, RT64.postProcessHLSL.c_str(),
            RT64.postProcessInputs.data(), (unsigned int)(RT64.postProcessInputs.size()),
            RT64.postProcessOutputName.c_str(), RT64.postProcessWidth, RT64.postProcessHeight);
    }

    RT64.postProcessDirty.store(false, std::memory_order_release);
}

void gfx_rt64_destroy_gpu_mesh(GPUMesh &mesh) {
    if (mesh.mesh != nullptr) {
        RT64.lib.DestroyMesh(mesh.mesh);
        mesh.mesh = nullptr;
        RT64.meshesDestroyed++;
    }
}

static RT64_TEXTURE *gfx_rt64_render_thread_find_texture(u32 textureKey) {
    if (textureKey == 0) {
        return RT64.blankTexture;
    }

    auto texIt = RT64.gpuTextures.find(textureKey);
    return ((texIt != RT64.gpuTextures.end()) && (texIt->second.texture != nullptr)) ? texIt->second.texture : RT64.blankTexture;
}

static inline void gfx_rt64_render_thread_add_light(const RT64_LIGHT &srcLight, const RT64_MATRIX4 &transform) {
    if (RT64.renderLightCount >= RT64_MAX_LIGHTS) {
        return;
    }

    auto &dstLight = RT64.renderLights[RT64.renderLightCount++];
    dstLight = srcLight;
    dstLight.position = transform_position_affine(transform, srcLight.position);

    RT64_VECTOR3 scaleVector = transform_direction_affine(transform, { 1.0f, 1.0f, 1.0f });
    float scale = vector_length(scaleVector) / sqrtf(3.0f);
    dstLight.attenuationRadius *= scale;
    dstLight.pointRadius *= scale;
    dstLight.shadowOffset *= scale;

    if (srcLight.lightType == RT64_LIGHT_TYPE_POINT) {
        RT64_VECTOR3 localForward, localRight, localUp;
        gfx_rt64_point_light_basis(srcLight.pitch, srcLight.yaw, srcLight.roll, &localForward, &localRight, &localUp);

        RT64_VECTOR3 worldForward = normalize_vector(transform_direction_affine(transform, localForward));
        RT64_VECTOR3 worldRight = normalize_vector(transform_direction_affine(transform, localRight));
        gfx_rt64_point_light_angles(worldForward, worldRight, &dstLight.pitch, &dstLight.yaw, &dstLight.roll);

        if (srcLight.apertureEnabled) {
            RT64_VECTOR3 localApertureNormal, unusedApertureRight, unusedApertureUp;
            gfx_rt64_point_light_basis(srcLight.aperturePitch, srcLight.apertureYaw, 0.0f, &localApertureNormal, &unusedApertureRight, &unusedApertureUp);

            RT64_VECTOR3 worldApertureNormal = normalize_vector(transform_direction_affine(transform, localApertureNormal));
            float unusedApertureRoll;
            gfx_rt64_point_light_angles(worldApertureNormal, worldRight, &dstLight.aperturePitch, &dstLight.apertureYaw, &unusedApertureRoll);
        }

        dstLight.scaleX *= scale;
        dstLight.scaleY *= scale;
    }
}

static const GPUMesh sEmptyGPUMesh = {};

static void gfx_rt64_render_thread_upload_mesh(GPUMesh &dstMesh, const GameMesh &curMesh) {
    if (dstMesh.vertexBufferHash == curMesh.vertexBufferHash) {
        return;
    }

    if ((dstMesh.positionHash != 0) && (dstMesh.positionHash == curMesh.positionHash)) {
        RT64.lib.SetMeshVertexData(dstMesh.mesh, curMesh.vertexBuffer, curMesh.vertexCount, curMesh.vertexStride, RT64.indexTriangleList, curMesh.indexCount);
    }
    else {
        RT64.lib.SetMesh(dstMesh.mesh, curMesh.vertexBuffer, curMesh.vertexCount, curMesh.vertexStride, RT64.indexTriangleList, curMesh.indexCount);
    }

    dstMesh.vertexCount = curMesh.vertexCount;
    dstMesh.vertexStride = curMesh.vertexStride;
    dstMesh.indexCount = curMesh.indexCount;
    dstMesh.vertexBufferHash = curMesh.vertexBufferHash;
    dstMesh.positionHash = curMesh.positionHash;
}

template <typename Pool>
static void gfx_rt64_render_thread_evict_meshes(Pool &pool, int evictFrames) {
    auto poolIt = pool.begin();
    while (poolIt != pool.end()) {
        GPUMesh &pooledMesh = poolIt->second;
        if (pooledMesh.inUse) {
            pooledMesh.inUse = false;
            pooledMesh.unusedFrames = 0;
            poolIt++;
        }
        else if (++pooledMesh.unusedFrames >= evictFrames) {
            gfx_rt64_destroy_gpu_mesh(pooledMesh);
            poolIt = pool.erase(poolIt);
        }
        else {
            poolIt++;
        }
    }
}

static inline void gfx_rt64_render_thread_draw_display_list(u32 uid, GameFrame *curFrame) {
    auto &gpuDl = RT64.gpuDisplayLists[uid];
    const auto &curDisplayList = curFrame->displayLists.find(uid)->second;

    for (int i = 0; i < curDisplayList.drawCount; i++) {
        auto &dstInstance = gpuDl.instances[i];
        const auto &curMesh = curDisplayList.meshes[i];
        const auto &curInstance = curDisplayList.instances[i];

        RT64_MESH *usedMesh = nullptr;
        if (curMesh.raytrace) {
            auto staticIt = RT64.gpuStaticMeshes.find(curMesh.vertexBufferHash);
            const GPUMesh &staticMesh = (staticIt != RT64.gpuStaticMeshes.end()) ? staticIt->second : sEmptyGPUMesh;
            if (staticMesh.mesh != nullptr) {
                usedMesh = staticMesh.mesh;
                RT64.staticMeshesDrawn++;
            }
        }

        if (usedMesh == nullptr) {
            auto &dynamicMeshPool = curMesh.raytrace ? RT64.gpuDynamicRtMeshes : RT64.gpuDynamicRasterMeshes;
            u64 sizeKey = (u64)(curMesh.vertexCount);
            sizeKey = sizeKey * 0x9E3779B185EBCA87ull + curMesh.vertexStride;
            sizeKey = sizeKey * 0x9E3779B185EBCA87ull + curMesh.indexCount;

            const u64 ownerKey = ((u64)(uid) << 32) | (u32)(i);

            GPUMesh *exactMesh = nullptr;
            GPUMesh *sameOwnerMesh = nullptr;
            GPUMesh *anyFreeMesh = nullptr;
            auto poolRange = dynamicMeshPool.equal_range(sizeKey);
            for (auto poolIt = poolRange.first; poolIt != poolRange.second; ++poolIt) {
                GPUMesh &candidate = poolIt->second;
                if (candidate.inUse || (candidate.raytrace != curMesh.raytrace)) {
                    continue;
                }
                if (candidate.vertexBufferHash == curMesh.vertexBufferHash) {
                    exactMesh = &candidate;
                    break;
                }
                if ((sameOwnerMesh == nullptr) && (candidate.ownerKey == ownerKey)) {
                    sameOwnerMesh = &candidate;
                }
                if (anyFreeMesh == nullptr) {
                    anyFreeMesh = &candidate;
                }
            }

            GPUMesh *dynamicMesh = exactMesh ? exactMesh : (sameOwnerMesh ? sameOwnerMesh : anyFreeMesh);
            if (dynamicMesh == nullptr) {
                auto inserted = dynamicMeshPool.emplace(sizeKey, GPUMesh{});
                dynamicMesh = &inserted->second;
                dynamicMesh->mesh = RT64.lib.CreateMesh(RT64.device, curMesh.raytrace ? (RT64_MESH_RAYTRACE_ENABLED | RT64_MESH_RAYTRACE_UPDATABLE) : 0);
                dynamicMesh->vertexCount = curMesh.vertexCount;
                dynamicMesh->vertexStride = curMesh.vertexStride;
                dynamicMesh->indexCount = curMesh.indexCount;
                dynamicMesh->raytrace = curMesh.raytrace;
                RT64.meshesCreated++;
            }

            gfx_rt64_render_thread_upload_mesh(*dynamicMesh, curMesh);

            dynamicMesh->ownerKey = ownerKey;
            dynamicMesh->inUse = true;
            usedMesh = dynamicMesh->mesh;

            RT64.dynamicMeshesDrawn++;
        }

        assert(usedMesh != nullptr);

        // Create the instance if it doesn't exist yet.
        if (dstInstance.instance == nullptr) {
            dstInstance.instance = RT64.lib.CreateInstance(RT64.scene);
            dstInstance.transform = curInstance.desc.transform;
        }

        // Update the instance.
        RT64_INSTANCE_DESC instDesc = curInstance.desc;
        instDesc.diffuseTexture = gfx_rt64_render_thread_find_texture(curInstance.textures.diffuse);
        instDesc.normalTexture = gfx_rt64_render_thread_find_texture(curInstance.textures.normal);
        instDesc.specularTexture = gfx_rt64_render_thread_find_texture(curInstance.textures.specular);
        instDesc.diffuse2Texture = gfx_rt64_render_thread_find_texture(curInstance.textures.diffuse2);
        instDesc.bumpTexture = gfx_rt64_render_thread_find_texture(curInstance.textures.bump);
        instDesc.mesh = usedMesh;

        instDesc.material.lockMask = 0.0f;

        // Assign the shader to the instance. Create if necessary.
        const auto &shader = curInstance.shader;
        instDesc.shader = gfx_rt64_render_thread_load_shader_variant(shader.program, shader.raytrace, shader.filter, shader.hAddr, shader.vAddr, shader.normalMap, shader.specularMap, shader.bumpMap);

        instDesc.shaderUniformBlocks = curInstance.uniformBlocks.empty() ? nullptr : curInstance.uniformBlocks.data();
        instDesc.shaderUniformBlockCount = (unsigned int)(curInstance.uniformBlocks.size());

        const float minDot = sqrt(2.0f) / -2.0f;
        instDesc.transform = curInstance.desc.transform;
        if (gfx_rt64_skip_matrix_lerp(dstInstance.transform, curInstance.desc.transform, minDot)) {
            instDesc.previousTransform = curInstance.desc.transform;
        }
        else {
            instDesc.previousTransform = dstInstance.transform;
        }

        // Update the instance.
        RT64.lib.SetInstanceDescription(dstInstance.instance, &instDesc);
        dstInstance.transform = instDesc.transform;

        // Apply the display list instance light (if applicable).
        if (curInstance.light.groupBits > 0) {
            gfx_rt64_render_thread_add_light(curInstance.light, instDesc.transform);
        }

        gpuDl.drawCount++;
    }

    // Apply the display list light (if applicable).
    if (curDisplayList.light.groupBits > 0) {
        gfx_rt64_render_thread_add_light(curDisplayList.light, curDisplayList.transform);
    }
}

static void gfx_rt64_render_thread_draw_frame(GameFrame *curFrame, GameFrame *prevFrame, float curFrameWeight, float deltaTimeMs) {
    LARGE_INTEGER elapsedMicro;

    RT64.staticMeshesDrawn = 0;
    RT64.dynamicMeshesDrawn = 0;
    RT64.meshesCreated = 0;
    RT64.meshesDestroyed = 0;

    // Copy the frame's static lights.
    memcpy(RT64.renderLights, curFrame->areaLights, sizeof(RT64_LIGHT) * curFrame->areaLightCount);
    RT64.renderLightCount = curFrame->areaLightCount;

    // Reset the draw counter for all active display lists.
    LARGE_INTEGER dlStart = gfx_rt64_profile_marker();
    for (auto &gpuDlPair : RT64.gpuDisplayLists) { gpuDlPair.second.drawCount = 0; }

    // Queue up all display lists first.
    for (auto &dlPair : curFrame->displayLists) {
        gfx_rt64_render_thread_draw_display_list(dlPair.first, curFrame);
    }

    // Clean up any unused instances or meshes from the GPU display lists.
    auto gpuDlIt = RT64.gpuDisplayLists.begin();
    while (gpuDlIt != RT64.gpuDisplayLists.end()) {
        auto &dl = gpuDlIt->second;

        // Destroy all unused instances.
        while (dl.instances.size() > (size_t)(dl.drawCount)) {
            if (dl.instances.back().instance != nullptr) {
                RT64.lib.DestroyInstance(dl.instances.back().instance);
            }
            dl.instances.pop_back();
        }

        if (dl.drawCount > 0) {
            dl.idleFrames = 0;
        }
        else {
            dl.idleFrames++;
        }

        if (dl.idleFrames >= CACHED_MESH_EVICT_FRAMES) {
            gpuDlIt = RT64.gpuDisplayLists.erase(gpuDlIt);
        }
        else {
            gpuDlIt++;
        }
    }

    LARGE_INTEGER dlEnd = gfx_rt64_profile_marker();
    elapsedMicro = gfx_rt64_profile_delta(dlStart, dlEnd);
    double dlMs = elapsedMicro.QuadPart / 1000.0;

    // Interpolate and update the view.
    RT64_MATRIX4 viewMatrix;
    float fovRadians;
    bool interpolateView = curFrame->interpolateView;

    // Detect if camera interpolation should be skipped.
    // Attempts to fix sudden camera changes like the ones in BBH.
    if (interpolateView && gfx_rt64_skip_matrix_lerp(prevFrame->viewMatrix, curFrame->viewMatrix, 0.0f)) {
        interpolateView = false;
    }

    if (interpolateView) {
        viewMatrix = gfx_rt64_lerp_matrix(prevFrame->viewMatrix, curFrame->viewMatrix, curFrameWeight);
        fovRadians = gfx_rt64_lerp_float(prevFrame->fovRadians, curFrame->fovRadians, curFrameWeight);
    }
    else {
        viewMatrix = curFrame->viewMatrix;
        fovRadians = curFrame->fovRadians;
    }

    RT64.lib.SetViewPerspective(RT64.view, viewMatrix, fovRadians, curFrame->nearDist, curFrame->farDist, curFrame->interpolateView);

    for (unsigned int i = 0; i < RT64.renderLightCount; i++) {
        const float minAttenuationExponent = 0.001f;
        if (!(RT64.renderLights[i].attenuationExponent > minAttenuationExponent)) {
            RT64.renderLights[i].attenuationExponent = minAttenuationExponent;
        }
    }

    // Update the scene.
    RT64.lib.SetSceneLights(RT64.scene, RT64.renderLights, RT64.renderLightCount);
    RT64.lib.SetSceneDescription(RT64.scene, curFrame->sceneDesc);

    RT64_TEXTURE *skyTexture = nullptr;
    if (curFrame->skyTextureKey != 0) {
        auto skyTexIt = RT64.gpuTextures.find(curFrame->skyTextureKey);
        if (skyTexIt != RT64.gpuTextures.end()) {
            skyTexture = skyTexIt->second.texture;
        }
    }
    RT64.lib.SetViewSkyPlane(RT64.view, skyTexture);

    // Additional information.
    if ((RT64.renderInspector != nullptr) && RT64.renderInspectorActive) {
        char dlMsMessage[128];
        snprintf(dlMsMessage, sizeof(dlMsMessage), "RENDER DL: %.3f ms\n", dlMs);
        RT64.lib.PrintMessageInspector(RT64.renderInspector, dlMsMessage);

        char infoMessage[128];
        snprintf(infoMessage, sizeof(infoMessage), "ST %u DYN %u CRE %u DTY %u\n", RT64.staticMeshesDrawn, RT64.dynamicMeshesDrawn, RT64.meshesCreated, RT64.meshesDestroyed);
        RT64.lib.PrintMessageInspector(RT64.renderInspector, infoMessage);
    }

    // Draw everything and update the window.
    RT64.lib.DrawDevice(RT64.device, gfx_rt64_use_vsync() ? 1 : 0, deltaTimeMs);

    for (auto *pool : { &RT64.gpuDynamicRasterMeshes, &RT64.gpuDynamicRtMeshes }) {
        gfx_rt64_render_thread_evict_meshes(*pool, CACHED_MESH_EVICT_FRAMES);
    }
}

static void gfx_rt64_render_thread_preprocess_frames(GameFrame *curFrame, GameFrame *prevFrame) {
    // Right click allows to pick a texture for editing from the viewport.
    RT64_INSTANCE *pickSearchInstance = nullptr;
    bool pickSearchTexture = false;
    bool pickSearchGeoLayout = false;
    if (RT64.renderInspectorActive) {
        const std::lock_guard<std::mutex> pickLock(RT64.pickTextureMutex);
        if (RT64.pickTexture || RT64.pickGeoLayout) {
            RT64_INSTANCE *instance = RT64.lib.GetViewRaytracedInstanceAt(RT64.view, RT64.pickCursorX, RT64.pickCursorY);
            if (instance != nullptr) {
                pickSearchInstance = instance;
                pickSearchTexture = RT64.pickTexture;
                pickSearchGeoLayout = RT64.pickGeoLayout;
            }
            else {
                if (RT64.pickTexture) { RT64.pickTextureHash = 0; }
            }

            RT64.pickTexture = false;
            RT64.pickGeoLayout = false;
        }
    }

    int remainingStaticMeshesForCache = CACHED_MESH_MAX_PER_FRAME;
    for (auto &dlPair : curFrame->displayLists) {
        auto &gpuDl = RT64.gpuDisplayLists[dlPair.first];
        const auto &curDisplayList = dlPair.second;

        // Make the vectors large enough to fit all the instances and meshes.
        if (gpuDl.instances.size() < (size_t)(curDisplayList.drawCount)) {
            gpuDl.instances.resize(curDisplayList.drawCount);
        }

        // Search for the matching instance inside the DLs and find the hash corresponding to the diffuse texture.
        if (pickSearchInstance != nullptr) {
            for (int i = 0; (i < gpuDl.drawCount) && (i < (int)(gpuDl.instances.size())); i++) {
                if (gpuDl.instances[i].instance == pickSearchInstance) {
                    const std::lock_guard<std::mutex> pickLock(RT64.pickTextureMutex);

                    if (pickSearchTexture) {
                        RT64.pickTextureHash = curDisplayList.instances[i].textureHash;
                    }

                    if (pickSearchGeoLayout && (curDisplayList.instances[i].geoLayout != nullptr)) {
                        RT64.pickedGeoLayout = curDisplayList.instances[i].geoLayout;
                    }

                    pickSearchInstance = nullptr;
                    break;
                }
            }
        }

        for (int i = 0; i < curDisplayList.drawCount; i++) {
            const auto &curMesh = curDisplayList.meshes[i];
            if (!curMesh.raytrace) {
                continue;
            }

            RT64.curTickMeshHashes.insert(curMesh.vertexBufferHash);

            if (RT64.prevTickMeshHashes.find(curMesh.vertexBufferHash) == RT64.prevTickMeshHashes.end()) {
                continue;
            }

            auto &staticMesh = RT64.gpuStaticMeshes[curMesh.vertexBufferHash];

            staticMesh.inUse = true;
            if (staticMesh.mesh != nullptr) {
                continue;
            }

            if (staticMesh.staticFrames < CACHED_MESH_REQUIRED_FRAMES) {
                staticMesh.staticFrames++;
                continue;
            }

            if (remainingStaticMeshesForCache <= 0) {
                continue;
            }

            staticMesh.mesh = RT64.lib.CreateMesh(RT64.device, RT64_MESH_RAYTRACE_ENABLED | RT64_MESH_RAYTRACE_FAST_TRACE);
            staticMesh.vertexBufferHash = curMesh.vertexBufferHash;
            RT64.lib.SetMesh(staticMesh.mesh, curMesh.vertexBuffer, curMesh.vertexCount, curMesh.vertexStride, RT64.indexTriangleList, curMesh.indexCount);

            remainingStaticMeshesForCache--;
        }
    }

    RT64.prevTickMeshHashes.swap(RT64.curTickMeshHashes);
    RT64.curTickMeshHashes.clear();

    if (pickSearchInstance != nullptr) {
        const std::lock_guard<std::mutex> pickLock(RT64.pickTextureMutex);
        if (pickSearchTexture) { RT64.pickTextureHash = 0; }
    }

    gfx_rt64_render_thread_evict_meshes(RT64.gpuStaticMeshes, CACHED_MESH_EVICT_FRAMES);
}

static void gfx_rt64_render_thread_handle_messages(void) {
    std::queue<InspectorMessage> messages;
    {
        const std::lock_guard<std::mutex> lock(RT64.inspectorMessageQueueMutex);
        messages.swap(RT64.inspectorMessageQueue);
    }

    if (RT64.renderInspector == nullptr) {
        return;
    }

    while (!messages.empty()) {
        const InspectorMessage &m = messages.front();
        RT64.lib.HandleMessageInspector(RT64.renderInspector, m.message, (WPARAM)(m.wParam), (LPARAM)(m.lParam));
        messages.pop();
    }
}

static void gfx_rt64_render_thread_upload_texture_queue(void) {
    std::queue<UploadTexture> textureUploadQueue;
    {
        const std::lock_guard<std::mutex> lock(RT64.textureUploadQueueMutex);
        textureUploadQueue.swap(RT64.textureUploadQueue);
    }

    while (!textureUploadQueue.empty()) {
        UploadTexture uploadTexture = textureUploadQueue.front();
        textureUploadQueue.pop();

        auto &gpuTexture = RT64.gpuTextures[uploadTexture.key];
        gpuTexture.hash = uploadTexture.hash;

        auto existingIt = RT64.hashToTexture.find(uploadTexture.contentHash);
        if (existingIt != RT64.hashToTexture.end()) {
            gpuTexture.texture = existingIt->second;
        }
        else {
            gpuTexture.texture = RT64.lib.CreateTexture(RT64.device, uploadTexture.desc);
            RT64.hashToTexture[uploadTexture.contentHash] = gpuTexture.texture;
        }

        free(uploadTexture.desc.bytes);
    }
}

void gfx_rt64_render_thread(void) {
    LARGE_INTEGER frameStart, frameEnd, preprocessStart, preprocessEnd, elapsedMicro;

    // Setup scene and view.
    RT64.scene = RT64.lib.CreateScene(RT64.device);
    RT64.view = RT64.lib.CreateView(RT64.scene);

    // Draw at least one empty frame to fill the window.
    RT64.lib.DrawDevice(RT64.device, gfx_rt64_use_vsync() ? 1 : 0, 0.0f);

    // Preload a blank texture.
    const int blankTextureSize = 64;
    int blankBytesCount = blankTextureSize * blankTextureSize * 4;
    unsigned char *blankBytes = (unsigned char *)(malloc(blankBytesCount));
    if (blankBytes == nullptr) {
        gfx_rt64_error_message("RT64", "Failed to allocate the RT64 blank texture, ran out of memory.");
        abort();
    }
    memset(blankBytes, 0xFF, blankBytesCount);

    RT64_TEXTURE_DESC texDesc = {};
    texDesc.bytes = blankBytes;
    texDesc.byteCount = blankBytesCount;
    texDesc.format = RT64_TEXTURE_FORMAT_RGBA8;
    texDesc.width = blankTextureSize;
    texDesc.height = blankTextureSize;
    texDesc.rowPitch = texDesc.width * 4;
    RT64.blankTexture = RT64.lib.CreateTexture(RT64.device, texDesc);
    free(blankBytes);

    // Upload any pending textures that the game has already queued up.
    gfx_rt64_render_thread_upload_texture_queue();

    // Unpause the game once the render thread has finished loading.
    RT64.pauseMode = false;

    int curFrameIndex = -1;
    int prevFrameIndex = -1;
    double frameDeltaTimeMs = 0.0;
    double interFrameDeltaTimeMs = 1000.0 / 60.0;
    LARGE_INTEGER lastFrameStart = {};
    while (RT64.renderThreadRunning) {
        // Create or destroy the inspector depending on the current state of the flag.
        if (RT64.renderInspectorActive && (RT64.renderInspector == nullptr)) {
            RT64.renderInspector = RT64.lib.CreateInspector(RT64.device);

            if (RT64.lib.SetInspectorMaterialDefaults != nullptr) {
                RT64.lib.SetInspectorMaterialDefaults(RT64.renderInspector, &RT64.defaultMaterial);
            }
        }
        else if (!RT64.renderInspectorActive && (RT64.renderInspector != nullptr)) {
            RT64.lib.DestroyInspector(RT64.renderInspector);
            RT64.renderInspector = nullptr;
        }

        gfx_rt64_render_thread_handle_messages();

        // Update the view description if modified.
        {
            const std::lock_guard<std::mutex> lock(RT64.renderViewDescMutex);
            if (RT64.renderViewDescChanged) {
                RT64.lib.SetViewDescription(RT64.view, RT64.renderViewDesc);
                RT64.renderViewDescChanged = false;
            }
            else if (RT64.renderInspectorActive && (RT64.lib.GetViewDescription != nullptr)) {
                RT64.lib.GetViewDescription(RT64.view, &RT64.inspectorViewDesc);
                RT64.inspectorViewDescValid = true;
            }
        }

        {
            std::unique_lock<std::mutex> lock(RT64.renderFrameIndexMutex);
            RT64.renderFrameCV.wait(lock, [] { return !RT64.renderThreadRunning || (RT64.gpuFrameIndex >= 0); });
            curFrameIndex = RT64.gpuFrameIndex;
            prevFrameIndex = (curFrameIndex == 0) ? (MAX_RENDER_FRAMES - 1) : (curFrameIndex - 1);
            RT64.barrierFrameIndex = prevFrameIndex;
            RT64.gpuFrameIndex = -1;
        }
        RT64.renderFrameCV.notify_all();

        gfx_rt64_render_thread_upload_texture_queue();

        if (curFrameIndex >= 0) {
            // Run any necessary preprocessing.
            preprocessStart = gfx_rt64_profile_marker();
            gfx_rt64_render_thread_preprocess_frames(&RT64.frames[curFrameIndex], &RT64.frames[prevFrameIndex]);
            preprocessEnd = gfx_rt64_profile_marker();
            elapsedMicro = gfx_rt64_profile_delta(preprocessStart, preprocessEnd);
            double preprocessTimeMs = elapsedMicro.QuadPart / 1000.0;

            // Print to the inspector the previous time it took to draw a frame.
            if ((RT64.renderInspector != nullptr) && RT64.renderInspectorActive) {
                const std::lock_guard<std::mutex> lock(RT64.renderInspectorMutex);
                char preprocessTimeMsg[64], renderDeltaTimeMsg[64];
                snprintf(preprocessTimeMsg, sizeof(preprocessTimeMsg), "RENDER PREPROCESS: %.3f ms\n", preprocessTimeMs);
                snprintf(renderDeltaTimeMsg, sizeof(renderDeltaTimeMsg), "RENDER FRAME: %.3f ms\n", frameDeltaTimeMs);
                RT64.lib.PrintClearInspector(RT64.renderInspector);
                RT64.lib.PrintMessageInspector(RT64.renderInspector, preprocessTimeMsg);
                RT64.lib.PrintMessageInspector(RT64.renderInspector, renderDeltaTimeMsg);
                for (const std::string &message : RT64.renderInspectorMessages) {
                    RT64.lib.PrintMessageInspector(RT64.renderInspector, message.c_str());
                }
                const std::lock_guard<std::mutex> lightingLock(RT64.levelAreaLightingMutex);
                const std::lock_guard<std::mutex> pickLock(RT64.pickTextureMutex);
                int levelIndex = gfx_rt64_get_level_index();
                int areaIndex = gfx_rt64_get_area_index();

                AreaLighting &areaLighting = gfx_rt64_get_or_add_area_lighting(levelIndex, areaIndex);

                // Inspect the current scene.
                RT64.lib.SetSceneInspector(RT64.renderInspector, &areaLighting.sceneDesc);

                // Inspect the current level's lights.
                RT64.lib.SetLightsInspector(RT64.renderInspector, areaLighting.lights, &areaLighting.lightCount, MAX_LEVEL_LIGHTS);

                // Inspect the current picked material.
                if (RT64.pickTextureHash > 0) {
                    const std::lock_guard<std::mutex> texModsLock(RT64.texModsMutex);
                    auto texNameIt = RT64.texNameMap.find(RT64.pickTextureHash);

                    char solidName[32];
                    snprintf(solidName, sizeof(solidName), "solid_%016llx", (unsigned long long)(RT64.pickTextureHash));
                    const std::string textureName = (texNameIt != RT64.texNameMap.end()) ? texNameIt->second : std::string(solidName);

                    if (texNameIt == RT64.texNameMap.end()) {
                        RT64.texNameMap[RT64.pickTextureHash] = textureName;
                        RT64.nameTexMap[textureName] = RT64.pickTextureHash;
                    }
                    RecordedMod *texMod = RT64.texMods[RT64.pickTextureHash];
                    if (texMod == nullptr) {
                        texMod = new RecordedMod();
                        RT64.texMods[RT64.pickTextureHash] = texMod;
                    }

                    if (texMod->materialMod == nullptr) {
                        texMod->materialMod = new RT64_MATERIAL();
                        texMod->materialMod->enabledAttributes = RT64_ATTRIBUTE_NONE;
                    }

                    RT64.lib.SetMaterialInspector(RT64.renderInspector, texMod->materialMod, textureName.c_str());
                    gfx_rt64_sync_inspector_map_names(RT64_INSPECTOR_PANEL_MATERIAL, texMod);
                }

                if (RT64.lib.SetGeoLayoutInspector != nullptr) {
                    RT64.lib.SetGeoLayoutInspector(RT64.renderInspector, RT64.pickedGeoLayoutMaterial,
                        &RT64.pickedGeoLayoutLight, &RT64.pickedGeoLayoutLightEnabled,
                        RT64.pickedGeoLayoutOrigins, RT64.pickedGeoLayoutOriginCount,
                        RT64.pickedGeoLayoutName.c_str());

                    const std::lock_guard<std::mutex> texModsLock(RT64.texModsMutex);
                    gfx_rt64_sync_inspector_map_names(RT64_INSPECTOR_PANEL_GEO_LAYOUT, RT64.pickedGeoLayoutMod);
                }
            }

            // Draw the frame and measure the time right before and right after.
            frameStart = gfx_rt64_profile_marker();

            if (lastFrameStart.QuadPart != 0) {
                interFrameDeltaTimeMs = gfx_rt64_profile_delta(lastFrameStart, frameStart).QuadPart / 1000.0;
            }
            lastFrameStart = frameStart;

            gfx_rt64_render_thread_apply_post_process_shader();

            gfx_rt64_render_thread_draw_frame(&RT64.frames[curFrameIndex], &RT64.frames[prevFrameIndex], 1.0f, (float)(interFrameDeltaTimeMs));
            frameEnd = gfx_rt64_profile_marker();
            elapsedMicro = gfx_rt64_profile_delta(frameStart, frameEnd);
            frameDeltaTimeMs = elapsedMicro.QuadPart / 1000.0;

            // Clear the barrier.
            {
                const std::lock_guard<std::mutex> lock(RT64.renderFrameIndexMutex);
                RT64.barrierFrameIndex = -1;
            }
            RT64.renderFrameCV.notify_all();
        }
    }
}

void gfx_rt64_sync_inspector_map_names(int panel, RecordedMod *mod) {
    if ((mod == nullptr) || (RT64.lib.SetInspectorMapNames == nullptr) || (RT64.lib.GetInspectorMapNames == nullptr)) {
        return;
    }

    const int mapNameSize = 256;
    char bumpMapName[mapNameSize] = {};
    char normalMapName[mapNameSize] = {};
    char specularMapName[mapNameSize] = {};
    if (RT64.lib.GetInspectorMapNames(RT64.renderInspector, panel, bumpMapName, normalMapName, specularMapName, mapNameSize)) {
        mod->bumpMapHash = (bumpMapName[0] != '\0') ? gfx_rt64_get_texture_name_hash(bumpMapName) : 0;
        mod->normalMapHash = (normalMapName[0] != '\0') ? gfx_rt64_get_texture_name_hash(normalMapName) : 0;
        mod->specularMapHash = (specularMapName[0] != '\0') ? gfx_rt64_get_texture_name_hash(specularMapName) : 0;

        if (bumpMapName[0] != '\0') { gfx_rt64_register_map_texture(bumpMapName, nullptr); }
        if (normalMapName[0] != '\0') { gfx_rt64_register_map_texture(normalMapName, nullptr); }
        if (specularMapName[0] != '\0') { gfx_rt64_register_map_texture(specularMapName, nullptr); }

        if (panel == RT64_INSPECTOR_PANEL_GEO_LAYOUT) {
            gfx_rt64_invalidate_graph_node_mods();
        }

        return;
    }

    const std::string bumpName = gfx_rt64_texture_mod_name(mod->bumpMapHash);
    const std::string normName = gfx_rt64_texture_mod_name(mod->normalMapHash);
    const std::string specName = gfx_rt64_texture_mod_name(mod->specularMapHash);
    RT64.lib.SetInspectorMapNames(RT64.renderInspector, panel, bumpName.c_str(), normName.c_str(), specName.c_str());
}

void gfx_rt64_rapi_toggle_inspector(void) {
#if !RT64_INSPECTOR_ENABLED
    return;
#else
    if (!gfx_gfx_rt64_is_active()) {
        return;
    }
    RT64.renderInspectorActive = !RT64.renderInspectorActive;
#endif
}

bool gfx_rt64_rapi_inspector_active(void) {
    return gfx_gfx_rt64_is_active() && RT64.renderInspectorActive;
}

static void gfx_rt64_request_pick(bool *pick) {
    POINT cursorPos = {};
    GetCursorPos(&cursorPos);
    ScreenToClient(RT64.hwnd, &cursorPos);
    RT64.pickCursorX = cursorPos.x;
    RT64.pickCursorY = cursorPos.y;
    *pick = true;
}

bool gfx_rt64_rapi_handle_window_message(void *hWnd, unsigned int message, uintptr_t wParam, intptr_t lParam) {
    (void)(hWnd);
    if (!gfx_gfx_rt64_is_active() || !RT64.renderInspectorActive) {
        return false;
    }

    switch (message) {
        case WM_LBUTTONDOWN: {
            if ((RT64.lib.InspectorWantsMouse == nullptr) || RT64.lib.InspectorWantsMouse(RT64.renderInspector)) {
                break;
            }

            {
                const std::lock_guard<std::mutex> pickLock(RT64.pickTextureMutex);
                gfx_rt64_request_pick(&RT64.pickGeoLayout);
            }

            RT64.pickGeoLayoutHighlight = true;

            break;
        }
        case WM_LBUTTONUP: {
            RT64.pickGeoLayoutHighlight = false;
            break;
        }
        case WM_RBUTTONDOWN: {
            {
                const std::lock_guard<std::mutex> pickLock(RT64.pickTextureMutex);
                RT64.pickTextureHash = 0;
                gfx_rt64_request_pick(&RT64.pickTexture);
            }

            RT64.pickTextureHighlight = true;

            return true;
        }
        case WM_RBUTTONUP: {
            const std::lock_guard<std::mutex> pickLock(RT64.pickTextureMutex);
            RT64.pickTextureHighlight = false;
            return true;
        }
        default:
            break;
    }

    const bool isInputMessage =
        (message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
        (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) ||
        (message == WM_SETFOCUS) || (message == WM_KILLFOCUS) || (message == WM_SETCURSOR);
    if (!isInputMessage) {
        return false;
    }

    const size_t maxQueuedMessages = 256;
    const std::lock_guard<std::mutex> lock(RT64.inspectorMessageQueueMutex);
    if (RT64.inspectorMessageQueue.size() < maxQueuedMessages) {
        RT64.inspectorMessageQueue.push({ message, wParam, lParam });
    }

    return false;
}

unsigned int gfx_rt64_clamp_percent(long long percent, unsigned int minPercent, unsigned int maxPercent) {
    if (percent < minPercent) { return minPercent; }
    if (percent > maxPercent) { return maxPercent; }
    return (unsigned int)(percent);
}

void gfx_rt64_adopt_inspector_view_desc(const RT64_VIEW_DESC &desc) {
    bool changed = false;
    auto adoptUInt = [&](unsigned int &configValue, unsigned int newValue) {
        if (configValue != newValue) { configValue = newValue; changed = true; }
    };
    auto adoptBool = [&](bool &configValue, bool newValue) {
        if (configValue != newValue) { configValue = newValue; changed = true; }
    };

    adoptUInt(configRT64ResScale, gfx_rt64_clamp_percent(lround(desc.resolutionScale * 100.0f), sMinResolutionScale, sMaxResolutionScale));
    adoptUInt(configRT64MaxLights, desc.maxLights);
    adoptUInt(configRT64MaxReflections, desc.maxReflections);
    adoptUInt(configRT64MotionBlurStrength, (unsigned int)(lround(desc.motionBlurStrength * 100.0f)));
    adoptUInt(configRT64UpscalerSharpness, (unsigned int)(lround(desc.upscalerSharpness * 100.0f)));
    adoptUInt(configRT64Upscaler, desc.upscaler);
    adoptUInt(configRT64UpscalerMode, desc.upscalerMode);
    adoptBool(configRT64SphereLights, desc.diSamples > 0);
    adoptBool(configRT64GI, desc.giSamples > 0);
    adoptBool(configRT64Denoiser, desc.denoiserEnabled);

    static bool sPendingSave = false;
    static LARGE_INTEGER sLastSaveTime = {};
    if (changed) { sPendingSave = true; }
    if (!sPendingSave) { return; }

    const LONGLONG minSaveIntervalMicros = 1000000;
    LARGE_INTEGER now = gfx_rt64_profile_marker();
    if ((sLastSaveTime.QuadPart != 0) && (gfx_rt64_profile_delta(sLastSaveTime, now).QuadPart < minSaveIntervalMicros)) {
        return;
    }

    sLastSaveTime = now;
    sPendingSave = false;
    configfile_save(configfile_name());
}

void gfx_rt64_publish_picked_geo_layout(void) {
    if (!RT64.renderInspectorActive) {
        return;
    }

    void *pickedGeoLayout = nullptr;
    {
        const std::lock_guard<std::mutex> pickLock(RT64.pickTextureMutex);
        pickedGeoLayout = RT64.pickedGeoLayout;
    }

    RT64_MATERIAL *material = nullptr;
    RecordedMod *pickedMod = nullptr;
    std::string geoName;
    if (pickedGeoLayout != nullptr) {
        auto geoNameIt = RT64.geoLayoutNameMap.find(pickedGeoLayout);
        if (geoNameIt != RT64.geoLayoutNameMap.end()) {
            geoName = geoNameIt->second;

            RecordedMod *&geoMod = RT64.geoLayoutMods[pickedGeoLayout];
            if (geoMod == nullptr) {
                geoMod = new RecordedMod();

                RT64.nameGeoLayoutMap[geoName] = pickedGeoLayout;
            }

            if (geoMod->materialMod == nullptr) {
                geoMod->materialMod = new RT64_MATERIAL();
                geoMod->materialMod->enabledAttributes = RT64_ATTRIBUTE_NONE;
            }

            material = geoMod->materialMod;
            pickedMod = geoMod;

            if (RT64.publishedGeoLayout != pickedGeoLayout) {
                RT64.publishedGeoLayout = pickedGeoLayout;
                RT64.pickedGeoLayoutLightEnabled = (geoMod->lightMod != nullptr);
                if (geoMod->lightMod != nullptr) {
                    RT64.pickedGeoLayoutLight = *geoMod->lightMod;
                }
                else {
                    memset(&RT64.pickedGeoLayoutLight, 0, sizeof(RT64_LIGHT));
                    RT64.pickedGeoLayoutLight.diffuseColor = { 255.0f, 255.0f, 255.0f };
                    RT64.pickedGeoLayoutLight.specularColor = { 255.0f, 255.0f, 255.0f };
                    RT64.pickedGeoLayoutLight.intensity = 1.0f;
                    RT64.pickedGeoLayoutLight.attenuationRadius = 1000.0f;
                    RT64.pickedGeoLayoutLight.pointRadius = 25.0f;
                    RT64.pickedGeoLayoutLight.attenuationExponent = 1.0f;
                    RT64.pickedGeoLayoutLight.groupBits = RT64_LIGHT_GROUP_DEFAULT;
                }
            }

            if (RT64.pickedGeoLayoutLightEnabled) {
                if (geoMod->lightMod == nullptr) {
                    geoMod->lightMod = new RT64_LIGHT();
                }

                *geoMod->lightMod = RT64.pickedGeoLayoutLight;
            }
            else {
                delete geoMod->lightMod;
                geoMod->lightMod = nullptr;
            }
        }
    }

    if (material == nullptr) {
        RT64.publishedGeoLayout = nullptr;
    }

    const std::lock_guard<std::mutex> pickLock(RT64.pickTextureMutex);
    RT64.pickedGeoLayoutMaterial = material;
    RT64.pickedGeoLayoutMod = pickedMod;
    RT64.pickedGeoLayoutName = geoName;
}

#endif // _WIN32
