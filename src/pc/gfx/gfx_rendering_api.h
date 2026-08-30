#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "gfx_shader.h"

struct ShaderProgram;
struct ColorCombiner;
struct FramePass;

enum GfxBackendCapability {
    // Vertices remain in object space, with transforms supplied per batch.
    GFX_BACKEND_MODEL_SPACE_GEOMETRY = (1 << 0),

    // Backend handles visibility, clipping, and culling using its own camera.
    GFX_BACKEND_GPU_VISIBILITY = (1 << 1),

    // Backend tracks objects across frames using forwarded object identity tags.
    GFX_BACKEND_OBJECT_IDENTITY = (1 << 2),

    // Backend presents frames directly without engine-owned framebuffer compositing.
    GFX_BACKEND_PRESENTS_DIRECTLY = (1 << 3),

    // Backend stitches the skybox as a panoramic texture
    GFX_BACKEND_SEPARATE_SKYBOX = (1 << 4),
};

struct GfxRenderingAPI {
    bool (*z_is_from_0_to_1)(void);
    void (*unload_shader)(struct ShaderProgram *old_prg);
    void (*load_shader)(struct ShaderProgram *new_prg);
    void (*remove_shaders)(void);
    struct ShaderProgram *(*create_and_load_new_shader)(struct ColorCombiner* cc);
    struct ShaderProgram *(*create_or_load_post_process_shader)(void);
    struct ShaderProgram *(*lookup_shader)(struct ColorCombiner* cc);
    struct ShaderProgram *(*lookup_shader_using_index)(uint8_t shaderIndex, uint8_t framePassIndex);
    void (*shader_get_info)(struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2]);
    void (*create_framebuffer)(struct FramePass *framePass);
    void (*delete_framebuffer)(struct FramePass *framePass);
    void (*set_framebuffer)(struct FramePass *framePass);
    void (*reset_framebuffer)(void);
    size_t (*get_uniform_buffer_size)(enum ShaderStage stage, int bufferIndex);
    void (*set_uniform_buffer)(enum ShaderStage stage, const char *name);
    void (*set_uniform)(struct ShaderProgram *prg, const char *name, ShaderUniformType type, const void *data, uint32_t numElements);
    uint32_t (*new_texture)(const char *name);
    void (*select_texture)(int tile, uint32_t texture_id);
    void (*bind_texture_raw)(int tile, uint64_t texture_id);
    void (*upload_texture)(const uint8_t *rgba32_buf, int width, int height);
    void (*set_sampler_parameters)(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt);
    void (*set_depth_test)(bool depth_test);
    void (*set_depth_mask)(bool z_upd);
    void (*set_zmode_decal)(bool zmode_decal);
    void (*set_viewport)(int x, int y, int width, int height);
    void (*set_scissor)(int x, int y, int width, int height);
    void (*set_use_alpha)(bool use_alpha);
    void (*set_vsync)(bool enabled);
    void (*draw_triangles)(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris);
    void (*init)(void);
    void (*on_resize)(void);
    void (*start_frame)(void);
    void (*end_frame)(void);
    void (*finish_render)(void);
    const char *(*get_name)(void);
    bool (*is_legacy)(void);
    void (*shutdown)(void);

    // Hooks for ray tracer
    void (*set_fog)(u8 fogR, u8 fogG, u8 fogB, s16 fogMul, s16 fogOffset);
    void (*set_camera_perspective)(float fovDegrees, float nearDist, float farDist, bool canInterpolate);
    void (*set_camera_matrix)(float matrix[4][4]);
    void (*draw_triangles_ortho)(float bufVbo[], size_t bufVboLen, size_t bufVboNumTris, bool doubleSided, u32 uid);
    void (*draw_triangles_persp)(float bufVbo[], size_t bufVboLen, size_t bufVboNumTris, float transformAffine[4][4], bool doubleSided, u32 uid);
    void (*set_graph_node_mod)(void *graphNodeMod);
    void (*set_texture_gen)(bool enabled, const float coeffU[4], const float coeffV[4]);
    void (*register_layout_graph_node)(void *geoLayout, void *graphNode);
    void (*inherit_graph_node_mod)(void *originalGraphNode, void *replacementGraphNode);
    void *(*build_graph_node_mod)(void *graphNode, float modelviewMatrix[4][4], u32 uid);
    void (*set_material_display_list)(const void *displayList);
    void (*set_graph_node_root)(void *graphNodeRoot);
    void (*lua_config_save)(void);
    void (*toggle_inspector)(void);
    bool (*inspector_active)(void);
    bool (*handle_window_message)(void *hWnd, u32 message, uintptr_t wParam, intptr_t lParam);
    void (*main_loop_iter)(void (*runOneGameIter)(void));
    u32 (*get_capabilities)(void);
    bool (*shader_uses_full_vertex_layout)(struct ShaderProgram *prg);
    bool (*set_skybox)(const u8 *const *tiles, float diffuseColor[3]);
};
