#pragma once

#include "types.h"

struct lua_State;
struct Rt64AreaLighting;

#ifdef __cplusplus
extern "C" {
#endif

extern struct GfxRenderingAPI gfx_rt64_api;

void gfx_rt64_lua_register_level_lights(struct lua_State *L, s32 levelNum, s32 areaIndex, s32 tableIndex);
void gfx_rt64_lua_register_texture_mod(struct lua_State *L, const char *name, s32 tableIndex);
void gfx_rt64_lua_register_geo_layout_mod(struct lua_State *L, const char *name, s32 tableIndex);
struct Rt64AreaLighting *gfx_rt64_lua_get_area_lighting(s32 levelNum, s32 areaIndex);
bool gfx_rt64_lua_is_active(void);

#ifdef __cplusplus
}
#endif
