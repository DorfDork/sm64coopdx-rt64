#if defined(_WIN32)

#include <cstdio>
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

#define D3D12_FRAME_RING_SIZE 3
#define D3D12_VERTEX_ARENA_SIZE (16u * 1024u * 1024u)
#define D3D12_UNIFORM_ARENA_SIZE (8u * 1024u * 1024u)
#define D3D12_MAX_GAME_TEXTURES 8192
#define D3D12_MAX_SAMPLER_VARIANTS 32
#define D3D12_SRV_HEAP_SIZE (D3D12_MAX_GAME_TEXTURES + MAX_FRAME_PASSES)
#define D3D12_PSO_VARIANT_COUNT 8 // depth_test x depth_mask x zmode_decal, all bool

#define D3D12_PASS_TEXTURE_REGISTER_BASE 10
#define D3D12_PASS_SAMPLER_HEAP_BASE D3D12_MAX_SAMPLER_VARIANTS
#define D3D12_SAMPLER_HEAP_SIZE (D3D12_MAX_SAMPLER_VARIANTS + MAX_FRAME_PASSES)

#define D3D12_ROOT_PARAM_VS_CBV_BASE 0
#define D3D12_ROOT_PARAM_FS_CBV_BASE MAX_UNIFORM_BLOCKS
#define D3D12_ROOT_PARAM_SRV_BASE (2 * MAX_UNIFORM_BLOCKS)
#define D3D12_ROOT_PARAM_SAMPLER_BASE (D3D12_ROOT_PARAM_SRV_BASE + MAX_TEXTURES)
#define D3D12_ROOT_PARAM_PASS_SRV (D3D12_ROOT_PARAM_SAMPLER_BASE + MAX_TEXTURES)
#define D3D12_ROOT_PARAM_PASS_SAMPLER (D3D12_ROOT_PARAM_PASS_SRV + 1)
#define D3D12_ROOT_PARAM_COUNT (D3D12_ROOT_PARAM_PASS_SAMPLER + 1)

typedef HRESULT(WINAPI *PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob);

static void d3d12_set_name(ID3D12Object *object, const char *name) {
    if (object == nullptr || name == nullptr) { return; }
    wchar_t wideName[128];
    size_t i = 0;
    for (; (i < 127) && (name[i] != '\0'); i++) {
        wideName[i] = (wchar_t)((unsigned char)(name[i]));
    }
    wideName[i] = L'\0';
    object->SetName(wideName);
}

struct D3D12TextureData {
    ID3D12Resource *resource;
    bool linear_filtering;
    u32 cms, cmt;
    int sampler_slot;
    u32 width, height;
};

struct ShaderProgramD3D12 {
    ID3D12PipelineState *pso[D3D12_PSO_VARIANT_COUNT];
    ID3DBlob *vs_blob;
    ID3DBlob *ps_blob;
    D3D12_INPUT_ELEMENT_DESC input_elements[16];
    UINT input_element_count;
    bool use_alpha;

    struct Shader *vertexShader;
    struct Shader *fragmentShader;

    uint64_t hash;
    uint8_t num_inputs;
    uint8_t num_floats;
    bool used_textures[2];
    bool used_fog;
};

struct D3D12UploadArena {
    ID3D12Resource *resource;
    uint8_t *mapped;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_base;
    size_t size;
    size_t offset;
};

struct D3D12FrameSlot {
    ID3D12CommandAllocator *allocator;
    UINT64 fence_value;
    D3D12UploadArena vertex_arena;
    D3D12UploadArena uniform_arena;
};

struct D3D12SamplerVariant {
    bool used;
    bool linear_filter;
    u32 cms, cmt;
};

static struct {
    HMODULE d3d12_module;
    PFN_D3D12_CREATE_DEVICE D3D12CreateDevice;
    PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE D3D12SerializeVersionedRootSignature;

    HMODULE d3dcompiler_module;
    pD3DCompile D3DCompile;

    ComPtr<ID3D12Device> device;
    ComPtr<IDXGISwapChain3> swap_chain;
    ComPtr<ID3D12CommandQueue> command_queue;
    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12RootSignature> root_signature;

    ComPtr<ID3D12DescriptorHeap> rtv_heap;
    ComPtr<ID3D12DescriptorHeap> dsv_heap;
    ComPtr<ID3D12Resource> backbuffers[2];
    ComPtr<ID3D12Resource> depth_buffer;
    UINT rtv_descriptor_size;
    UINT dsv_descriptor_size;

    ComPtr<ID3D12DescriptorHeap> srv_heap;
    ComPtr<ID3D12DescriptorHeap> sampler_heap;
    UINT srv_descriptor_size;
    UINT sampler_descriptor_size;
    D3D12SamplerVariant sampler_variants[D3D12_MAX_SAMPLER_VARIANTS];
    ComPtr<ID3D12Resource> default_texture;

    bool pass_sampler_linear[MAX_FRAME_PASSES];

    ComPtr<ID3D12Fence> fence;
    HANDLE fence_event;
    UINT64 next_fence_value;

    D3D12FrameSlot frame_slots[D3D12_FRAME_RING_SIZE];
    int current_slot;

    std::vector<D3D12TextureData> textures;
    int current_tile;
    u32 current_texture_ids[2];

    int bound_texture_ids[MAX_TEXTURES];

    struct ShaderProgramD3D12 shader_program_pool[MAX_FRAME_PASSES][CC_MAX_SHADERS];
    u8 shader_program_pool_size[MAX_FRAME_PASSES] = { 0 };
    u8 shader_program_pool_index[MAX_FRAME_PASSES] = { 0 };
    struct ShaderProgramD3D12 post_process_shader_program_pool[MAX_FRAME_PASSES];

    struct ShaderProgramD3D12 *shader_program;

    u32 current_width, current_height;
    s8 depth_test;
    s8 depth_mask;
    s8 zmode_decal;

    D3D12_CPU_DESCRIPTOR_HANDLE current_rtv;
    D3D12_CPU_DESCRIPTOR_HANDLE current_dsv;
    bool current_target_is_backbuffer;
    bool command_list_open_for_pass;
    bool backbuffer_is_render_target;
    UINT current_backbuffer_index;

    std::vector<std::pair<UINT64, ID3D12Resource *>> pending_upload_releases;
} d3d;

static inline int d3d12_pso_variant_index() {
    return (d3d.depth_test ? 4 : 0) | (d3d.depth_mask ? 2 : 0) | (d3d.zmode_decal ? 1 : 0);
}

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_srv_cpu_handle(int slot) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = d3d.srv_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (SIZE_T)(slot) * d3d.srv_descriptor_size;
    return handle;
}

static D3D12_GPU_DESCRIPTOR_HANDLE d3d12_srv_gpu_handle(int slot) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = d3d.srv_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (UINT64)(slot) * d3d.srv_descriptor_size;
    return handle;
}

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_sampler_cpu_handle(int slot) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = d3d.sampler_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (SIZE_T)(slot) * d3d.sampler_descriptor_size;
    return handle;
}

static D3D12_GPU_DESCRIPTOR_HANDLE d3d12_sampler_gpu_handle(int slot) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = d3d.sampler_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (UINT64)(slot) * d3d.sampler_descriptor_size;
    return handle;
}

static inline int d3d12_frame_pass_slot(struct FramePass *framePass) {
    if (framePass == &gDefaultGeoFramePass) { return 0; }
    return (int)(framePass - gFramePasses) + 1;
}

static void d3d12_wait_for_fence_value(UINT64 value) {
    if (value == 0) { return; }
    if (d3d.fence->GetCompletedValue() < value) {
        ThrowIfFailed(d3d.fence->SetEventOnCompletion(value, d3d.fence_event));
        WaitForSingleObject(d3d.fence_event, INFINITE);
    }
}

static void d3d12_create_arena(D3D12UploadArena &arena, size_t size, const char *debugName) {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = size;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(d3d.device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&arena.resource)));
    d3d12_set_name(arena.resource, debugName);

    D3D12_RANGE readRange = { 0, 0 };
    ThrowIfFailed(arena.resource->Map(0, &readRange, (void **)(&arena.mapped)));
    arena.gpu_base = arena.resource->GetGPUVirtualAddress();
    arena.size = size;
    arena.offset = 0;
}

static bool d3d12_arena_alloc(D3D12UploadArena &arena, size_t size, size_t alignment, void **outCpuPtr, D3D12_GPU_VIRTUAL_ADDRESS *outGpuAddr) {
    size_t alignedOffset = (arena.offset + (alignment - 1)) & ~(alignment - 1);
    if (alignedOffset + size > arena.size) { return false; }

    *outCpuPtr = arena.mapped + alignedOffset;
    *outGpuAddr = arena.gpu_base + alignedOffset;
    arena.offset = alignedOffset + size;
    return true;
}

static void d3d12_release_pending_uploads() {
    UINT64 completed = d3d.fence->GetCompletedValue();
    size_t writeIdx = 0;
    for (size_t i = 0; i < d3d.pending_upload_releases.size(); i++) {
        if (d3d.pending_upload_releases[i].first <= completed) {
            d3d.pending_upload_releases[i].second->Release();
        } else {
            d3d.pending_upload_releases[writeIdx++] = d3d.pending_upload_releases[i];
        }
    }
    d3d.pending_upload_releases.resize(writeIdx);
}

static void d3d12_transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    d3d.command_list->ResourceBarrier(1, &barrier);
}

static void d3d12_ensure_backbuffer_render_target() {
    if (d3d.backbuffer_is_render_target) { return; }
    d3d12_transition(d3d.backbuffers[d3d.current_backbuffer_index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    d3d.backbuffer_is_render_target = true;
}

static void d3d12_ensure_backbuffer_present() {
    if (!d3d.backbuffer_is_render_target) { return; }
    d3d12_transition(d3d.backbuffers[d3d.current_backbuffer_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    d3d.backbuffer_is_render_target = false;
}

static void d3d12_create_depth_buffer(u32 width, u32 height) {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    d3d.depth_buffer.Reset();
    ThrowIfFailed(d3d.device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(d3d.depth_buffer.GetAddressOf())));
    d3d12_set_name(d3d.depth_buffer.Get(), "D3D12_BackbufferDepth");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    d3d.device->CreateDepthStencilView(d3d.depth_buffer.Get(), &dsvDesc, d3d.dsv_heap->GetCPUDescriptorHandleForHeapStart());
}

static void d3d12_create_render_target_views(bool is_resize) {
    if (is_resize) {
        for (int i = 0; i < 2; i++) { d3d.backbuffers[i].Reset(); }
        d3d.depth_buffer.Reset();

        DXGI_SWAP_CHAIN_DESC1 desc1;
        ThrowIfFailed(d3d.swap_chain->GetDesc1(&desc1));
        ThrowIfFailed(d3d.swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, desc1.Flags), gfx_window_dxgi_get_h_wnd(), "Failed to resize IDXGISwapChain buffers.");
    }

    DXGI_SWAP_CHAIN_DESC1 desc1;
    ThrowIfFailed(d3d.swap_chain->GetDesc1(&desc1));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = d3d.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < 2; i++) {
        ThrowIfFailed(d3d.swap_chain->GetBuffer(i, IID_PPV_ARGS(d3d.backbuffers[i].GetAddressOf())), gfx_window_dxgi_get_h_wnd(), "Failed to get backbuffer from IDXGISwapChain.");
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHandle;
        handle.ptr += (SIZE_T)(i) * d3d.rtv_descriptor_size;
        d3d.device->CreateRenderTargetView(d3d.backbuffers[i].Get(), nullptr, handle);
        char debugName[32];
        snprintf(debugName, sizeof(debugName), "D3D12_Backbuffer%u", i);
        d3d12_set_name(d3d.backbuffers[i].Get(), debugName);
    }

    d3d12_create_depth_buffer(desc1.Width, desc1.Height);

    d3d.current_width = desc1.Width;
    d3d.current_height = desc1.Height;
    d3d.current_backbuffer_index = d3d.swap_chain->GetCurrentBackBufferIndex();
    d3d.backbuffer_is_render_target = false;
}

static void d3d12_create_root_signature() {
    std::vector<D3D12_ROOT_PARAMETER1> rootParams(D3D12_ROOT_PARAM_COUNT);
    memset(rootParams.data(), 0, sizeof(D3D12_ROOT_PARAMETER1) * rootParams.size());

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

    static D3D12_DESCRIPTOR_RANGE1 srvRanges[MAX_TEXTURES];
    static D3D12_DESCRIPTOR_RANGE1 samplerRanges[MAX_TEXTURES];
    for (int i = 0; i < MAX_TEXTURES; i++) {
        srvRanges[i] = {};
        srvRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRanges[i].NumDescriptors = 1;
        srvRanges[i].BaseShaderRegister = i;
        srvRanges[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        srvRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER1 &srvParam = rootParams[D3D12_ROOT_PARAM_SRV_BASE + i];
        srvParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        srvParam.DescriptorTable.NumDescriptorRanges = 1;
        srvParam.DescriptorTable.pDescriptorRanges = &srvRanges[i];
        srvParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        samplerRanges[i] = {};
        samplerRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        samplerRanges[i].NumDescriptors = 1;
        samplerRanges[i].BaseShaderRegister = i;
        samplerRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER1 &samplerParam = rootParams[D3D12_ROOT_PARAM_SAMPLER_BASE + i];
        samplerParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        samplerParam.DescriptorTable.NumDescriptorRanges = 1;
        samplerParam.DescriptorTable.pDescriptorRanges = &samplerRanges[i];
        samplerParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }

    static D3D12_DESCRIPTOR_RANGE1 passSrvRange = {};
    passSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    passSrvRange.NumDescriptors = MAX_FRAME_PASSES;
    passSrvRange.BaseShaderRegister = D3D12_PASS_TEXTURE_REGISTER_BASE;
    passSrvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
    passSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER1 &passSrvParam = rootParams[D3D12_ROOT_PARAM_PASS_SRV];
    passSrvParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    passSrvParam.DescriptorTable.NumDescriptorRanges = 1;
    passSrvParam.DescriptorTable.pDescriptorRanges = &passSrvRange;
    passSrvParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    static D3D12_DESCRIPTOR_RANGE1 passSamplerRange = {};
    passSamplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    passSamplerRange.NumDescriptors = MAX_FRAME_PASSES;
    passSamplerRange.BaseShaderRegister = D3D12_PASS_TEXTURE_REGISTER_BASE;
    passSamplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER1 &passSamplerParam = rootParams[D3D12_ROOT_PARAM_PASS_SAMPLER];
    passSamplerParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    passSamplerParam.DescriptorTable.NumDescriptorRanges = 1;
    passSamplerParam.DescriptorTable.pDescriptorRanges = &passSamplerRange;
    passSamplerParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSigDesc.Desc_1_1.NumParameters = (UINT)(rootParams.size());
    rootSigDesc.Desc_1_1.pParameters = rootParams.data();
    rootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob *signatureBlob = nullptr;
    ID3DBlob *errorBlob = nullptr;
    HRESULT hr = d3d.D3D12SerializeVersionedRootSignature(&rootSigDesc, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob != nullptr) {
            ThrowIfFailed(hr, gfx_window_dxgi_get_h_wnd(), (const char *)(errorBlob->GetBufferPointer()));
        } else {
            ThrowIfFailed(hr, gfx_window_dxgi_get_h_wnd(), "Failed to serialize D3D12 root signature.");
        }
    }

    ThrowIfFailed(d3d.device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(d3d.root_signature.GetAddressOf())));
    signatureBlob->Release();
}

static void d3d12_create_default_texture() {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;

    ThrowIfFailed(d3d.device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(d3d.default_texture.GetAddressOf())));
    d3d12_set_name(d3d.default_texture.Get(), "D3D12_DefaultWhiteTexture");

    UINT64 uploadSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT numRows = 0;
    d3d.device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, nullptr, &uploadSize);

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource *stagingResource = nullptr;
    ThrowIfFailed(d3d.device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&stagingResource)));

    void *mapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    ThrowIfFailed(stagingResource->Map(0, &readRange, &mapped));
    uint32_t whitePixel = 0xFFFFFFFFu;
    memcpy((uint8_t *)(mapped) + footprint.Offset, &whitePixel, 4);
    stagingResource->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource = d3d.default_texture.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = stagingResource;
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = footprint;

    d3d.command_list->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
    d3d12_transition(d3d.default_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    ThrowIfFailed(d3d.command_list->Close());
    ID3D12CommandList *lists[] = { d3d.command_list.Get() };
    d3d.command_queue->ExecuteCommandLists(1, lists);

    UINT64 initFence = d3d.next_fence_value++;
    ThrowIfFailed(d3d.command_queue->Signal(d3d.fence.Get(), initFence));
    d3d12_wait_for_fence_value(initFence);
    stagingResource->Release();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    for (int i = 0; i < D3D12_SRV_HEAP_SIZE; i++) {
        d3d.device->CreateShaderResourceView(d3d.default_texture.Get(), &srvDesc, d3d12_srv_cpu_handle(i));
    }

    ThrowIfFailed(d3d.frame_slots[0].allocator->Reset());
    ThrowIfFailed(d3d.command_list->Reset(d3d.frame_slots[0].allocator, nullptr));
    ThrowIfFailed(d3d.command_list->Close());
}

static void d3d12_create_default_sampler() {
    D3D12_SAMPLER_DESC desc = {};
    desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D12_FLOAT32_MAX;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    d3d.device->CreateSampler(&desc, d3d12_sampler_cpu_handle(0));

    d3d.sampler_variants[0].used = true;
    d3d.sampler_variants[0].linear_filter = true;
    d3d.sampler_variants[0].cms = G_TX_CLAMP;
    d3d.sampler_variants[0].cmt = G_TX_CLAMP;

    for (int i = 0; i < MAX_FRAME_PASSES; i++) {
        d3d.device->CreateSampler(&desc, d3d12_sampler_cpu_handle(D3D12_PASS_SAMPLER_HEAP_BASE + i));
        d3d.pass_sampler_linear[i] = true;
    }
}

static void gfx_d3d12_init(void) {
    d3d.d3d12_module = LoadLibraryW(L"d3d12.dll");
    if (d3d.d3d12_module == nullptr) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), gfx_window_dxgi_get_h_wnd(), "d3d12.dll could not be loaded");
    }
    d3d.D3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)(GetProcAddress(d3d.d3d12_module, "D3D12CreateDevice"));
    d3d.D3D12SerializeVersionedRootSignature = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)(GetProcAddress(d3d.d3d12_module, "D3D12SerializeVersionedRootSignature"));
    if (d3d.D3D12CreateDevice == nullptr || d3d.D3D12SerializeVersionedRootSignature == nullptr) {
        ThrowIfFailed(E_FAIL, gfx_window_dxgi_get_h_wnd(), "Failed to resolve D3D12 entry points from d3d12.dll");
    }

    d3d.d3dcompiler_module = LoadLibraryW(L"D3DCompiler_47.dll");
    if (d3d.d3dcompiler_module == nullptr) {
        d3d.d3dcompiler_module = LoadLibraryW(L"D3DCompiler_43.dll");
        if (d3d.d3dcompiler_module == nullptr) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), gfx_window_dxgi_get_h_wnd(), "D3DCompiler_47.dll or D3DCompiler_43.dll could not be loaded");
        }
    }
    d3d.D3DCompile = (pD3DCompile)(GetProcAddress(d3d.d3dcompiler_module, "D3DCompile"));

#if DEBUG_D3D12
    {
        typedef HRESULT(WINAPI *PFN_D3D12_GET_DEBUG_INTERFACE)(REFIID, void **);
        PFN_D3D12_GET_DEBUG_INTERFACE getDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)(GetProcAddress(d3d.d3d12_module, "D3D12GetDebugInterface"));
        if (getDebugInterface != nullptr) {
            ID3D12Debug *debugController = nullptr;
            if (SUCCEEDED(getDebugInterface(IID_PPV_ARGS(&debugController)))) {
                debugController->EnableDebugLayer();
                debugController->Release();
            }
        }
    }
#endif

    gfx_window_dxgi_create_factory_and_device(DEBUG_D3D12, 12, [](IDXGIAdapter1 *adapter, bool test_only) {
        HRESULT res = d3d.D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), test_only ? nullptr : (void **)(d3d.device.GetAddressOf()));
        if (test_only) {
            return SUCCEEDED(res);
        } else {
            ThrowIfFailed(res, gfx_window_dxgi_get_h_wnd(), "Failed to create D3D12 device.");
            return true;
        }
    });

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(d3d.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(d3d.command_queue.GetAddressOf())));

    ComPtr<IDXGISwapChain1> swapChain1 = gfx_window_dxgi_create_swap_chain(d3d.command_queue.Get());
    ThrowIfFailed(swapChain1.As(&d3d.swap_chain));

    d3d.rtv_descriptor_size = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    d3d.dsv_descriptor_size = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    d3d.srv_descriptor_size = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    d3d.sampler_descriptor_size = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 2;
    ThrowIfFailed(d3d.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(d3d.rtv_heap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    ThrowIfFailed(d3d.device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(d3d.dsv_heap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = D3D12_SRV_HEAP_SIZE;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(d3d.device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(d3d.srv_heap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
    samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    samplerHeapDesc.NumDescriptors = D3D12_SAMPLER_HEAP_SIZE;
    samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(d3d.device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(d3d.sampler_heap.GetAddressOf())));

    d3d12_create_root_signature();
    d3d12_create_render_target_views(false);

    ThrowIfFailed(d3d.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(d3d.fence.GetAddressOf())));
    d3d.fence_event = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    d3d.next_fence_value = 1;

    for (int i = 0; i < D3D12_FRAME_RING_SIZE; i++) {
        ThrowIfFailed(d3d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d.frame_slots[i].allocator)));
        char debugName[32];
        snprintf(debugName, sizeof(debugName), "D3D12_VertexArena%d", i);
        d3d12_create_arena(d3d.frame_slots[i].vertex_arena, D3D12_VERTEX_ARENA_SIZE, debugName);
        snprintf(debugName, sizeof(debugName), "D3D12_UniformArena%d", i);
        d3d12_create_arena(d3d.frame_slots[i].uniform_arena, D3D12_UNIFORM_ARENA_SIZE, debugName);
        d3d.frame_slots[i].fence_value = 0;
    }

    d3d.current_slot = 0;
    ThrowIfFailed(d3d.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d.frame_slots[0].allocator, nullptr, IID_PPV_ARGS(d3d.command_list.GetAddressOf())));

    for (int i = 0; i < D3D12_MAX_SAMPLER_VARIANTS; i++) { d3d.sampler_variants[i].used = false; }
    d3d12_create_default_sampler();
    d3d12_create_default_texture(); // uses, then closes, the still-open command list from CreateCommandList above

    controller_bind_init();
}

static bool d3d12_z_is_from_0_to_1(void) {
    return true;
}

static void d3d12_unload_shader(struct ShaderProgram *old_prg) {
}

static void d3d12_load_shader(struct ShaderProgram *new_prg) {
    if ((struct ShaderProgram *)(d3d.shader_program) != new_prg) {
        for (int i = 0; i < MAX_TEXTURES; i++) { d3d.bound_texture_ids[i] = -1; }
    }
    d3d.shader_program = (struct ShaderProgramD3D12 *)(new_prg);
}

static void d3d12_free_shader_program_contents(struct ShaderProgramD3D12 *prg) {
    for (int i = 0; i < D3D12_PSO_VARIANT_COUNT; i++) {
        if (prg->pso[i] != nullptr) { prg->pso[i]->Release(); }
    }
    if (prg->vs_blob != nullptr) { prg->vs_blob->Release(); }
    if (prg->ps_blob != nullptr) { prg->ps_blob->Release(); }
    gfx_destroy_shader(prg->vertexShader);
    gfx_destroy_shader(prg->fragmentShader);
    *prg = { 0 };
}

static void d3d12_remove_shaders(void) {
    for (int i = 0; i < MAX_FRAME_PASSES; i++) {
        for (int j = 0; j < CC_MAX_SHADERS; j++) {
            d3d12_free_shader_program_contents(&d3d.shader_program_pool[i][j]);
        }
        d3d.shader_program_pool_index[i] = 0;
        d3d.shader_program_pool_size[i] = 0;
        d3d12_free_shader_program_contents(&d3d.post_process_shader_program_pool[i]);
    }

    d3d.shader_program = nullptr;
}

static ID3D12PipelineState *d3d12_get_or_create_pso(struct ShaderProgramD3D12 *prg) {
    int variant = d3d12_pso_variant_index();
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

    D3D12_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterDesc.FrontCounterClockwise = TRUE;
    rasterDesc.SlopeScaledDepthBias = d3d.zmode_decal ? -2.0f : 0.0f;
    rasterDesc.DepthClipEnable = TRUE;
    psoDesc.RasterizerState = rasterDesc;

    D3D12_BLEND_DESC blendDesc = {};
    if (prg->use_alpha) {
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    }
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState = blendDesc;

    D3D12_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = d3d.depth_test ? TRUE : FALSE;
    depthDesc.DepthWriteMask = d3d.depth_mask ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;
    psoDesc.DepthStencilState = depthDesc;

    ID3D12PipelineState *pso = nullptr;
    HRESULT hr = d3d.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12-DEBUG] PSO creation failed (hr=0x%08lX, variant=%d)\n", hr, variant);
        return nullptr;
    }

    prg->pso[variant] = pso;
    return pso;
}

static void d3d12_build_input_layout(struct ShaderProgramD3D12 *prg, struct Shader *vertexShader, struct ShaderInput *inputs) {
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

static struct ShaderProgram *d3d12_create_and_load_new_shader(struct ColorCombiner *cc) {
    CCFeatures cc_features = { 0 };
    gfx_cc_get_features(cc, &cc_features);

    struct Shader *vertexShader = (struct Shader *)(calloc(1, sizeof(struct Shader)));
    struct Shader *fragmentShader = (struct Shader *)(calloc(1, sizeof(struct Shader)));
    if (!vertexShader || !fragmentShader) {
        sys_fatal("Failed to allocate shaders, ran out of memory!");
    }

    gfx_generate_vertex_and_fragment_shader_from_cc(vertexShader, fragmentShader, cc, nullptr, nullptr);

    char *vs_hlsl = nullptr;
    char *ps_hlsl = nullptr;
    gfx_convert_spirv_to_hlsl(&vs_hlsl, vertexShader);
    gfx_convert_spirv_to_hlsl(&ps_hlsl, fragmentShader);

    ComPtr<ID3DBlob> vs, ps;
    ComPtr<ID3DBlob> error_blob;

#if DEBUG_D3D12
    UINT compile_flags = D3DCOMPILE_DEBUG;
#else
    UINT compile_flags = D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = d3d.D3DCompile(vs_hlsl, strlen(vs_hlsl), nullptr, nullptr, nullptr, "main", "vs_5_0", compile_flags, 0, vs.GetAddressOf(), error_blob.GetAddressOf());
    if (FAILED(hr)) {
        MessageBox(gfx_window_dxgi_get_h_wnd(), (char *)(error_blob->GetBufferPointer()), "Vertex Shader Error", MB_OK | MB_ICONERROR);
        free(vs_hlsl);
        free(ps_hlsl);
        throw hr;
    }

    hr = d3d.D3DCompile(ps_hlsl, strlen(ps_hlsl), nullptr, nullptr, nullptr, "main", "ps_5_0", compile_flags, 0, ps.GetAddressOf(), error_blob.GetAddressOf());
    if (FAILED(hr)) {
        MessageBox(gfx_window_dxgi_get_h_wnd(), (char *)(error_blob->GetBufferPointer()), "Pixel Shader Error", MB_OK | MB_ICONERROR);
        free(vs_hlsl);
        free(ps_hlsl);
        throw hr;
    }

    free(vs_hlsl);
    free(ps_hlsl);

    int framePassIndex = gCurrentFramePassIndex + 1;

    struct ShaderProgramD3D12 *prg = &d3d.shader_program_pool[framePassIndex][d3d.shader_program_pool_index[framePassIndex]];
    d3d12_free_shader_program_contents(prg);
    d3d.shader_program_pool_index[framePassIndex] = (d3d.shader_program_pool_index[framePassIndex] + 1) % CC_MAX_SHADERS;
    if (d3d.shader_program_pool_size[framePassIndex] < CC_MAX_SHADERS) { d3d.shader_program_pool_size[framePassIndex]++; }

    vs->AddRef();
    ps->AddRef();
    prg->vs_blob = vs.Get();
    prg->ps_blob = ps.Get();

    d3d12_build_input_layout(prg, vertexShader, gShaderInputs);
    prg->use_alpha = cc->cm.use_alpha;

    size_t num_floats = 0;
    for (int i = 0; i < MAX_SHADER_INPUTS; i++) {
        if (gShaderInputs[i].size == 0) { continue; }
        num_floats += gShaderInputs[i].size;
    }

    prg->hash = cc->hash;
    prg->num_inputs = cc_features.num_inputs;
    prg->num_floats = (uint8_t)(num_floats);
    prg->used_textures[0] = cc_features.used_textures[0];
    prg->used_textures[1] = cc_features.used_textures[1];
    prg->used_fog = cc->cm.use_fog;
    prg->vertexShader = vertexShader;
    prg->fragmentShader = fragmentShader;

    return (struct ShaderProgram *)(d3d.shader_program = prg);
}

static struct ShaderProgram *d3d12_create_or_load_post_process_shader(void) {
    int framePassIndex = gCurrentFramePassIndex + 1;
    struct ShaderProgramD3D12 *prg = &d3d.post_process_shader_program_pool[framePassIndex];

    if (prg->vs_blob != nullptr) {
        d3d.shader_program = prg;
        return (struct ShaderProgram *)(prg);
    }

    struct Shader *vertexShader = (struct Shader *)(calloc(1, sizeof(struct Shader)));
    struct Shader *fragmentShader = (struct Shader *)(calloc(1, sizeof(struct Shader)));
    if (!vertexShader || !fragmentShader) {
        sys_fatal("Failed to allocate shaders, ran out of memory!");
    }

    gfx_generate_post_process_vertex_and_fragment_shader(vertexShader, fragmentShader, nullptr, nullptr);

    char *vs_hlsl = nullptr;
    char *ps_hlsl = nullptr;
    gfx_convert_spirv_to_hlsl(&vs_hlsl, vertexShader);
    gfx_convert_spirv_to_hlsl(&ps_hlsl, fragmentShader);

    ComPtr<ID3DBlob> vs, ps;
    ComPtr<ID3DBlob> error_blob;

#if DEBUG_D3D12
    UINT compile_flags = D3DCOMPILE_DEBUG;
#else
    UINT compile_flags = D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = d3d.D3DCompile(vs_hlsl, strlen(vs_hlsl), nullptr, nullptr, nullptr, "main", "vs_5_0", compile_flags, 0, vs.GetAddressOf(), error_blob.GetAddressOf());
    if (FAILED(hr)) {
        MessageBox(gfx_window_dxgi_get_h_wnd(), (char *)(error_blob->GetBufferPointer()), "Post-Process VS Error", MB_OK | MB_ICONERROR);
        free(vs_hlsl);
        free(ps_hlsl);
        throw hr;
    }

    hr = d3d.D3DCompile(ps_hlsl, strlen(ps_hlsl), nullptr, nullptr, nullptr, "main", "ps_5_0", compile_flags, 0, ps.GetAddressOf(), error_blob.GetAddressOf());
    if (FAILED(hr)) {
        MessageBox(gfx_window_dxgi_get_h_wnd(), (char *)(error_blob->GetBufferPointer()), "Post-Process PS Error", MB_OK | MB_ICONERROR);
        free(vs_hlsl);
        free(ps_hlsl);
        throw hr;
    }

    free(vs_hlsl);
    free(ps_hlsl);

    vs->AddRef();
    ps->AddRef();
    prg->vs_blob = vs.Get();
    prg->ps_blob = ps.Get();

    d3d12_build_input_layout(prg, vertexShader, gPostProcessShaderInputs);
    prg->use_alpha = false;

    size_t num_floats = 0;
    for (int i = 0; i < MAX_SHADER_INPUTS; i++) {
        num_floats += gPostProcessShaderInputs[i].size;
    }

    prg->hash = (uint64_t)(framePassIndex);
    prg->num_inputs = prg->input_element_count;
    prg->num_floats = (uint8_t)(num_floats);
    prg->used_textures[0] = true;
    prg->used_textures[1] = false;
    prg->used_fog = false;
    prg->vertexShader = vertexShader;
    prg->fragmentShader = fragmentShader;

    d3d.shader_program = prg;
    return (struct ShaderProgram *)(prg);
}

static struct ShaderProgram *d3d12_lookup_shader(struct ColorCombiner *cc) {
    int framePassIndex = gCurrentFramePassIndex + 1;
    if (framePassIndex < 0 || framePassIndex >= MAX_FRAME_PASSES) { return nullptr; }
    for (size_t i = 0; i < d3d.shader_program_pool_size[framePassIndex]; i++) {
        if (d3d.shader_program_pool[framePassIndex][i].hash == cc->hash) {
            return (struct ShaderProgram *)(&d3d.shader_program_pool[framePassIndex][i]);
        }
    }
    return nullptr;
}

static struct ShaderProgram *d3d12_lookup_shader_using_index(u8 shaderIndex, u8 framePassIndex) {
    framePassIndex++;
    if (shaderIndex >= d3d.shader_program_pool_size[framePassIndex]) { return nullptr; }
    return (struct ShaderProgram *)(&d3d.shader_program_pool[framePassIndex][shaderIndex]);
}

static void d3d12_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs, bool used_textures[2]) {
    struct ShaderProgramD3D12 *p = (struct ShaderProgramD3D12 *)(prg);
    *num_inputs = p->num_inputs;
    used_textures[0] = p->used_textures[0];
    used_textures[1] = p->used_textures[1];
}

static void d3d12_delete_framebuffer(struct FramePass *framePass) {
    if (framePass->d3dRtv != nullptr) { ((ID3D12DescriptorHeap *)(framePass->d3dRtv))->Release(); framePass->d3dRtv = nullptr; }
    if (framePass->d3dDsv != nullptr) { ((ID3D12DescriptorHeap *)(framePass->d3dDsv))->Release(); framePass->d3dDsv = nullptr; }
    if (framePass->d3dTexture != nullptr) { ((ID3D12Resource *)(framePass->d3dTexture))->Release(); framePass->d3dTexture = nullptr; }
    if (framePass->d3dSrv != nullptr) { ((ID3D12Resource *)(framePass->d3dSrv))->Release(); framePass->d3dSrv = nullptr; }
    framePass->passTexture = 0;
    framePass->fbo = 0;
}

static void d3d12_create_framebuffer(struct FramePass *framePass) {
    u32 viewportWidth, viewportHeight;
    gfx_get_frame_pass_viewport_dimensions(framePass, &viewportWidth, &viewportHeight);
    if (viewportWidth == 0 || viewportHeight == 0) { return; }

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = viewportWidth;
    texDesc.Height = viewportHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[3] = 1.0f; // matches the opaque-black clear gfx_pc.c's clearColor[3]=255 produces

    ID3D12Resource *colorTexture = nullptr;
    HRESULT hr = d3d.device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&colorTexture));
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12-DEBUG] framebuffer texture creation failed (%ux%u, hr=0x%08lX)\n", viewportWidth, viewportHeight, hr);
        return;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    ID3D12DescriptorHeap *rtvHeap = nullptr;
    hr = d3d.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr)) { colorTexture->Release(); return; }
    d3d.device->CreateRenderTargetView(colorTexture, nullptr, rtvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_HEAP_PROPERTIES depthHeapProps = {};
    depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = viewportWidth;
    depthDesc.Height = viewportHeight;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClearValue = {};
    depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthClearValue.DepthStencil.Depth = 1.0f;

    ID3D12Resource *depthTexture = nullptr;
    ID3D12DescriptorHeap *dsvHeap = nullptr;
    hr = d3d.device->CreateCommittedResource(&depthHeapProps, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&depthTexture));
    if (SUCCEEDED(hr)) {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NumDescriptors = 1;
        hr = d3d.device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
        if (SUCCEEDED(hr)) {
            d3d.device->CreateDepthStencilView(depthTexture, nullptr, dsvHeap->GetCPUDescriptorHandleForHeapStart());
        }
    }

    int slot = d3d12_frame_pass_slot(framePass);
    int heapIndex = D3D12_MAX_GAME_TEXTURES + slot;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    d3d.device->CreateShaderResourceView(colorTexture, &srvDesc, d3d12_srv_cpu_handle(heapIndex));

    char debugName[64];
    snprintf(debugName, sizeof(debugName), "D3D12_FramebufferColor_slot%d", slot);
    d3d12_set_name(colorTexture, debugName);
    if (depthTexture != nullptr) {
        snprintf(debugName, sizeof(debugName), "D3D12_FramebufferDepth_slot%d", slot);
        d3d12_set_name(depthTexture, debugName);
    }

    framePass->d3dTexture = colorTexture;
    framePass->d3dRtv = rtvHeap;
    framePass->d3dDsv = dsvHeap; // may be null if depth creation failed; treated as "no depth" below
    if (dsvHeap != nullptr) {
        framePass->d3dSrv = depthTexture; // see d3d12_delete_framebuffer for why this is stored here
    } else {
        framePass->d3dSrv = nullptr;
        if (depthTexture != nullptr) { depthTexture->Release(); }
    }
    framePass->passTexture = (u64)(slot + 1);
    framePass->width = viewportWidth;
    framePass->height = viewportHeight;
    framePass->d3d12CurrentlyShaderResource = false;
    framePass->fbo = 1;
}

static void d3d12_prepare_framebuffer_for_render(struct FramePass *framePass) {
    if (framePass->d3d12CurrentlyShaderResource) {
        d3d12_transition((ID3D12Resource *)(framePass->d3dTexture), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        framePass->d3d12CurrentlyShaderResource = false;
    }
}

static void d3d12_prepare_framebuffer_for_sampling(struct FramePass *framePass) {
    if (!framePass->d3d12CurrentlyShaderResource) {
        d3d12_transition((ID3D12Resource *)(framePass->d3dTexture), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        framePass->d3d12CurrentlyShaderResource = true;
    }
}

static void d3d12_begin_pass_if_needed() {
    if (d3d.command_list_open_for_pass) { return; }

    d3d.current_slot = (d3d.current_slot + 1) % D3D12_FRAME_RING_SIZE;
    D3D12FrameSlot &slot = d3d.frame_slots[d3d.current_slot];

    d3d12_wait_for_fence_value(slot.fence_value);
    d3d12_release_pending_uploads();

    ThrowIfFailed(slot.allocator->Reset());
    ThrowIfFailed(d3d.command_list->Reset(slot.allocator, nullptr));
    slot.vertex_arena.offset = 0;
    slot.uniform_arena.offset = 0;

    for (int i = 0; i < MAX_TEXTURES; i++) { d3d.bound_texture_ids[i] = -1; }

    ID3D12DescriptorHeap *heaps[] = { d3d.srv_heap.Get(), d3d.sampler_heap.Get() };
    d3d.command_list->SetDescriptorHeaps(2, heaps);
    d3d.command_list->SetGraphicsRootSignature(d3d.root_signature.Get());

    d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_PASS_SRV, d3d12_srv_gpu_handle(D3D12_MAX_GAME_TEXTURES));
    d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_PASS_SAMPLER, d3d12_sampler_gpu_handle(D3D12_PASS_SAMPLER_HEAP_BASE));

    d3d.command_list_open_for_pass = true;
}

static void d3d12_set_framebuffer(struct FramePass *framePass) {
    if (framePass->d3dTexture == nullptr) { return; }

    d3d12_begin_pass_if_needed();
    d3d12_prepare_framebuffer_for_render(framePass);

    d3d.current_rtv = ((ID3D12DescriptorHeap *)(framePass->d3dRtv))->GetCPUDescriptorHandleForHeapStart();
    bool hasDepth = framePass->d3dDsv != nullptr;
    if (hasDepth) { d3d.current_dsv = ((ID3D12DescriptorHeap *)(framePass->d3dDsv))->GetCPUDescriptorHandleForHeapStart(); }
    d3d.current_target_is_backbuffer = false;

    d3d.command_list->OMSetRenderTargets(1, &d3d.current_rtv, FALSE, hasDepth ? &d3d.current_dsv : nullptr);

    D3D12_VIEWPORT viewport = {};
    viewport.Width = (float)(framePass->width);
    viewport.Height = (float)(framePass->height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    d3d.command_list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {};
    scissor.right = (LONG)(framePass->width);
    scissor.bottom = (LONG)(framePass->height);
    d3d.command_list->RSSetScissorRects(1, &scissor);
}

static void d3d12_reset_framebuffer(void) {
    d3d12_begin_pass_if_needed();

    u32 windowWidth, windowHeight;
    gfx_get_dimensions(&windowWidth, &windowHeight);

    d3d.current_backbuffer_index = d3d.swap_chain->GetCurrentBackBufferIndex();

    d3d12_ensure_backbuffer_render_target();

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = d3d.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += (SIZE_T)(d3d.current_backbuffer_index) * d3d.rtv_descriptor_size;
    d3d.current_rtv = rtv;
    d3d.current_dsv = d3d.dsv_heap->GetCPUDescriptorHandleForHeapStart();
    d3d.current_target_is_backbuffer = true;

    d3d.command_list->OMSetRenderTargets(1, &d3d.current_rtv, FALSE, &d3d.current_dsv);

    D3D12_VIEWPORT viewport = {};
    viewport.Width = (float)(windowWidth);
    viewport.Height = (float)(windowHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    d3d.command_list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {};
    scissor.right = (LONG)(windowWidth);
    scissor.bottom = (LONG)(windowHeight);
    d3d.command_list->RSSetScissorRects(1, &scissor);
}

static void d3d12_set_uniform_buffer(enum ShaderStage stage, const char *name) {
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

static void d3d12_set_uniform_for_specific_shader(struct ShaderUniformBlock *uniformBlock, const char *name, const void *data, uint32_t numElements) {
    for (int i = 0; i < MAX_SHADER_UNIFORMS; i++) {
        struct ShaderUniform *uniform = &uniformBlock->uniforms[i];
        if (uniform->size == 0) { break; }

        if (strcmp(uniform->name, name) == 0) {
            u8 *dst = uniformBlock->buffer + uniform->location;

            if (uniform->arrayLength > 1) {
                const u8 *src = (const u8 *)(data);
                u32 count = MIN(numElements, (u32)(uniform->arrayLength));
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

static void d3d12_set_uniform(struct ShaderProgram *prg_, const char *name, UNUSED ShaderUniformType type, const void *data, uint32_t numElements) {
    struct ShaderProgramD3D12 *prg = (struct ShaderProgramD3D12 *)(prg_);
    if (prg == nullptr) {
        if (d3d.shader_program == nullptr) { return; }
        prg = d3d.shader_program;
    }

    if (gfx_shader_stage_is(SHADER_STAGE_VERTEX)) {
        d3d12_set_uniform_for_specific_shader(&prg->vertexShader->uniformBlocks[gSelectedVertexUniformBuffer], name, data, numElements);
    }
    if (gfx_shader_stage_is(SHADER_STAGE_FRAGMENT)) {
        d3d12_set_uniform_for_specific_shader(&prg->fragmentShader->uniformBlocks[gSelectedFragmentUniformBuffer], name, data, numElements);
    }
}

static uint32_t d3d12_new_texture(void) {
    d3d.textures.emplace_back();
    D3D12TextureData &tex = d3d.textures.back();
    tex.resource = nullptr;
    tex.sampler_slot = -1;
    tex.linear_filtering = false;
    tex.cms = tex.cmt = 0;
    tex.width = tex.height = 0;
    return (uint32_t)(d3d.textures.size() - 1);
}

static void d3d12_select_texture(int tile, uint32_t texture_id) {
    d3d.current_tile = tile;
    d3d.current_texture_ids[tile] = texture_id;
}

static void d3d12_bind_texture_raw(int tile, uint64_t texture_id) {
    if (tile < D3D12_PASS_TEXTURE_REGISTER_BASE) { return; }

    int slot = (int)(texture_id) - 1;
    if (slot < 0) { return; }

    int registerOffset = tile - D3D12_PASS_TEXTURE_REGISTER_BASE;
    int srvHeapIndex = D3D12_MAX_GAME_TEXTURES + slot;
    int srvTableBase = srvHeapIndex - registerOffset;

    struct FramePass *ownerFramePass = (slot == 0) ? &gDefaultGeoFramePass : &gFramePasses[slot - 1];
    if (ownerFramePass->d3dTexture != nullptr) {
        d3d12_prepare_framebuffer_for_sampling(ownerFramePass);
    }

    d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_PASS_SRV, d3d12_srv_gpu_handle(srvTableBase));

    int samplerHeapIndex = D3D12_PASS_SAMPLER_HEAP_BASE + slot;
    bool wantLinear = (ownerFramePass->passFilter == PASS_FILTER_LINEAR);
    if (d3d.pass_sampler_linear[slot] != wantLinear) {
        D3D12_SAMPLER_DESC desc = {};
        desc.Filter = wantLinear ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.MinLOD = 0.0f;
        desc.MaxLOD = D3D12_FLOAT32_MAX;
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        d3d.device->CreateSampler(&desc, d3d12_sampler_cpu_handle(samplerHeapIndex));
        d3d.pass_sampler_linear[slot] = wantLinear;
    }

    int samplerTableBase = samplerHeapIndex - registerOffset;
    d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_PASS_SAMPLER, d3d12_sampler_gpu_handle(samplerTableBase));
}

static D3D12_TEXTURE_ADDRESS_MODE d3d12_cm_to_address_mode(uint32_t val) {
    if (val & G_TX_CLAMP) { return D3D12_TEXTURE_ADDRESS_MODE_CLAMP; }
    return (val & G_TX_MIRROR) ? D3D12_TEXTURE_ADDRESS_MODE_MIRROR : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

static void d3d12_upload_texture(const uint8_t *rgba32_buf, int width, int height) {
    D3D12TextureData &texture_data = d3d.textures[d3d.current_texture_ids[d3d.current_tile]];

    if (texture_data.resource != nullptr) {
        d3d.pending_upload_releases.emplace_back(d3d.next_fence_value, texture_data.resource);
        texture_data.resource = nullptr;
    }

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;

    ID3D12Resource *texture = nullptr;
    HRESULT hr = d3d.device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture));
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12-DEBUG] texture creation failed (%dx%d, hr=0x%08lX)\n", width, height, hr);
        return;
    }

    UINT64 uploadSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT numRows = 0;
    d3d.device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, nullptr, &uploadSize);

    ID3D12Resource *stagingResource = nullptr;
    void *stagingMapped = nullptr;

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = d3d.device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&stagingResource));
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12-DEBUG] texture upload staging buffer creation failed (hr=0x%08lX)\n", hr);
        texture->Release();
        return;
    }

    D3D12_RANGE readRange = { 0, 0 };
    stagingResource->Map(0, &readRange, &stagingMapped);
    for (UINT row = 0; row < numRows; row++) {
        memcpy((uint8_t *)(stagingMapped) + footprint.Offset + row * footprint.Footprint.RowPitch, rgba32_buf + row * width * 4, width * 4);
    }
    stagingResource->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource = texture;
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = stagingResource;
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = footprint;

    d3d.command_list->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
    d3d12_transition(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    d3d.pending_upload_releases.emplace_back(d3d.next_fence_value, stagingResource);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    d3d.device->CreateShaderResourceView(texture, &srvDesc, d3d12_srv_cpu_handle(d3d.current_texture_ids[d3d.current_tile]));

    texture_data.resource = texture;
    texture_data.width = (u32)(width);
    texture_data.height = (u32)(height);
}

static void d3d12_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt) {
    D3D12TextureData &texture_data = d3d.textures[d3d.current_texture_ids[tile]];
    texture_data.linear_filtering = linear_filter;
    texture_data.cms = cms;
    texture_data.cmt = cmt;

    for (int i = 0; i < D3D12_MAX_SAMPLER_VARIANTS; i++) {
        if (d3d.sampler_variants[i].used && d3d.sampler_variants[i].linear_filter == linear_filter &&
            d3d.sampler_variants[i].cms == cms && d3d.sampler_variants[i].cmt == cmt) {
            texture_data.sampler_slot = i;
            return;
        }
    }

    for (int i = 0; i < D3D12_MAX_SAMPLER_VARIANTS; i++) {
        if (!d3d.sampler_variants[i].used) {
            D3D12_SAMPLER_DESC desc = {};
            desc.Filter = linear_filter ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT;
            desc.AddressU = d3d12_cm_to_address_mode(cms);
            desc.AddressV = d3d12_cm_to_address_mode(cmt);
            desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            desc.MinLOD = 0.0f;
            desc.MaxLOD = D3D12_FLOAT32_MAX;
            desc.MaxAnisotropy = 1;
            desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            d3d.device->CreateSampler(&desc, d3d12_sampler_cpu_handle(i));

            d3d.sampler_variants[i].used = true;
            d3d.sampler_variants[i].linear_filter = linear_filter;
            d3d.sampler_variants[i].cms = cms;
            d3d.sampler_variants[i].cmt = cmt;

            texture_data.sampler_slot = i;
            return;
        }
    }

    fprintf(stderr, "[D3D12-DEBUG] sampler variant cache exhausted (%d slots)\n", D3D12_MAX_SAMPLER_VARIANTS);
    texture_data.sampler_slot = 0;
}

static void d3d12_set_depth_test(bool depth_test) {
    d3d.depth_test = depth_test;
}

static void d3d12_set_depth_mask(bool depth_mask) {
    d3d.depth_mask = depth_mask;
}

static void d3d12_set_zmode_decal(bool zmode_decal) {
    d3d.zmode_decal = zmode_decal;
}

static void d3d12_set_viewport(int x, int y, int width, int height) {
    struct FramePass *framePass = gfx_get_current_frame_pass();
    u32 viewportHeight;
    gfx_get_frame_pass_viewport_dimensions(framePass, NULL, &viewportHeight);

    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = (float)(x);
    viewport.TopLeftY = (float)((int)(viewportHeight) - y - height);
    viewport.Width = (float)(width);
    viewport.Height = (float)(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    d3d.command_list->RSSetViewports(1, &viewport);
}

static void d3d12_set_scissor(int x, int y, int width, int height) {
    struct FramePass *framePass = gfx_get_current_frame_pass();
    u32 viewportHeight;
    gfx_get_frame_pass_viewport_dimensions(framePass, NULL, &viewportHeight);

    D3D12_RECT rect = {};
    rect.left = x;
    rect.top = (LONG)(viewportHeight) - y - height;
    rect.right = x + width;
    rect.bottom = (LONG)(viewportHeight) - y;
    d3d.command_list->RSSetScissorRects(1, &rect);
}

static void d3d12_set_use_alpha(bool use_alpha) {
    // Already part of the pipeline state from shader info
}

static void d3d12_set_vsync(bool enabled) {
}

static void d3d12_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    if (d3d.shader_program == nullptr || buf_vbo_num_tris == 0) { return; }

    ID3D12PipelineState *pso = d3d12_get_or_create_pso(d3d.shader_program);
    if (pso == nullptr) { return; }

    for (int i = 0; i < MAX_TEXTURES; i++) {
        if (!d3d.shader_program->used_textures[i]) { continue; }

        if (d3d.bound_texture_ids[i] == (int)(d3d.current_texture_ids[i])) { continue; }
        d3d.bound_texture_ids[i] = (int)(d3d.current_texture_ids[i]);

        D3D12TextureData &texture_data = d3d.textures[d3d.current_texture_ids[i]];
        d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_SRV_BASE + i, d3d12_srv_gpu_handle(d3d.current_texture_ids[i]));
        int samplerSlot = (texture_data.sampler_slot >= 0) ? texture_data.sampler_slot : 0;
        d3d.command_list->SetGraphicsRootDescriptorTable(D3D12_ROOT_PARAM_SAMPLER_BASE + i, d3d12_sampler_gpu_handle(samplerSlot));

        char sizeUniformName[MAX_SHADER_VARIABLE_NAME];
        snprintf(sizeUniformName, sizeof(sizeUniformName), "uTex%dSize", i);
        float texSize[2] = { (float)(texture_data.width), (float)(texture_data.height) };
        d3d12_set_uniform(NULL, sizeUniformName, SHADER_UNIFORM_TYPE_VEC2, texSize, 1);

        char filterUniformName[MAX_SHADER_VARIABLE_NAME];
        snprintf(filterUniformName, sizeof(filterUniformName), "uTex%dFilter", i);
        u32 isLinear = texture_data.linear_filtering ? 1 : 0;
        d3d12_set_uniform(NULL, filterUniformName, SHADER_UNIFORM_TYPE_INT, &isLinear, 1);
    }

    gfx_update_matrices();
    if (d3d.shader_program->used_fog) {
        gfx_update_fog_uniforms();
    }
    smlua_call_event_hooks(HOOK_ON_DRAW_TRIANGLE);

    D3D12UploadArena &uniformArena = d3d.frame_slots[d3d.current_slot].uniform_arena;
    for (int i = 0; i < MAX_UNIFORM_BLOCKS; i++) {
        struct ShaderUniformBlock &vsBlock = d3d.shader_program->vertexShader->uniformBlocks[i];
        if (vsBlock.size > 0) {
            void *cpuPtr = nullptr;
            D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = 0;
            if (d3d12_arena_alloc(uniformArena, vsBlock.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, &cpuPtr, &gpuAddr)) {
                memcpy(cpuPtr, vsBlock.buffer, vsBlock.size);
                d3d.command_list->SetGraphicsRootConstantBufferView(D3D12_ROOT_PARAM_VS_CBV_BASE + vsBlock.location, gpuAddr);
            }
        }

        struct ShaderUniformBlock &fsBlock = d3d.shader_program->fragmentShader->uniformBlocks[i];
        if (fsBlock.size > 0) {
            void *cpuPtr = nullptr;
            D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = 0;
            if (d3d12_arena_alloc(uniformArena, fsBlock.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, &cpuPtr, &gpuAddr)) {
                memcpy(cpuPtr, fsBlock.buffer, fsBlock.size);
                d3d.command_list->SetGraphicsRootConstantBufferView(D3D12_ROOT_PARAM_FS_CBV_BASE + fsBlock.location, gpuAddr);
            }
        }
    }

    size_t vertexBytes = buf_vbo_len * sizeof(float);
    D3D12UploadArena &vertexArena = d3d.frame_slots[d3d.current_slot].vertex_arena;
    void *vertexCpuPtr = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS vertexGpuAddr = 0;
    if (!d3d12_arena_alloc(vertexArena, vertexBytes, 16, &vertexCpuPtr, &vertexGpuAddr)) {
        fprintf(stderr, "[D3D12-DEBUG] vertex arena exhausted this frame, skipping draw\n");
        return;
    }
    memcpy(vertexCpuPtr, buf_vbo, vertexBytes);

    UINT stride = (UINT)(d3d.shader_program->num_floats) * sizeof(float);
    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = vertexGpuAddr;
    vbView.SizeInBytes = (UINT)(vertexBytes);
    vbView.StrideInBytes = stride;

    d3d.command_list->SetPipelineState(pso);
    d3d.command_list->IASetVertexBuffers(0, 1, &vbView);
    d3d.command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3d.command_list->DrawInstanced((UINT)(buf_vbo_num_tris * 3), 1, 0, 0);
}

static void d3d12_on_resize(void) {
    for (int i = 0; i < MAX_CUSTOM_FRAME_PASSES; i++) {
        struct FramePass *framePass = &gFramePasses[i];
        if (!framePass->active) { continue; }
        if (framePass->width == 0 || framePass->height == 0) {
            d3d12_delete_framebuffer(framePass);
        }
    }

    for (int i = 0; i < D3D12_FRAME_RING_SIZE; i++) {
        d3d12_wait_for_fence_value(d3d.frame_slots[i].fence_value);
    }

    d3d12_create_render_target_views(true);
}

static void d3d12_start_frame(void) {
    struct FramePass *framePass = gfx_get_current_frame_pass();

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (framePass != nullptr) {
        clearColor[0] = framePass->clearColor[0] / 255.0f;
        clearColor[1] = framePass->clearColor[1] / 255.0f;
        clearColor[2] = framePass->clearColor[2] / 255.0f;
        clearColor[3] = framePass->clearColor[3] / 255.0f;
    }

    d3d.command_list->ClearRenderTargetView(d3d.current_rtv, clearColor, 0, nullptr);
    d3d.command_list->ClearDepthStencilView(d3d.current_dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

static void d3d12_end_frame(void) {
    if (d3d.current_target_is_backbuffer) {
        d3d12_ensure_backbuffer_present();
    }

    ThrowIfFailed(d3d.command_list->Close());
    ID3D12CommandList *lists[] = { d3d.command_list.Get() };
    d3d.command_queue->ExecuteCommandLists(1, lists);

    UINT64 signalValue = d3d.next_fence_value++;
    ThrowIfFailed(d3d.command_queue->Signal(d3d.fence.Get(), signalValue));
    d3d.frame_slots[d3d.current_slot].fence_value = signalValue;

    d3d.command_list_open_for_pass = false;
}

static void d3d12_finish_render(void) {
}

static const char *d3d12_get_name(void) {
    return "DirectX 12";
}

static bool d3d12_is_legacy(void) {
    return false;
}

static void d3d12_shutdown(void) {
    for (int i = 0; i < D3D12_FRAME_RING_SIZE; i++) {
        UINT64 value = d3d.frame_slots[i].fence_value;
        if (value == 0) { continue; }
        UINT64 completed = d3d.fence->GetCompletedValue();
        if (completed < value) {
            HRESULT hr = d3d.fence->SetEventOnCompletion(value, d3d.fence_event);
            if (FAILED(hr)) {
                fprintf(stderr, "d3d12_shutdown: SetEventOnCompletion failed for fence slot %d (value %llu, completed %llu): 0x%08X - skipping this slot's wait.\n",
                    i, (unsigned long long)value, (unsigned long long)completed, (unsigned int)hr);
                fflush(stderr);
                continue;
            }
            if (WaitForSingleObject(d3d.fence_event, 2000) != WAIT_OBJECT_0) {
                fprintf(stderr, "d3d12_shutdown: timed out waiting on fence slot %d (value %llu, completed %llu) - abandoning wait so the process can still exit.\n",
                    i, (unsigned long long)value, (unsigned long long)d3d.fence->GetCompletedValue());
                break;
            }
        }
    }
}

} // namespace

struct GfxRenderingAPI gfx_direct3d12_api = {
    d3d12_z_is_from_0_to_1,
    d3d12_unload_shader,
    d3d12_load_shader,
    d3d12_remove_shaders,
    d3d12_create_and_load_new_shader,
    d3d12_create_or_load_post_process_shader,
    d3d12_lookup_shader,
    d3d12_lookup_shader_using_index,
    d3d12_shader_get_info,
    d3d12_create_framebuffer,
    d3d12_delete_framebuffer,
    d3d12_set_framebuffer,
    d3d12_reset_framebuffer,
    d3d12_set_uniform_buffer,
    d3d12_set_uniform,
    d3d12_new_texture,
    d3d12_select_texture,
    d3d12_bind_texture_raw,
    d3d12_upload_texture,
    d3d12_set_sampler_parameters,
    d3d12_set_depth_test,
    d3d12_set_depth_mask,
    d3d12_set_zmode_decal,
    d3d12_set_viewport,
    d3d12_set_scissor,
    d3d12_set_use_alpha,
    d3d12_set_vsync,
    d3d12_draw_triangles,
    gfx_d3d12_init,
    d3d12_on_resize,
    d3d12_start_frame,
    d3d12_end_frame,
    d3d12_finish_render,
    d3d12_get_name,
    d3d12_is_legacy,
    d3d12_shutdown,
};

#endif
