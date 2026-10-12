/* CPU canvas rendering shared by the editor and its headless benchmark. */
#ifndef CANVAS_RENDER_H
#define CANVAS_RENDER_H

#include "layer.h"

/* Recompute only this canvas-space rectangle; preserve all other pixels. */
static void
canvas_flatten_rect(FFImage *dst, Layer *const *stack, int count,
                    int x0, int y0, int x1, int y1)
{
  int i, y;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > (int)dst->w) x1 = (int)dst->w;
  if (y1 > (int)dst->h) y1 = (int)dst->h;
  if (x0 >= x1 || y0 >= y1) return;
  for (y = y0; y < y1; y++) {
    if ((y & 63) == 0) WMD_WORK_CHECKPOINT();
    memset(dst->px + ((size_t)y * dst->w + x0) * 4, 0,
           (size_t)(x1 - x0) * 4 * sizeof(uint16_t));
  }
  for (i = 0; i < count; i++)
    if (stack[i] && stack[i]->visible)
      layer_flatten_rect(stack[i], dst, x0, y0, x1, y1);
}

/* Convert into a persistent RGBA8 buffer with full-canvas row stride.
 * Callers supply a clipped, nonempty rectangle. */
static void
canvas_rgba8_rect(const FFImage *src, unsigned char *buf,
                  int x0, int y0, int x1, int y1)
{
  int y;
  size_t channels = (size_t)(x1 - x0) * 4;
  for (y = y0; y < y1; y++) {
    if ((y & 63) == 0) WMD_WORK_CHECKPOINT();
    size_t start = ((size_t)y * src->w + x0) * 4;
    const uint16_t *in = src->px + start;
    unsigned char *out = buf + start;
    size_t c;
    for (c = 0; c < channels; c++)
      out[c] = (unsigned char)(in[c] >> 8);
  }
}

#endif
