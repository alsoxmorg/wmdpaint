#define _DEFAULT_SOURCE
#include <stdlib.h>
static int fail_malloc_after = -1;
static void *test_malloc(size_t n)
{
  if (fail_malloc_after == 0) return NULL;
  if (fail_malloc_after > 0) fail_malloc_after--;
  return malloc(n);
}
#define malloc test_malloc
#define main wmdpaint_application_main
#include "../src/wmdpaint.c"
#undef main
#undef malloc
#include <assert.h>

static void reset(void)
{
  int i;
  history_clear();
  for (i = 0; i < nlayers; i++) layer_free(layers[i]);
  ff_free(canvas); canvas = ff_new(8, 6); assert(canvas);
  nlayers = 2; active_layer = 0; flatten_expand = 0;
  layers[0] = layer_new(5, 4); layers[1] = layer_new(2, 2);
  assert(layers[0] && layers[1]);
  layers[0]->x = -1; layers[0]->y = -2;
  layers[1]->x = 6; layers[1]->y = 4;
  for (i = 0; i < 80; i++) layers[0]->px[i] = (uint16_t)(100+i*193);
  for (i = 0; i < 16; i++) layers[1]->px[i] = (uint16_t)(1000+i*17);
  strcpy(layers[0]->name, "offset"); layers[0]->opacity = 40000;
  dirty_valid = 0; canvas_composite();
}

int main(void)
{
  SDL_Surface *surface; uint16_t pixels[80], cropped[8];
  assert(!WMD_SDL_INIT_FAILED(SDL_Init(0)));
  surface = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
  assert(surface); sdl_ren = SDL_CreateSoftwareRenderer(surface); assert(sdl_ren);
  reset(); memcpy(pixels, layers[0]->px, sizeof pixels);
  memcpy(cropped, pixels + (3*5+2)*4, sizeof cropped);
  snapshot_canvas(); canvas_crop(1, 1, 3, 3);
  assert(canvas->w == 3 && canvas->h == 3);
  assert(layers[0]->w == 3 && layers[0]->h == 1);
  assert(layers[0]->x == 0 && layers[0]->y == 0);
  assert(memcmp(layers[0]->px, cropped, sizeof cropped) == 0);
  assert(layers[1]->w == 1 && layers[1]->h == 1 && layers[1]->px[3] == 0);
  do_undo(); assert(canvas->w == 8 && layers[0]->x == -1 && layers[0]->y == -2);
  assert(memcmp(layers[0]->px, pixels, sizeof pixels) == 0);
  assert(layers[0]->opacity == 40000 && strcmp(layers[0]->name, "offset") == 0);
  do_redo(); assert(canvas->w == 3 && layers[0]->h == 1);
  reset(); snapshot_canvas(); canvas_extend(3, 2);
  assert(canvas->w == 3 && layers[0]->w == 5 && layers[0]->x == -1);
  assert(memcmp(layers[0]->px, pixels, sizeof pixels) == 0);
  do_undo(); assert(canvas->w == 8); do_redo(); assert(canvas->w == 3);
  reset(); snapshot_canvas(); img_resize(16, 12);
  assert(canvas->w == 16 && canvas->h == 12);
  assert(layers[0]->w == 10 && layers[0]->h == 8);
  assert(layers[0]->x == -2 && layers[0]->y == -4);
  assert(layers[1]->x == 12 && layers[1]->y == 8);
  assert(memcmp(layers[0]->px, pixels, 8) == 0);
  do_undo(); assert(layers[0]->x == -1 && layers[0]->w == 5);
  assert(memcmp(layers[0]->px, pixels, sizeof pixels) == 0);
  do_redo(); assert(layers[0]->x == -2 && canvas->w == 16);
  reset();
  /* Canvas allocation and the first layer succeed; the second layer fails. */
  fail_malloc_after = 2;
  modified = 0; img_resize(16, 12); fail_malloc_after = -1;
  assert(canvas->w == 8 && canvas->h == 6 && !modified);
  assert(layers[0]->x == -1 && layers[0]->w == 5);
  assert(layers[1]->x == 6 && layers[1]->w == 2);
  assert(memcmp(layers[0]->px, pixels, sizeof pixels) == 0);
  img_resize(8, 6);
  assert(layers[0]->x == -1 && layers[0]->y == -2);
  assert(!scale_px_bilinear(pixels, 5, 4, 0, 1));
  assert(!scale_px_bilinear(pixels, 5, 4, UINT32_MAX, UINT32_MAX));
  reset(); layer_add_new(); assert(nlayers == 3); do_undo(); assert(nlayers == 2);
  do_redo(); assert(nlayers == 3); layer_delete(1); assert(nlayers == 2);
  do_undo(); assert(nlayers == 3); do_redo(); assert(nlayers == 2);
  reset(); layer_move_up(0); assert(layers[1]->x == -1);
  do_undo(); assert(layers[0]->x == -1); do_redo(); assert(layers[1]->x == -1);
  snapshot_pixels(); layers[active_layer]->px[0] = 42;
  assert(redo_count == 0);
  reset();
  if (access("/dev/full", W_OK) == 0) {
    strcpy(filepath, "/dev/full"); backup_enabled = 0;
    modified = 1; io_error[0] = 0; do_save();
    assert(modified && io_error[0]); filepath[0] = 0;
  }
  {
    const uint16_t ramp[] = {0,0,0,0, 65534,65534,65534,65534};
    uint16_t *scaled = scale_px_bilinear(ramp, 2, 1, 3, 1);
    assert(scaled && scaled[0] == 0 && scaled[4] == 32767 && scaled[8] == 65534);
    free(scaled);
  }
  history_clear();
  while (nlayers) layer_free(layers[--nlayers]);
  ff_free(canvas); canvas_image_free(); SDL_DestroyRenderer(sdl_ren);
  SDL_FreeSurface(surface); SDL_Quit();
  puts("document operations: crop, canvas sizing, offsets, pixels, layer history ok");
  return 0;
}
