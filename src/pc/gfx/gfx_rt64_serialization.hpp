#pragma once

#include <string>
#include <vector>
#include "types.h"
#include "rt64/rt64.h"

u64 gfx_rt64_get_texture_name_hash(const std::string &name);
u64 gfx_rt64_texture_name_string_hash(const std::string &name);
std::string gfx_rt64_texture_mod_name(u64 texHash);
void gfx_gfx_rt64_set_level_lights(u32 levelIndex, u32 areaIndex, const std::vector<RT64_LIGHT> *lights, const RT64_SCENE_DESC *sceneDesc);
void gfx_rt64_load_level_lights(void);
void gfx_rt64_save_level_lights(void);
void gfx_gfx_rt64_set_geo_layout_mod(const std::string &geoName, const RT64_MATERIAL *materialMod, const RT64_LIGHT *lightMod, const std::string &bumpMapName, const std::string &normalMapName, const std::string &specularMapName);
void gfx_rt64_load_geo_layout_mods(void);
void gfx_rt64_save_geo_layout_mods(void);
void gfx_gfx_rt64_set_texture_mod(const std::string &texName, const RT64_MATERIAL *materialMod, const RT64_LIGHT *lightMod, const std::string &bumpMapName, const std::string &normalMapName, const std::string &specularMapName);
void gfx_rt64_load_texture_mods(void);
void gfx_rt64_save_texture_mods(void);
bool gfx_rt64_register_map_texture(const char *name, const char *preferredRoot);
