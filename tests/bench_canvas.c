/* Headless timings for the actual CPU compositor/converter and SDL uploads.
 * Software uploads deliberately exclude display, GPU, and presentation costs. */
#include <stdio.h>
#include <stdlib.h>
#include "sdl_wmd.h"
#include "canvas_render.h"

static double
milliseconds(uint64_t ticks)
{
  return (double)ticks * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

static void
run(SDL_Renderer *renderer, int w, int h, int count, int sparse, int dirty)
{
  Layer *stack[16];
  FFImage *canvas = ff_new((uint32_t)w, (uint32_t)h);
  unsigned char *buf = malloc((size_t)w * h * 4);
  SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STREAMING, w, h);
  uint64_t blend = 0, convert = 0, upload = 0, start;
  int i, j, loops = dirty ? 200 : 5;
  SDL_Rect rect = {dirty ? w / 2 : 0, dirty ? h / 2 : 0,
                   dirty ? 64 : w, dirty ? 64 : h};
  if (!canvas || !buf || !tex) {
    fprintf(stderr, "benchmark allocation failed: %s\n", SDL_GetError());
    exit(1);
  }
  for (i = 0; i < count; i++) {
    size_t p, n = (size_t)w * h;
    stack[i] = layer_new((uint32_t)w, (uint32_t)h);
    if (!stack[i]) { fputs("layer allocation failed\n", stderr); exit(1); }
    if (sparse == 2) {
      stack[i]->blend_mode = i % LAYER_BLEND_COUNT;
      stack[i]->opacity = 0.75f;
    }
    /* Also exercise translucent layers with all four blend modes. */
    for (p = 0; p < n; p++) {
      uint16_t *px = stack[i]->px + p * 4;
      px[0] = (uint16_t)(p * 13 + i * 101);
      px[1] = (uint16_t)(p * 29 + i * 313);
      px[2] = (uint16_t)(p * 7 + i * 919);
      px[3] = sparse == 2 ? 32768 : !sparse || i == 0 ? 65535 :
              ((p + (size_t)i) % 8 == 0 ? 65535 :
               ((p + (size_t)i) % 8 == 1 ? 32768 : 0));
    }
  }
  /* Initialize the full persistent buffer/texture, then warm the update path. */
  canvas_flatten_rect(canvas, stack, count, 0, 0, w, h);
  canvas_rgba8_rect(canvas, buf, 0, 0, w, h);
  if (WMD_SDL_INIT_FAILED(SDL_UpdateTexture(tex, NULL, buf, w * 4))) {
    fprintf(stderr, "texture upload failed: %s\n", SDL_GetError()); exit(1);
  }
  for (j = -1; j < loops; j++) {
    uint64_t a, b, c;
    start = SDL_GetPerformanceCounter();
    canvas_flatten_rect(canvas, stack, count, rect.x, rect.y,
                        rect.x + rect.w, rect.y + rect.h);
    a = SDL_GetPerformanceCounter();
    canvas_rgba8_rect(canvas, buf, rect.x, rect.y,
                      rect.x + rect.w, rect.y + rect.h);
    b = SDL_GetPerformanceCounter();
    if (WMD_SDL_INIT_FAILED(SDL_UpdateTexture(tex, &rect,
        buf + ((size_t)rect.y * w + rect.x) * 4, w * 4))) {
      fprintf(stderr, "texture upload failed: %s\n", SDL_GetError()); exit(1);
    }
    c = SDL_GetPerformanceCounter();
    if (j >= 0) { blend += a-start; convert += b-a; upload += c-b; }
  }
  printf("%dx%d %2d layers %-6s %-5s: composite %8.3f ms, convert %7.3f ms, upload %7.3f ms\n",
         w, h, count, sparse == 2 ? "mixed" : sparse ? "sparse" : "opaque", dirty ? "64x64" : "full",
         milliseconds(blend)/loops, milliseconds(convert)/loops,
         milliseconds(upload)/loops);
  if (sparse == 1 && dirty && count == 16) {
    int batch, step;
    for (batch = 0; batch <= 1; batch++) {
      uint64_t elapsed = 0;
      for (j = 0; j < 5; j++) {
        start = SDL_GetPerformanceCounter();
        for (step = 0; step < (batch ? 1 : 64); step++) {
          SDL_Rect r = {w/2 + step, h/2, batch ? 127 : 64, 64};
          canvas_flatten_rect(canvas, stack, count, r.x, r.y, r.x+r.w, r.y+r.h);
          canvas_rgba8_rect(canvas, buf, r.x, r.y, r.x+r.w, r.y+r.h);
          if (WMD_SDL_INIT_FAILED(SDL_UpdateTexture(tex, &r,
              buf + ((size_t)r.y*w+r.x)*4, w*4))) {
            fprintf(stderr, "texture upload failed: %s\n", SDL_GetError()); exit(1);
          }
        }
        elapsed += SDL_GetPerformanceCounter()-start;
      }
      printf("64-stamp horizontal stroke render work (%s, no painting/undo): %.3f ms\n",
             batch ? "one union update" : "per-stamp updates", milliseconds(elapsed)/5);
    }
  }
  for (i = 0; i < count; i++) layer_free(stack[i]);
  ff_free(canvas); free(buf); SDL_DestroyTexture(tex);
}

int
main(int argc, char **argv)
{
  SDL_Surface *surface;
  SDL_Renderer *renderer;
  int sparse, dirty;
  /* A software renderer needs no window, display server, or video driver. */
  if (WMD_SDL_INIT_FAILED(SDL_Init(0))) return 1;
  surface = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
  renderer = surface ? SDL_CreateSoftwareRenderer(surface) : NULL;
  if (!renderer) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
  puts("Mean per update; SDL software renderer, no GPU/presentation or undo cost.");
  if (argc > 2 || (argc == 2 && strcmp(argv[1], "mixed") != 0)) {
    fputs("usage: bench_canvas [mixed]\n", stderr); return 1;
  }
  for (sparse = argc == 2 ? 2 : 0; sparse <= (argc == 2 ? 2 : 1); sparse++)
    for (dirty = 0; dirty <= 1; dirty++) {
      run(renderer, 3072, 2048, 4, sparse, dirty);
      run(renderer, 2048, 2048, 16, sparse, dirty);
    }
  SDL_DestroyRenderer(renderer); SDL_FreeSurface(surface); SDL_Quit();
  return 0;
}
