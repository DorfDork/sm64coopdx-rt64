#pragma once

#include <PR/ultratypes.h>
#include <SDL3/SDL.h>

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// special value for window position that signifies centered position
#define WAPI_WIN_CENTERPOS 0xFFFFFFFF

#define WAPI_CLIPBOARD_BUFSIZ 1024

#define WAPI_WINDOW_TITLE_BUFSIZ 256

typedef bool (*kb_callback_t)(int code);

enum GfxWindowBackend {
    #if defined(_WIN32)
        GFX_WINDOW_BACKEND_DIRECTX11,
        GFX_WINDOW_BACKEND_DIRECTX12,
        GFX_WINDOW_BACKEND_RT64,
        GFX_WINDOW_BACKEND_VULKAN,
    #elif defined(__APPLE__)
        GFX_WINDOW_BACKEND_METAL,
    #else
        GFX_WINDOW_BACKEND_VULKAN,
    #endif
    GFX_WINDOW_BACKEND_OPENGL,
    GFX_WINDOW_BACKEND_DUMMY,
    GFX_WINDOW_BACKEND_COUNT,
    GFX_WINDOW_BACKEND_MAX = GFX_WINDOW_BACKEND_COUNT - 1,
};

struct GfxWindowBackendAPI {
    void (*init)(const char *window_title);
    void (*set_fullscreen)(void);
    void (*handle_events)(SDL_Event);
    bool (*start_frame)(void);
    void (*swap_buffers_begin)(void);
    void (*swap_buffers_end)(void);
    int  (*get_max_msaa)(void);
    void (*shutdown)(void);
};

void gfx_wm_set_window(SDL_Window *window);
SDL_Window *gfx_wm_get_window(void);

void gfx_wm_init(const char *window_title);
enum GfxWindowBackend gfx_wm_get_backend(void);
const char *gfx_wm_get_backend_name(enum GfxWindowBackend backend);
enum GfxWindowBackend gfx_wm_get_backend_from_name(const char *name);
bool gfx_wm_is_backend_supported(enum GfxWindowBackend backend);
u32 gfx_wm_window_visibility_flag(void);
void gfx_wm_switch_backend(enum GfxWindowBackend backend);
bool gfx_wm_switch_backend_pending(void);
void gfx_wm_finish_switch_backend(void);
void gfx_wm_set_keyboard_callbacks(kb_callback_t on_key_down, kb_callback_t on_key_up, void (*on_all_keys_up)(void),
    void (*on_text_input)(char*), void (*on_text_editing)(char*, int));
void gfx_wm_set_scroll_callback(void (*on_scroll)(float, float));
void gfx_wm_main_loop(void (*run_one_game_iter)(void));
void gfx_wm_get_dimensions(uint32_t *width, uint32_t *height);
void gfx_wm_handle_events(void);
bool gfx_wm_start_frame(void);
void gfx_wm_swap_buffers_begin(void);
void gfx_wm_swap_buffers_end(void);
void gfx_wm_shutdown(void);
void gfx_wm_start_text_input(void);
void gfx_wm_stop_text_input(void);
char* gfx_wm_get_clipboard_text(void);
void gfx_wm_set_clipboard_text(const char*);
void gfx_wm_set_cursor_visible(bool);
void gfx_wm_delay(u32 ms);
int gfx_wm_get_max_msaa(void);
void gfx_wm_set_window_title(const char* title);
void gfx_wm_reset_window_title(void);
bool gfx_wm_has_focus(void);

#ifdef __cplusplus
}
#endif
