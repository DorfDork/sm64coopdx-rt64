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
#include <cmath>
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

#include "gfx_rt64_texture.hpp"
#include "gfx_rt64_common.hpp"

extern "C" const char *dynos_gfx_get_name(Gfx *gfx);
extern "C" const char *dynos_gfx_get_custom_name(Gfx *gfx);

extern "C" bool dynos_texture_get(const char *textureName, struct TextureInfo *outTextureInfo);
extern "C" bool dynos_texture_get_from_data(const Texture *tex, struct TextureInfo *outTextureInfo);
extern "C" u32 dynos_texture_get_generation(void);
extern "C" u8 *dynos_texture_convert_to_rgba32(const Texture *tex, u32 width, u32 height, u8 fmt, u8 siz);

void gfx_rt64_rapi_bind_texture_raw(int tile, u64 texture_id) {
}

u32 gfx_rt64_rapi_new_texture(const char *name) {
    // We reserve 0 for unassigned textures.
    u32 textureKey = 1 + (u32)(RT64.textures.size());
    auto &recordedTexture = RT64.textures[textureKey];
    recordedTexture.linearFilter = false;
    recordedTexture.cms = 0;
    recordedTexture.cmt = 0;
    recordedTexture.hash = 0;
    recordedTexture.pendingName = (name != nullptr) ? name : "";

    return textureKey;
}

void gfx_rt64_rapi_select_texture(int tile, u32 texture_id) {
    assert(tile < 2);
    RT64.currentTile = tile;
    RT64.currentTextureIds[tile] = texture_id;
}

static void gfx_rt64_name_content_hashed_texture(u64 hash) {
    if (RT64.texNameMap.find(hash) != RT64.texNameMap.end()) {
        return;
    }

    char nameBuf[32];
    snprintf(nameBuf, sizeof(nameBuf), "texture_%016llx", (unsigned long long)(hash));
    std::string name(nameBuf);
    RT64.texNameMap[hash] = name;
    RT64.nameTexMap[name] = hash;
}

static void gfx_rt64_register_texture_id(u64 hashID, u32 textureKey, const std::string &name) {
    const std::lock_guard<std::mutex> lock(RT64.texModsMutex);

    if (RT64.texNameMap.find(hashID) == RT64.texNameMap.end()) {
        RT64.texNameMap[hashID] = name;
    }
    RT64.nameTexMap[name] = hashID;

    if (textureKey != 0) {
        RT64.textureHashIdMap[hashID] = textureKey;
    }
}

void gfx_rt64_rapi_set_material_display_list(const void *display_list) {
    RT64.materialDisplayList = display_list;
}

u64 gfx_rt64_material_vanilla_name_hash(void) {
    if (RT64.materialDisplayList == RT64.materialNameHashDl) {
        return RT64.materialNameHashCached;
    }

    auto hashIt = RT64.materialNameHashes.find(RT64.materialDisplayList);
    if (hashIt != RT64.materialNameHashes.end()) {
        RT64.materialNameHashDl = RT64.materialDisplayList;
        RT64.materialNameHashCached = hashIt->second;
        return RT64.materialNameHashCached;
    }

    u64 nameHash = 0;
    const char *name = dynos_gfx_get_name((Gfx *)(RT64.materialDisplayList));
    if ((name != nullptr) && (name[0] != '\0')) {
        nameHash = gfx_rt64_texture_name_string_hash(name);

        gfx_rt64_register_texture_id(nameHash, 0, name);
    }

    RT64.materialNameHashes[RT64.materialDisplayList] = nameHash;
    RT64.materialNameHashDl = RT64.materialDisplayList;
    RT64.materialNameHashCached = nameHash;
    return nameHash;
}

u64 gfx_rt64_material_mod_name_hash(void) {
    auto hashIt = RT64.materialModNameHashes.find(RT64.materialDisplayList);
    if (hashIt != RT64.materialModNameHashes.end()) {
        return hashIt->second;
    }

    u64 nameHash = 0;
    const char *name = dynos_gfx_get_custom_name((Gfx *)(RT64.materialDisplayList));
    if ((name != nullptr) && (name[0] != '\0')) {
        nameHash = gfx_rt64_texture_name_string_hash(name);
        gfx_rt64_register_texture_id(nameHash, 0, name);
    }

    RT64.materialModNameHashes[RT64.materialDisplayList] = nameHash;
    return nameHash;
}

static void gfx_rt64_filter_texture_id(RecordedTexture &recorded, u32 textureKey, const std::string &name, u64 contentHash) {
    if (recorded.hash != 0) {
        return;
    }

    if (!name.empty()) {
        recorded.hash = gfx_rt64_texture_name_string_hash(name);
        gfx_rt64_register_texture_id(recorded.hash, textureKey, name);
    }
    else {
        recorded.hash = contentHash;
        RT64.textureHashIdMap[recorded.hash] = textureKey;
        gfx_rt64_name_content_hashed_texture(recorded.hash);
    }
}

static void gfx_rt64_queue_texture_upload(u32 textureKey, u64 hash, u64 contentHash, const RT64_TEXTURE_DESC &desc) {
    UploadTexture uploadTexture;
    uploadTexture.desc = desc;
    uploadTexture.hash = hash;
    uploadTexture.contentHash = contentHash;
    uploadTexture.key = textureKey;

    const std::lock_guard<std::mutex> lock(RT64.textureUploadQueueMutex);
    RT64.textureUploadQueue.push(uploadTexture);
}

static bool gfx_rt64_texture_desc_from_file(RT64_TEXTURE_DESC &texDesc, const char *path, const u8 *fileBuf, size_t fileBufSize) {
    // Use special case for loading DDS directly.
    if (strstr(path, ".dds") || strstr(path, ".DDS")) {
        texDesc.byteCount = (int)(fileBufSize);
        texDesc.bytes = malloc(texDesc.byteCount);
        if (texDesc.bytes == nullptr) {
            return false;
        }
        memcpy(texDesc.bytes, fileBuf, texDesc.byteCount);
        texDesc.width = texDesc.height = texDesc.rowPitch = -1;
        texDesc.format = RT64_TEXTURE_FORMAT_DDS;
        return true;
    }

    // Use stb image to load the file from memory instead if possible.
    int width, height;
    stbi_uc *data = stbi_load_from_memory(fileBuf, (int)(fileBufSize), &width, &height, nullptr, 4);
    if (data == nullptr) {
        return false;
    }

    texDesc.bytes = data;
    texDesc.width = width;
    texDesc.height = height;
    texDesc.rowPitch = texDesc.width * 4;
    texDesc.byteCount = texDesc.height * texDesc.rowPitch;
    texDesc.format = RT64_TEXTURE_FORMAT_RGBA8;
    return true;
}

void gfx_rt64_rapi_upload_texture(const u8 *rgba32_buf, int width, int height) {
    u32 textureKey = RT64.currentTextureIds[RT64.currentTile];

    XXHash64 hashStream(0);
    hashStream.add(rgba32_buf, (size_t)(width) * (size_t)(height) * 4);
    const u64 contentHash = hashStream.hash();

    RecordedTexture &recorded = RT64.textures[textureKey];
    gfx_rt64_filter_texture_id(recorded, textureKey, recorded.pendingName, contentHash);

    RT64_TEXTURE_DESC texDesc = {};
    texDesc.width = width;
    texDesc.height = height;
    texDesc.rowPitch = texDesc.width * 4;
    texDesc.format = RT64_TEXTURE_FORMAT_RGBA8;
    texDesc.byteCount = texDesc.height * texDesc.rowPitch;
    texDesc.bytes = malloc(texDesc.byteCount);
    if (texDesc.bytes == nullptr) {
        return;
    }
    memcpy(texDesc.bytes, rgba32_buf, texDesc.byteCount);

    gfx_rt64_queue_texture_upload(textureKey, recorded.hash, contentHash, texDesc);
}

static const char *sMapTextureExtensions[] = { "", ".png", ".dds", ".jpg", ".bmp" };

static std::string gfx_rt64_mod_texture_root(struct Mod *mod) {
    if ((mod == nullptr) || !mod->isDirectory || (mod->basePath[0] == '\0')) {
        return std::string();
    }

    std::string root = mod->basePath;
    if ((root.back() != '/') && (root.back() != '\\')) {
        root += "/";
    }

    return root + "textures/";
}

static bool gfx_rt64_try_map_texture_root(u64 nameHash, const char *name, const std::string &root) {
    if (root.empty()) {
        return false;
    }

    for (const char *extension : sMapTextureExtensions) {
        const std::string path = root + name + extension;
        if (fs_sys_file_exists(path.c_str())) {
            RT64.mapTexturePaths[nameHash] = path;
            RT64.mapTexturesLoaded.erase(nameHash);
            return true;
        }
    }

    return false;
}

bool gfx_rt64_register_map_texture(const char *name, const char *preferredRoot) {
    if ((name == nullptr) || (name[0] == '\0')) {
        return false;
    }

    const u64 nameHash = gfx_rt64_get_texture_name_hash(name);

    RT64.mapTextureNames[nameHash] = name;

    if ((preferredRoot != nullptr) && gfx_rt64_try_map_texture_root(nameHash, name, preferredRoot)) {
        return true;
    }

    for (int i = 0; i < gActiveMods.entryCount; i++) {
        if (gfx_rt64_try_map_texture_root(nameHash, name, gfx_rt64_mod_texture_root(gActiveMods.entries[i]))) {
            return true;
        }
    }

    return gfx_rt64_try_map_texture_root(nameHash, name, fs_get_write_path("textures/segment2/"));
}

static u32 gfx_rt64_load_map_texture(u64 nameHash, const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        fprintf(stderr, "RT64: unable to open the map texture '%s'.\n", path.c_str());
        return 0;
    }

    fseek(f, 0, SEEK_END);
    const long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fileSize <= 0) {
        fclose(f);
        fprintf(stderr, "RT64: the map texture '%s' is empty.\n", path.c_str());
        return 0;
    }

    std::vector<u8> fileBuf((size_t)(fileSize));
    const size_t readBytes = fread(fileBuf.data(), 1, fileBuf.size(), f);
    fclose(f);
    if (readBytes != fileBuf.size()) {
        fprintf(stderr, "RT64: unable to read the map texture '%s'.\n", path.c_str());
        return 0;
    }

    RT64_TEXTURE_DESC texDesc = {};
    if (!gfx_rt64_texture_desc_from_file(texDesc, path.c_str(), fileBuf.data(), fileBuf.size())) {
        fprintf(stderr, "RT64: stb_image was unable to decode the map texture '%s'.\n", path.c_str());
        return 0;
    }

    const u32 textureKey = 1 + (u32)(RT64.textures.size());
    RecordedTexture &recorded = RT64.textures[textureKey];
    recorded.linearFilter = true;
    recorded.cms = 0;
    recorded.cmt = 0;
    recorded.hash = nameHash;

    XXHash64 hashStream(0);
    hashStream.add(fileBuf.data(), fileBuf.size());
    gfx_rt64_queue_texture_upload(textureKey, nameHash, hashStream.hash(), texDesc);

    RT64.textureHashIdMap[nameHash] = textureKey;
    return textureKey;
}

static u32 gfx_rt64_load_dynos_map_texture(u64 nameHash, const std::string &name) {
    struct TextureInfo texInfo = {};
    if (!dynos_texture_get(name.c_str(), &texInfo)) {
        return 0;
    }

    if ((texInfo.texture == nullptr) || (texInfo.width == 0) || (texInfo.height == 0)) {
        return 0;
    }

    u8 *rgba32 = dynos_texture_convert_to_rgba32(texInfo.texture, texInfo.width, texInfo.height, texInfo.format, texInfo.size);
    if (rgba32 == nullptr) {
        fprintf(stderr, "RT64: unable to convert the map texture '%s' to RGBA8.\n", name.c_str());
        return 0;
    }

    RT64_TEXTURE_DESC texDesc = {};
    texDesc.width = (int)(texInfo.width);
    texDesc.height = (int)(texInfo.height);
    texDesc.rowPitch = texDesc.width * 4;
    texDesc.byteCount = texDesc.height * texDesc.rowPitch;
    texDesc.format = RT64_TEXTURE_FORMAT_RGBA8;
    texDesc.bytes = rgba32;

    const u32 textureKey = 1 + (u32)(RT64.textures.size());
    RecordedTexture &recorded = RT64.textures[textureKey];
    recorded.linearFilter = true;
    recorded.cms = 0;
    recorded.cmt = 0;
    recorded.hash = nameHash;

    XXHash64 hashStream(0);
    hashStream.add(rgba32, (size_t)(texDesc.byteCount));
    gfx_rt64_queue_texture_upload(textureKey, nameHash, hashStream.hash(), texDesc);

    RT64.textureHashIdMap[nameHash] = textureKey;
    return textureKey;
}

u32 gfx_rt64_map_texture_key(u64 nameHash) {
    auto hashIt = RT64.textureHashIdMap.find(nameHash);
    if (hashIt != RT64.textureHashIdMap.end()) {
        return (RT64.textures.find(hashIt->second) != RT64.textures.end()) ? hashIt->second : 0;
    }

    auto pathIt = RT64.mapTexturePaths.find(nameHash);
    auto nameIt = RT64.mapTextureNames.find(nameHash);
    if ((pathIt == RT64.mapTexturePaths.end()) && (nameIt == RT64.mapTextureNames.end())) {
        return 0;
    }

    if (!RT64.mapTexturesLoaded.insert(nameHash).second) {
        return 0;
    }

    if (pathIt != RT64.mapTexturePaths.end()) {
        return gfx_rt64_load_map_texture(nameHash, pathIt->second);
    }

    return gfx_rt64_load_dynos_map_texture(nameHash, nameIt->second);
}

// Panoramic skybox tiles :)

static const int sSkyboxTileCols = 8;
static const int sSkyboxTileRows = 8;
static const int sSkyboxTileStride = 10;
static const int sSkyboxTileListCount = sSkyboxTileStride * sSkyboxTileRows;

static const int sSkyboxTileUsedTexels = 31;
static const int sSkyboxTileTexels = 32;
static const double sSkyboxTileFilterInset = 0.5;

static const u32 sSkyboxPanoramaMaxSize = 4096;

static u8 *gfx_rt64_read_skybox_tile_rgba32(const Texture *tile, u32 *outWidth, u32 *outHeight) {
    struct TextureInfo texInfo = {};
    const Texture *texData = tile;
    u32 width = 32;
    u32 height = 32;
    u8 format = G_IM_FMT_RGBA;
    u8 size = G_IM_SIZ_16b;

    if (dynos_texture_get_from_data(tile, &texInfo) && (texInfo.texture != nullptr) && (texInfo.width != 0) && (texInfo.height != 0)) {
        texData = texInfo.texture;
        width = texInfo.width;
        height = texInfo.height;
        format = texInfo.format;
        size = texInfo.size;
    }

    u8 *rgba32 = dynos_texture_convert_to_rgba32(texData, width, height, format, size);
    if (rgba32 == nullptr) {
        return nullptr;
    }

    *outWidth = width;
    *outHeight = height;
    return rgba32;
}

static void gfx_rt64_blit_skybox_tile(u8 *dst, u32 dstRowPixels, u32 dstWidth, u32 dstHeight, const u8 *src, u32 srcWidth, u32 srcHeight, double srcOriginX, double srcOriginY, double srcSpanX, double srcSpanY) {
    const double scaleX = srcSpanX / dstWidth;
    const double scaleY = srcSpanY / dstHeight;

    for (u32 y = 0; y < dstHeight; y++) {
        const double sy = fmax((srcOriginY + ((y + 0.5) * scaleY)) - 0.5, 0.0);
        const u32 y0 = ((u32)(sy) < srcHeight) ? (u32)(sy) : (srcHeight - 1);
        const u32 y1 = ((y0 + 1) < srcHeight) ? (y0 + 1) : y0;
        const double fy = sy - y0;

        for (u32 x = 0; x < dstWidth; x++) {
            const double sx = fmax((srcOriginX + ((x + 0.5) * scaleX)) - 0.5, 0.0);
            const u32 x0 = ((u32)(sx) < srcWidth) ? (u32)(sx) : (srcWidth - 1);
            const u32 x1 = ((x0 + 1) < srcWidth) ? (x0 + 1) : x0;
            const double fx = sx - x0;

            const u8 *row0 = src + (size_t)(y0) * srcWidth * 4;
            const u8 *row1 = src + (size_t)(y1) * srcWidth * 4;
            const u8 *p00 = row0 + (size_t)(x0) * 4;
            const u8 *p10 = row0 + (size_t)(x1) * 4;
            const u8 *p01 = row1 + (size_t)(x0) * 4;
            const u8 *p11 = row1 + (size_t)(x1) * 4;
            u8 *out = dst + ((size_t)(y) * dstRowPixels + x) * 4;

            for (int c = 0; c < 4; c++) {
                const double top = p00[c] + (p10[c] - p00[c]) * fx;
                const double bottom = p01[c] + (p11[c] - p01[c]) * fx;
                out[c] = (u8)((top + (bottom - top) * fy) + 0.5);
            }
        }
    }
}

u32 gfx_rt64_stitch_skybox_texture(const Texture *const *tiles) {
    if (tiles == nullptr) {
        return 0;
    }

    const u32 texGeneration = dynos_texture_get_generation();
    XXHash64 keyHash(0);
    keyHash.add(&texGeneration, sizeof(texGeneration));
    keyHash.add(tiles, sSkyboxTileListCount * sizeof(const Texture *));
    const u64 cacheKey = keyHash.hash();

    auto cacheIt = RT64.stitchedSkyTextureKeys.find(cacheKey);
    if ((cacheIt != RT64.stitchedSkyTextureKeys.end()) && (RT64.textures.find(cacheIt->second) != RT64.textures.end())) {
        return cacheIt->second;
    }

    u32 tileWidth = 0;
    u32 tileHeight = 0;
    std::vector<u8 *> tileRgba32(sSkyboxTileCols * sSkyboxTileRows, nullptr);
    bool ok = true;

    for (int row = 0; ok && (row < sSkyboxTileRows); row++) {
        for (int col = 0; ok && (col < sSkyboxTileCols); col++) {
            u32 width, height;
            u8 *rgba32 = gfx_rt64_read_skybox_tile_rgba32(tiles[row * sSkyboxTileStride + col], &width, &height);
            if (rgba32 == nullptr) {
                fprintf(stderr, "RT64: unable to convert a skybox tile to RGBA8 for panorama stitching.\n");
                ok = false;
                break;
            }

            if (tileWidth == 0) {
                tileWidth = width;
                tileHeight = height;
            }
            else if ((width != tileWidth) || (height != tileHeight)) {
                fprintf(stderr, "RT64: skybox tiles don't agree on a size, can't stitch a panorama.\n");
                free(rgba32);
                ok = false;
                break;
            }

            tileRgba32[row * sSkyboxTileCols + col] = rgba32;
        }
    }

    if (!ok) {
        for (u8 *rgba32 : tileRgba32) { free(rgba32); }
        return 0;
    }

    const double srcOriginX = (tileWidth * sSkyboxTileFilterInset) / sSkyboxTileTexels;
    const double srcOriginY = (tileHeight * sSkyboxTileFilterInset) / sSkyboxTileTexels;
    const double srcSpanX = (tileWidth * (double)(sSkyboxTileUsedTexels)) / sSkyboxTileTexels;
    const double srcSpanY = (tileHeight * (double)(sSkyboxTileUsedTexels)) / sSkyboxTileTexels;
    const u32 maxCropWidth = sSkyboxPanoramaMaxSize / sSkyboxTileCols;
    const u32 maxCropHeight = sSkyboxPanoramaMaxSize / sSkyboxTileRows;

    u32 cropWidth = (u32)(srcSpanX + 0.5);
    u32 cropHeight = (u32)(srcSpanY + 0.5);
    if (cropWidth > maxCropWidth) { cropWidth = maxCropWidth; }
    if (cropHeight > maxCropHeight) { cropHeight = maxCropHeight; }

    const u32 panoramaWidth = cropWidth * sSkyboxTileCols;
    const u32 panoramaHeight = cropHeight * sSkyboxTileRows;

    u8 *panorama = (u8 *)malloc((size_t)(panoramaWidth) * panoramaHeight * 4);
    if (panorama == nullptr) {
        for (u8 *rgba32 : tileRgba32) { free(rgba32); }
        return 0;
    }

    for (int row = 0; row < sSkyboxTileRows; row++) {
        for (int col = 0; col < sSkyboxTileCols; col++) {
            const u8 *src = tileRgba32[row * sSkyboxTileCols + col];
            u8 *dst = panorama + ((size_t)(row) * cropHeight * panoramaWidth + (size_t)(col) * cropWidth) * 4;
            gfx_rt64_blit_skybox_tile(dst, panoramaWidth, cropWidth, cropHeight, src, tileWidth, tileHeight, srcOriginX, srcOriginY, srcSpanX, srcSpanY);
        }
    }

    for (u8 *rgba32 : tileRgba32) { free(rgba32); }

    RT64_TEXTURE_DESC texDesc = {};
    texDesc.width = (int)(panoramaWidth);
    texDesc.height = (int)(panoramaHeight);
    texDesc.rowPitch = texDesc.width * 4;
    texDesc.byteCount = texDesc.height * texDesc.rowPitch;
    texDesc.format = RT64_TEXTURE_FORMAT_RGBA8;
    texDesc.bytes = panorama;

    const u32 textureKey = 1 + (u32)(RT64.textures.size());
    RecordedTexture &recorded = RT64.textures[textureKey];
    recorded.linearFilter = true;
    recorded.cms = 0;
    recorded.cmt = 0;
    recorded.hash = cacheKey;

    XXHash64 contentHash(0);
    contentHash.add(panorama, (size_t)(texDesc.byteCount));
    gfx_rt64_queue_texture_upload(textureKey, cacheKey, contentHash.hash(), texDesc);

    RT64.stitchedSkyTextureKeys[cacheKey] = textureKey;
    return textureKey;
}

#endif // _WIN32
