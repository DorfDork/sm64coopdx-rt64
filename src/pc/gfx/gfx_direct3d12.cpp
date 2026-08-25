#if defined(_WIN32)

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <cstring>

#include <windows.h>
#include <versionhelpers.h>
#include <wrl/client.h>

#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include "types.h"
#include "pc/configfile.h"

#include "gfx_cc.h"
#include "gfx_window_manager.h"
#include "gfx_rendering_api.h"
#include "gfx_shader.h"

#include "game/rendering_graph_node.h"

extern "C" {
    #include "gfx_pc.h"
    #include "pc/lua/smlua.h"
    #include "pc/fs/fs.h"
    #include "pc/mods/mods_utils.h"
    #include "pc/controller/controller_bind_mapping.h"
    #include "engine/math_util.h"
    extern Color gVertexColor;
}

#define DECLARE_GFX_DXGI_FUNCTIONS
#include "gfx_window_dxgi.h"

#include "gfx_screen_config.h"

#define DEBUG_D3D12 0

using namespace Microsoft::WRL;

namespace {

#define D3D12_FRAMES_IN_FLIGHT 3
#define D3D12_FRAME_BUFFERS_COUNT (D3D12_FRAMES_IN_FLIGHT + 1)
#define D3D12_SWAP_CHAIN_BUFFERS (D3D12_FRAMES_IN_FLIGHT + 1)
#define D3D12_VERTEX_ARENA_SIZE (16u * 1024u * 1024u)
#define D3D12_UNIFORM_ARENA_SIZE (8u * 1024u * 1024u)
#define D3D12_TEXTURE_ARENA_SIZE (16u * 1024u * 1024u)
#define D3D12_MAX_GAME_TEXTURES (1 + MAX_CACHED_TEXTURES * (D3D12_FRAME_BUFFERS_COUNT + 1))
#define D3D12_DEFAULT_SRV_SLOT 0
#define D3D12_MAX_SAMPLER_VARIANTS 32
#define D3D12_HEAP_SHIFT_PADDING (MAX_FRAME_PASSES - 1)
#define D3D12_SRV_HEAP_SIZE (D3D12_MAX_GAME_TEXTURES + MAX_FRAME_PASSES + D3D12_HEAP_SHIFT_PADDING)
#define D3D12_PSO_VARIANT_COUNT 16
#define D3D12_PASS_TEXTURE_REGISTER_BASE 10
#define D3D12_SAMPLER_HEAP_PASS_OFFSET D3D12_MAX_SAMPLER_VARIANTS
#define D3D12_SAMPLER_HEAP_SIZE (D3D12_MAX_SAMPLER_VARIANTS + MAX_FRAME_PASSES + D3D12_HEAP_SHIFT_PADDING)
#define D3D12_ROOT_PARAM_VS_CBV_BASE 0
#define D3D12_ROOT_PARAM_FS_CBV_BASE MAX_UNIFORM_BLOCKS
#define D3D12_ROOT_PARAM_SRV_BASE (2 * MAX_UNIFORM_BLOCKS)
#define D3D12_ROOT_PARAM_SAMPLER_BASE (D3D12_ROOT_PARAM_SRV_BASE + MAX_TEXTURES)
#define D3D12_ROOT_PARAM_PASS_SRV (D3D12_ROOT_PARAM_SAMPLER_BASE + MAX_TEXTURES)
#define D3D12_ROOT_PARAM_PASS_SAMPLER (D3D12_ROOT_PARAM_PASS_SRV + 1)
#define D3D12_ROOT_PARAM_COUNT (D3D12_ROOT_PARAM_PASS_SAMPLER + 1)

#if DEBUG_D3D12
#define D3D12_COMPILE_FLAGS D3DCOMPILE_DEBUG
#else

#define D3D12_COMPILE_FLAGS D3DCOMPILE_OPTIMIZATION_LEVEL1
#endif

#define D3D12_PIPELINE_LIBRARY_FILE "d3d12_pipeline_cache.bin"

struct D3D12TextureData {
    ID3D12Resource *resource;
    bool linear_filtering;
    u32 cms, cmt;
    int sampler_slot;
    int srv_slot; // D3D12_DEFAULT_SRV_SLOT until the first upload
    u32 width, height;
};

struct ShaderProgramD3D12 {
    ID3D12PipelineState *pso[D3D12_PSO_VARIANT_COUNT];
    ID3DBlob *vs_blob;
    ID3DBlob *ps_blob;
    D3D12_INPUT_ELEMENT_DESC input_elements[16];
    UINT input_element_count;

    struct Shader *vertexShader;
    struct Shader *fragmentShader;

    u64 hash;
    u64 pso_cache_key;
    u8 num_inputs;
    u8 num_floats;
    bool used_textures[2];
    bool used_fog;
    bool uses_matrices;
};

struct D3D12UploadArena {
    ID3D12Resource *resource;
    u8 *mapped;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_base;
    size_t size;
    size_t offset;
};

struct D3D12UploadPool {
    std::vector<D3D12UploadArena> blocks;
    size_t current;
    size_t block_size;
    const char *debug_name;
};

struct D3D12FrameSlot {
    ID3D12CommandAllocator *allocator;
    UINT64 fence_value;
    D3D12UploadPool vertex_pool;
    D3D12UploadPool uniform_pool;
    D3D12UploadPool texture_pool;
};

struct D3D12SamplerVariant {
    bool used;
    bool linear_filter;
    u32 cms, cmt;
};

static struct {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12InfoQueue> info_queue;
    ComPtr<IDXGISwapChain3> swap_chain;
    ComPtr<ID3D12CommandQueue> command_queue;
    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12RootSignature> root_signature;

    ComPtr<ID3D12PipelineLibrary> pipeline_library;
    std::vector<u8> pipeline_library_blob;
    bool pipeline_library_dirty;

    ComPtr<ID3D12DescriptorHeap> rtv_heap;
    ComPtr<ID3D12DescriptorHeap> dsv_heap;
    ComPtr<ID3D12Resource> backbuffers[D3D12_SWAP_CHAIN_BUFFERS];
    ComPtr<ID3D12Resource> depth_buffer;
    UINT rtv_descriptor_size;
    UINT dsv_descriptor_size;

    ComPtr<ID3D12DescriptorHeap> srv_heap;
    ComPtr<ID3D12DescriptorHeap> sampler_heap;
    UINT srv_descriptor_size;
    UINT sampler_descriptor_size;
    D3D12SamplerVariant sampler_variants[D3D12_MAX_SAMPLER_VARIANTS];
    ComPtr<ID3D12Resource> default_texture;

    bool framebuffer_is_shader_resource[MAX_FRAME_PASSES];
    bool pass_sampler_linear[MAX_FRAME_PASSES];
    ID3D12Resource *framebuffer_color[MAX_FRAME_PASSES];

    ComPtr<ID3D12Fence> fence;
    HANDLE fence_event;
    UINT64 next_fence_value;

    D3D12FrameSlot frame_slots[D3D12_FRAME_BUFFERS_COUNT];
    int current_slot;

    std::vector<D3D12TextureData> textures;
    int current_tile;
    u32 current_texture_ids[2];

    std::vector<int> free_srv_slots;
    std::vector<std::pair<UINT64, int>> pending_srv_slot_releases;
    bool srv_slots_exhausted_warned;

    int bound_srv_slots[MAX_TEXTURES];
    int bound_sampler_slots[MAX_TEXTURES];

    struct ShaderProgramD3D12 shader_program_pool[MAX_FRAME_PASSES][CC_MAX_SHADERS];
    u8 shader_program_pool_size[MAX_FRAME_PASSES] = { 0 };
    u8 shader_program_pool_index[MAX_FRAME_PASSES] = { 0 };
    struct ShaderProgramD3D12 post_process_shader_program_pool[MAX_FRAME_PASSES];

    struct ShaderProgramD3D12 *shader_program;

    u32 current_width, current_height;
    s8 depth_test;
    s8 depth_mask;
    s8 zmode_decal;
    s8 use_alpha;

    D3D12_CPU_DESCRIPTOR_HANDLE current_rtv;
    D3D12_CPU_DESCRIPTOR_HANDLE current_dsv;
    bool current_target_is_backbuffer;
    bool current_target_has_depth;
    bool command_list_recording;
    bool backbuffer_is_render_target;
    UINT current_backbuffer_index;

    ID3D12PipelineState *last_pso;
    D3D12_GPU_VIRTUAL_ADDRESS bound_vertex_block;
    UINT bound_vertex_stride;

    u64 command_list_generation;
    u64 bound_cbv[2][MAX_UNIFORM_BLOCKS]; // [stage][block location]

    std::vector<std::pair<UINT64, IUnknown *>> pending_releases;
} d3d;

static void gfx_d3d12_widen_name(const char *name, wchar_t *wideName, size_t wideNameCount) {
    size_t i = 0;
    for (; (i + 1 < wideNameCount) && (name[i] != '\0'); i++) {
        wideName[i] = (wchar_t)((unsigned char)(name[i]));
    }
    wideName[i] = L'\0';
}

static void gfx_d3d12_set_name(ID3D12Object *object, const char *name) {
    if (object == nullptr || name == nullptr) { return; }
    wchar_t wideName[128];
    gfx_d3d12_widen_name(name, wideName, ARRAYSIZE(wideName));
    object->SetName(wideName);
}

static inline int gfx_d3d12_pso_variant_index(void) {
    return (d3d.use_alpha ? 8 : 0) | (d3d.depth_test ? 4 : 0) | (d3d.depth_mask ? 2 : 0) | (d3d.zmode_decal ? 1 : 0);
}

static inline int gfx_d3d12_frame_pass_slot(struct FramePass *framePass) {
    if (framePass == &gDefaultGeoFramePass) { return 0; }
    return (int)(framePass - gFramePasses) + 1;
}

static D3D12_CPU_DESCRIPTOR_HANDLE gfx_d3d12_srv_cpu_handle(int slot) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = d3d.srv_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (SIZE_T)(slot) * d3d.srv_descriptor_size;
    return handle;
}

static D3D12_GPU_DESCRIPTOR_HANDLE gfx_d3d12_srv_gpu_handle(int slot) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = d3d.srv_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (UINT64)(slot) * d3d.srv_descriptor_size;
    return handle;
}

static D3D12_CPU_DESCRIPTOR_HANDLE gfx_d3d12_sampler_cpu_handle(int slot) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = d3d.sampler_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (SIZE_T)(slot) * d3d.sampler_descriptor_size;
    return handle;
}

static D3D12_GPU_DESCRIPTOR_HANDLE gfx_d3d12_sampler_gpu_handle(int slot) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = d3d.sampler_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (UINT64)(slot) * d3d.sampler_descriptor_size;
    return handle;
}

static D3D12_HEAP_PROPERTIES gfx_d3d12_heap_props(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES props = {};
    props.Type = type;
    return props;
}

static D3D12_RESOURCE_DESC gfx_d3d12_buffer_desc(UINT64 size) {
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return desc;
}

static void gfx_d3d12_wait_for_fence_value(UINT64 value) {
    if (value == 0) { return; }
    if (d3d.fence->GetCompletedValue() < value) {
        ThrowIfFailed(d3d.fence->SetEventOnCompletion(value, d3d.fence_event));
        WaitForSingleObject(d3d.fence_event, INFINITE);
    }
}

static void gfx_d3d12_transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    d3d.command_list->ResourceBarrier(1, &barrier);
}

static bool gfx_d3d12_create_arena(D3D12UploadArena &arena, size_t size, const char *debugName) {
    arena = {};

    D3D12_HEAP_PROPERTIES heapProps = gfx_d3d12_heap_props(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC bufferDesc = gfx_d3d12_buffer_desc(size);

    HRESULT hr = d3d.device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&arena.resource));
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12] upload block allocation failed (%s, %zu bytes, hr=0x%08lX)\n", debugName ? debugName : "?", size, hr);
        return false;
    }
    gfx_d3d12_set_name(arena.resource, debugName);

    D3D12_RANGE readRange = { 0, 0 };
    hr = arena.resource->Map(0, &readRange, (void **)(&arena.mapped));
    if (FAILED(hr)) {
        arena.resource->Release();
        arena = {};
        return false;
    }

    arena.gpu_base = arena.resource->GetGPUVirtualAddress();
    arena.size = size;
    arena.offset = 0;
    return true;
}

static bool gfx_d3d12_arena_alloc(D3D12UploadArena &arena, size_t size, size_t alignment, void **outCpuPtr, D3D12_GPU_VIRTUAL_ADDRESS *outGpuAddr) {
    size_t alignedOffset = arena.offset;
    if ((alignment & (alignment - 1)) == 0) {
        alignedOffset = (alignedOffset + (alignment - 1)) & ~(alignment - 1);
    } else {
        size_t remainder = alignedOffset % alignment;
        if (remainder != 0) { alignedOffset += alignment - remainder; }
    }
    if (alignedOffset + size > arena.size) { return false; }

    *outCpuPtr = arena.mapped + alignedOffset;
    *outGpuAddr = arena.gpu_base + alignedOffset;
    arena.offset = alignedOffset + size;
    return true;
}

static void gfx_d3d12_pool_init(D3D12UploadPool &pool, size_t blockSize, const char *debugName) {
    pool.blocks.clear();
    pool.current = 0;
    pool.block_size = blockSize;
    pool.debug_name = debugName;
}

static void gfx_d3d12_pool_reset(D3D12UploadPool &pool) {
    for (size_t i = 0; i < pool.blocks.size(); i++) { pool.blocks[i].offset = 0; }
    pool.current = 0;
}

static bool gfx_d3d12_pool_alloc(D3D12UploadPool &pool, size_t size, size_t alignment, void **outCpuPtr, D3D12_GPU_VIRTUAL_ADDRESS *outGpuAddr, ID3D12Resource **outResource = nullptr, size_t *outOffset = nullptr) {
    while (pool.current < pool.blocks.size()) {
        D3D12UploadArena &block = pool.blocks[pool.current];
        if (gfx_d3d12_arena_alloc(block, size, alignment, outCpuPtr, outGpuAddr)) {
            if (outResource != nullptr) { *outResource = block.resource; }
            if (outOffset != nullptr) { *outOffset = (size_t)(*outGpuAddr - block.gpu_base); }
            return true;
        }
        pool.current++;
    }

    size_t blockSize = pool.block_size;
    if (size + alignment > blockSize) { blockSize = size + alignment; } // one oversized allocation still has to fit

    D3D12UploadArena block;
    if (!gfx_d3d12_create_arena(block, blockSize, pool.debug_name)) { return false; }

    pool.blocks.push_back(block);
    pool.current = pool.blocks.size() - 1;

    size_t totalBytes = 0;
    for (const D3D12UploadArena &poolBlock : pool.blocks) { totalBytes += poolBlock.size; }

    fprintf(stderr, "[D3D12] %s grew to %zu blocks (%zu bytes total)\n",
            pool.debug_name, pool.blocks.size(), totalBytes);

    D3D12UploadArena &newBlock = pool.blocks[pool.current];
    if (!gfx_d3d12_arena_alloc(newBlock, size, alignment, outCpuPtr, outGpuAddr)) { return false; }
    if (outResource != nullptr) { *outResource = newBlock.resource; }
    if (outOffset != nullptr) { *outOffset = (size_t)(*outGpuAddr - newBlock.gpu_base); }
    return true;
}

static void gfx_d3d12_pool_destroy(D3D12UploadPool &pool) {
    for (size_t i = 0; i < pool.blocks.size(); i++) {
        if (pool.blocks[i].resource != nullptr) {
            pool.blocks[i].resource->Unmap(0, nullptr);
            pool.blocks[i].resource->Release();
        }
    }
    pool.blocks.clear();
    pool.current = 0;
}

static void gfx_d3d12_retire_object(void *object) {
    if (object == nullptr) { return; }
    d3d.pending_releases.emplace_back(d3d.next_fence_value, (IUnknown *)(object));
}

static void gfx_d3d12_release_pending(UINT64 completed) {
    size_t writeIdx = 0;
    for (size_t i = 0; i < d3d.pending_releases.size(); i++) {
        if (d3d.pending_releases[i].first <= completed) {
            d3d.pending_releases[i].second->Release();
        } else {
            d3d.pending_releases[writeIdx++] = d3d.pending_releases[i];
        }
    }
    d3d.pending_releases.resize(writeIdx);

    writeIdx = 0;
    for (size_t i = 0; i < d3d.pending_srv_slot_releases.size(); i++) {
        if (d3d.pending_srv_slot_releases[i].first <= completed) {
            d3d.free_srv_slots.push_back(d3d.pending_srv_slot_releases[i].second);
        } else {
            d3d.pending_srv_slot_releases[writeIdx++] = d3d.pending_srv_slot_releases[i];
        }
    }
    d3d.pending_srv_slot_releases.resize(writeIdx);
}

static int gfx_d3d12_acquire_srv_slot(void) {
    if (d3d.free_srv_slots.empty()) {
        if (!d3d.srv_slots_exhausted_warned) {
            d3d.srv_slots_exhausted_warned = true;
            fprintf(stderr, "[D3D12] SRV descriptor slots exhausted (%d)\n", D3D12_MAX_GAME_TEXTURES);
        }
        return D3D12_DEFAULT_SRV_SLOT;
    }

    int slot = d3d.free_srv_slots.back();
    d3d.free_srv_slots.pop_back();
    return slot;
}

static void gfx_d3d12_retire_srv_slot(int slot) {
    if (slot == D3D12_DEFAULT_SRV_SLOT) { return; }
    d3d.pending_srv_slot_releases.emplace_back(d3d.next_fence_value, slot);
}

static ID3D12Resource *gfx_d3d12_create_texture(u32 width, u32 height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
                                                D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE *clearValue, const char *debugName) {
    D3D12_HEAP_PROPERTIES heapProps = gfx_d3d12_heap_props(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Flags = flags;

    ID3D12Resource *resource = nullptr;
    HRESULT hr = d3d.device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, initialState, clearValue, IID_PPV_ARGS(&resource));
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12] texture creation failed (%ux%u, format=%d, hr=0x%08lX)\n", width, height, (int)(format), hr);
        return nullptr;
    }
    gfx_d3d12_set_name(resource, debugName);
    return resource;
}

static ID3D12Resource *gfx_d3d12_create_depth_texture(u32 width, u32 height, const char *debugName) {
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    return gfx_d3d12_create_texture(width, height, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                                    D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, debugName);
}

static ID3D12DescriptorHeap *gfx_d3d12_create_descriptor_heap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, bool shaderVisible) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = count;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ID3D12DescriptorHeap *heap = nullptr;
    HRESULT hr = d3d.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12] descriptor heap creation failed (type=%d, count=%u, hr=0x%08lX)\n", (int)(type), count, hr);
        return nullptr;
    }
    return heap;
}

static void gfx_d3d12_create_descriptor_heap(ComPtr<ID3D12DescriptorHeap> &heap, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, bool shaderVisible) {
    heap.Attach(gfx_d3d12_create_descriptor_heap(type, count, shaderVisible));
    if (heap.Get() == nullptr) {
        ThrowIfFailed(E_FAIL, gfx_window_dxgi_get_h_wnd(), "Failed to create a D3D12 descriptor heap.");
    }
}

static void gfx_d3d12_create_srv(ID3D12Resource *resource, int heapSlot) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    d3d.device->CreateShaderResourceView(resource, &srvDesc, gfx_d3d12_srv_cpu_handle(heapSlot));
}

static D3D12_TEXTURE_ADDRESS_MODE gfx_d3d12_cm_to_address_mode(u32 val) {
    if (val & G_TX_CLAMP) { return D3D12_TEXTURE_ADDRESS_MODE_CLAMP; }
    return (val & G_TX_MIRROR) ? D3D12_TEXTURE_ADDRESS_MODE_MIRROR : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

static void gfx_d3d12_create_sampler(int heapSlot, bool linearFilter, u32 cms, u32 cmt) {
    D3D12_SAMPLER_DESC desc = {};
    desc.Filter = linearFilter ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU = gfx_d3d12_cm_to_address_mode(cms);
    desc.AddressV = gfx_d3d12_cm_to_address_mode(cmt);
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D12_FLOAT32_MAX;
    desc.MaxAnisotropy = 1;
    d3d.device->CreateSampler(&desc, gfx_d3d12_sampler_cpu_handle(heapSlot));
}

static bool gfx_d3d12_upload_texture_data(ID3D12Resource *texture, const u8 *rgba32_buf, u32 width) {
    D3D12_RESOURCE_DESC texDesc = texture->GetDesc();

    UINT64 uploadSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT numRows = 0;
    d3d.device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, nullptr, &uploadSize);

    void *mapped = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = 0;
    ID3D12Resource *staging = nullptr;
    size_t stagingOffset = 0;
    D3D12UploadPool &pool = d3d.frame_slots[d3d.current_slot].texture_pool;
    if (!gfx_d3d12_pool_alloc(pool, (size_t)(uploadSize), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &mapped, &gpuAddr, &staging, &stagingOffset)) {
        fprintf(stderr, "[D3D12] texture upload staging allocation failed (%llu bytes)\n", (unsigned long long)(uploadSize));
        return false;
    }

    d3d.device->GetCopyableFootprints(&texDesc, 0, 1, (UINT64)(stagingOffset), &footprint, &numRows, nullptr, nullptr);

    for (UINT row = 0; row < numRows; row++) {
        memcpy((u8 *)(mapped) + row * footprint.Footprint.RowPitch, rgba32_buf + row * width * 4, width * 4);
    }

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource = texture;
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = staging;
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = footprint;

    d3d.command_list->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
    gfx_d3d12_transition(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    return true;
}

static void gfx_d3d12_ensure_backbuffer_render_target(void) {
    if (d3d.backbuffer_is_render_target) { return; }
    gfx_d3d12_transition(d3d.backbuffers[d3d.current_backbuffer_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    d3d.backbuffer_is_render_target = true;
}

static void gfx_d3d12_ensure_backbuffer_present(void) {
    if (!d3d.backbuffer_is_render_target) { return; }
    gfx_d3d12_transition(d3d.backbuffers[d3d.current_backbuffer_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    d3d.backbuffer_is_render_target = false;
}

static void gfx_d3d12_create_render_target_views(bool is_resize) {
    if (is_resize) {
        for (int i = 0; i < D3D12_SWAP_CHAIN_BUFFERS; i++) { d3d.backbuffers[i].Reset(); }
        d3d.depth_buffer.Reset();

        DXGI_SWAP_CHAIN_DESC1 desc1;
        ThrowIfFailed(d3d.swap_chain->GetDesc1(&desc1));
        ThrowIfFailed(d3d.swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, desc1.Flags), gfx_window_dxgi_get_h_wnd(), "Failed to resize IDXGISwapChain buffers.");
    }

    DXGI_SWAP_CHAIN_DESC1 desc1;
    ThrowIfFailed(d3d.swap_chain->GetDesc1(&desc1));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = d3d.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < D3D12_SWAP_CHAIN_BUFFERS; i++) {
        ThrowIfFailed(d3d.swap_chain->GetBuffer(i, IID_PPV_ARGS(d3d.backbuffers[i].GetAddressOf())), gfx_window_dxgi_get_h_wnd(), "Failed to get backbuffer from IDXGISwapChain.");
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHandle;
        handle.ptr += (SIZE_T)(i) * d3d.rtv_descriptor_size;
        d3d.device->CreateRenderTargetView(d3d.backbuffers[i].Get(), nullptr, handle);
        char debugName[32];
        snprintf(debugName, sizeof(debugName), "D3D12_Backbuffer%u", i);
        gfx_d3d12_set_name(d3d.backbuffers[i].Get(), debugName);
    }

    d3d.depth_buffer.Attach(gfx_d3d12_create_depth_texture(desc1.Width, desc1.Height, "D3D12_BackbufferDepth"));
    if (d3d.depth_buffer.Get() != nullptr) {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        d3d.device->CreateDepthStencilView(d3d.depth_buffer.Get(), &dsvDesc, d3d.dsv_heap->GetCPUDescriptorHandleForHeapStart());
    }

    d3d.current_width = desc1.Width;
    d3d.current_height = desc1.Height;
    d3d.current_backbuffer_index = d3d.swap_chain->GetCurrentBackBufferIndex();
    d3d.backbuffer_is_render_target = false;
}

static void gfx_d3d12_load_pipeline_library(void) {
    ComPtr<ID3D12Device1> device1;
    if (FAILED(d3d.device.As(&device1))) { return; }

    const char *path = fs_get_write_path(D3D12_PIPELINE_LIBRARY_FILE);
    if (path != nullptr) {
        FILE *file = fopen(path, "rb");
        if (file != nullptr) {
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, 0, SEEK_SET);
            if (size > 0) {
                d3d.pipeline_library_blob.resize((size_t)(size));
                if (fread(d3d.pipeline_library_blob.data(), 1, (size_t)(size), file) != (size_t)(size)) {
                    d3d.pipeline_library_blob.clear();
                }
            }
            fclose(file);
        }
    }

    HRESULT hr = device1->CreatePipelineLibrary(d3d.pipeline_library_blob.empty() ? nullptr : d3d.pipeline_library_blob.data(),
                                                d3d.pipeline_library_blob.size(), IID_PPV_ARGS(d3d.pipeline_library.GetAddressOf()));
    if (FAILED(hr) && !d3d.pipeline_library_blob.empty()) {
        d3d.pipeline_library_blob.clear();
        hr = device1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(d3d.pipeline_library.GetAddressOf()));
    }

    if (FAILED(hr)) {
        d3d.pipeline_library.Reset();
        fprintf(stderr, "[D3D12] pipeline library unavailable (hr=0x%08lX), pipeline states will be rebuilt every session\n", hr);
    }
    d3d.pipeline_library_dirty = false;
}

static void gfx_d3d12_save_pipeline_library(void) {
    if (d3d.pipeline_library.Get() == nullptr || !d3d.pipeline_library_dirty) { return; }

    SIZE_T size = d3d.pipeline_library->GetSerializedSize();
    if (size == 0) { return; }

    std::vector<u8> blob(size);
    if (FAILED(d3d.pipeline_library->Serialize(blob.data(), size))) { return; }

    const char *path = fs_get_write_path(D3D12_PIPELINE_LIBRARY_FILE);
    if (path == nullptr) { return; }

    FILE *file = fopen(path, "wb");
    if (file == nullptr) { return; }
    fwrite(blob.data(), 1, size, file);
    fclose(file);

    d3d.pipeline_library_dirty = false;
}

static void gfx_d3d12_init_table_param(D3D12_ROOT_PARAMETER1 &param, D3D12_DESCRIPTOR_RANGE1 &range,
                                       D3D12_DESCRIPTOR_RANGE_TYPE type, UINT baseRegister, UINT count) {
    range = {};
    range.RangeType = type;
    range.NumDescriptors = count;
    range.BaseShaderRegister = baseRegister;
    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    if (type == D3D12_DESCRIPTOR_RANGE_TYPE_SRV) { range.Flags |= D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE; }
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges = &range;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
}

static void gfx_d3d12_create_root_signature(void) {
    D3D12_ROOT_PARAMETER1 rootParams[D3D12_ROOT_PARAM_COUNT] = {};
    D3D12_DESCRIPTOR_RANGE1 ranges[D3D12_ROOT_PARAM_COUNT] = {};

    for (int i = 0; i < MAX_UNIFORM_BLOCKS; i++) {
        D3D12_ROOT_PARAMETER1 &vsParam = rootParams[D3D12_ROOT_PARAM_VS_CBV_BASE + i];
        vsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        vsParam.Descriptor.ShaderRegister = i;
        vsParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
        vsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_PARAMETER1 &fsParam = rootParams[D3D12_ROOT_PARAM_FS_CBV_BASE + i];
        fsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        fsParam.Descriptor.ShaderRegister = i;
        fsParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
        fsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }

    for (int i = 0; i < MAX_TEXTURES; i++) {
        int srv = D3D12_ROOT_PARAM_SRV_BASE + i;
        int sampler = D3D12_ROOT_PARAM_SAMPLER_BASE + i;
        gfx_d3d12_init_table_param(rootParams[srv], ranges[srv], D3D12_DESCRIPTOR_RANGE_TYPE_SRV, i, 1);
        gfx_d3d12_init_table_param(rootParams[sampler], ranges[sampler], D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, i, 1);
    }

    gfx_d3d12_init_table_param(rootParams[D3D12_ROOT_PARAM_PASS_SRV], ranges[D3D12_ROOT_PARAM_PASS_SRV],
                               D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_PASS_TEXTURE_REGISTER_BASE, MAX_FRAME_PASSES);
    gfx_d3d12_init_table_param(rootParams[D3D12_ROOT_PARAM_PASS_SAMPLER], ranges[D3D12_ROOT_PARAM_PASS_SAMPLER],
                               D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, D3D12_PASS_TEXTURE_REGISTER_BASE, MAX_FRAME_PASSES);

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSigDesc.Desc_1_1.NumParameters = D3D12_ROOT_PARAM_COUNT;
    rootSigDesc.Desc_1_1.pParameters = rootParams;
    rootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeVersionedRootSignature(&rootSigDesc, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        const char *message = (errorBlob.Get() != nullptr) ? (const char *)(errorBlob->GetBufferPointer()) : "Failed to serialize D3D12 root signature.";
        ThrowIfFailed(hr, gfx_window_dxgi_get_h_wnd(), message);
    }

    ThrowIfFailed(d3d.device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(d3d.root_signature.GetAddressOf())));
}

static void gfx_d3d12_create_default_sampler(void) {
    for (int i = 0; i < D3D12_MAX_SAMPLER_VARIANTS; i++) { d3d.sampler_variants[i].used = false; }

    gfx_d3d12_create_sampler(0, true, G_TX_CLAMP, G_TX_CLAMP);
    d3d.sampler_variants[0].used = true;
    d3d.sampler_variants[0].linear_filter = true;
    d3d.sampler_variants[0].cms = G_TX_CLAMP;
    d3d.sampler_variants[0].cmt = G_TX_CLAMP;

    for (int i = 0; i < MAX_FRAME_PASSES; i++) {
        gfx_d3d12_create_sampler(D3D12_SAMPLER_HEAP_PASS_OFFSET + i, true, G_TX_CLAMP, G_TX_CLAMP);
        d3d.pass_sampler_linear[i] = true;
    }

    for (int i = MAX_FRAME_PASSES; i < (D3D12_SAMPLER_HEAP_SIZE - D3D12_SAMPLER_HEAP_PASS_OFFSET); i++) {
        gfx_d3d12_create_sampler(D3D12_SAMPLER_HEAP_PASS_OFFSET + i, true, G_TX_CLAMP, G_TX_CLAMP);
    }
}

static void gfx_d3d12_create_default_texture(void) {
    d3d.default_texture.Attach(gfx_d3d12_create_texture(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE,
                                                        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, "D3D12_DefaultWhiteTexture"));
    if (d3d.default_texture.Get() == nullptr) {
        ThrowIfFailed(E_FAIL, gfx_window_dxgi_get_h_wnd(), "Failed to create the D3D12 default texture.");
    }

    const u32 whitePixel = 0xFFFFFFFFu;
    gfx_d3d12_upload_texture_data(d3d.default_texture.Get(), (const u8 *)(&whitePixel), 1);

    ThrowIfFailed(d3d.command_list->Close());
    ID3D12CommandList *lists[] = { d3d.command_list.Get() };
    d3d.command_queue->ExecuteCommandLists(1, lists);

    UINT64 initFence = d3d.next_fence_value++;
    ThrowIfFailed(d3d.command_queue->Signal(d3d.fence.Get(), initFence));
    gfx_d3d12_wait_for_fence_value(initFence);

    for (int i = 0; i < D3D12_SRV_HEAP_SIZE; i++) { gfx_d3d12_create_srv(d3d.default_texture.Get(), i); }

    d3d.free_srv_slots.clear();
    d3d.free_srv_slots.reserve(D3D12_MAX_GAME_TEXTURES - 1);
    for (int i = D3D12_MAX_GAME_TEXTURES - 1; i > D3D12_DEFAULT_SRV_SLOT; i--) {
        d3d.free_srv_slots.push_back(i);
    }
    d3d.pending_srv_slot_releases.clear();
    d3d.srv_slots_exhausted_warned = false;

    ThrowIfFailed(d3d.frame_slots[0].allocator->Reset());
    ThrowIfFailed(d3d.command_list->Reset(d3d.frame_slots[0].allocator, nullptr));
    ThrowIfFailed(d3d.command_list->Close());
}

static void gfx_d3d12_drain_debug_messages(const char *when) {
    if (d3d.info_queue.Get() == nullptr) { return; }

    const UINT64 count = d3d.info_queue->GetNumStoredMessages();
    for (UINT64 i = 0; i < count; i++) {
        SIZE_T length = 0;
        if (FAILED(d3d.info_queue->GetMessage(i, nullptr, &length)) || length == 0) { continue; }

        D3D12_MESSAGE *message = (D3D12_MESSAGE *)(malloc(length));
        if (message == nullptr) { break; }
        if (SUCCEEDED(d3d.info_queue->GetMessage(i, message, &length))) {
            printf("[D3D12 debug layer] (%s) severity=%d id=%d: %.*s\n",
                   when, (int)(message->Severity), (int)(message->ID),
                   (int)(message->DescriptionByteLength), message->pDescription);
        }
        free(message);
    }
    fflush(stdout);
    d3d.info_queue->ClearStoredMessages();
}

static void gfx_d3d12_setup_info_queue(void) {
    if (FAILED(d3d.device.As(&d3d.info_queue))) {
        d3d.info_queue.Reset();
        return;
    }

    d3d.info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, FALSE);
    d3d.info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);
    d3d.info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
    printf("[D3D12] debug layer is active; its breakpoints have been disabled\n");
    fflush(stdout);
}

static void gfx_d3d12_init(void) {
#if DEBUG_D3D12
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
            debugController->EnableDebugLayer();
        }
    }
#endif

    gfx_window_dxgi_create_factory_and_device(DEBUG_D3D12, 12, [](IDXGIAdapter1 *adapter, bool test_only) {
        HRESULT res = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), test_only ? nullptr : (void **)(d3d.device.GetAddressOf()));
        if (test_only) {
            return SUCCEEDED(res);
        } else {
            ThrowIfFailed(res, gfx_window_dxgi_get_h_wnd(), "Failed to create D3D12 device.");
            return true;
        }
    });

    gfx_d3d12_setup_info_queue();

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(d3d.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(d3d.command_queue.GetAddressOf())));

    ComPtr<IDXGISwapChain1> swapChain1 = gfx_window_dxgi_create_swap_chain(d3d.command_queue.Get(), D3D12_FRAMES_IN_FLIGHT);
    ThrowIfFailed(swapChain1.As(&d3d.swap_chain));

    d3d.rtv_descriptor_size = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    d3d.dsv_descriptor_size = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    d3d.srv_descriptor_size = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    d3d.sampler_descriptor_size = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    gfx_d3d12_create_descriptor_heap(d3d.rtv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_SWAP_CHAIN_BUFFERS, false);
    gfx_d3d12_create_descriptor_heap(d3d.dsv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    gfx_d3d12_create_descriptor_heap(d3d.srv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_SRV_HEAP_SIZE, true);
    gfx_d3d12_create_descriptor_heap(d3d.sampler_heap, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_SAMPLER_HEAP_SIZE, true);

    gfx_d3d12_create_root_signature();
    gfx_d3d12_load_pipeline_library();
    gfx_d3d12_create_render_target_views(false);

    ThrowIfFailed(d3d.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(d3d.fence.GetAddressOf())));
    d3d.fence_event = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    d3d.next_fence_value = 1;

    for (int i = 0; i < D3D12_FRAME_BUFFERS_COUNT; i++) {
        D3D12FrameSlot &slot = d3d.frame_slots[i];
        ThrowIfFailed(d3d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&slot.allocator)));

        gfx_d3d12_pool_init(slot.vertex_pool, D3D12_VERTEX_ARENA_SIZE, "D3D12_VertexUpload");
        gfx_d3d12_pool_init(slot.uniform_pool, D3D12_UNIFORM_ARENA_SIZE, "D3D12_UniformUpload");
        gfx_d3d12_pool_init(slot.texture_pool, D3D12_TEXTURE_ARENA_SIZE, "D3D12_TextureUpload");

        D3D12UploadArena block;
        if (gfx_d3d12_create_arena(block, D3D12_VERTEX_ARENA_SIZE, slot.vertex_pool.debug_name)) { slot.vertex_pool.blocks.push_back(block); }
        if (gfx_d3d12_create_arena(block, D3D12_UNIFORM_ARENA_SIZE, slot.uniform_pool.debug_name)) { slot.uniform_pool.blocks.push_back(block); }
        if (gfx_d3d12_create_arena(block, D3D12_TEXTURE_ARENA_SIZE, slot.texture_pool.debug_name)) { slot.texture_pool.blocks.push_back(block); }

        slot.fence_value = 0;
    }

    d3d.current_slot = 0;
    ThrowIfFailed(d3d.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d.frame_slots[0].allocator, nullptr, IID_PPV_ARGS(d3d.command_list.GetAddressOf())));

    gfx_d3d12_create_default_sampler();
    gfx_d3d12_create_default_texture();

    controller_bind_init();
}

static bool gfx_d3d12_z_is_from_0_to_1(void) {
    return true;
}

static void gfx_d3d12_unload_shader(UNUSED struct ShaderProgram *old_prg) {
}

static void gfx_d3d12_invalidate_bound_textures(void) {
    for (int i = 0; i < MAX_TEXTURES; i++) {
        d3d.bound_srv_slots[i] = -1;
        d3d.bound_sampler_slots[i] = -1;
    }
}

static void gfx_d3d12_load_shader(struct ShaderProgram *new_prg) {
    if ((struct ShaderProgram *)(d3d.shader_program) != new_prg) {
        gfx_d3d12_invalidate_bound_textures();
    }
    d3d.shader_program = (struct ShaderProgramD3D12 *)(new_prg);
}

static void gfx_d3d12_free_shader_program_contents(struct ShaderProgramD3D12 *prg) {
    for (int i = 0; i < D3D12_PSO_VARIANT_COUNT; i++) {
        if (prg->pso[i] != nullptr) { prg->pso[i]->Release(); }
    }
    if (prg->vs_blob != nullptr) { prg->vs_blob->Release(); }
    if (prg->ps_blob != nullptr) { prg->ps_blob->Release(); }
    gfx_destroy_shader(prg->vertexShader);
    gfx_destroy_shader(prg->fragmentShader);
    *prg = {};
    d3d.last_pso = nullptr;
}

static void gfx_d3d12_remove_shaders(void) {
    for (int i = 0; i < MAX_FRAME_PASSES; i++) {
        for (int j = 0; j < CC_MAX_SHADERS; j++) {
            gfx_d3d12_free_shader_program_contents(&d3d.shader_program_pool[i][j]);
        }
        d3d.shader_program_pool_index[i] = 0;
        d3d.shader_program_pool_size[i] = 0;
        gfx_d3d12_free_shader_program_contents(&d3d.post_process_shader_program_pool[i]);
    }

    d3d.shader_program = nullptr;
}

static ID3D12PipelineState *gfx_d3d12_get_or_create_pso(struct ShaderProgramD3D12 *prg) {
    int variant = gfx_d3d12_pso_variant_index();
    if (prg->pso[variant] != nullptr) { return prg->pso[variant]; }
    if (prg->vs_blob == nullptr || prg->ps_blob == nullptr) { return nullptr; }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = d3d.root_signature.Get();
    psoDesc.VS = { prg->vs_blob->GetBufferPointer(), prg->vs_blob->GetBufferSize() };
    psoDesc.PS = { prg->ps_blob->GetBufferPointer(), prg->ps_blob->GetBufferSize() };
    psoDesc.InputLayout = { prg->input_elements, prg->input_element_count };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;

    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.RasterizerState.SlopeScaledDepthBias = d3d.zmode_decal ? -2.0f : 0.0f;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    if (d3d.use_alpha) {
        D3D12_RENDER_TARGET_BLEND_DESC &rt = psoDesc.BlendState.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_ZERO;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    }
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.DepthStencilState.DepthEnable = d3d.depth_test ? TRUE : FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = d3d.depth_mask ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    ID3D12PipelineState *pso = nullptr;
    HRESULT hr = E_FAIL;

    wchar_t psoName[32];
    if (d3d.pipeline_library.Get() != nullptr) {
        char psoNameNarrow[32];
        snprintf(psoNameNarrow, sizeof(psoNameNarrow), "%016llX_%02d", (unsigned long long)(prg->pso_cache_key), variant);
        gfx_d3d12_widen_name(psoNameNarrow, psoName, ARRAYSIZE(psoName));
        hr = d3d.pipeline_library->LoadGraphicsPipeline(psoName, &psoDesc, IID_PPV_ARGS(&pso));
    }

    if (FAILED(hr)) {
        hr = d3d.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
        if (FAILED(hr)) {
            fprintf(stderr, "[D3D12] PSO creation failed (hr=0x%08lX, variant=%d)\n", hr, variant);
            return nullptr;
        }

        if (d3d.pipeline_library.Get() != nullptr && SUCCEEDED(d3d.pipeline_library->StorePipeline(psoName, pso))) {
            d3d.pipeline_library_dirty = true;
        }
    }

    prg->pso[variant] = pso;
    return pso;
}

static void gfx_d3d12_build_input_layout(struct ShaderProgramD3D12 *prg, struct Shader *vertexShader, struct ShaderInput *inputs) {
    prg->input_element_count = 0;
    for (int i = 0; i < MAX_SHADER_INPUTS; i++) {
        if (inputs[i].size == 0) { continue; }
        if (prg->input_element_count >= 16) { break; }

        DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        switch (vertexShader->shaderInputs[i].size) {
            case 1: format = DXGI_FORMAT_R32_FLOAT; break;
            case 2: format = DXGI_FORMAT_R32G32_FLOAT; break;
            case 3: format = DXGI_FORMAT_R32G32B32_FLOAT; break;
            case 4: format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
        }

        D3D12_INPUT_ELEMENT_DESC &elem = prg->input_elements[prg->input_element_count++];
        elem.SemanticName = "TEXCOORD";
        elem.SemanticIndex = (UINT)(vertexShader->shaderInputs[i].location);
        elem.Format = format;
        elem.InputSlot = 0;
        elem.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        elem.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        elem.InstanceDataStepRate = 0;
    }
}

static void gfx_d3d12_alloc_shaders(struct Shader **vertexShader, struct Shader **fragmentShader) {
    *vertexShader = (struct Shader *)(calloc(1, sizeof(struct Shader)));
    *fragmentShader = (struct Shader *)(calloc(1, sizeof(struct Shader)));
    if (!*vertexShader || !*fragmentShader) {
        sys_fatal("Failed to allocate shaders, ran out of memory!");
    }
}

static ID3DBlob *gfx_d3d12_compile_stage(struct Shader *shader, const char *target, const char *errorCaption) {
    char *hlsl = nullptr;
    gfx_convert_spirv_to_hlsl(&hlsl, shader);

    ID3DBlob *blob = nullptr;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompile(hlsl, strlen(hlsl), nullptr, nullptr, nullptr, "main", target, D3D12_COMPILE_FLAGS, 0, &blob, errorBlob.GetAddressOf());
    free(hlsl);

    if (FAILED(hr)) {
        const char *message = (errorBlob.Get() != nullptr) ? (const char *)(errorBlob->GetBufferPointer()) : "D3DCompile failed.";
        MessageBox(gfx_window_dxgi_get_h_wnd(), message, errorCaption, MB_OK | MB_ICONERROR);
        throw hr;
    }
    return blob;
}

static u8 gfx_d3d12_count_input_floats(struct ShaderInput *inputs) {
    size_t num_floats = 0;
    for (int i = 0; i < MAX_SHADER_INPUTS; i++) { num_floats += inputs[i].size; }
    return (u8)(num_floats);
}

static bool gfx_d3d12_block_declares_uniform(struct ShaderUniformBlock *uniformBlock, const char *name) {
    for (int i = 0; i < MAX_SHADER_UNIFORMS; i++) {
        struct ShaderUniform *uniform = &uniformBlock->uniforms[i];
        if (uniform->size == 0) { break; }
        if (strcmp(uniform->name, name) == 0) { return true; }
    }
    return false;
}

static bool gfx_d3d12_program_declares_matrices(struct Shader *vertexShader, struct Shader *fragmentShader) {
    static const char *const sMatrixNames[] = {
        "uModelViewProjectionMatrix", "uModelViewMatrix", "uModelMatrix", "uViewMatrix", "uProjectionMatrix"
    };

    for (size_t i = 0; i < sizeof(sMatrixNames) / sizeof(sMatrixNames[0]); i++) {
        if (gfx_d3d12_block_declares_uniform(&vertexShader->uniformBlocks[0], sMatrixNames[i])) { return true; }
        if (gfx_d3d12_block_declares_uniform(&fragmentShader->uniformBlocks[0], sMatrixNames[i])) { return true; }
    }
    return false;
}

static u64 gfx_d3d12_hash_blobs(ID3DBlob *vsBlob, ID3DBlob *psBlob) {
    u64 hash = 14695981039346656037ull;

    ID3DBlob *const blobs[2] = { vsBlob, psBlob };
    for (int i = 0; i < 2; i++) {
        if (blobs[i] == nullptr) { continue; }
        const u8 *bytes = (const u8 *)(blobs[i]->GetBufferPointer());
        SIZE_T size = blobs[i]->GetBufferSize();
        for (SIZE_T j = 0; j < size; j++) {
            hash ^= bytes[j];
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

static void gfx_d3d12_build_program(struct ShaderProgramD3D12 *prg, struct Shader *vertexShader, struct Shader *fragmentShader,
                                    struct ShaderInput *inputs, const char *vsErrorCaption, const char *psErrorCaption) {
    prg->vs_blob = gfx_d3d12_compile_stage(vertexShader, "vs_5_0", vsErrorCaption);
    prg->ps_blob = gfx_d3d12_compile_stage(fragmentShader, "ps_5_0", psErrorCaption);
    prg->pso_cache_key = gfx_d3d12_hash_blobs(prg->vs_blob, prg->ps_blob);
    prg->vertexShader = vertexShader;
    prg->fragmentShader = fragmentShader;
    prg->num_floats = gfx_d3d12_count_input_floats(inputs);
    prg->uses_matrices = gfx_d3d12_program_declares_matrices(vertexShader, fragmentShader);
    gfx_d3d12_build_input_layout(prg, vertexShader, inputs);
}

static struct ShaderProgram *gfx_d3d12_create_and_load_new_shader(struct ColorCombiner *cc) {
    CCFeatures cc_features = { 0 };
    gfx_cc_get_features(cc, &cc_features);

    struct Shader *vertexShader = nullptr;
    struct Shader *fragmentShader = nullptr;
    gfx_d3d12_alloc_shaders(&vertexShader, &fragmentShader);

    gfx_generate_vertex_and_fragment_shader_from_cc(vertexShader, fragmentShader, cc, nullptr, nullptr);

    int framePassIndex = gCurrentFramePassIndex + 1;

    struct ShaderProgramD3D12 *prg = &d3d.shader_program_pool[framePassIndex][d3d.shader_program_pool_index[framePassIndex]];
    gfx_d3d12_free_shader_program_contents(prg);
    d3d.shader_program_pool_index[framePassIndex] = (d3d.shader_program_pool_index[framePassIndex] + 1) % CC_MAX_SHADERS;
    if (d3d.shader_program_pool_size[framePassIndex] < CC_MAX_SHADERS) { d3d.shader_program_pool_size[framePassIndex]++; }

    gfx_d3d12_build_program(prg, vertexShader, fragmentShader, gShaderInputs, "Vertex Shader Error", "Pixel Shader Error");

    prg->hash = cc->hash;
    prg->num_inputs = cc_features.num_inputs;
    prg->used_textures[0] = cc_features.used_textures[0];
    prg->used_textures[1] = cc_features.used_textures[1];
    prg->used_fog = cc->cm.use_fog;

    return (struct ShaderProgram *)(d3d.shader_program = prg);
}

static struct ShaderProgram *gfx_d3d12_create_or_load_post_process_shader(void) {
    int framePassIndex = gCurrentFramePassIndex + 1;
    struct ShaderProgramD3D12 *prg = &d3d.post_process_shader_program_pool[framePassIndex];

    if (prg->vs_blob == nullptr) {
        struct Shader *vertexShader = nullptr;
        struct Shader *fragmentShader = nullptr;
        gfx_d3d12_alloc_shaders(&vertexShader, &fragmentShader);
        gfx_generate_post_process_vertex_and_fragment_shader(vertexShader, fragmentShader, nullptr, nullptr);

        gfx_d3d12_build_program(prg, vertexShader, fragmentShader, gPostProcessShaderInputs, "Post-Process VS Error", "Post-Process PS Error");

        prg->hash = (u64)(framePassIndex);
        prg->num_inputs = (u8)(prg->input_element_count);
        prg->used_textures[0] = true;
        prg->used_textures[1] = false;
        prg->used_fog = false;
    }

    d3d.shader_program = prg;
    return (struct ShaderProgram *)(prg);
}

static struct ShaderProgram *gfx_d3d12_lookup_shader(struct ColorCombiner *cc) {
    int framePassIndex = gCurrentFramePassIndex + 1;
    if (framePassIndex < 0 || framePassIndex >= MAX_FRAME_PASSES) { return nullptr; }
    for (size_t i = 0; i < d3d.shader_program_pool_size[framePassIndex]; i++) {
        if (d3d.shader_program_pool[framePassIndex][i].hash == cc->hash) {
            return (struct ShaderProgram *)(&d3d.shader_program_pool[framePassIndex][i]);
        }
    }
    return nullptr;
}

static struct ShaderProgram *gfx_d3d12_lookup_shader_using_index(u8 shaderIndex, u8 framePassIndex) {
    framePassIndex++;
    if (framePassIndex >= MAX_FRAME_PASSES) { return nullptr; }
    if (shaderIndex >= d3d.shader_program_pool_size[framePassIndex]) { return nullptr; }
    return (struct ShaderProgram *)(&d3d.shader_program_pool[framePassIndex][shaderIndex]);
}

static void gfx_d3d12_shader_get_info(struct ShaderProgram *prg, u8 *num_inputs, bool used_textures[2]) {
    struct ShaderProgramD3D12 *p = (struct ShaderProgramD3D12 *)(prg);
    *num_inputs = p->num_inputs;
    used_textures[0] = p->used_textures[0];
    used_textures[1] = p->used_textures[1];
}

static void gfx_d3d12_delete_framebuffer(struct FramePass *framePass) {
    gfx_d3d12_retire_object(framePass->d3dRtv);     framePass->d3dRtv = nullptr;
    gfx_d3d12_retire_object(framePass->d3dDsv);     framePass->d3dDsv = nullptr;
    gfx_d3d12_retire_object(d3d.framebuffer_color[gfx_d3d12_frame_pass_slot(framePass)]);
    d3d.framebuffer_color[gfx_d3d12_frame_pass_slot(framePass)] = nullptr;
    gfx_d3d12_retire_object(framePass->d3dSrv);     framePass->d3dSrv = nullptr;
    d3d.framebuffer_is_shader_resource[gfx_d3d12_frame_pass_slot(framePass)] = false;
    framePass->passTexture = 0;
    framePass->fbo = 0;
}

static void gfx_d3d12_create_framebuffer(struct FramePass *framePass) {
    u32 viewportWidth, viewportHeight;
    gfx_get_frame_pass_viewport_dimensions(framePass, &viewportWidth, &viewportHeight);
    if (viewportWidth == 0 || viewportHeight == 0) { return; }

    gfx_d3d12_delete_framebuffer(framePass);

    int slot = gfx_d3d12_frame_pass_slot(framePass);
    char debugName[64];

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[3] = 1.0f;

    snprintf(debugName, sizeof(debugName), "D3D12_FramebufferColor_slot%d", slot);
    ID3D12Resource *colorTexture = gfx_d3d12_create_texture(viewportWidth, viewportHeight, DXGI_FORMAT_R8G8B8A8_UNORM,
                                                            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                            &clearValue, debugName);
    if (colorTexture == nullptr) { return; }

    ID3D12DescriptorHeap *rtvHeap = gfx_d3d12_create_descriptor_heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    if (rtvHeap == nullptr) {
        colorTexture->Release();
        return;
    }
    d3d.device->CreateRenderTargetView(colorTexture, nullptr, rtvHeap->GetCPUDescriptorHandleForHeapStart());

    snprintf(debugName, sizeof(debugName), "D3D12_FramebufferDepth_slot%d", slot);
    ID3D12Resource *depthTexture = gfx_d3d12_create_depth_texture(viewportWidth, viewportHeight, debugName);
    ID3D12DescriptorHeap *dsvHeap = (depthTexture != nullptr) ? gfx_d3d12_create_descriptor_heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false) : nullptr;
    if (dsvHeap != nullptr) {
        d3d.device->CreateDepthStencilView(depthTexture, nullptr, dsvHeap->GetCPUDescriptorHandleForHeapStart());
    } else if (depthTexture != nullptr) {
        depthTexture->Release();
        depthTexture = nullptr;
    }

    gfx_d3d12_create_srv(colorTexture, D3D12_MAX_GAME_TEXTURES + slot);

    d3d.framebuffer_color[slot] = colorTexture;
    framePass->d3dRtv = rtvHeap;
    framePass->d3dDsv = dsvHeap;
    framePass->d3dSrv = depthTexture;
    framePass->passTexture = (u64)(slot + 1);
    framePass->width = viewportWidth;
    framePass->height = viewportHeight;
    framePass->fbo = 1;
    d3d.framebuffer_is_shader_resource[slot] = false;
}

static void gfx_d3d12_prepare_framebuffer_for_render(struct FramePass *framePass) {
    int slot = gfx_d3d12_frame_pass_slot(framePass);
    if (!d3d.framebuffer_is_shader_resource[slot]) { return; }
    gfx_d3d12_transition(d3d.framebuffer_color[slot], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    d3d.framebuffer_is_shader_resource[slot] = false;
}

static void gfx_d3d12_prepare_framebuffer_for_sampling(struct FramePass *framePass) {
    int slot = gfx_d3d12_frame_pass_slot(framePass);
    if (d3d.framebuffer_is_shader_resource[slot]) { return; }
    gfx_d3d12_transition(d3d.framebuffer_color[slot], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    d3d.framebuffer_is_shader_resource[slot] = true;
}

static void gfx_d3d12_begin_frame_if_needed(void) {
    if (d3d.command_list_recording) { return; }

    d3d.current_slot = (d3d.current_slot + 1) % D3D12_FRAME_BUFFERS_COUNT;
    D3D12FrameSlot &slot = d3d.frame_slots[d3d.current_slot];

    gfx_d3d12_wait_for_fence_value(slot.fence_value);
    gfx_d3d12_release_pending(d3d.fence->GetCompletedValue());

    ThrowIfFailed(slot.allocator->Reset());
    ThrowIfFailed(d3d.command_list->Reset(slot.allocator, nullptr));
    gfx_d3d12_pool_reset(slot.vertex_pool);
    gfx_d3d12_pool_reset(slot.uniform_pool);
    gfx_d3d12_pool_reset(slot.texture_pool);

    gfx_d3d12_invalidate_bound_textures();
    d3d.last_pso = nullptr;
    d3d.bound_vertex_block = 0;
    d3d.bound_vertex_stride = 0;
    d3d.command_list_generation++;
    memset(d3d.bound_cbv, 0, sizeof(d3d.bound_cbv));

    ID3D12DescriptorHeap *heaps[] = { d3d.srv_heap.Get(), d3d.sampler_heap.Get() };
    d3d.command_list->SetDescriptorHeaps(2, heaps);
    d3d.command_list->SetGraphicsRootSignature(d3d.root_signature.Get());
    d3d.command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_PASS_SRV, gfx_d3d12_srv_gpu_handle(D3D12_MAX_GAME_TEXTURES));
    d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_PASS_SAMPLER, gfx_d3d12_sampler_gpu_handle(D3D12_SAMPLER_HEAP_PASS_OFFSET));

    d3d.command_list_recording = true;
}

static void gfx_d3d12_set_viewport_and_scissor(u32 width, u32 height) {
    D3D12_VIEWPORT viewport = {};
    viewport.Width = (float)(width);
    viewport.Height = (float)(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    d3d.command_list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {};
    scissor.right = (LONG)(width);
    scissor.bottom = (LONG)(height);
    d3d.command_list->RSSetScissorRects(1, &scissor);
}

static void gfx_d3d12_set_framebuffer(struct FramePass *framePass) {
    if (d3d.framebuffer_color[gfx_d3d12_frame_pass_slot(framePass)] == nullptr) { return; }

    gfx_d3d12_begin_frame_if_needed();
    gfx_d3d12_prepare_framebuffer_for_render(framePass);

    d3d.current_rtv = ((ID3D12DescriptorHeap *)(framePass->d3dRtv))->GetCPUDescriptorHandleForHeapStart();
    bool hasDepth = framePass->d3dDsv != nullptr;
    if (hasDepth) { d3d.current_dsv = ((ID3D12DescriptorHeap *)(framePass->d3dDsv))->GetCPUDescriptorHandleForHeapStart(); }
    d3d.current_target_is_backbuffer = false;
    d3d.current_target_has_depth = hasDepth;

    d3d.command_list->OMSetRenderTargets(1, &d3d.current_rtv, FALSE, hasDepth ? &d3d.current_dsv : nullptr);
    gfx_d3d12_set_viewport_and_scissor(framePass->width, framePass->height);
}

static void gfx_d3d12_reset_framebuffer(void) {
    gfx_d3d12_begin_frame_if_needed();

    u32 windowWidth, windowHeight;
    gfx_get_dimensions(&windowWidth, &windowHeight);

    d3d.current_backbuffer_index = d3d.swap_chain->GetCurrentBackBufferIndex();
    gfx_d3d12_ensure_backbuffer_render_target();

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += (SIZE_T)(d3d.current_backbuffer_index) * d3d.rtv_descriptor_size;
    d3d.current_rtv = rtv;
    d3d.current_dsv = d3d.dsv_heap->GetCPUDescriptorHandleForHeapStart();
    d3d.current_target_is_backbuffer = true;
    d3d.current_target_has_depth = true;

    d3d.command_list->OMSetRenderTargets(1, &d3d.current_rtv, FALSE, &d3d.current_dsv);
    gfx_d3d12_set_viewport_and_scissor(windowWidth, windowHeight);
}

static size_t gfx_d3d12_get_uniform_buffer_size(enum ShaderStage stage, int bufferIndex) {
    if (d3d.shader_program == nullptr) { return 0; }
    if (bufferIndex < 0 || bufferIndex >= MAX_UNIFORM_BLOCKS) { return 0; }

    struct Shader *shader = nullptr;
    if (stage == SHADER_STAGE_VERTEX) {
        shader = d3d.shader_program->vertexShader;
    } else if (stage == SHADER_STAGE_FRAGMENT) {
        shader = d3d.shader_program->fragmentShader;
    } else {
        return 0;
    }

    return shader->uniformBlocks[bufferIndex].size;
}

static void gfx_d3d12_set_uniform_buffer(enum ShaderStage stage, const char *name) {
    if (d3d.shader_program == nullptr) { return; }

    struct Shader *shader = nullptr;
    int *destination = nullptr;
    if (stage == SHADER_STAGE_VERTEX) {
        shader = d3d.shader_program->vertexShader;
        destination = &gSelectedVertexUniformBuffer;
    } else if (stage == SHADER_STAGE_FRAGMENT) {
        shader = d3d.shader_program->fragmentShader;
        destination = &gSelectedFragmentUniformBuffer;
    } else {
        return;
    }

    for (int i = 0; i < MAX_UNIFORM_BLOCKS; i++) {
        struct ShaderUniformBlock *uniformBlock = &shader->uniformBlocks[i];
        if (strcmp(uniformBlock->name, name) == 0) {
            *destination = i;
        }
    }
}

static void gfx_d3d12_set_uniform_for_specific_shader(struct ShaderUniformBlock *uniformBlock, const char *name, const void *data, u32 numElements) {
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

static void gfx_d3d12_set_uniform(struct ShaderProgram *prg_, const char *name, UNUSED ShaderUniformType type, const void *data, u32 numElements) {
    struct ShaderProgramD3D12 *prg = (struct ShaderProgramD3D12 *)(prg_);
    if (prg == nullptr) {
        if (d3d.shader_program == nullptr) { return; }
        prg = d3d.shader_program;
    }

    if (gfx_shader_stage_is(SHADER_STAGE_VERTEX)) {
        gfx_d3d12_set_uniform_for_specific_shader(&prg->vertexShader->uniformBlocks[gSelectedVertexUniformBuffer], name, data, numElements);
    }
    if (gfx_shader_stage_is(SHADER_STAGE_FRAGMENT)) {
        gfx_d3d12_set_uniform_for_specific_shader(&prg->fragmentShader->uniformBlocks[gSelectedFragmentUniformBuffer], name, data, numElements);
    }
}

static u32 gfx_d3d12_new_texture(UNUSED const char *name) {
    d3d.textures.emplace_back();
    D3D12TextureData &tex = d3d.textures.back();
    tex.resource = nullptr;
    tex.sampler_slot = -1;
    tex.srv_slot = D3D12_DEFAULT_SRV_SLOT;
    tex.linear_filtering = false;
    tex.cms = tex.cmt = 0;
    tex.width = tex.height = 0;
    return (u32)(d3d.textures.size() - 1);
}

static void gfx_d3d12_select_texture(int tile, u32 texture_id) {
    d3d.current_tile = tile;
    d3d.current_texture_ids[tile] = texture_id;
}

static void gfx_d3d12_bind_texture_raw(int tile, u64 texture_id) {
    if (tile < D3D12_PASS_TEXTURE_REGISTER_BASE) { return; }

    int slot = (int)(texture_id) - 1;
    if (slot < 0 || slot >= MAX_FRAME_PASSES) { return; }

    int registerOffset = tile - D3D12_PASS_TEXTURE_REGISTER_BASE;

    struct FramePass *ownerFramePass = (slot == 0) ? &gDefaultGeoFramePass : &gFramePasses[slot - 1];
    if (d3d.framebuffer_color[slot] != nullptr) {
        gfx_d3d12_prepare_framebuffer_for_sampling(ownerFramePass);
    }

    d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_PASS_SRV, gfx_d3d12_srv_gpu_handle(D3D12_MAX_GAME_TEXTURES + slot - registerOffset));

    bool wantLinear = (ownerFramePass->passFilter == PASS_FILTER_LINEAR);
    if (d3d.pass_sampler_linear[slot] != wantLinear) {
        gfx_d3d12_create_sampler(D3D12_SAMPLER_HEAP_PASS_OFFSET + slot, wantLinear, G_TX_CLAMP, G_TX_CLAMP);
        d3d.pass_sampler_linear[slot] = wantLinear;
    }
    d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_PASS_SAMPLER, gfx_d3d12_sampler_gpu_handle(D3D12_SAMPLER_HEAP_PASS_OFFSET + slot - registerOffset));
}

static void gfx_d3d12_upload_texture(const u8 *rgba32_buf, int width, int height) {
    u32 textureId = d3d.current_texture_ids[d3d.current_tile];
    D3D12TextureData &texture_data = d3d.textures[textureId];

    const bool canReuse = (texture_data.resource != nullptr) &&
        (texture_data.width == (u32)(width)) && (texture_data.height == (u32)(height));

    ID3D12Resource *texture = nullptr;
    if (canReuse) {
        texture = texture_data.resource;
        gfx_d3d12_transition(texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    } else {
        texture = gfx_d3d12_create_texture((u32)(width), (u32)(height), DXGI_FORMAT_R8G8B8A8_UNORM,
                                           D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, nullptr);
        if (texture == nullptr) { return; }
    }

    if (!gfx_d3d12_upload_texture_data(texture, rgba32_buf, (u32)(width))) {
        if (canReuse) {
            gfx_d3d12_transition(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        } else {
            texture->Release();
        }
        return;
    }

    if (canReuse) {
        return;
    }

    // A fresh slot, never the one this texture was already using.
    int previousSlot = texture_data.srv_slot;
    texture_data.srv_slot = gfx_d3d12_acquire_srv_slot();
    gfx_d3d12_create_srv(texture, texture_data.srv_slot);
    gfx_d3d12_retire_srv_slot(previousSlot);

    gfx_d3d12_retire_object(texture_data.resource);
    texture_data.resource = texture;
    texture_data.width = (u32)(width);
    texture_data.height = (u32)(height);
}

static void gfx_d3d12_set_sampler_parameters(int tile, bool linear_filter, u32 cms, u32 cmt) {
    D3D12TextureData &texture_data = d3d.textures[d3d.current_texture_ids[tile]];
    texture_data.linear_filtering = linear_filter;
    texture_data.cms = cms;
    texture_data.cmt = cmt;

    for (int i = 0; i < D3D12_MAX_SAMPLER_VARIANTS; i++) {
        D3D12SamplerVariant &variant = d3d.sampler_variants[i];
        if (variant.used && variant.linear_filter == linear_filter && variant.cms == cms && variant.cmt == cmt) {
            texture_data.sampler_slot = i;
            return;
        }
    }

    for (int i = 0; i < D3D12_MAX_SAMPLER_VARIANTS; i++) {
        D3D12SamplerVariant &variant = d3d.sampler_variants[i];
        if (variant.used) { continue; }

        gfx_d3d12_create_sampler(i, linear_filter, cms, cmt);
        variant.used = true;
        variant.linear_filter = linear_filter;
        variant.cms = cms;
        variant.cmt = cmt;

        texture_data.sampler_slot = i;
        return;
    }

    fprintf(stderr, "[D3D12] sampler variant cache exhausted (%d slots)\n", D3D12_MAX_SAMPLER_VARIANTS);
    texture_data.sampler_slot = 0;
}

static void gfx_d3d12_set_depth_test(bool depth_test) {
    d3d.depth_test = depth_test;
}

static void gfx_d3d12_set_depth_mask(bool depth_mask) {
    d3d.depth_mask = depth_mask;
}

static void gfx_d3d12_set_zmode_decal(bool zmode_decal) {
    d3d.zmode_decal = zmode_decal;
}

static void gfx_d3d12_set_use_alpha(bool use_alpha) {
    d3d.use_alpha = use_alpha;
}

static void gfx_d3d12_set_vsync(UNUSED bool enabled) {
}

static void gfx_d3d12_set_viewport(int x, int y, int width, int height) {
    u32 viewportHeight;
    gfx_get_frame_pass_viewport_dimensions(gfx_get_current_frame_pass(), NULL, &viewportHeight);

    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = (float)(x);
    viewport.TopLeftY = (float)((int)(viewportHeight) - y - height);
    viewport.Width = (float)(width);
    viewport.Height = (float)(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    d3d.command_list->RSSetViewports(1, &viewport);
}

static void gfx_d3d12_set_scissor(int x, int y, int width, int height) {
    u32 viewportHeight;
    gfx_get_frame_pass_viewport_dimensions(gfx_get_current_frame_pass(), NULL, &viewportHeight);

    D3D12_RECT rect = {};
    rect.left = x;
    rect.top = (LONG)(viewportHeight) - y - height;
    rect.right = x + width;
    rect.bottom = (LONG)(viewportHeight) - y;
    d3d.command_list->RSSetScissorRects(1, &rect);
}

static void gfx_d3d12_bind_draw_textures(void) {
    for (int i = 0; i < MAX_TEXTURES; i++) {
        if (!d3d.shader_program->used_textures[i]) { continue; }

        D3D12TextureData &texture_data = d3d.textures[d3d.current_texture_ids[i]];
        int srvSlot = texture_data.srv_slot;
        int samplerSlot = (texture_data.sampler_slot >= 0) ? texture_data.sampler_slot : 0;

        bool srvChanged = (d3d.bound_srv_slots[i] != srvSlot);
        bool samplerChanged = (d3d.bound_sampler_slots[i] != samplerSlot);
        if (!srvChanged && !samplerChanged) { continue; }

        d3d.bound_srv_slots[i] = srvSlot;
        d3d.bound_sampler_slots[i] = samplerSlot;

        if (srvChanged) {
            d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_SRV_BASE + i, gfx_d3d12_srv_gpu_handle(srvSlot));
        }
        if (samplerChanged) {
            d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_SAMPLER_BASE + i, gfx_d3d12_sampler_gpu_handle(samplerSlot));
        }

        static const char *const sSizeNames[2] = { "uTex0Size", "uTex1Size" };
        static const char *const sFilterNames[2] = { "uTex0Filter", "uTex1Filter" };

        float texSize[2] = { (float)(texture_data.width), (float)(texture_data.height) };
        gfx_d3d12_set_uniform(NULL, sSizeNames[i], SHADER_UNIFORM_TYPE_VEC2, texSize, 1);

        u32 isLinear = texture_data.linear_filtering ? 1 : 0;
        gfx_d3d12_set_uniform(NULL, sFilterNames[i], SHADER_UNIFORM_TYPE_INT, &isLinear, 1);
    }
}

static bool gfx_d3d12_upload_uniform_blocks(void) {
    D3D12UploadPool &pool = d3d.frame_slots[d3d.current_slot].uniform_pool;

    struct Shader *const shaders[2] = { d3d.shader_program->vertexShader, d3d.shader_program->fragmentShader };
    const int rootParamBase[2] = { D3D12_ROOT_PARAM_VS_CBV_BASE, D3D12_ROOT_PARAM_FS_CBV_BASE };

    for (int stage = 0; stage < 2; stage++) {
        for (int i = 0; i < shaders[stage]->uniformBlockCount; i++) {
            struct ShaderUniformBlock *block = &shaders[stage]->uniformBlocks[i];
            if (block->size == 0) { continue; }

            u32 location = block->location;
            if (location >= MAX_UNIFORM_BLOCKS) { continue; }

            if (block->dirty || block->d3d12Generation != d3d.command_list_generation) {
                void *cpuPtr = nullptr;
                D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = 0;
                if (!gfx_d3d12_pool_alloc(pool, block->size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, &cpuPtr, &gpuAddr)) { return false; }

                memcpy(cpuPtr, block->buffer, block->size);
                block->d3d12GpuAddr = (u64)(gpuAddr);
                block->d3d12Generation = d3d.command_list_generation;
                block->dirty = false;
            }

            if (d3d.bound_cbv[stage][location] != block->d3d12GpuAddr) {
                d3d.bound_cbv[stage][location] = block->d3d12GpuAddr;
                d3d.command_list->SetGraphicsRootConstantBufferView(rootParamBase[stage] + location, (D3D12_GPU_VIRTUAL_ADDRESS)(block->d3d12GpuAddr));
            }
        }
    }
    return true;
}

static void gfx_d3d12_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    if (d3d.shader_program == nullptr || buf_vbo_num_tris == 0) { return; }

    ID3D12PipelineState *pso = gfx_d3d12_get_or_create_pso(d3d.shader_program);
    if (pso == nullptr) { return; }

    gfx_d3d12_bind_draw_textures();

    if (d3d.shader_program->uses_matrices) {
        gfx_update_matrices();
    }
    if (d3d.shader_program->used_fog) {
        gfx_update_fog_uniforms();
    }
    smlua_call_event_hooks(HOOK_ON_DRAW_TRIANGLE);

    if (!gfx_d3d12_upload_uniform_blocks()) { return; }

    const UINT stride = (UINT)(d3d.shader_program->num_floats * sizeof(float));
    if (stride == 0) { return; }

    size_t vertexBytes = buf_vbo_len * sizeof(float);
    D3D12UploadPool &vertexPool = d3d.frame_slots[d3d.current_slot].vertex_pool;
    void *vertexCpuPtr = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS vertexGpuAddr = 0;

    if (!gfx_d3d12_pool_alloc(vertexPool, vertexBytes, stride, &vertexCpuPtr, &vertexGpuAddr)) {
        fprintf(stderr, "[D3D12] could not allocate %zu bytes of vertex upload space, skipping draw\n", vertexBytes);
        return;
    }
    memcpy(vertexCpuPtr, buf_vbo, vertexBytes);

    if (d3d.last_pso != pso) {
        d3d.last_pso = pso;
        d3d.command_list->SetPipelineState(pso);
    }

    const D3D12UploadArena &vertexBlock = vertexPool.blocks[vertexPool.current];
    if (d3d.bound_vertex_block != vertexBlock.gpu_base || d3d.bound_vertex_stride != stride) {
        d3d.bound_vertex_block = vertexBlock.gpu_base;
        d3d.bound_vertex_stride = stride;

        D3D12_VERTEX_BUFFER_VIEW vbView = {};
        vbView.BufferLocation = vertexBlock.gpu_base;
        vbView.SizeInBytes = (UINT)(vertexBlock.size - (vertexBlock.size % stride));
        vbView.StrideInBytes = stride;
        d3d.command_list->IASetVertexBuffers(0, 1, &vbView);
    }

    const UINT startVertex = (UINT)((vertexGpuAddr - vertexBlock.gpu_base) / stride);
    d3d.command_list->DrawInstanced((UINT)(buf_vbo_num_tris * 3), 1, startVertex, 0);
}

// Drops any half-recorded frame, blocks the CPU until the GPU finishes all submitted work
static void gfx_d3d12_idle_and_abandon_frame(void) {
    if (d3d.command_list_recording) {
        d3d.command_list->Close();
        d3d.command_list_recording = false;
        d3d.backbuffer_is_render_target = false;
    }

    UINT64 idleValue = d3d.next_fence_value++;
    if (SUCCEEDED(d3d.command_queue->Signal(d3d.fence.Get(), idleValue))) {
        gfx_d3d12_wait_for_fence_value(idleValue);
    }

    for (int i = 0; i < D3D12_FRAME_BUFFERS_COUNT; i++) {
        gfx_d3d12_wait_for_fence_value(d3d.frame_slots[i].fence_value);
    }

    d3d.command_list_generation++;
    memset(d3d.bound_cbv, 0, sizeof(d3d.bound_cbv));
    gfx_d3d12_invalidate_bound_textures();
    d3d.last_pso = nullptr;
    d3d.bound_vertex_block = 0;
    d3d.bound_vertex_stride = 0;
}

static void gfx_d3d12_on_resize(void) {
    for (int i = 0; i < MAX_CUSTOM_FRAME_PASSES; i++) {
        struct FramePass *framePass = &gFramePasses[i];
        if (!framePass->active) { continue; }
        if (framePass->width == 0 || framePass->height == 0) {
            gfx_d3d12_delete_framebuffer(framePass);
        }
    }

    gfx_d3d12_idle_and_abandon_frame();
    gfx_d3d12_create_render_target_views(true);

    ThrowIfFailed(d3d.frame_slots[d3d.current_slot].allocator->Reset());
    ThrowIfFailed(d3d.command_list->Reset(d3d.frame_slots[d3d.current_slot].allocator, nullptr));
    ThrowIfFailed(d3d.command_list->Close());
}

static void gfx_d3d12_start_frame(void) {
    gfx_d3d12_begin_frame_if_needed();

    struct FramePass *framePass = gfx_get_current_frame_pass();

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (framePass != nullptr) {
        for (int i = 0; i < 4; i++) { clearColor[i] = framePass->clearColor[i] / 255.0f; }
    }

    d3d.command_list->ClearRenderTargetView(d3d.current_rtv, clearColor, 0, nullptr);
    if (d3d.current_target_has_depth) {
        d3d.command_list->ClearDepthStencilView(d3d.current_dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }
}

static void gfx_d3d12_end_frame(void) {
    if (d3d.current_target_is_backbuffer) {
        gfx_d3d12_ensure_backbuffer_present();
    }

    ThrowIfFailed(d3d.command_list->Close());
    ID3D12CommandList *lists[] = { d3d.command_list.Get() };
    d3d.command_queue->ExecuteCommandLists(1, lists);

    UINT64 signalValue = d3d.next_fence_value++;
    ThrowIfFailed(d3d.command_queue->Signal(d3d.fence.Get(), signalValue));
    d3d.frame_slots[d3d.current_slot].fence_value = signalValue;

    d3d.command_list_recording = false;
}

static void gfx_d3d12_finish_render(void) {
}

static const char *gfx_d3d12_get_name(void) {
    return "DirectX 12";
}

static bool gfx_d3d12_is_legacy(void) {
    return false;
}

static void gfx_d3d12_shutdown(void) {
    if (d3d.command_list_recording) {
        d3d.command_list->Close();
        d3d.command_list_recording = false;
        d3d.backbuffer_is_render_target = false;
    }

    if (d3d.swap_chain.Get() != nullptr) {
        BOOL isFullscreen = FALSE;
        if (SUCCEEDED(d3d.swap_chain->GetFullscreenState(&isFullscreen, nullptr)) && isFullscreen) {
            d3d.swap_chain->SetFullscreenState(FALSE, nullptr);
        }
    }

    for (int i = 0; i < D3D12_FRAME_BUFFERS_COUNT; i++) {
        UINT64 value = d3d.frame_slots[i].fence_value;
        if (value == 0) { continue; }
        UINT64 completed = d3d.fence->GetCompletedValue();
        if (completed < value) {
            HRESULT hr = d3d.fence->SetEventOnCompletion(value, d3d.fence_event);
            if (FAILED(hr)) {
                fprintf(stderr, "gfx_d3d12_shutdown: SetEventOnCompletion failed for fence slot %d (value %llu, completed %llu): 0x%08X - skipping this slot's wait.\n",
                    i, (unsigned long long)value, (unsigned long long)completed, (unsigned int)hr);
                fflush(stderr);
                continue;
            }
            if (WaitForSingleObject(d3d.fence_event, 2000) != WAIT_OBJECT_0) {
                fprintf(stderr, "gfx_d3d12_shutdown: timed out waiting on fence slot %d (value %llu, completed %llu) - abandoning wait so the process can still exit.\n",
                    i, (unsigned long long)value, (unsigned long long)d3d.fence->GetCompletedValue());
                break;
            }
        }
    }

    if (d3d.command_queue.Get() != nullptr && d3d.fence.Get() != nullptr && d3d.fence_event != nullptr) {
        UINT64 drainValue = d3d.next_fence_value++;
        if (SUCCEEDED(d3d.command_queue->Signal(d3d.fence.Get(), drainValue))) {
            if (d3d.fence->GetCompletedValue() < drainValue) {
                if (SUCCEEDED(d3d.fence->SetEventOnCompletion(drainValue, d3d.fence_event))) {
                    if (WaitForSingleObject(d3d.fence_event, 2000) != WAIT_OBJECT_0) {
                        fprintf(stderr, "gfx_d3d12_shutdown: timed out draining the command queue (value %llu, completed %llu) - releasing anyway so the process can exit.\n",
                            (unsigned long long)drainValue, (unsigned long long)d3d.fence->GetCompletedValue());
                        fflush(stderr);
                    }
                }
            }
        }
    }

    gfx_d3d12_save_pipeline_library();
    d3d.pipeline_library.Reset();
    d3d.pipeline_library_blob.clear();

    gfx_d3d12_remove_shaders();

    gfx_d3d12_delete_framebuffer(&gDefaultGeoFramePass);
    for (int i = 0; i < MAX_CUSTOM_FRAME_PASSES; i++) {
        if (gFramePasses[i].active) { gfx_d3d12_delete_framebuffer(&gFramePasses[i]); }
    }

    for (size_t i = 0; i < d3d.textures.size(); i++) {
        if (d3d.textures[i].resource != nullptr) { d3d.textures[i].resource->Release(); }
    }
    d3d.textures.clear();

    gfx_d3d12_release_pending(UINT64_MAX);
    d3d.free_srv_slots.clear();

    for (int i = 0; i < D3D12_FRAME_BUFFERS_COUNT; i++) {
        D3D12FrameSlot &slot = d3d.frame_slots[i];
        gfx_d3d12_pool_destroy(slot.vertex_pool);
        gfx_d3d12_pool_destroy(slot.uniform_pool);
        gfx_d3d12_pool_destroy(slot.texture_pool);
        if (slot.allocator != nullptr) { slot.allocator->Release(); slot.allocator = nullptr; }
    }

    if (d3d.fence_event != nullptr) { CloseHandle(d3d.fence_event); d3d.fence_event = nullptr; }

    d3d.default_texture.Reset();
    for (int i = 0; i < D3D12_SWAP_CHAIN_BUFFERS; i++) { d3d.backbuffers[i].Reset(); }
    d3d.depth_buffer.Reset();
    d3d.rtv_heap.Reset();
    d3d.dsv_heap.Reset();
    d3d.srv_heap.Reset();
    d3d.sampler_heap.Reset();
    d3d.root_signature.Reset();
    d3d.command_list.Reset();
    d3d.fence.Reset();
    gfx_d3d12_drain_debug_messages("before releasing the swap chain");
    d3d.swap_chain.Reset();
    gfx_window_dxgi_release_swap_chain();
    gfx_d3d12_drain_debug_messages("after releasing the swap chain");
    d3d.command_queue.Reset();
    d3d.info_queue.Reset();
    d3d.device.Reset();
}

} // namespace

struct GfxRenderingAPI gfx_direct3d12_api = {
    gfx_d3d12_z_is_from_0_to_1,
    gfx_d3d12_unload_shader,
    gfx_d3d12_load_shader,
    gfx_d3d12_remove_shaders,
    gfx_d3d12_create_and_load_new_shader,
    gfx_d3d12_create_or_load_post_process_shader,
    gfx_d3d12_lookup_shader,
    gfx_d3d12_lookup_shader_using_index,
    gfx_d3d12_shader_get_info,
    gfx_d3d12_create_framebuffer,
    gfx_d3d12_delete_framebuffer,
    gfx_d3d12_set_framebuffer,
    gfx_d3d12_reset_framebuffer,
    gfx_d3d12_get_uniform_buffer_size,
    gfx_d3d12_set_uniform_buffer,
    gfx_d3d12_set_uniform,
    gfx_d3d12_new_texture,
    gfx_d3d12_select_texture,
    gfx_d3d12_bind_texture_raw,
    gfx_d3d12_upload_texture,
    gfx_d3d12_set_sampler_parameters,
    gfx_d3d12_set_depth_test,
    gfx_d3d12_set_depth_mask,
    gfx_d3d12_set_zmode_decal,
    gfx_d3d12_set_viewport,
    gfx_d3d12_set_scissor,
    gfx_d3d12_set_use_alpha,
    gfx_d3d12_set_vsync,
    gfx_d3d12_draw_triangles,
    gfx_d3d12_init,
    gfx_d3d12_on_resize,
    gfx_d3d12_start_frame,
    gfx_d3d12_end_frame,
    gfx_d3d12_finish_render,
    gfx_d3d12_get_name,
    gfx_d3d12_is_legacy,
    gfx_d3d12_shutdown,
};

#endif
