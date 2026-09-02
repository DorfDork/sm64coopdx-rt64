#include <SDL3/SDL.h>

#include "types.h"

#include "gfx_sdl_gpu.h"

#include "pc/pc_main.h"
#include "pc/debuglog.h"
#include "pc/lua/smlua.h"
#include "pc/mods/mods_utils.h"

#include "gfx_window_manager.h"
#include "gfx_rendering_api.h"
#include "gfx_pc.h"
#include "gfx_shader.h"

#define MAX_FRAMES_IN_FLIGHT 3
#define MAX_STORAGE_BUFFER_SIZE 32 * 1024 * 1024

static SDL_GPUShaderFormat sShaderFormats = (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL);
static SDL_GPUDevice *sGpuDevice = NULL;
static SDL_Window *sSdlWindow = NULL;
static SDL_GPUCommandBuffer *sCmdBuffer = NULL;
static SDL_GPURenderPass *sRenderPass = NULL;
static SDL_GPUTexture *sDepthTexture = NULL;
static SDL_GPUTexture *sSwapchainTex = NULL;
static SDL_GPUTextureFormat sSwapchainFormat = 0;

struct GpuRingBuffer {
    SDL_GPUBuffer *gpuBuffer;
    SDL_GPUTransferBuffer *transferBuffer;
    u8 *mappedData;
    u32 currentOffset;
    u32 size;
};

static struct GpuRingBuffer sVertexRingBuffer = { 0 };

struct TextureData {
    SDL_GPUTexture *texture;
    SDL_GPUSampler *sampler;
    u32 width;
    u32 height;
    bool linearFilter;
    u32 cms;
    u32 cmt;
};

struct ShaderProgramSdlGpu {
    SDL_GPUGraphicsPipeline *pipeline;

    SDL_GPUShader *sdlVertexShader;
    SDL_GPUShader *sdlFragmentShader;

    struct Shader *vertexShader;
    struct Shader *fragmentShader;

    u64 hash;

    u8 numInputs;
    u8 numFloats;

    bool usedTextures[MAX_TEXTURES];
    bool usedLightmap;
    bool usedFog;
    bool worldGeometry;
};

static u32 sRenderWidth = 0;
static u32 sRenderHeight = 0;

struct ShaderProgramSdlGpu sShaderProgramPool[MAX_FRAME_PASSES][CC_MAX_SHADERS];
u8 sShaderProgramPoolSize[MAX_FRAME_PASSES] = { 0 };
u8 sShaderProgramPoolIndex[MAX_FRAME_PASSES] = { 0 };

struct ShaderProgramSdlGpu sPostProcessShaderProgramPool[MAX_FRAME_PASSES];

struct ShaderProgramSdlGpu *sShaderProgram = NULL;

static struct TextureData *sTextures = NULL;
static u32 sTexturesCapacity = 0;
static u32 sTexturesCount = 0;

static s32 sCurrentTile = 0;
static u32 sCurrentTextureIds[MAX_TEXTURES] = { 0 };
static u32 sLastTextureIds[MAX_TEXTURES] = { 0 };

static SDL_GPUSampler *sLinearClampSampler = NULL;
static SDL_GPUSampler *sNearestClampSampler = NULL;

static bool sDepthTest = false;
static bool sDepthMask = false;
static bool sZModeDecal = false;

static bool sLastDepthTest = false;
static bool sLastDepthMask = false;
static bool sLastZModeDecal = false;

static bool sStartedFrame = false;
static u32 sCurrentFrame = 0;

// helper functions
static void gfx_sdl_gpu_create_ring_buffer(struct GpuRingBuffer *ringBuffer, u32 size, SDL_GPUBufferUsageFlags usage) {
    ringBuffer->size = size;
    ringBuffer->currentOffset = 0;

    // create gpu buffer
    SDL_GPUBufferCreateInfo bufferInfo = {
        .usage = usage,
        .size = size
    };
    ringBuffer->gpuBuffer = SDL_CreateGPUBuffer(sGpuDevice, &bufferInfo);

    // create transfer buffer for the gpu buffer
    SDL_GPUTransferBufferCreateInfo transferInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = size
    };
    ringBuffer->transferBuffer = SDL_CreateGPUTransferBuffer(sGpuDevice, &transferInfo);

    if (!ringBuffer->gpuBuffer || !ringBuffer->transferBuffer) {
        sys_fatal("Failed to allocate ring buffer!");
    }

    // map transfer buffer to gpu device
    ringBuffer->mappedData = (u8 *)SDL_MapGPUTransferBuffer(sGpuDevice, ringBuffer->transferBuffer, false);
    if (!ringBuffer->mappedData) {
        sys_fatal("Failed to map ring buffer!");
    }
}

u32 gfx_sdl_gpu_allocate_to_ring_buffer(struct GpuRingBuffer *ringBuffer, u32 bytesNeeded) {
    // align to 16 bytes
    u32 alignedBytes = (bytesNeeded + 16 - 1) & ~(16 - 1);

    if (ringBuffer->currentOffset + alignedBytes > ringBuffer->size) {
        ringBuffer->currentOffset = 0;
    }

    u32 allocatedOffset = ringBuffer->currentOffset;
    ringBuffer->currentOffset += alignedBytes;

    return allocatedOffset;
}

static void gfx_sdl_gpu_create_depth_texture(void) {
    if (sDepthTexture != NULL) {
        SDL_ReleaseGPUTexture(sGpuDevice, sDepthTexture);
        sDepthTexture = NULL;
    }

    SDL_GPUTextureCreateInfo depthDesc = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width = sRenderWidth,
        .height = sRenderHeight,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };

    sDepthTexture = SDL_CreateGPUTexture(sGpuDevice, &depthDesc);
    if (!sDepthTexture) {
        sys_fatal("Failed to recreate swapchain depth texture on resize: %s", SDL_GetError());
    }
}

static void gfx_sdl_gpu_end_render_pass(void) {
    if (sRenderPass != NULL) {
        SDL_EndGPURenderPass(sRenderPass);
        sRenderPass = NULL;
    }
}

static void gfx_sdl_gpu_setup_command_buffer(void) {
    if (sCmdBuffer != NULL) { return; }

    sCmdBuffer = SDL_AcquireGPUCommandBuffer(sGpuDevice);
    if (sCmdBuffer == NULL) {
        sys_fatal("Failed to acquire GPU command buffer: %s", SDL_GetError());
    }
}

static SDL_GPUShader *gfx_sdl_gpu_create_shader_from_spirv(SDL_GPUShaderStage stage, SpirVShader *spirvShader, u32 numSamplers, u32 numUniformBuffers) {
    SDL_GPUShaderCreateInfo createInfo = {
        .code_size = spirvShader->size,
        .code = (const u8 *)spirvShader->words,
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = stage,
        .num_samplers = numSamplers,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = numUniformBuffers
    };
    return SDL_CreateGPUShader(sGpuDevice, &createInfo);
}

static SDL_GPUShader *gfx_sdl_gpu_create_shader_from_msl(SDL_GPUShaderStage stage, char *mslCode, u32 numSamplers, u32 numUniformBuffers) {
    SDL_GPUShaderCreateInfo createInfo = {
        .code_size = strlen(mslCode),
        .code = (const u8 *)mslCode,
        .entrypoint = "main0",
        .format = SDL_GPU_SHADERFORMAT_MSL,
        .stage = stage,
        .num_samplers = numSamplers,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = numUniformBuffers
    };
    return SDL_CreateGPUShader(sGpuDevice, &createInfo);
}

static SDL_GPUShader *gfx_sdl_gpu_create_shader(struct Shader *shader) {
    SDL_GPUShaderStage stage = (shader->stage == SHADER_STAGE_VERTEX ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT);

#ifdef __APPLE__
    char *mslCode = NULL;
    gfx_convert_spirv_to_msl(&mslCode, shader);

    SDL_GPUShader *sdlShader = gfx_sdl_gpu_create_shader_from_msl(stage, mslCode, shader->samplerCount, shader->uniformBlockCount);

    free(mslCode);
    return sdlShader;
#else

#endif
}

static void gfx_sdl_gpu_reset_state() {
    sLastDepthTest = -1;
    sLastDepthMask = -1;
    sLastZModeDecal = -1;
    memset(sLastTextureIds, 0, sizeof(sLastTextureIds));
}

static void gfx_sdl_gpu_resume_render_pass(void) {
    if (!gDefaultGeoFramePass.active) {
        gfx_sdl_gpu_api.set_framebuffer(gfx_get_current_frame_pass());
    } else {
        gfx_sdl_gpu_api.reset_framebuffer();
    }
}

static SDL_GPUSamplerAddressMode gfx_cm_to_sdl_gpu(u32 cm) {
    if (cm & G_TX_CLAMP) {
        return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    }
    return (cm & G_TX_MIRROR) ? SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
}

static bool gfx_sdl_gpu_z_is_from_0_to_1(void) {
    return true;
}

static void gfx_sdl_gpu_unload_shader(UNUSED struct ShaderProgram *oldPrg) {
}

static void gfx_sdl_gpu_load_shader(struct ShaderProgram *newPrg) {
    sShaderProgram = (struct ShaderProgramSdlGpu *)newPrg;
}

void gfx_sdl_gpu_remove_shaders(void) {
    for (int i = 0; i < MAX_FRAME_PASSES; i++) {
        for (int j = 0; j < CC_MAX_SHADERS; j++) {
            gfx_destroy_shader(sShaderProgramPool[i][j].vertexShader);
            gfx_destroy_shader(sShaderProgramPool[i][j].fragmentShader);
            if (sShaderProgramPool[i][j].pipeline) {
                SDL_ReleaseGPUGraphicsPipeline(sGpuDevice, sShaderProgramPool[i][j].pipeline);
                sShaderProgramPool[i][j].pipeline = NULL;
            }
            memset(&sShaderProgramPool[i][j], 0, sizeof(sShaderProgramPool[i][j]));
        }
        sShaderProgramPoolIndex[i] = 0;
        sShaderProgramPoolSize[i] = 0;

        gfx_destroy_shader(sPostProcessShaderProgramPool[i].vertexShader);
        gfx_destroy_shader(sPostProcessShaderProgramPool[i].fragmentShader);
        if (sPostProcessShaderProgramPool[i].pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(sGpuDevice, sPostProcessShaderProgramPool[i].pipeline);
            sPostProcessShaderProgramPool[i].pipeline = NULL;
        }
        memset(&sPostProcessShaderProgramPool[i], 0, sizeof(sPostProcessShaderProgramPool[i]));
    }

    sShaderProgram = NULL;
}

static struct ShaderProgram *gfx_sdl_gpu_create_and_load_new_shader(struct ColorCombiner *cc) {
    struct CCFeatures ccFeatures = { 0 };
    gfx_cc_get_features(cc, &ccFeatures);

    struct Shader *vertexShader = (struct Shader *)calloc(1, sizeof(struct Shader));
    struct Shader *fragmentShader = (struct Shader *)calloc(1, sizeof(struct Shader));
    if (!vertexShader || !fragmentShader) {
        sys_fatal("Failed to allocate shaders, ran out of memory!");
    }

    gfx_generate_vertex_and_fragment_shader_from_cc(vertexShader, fragmentShader, cc, NULL, NULL);

    SDL_GPUShader *sdlVs = gfx_sdl_gpu_create_shader(vertexShader);
    SDL_GPUShader *sdlFs = gfx_sdl_gpu_create_shader(fragmentShader);

    if (!sdlVs || !sdlFs) {
        sys_fatal("Failed to create SDL GPU Shaders: %s", SDL_GetError());
    }

    s32 framePassIndex = gCurrentFramePassIndex + 1;
    u8 poolIndex = sShaderProgramPoolIndex[framePassIndex];

    struct ShaderProgramSdlGpu *prg = &sShaderProgramPool[framePassIndex][poolIndex];

    sShaderProgramPoolIndex[framePassIndex] = (poolIndex + 1) % CC_MAX_SHADERS;
    if (sShaderProgramPoolSize[framePassIndex] < CC_MAX_SHADERS) {
        sShaderProgramPoolSize[framePassIndex]++;
    }

    SDL_GPUVertexAttribute vertexAttributes[MAX_SHADER_INPUTS];
    u8 attributeCount = 0;
    u32 currentOffset = 0;

    for (s32 i = 0; i < MAX_SHADER_INPUTS; i++) {
        if (gShaderInputs[i].size == 0) { continue; }

        s32 loc = vertexShader->shaderInputs[i].location;
        SDL_GPUVertexElementFormat format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;

        switch (vertexShader->shaderInputs[i].size) {
            case 1: format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT; break;
            case 2: format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2; break;
            case 3: format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; break;
            case 4: format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4; break;
        }

        vertexAttributes[attributeCount].location = loc;
        vertexAttributes[attributeCount].buffer_slot = 0;
        vertexAttributes[attributeCount].format = format;
        vertexAttributes[attributeCount].offset = currentOffset;

        currentOffset += vertexShader->shaderInputs[i].size * sizeof(float);
        attributeCount++;
    }

    SDL_GPUVertexBufferDescription vertexBufferDesc = {
        .slot = 0,
        .pitch = currentOffset,
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0
    };

    SDL_GPUColorTargetDescription colorTargetDesc = {
        .format = sSwapchainFormat, // TODO: may need to undo to use SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
        .blend_state = {
            .enable_blend = cc->cm.use_alpha,
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A
        }
    };

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {
        .vertex_shader = sdlVs,
        .fragment_shader = sdlFs,
        .vertex_input_state = {
            .vertex_buffer_descriptions = &vertexBufferDesc,
            .num_vertex_buffers = (attributeCount > 0) ? 1 : 0,
            .vertex_attributes = vertexAttributes,
            .num_vertex_attributes = attributeCount
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .target_info = {
            .color_target_descriptions = &colorTargetDesc,
            .num_color_targets = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            .has_depth_stencil_target = true
        },
        .rasterizer_state = {
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
        }
    };

    if (prg->pipeline != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(sGpuDevice, prg->pipeline);
    }

    if (prg->sdlVertexShader != NULL) {
        SDL_ReleaseGPUShader(sGpuDevice, prg->sdlVertexShader);
    }

    if (prg->sdlFragmentShader != NULL) {
        SDL_ReleaseGPUShader(sGpuDevice, prg->sdlFragmentShader);
    }

    prg->pipeline = SDL_CreateGPUGraphicsPipeline(sGpuDevice, &pipelineInfo);
    if (!prg->pipeline) {
        sys_fatal("Failed to create SDL GPU Graphics Pipeline: %s", SDL_GetError());
    }

    size_t numFloats = 0;
    for (s32 i = 0; i < MAX_SHADER_INPUTS; i++) {
        if (gShaderInputs[i].size == 0) { continue; }
        numFloats += gShaderInputs[i].size;
    }

    prg->hash = cc->hash;
    prg->numInputs = ccFeatures.num_inputs;
    prg->numFloats = numFloats;
    prg->usedTextures[0] = ccFeatures.used_textures[0];
    prg->usedTextures[1] = ccFeatures.used_textures[1];
    prg->usedLightmap = cc->cm.light_map;
    prg->usedFog = cc->cm.use_fog;

    if (prg->vertexShader) { gfx_destroy_shader(prg->vertexShader); }
    if (prg->fragmentShader) { gfx_destroy_shader(prg->fragmentShader); }

    prg->sdlVertexShader = sdlVs;
    prg->sdlFragmentShader = sdlFs;
    prg->vertexShader = vertexShader;
    prg->fragmentShader = fragmentShader;
    prg->worldGeometry = cc->cm.world_geometry;

    sShaderProgram = prg;
    return (struct ShaderProgram *)prg;
}

static struct ShaderProgram *gfx_sdl_gpu_create_or_load_post_process_shader(void) {
    s32 framePassIndex = gCurrentFramePassIndex + 1;
    struct ShaderProgramSdlGpu *prg = &sPostProcessShaderProgramPool[framePassIndex];

    if (prg->pipeline != NULL) {
        sShaderProgram = prg;
        return (struct ShaderProgram *)prg;
    }

    struct Shader *vertexShader = (struct Shader *)calloc(1, sizeof(struct Shader));
    struct Shader *fragmentShader = (struct Shader *)calloc(1, sizeof(struct Shader));
    if (!vertexShader || !fragmentShader) {
        sys_fatal("Failed to allocate shaders, ran out of memory!");
    }

    gfx_generate_post_process_vertex_and_fragment_shader(vertexShader, fragmentShader, NULL, NULL);

    SDL_GPUShader *sdlVs = gfx_sdl_gpu_create_shader(vertexShader);
    SDL_GPUShader *sdlFs = gfx_sdl_gpu_create_shader(fragmentShader);

    SDL_GPUVertexAttribute vertexAttributes[MAX_SHADER_INPUTS];
    u8 attributeCount = 0;
    u32 currentOffset = 0;

    for (s32 i = 0; i < MAX_SHADER_INPUTS; i++) {
        if (gPostProcessShaderInputs[i].size == 0) { continue; }

        s32 loc = vertexShader->shaderInputs[i].location;
        SDL_GPUVertexElementFormat format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;

        switch (vertexShader->shaderInputs[i].size) {
            case 1: format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT; break;
            case 2: format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2; break;
            case 3: format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; break;
            case 4: format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4; break;
        }

        vertexAttributes[attributeCount].location = loc;
        vertexAttributes[attributeCount].buffer_slot = 0;
        vertexAttributes[attributeCount].format = format;
        vertexAttributes[attributeCount].offset = currentOffset;

        currentOffset += vertexShader->shaderInputs[i].size * sizeof(float);
        attributeCount++;
    }

    SDL_GPUVertexBufferDescription vertexBufferDesc = {
        .slot = 0,
        .pitch = currentOffset,
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0
    };

    SDL_GPUColorTargetDescription colorTargetDesc = {
        .format = sSwapchainFormat,
        .blend_state = { .enable_blend = false }
    };

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {
        .vertex_shader = sdlVs,
        .fragment_shader = sdlFs,
        .vertex_input_state = {
            .vertex_buffer_descriptions = &vertexBufferDesc,
            .num_vertex_buffers = (attributeCount > 0) ? 1 : 0,
            .vertex_attributes = vertexAttributes,
            .num_vertex_attributes = attributeCount
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .target_info = {
            .color_target_descriptions = &colorTargetDesc,
            .num_color_targets = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            .has_depth_stencil_target = true
        },
        .rasterizer_state = {
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
        }
    };

    prg->pipeline = SDL_CreateGPUGraphicsPipeline(sGpuDevice, &pipelineInfo);
    if (!prg->pipeline) {
        sys_fatal("Failed to create post process SDL GPU Pipeline: %s", SDL_GetError());
    }

    size_t numFloats = 0;
    for (s32 i = 0; i < MAX_SHADER_INPUTS; i++) {
        numFloats += gPostProcessShaderInputs[i].size;
    }

    prg->hash = framePassIndex;
    prg->numInputs = attributeCount;
    prg->numFloats = numFloats;
    prg->usedTextures[0] = false;
    prg->usedTextures[1] = false;
    prg->usedLightmap = false;
    prg->usedFog = false;
    prg->worldGeometry = false;

    if (prg->vertexShader) { gfx_destroy_shader(prg->vertexShader); }
    if (prg->fragmentShader) { gfx_destroy_shader(prg->fragmentShader); }

    prg->sdlVertexShader = sdlVs;
    prg->sdlFragmentShader = sdlFs;
    prg->vertexShader = vertexShader;
    prg->fragmentShader = fragmentShader;

    sShaderProgram = prg;
    return (struct ShaderProgram *)prg;
}

static struct ShaderProgram *gfx_sdl_gpu_lookup_shader(struct ColorCombiner *cc) {
    int framePassIndex = gCurrentFramePassIndex + 1;
    if (framePassIndex < 0 || framePassIndex >= MAX_FRAME_PASSES) { return NULL; }
    for (size_t i = 0; i < sShaderProgramPoolSize[framePassIndex]; i++) {
        if (sShaderProgramPool[framePassIndex][i].hash == cc->hash) {
            return (struct ShaderProgram *)&sShaderProgramPool[framePassIndex][i];
        }
    }
    return NULL;
}

static struct ShaderProgram *gfx_sdl_gpu_lookup_shader_using_index(u8 shaderIndex, u8 framePassIndex) {
    framePassIndex++;
    if (shaderIndex >= sShaderProgramPoolSize[framePassIndex]) { return NULL; }
    return (struct ShaderProgram *)&sShaderProgramPool[framePassIndex][shaderIndex];
}

static void gfx_sdl_gpu_shader_get_info(struct ShaderProgram *prg, u8 *numInputs, bool used_textures[2]) {
    struct ShaderProgramSdlGpu *p = (struct ShaderProgramSdlGpu *)prg;
    if (!p) { return; }

    *numInputs = p->numInputs;
    used_textures[0] = p->usedTextures[0];
    used_textures[1] = p->usedTextures[1];
}

static void gfx_sdl_gpu_create_framebuffer(struct FramePass *framePass) {
    if (framePass == NULL) { return; }

    u32 viewportWidth;
    u32 viewportHeight;
    gfx_get_frame_pass_viewport_dimensions(framePass, &viewportWidth, &viewportHeight);

    SDL_GPUTextureCreateInfo colorDesc = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = viewportWidth,
        .height = viewportHeight,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };

    SDL_GPUTexture *colorTex = SDL_CreateGPUTexture(sGpuDevice, &colorDesc);
    if (colorTex == NULL) {
        return;
    }

    SDL_GPUTextureCreateInfo depthDesc = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width = viewportWidth,
        .height = viewportHeight,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };

    SDL_GPUTexture *depthTex = SDL_CreateGPUTexture(sGpuDevice, &depthDesc);
    if (depthTex == NULL) {
        SDL_ReleaseGPUTexture(sGpuDevice, colorTex);
        return;
    }

    framePass->passTexture = (u64)(uintptr_t)colorTex;
    framePass->d3dRtv = (void *)colorTex;
    framePass->d3dDsv = (void *)depthTex;
    framePass->fbo = 1;
}

static void gfx_sdl_gpu_delete_framebuffer(struct FramePass *framePass) {
    if (framePass == NULL || !framePass->fbo) {
        return;
    }

    if (framePass->d3dRtv != NULL) {
        SDL_ReleaseGPUTexture(sGpuDevice, (SDL_GPUTexture *)framePass->d3dRtv);
        framePass->d3dRtv = NULL;
    }

    if (framePass->d3dDsv != NULL) {
        SDL_ReleaseGPUTexture(sGpuDevice, (SDL_GPUTexture *)framePass->d3dDsv);
        framePass->d3dDsv = NULL;
    }

    framePass->passTexture = 0;
    framePass->fbo = 0;
}

static void gfx_sdl_gpu_set_framebuffer(struct FramePass *framePass) {
    if (framePass == NULL || !framePass->fbo) {
        return;
    }

    gfx_sdl_gpu_setup_command_buffer();
    gfx_sdl_gpu_end_render_pass();

    SDL_GPUColorTargetInfo colorTargetInfo = {
        .texture = (SDL_GPUTexture *)framePass->d3dRtv,
        .clear_color = {
            (float)framePass->clearColor[0] / 255.0f,
            (float)framePass->clearColor[1] / 255.0f,
            (float)framePass->clearColor[2] / 255.0f,
            (float)framePass->clearColor[3] / 255.0f
        },
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
    };

    SDL_GPUDepthStencilTargetInfo depthTargetInfo = {
        .texture = (SDL_GPUTexture *)framePass->d3dDsv,
        .clear_depth = 1.0f,
        .load_op = SDL_GPU_LOADOP_DONT_CARE,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
    };

    sRenderPass = SDL_BeginGPURenderPass(sCmdBuffer, &colorTargetInfo, 1, &depthTargetInfo);

    u32 viewportWidth;
    u32 viewportHeight;
    gfx_get_frame_pass_viewport_dimensions(framePass, &viewportWidth, &viewportHeight);

    SDL_GPUViewport vp = {
        .x = 0.0f,
        .y = 0.0f,
        .w = (float)viewportWidth,
        .h = (float)viewportHeight,
        .min_depth = 0.0f,
        .max_depth = 1.0f
    };
    SDL_SetGPUViewport(sRenderPass, &vp);

    SDL_Rect scissor = {
        .x = 0,
        .y = 0,
        .w = (int)viewportWidth,
        .h = (int)viewportHeight
    };
    SDL_SetGPUScissor(sRenderPass, &scissor);
}

static void gfx_sdl_gpu_reset_framebuffer(void) {
    gfx_sdl_gpu_setup_command_buffer();
    gfx_sdl_gpu_end_render_pass();

    if (!sStartedFrame) {
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(sCmdBuffer, sSdlWindow, &sSwapchainTex, &sRenderWidth, &sRenderHeight) || sSwapchainTex == NULL) {
            return;
        }
        sStartedFrame = true;
    }

    SDL_GPUColorTargetInfo colorTargetInfo = {
        .texture = sSwapchainTex,
        .load_op = SDL_GPU_LOADOP_LOAD,
        .store_op = SDL_GPU_STOREOP_STORE,
    };

    SDL_GPUDepthStencilTargetInfo depthTargetInfo = {
        .texture = sDepthTexture,
        .load_op = SDL_GPU_LOADOP_DONT_CARE,
        .store_op = SDL_GPU_STOREOP_DONT_CARE,
    };

    sRenderPass = SDL_BeginGPURenderPass(sCmdBuffer, &colorTargetInfo, 1, sDepthTexture ? &depthTargetInfo : NULL);

    SDL_GPUViewport vp = {
        .x = 0.0f,
        .y = 0.0f,
        .w = (float)sRenderWidth,
        .h = (float)sRenderHeight,
        .min_depth = 0.0f,
        .max_depth = 1.0f
    };
    SDL_SetGPUViewport(sRenderPass, &vp);

    SDL_Rect scissor = {
        .x = 0,
        .y = 0,
        .w = (int)sRenderWidth,
        .h = (int)sRenderHeight
    };
    SDL_SetGPUScissor(sRenderPass, &scissor);
}

static void gfx_sdl_gpu_set_uniform_buffer(enum ShaderStage stage, const char *name) {
    struct Shader *shader = NULL;
    s32 *destination = NULL;

    if (stage == SHADER_STAGE_VERTEX) {
        if (!sShaderProgram || !sShaderProgram->vertexShader) { return; }
        shader = sShaderProgram->vertexShader;
        destination = &gSelectedVertexUniformBuffer;
    } else if (stage == SHADER_STAGE_FRAGMENT) {
        if (!sShaderProgram || !sShaderProgram->fragmentShader) { return; }
        shader = sShaderProgram->fragmentShader;
        destination = &gSelectedFragmentUniformBuffer;
    } else {
        return;
    }

    for (s32 i = 0; i < MAX_UNIFORM_BLOCKS; i++) {
        struct ShaderUniformBlock *uniformBlock = &shader->uniformBlocks[i];

        if (strcmp(uniformBlock->name, name) == 0) {
            *destination = i;
            return;
        }
    }
}

static void gfx_sdl_gpu_set_uniform_for_specific_shader(struct ShaderUniformBlock *uniformBlock, const char *name, UNUSED ShaderUniformType type, const void *data, u32 numElements) {
    if (uniformBlock == NULL || uniformBlock->buffer == NULL) {
        return;
    }

    for (s32 i = 0; i < MAX_SHADER_UNIFORMS; i++) {
        struct ShaderUniform *uniform = &uniformBlock->uniforms[i];
        if (uniform->size == 0) { break; }

        if (strcmp(uniform->name, name) == 0) {
            u8 *dst = uniformBlock->buffer;
            dst += uniform->location;

            if (uniform->arrayLength > 1) {
                const u8 *src = (const u8 *)data;
                u32 count = MIN(numElements, (u32)uniform->arrayLength);
                for (u32 j = 0; j < count; j++) {
                    memcpy(dst + j * uniform->arrayStride, src + j * uniform->elementSize, uniform->elementSize);
                }
            } else {
                memcpy(dst, data, uniform->size);
            }
            return;
        }
    }
}

static void gfx_sdl_gpu_set_uniform(struct ShaderProgram *prg, const char *name, ShaderUniformType type, const void *data, u32 numElements) {
    struct ShaderProgramSdlGpu *sdlPrg = (struct ShaderProgramSdlGpu *)prg;
    if (sdlPrg == NULL) {
        if (sShaderProgram == NULL) { return; }
        sdlPrg = sShaderProgram;
    }

    if (gfx_shader_stage_is(SHADER_STAGE_VERTEX)) {
        gfx_sdl_gpu_set_uniform_for_specific_shader(&sdlPrg->vertexShader->uniformBlocks[gSelectedVertexUniformBuffer], name, type, data, numElements);
    }

    if (gfx_shader_stage_is(SHADER_STAGE_FRAGMENT)) {
        gfx_sdl_gpu_set_uniform_for_specific_shader(&sdlPrg->fragmentShader->uniformBlocks[gSelectedFragmentUniformBuffer], name, type, data, numElements);
    }
}

static u32 gfx_sdl_gpu_new_texture(void) {
    if (sTexturesCount >= sTexturesCapacity) {
        sTexturesCapacity = (sTexturesCapacity == 0) ? 16 : sTexturesCapacity * 2;
        sTextures = realloc(sTextures, sTexturesCapacity * sizeof(struct TextureData));
        if (!sTextures) {
            sys_fatal("Failed to reallocate texture storage array!");
        }
    }

    u32 newId = sTexturesCount++;
    memset(&sTextures[newId], 0, sizeof(struct TextureData));
    return newId;
}

static void gfx_sdl_gpu_select_texture(s32 tile, u32 texture_id) {
    sCurrentTile = tile;
    if (tile >= 0 && tile < MAX_TEXTURES) {
        sCurrentTextureIds[tile] = texture_id;
    }
}

static void gfx_sdl_gpu_bind_texture_raw(s32 tile, u64 textureId) {
    SDL_GPUTexture *texture = (SDL_GPUTexture *)(uintptr_t)textureId;
    if (texture == NULL || sRenderPass == NULL) {
        return;
    }

    SDL_GPUSampler *sampler = NULL;

    if (tile < MAX_TEXTURES) {
        sampler = sTextures[sCurrentTextureIds[tile]].sampler;
    } else {
        struct FramePass *currentFramePass = NULL;

        for (s32 i = 0; i < MAX_CUSTOM_FRAME_PASSES; i++) {
            struct FramePass *framePass = &gFramePasses[i];
            if (!framePass->active) { continue; }
            if (framePass->passTexture != textureId) { continue; }
            currentFramePass = framePass;
            break;
        }

        if (currentFramePass == NULL) {
            currentFramePass = &gDefaultGeoFramePass;
        }

        sampler = (currentFramePass->passFilter == PASS_FILTER_LINEAR) ? sLinearClampSampler : sNearestClampSampler;
    }

    if (sampler != NULL) {
        SDL_GPUTextureSamplerBinding binding = {
            .texture = texture,
            .sampler = sampler
        };
        SDL_BindGPUFragmentSamplers(sRenderPass, tile, &binding, 1);
    }
}

static void gfx_sdl_gpu_upload_texture(const u8 *rgba32_buf, s32 width, s32 height) {
    if (sCurrentTile < 0 || sCurrentTile >= MAX_TEXTURES) { return; }

    u32 textureId = sCurrentTextureIds[sCurrentTile];
    struct TextureData *textureData = &sTextures[textureId];

    SDL_GPUTextureCreateInfo textureCreateInfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = (u32)width,
        .height = (u32)height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };

    SDL_GPUTexture *texture = SDL_CreateGPUTexture(sGpuDevice, &textureCreateInfo);
    if (!texture) {
        sys_fatal("Failed to create SDL GPU texture for upload: %s", SDL_GetError());
    }

    u32 bufferSize = (u32)(width * height * 4);
    SDL_GPUTransferBufferCreateInfo transferCreateInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = bufferSize
    };

    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(sGpuDevice, &transferCreateInfo);
    if (!transferBuffer) {
        sys_fatal("Failed to create transfer buffer: %s", SDL_GetError());
    }

    void *map = SDL_MapGPUTransferBuffer(sGpuDevice, transferBuffer, false);
    if (map) {
        memcpy(map, rgba32_buf, bufferSize);
        SDL_UnmapGPUTransferBuffer(sGpuDevice, transferBuffer);
    }

    SDL_GPUCommandBuffer *uploadCmdBuffer = SDL_AcquireGPUCommandBuffer(sGpuDevice);
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(uploadCmdBuffer);

    SDL_GPUTextureTransferInfo source = {
        .transfer_buffer = transferBuffer,
        .offset = 0
    };

    SDL_GPUTextureRegion destination = {
        .texture = texture,
        .w = (u32)width,
        .h = (u32)height,
        .d = 1
    };

    SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmdBuffer);

    SDL_ReleaseGPUTransferBuffer(sGpuDevice, transferBuffer);

    textureData->width = (u32)width;
    textureData->height = (u32)height;

    if (textureData->texture != NULL) {
        SDL_ReleaseGPUTexture(sGpuDevice, textureData->texture);
    }
    textureData->texture = texture;
}

static void gfx_sdl_gpu_set_sampler_parameters(s32 tile, bool linear_filter, u32 cms, u32 cmt) {
    u32 textureId = sCurrentTextureIds[tile];
    struct TextureData *textureData = &sTextures[textureId];

    // check if sampler already exists
    if (textureData->sampler != NULL &&
        textureData->linearFilter == linear_filter &&
        textureData->cms == cms &&
        textureData->cmt == cmt) {
        return;
    }

    if (textureData->sampler != NULL) {
        SDL_ReleaseGPUSampler(sGpuDevice, textureData->sampler);
    }

    SDL_GPUFilter filterMode = linear_filter ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;

    SDL_GPUSamplerCreateInfo samplerCreateInfo = {
        .min_filter = filterMode,
        .mag_filter = filterMode,
        .mipmap_mode = linear_filter ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = gfx_cm_to_sdl_gpu(cms),
        .address_mode_v = gfx_cm_to_sdl_gpu(cmt),
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };

    textureData->sampler = SDL_CreateGPUSampler(sGpuDevice, &samplerCreateInfo);
    textureData->linearFilter = linear_filter;
    textureData->cms = cms;
    textureData->cmt = cmt;
}

static void gfx_sdl_gpu_set_depth_test(bool depthTest) {
    sDepthTest = depthTest;
}

static void gfx_sdl_gpu_set_depth_mask(bool zUpd) {
    sDepthMask = zUpd;
}

static void gfx_sdl_gpu_set_zmode_decal(bool zModeDecal) {
    sZModeDecal = zModeDecal;
}

static void gfx_sdl_gpu_set_viewport(s32 x, s32 y, s32 width, s32 height) {
    if (sRenderPass == NULL) {
        return;
    }

    struct FramePass *framePass = gfx_get_current_frame_pass();
    u32 viewportHeight;
    gfx_get_frame_pass_viewport_dimensions(framePass, NULL, &viewportHeight);

    SDL_GPUViewport vp = {
        .x = (f32)x,
        .y = (f32)(viewportHeight - y - height),
        .w = (f32)width,
        .h = (f32)height,
        .min_depth = 0.0f,
        .max_depth = 1.0f
    };

    SDL_SetGPUViewport(sRenderPass, &vp);
}

static void gfx_sdl_gpu_set_scissor(s32 x, s32 y, s32 width, s32 height) {
    if (sRenderPass == NULL) {
        return;
    }

    struct FramePass *framePass = gfx_get_current_frame_pass();
    u32 viewportHeight;
    gfx_get_frame_pass_viewport_dimensions(framePass, NULL, &viewportHeight);

    SDL_Rect r = {
        .x = x,
        .y = (s32)viewportHeight - y - height,
        .w = width,
        .h = height
    };

    SDL_SetGPUScissor(sRenderPass, &r);
}

static void gfx_sdl_gpu_set_use_alpha(UNUSED bool useAlpha) {
}

static void gfx_sdl_gpu_set_vsync(bool enabled) {
    enum SDL_GPUPresentMode presentMode = (enabled ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE);
    SDL_SetGPUSwapchainParameters(sGpuDevice, sSdlWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, presentMode);
}

static void upload_uniform_buffers_for_shader(struct Shader *shader) {
    if (shader == NULL || sRenderPass == NULL) { return; }

    for (s32 i = 0; i < shader->uniformBlockCount; i++) {
        struct ShaderUniformBlock *uniformBlock = &shader->uniformBlocks[i];
        if (uniformBlock->size == 0) { continue; }

        if (shader->stage == SHADER_STAGE_VERTEX) {
            SDL_PushGPUVertexUniformData(sCmdBuffer, uniformBlock->location, uniformBlock->buffer, (u32)uniformBlock->size);
        } else if (shader->stage == SHADER_STAGE_FRAGMENT) {
            SDL_PushGPUFragmentUniformData(sCmdBuffer, uniformBlock->location, uniformBlock->buffer, (u32)uniformBlock->size);
        }
    }
}

void gfx_sdl_gpu_draw_triangles(f32 buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    if (sShaderProgram == NULL || sRenderPass == NULL) { return; }

    u32 offset = 0;
    u32 vboByteSize = (u32)(buf_vbo_len * sizeof(f32));

    if (buf_vbo_len > 0) {
        // allocate new data to vertex ring buffer
        offset = gfx_sdl_gpu_allocate_to_ring_buffer(&sVertexRingBuffer, vboByteSize);
        memcpy(sVertexRingBuffer.mappedData + offset, buf_vbo, vboByteSize);

        // end current render pass
        gfx_sdl_gpu_end_render_pass();

        // create a copy pass
        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(sCmdBuffer);

        // create the transfer source
        SDL_GPUTransferBufferLocation transferSrc = {
            .transfer_buffer = sVertexRingBuffer.transferBuffer,
            .offset = offset
        };

        // create the destination
        SDL_GPUBufferRegion bufferDst = {
            .buffer = sVertexRingBuffer.gpuBuffer,
            .offset = offset,
            .size = vboByteSize
        };

        // upload data to gpu buffer
        SDL_UploadToGPUBuffer(copyPass, &transferSrc, &bufferDst, false);
        // end copy pass
        SDL_EndGPUCopyPass(copyPass);

        // resume last render pass
        gfx_sdl_gpu_resume_render_pass();
    }

    if (sLastZModeDecal != sZModeDecal) {
        sLastZModeDecal = sZModeDecal;
        if (sZModeDecal) {
            // this function does not exist. This is done in the raster state during
            // the shader pipeline creation, but it is changed live. That causes
            // complication
            //SDL_SetGPUDepthBias(sRenderPass, -2.0f, 0.0f, 0.0f);
        } else {
            // this function does not exist. This is done in the raster state during
            // the shader pipeline creation, but it is changed live. That causes
            // complication
            //SDL_SetGPUDepthBias(sRenderPass, 0.0f, 0.0f, 0.0f);
        }
    }

    for (s32 i = 0; i < MAX_TEXTURES; i++) {
        if (sShaderProgram->usedTextures[i]) {
            u32 textureId = sCurrentTextureIds[i];
            struct TextureData *textureData = &sTextures[textureId];

            if (textureData->texture != NULL && textureData->sampler != NULL) {
                SDL_GPUTextureSamplerBinding binding = {
                    .texture = textureData->texture,
                    .sampler = textureData->sampler
                };
                SDL_BindGPUFragmentSamplers(sRenderPass, i, &binding, 1);

                char sizeUniformName[MAX_SHADER_VARIABLE_NAME];
                snprintf(sizeUniformName, sizeof(sizeUniformName), "uTex%dSize", i);
                f32 texSize[2] = { (f32)textureData->width, (f32)textureData->height };
                gfx_sdl_gpu_set_uniform((struct ShaderProgram *)sShaderProgram, sizeUniformName, SHADER_UNIFORM_TYPE_VEC2, texSize, 1);

                char filterUniformName[MAX_SHADER_VARIABLE_NAME];
                snprintf(filterUniformName, sizeof(filterUniformName), "uTex%dFilter", i);
                u32 isLinear = textureData->linearFilter ? 1 : 0;
                gfx_sdl_gpu_set_uniform((struct ShaderProgram *)sShaderProgram, filterUniformName, SHADER_UNIFORM_TYPE_INT, &isLinear, 1);
            }
        }
    }

    gfx_update_matrices();
    if (sShaderProgram->usedFog) {
        gfx_update_fog_uniforms();
    }
    smlua_call_event_hooks(HOOK_ON_DRAW_TRIANGLE);

    upload_uniform_buffers_for_shader(sShaderProgram->vertexShader);
    upload_uniform_buffers_for_shader(sShaderProgram->fragmentShader);

    SDL_BindGPUGraphicsPipeline(sRenderPass, sShaderProgram->pipeline);

    if (buf_vbo_len > 0) {
        // bind vertex buffers
        SDL_GPUBufferBinding vboBinding = {
            .buffer = sVertexRingBuffer.gpuBuffer,
            .offset = offset
        };
        SDL_BindGPUVertexBuffers(sRenderPass, 0, &vboBinding, 1);

        // draw tris
        u32 numVertices = (u32)(buf_vbo_num_tris * 3);
        SDL_DrawGPUPrimitives(sRenderPass, numVertices, 1, 0, 0);
    }
}

static void gfx_sdl_gpu_init(void) {
    sSdlWindow = gfx_wm_get_window();

    // get and claim gpu device
    sGpuDevice = SDL_CreateGPUDevice(sShaderFormats, false, NULL);
    if (sGpuDevice == NULL) {
        sys_fatal("Couldn't create GPU device: %s", SDL_GetError());
    }

    if (!SDL_ClaimWindowForGPUDevice(sGpuDevice, sSdlWindow)) {
        sys_fatal("Failed to claim window for gpu device: %s", SDL_GetError());
    }

    // set width and height of screen
    gfx_wm_get_dimensions(&sRenderWidth, &sRenderHeight);

    // init default samplers
    SDL_GPUSamplerCreateInfo linearClampInfo = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
    };
    sLinearClampSampler = SDL_CreateGPUSampler(sGpuDevice, &linearClampInfo);
    if (!sLinearClampSampler) {
        sys_fatal("Failed to create default linear clamp sampler: %s", SDL_GetError());
    }

    SDL_GPUSamplerCreateInfo nearestClampInfo = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
    };
    sNearestClampSampler = SDL_CreateGPUSampler(sGpuDevice, &nearestClampInfo);
    if (!sNearestClampSampler) {
        sys_fatal("Failed to create default nearest clamp sampler: %s", SDL_GetError());
    }

    // queue swapchain format
    sSwapchainFormat = SDL_GetGPUSwapchainTextureFormat(sGpuDevice, sSdlWindow);

    // create a 16 megabyte vertex ring buffer
    gfx_sdl_gpu_create_ring_buffer(&sVertexRingBuffer, (16 * 1024 * 1024), SDL_GPU_BUFFERUSAGE_VERTEX);

    gfx_sdl_gpu_create_depth_texture();
}

static void gfx_sdl_gpu_on_resize(void) {
    for (s32 i = 0; i < MAX_CUSTOM_FRAME_PASSES; i++) {
        struct FramePass *framePass = &gFramePasses[i];
        if (!framePass->active) { continue; }

        if (framePass->width == 0 || framePass->height == 0) {
            // needs to be recreated to redo viewport size
            gfx_sdl_gpu_delete_framebuffer(framePass);
        }
    }

    gfx_wm_get_dimensions(&sRenderWidth, &sRenderHeight);
    gfx_sdl_gpu_create_depth_texture();
}

static void gfx_sdl_gpu_start_frame(void) {
    gfx_sdl_gpu_setup_command_buffer();

    if (!sStartedFrame) {
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(sCmdBuffer, sSdlWindow, &sSwapchainTex, &sRenderWidth, &sRenderHeight) || sSwapchainTex == NULL) {
            return;
        }
        sStartedFrame = true;
    }

    gfx_sdl_gpu_reset_state();
}

static void gfx_sdl_gpu_end_frame(void) {
    gfx_sdl_gpu_end_render_pass();
}

static void gfx_sdl_gpu_finish_render(void) {
    gfx_sdl_gpu_end_render_pass();

    if (sCmdBuffer != NULL) {
        SDL_SubmitGPUCommandBuffer(sCmdBuffer);
        sCmdBuffer = NULL;
        sSwapchainTex = NULL;
    }

    sStartedFrame = false;
}

static const char *gfx_sdl_gpu_get_name(void) {
    return "SDL_GPU";
}

static bool gfx_sdl_gpu_is_legacy(void) {
    return false;
}

static void gfx_sdl_gpu_shutdown(void) {
}

struct GfxRenderingAPI gfx_sdl_gpu_api = {
    gfx_sdl_gpu_z_is_from_0_to_1,
    gfx_sdl_gpu_unload_shader,
    gfx_sdl_gpu_load_shader,
    gfx_sdl_gpu_remove_shaders,
    gfx_sdl_gpu_create_and_load_new_shader,
    gfx_sdl_gpu_create_or_load_post_process_shader,
    gfx_sdl_gpu_lookup_shader,
    gfx_sdl_gpu_lookup_shader_using_index,
    gfx_sdl_gpu_shader_get_info,
    gfx_sdl_gpu_create_framebuffer,
    gfx_sdl_gpu_delete_framebuffer,
    gfx_sdl_gpu_set_framebuffer,
    gfx_sdl_gpu_reset_framebuffer,
    gfx_sdl_gpu_set_uniform_buffer,
    gfx_sdl_gpu_set_uniform,
    gfx_sdl_gpu_new_texture,
    gfx_sdl_gpu_select_texture,
    gfx_sdl_gpu_bind_texture_raw,
    gfx_sdl_gpu_upload_texture,
    gfx_sdl_gpu_set_sampler_parameters,
    gfx_sdl_gpu_set_depth_test,
    gfx_sdl_gpu_set_depth_mask,
    gfx_sdl_gpu_set_zmode_decal,
    gfx_sdl_gpu_set_viewport,
    gfx_sdl_gpu_set_scissor,
    gfx_sdl_gpu_set_use_alpha,
    gfx_sdl_gpu_set_vsync,
    gfx_sdl_gpu_draw_triangles,
    gfx_sdl_gpu_init,
    gfx_sdl_gpu_on_resize,
    gfx_sdl_gpu_start_frame,
    gfx_sdl_gpu_end_frame,
    gfx_sdl_gpu_finish_render,
    gfx_sdl_gpu_get_name,
    gfx_sdl_gpu_is_legacy,
    gfx_sdl_gpu_shutdown
};
