#include <assert.h>
#include <stdio.h>
#include "canvas_render.h"

static uint32_t seed = 17;
static uint16_t
random16(void)
{
  seed = seed * 1664525u + 1013904223u;
  return (uint16_t)(seed >> 16);
}

/* Scalar reference: walk destination coordinates rather than optimized
 * layer-local clips, and always evaluate blending (including alpha 0/1). */
static void
reference_layer(const Layer *l, FFImage *dst, int x0, int y0, int x1, int y1)
{
  int x, y, c;
  for (y = 0; y < (int)dst->h; y++) for (x = 0; x < (int)dst->w; x++) {
    int sx = x-l->x, sy = y-l->y;
    const uint16_t *s;
    uint16_t *d;
    uint32_t a, inverse;
    if (x < x0 || x >= x1 || y < y0 || y >= y1 ||
        sx < 0 || sy < 0 || sx >= (int)l->w || sy >= (int)l->h) continue;
    s = l->px + ((size_t)sy*l->w+sx)*4;
    d = dst->px + ((size_t)y*dst->w+x)*4;
    a = (uint32_t)((float)s[3]*l->opacity);
    if (a > 65535) a = 65535;
    inverse = 65535-a;
    for (c = 0; c < 3; c++) {
      uint64_t colour = s[c], back = d[c];
      switch (l->blend_mode) {
      case LAYER_BLEND_MULTIPLY: colour = colour*back/65535; break;
      case LAYER_BLEND_SCREEN: colour = 65535-(65535-colour)*(65535-back)/65535; break;
      case LAYER_BLEND_OVERLAY:
        colour = back < 32768 ? 2*colour*back/65535 :
                 65535-2*(65535-colour)*(65535-back)/65535;
        break;
      }
      d[c] = (uint16_t)((a*colour+inverse*back)/65535);
    }
    d[3] = (uint16_t)(a+(uint64_t)inverse*d[3]/65535);
  }
}

static void
test_blending(void)
{
  const float opacity[] = {0, 0.1f, 0.5f, 0.999f, 1, 1.25f};
  Layer *l = layer_new(11, 9);
  FFImage *actual = ff_new(13, 12), *expected = ff_new(13, 12);
  int mode, op, pos, trial;
  size_t i;
  assert(l && actual && expected);
  for (mode = 0; mode < LAYER_BLEND_COUNT; mode++)
    for (op = 0; op < (int)(sizeof opacity/sizeof *opacity); op++)
      for (pos = 0; pos < 5; pos++) for (trial = 0; trial < 12; trial++) {
        int x0 = (int)(random16()%20)-4, y0 = (int)(random16()%18)-3;
        int x1 = x0+(int)(random16()%20), y1 = y0+(int)(random16()%18);
        l->x = pos*5-9; l->y = pos*3-5;
        l->opacity = opacity[op]; l->blend_mode = mode;
        for (i = 0; i < (size_t)l->w*l->h*4; i++) l->px[i] = random16();
        for (i = 0; i < (size_t)l->w*l->h; i++)
          if (i%3 != 2) l->px[i*4+3] = i%3 ? 65535 : 0;
        for (i = 0; i < (size_t)actual->w*actual->h*4; i++) actual->px[i] = random16();
        memcpy(expected->px, actual->px, (size_t)actual->w*actual->h*8);
        reference_layer(l, expected, x0, y0, x1, y1);
        layer_flatten_rect(l, actual, x0, y0, x1, y1);
        assert(memcmp(actual->px, expected->px, (size_t)actual->w*actual->h*8) == 0);
      }
  layer_free(l); ff_free(actual); ff_free(expected);
}

static void
test_dirty_pipeline(void)
{
  Layer *stack[4] = {layer_new(13, 12), layer_new(7, 9), NULL, layer_new(3, 3)};
  FFImage *full = ff_new(13, 12), *dirty = ff_new(13, 12);
  unsigned char full8[13*12*4], dirty8[13*12*4];
  size_t i;
  int n, mode;
  assert(stack[0] && stack[1] && stack[3] && full && dirty);
  stack[1]->x = -2; stack[1]->y = 4; stack[1]->opacity = 0.5f;
  stack[3]->visible = 0;
  for (n = 0; n < 4; n++) if (stack[n])
    for (i = 0; i < (size_t)stack[n]->w*stack[n]->h*4; i++) stack[n]->px[i] = random16();
  for (mode = 0; mode < LAYER_BLEND_COUNT; mode++) {
    stack[1]->blend_mode = mode;
    canvas_flatten_rect(full, stack, 4, 0, 0, 13, 12);
    memcpy(dirty->px, full->px, sizeof full8*2);
    canvas_rgba8_rect(full, full8, 0, 0, 13, 12);
    memcpy(dirty8, full8, sizeof full8);
    /* Change exactly the rectangle to be recomputed, including edge clipping. */
    for (n = 9; n < 12; n++) for (i = 10; i < 13; i++) {
      uint16_t *p = stack[0]->px+((size_t)n*13+i)*4;
      p[0] ^= 0xffff; p[3] = 65535;
    }
    canvas_flatten_rect(full, stack, 4, 0, 0, 13, 12);
    canvas_flatten_rect(dirty, stack, 4, 10, 9, 30, 20);
    assert(memcmp(full->px, dirty->px, sizeof full8*2) == 0);
    canvas_rgba8_rect(full, full8, 0, 0, 13, 12);
    canvas_rgba8_rect(dirty, dirty8, 10, 9, 13, 12);
    assert(memcmp(full8, dirty8, sizeof full8) == 0);
    for (i = 0; i < sizeof full8; i++) assert(full8[i] == (unsigned char)(full->px[i]>>8));
    canvas_flatten_rect(dirty, stack, 4, -8, -7, -1, -1);
    canvas_flatten_rect(dirty, stack, 4, 2, 3, 1, 0);
    assert(memcmp(full->px, dirty->px, sizeof full8*2) == 0);
  }
  for (n = 0; n < 4; n++) layer_free(stack[n]);
  ff_free(full); ff_free(dirty);
}

int
main(void)
{
  test_blending(); test_dirty_pipeline();
  puts("canvas rendering: scalar equivalence, clipping, dirty updates, RGBA8 ok");
  return 0;
}
