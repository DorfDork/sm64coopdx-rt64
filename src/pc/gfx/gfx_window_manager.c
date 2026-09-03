#include <SDL3/SDL.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "gfx_window_manager.h"
#include "gfx_window_opengl.h"
#include "gfx_window_sdl_gpu.h"
#include "gfx_window_dxgi.h"
#include "gfx_screen_config.h"
#include "gfx_pc.h"
#include "gfx_rt64.h"
#include "gfx_sdl_gpu.h"

#include "pc/pc_main.h"
#include "pc/configfile.h"
#include "pc/cliopts.h"
#include "pc/controller/controller_keyboard.h"
#include "pc/controller/controller_sdl.h"
#include "pc/controller/controller_bind_mapping.h"
#include "pc/utils/misc.h"
#include "pc/mods/mod_import.h"
#include "pc/rom_checker.h"
#include "pc/debuglog.h"

static struct GfxWindowBackendAPI *sBackends[GFX_WINDOW_BACKEND_COUNT] = {
    #if defined(_WIN32)
        [GFX_WINDOW_BACKEND_DIRECTX11] = &gfx_window_dxgi,
        [GFX_WINDOW_BACKEND_RT64] = &gfx_window_dxgi,
        [GFX_WINDOW_BACKEND_DIRECTX12] = &gfx_window_sdl_gpu,
        [GFX_WINDOW_BACKEND_VULKAN] = &gfx_window_sdl_gpu,
    #elif defined(__APPLE__)
        [GFX_WINDOW_BACKEND_METAL] = &gfx_window_sdl_gpu,
    #else
        [GFX_WINDOW_BACKEND_VULKAN] = &gfx_window_sdl_gpu,
    #endif
    [GFX_WINDOW_BACKEND_OPENGL] = &gfx_window_opengl,
    [GFX_WINDOW_BACKEND_DUMMY] = &gfx_window_dummy,
};

struct GfxWindowBackendInfo {
    const char *displayName;
    const char *cliName;
};

static const struct GfxWindowBackendInfo sBackendInfo[GFX_WINDOW_BACKEND_COUNT] = {
    #if defined(_WIN32)
        [GFX_WINDOW_BACKEND_DIRECTX11] = { "DirectX 11", "directx11" },
        [GFX_WINDOW_BACKEND_DIRECTX12] = { "DirectX 12", "directx12" },
        [GFX_WINDOW_BACKEND_RT64] = { "RT64", "rt64" },
        [GFX_WINDOW_BACKEND_VULKAN] = { "Vulkan", "vulkan" },
    #elif defined(__APPLE__)
        [GFX_WINDOW_BACKEND_METAL] = { "Metal", "metal" },
    #else
        [GFX_WINDOW_BACKEND_VULKAN] = { "Vulkan", "vulkan" },
    #endif
    [GFX_WINDOW_BACKEND_OPENGL] = { "OpenGL", "opengl" },
    [GFX_WINDOW_BACKEND_DUMMY] = { "Headless", "headless" },
};

static enum GfxWindowBackend sCurrBackend = GFX_WINDOW_BACKEND_DUMMY;

static SDL_Window *sSdlWindow;

// kept around so the window can be recreated with the same title when the backend is switched
static char sWindowTitle[WAPI_WINDOW_TITLE_BUFSIZ] = { 0 };

// the window a backend switch is leaving behind
static SDL_Window *sOutgoingSdlWindow = NULL;
static enum GfxWindowBackend sOutgoingBackend = GFX_WINDOW_BACKEND_DUMMY;

static kb_callback_t kb_key_down = NULL;
static kb_callback_t kb_key_up = NULL;
static void (*kb_all_keys_up)(void) = NULL;
static void (*kb_text_input)(char*) = NULL;
static void (*kb_text_editing)(char*, int) = NULL;

static void (*m_scroll)(float, float) = NULL;

#define IS_FULLSCREEN() ((SDL_GetWindowFlags(sSdlWindow) & SDL_WINDOW_FULLSCREEN) != 0)

void gfx_wm_set_window(SDL_Window *window) {
    sSdlWindow = window;
}

SDL_Window *gfx_wm_get_window(void) {
    return sSdlWindow;
}

static void gfx_wm_set_fullscreen(void) {
    if (configWindow.reset) {
        configWindow.fullscreen = false;
    }

    if (configWindow.fullscreen == IS_FULLSCREEN()) {
        return;
    }

    if (configWindow.fullscreen) {
        SDL_SetWindowFullscreen(sSdlWindow, true);
    } else {
        SDL_SetWindowFullscreen(sSdlWindow, false);
        SDL_ShowCursor();
        configWindow.exiting_fullscreen = true;
    }

    SDL_SyncWindow(sSdlWindow);

    sBackends[sCurrBackend]->set_fullscreen();
}

static void gfx_wm_reset_dimension_and_pos(void) {
    if (configWindow.exiting_fullscreen) {
        configWindow.exiting_fullscreen = false;
        SDL_HideCursor();
    }

    if (configWindow.reset) {
        configWindow.x = WAPI_WIN_CENTERPOS;
        configWindow.y = WAPI_WIN_CENTERPOS;
        configWindow.w = DESIRED_SCREEN_WIDTH;
        configWindow.h = DESIRED_SCREEN_HEIGHT;
        configWindow.reset = false;
    } else if (!configWindow.settings_changed) {
        return;
    }

    if (IS_FULLSCREEN()) {
        return;
    }

    int xpos = (configWindow.x == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.x;
    int ypos = (configWindow.y == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.y;

    SDL_SetWindowSize(sSdlWindow, configWindow.w, configWindow.h);
    SDL_SetWindowPosition(sSdlWindow, xpos, ypos);
}

#if defined(_WIN32)
static bool SDLCALL gfx_wm_windows_message_hook(void *userdata, MSG *msg) {
    (void)(userdata);
    gfx_rt64_handle_window_message(msg->hwnd, msg->message, (uintptr_t)(msg->wParam), (intptr_t)(msg->lParam));
    return true;
}
#endif

u32 gfx_wm_window_visibility_flag(void) {
    return (sOutgoingSdlWindow != NULL) ? SDL_WINDOW_HIDDEN : 0;
}

static bool gfx_wm_set_window_cloaked(SDL_Window *window, bool cloaked) {
#if defined(_WIN32)
    static HRESULT (WINAPI *sDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD) = NULL;
    if (sDwmSetWindowAttribute == NULL) {
        HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
        if (dwmapi == NULL) { return false; }
        *(FARPROC *)&sDwmSetWindowAttribute = GetProcAddress(dwmapi, "DwmSetWindowAttribute");
        if (sDwmSetWindowAttribute == NULL) { return false; }
    }

    HWND hWnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (hWnd == NULL) { return false; }

    BOOL value = cloaked ? TRUE : FALSE;
    return SUCCEEDED(sDwmSetWindowAttribute(hWnd, 13, &value, sizeof(value)));
#else
    (void)(window);
    (void)(cloaked);
    return false;
#endif
}

static void gfx_wm_apply_fullscreen_state(void) {
    gfx_wm_set_fullscreen();
    if (configWindow.fullscreen) {
        SDL_HideCursor();
    }
}

// brings up the window for sCurrBackend and puts it in the state the game expects
static void gfx_wm_open_window(void) {
    sBackends[sCurrBackend]->init(sWindowTitle);
    gfx_wm_apply_fullscreen_state();
    SDL_PumpEvents();
}

// releases the backend attached to a window, leaving the window itself alive
static void gfx_wm_detach_backend(enum GfxWindowBackend backend) {
    if (sBackends[backend]->shutdown) {
        sBackends[backend]->shutdown();
    }
}

static void gfx_wm_close_window(SDL_Window *window) {
    if (window) {
        SDL_DestroyWindow(window);
    }
}

void gfx_wm_init(const char *window_title) {
    if (gCLIOpts.headless) { return; }
#if defined(_WIN32)
    SetProcessDPIAware();
#endif

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_Init(SDL_INIT_VIDEO);

    snprintf(sWindowTitle, sizeof(sWindowTitle), "%s", window_title);

    sCurrBackend = gCLIOpts.backend < GFX_WINDOW_BACKEND_COUNT ? gCLIOpts.backend : configGraphicsBackend;
    gfx_wm_open_window();

    SDL_StopTextInput(sSdlWindow);

#if defined(_WIN32)
    SDL_SetWindowsMessageHook(gfx_wm_windows_message_hook, NULL);
#endif

    controller_bind_init();
}

enum GfxWindowBackend gfx_wm_get_backend(void) {
    return sCurrBackend;
}

const char *gfx_wm_get_backend_name(enum GfxWindowBackend backend) {
    if (backend >= GFX_WINDOW_BACKEND_COUNT) { return "Unknown"; }
    return sBackendInfo[backend].displayName;
}

enum GfxWindowBackend gfx_wm_get_backend_from_name(const char *name) {
#if defined(_WIN32)
    if (!strcasecmp(name, "directx")) { return GFX_WINDOW_BACKEND_DIRECTX11; }
#endif
    for (s32 i = 0; i < GFX_WINDOW_BACKEND_COUNT; i++) {
        if (i == GFX_WINDOW_BACKEND_DUMMY) { continue; }
        if (!strcasecmp(name, sBackendInfo[i].cliName)) { return (enum GfxWindowBackend)i; }
    }
    return GFX_WINDOW_BACKEND_COUNT;
}

bool gfx_wm_is_backend_supported(enum GfxWindowBackend backend) {
    switch (backend) {
#if defined(_WIN32)
        case GFX_WINDOW_BACKEND_OPENGL:
            return gfx_window_opengl_check_compatibility();
        case GFX_WINDOW_BACKEND_DIRECTX12:
        case GFX_WINDOW_BACKEND_VULKAN:
#elif defined(__APPLE__)
        case GFX_WINDOW_BACKEND_METAL:
#else
        case GFX_WINDOW_BACKEND_VULKAN:
#endif
            return gfx_sdl_gpu_is_backend_supported(backend);
        default:
            return backend < GFX_WINDOW_BACKEND_COUNT;
    }
}

void gfx_wm_switch_backend(enum GfxWindowBackend backend) {
    if (backend >= GFX_WINDOW_BACKEND_COUNT || backend == sCurrBackend) { return; }
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY || backend == GFX_WINDOW_BACKEND_DUMMY) { return; }

    if (sBackends[backend] == sBackends[sCurrBackend]) {
        sCurrBackend = backend;
        return;
    }

    // build the incoming window behind the outgoing one
    sOutgoingBackend = sCurrBackend;
    sOutgoingSdlWindow = sSdlWindow;

    gfx_wm_detach_backend(sOutgoingBackend);

    sCurrBackend = backend;
    sBackends[sCurrBackend]->init(sWindowTitle);

    if (gfx_wm_set_window_cloaked(sSdlWindow, true)) {
        SDL_ShowWindow(sSdlWindow);
    }

    SDL_PumpEvents();
}

bool gfx_wm_switch_backend_pending(void) {
    return sOutgoingSdlWindow != NULL;
}

void gfx_wm_finish_switch_backend(void) {
    if (sOutgoingSdlWindow == NULL) { return; }

    // reveal the incoming window before dropping the old one
    SDL_ShowWindow(sSdlWindow);
    gfx_wm_set_window_cloaked(sSdlWindow, false);
    SDL_RaiseWindow(sSdlWindow);
    gfx_wm_apply_fullscreen_state();

    gfx_wm_close_window(sOutgoingSdlWindow);
    sOutgoingSdlWindow = NULL;
    sOutgoingBackend = GFX_WINDOW_BACKEND_DUMMY;

    SDL_PumpEvents();
}

void gfx_wm_main_loop(void (*run_one_game_iter)(void)) {
    run_one_game_iter();
}

void gfx_wm_get_dimensions(uint32_t *width, uint32_t *height) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) {
        if (width) { *width = 320; }
        if (height) { *height = 240; }
        return;
    }
    int w, h;
    SDL_GetWindowSize(sSdlWindow, &w, &h);
    if (width) { *width = w; }
    if (height) { *height = h; }
}

static void gfx_wm_onkeydown(int scancode) {
    const bool *state = SDL_GetKeyboardState(NULL);

    if ((state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT]) && state[SDL_SCANCODE_RETURN]) {
        configWindow.fullscreen = !configWindow.fullscreen;
        configWindow.settings_changed = true;
        return;
    }

    if (kb_key_down) {
        kb_key_down(translate_sdl_scancode(scancode));
    }
}

static void gfx_wm_onkeyup(int scancode) {
    if (kb_key_up) {
        kb_key_up(translate_sdl_scancode(scancode));
    }
}

static void gfx_wm_onscroll(float x, float y) {
    if (m_scroll) {
        m_scroll(x, y);
    }
}

static void gfx_wm_ondropfile(char* path) {
#ifdef _WIN32
    char portable_path[SYS_MAX_PATH];
    if (sys_windows_short_path_from_mbs(portable_path, SYS_MAX_PATH, path)) {
        if (!gRomIsValid) {
            rom_on_drop_file(portable_path);
        } else if (gGameInited) {
            mod_import_file(portable_path);
        }
    }
#else
    if (!gRomIsValid) {
        rom_on_drop_file(path);
    } else if (gGameInited) {
        mod_import_file(path);
    }
#endif
}

static void gfx_wm_update_inspector_cursor(void) {
    static bool sWasInspectorActive = false;
    static bool sPrevRelativeMode = false;
    static bool sPrevCursorShown = true;

    bool isInspectorActive = gfx_rt64_inspector_active();
    if (isInspectorActive == sWasInspectorActive) {
        return;
    }

    sWasInspectorActive = isInspectorActive;
    if (isInspectorActive) {
        sPrevRelativeMode = SDL_GetWindowRelativeMouseMode(sSdlWindow);
        sPrevCursorShown = SDL_CursorVisible();
        SDL_SetWindowRelativeMouseMode(sSdlWindow, false);
        SDL_ShowCursor();
    } else {
        SDL_SetWindowRelativeMouseMode(sSdlWindow, sPrevRelativeMode);
        if (sPrevCursorShown) {
            SDL_ShowCursor();
        } else {
            SDL_HideCursor();
        }
    }
}

void gfx_wm_handle_events(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    gfx_wm_update_inspector_cursor();
    SDL_Event event = { 0 };
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_TEXT_INPUT:
                if (kb_text_input) { kb_text_input((char *)event.text.text); }
                break;
            case SDL_EVENT_TEXT_EDITING: //IME composition
                if (kb_text_editing) { kb_text_editing((char *)event.edit.text, event.edit.start); }
                break;
            case SDL_EVENT_KEY_DOWN:
                gfx_wm_onkeydown(event.key.scancode);
                break;
            case SDL_EVENT_KEY_UP:
                gfx_wm_onkeyup(event.key.scancode);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                gfx_wm_onscroll(event.wheel.x, event.wheel.y);
                break;
            case SDL_EVENT_WINDOW_MOVED:
                if (!configWindow.exiting_fullscreen && !IS_FULLSCREEN()) {
                    if (event.window.data1 >= 0) { configWindow.x = event.window.data1; }
                    if (event.window.data2 >= 0) { configWindow.y = event.window.data2; }
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                if (!IS_FULLSCREEN()) {
                    configWindow.w = event.window.data1;
                    configWindow.h = event.window.data2;
                }
                break;
            case SDL_EVENT_DROP_FILE:
                gfx_wm_ondropfile((char *)event.drop.data);
                break;
            case SDL_EVENT_QUIT:
                game_exit();
                break;
        }
        sBackends[sCurrBackend]->handle_events(event);
    }

    if (configWindow.settings_changed) {
        gfx_wm_set_fullscreen();
        gfx_wm_reset_dimension_and_pos();
        memset(&event, 0, sizeof(event));
        sBackends[sCurrBackend]->handle_events(event);
        configWindow.settings_changed = false;
    }
}

void gfx_wm_set_keyboard_callbacks(kb_callback_t on_key_down, kb_callback_t on_key_up,
    void (*on_all_keys_up)(void), void (*on_text_input)(char*), void (*on_text_editing)(char*, int)) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    kb_key_down = on_key_down;
    kb_key_up = on_key_up;
    kb_all_keys_up = on_all_keys_up;
    kb_text_input = on_text_input;
    kb_text_editing = on_text_editing;
}

void gfx_wm_set_scroll_callback(void (*on_scroll)(float, float)) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    m_scroll = on_scroll;
}

bool gfx_wm_start_frame(void) {
    return sBackends[sCurrBackend]->start_frame();
}

void gfx_wm_swap_buffers_begin(void) {
    sBackends[sCurrBackend]->swap_buffers_begin();
}

void gfx_wm_swap_buffers_end(void) {
    sBackends[sCurrBackend]->swap_buffers_end();
}

void gfx_wm_delay(u32 ms) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    SDL_Delay(ms);
}

int gfx_wm_get_max_msaa(void) {
    return sBackends[sCurrBackend]->get_max_msaa();
}

void gfx_wm_set_window_title(const char *title) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    snprintf(sWindowTitle, sizeof(sWindowTitle), "%s", title);
    SDL_SetWindowTitle(sSdlWindow, sWindowTitle);
}

void gfx_wm_reset_window_title(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    gfx_wm_set_window_title(TITLE);
}

void gfx_wm_shutdown(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    if (SDL_WasInit(0)) {
        SDL_GLContext ctx = SDL_GL_GetCurrentContext();
        if (ctx) { SDL_GL_DestroyContext(ctx); }
        if (sSdlWindow) { SDL_DestroyWindow(sSdlWindow); sSdlWindow = NULL; }
        SDL_Quit();
    }
}

bool gfx_wm_has_focus(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return true; }
    return (SDL_GetWindowFlags(sSdlWindow) & SDL_WINDOW_INPUT_FOCUS);
}

void gfx_wm_start_text_input(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    SDL_StartTextInput(sSdlWindow);
}

void gfx_wm_stop_text_input(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    SDL_StopTextInput(sSdlWindow);
}

char *gfx_wm_get_clipboard_text(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return ""; }
    static char sClipboardBuf[WAPI_CLIPBOARD_BUFSIZ];

    char *text = SDL_GetClipboardText();
    snprintf(sClipboardBuf, sizeof(sClipboardBuf), "%s", text);
    SDL_free(text);

    return sClipboardBuf;
}

void gfx_wm_set_clipboard_text(const char *text) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    SDL_SetClipboardText(text);
}

void gfx_wm_set_cursor_visible(bool visible) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    if (visible) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
}
