/* sdl_wmd.h — SDL 2/3 compatibility layer for wmdpaint.
 *
 * Include this header everywhere SDL is needed.  Provides:
 *   - The right SDL headers based on WMD_SDL_VERSION in config.h
 *   - WMD_* wrappers for functions whose signatures changed between versions
 *   - Backward-compat macro aliases for removed/renamed constants
 *
 * Main.c call-site convention (use these everywhere, not the bare SDL names):
 *   WMD_CreateRenderer(win)
 *   WMD_RenderCopy / WMD_RenderFillRect / WMD_RenderDrawRect /
 *   WMD_RenderDrawLine / WMD_RenderDrawPoint
 *   WMD_GetMouseState(px, py)       — always fills int*
 *   WMD_StartTextInput(win) / WMD_StopTextInput(win)
 *   WMD_KEYSYM(evt)                 — SDL_Keycode from a key event
 *   WMD_IS_WINDOW_RESIZE(evt)       — true on window-resize events
 *   WMD_RESIZE_W/H(evt)             — new w/h from resize event
 *
 * NK wrapper macros (defined in main.c after the renderer include):
 *   WMD_NK_INIT / WMD_NK_FONT_STASH_BEGIN / WMD_NK_FONT_STASH_END /
 *   WMD_NK_HANDLE_EVENT / WMD_NK_RENDER / WMD_NK_SHUTDOWN
 */

#ifndef SDL_WMD_H
#define SDL_WMD_H

#include "config.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* SDL3                                                                 */
/* ------------------------------------------------------------------ */
#if WMD_SDL_VERSION >= 3

/* Suppress SDL3's oldnames.h migration-hint macros so our aliases below
 * don't trigger redefinition warnings. */
#define SDL_DISABLE_OLD_NAMES
#include <SDL3/SDL.h>
#define WMD_SDL_VERSTR "SDL3"

/* SDL2 event-type names → SDL3 */
#define SDL_QUIT            SDL_EVENT_QUIT
#define SDL_KEYDOWN         SDL_EVENT_KEY_DOWN
#define SDL_KEYUP           SDL_EVENT_KEY_UP
#define SDL_TEXTINPUT       SDL_EVENT_TEXT_INPUT
#define SDL_MOUSEBUTTONDOWN SDL_EVENT_MOUSE_BUTTON_DOWN
#define SDL_MOUSEBUTTONUP   SDL_EVENT_MOUSE_BUTTON_UP
#define SDL_MOUSEMOTION     SDL_EVENT_MOUSE_MOTION
#define SDL_MOUSEWHEEL      SDL_EVENT_MOUSE_WHEEL

/* Window-resize: SDL3 has a dedicated event type, no sub-event field */
#define WMD_IS_WINDOW_RESIZE(e) ((e).type == SDL_EVENT_WINDOW_RESIZED)
#define WMD_RESIZE_W(e)         ((e).window.data1)
#define WMD_RESIZE_H(e)         ((e).window.data2)

/* Key symbol field moved from evt.key.keysym.sym to evt.key.key */
#define WMD_KEYSYM(e) ((e).key.key)

/* Modifier key names gained SDL_ prefix */
#define KMOD_CTRL  SDL_KMOD_CTRL
#define KMOD_SHIFT SDL_KMOD_SHIFT

/* SDL_WINDOW_SHOWN was removed; windows are shown by default */
#define SDL_WINDOW_SHOWN 0

/* SDL3 SDL_Init returns bool (true=success); provide SDL2-style failure test */
#define WMD_SDL_INIT_FAILED(r) (!(r))

/* SDL3 dropped x,y position params from SDL_CreateWindow */
#define WMD_CreateWindow(title,x,y,w,h,flags) \
    SDL_CreateWindow((title), (w), (h), (SDL_WindowFlags)(flags))

/* SDL_BUTTON(x) was renamed SDL_BUTTON_MASK(x) in SDL3 */
#define SDL_BUTTON(x) SDL_BUTTON_MASK(x)

/* Surface API renamed */
#define SDL_FreeSurface SDL_DestroySurface
#define SDL_CreateRGBSurfaceWithFormat(flags,w,h,depth,fmt) \
    SDL_CreateSurface((w), (h), (fmt))

/* SDL_bool was removed; provide compat for any headers that still use it */
#ifndef SDL_bool
typedef int SDL_bool;
#define SDL_TRUE  1
#define SDL_FALSE 0
#endif

/* Renderer creation: flags moved to properties; vsync set separately.
 * VULKANTARD_MODE tries the Vulkan backend first; falls back to SDL's
 * default choice if Vulkan isn't available. */
#if VULKANTARD_MODE
static int wmd_vulkan_fallback;
#endif

static inline SDL_Renderer *
WMD_CreateRenderer(SDL_Window *win)
{
#if VULKANTARD_MODE
    SDL_Renderer *r = SDL_CreateRenderer(win, "vulkan");
    if (!r) {
        wmd_vulkan_fallback = 1;
        r = SDL_CreateRenderer(win, NULL);
    }
#else
    SDL_Renderer *r = SDL_CreateRenderer(win, NULL);
#endif
    if (r) SDL_SetRenderVSync(r, 1);
    return r;
}

/* Mouse state: SDL3 uses float coords */
static inline uint32_t
WMD_GetMouseState(int *x, int *y)
{
    float fx, fy;
    uint32_t b = (uint32_t)SDL_GetMouseState(&fx, &fy);
    *x = (int)fx; *y = (int)fy;
    return b;
}
static inline uint32_t
WMD_GetGlobalMouseState(int *x, int *y)
{
    float fx, fy;
    uint32_t b = (uint32_t)SDL_GetGlobalMouseState(&fx, &fy);
    *x = (int)fx; *y = (int)fy;
    return b;
}

/* Text input: SDL3 requires the window */
#define WMD_StartTextInput(win) SDL_StartTextInput(win)
#define WMD_StopTextInput(win)  SDL_StopTextInput(win)

/* Must be called once per frame after nk_input_end so Nuklear can
 * start/stop SDL text input when edit widgets gain or lose focus. */
#define WMD_NK_UPDATE_TEXT_INPUT(ctx) nk_sdl_update_TextInput(ctx)

#define WMD_SetFullscreen(win, on) SDL_SetWindowFullscreen((win), (on))

/* GL context: SDL3 renamed Delete → Destroy */
#define WMD_GL_DestroyContext SDL_GL_DestroyContext

/* Render functions: int-coord SDL_Rect → float SDL_FRect */
static inline void
WMD_RenderCopy(SDL_Renderer *r, SDL_Texture *t,
               const SDL_Rect *src, const SDL_Rect *dst)
{
    SDL_FRect fs, fd;
    const SDL_FRect *ps = NULL, *pd = NULL;
    if (src) { fs = (SDL_FRect){(float)src->x,(float)src->y,(float)src->w,(float)src->h}; ps = &fs; }
    if (dst) { fd = (SDL_FRect){(float)dst->x,(float)dst->y,(float)dst->w,(float)dst->h}; pd = &fd; }
    SDL_RenderTexture(r, t, ps, pd);
}
static inline void
WMD_RenderFillRect(SDL_Renderer *r, const SDL_Rect *rect)
{
    if (!rect) { SDL_RenderFillRect(r, NULL); return; }
    SDL_FRect f = {(float)rect->x,(float)rect->y,(float)rect->w,(float)rect->h};
    SDL_RenderFillRect(r, &f);
}
static inline void
WMD_RenderDrawRect(SDL_Renderer *r, const SDL_Rect *rect)
{
    if (!rect) { SDL_RenderRect(r, NULL); return; }
    SDL_FRect f = {(float)rect->x,(float)rect->y,(float)rect->w,(float)rect->h};
    SDL_RenderRect(r, &f);
}
static inline void
WMD_RenderDrawLine(SDL_Renderer *r, int x1, int y1, int x2, int y2)
{ SDL_RenderLine(r, (float)x1, (float)y1, (float)x2, (float)y2); }
static inline void
WMD_RenderDrawPoint(SDL_Renderer *r, int x, int y)
{ SDL_RenderPoint(r, (float)x, (float)y); }

/* ------------------------------------------------------------------ */
/* SDL2 (reference implementation — thin pass-throughs)                */
/* ------------------------------------------------------------------ */
#else /* WMD_SDL_VERSION < 3 */

#include <SDL2/SDL.h>
#define WMD_SDL_VERSTR "SDL2"

#define WMD_IS_WINDOW_RESIZE(e) \
    ((e).type == SDL_WINDOWEVENT && (e).window.event == SDL_WINDOWEVENT_RESIZED)
#define WMD_RESIZE_W(e) ((e).window.data1)
#define WMD_RESIZE_H(e) ((e).window.data2)

#define WMD_KEYSYM(e) ((e).key.keysym.sym)

#define WMD_SDL_INIT_FAILED(r) ((r) < 0)

#define WMD_CreateWindow(title,x,y,w,h,flags) \
    SDL_CreateWindow((title), (x), (y), (w), (h), (flags))

#define WMD_CreateRenderer(win) \
    SDL_CreateRenderer((win), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)

static inline uint32_t
WMD_GetMouseState(int *x, int *y) { return SDL_GetMouseState(x, y); }
static inline uint32_t
WMD_GetGlobalMouseState(int *x, int *y) { return SDL_GetGlobalMouseState(x, y); }

#define WMD_StartTextInput(win) ((void)(win), SDL_StartTextInput())
#define WMD_StopTextInput(win)  ((void)(win), SDL_StopTextInput())

/* SDL2 manages text input automatically; no per-frame call needed. */
#define WMD_NK_UPDATE_TEXT_INPUT(ctx) ((void)(ctx))

#define WMD_SetFullscreen(win, on) \
    SDL_SetWindowFullscreen((win), (on) ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0)

/* GL context: SDL2 uses Delete, SDL3 uses Destroy */
#define WMD_GL_DestroyContext SDL_GL_DeleteContext

#define WMD_RenderCopy      SDL_RenderCopy
#define WMD_RenderFillRect  SDL_RenderFillRect
#define WMD_RenderDrawRect  SDL_RenderDrawRect
#define WMD_RenderDrawLine  SDL_RenderDrawLine
#define WMD_RenderDrawPoint SDL_RenderDrawPoint

#endif /* WMD_SDL_VERSION */

#endif /* SDL_WMD_H */
