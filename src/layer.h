/*
 * Layer — a pixel buffer with a canvas-space origin.
 * Designed to be composited over an FFImage.
 * Pixel layout: RGBA uint16_t, native endian (same as FFImage.px).
 */
#ifndef LAYER_H
#define LAYER_H

#include <stdint.h>
#include <stdlib.h>
#include "farbfeld.h"

typedef struct {
	uint16_t *px;      /* RGBA 16-bit, same layout as FFImage */
	int       x, y;    /* top-left position in canvas image coords */
	uint32_t  w, h;
	char      name[32];
	int       visible;
	float     opacity; /* 0.0–1.0; multiplied against each pixel's alpha */
	int       alpha_lock; /* preserve existing alpha while painting */
	int       blend_mode; /* LAYER_BLEND_* */
} Layer;

enum {
	LAYER_BLEND_NORMAL = 0,
	LAYER_BLEND_MULTIPLY,
	LAYER_BLEND_SCREEN,
	LAYER_BLEND_OVERLAY,
	LAYER_BLEND_COUNT
};

static Layer *
layer_new(uint32_t w, uint32_t h)
{
	Layer *l = malloc(sizeof *l);
	if (!l) return NULL;
	l->px = calloc((size_t)w * h * 4, sizeof(uint16_t));
	if (!l->px) { free(l); return NULL; }
	l->x = l->y = 0;
	l->w = w;
	l->h = h;
	l->name[0] = '\0';
	l->visible  = 1;
	l->opacity  = 1.0f;
	l->alpha_lock = 0;
	l->blend_mode = LAYER_BLEND_NORMAL;
	return l;
}

static void
layer_free(Layer *l)
{
	if (l) { free(l->px); free(l); }
}

/*
 * Alpha-composite layer onto dst, clipped to canvas rect [cx0,cx1) x [cy0,cy1).
 * Layer pixels outside dst bounds or the clip rect are silently skipped.
 */
#ifndef WMD_WORK_CHECKPOINT
#define WMD_WORK_CHECKPOINT() ((void)0)
#endif

static void
layer_flatten_rect(const Layer *l, FFImage *dst,
                   int cx0, int cy0, int cx1, int cy1)
{
	int x, y, x0, y0, x1, y1, dx, dy;

	if (l->opacity == 0.0f) return;

	if (cx0 < 0) cx0 = 0;
	if (cy0 < 0) cy0 = 0;
	if (cx1 > (int)dst->w) cx1 = (int)dst->w;
	if (cy1 > (int)dst->h) cy1 = (int)dst->h;
	if (cx0 >= cx1 || cy0 >= cy1) return;

	/* Convert the destination clip into layer-local bounds first.  The old
	 * loops walked every pixel in a large layer even when a small dirty
	 * rectangle was being recomposited. */
	x0 = cx0 - l->x;
	y0 = cy0 - l->y;
	x1 = cx1 - l->x;
	y1 = cy1 - l->y;
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > (int)l->w) x1 = (int)l->w;
	if (y1 > (int)l->h) y1 = (int)l->h;
	if (x0 >= x1 || y0 >= y1) return;

	/* Common paint layers: specialize once per layer, keeping extra branches
	 * out of the general translucent/non-Normal blend loop. */
	if (l->blend_mode == LAYER_BLEND_NORMAL && l->opacity == 1.0f) {
		for (y = y0; y < y1; y++) {
			if ((y & 63) == 0) WMD_WORK_CHECKPOINT();
			const uint16_t *s = l->px + ((size_t)y * l->w + x0) * 4;
			uint16_t *d = dst->px + ((size_t)(l->y+y) * dst->w + (size_t)(l->x+x0)) * 4;
			for (x = x0; x < x1; x++, s += 4, d += 4) {
				uint32_t sa = s[3], na = 65535u-sa;
				if (sa == 0) continue;
				if (sa == 65535u) {
					memcpy(d, s, 4 * sizeof(uint16_t));
					continue;
				}
				d[0] = (uint16_t)((sa*s[0] + na*d[0]) / 65535u);
				d[1] = (uint16_t)((sa*s[1] + na*d[1]) / 65535u);
				d[2] = (uint16_t)((sa*s[2] + na*d[2]) / 65535u);
				d[3] = (uint16_t)(sa + na*d[3] / 65535u);
			}
		}
		return;
	}

	for (y = y0; y < y1; y++) {
		if ((y & 63) == 0) WMD_WORK_CHECKPOINT();
		dy = l->y + y;
		for (x = x0; x < x1; x++) {
			size_t   si, di;
			uint32_t sa, na, sr, sg, sb, dr, dg, db, da;
			dx = l->x + x;
			si = ((size_t)y  * l->w   + x)  * 4;
			di = ((size_t)dy * dst->w + dx) * 4;
			sa = (uint32_t)((float)l->px[si+3] * l->opacity);
			if (sa > 65535u) sa = 65535u;
			na = 65535u - sa;
			sr = l->px[si+0]; sg = l->px[si+1]; sb = l->px[si+2];
			dr = dst->px[di+0]; dg = dst->px[di+1];
			db = dst->px[di+2]; da = dst->px[di+3];
			if (l->blend_mode == LAYER_BLEND_MULTIPLY) {
				sr = (uint32_t)((uint64_t)sr * dr / 65535u); sg = (uint32_t)((uint64_t)sg * dg / 65535u); sb = (uint32_t)((uint64_t)sb * db / 65535u);
			} else if (l->blend_mode == LAYER_BLEND_SCREEN) {
				sr = (uint32_t)(65535u - (uint64_t)(65535u - sr) * (65535u - dr) / 65535u);
				sg = (uint32_t)(65535u - (uint64_t)(65535u - sg) * (65535u - dg) / 65535u);
				sb = (uint32_t)(65535u - (uint64_t)(65535u - sb) * (65535u - db) / 65535u);
			} else if (l->blend_mode == LAYER_BLEND_OVERLAY) {
				sr = dr < 32768u ? (uint32_t)(2u * (uint64_t)sr * dr / 65535u) : (uint32_t)(65535u - 2u * (uint64_t)(65535u - sr) * (65535u - dr) / 65535u);
				sg = dg < 32768u ? (uint32_t)(2u * (uint64_t)sg * dg / 65535u) : (uint32_t)(65535u - 2u * (uint64_t)(65535u - sg) * (65535u - dg) / 65535u);
				sb = db < 32768u ? (uint32_t)(2u * (uint64_t)sb * db / 65535u) : (uint32_t)(65535u - 2u * (uint64_t)(65535u - sb) * (65535u - db) / 65535u);
			}
			dst->px[di+0] = (uint16_t)((sa * sr + na * dr) / 65535u);
			dst->px[di+1] = (uint16_t)((sa * sg + na * dg) / 65535u);
			dst->px[di+2] = (uint16_t)((sa * sb + na * db) / 65535u);
			dst->px[di+3] = (uint16_t)(sa + (na * da)      / 65535u);
		}
	}
}

static void
layer_flatten(const Layer *l, FFImage *dst)
{
	layer_flatten_rect(l, dst, 0, 0, (int)dst->w, (int)dst->h);
}

#endif /* LAYER_H */
