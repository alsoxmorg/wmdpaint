/* Real resize callbacks, shared hit testing, and undo under headless SDL. */
#define main wmdpaint_application_main
#include "../src/wmdpaint.c"
#undef main
#include <assert.h>

static uint16_t original[8*4*4];

static void
reset_document(void)
{
  int i;
  history_clear();
  for (i = 0; i < nlayers; i++) layer_free(layers[i]);
  ff_free(canvas); canvas = ff_new(256, 256); assert(canvas);
  flatten_expand = 0;
  nlayers = 1; active_layer = 0;
  layers[0] = layer_new(8, 4); assert(layers[0]);
  layers[0]->x = 10; layers[0]->y = 12;
  memcpy(layers[0]->px, original, sizeof original);
  active_tool = TOOL_RESIZE; scale_keep_aspect = 1;
  dirty_valid = 0; canvas_composite();
}

static void
corner_for_size(int handle, int w, int h, int *x, int *y)
{
  *x = (handle == 0 || handle == 3) ? 18-w : 10+w;
  *y = (handle == 0 || handle == 1) ? 16-h : 12+h;
}

static void
assert_anchor(int handle)
{
  Layer *l = layers[0];
  assert((handle == 0 || handle == 3) ? l->x+(int)l->w == 18 : l->x == 10);
  assert((handle == 0 || handle == 1) ? l->y+(int)l->h == 16 : l->y == 12);
}

static void
assert_original(void)
{
  Layer *l = layers[0];
  assert(l->w == 8 && l->h == 4 && l->x == 10 && l->y == 12);
  assert(memcmp(l->px, original, sizeof original) == 0);
}

static void
test_corners(void)
{
  int handle, locked;
  for (handle = 0; handle < 4; handle++) for (locked = 0; locked <= 1; locked++) {
    int x, y;
    uint16_t *expected;
    reset_document(); scale_keep_aspect = locked;
    corner_for_size(handle, 8, 4, &x, &y);
    assert(scale_start_drag(layers[0], handle, x, y));
    assert(!scale_apply(layers[0], x, y) && undo_count == 0);
    corner_for_size(handle, 12, locked ? 4 : 7, &x, &y);
    assert(scale_apply(layers[0], x, y));
    assert(layers[0]->w == 12 && layers[0]->h == (locked ? 6u : 7u));
    assert_anchor(handle); assert(undo_count == 1);
    corner_for_size(handle, 4, locked ? 4 : 2, &x, &y);
    assert(scale_apply(layers[0], x, y));
    assert(layers[0]->w == 4 && layers[0]->h == 2); assert_anchor(handle);
    corner_for_size(handle, 12, locked ? 4 : 7, &x, &y);
    assert(scale_apply(layers[0], x, y) && undo_count == 1);
    expected = scale_px_bilinear(original, 8, 4, 12, locked ? 6 : 7);
    assert(expected);
    assert(memcmp(layers[0]->px, expected, 12*(locked ? 6 : 7)*8) == 0);
    scale_reset(); move_snapshot_taken = 0;
    do_undo(); assert_original(); assert(undo_count == 0 && redo_count == 1);
    do_redo(); assert(undo_count == 1 && redo_count == 0); assert_anchor(handle);
    assert(memcmp(layers[0]->px, expected, 12*(locked ? 6 : 7)*8) == 0);
    free(expected);
  }
}

static void
test_ratio_and_minimum(void)
{
  int w, h, handle, x, y;
  w = 13; h = 4; resize_match_aspect(&w, &h, 8, 4, 1); assert(w == 13 && h == 7);
  w = 8; h = 5; resize_match_aspect(&w, &h, 8, 4, 0); assert(w == 10 && h == 5);
  w = 20; h = 8; resize_match_aspect(&w, &h, 4, 8, 1); assert(w == 20 && h == 40);
  w = LAYER_RESIZE_MAX; h = 8; resize_match_aspect(&w, &h, 4, 8, 1);
  assert(w == (LAYER_RESIZE_MAX+1)/2 && h == LAYER_RESIZE_MAX);
  w = 1; h = 1; resize_match_aspect(&w, &h, 8, 4, 1); assert(w == 1 && h == 1);
  for (handle = 0; handle < 4; handle++) {
    reset_document();
    corner_for_size(handle, 8, 4, &x, &y);
    assert(scale_start_drag(layers[0], handle, x, y));
    /* Vertical movement also drives uniform resizing. */
    corner_for_size(handle, 8, 10, &x, &y);
    assert(scale_apply(layers[0], x, y));
    assert(layers[0]->w == 20 && layers[0]->h == 10); assert_anchor(handle);
    scale_keep_aspect = 0;
    corner_for_size(handle, -5, -8, &x, &y);
    assert(scale_apply(layers[0], x, y));
    assert(layers[0]->w == 1 && layers[0]->h == 1); assert_anchor(handle);
    scale_cancel(); assert_original(); assert(!scale_dragging && !scale_orig_px);
  }
}

static void
test_input_and_hit_testing(void)
{
  int sx[4], sy[4], k;
  reset_document(); assert(scale_tool_active());
  move_mode = MOVE_MODE_SCALE; active_tool = TOOL_MOVE; assert(scale_tool_active());
  move_mode = MOVE_MODE_MOVE; assert(!scale_tool_active()); active_tool = TOOL_RESIZE;
  scale_handle_positions(layers[0], 100, 50, 256, 256, sx, sy);
  for (k = 0; k < 4; k++) {
    assert(scale_hit_handle(layers[0], 100, 50, 256, 256, sx[k], sy[k]) == k);
    assert(scale_hit_handle(layers[0], 100, 50, 1024, 1024,
                           100+(k == 1 || k == 2 ? 18 : 10)*4,
                           50+(k == 2 || k == 3 ? 16 : 12)*4) == k);
  }
  assert(scale_hit_handle(layers[0], 100, 50, 256, 256, 0, 0) == -1);
  /* Grab slightly away from BR, then hold still: no jump and no undo. */
  scale_drag_update(1, 1, 0, 20, 17, 100, 50, 1024, 1024, 180, 118);
  assert(scale_dragging && scale_handle == 2 && undo_count == 0);
  scale_drag_update(0, 1, 0, 20, 17, 100, 50, 1024, 1024, 180, 118);
  assert_original(); assert(undo_count == 0);
  /* Final release coordinates are applied even without an intermediate frame. */
  scale_drag_update(0, 0, 1, 24, 17, 100, 50, 1024, 1024, 196, 118);
  assert(layers[0]->w == 12 && layers[0]->h == 6 && undo_count == 1);
  assert(!scale_dragging && !scale_orig_px); assert_anchor(2);
  do_undo(); assert_original();
  /* An untouched drag canceled with Escape must preserve existing redo. */
  assert(scale_start_drag(layers[0], 2, 18, 16));
  scale_cancel(); assert_original(); assert(redo_count == 1 && undo_count == 0);
  /* Starting over after cancellation must create exactly one new action. */
  assert(scale_start_drag(layers[0], 0, 10, 12));
  assert(scale_apply(layers[0], 6, 10));
  assert(undo_count == 1 && redo_count == 0);
  scale_cancel(); assert_original();
  assert(!scale_dragging && !scale_orig_px && !move_snapshot_taken);
  assert(scale_start_drag(layers[0], 2, 18, 16));
  assert(scale_apply(layers[0], 14, 14));
  assert(scale_apply(layers[0], 18, 16));
  assert_original(); scale_reset(); move_snapshot_taken = 0;
}

static void
test_history_after_layer_selection(void)
{
  uint16_t other[4*3*4];
  size_t i;
  reset_document();
  layers[1] = layer_new(4, 3); assert(layers[1]); nlayers = 2;
  for (i = 0; i < sizeof other/sizeof *other; i++) other[i] = (uint16_t)(i*197);
  memcpy(layers[1]->px, other, sizeof other);
  assert(scale_start_drag(layers[0], 2, 18, 16));
  assert(scale_apply(layers[0], 22, 18));
  scale_reset(); move_snapshot_taken = 0;
  active_layer = 1; do_undo(); assert_original();
  active_layer = 1; do_redo();
  assert(layers[0]->w == 12 && layers[0]->h == 6);
  assert(layers[1]->w == 4 && layers[1]->h == 3);
  assert(memcmp(layers[1]->px, other, sizeof other) == 0);
}

static void
test_canvas_expansion(void)
{
  int cancel;
  for (cancel = 0; cancel <= 1; cancel++) {
    reset_document(); flatten_expand = 1;
    scale_drag_update(1, 1, 0, 10, 12, 100, 50, 256, 256, 110, 62);
    scale_drag_update(0, 1, 0, -12, 1, 100, 50, 256, 256, 88, 51);
    assert(layers[0]->x == -12 && layers[0]->w == 30 && layers[0]->h == 15);
    assert(canvas->w == 256 && undo_count == 1 && undo_stack[0].tag == UR_CANVAS);
    if (cancel) {
      scale_cancel(); assert_original(); assert(canvas->w == 256);
    } else {
      scale_drag_update(0, 0, 1, -12, 1, 100, 50, 256, 256, 88, 51);
      assert(canvas->w == 268 && layers[0]->x == 0 && undo_count == 1);
      do_undo(); assert_original(); assert(canvas->w == 256);
      do_redo(); assert(canvas->w == 268 && layers[0]->w == 30 && layers[0]->x == 0);
    }
  }
  flatten_expand = 0;
}

static void
test_nearest(void)
{
  const uint16_t src[] = {1,2,3,4, 100,200,300,400,
                         1000,2000,3000,4000, 65535,0,12345,32768};
  const int map[] = {0,1,1};
  uint16_t *dst;
  int x, y;
  scale_nearest = 1;
  dst = scale_px(src, 2, 2, 3, 3); assert(dst);
  for (y = 0; y < 3; y++) for (x = 0; x < 3; x++)
    assert(memcmp(dst + (y*3+x)*4, src + (map[y]*2+map[x])*4, 8) == 0);
  free(dst);
  dst = scale_px(src, 2, 2, 1, 1); assert(dst);
  assert(memcmp(dst, src+12, 8) == 0); free(dst);
  dst = scale_px(src, 2, 2, 2, 2); assert(dst);
  assert(memcmp(dst, src, sizeof src) == 0); free(dst);
  assert(!scale_px(src, 2, 2, 0, 1));
  assert(!scale_px(src, 2, 2, UINT32_MAX, UINT32_MAX));
  reset_document();
  snapshot_pixels(); layer_resize_one(0, 16, 8);
  for (y = 0; y < 8; y++) for (x = 0; x < 16; x++)
    assert(memcmp(layers[0]->px+(y*16+x)*4, original+((y/2)*8+x/2)*4, 8) == 0);
  do_undo(); assert_original(); do_redo(); assert(layers[0]->w == 16);
  reset_document();
  snapshot_pixels(); layer_resize_one(0, 40000, 1);
  assert(layers[0]->w == 40000 && layers[0]->h == 1);
  do_undo(); assert_original();
  snapshot_canvas(); img_resize(40000, 1);
  assert(canvas->w == 40000 && canvas->h == 1);
  assert(layers[0]->w == 1250 && layers[0]->h == 1);
  do_undo(); assert_original();
  reset_document(); snapshot_canvas(); img_resize(512, 512);
  assert(layers[0]->x == 20 && layers[0]->y == 24 && layers[0]->w == 16);
  assert(memcmp(layers[0]->px+4, original, 8) == 0);
  do_undo(); assert_original(); assert(canvas->w == 256);
  do_redo(); assert(layers[0]->w == 16 && canvas->w == 512);
  reset_document();
  assert(scale_start_drag(layers[0], 2, 18, 16));
  assert(scale_apply(layers[0], 22, 18));
  dst = scale_px(original, 8, 4, 12, 6); assert(dst);
  assert(memcmp(layers[0]->px, dst, 12*6*8) == 0); free(dst);
  /* Switching sampling at a stationary handle must refresh from the source. */
  scale_nearest = 0;
  assert(scale_apply(layers[0], 22, 18) && undo_count == 1);
  dst = scale_px_bilinear(original, 8, 4, 12, 6); assert(dst);
  assert(memcmp(layers[0]->px, dst, 12*6*8) == 0); free(dst);
  assert(!scale_apply(layers[0], 22, 18));
  scale_cancel(); assert_original();
  assert(scale_start_drag(layers[0], 2, 18, 16));
  scale_nearest = 1;
  assert(!scale_apply(layers[0], 18, 16) && undo_count == 0);
  scale_cancel(); scale_nearest = 0;
}

int
main(void)
{
  SDL_Surface *surface;
  size_t i;
  assert(scale_keep_aspect == 1 && scale_nearest == 0);
  assert(!WMD_SDL_INIT_FAILED(SDL_Init(0)));
  surface = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
  assert(surface); sdl_ren = SDL_CreateSoftwareRenderer(surface); assert(sdl_ren);
  canvas = ff_new(256, 256); assert(canvas);
  for (i = 0; i < sizeof original / sizeof *original; i++) original[i] = (uint16_t)(i*503);
  test_corners(); test_ratio_and_minimum(); test_input_and_hit_testing();
  test_history_after_layer_selection(); test_canvas_expansion(); test_nearest();
  history_clear(); layer_free(layers[0]); ff_free(canvas); canvas_image_free();
  SDL_DestroyRenderer(sdl_ren); SDL_FreeSurface(surface); SDL_Quit();
  puts("layer resizing: handles, aspect toggle, anchors, original raster, undo/redo/cancel ok");
  return 0;
}
