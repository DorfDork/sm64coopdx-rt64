#include <SDL2/SDL.h>

#if defined(_WIN32)
#include <windows.h>
#include <SDL2/SDL_system.h>
#endif

#include <stdio.h>
#include <unistd.h>

#include "gfx_window_manager.h"
#include "gfx_window_opengl.h"
#include "gfx_window_metal.h"
#include "gfx_window_dxgi.h"
#include "gfx_screen_config.h"
#include "gfx_pc.h"
#include "gfx_rendering_api.h"

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
        [GFX_WINDOW_BACKEND_DIRECTX12] = &gfx_window_dxgi,
        [GFX_WINDOW_BACKEND_RT64] = &gfx_window_dxgi,
    #endif
    #if defined(__APPLE__)
        [GFX_WINDOW_BACKEND_METAL] = &gfx_window_metal,
    #endif
    [GFX_WINDOW_BACKEND_OPENGL] = &gfx_window_opengl,
    [GFX_WINDOW_BACKEND_DUMMY] = &gfx_window_dummy,
};

static enum GfxWindowBackend sCurrBackend = GFX_WINDOW_BACKEND_DUMMY;

static SDL_Window *sSdlWindow;

// kept around so the window can be recreated with the same title when the backend is switched
static char sWindowTitle[WAPI_WINDOW_TITLE_BUFSIZ] = { 0 };

static kb_callback_t kb_key_down = NULL;
static kb_callback_t kb_key_up = NULL;
static void (*kb_all_keys_up)(void) = NULL;
static void (*kb_text_input)(char*) = NULL;
static void (*kb_text_editing)(char*, int) = NULL;

static void (*m_scroll)(float, float) = NULL;

#define IS_FULLSCREEN() ((SDL_GetWindowFlags(sSdlWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)

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
        SDL_SetWindowFullscreen(sSdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(sSdlWindow, 0);
        SDL_ShowCursor(1);
        configWindow.exiting_fullscreen = true;
    }
    sBackends[sCurrBackend]->set_fullscreen();
}

static void gfx_wm_reset_dimension_and_pos(void) {
    if (configWindow.exiting_fullscreen) {
        configWindow.exiting_fullscreen = false;
        SDL_ShowCursor(0);
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

    int xpos = (configWindow.x == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.x;
    int ypos = (configWindow.y == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.y;

    SDL_SetWindowSize(sSdlWindow, configWindow.w, configWindow.h);
    SDL_SetWindowPosition(sSdlWindow, xpos, ypos);
}

#if defined(_WIN32)
static void SDLCALL gfx_wm_windows_message_hook(void *userdata, void *hWnd, unsigned int message, Uint64 wParam, Sint64 lParam) {
    (void)(userdata);
    struct GfxRenderingAPI *api = gfx_get_current_rendering_api();
    if (api && api->handle_window_message) {
        api->handle_window_message(hWnd, message, (uintptr_t)(wParam), (intptr_t)(lParam));
    }
}
#endif

// brings up the window for sCurrBackend and puts it in the state the game expects
static void gfx_wm_open_window(void) {
    sBackends[sCurrBackend]->init(sWindowTitle);

    gfx_wm_set_fullscreen();
    if (configWindow.fullscreen) {
        SDL_ShowCursor(SDL_DISABLE);
    }

    SDL_PumpEvents();
}

// tears the window back down, leaving SDL itself running
static void gfx_wm_close_window(void) {
    if (sBackends[sCurrBackend]->shutdown) {
        sBackends[sCurrBackend]->shutdown();
    }
    if (sSdlWindow) {
        SDL_DestroyWindow(sSdlWindow);
        sSdlWindow = NULL;
    }
}

void gfx_wm_init(const char *window_title) {
    if (gCLIOpts.headless) { return; }
#if defined(_WIN32)
    SetProcessDPIAware();
#endif

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_Init(SDL_INIT_VIDEO);

    SDL_StopTextInput();

    snprintf(sWindowTitle, sizeof(sWindowTitle), "%s", window_title);

#if defined(_WIN32) || defined(__APPLE__)
    sCurrBackend = gCLIOpts.backend < GFX_WINDOW_BACKEND_COUNT ? gCLIOpts.backend : configGraphicsBackend;
#else
    sCurrBackend = configGraphicsBackend;
#endif
    gfx_wm_open_window();

#if defined(_WIN32)
    SDL_SetWindowsMessageHook(gfx_wm_windows_message_hook, NULL);
#endif

    controller_bind_init();
}

enum GfxWindowBackend gfx_wm_get_backend(void) {
    return sCurrBackend;
}

void gfx_wm_switch_backend(enum GfxWindowBackend backend) {
    if (backend >= GFX_WINDOW_BACKEND_COUNT || backend == sCurrBackend) { return; }
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY || backend == GFX_WINDOW_BACKEND_DUMMY) { return; }

    if (!IS_FULLSCREEN()) {
        int xpos = 0, ypos = 0, width = 0, height = 0;
        SDL_GetWindowPosition(sSdlWindow, &xpos, &ypos);
        SDL_GetWindowSize(sSdlWindow, &width, &height);
        configWindow.x = xpos;
        configWindow.y = ypos;
        configWindow.w = width;
        configWindow.h = height;
    }

    gfx_wm_close_window();

    sCurrBackend = backend;
    gfx_wm_open_window();
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
    const Uint8 *state = SDL_GetKeyboardState(NULL);

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
    static SDL_bool sPrevRelativeMode = SDL_FALSE;
    static int sPrevCursorShown = SDL_ENABLE;

    struct GfxRenderingAPI *api = gfx_get_current_rendering_api();
    bool isInspectorActive = api && api->inspector_active && api->inspector_active();
    if (isInspectorActive == sWasInspectorActive) {
        return;
    }

    sWasInspectorActive = isInspectorActive;
    if (isInspectorActive) {
        sPrevRelativeMode = SDL_GetRelativeMouseMode();
        sPrevCursorShown = SDL_ShowCursor(SDL_QUERY);
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);
    } else {
        SDL_SetRelativeMouseMode(sPrevRelativeMode);
        SDL_ShowCursor(sPrevCursorShown);
    }
}

void gfx_wm_handle_events(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    gfx_wm_update_inspector_cursor();
    SDL_Event event = { 0 };
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_TEXTINPUT:
                if (kb_text_input) { kb_text_input(event.text.text); }
                break;
            case SDL_TEXTEDITING: //IME composition
                if (kb_text_editing) { kb_text_editing(event.edit.text,event.edit.start); }
                break;
            case SDL_KEYDOWN:
                gfx_wm_onkeydown(event.key.keysym.scancode);
                break;
            case SDL_KEYUP:
                gfx_wm_onkeyup(event.key.keysym.scancode);
                break;
            case SDL_MOUSEWHEEL:
                gfx_wm_onscroll(event.wheel.preciseX, event.wheel.preciseY);
                break;
            case SDL_WINDOWEVENT:
                if (!IS_FULLSCREEN()) {
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_MOVED:
                            if (!configWindow.exiting_fullscreen) {
                                if (event.window.data1 >= 0) { configWindow.x = event.window.data1; }
                                if (event.window.data2 >= 0) { configWindow.y = event.window.data2; }
                            }
                            break;
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                            configWindow.w = event.window.data1;
                            configWindow.h = event.window.data2;
                            break;
                    }
                }
                break;
            case SDL_DROPFILE:
                gfx_wm_ondropfile(event.drop.file);
                break;
            case SDL_QUIT:
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

double gfx_wm_get_time(void) {
    return sBackends[sCurrBackend]->get_time();
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
        gfx_wm_close_window();
        SDL_Quit();
    }
}

bool gfx_wm_has_focus(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return true; }
    return (SDL_GetWindowFlags(sSdlWindow) & SDL_WINDOW_INPUT_FOCUS);
}

void gfx_wm_start_text_input(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    SDL_StartTextInput();
}

void gfx_wm_stop_text_input(void) {
    if (sCurrBackend == GFX_WINDOW_BACKEND_DUMMY) { return; }
    SDL_StopTextInput();
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
    SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
}
