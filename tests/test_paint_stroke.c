/* Exercise the editor's real stroke callbacks and upload path headlessly. */
#define _DEFAULT_SOURCE
#include <assert.h>
#include "sdl_wmd.h"

static unsigned uploads;
#if WMD_SDL_VERSION >= 3
static bool
#else
static int
#endif
counted_upload(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch)
{
  uploads++;
  return SDL_UpdateTexture(texture, rect, pixels, pitch);
}
#define SDL_UpdateTexture counted_upload
#define main wmdpaint_application_main
#include "../src/wmdpaint.c"
#undef main
#undef SDL_UpdateTexture

/* Original per-stamp rendering, used as the output reference. */
static void
reference_stroke(int x0, int y0, int x1, int y1, void (*paint)(int, int))
{
  int dx = abs(x1-x0), dy = abs(y1-y0);
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx-dy;
  while (x0 != x1 || y0 != y1) {
    int e2 = err*2;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 < dx) { err += dx; y0 += sy; }
    paint(x0, y0);
  }
}

static void
test_stroke(void (*paint)(int, int), int monochrome, int reverse)
{
  uint16_t original[32*24*4], expected_layer[32*24*4], expected_canvas[40*30*4];
  unsigned char expected8[40*30*4];
  size_t i;
  unsigned before, reference_uploads;
  int x0 = reverse ? 38 : -3, y0 = reverse ? 25 : 2;
  int x1 = reverse ? -3 : 38, y1 = reverse ? 2 : 25;
  for (i = 0; i < sizeof original / sizeof *original; i++)
    original[i] = (uint16_t)(i*233+17);
  pirate_mode = monochrome;
  memcpy(layers[0]->px, original, sizeof original);
  dirty_valid = 0; canvas_composite();
  /* Capture monochrome initialization, which dithers the layer in place. */
  memcpy(original, layers[0]->px, sizeof original);
  srand(123);
  before = uploads;
  reference_stroke(x0, y0, x1, y1, paint);
  reference_uploads = uploads-before;
  memcpy(expected_layer, layers[0]->px, sizeof expected_layer);
  memcpy(expected_canvas, canvas->px, sizeof expected_canvas);
  memcpy(expected8, cimg_buf, sizeof expected8);
  memcpy(layers[0]->px, original, sizeof original);
  dirty_valid = 0; canvas_composite();
  srand(123);
  before = uploads;
  paint_stroke(x0, y0, x1, y1, paint);
  assert(memcmp(layers[0]->px, expected_layer, sizeof expected_layer) == 0);
  assert(memcmp(canvas->px, expected_canvas, sizeof expected_canvas) == 0);
  assert(memcmp(cimg_buf, expected8, sizeof expected8) == 0);
  assert(uploads-before == (monochrome ? reference_uploads : 1));
  assert(!composite_deferred && !composite_pending && !dirty_valid);
  before = uploads;
  paint_stroke(5, 6, 5, 6, paint);
  assert(uploads == before);
}

static void
selection_shape(int x, int y)
{
  shape_line_commit(x-2, y, x+2, y);
  shape_rect_commit(x-2, y-2, x+2, y+2);
  shape_circle_commit(x, y, 3);
}

static void
test_selection_painting(void)
{
  void (*tools[])(int, int) = {brush_paint, eraser_paint, decolor_paint,
    airbrush_paint, area_copypasta_paint, flood_fill, selection_shape};
  uint16_t original[32*24*4], painted[32*24*4];
  unsigned char *mask = sel_mask_new();
  int t, soft, mono, x, y;
  assert(mask);
  /* Irregular shape with a hole and a disconnected island. */
  for (y = 8; y < 16; y++) for (x = 7; x < 16; x++)
    mask[y*40+x] = (x+y)%3 != 0;
  mask[11*40+10] = 1; mask[11*40+22] = 1;
  sel_active = 1;
  brush_active = -1; brush_size = BRUSH_SIZE_MAX;
  for (t = 0; t < (int)(sizeof tools/sizeof *tools); t++)
    for (soft = 0; soft <= 1; soft++) for (mono = 0; mono <= 1; mono++) {
      int changed = 0;
      pirate_mode = 0; brush_shape = BRUSH_SQUARE;
      brush_mode = soft ? BRUSH_MODE_SOFT : BRUSH_MODE_PENCIL;
      air_mode = soft ? AIR_MODE_SOFT : AIR_MODE_SPRAY;
      for (y = 0; y < 24; y++) for (x = 0; x < 32; x++) {
        uint16_t *p = layers[0]->px+(y*32+x)*4;
        p[0] = p[1] = p[2] = 50000; p[3] = 65535;
      }
      pirate_mode = mono; dirty_valid = 0; canvas_composite();
      memcpy(original, layers[0]->px, sizeof original);
      snapshot_pixels(); srand(7); tools[t](10, 11);
      for (y = 0; y < 24; y++) for (x = 0; x < 32; x++) {
        int cx = x+3, cy = y-2;
        int selected = cy >= 0 && cy < 30 && cx < 40 && mask[cy*40+cx];
        int different = memcmp(layers[0]->px+(y*32+x)*4,
                               original+(y*32+x)*4, 8) != 0;
        if (!selected) assert(!different);
        changed += different;
      }
      /* A faint puff on white can dither back to white in monochrome. */
      assert(changed > 0 || (t == 3 && soft && mono));
      if (t == 5) /* Fill cannot cross an unselected gap to the island. */
        assert(memcmp(layers[0]->px+(13*32+19)*4,
                      original+(13*32+19)*4, 8) == 0);
      memcpy(painted, layers[0]->px, sizeof painted);
      do_undo(); assert(memcmp(layers[0]->px, original, sizeof original) == 0);
      do_redo(); assert(memcmp(layers[0]->px, painted, sizeof painted) == 0);
      history_clear();
    }
  pirate_mode = 0;
  /* A bucket seed outside the selection is a no-op. */
  memcpy(original, layers[0]->px, sizeof original);
  flood_fill(1, 1); assert(memcmp(original, layers[0]->px, sizeof original) == 0);
  /* An image brush observes the same mask. */
  {
    unsigned char bits[3] = {0xf8,0xf8,0xf8};
    brush_defs[0].w = 5; brush_defs[0].h = 3;
    brush_defs[0].row_bytes = 1; brush_defs[0].bits = bits;
    brush_def_n = 1; brush_active = 0; brush_mode = BRUSH_MODE_PENCIL;
    memcpy(original, layers[0]->px, sizeof original); brush_paint(10,11);
    for (y = 0; y < 24; y++) for (x = 0; x < 32; x++)
      if (!paint_selected(layers[0],x,y))
        assert(memcmp(layers[0]->px+(y*32+x)*4, original+(y*32+x)*4, 8) == 0);
    brush_active = -1; brush_def_n = 0; brush_defs[0].bits = NULL;
  }
  sel_clear();
  brush_mode = BRUSH_MODE_PENCIL;
  brush_paint(10,11); /* Large brushes are clipped to the small layer. */
  assert(layers[0]->px[0] == brush_r);
  assert(layers[0]->px[(32*24-1)*4] == brush_r);
}

int
main(void)
{
  void (*tools[])(int, int) = {brush_paint, eraser_paint, decolor_paint,
                              airbrush_paint, area_copypasta_paint};
  SDL_Surface *surface;
  size_t i;
  int t, mono, reverse, soft;
  assert(!WMD_SDL_INIT_FAILED(SDL_Init(0)));
  surface = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
  assert(surface);
  sdl_ren = SDL_CreateSoftwareRenderer(surface); assert(sdl_ren);
  canvas = ff_new(40, 30); layers[0] = layer_new(32, 24);
  assert(canvas && layers[0]);
  nlayers = 1; active_layer = 0;
  layers[0]->x = 3; layers[0]->y = -2;
  brush_size = 9; brush_r = 61000; brush_g = 27000;
  brush_b = 19000; brush_a = 45000;
  area_copypasta_w = 40; area_copypasta_h = 30;
  area_copypasta_px = malloc(40*30*4*sizeof(uint16_t)); assert(area_copypasta_px);
  for (i = 0; i < 40*30*4; i++) area_copypasta_px[i] = (uint16_t)(i*117);
  area_copypasta_source_set = 1;
  for (t = 0; t < (int)(sizeof tools/sizeof *tools); t++)
    for (mono = 0; mono <= 1; mono++) for (reverse = 0; reverse <= 1; reverse++)
      for (soft = 0; soft <= 1; soft++) {
        layers[0]->alpha_lock = soft;
        layers[0]->blend_mode = reverse ? LAYER_BLEND_MULTIPLY : LAYER_BLEND_NORMAL;
        brush_mode = soft ? BRUSH_MODE_SOFT : BRUSH_MODE_PENCIL;
        air_mode = soft ? AIR_MODE_SOFT : AIR_MODE_SPRAY;
        test_stroke(tools[t], mono, reverse);
      }
  {
    unsigned char bits[3] = {0xa8, 0xf8, 0x50};
    brush_defs[0].w = 5; brush_defs[0].h = 3;
    brush_defs[0].row_bytes = 1; brush_defs[0].bits = bits;
    brush_def_n = 1; brush_active = 0;
    for (mono = 0; mono <= 1; mono++) for (soft = 0; soft <= 1; soft++) {
      brush_mode = soft ? BRUSH_MODE_SOFT : BRUSH_MODE_PENCIL;
      test_stroke(brush_paint, mono, 0);
    }
    brush_active = -1; brush_def_n = 0; brush_defs[0].bits = NULL;
  }
  layers[0]->alpha_lock = 0;
  test_selection_painting();
  pirate_mode = 0; area_copypasta_source_set = 0;
  uploads = 0;
  paint_stroke(1, 1, 8, 8, area_copypasta_paint);
  assert(uploads == 0 && !composite_pending);
  canvas_image_free(); ff_free(canvas); layer_free(layers[0]);
  free(area_copypasta_px);
  SDL_DestroyRenderer(sdl_ren); SDL_FreeSurface(surface); SDL_Quit();
  puts("paint strokes: batching, selection masks, large brushes, undo/redo, monochrome ok");
  return 0;
}
