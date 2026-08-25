#pragma once

#include "gfx_rt64_context.hpp"

u32 gfx_rt64_rapi_new_texture(const char *name);
void gfx_rt64_rapi_bind_texture_raw(int tile, u64 texture_id);
void gfx_rt64_rapi_select_texture(int tile, u32 texture_id);
void gfx_rt64_rapi_upload_texture(const u8 *rgba32_buf, int width, int height);
void gfx_rt64_rapi_set_material_display_list(const void *display_list);

u64 gfx_rt64_material_vanilla_name_hash(void);
u64 gfx_rt64_material_mod_name_hash(void);
u32 gfx_rt64_map_texture_key(u64 nameHash);
u32 gfx_rt64_stitch_skybox_texture(const Texture *const *tiles);
