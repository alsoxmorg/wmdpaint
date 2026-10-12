#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdl_wmd.h"
#include "config_paint.h"
#include "bitmap_font_data.h"
static SDL_Window *sdl_win;
static SDL_Renderer *sdl_ren;
static int win_w = 128, win_h = 64;
#include "busy.h"

static SDL_Surface *
read_frame(void)
{
#if WMD_SDL_VERSION >= 3
  SDL_Surface *raw = SDL_RenderReadPixels(sdl_ren, NULL);
  SDL_Surface *rgba;
  assert(raw);
  rgba = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(raw);
  assert(rgba); return rgba;
#else
  SDL_Surface *rgba = SDL_CreateRGBSurfaceWithFormat(0, win_w, win_h, 32,
                                                    SDL_PIXELFORMAT_RGBA32);
  assert(rgba);
  assert(SDL_RenderReadPixels(sdl_ren, NULL, SDL_PIXELFORMAT_RGBA32,
                             rgba->pixels, rgba->pitch) == 0);
  return rgba;
#endif
}

int main(void)
{
  SDL_Surface *screen, *working, *spinning, *complete;
  SDL_Texture *other;
  SDL_Rect clip = {1, 2, 20, 20}, restored;
  unsigned char r, g, b, a;
  int x, y, changed = 0;
  {
    size_t bytes = 3 * 1024 * 1024 + 17, i;
    unsigned char *source = malloc(bytes), *copy = malloc(bytes);
    assert(source && copy);
    for (i = 0; i < bytes; i++) source[i] = (unsigned char)(i * 37);
    busy_copy(copy, source, bytes); assert(memcmp(copy, source, bytes) == 0);
    free(source); free(copy);
  }
  assert(!WMD_SDL_INIT_FAILED(SDL_Init(0)));
  screen = SDL_CreateRGBSurfaceWithFormat(0, win_w, win_h, 32, SDL_PIXELFORMAT_RGBA32);
  assert(screen);
  sdl_ren = SDL_CreateSoftwareRenderer(screen); assert(sdl_ren);
  busy_frame_begin(); assert(busy_frame && SDL_GetRenderTarget(sdl_ren) == busy_frame);
  SDL_SetRenderDrawColor(sdl_ren, 10, 20, 30, 255); SDL_RenderClear(sdl_ren);
  busy_frame_present(); assert(busy_frame_ready);
  busy_begin(); busy_checkpoint(); assert(!busy_visible);
  busy_started -= 200; busy_last_draw -= 200;
  busy_checkpoint(); assert(busy_visible);
  working = read_frame();
  busy_draw(busy_last_draw + 100);
  spinning = read_frame();
  for (y = 0; y < win_h; y++) for (x = 0; x < win_w; x++) {
    unsigned char *p = (unsigned char *)working->pixels + y * working->pitch + x * 4;
    unsigned char *q = (unsigned char *)spinning->pixels + y * spinning->pitch + x * 4;
    if (y < win_h - STATUS_H || x < win_w - 92)
      assert(p[0] == 10 && p[1] == 20 && p[2] == 30 && p[3] == 255);
    if (memcmp(p, q, 4)) changed++;
  }
  assert(changed > 0); /* The spinner advances while the cached UI stays intact. */
  other = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_RGBA8888,
                            SDL_TEXTUREACCESS_TARGET, win_w, win_h); assert(other);
  SDL_SetRenderTarget(sdl_ren, other); SDL_SetRenderClipRect(sdl_ren, &clip);
  SDL_SetRenderDrawColor(sdl_ren, 1, 2, 3, 4);
  busy_draw(busy_last_draw + 100);
  assert(SDL_GetRenderTarget(sdl_ren) == other && SDL_RenderClipEnabled(sdl_ren));
  SDL_GetRenderClipRect(sdl_ren, &restored); assert(memcmp(&clip, &restored, sizeof clip) == 0);
  SDL_GetRenderDrawColor(sdl_ren, &r, &g, &b, &a); assert(r == 1 && g == 2 && b == 3 && a == 4);
  SDL_SetRenderTarget(sdl_ren, NULL); SDL_SetRenderClipRect(sdl_ren, NULL);
  busy_end(); busy_frame_begin();
  SDL_SetRenderDrawColor(sdl_ren, 40, 50, 60, 255); SDL_RenderClear(sdl_ren);
  busy_frame_present(); complete = read_frame();
  for (y = 0; y < win_h; y++) for (x = 0; x < win_w; x++) {
    unsigned char *p = (unsigned char *)complete->pixels + y * complete->pitch + x * 4;
    assert(p[0] == 40 && p[1] == 50 && p[2] == 60);
  }
  busy_begin(); busy_file_hint(); assert(busy_visible); busy_end();
  busy_free(); assert(!busy_frame && !busy_frame_ready && !busy_active);
  SDL_DestroyTexture(other);
  SDL_FreeSurface(working); SDL_FreeSurface(spinning); SDL_FreeSurface(complete);
  SDL_DestroyRenderer(sdl_ren); SDL_FreeSurface(screen); SDL_Quit();
  puts("busy indicator: fast-job delay, animation, cached frame, renderer state, completion ok");
  return 0;
}
