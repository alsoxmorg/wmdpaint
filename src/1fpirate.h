/*
 * 1fpirate — 1-bit farbfeld image I/O
 * Format: "1fpirate" magic (8B), width BE32, height BE32,
 *         then packed 1-bit rows, MSB first, each row padded to byte boundary.
 * bit=1 → white (0xffff), bit=0 → black (0x0000).
 * Color images are reduced to 1-bit by luminance threshold on save.
 */
#ifndef ONEFIRATE_H
#define ONEFIRATE_H

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "farbfeld.h"

#define FP_MAGIC "1fpirate"

/* Returns 1 if path ends with ".1fpirate" (case-sensitive). */
static int
fp_is_1fpirate_path(const char *path)
{
	size_t n = strlen(path);
	const char *ext = ".1fpirate";
	size_t elen = strlen(ext);
	return n >= elen && memcmp(path + n - elen, ext, elen) == 0;
}

/* Convert native-endian uint16_t RGB to 1-bit using BT.601 luma threshold. */
static int
fp_pixel_is_white(uint16_t r, uint16_t g, uint16_t b)
{
	/* weights: 0.299, 0.587, 0.114 scaled to sum ~65535 */
	uint32_t luma = (uint32_t)r * 19595u
	              + (uint32_t)g * 38470u
	              + (uint32_t)b *  7471u;
	/* luma >> 16 gives 0–65535; threshold at midpoint */
	return (luma >> 16) >= 0x8000u;
}

static FFImage *
fp_load_fp(FILE *f)
{
	char magic[8];
	uint32_t w, h;
	size_t row_bytes;
	uint8_t *bitmap;
	FFImage *img;
	uint32_t x, y;

	if (fread(magic, 1, 8, f) != 8) return NULL;
	if (memcmp(magic, FP_MAGIC, 8) != 0)
		return NULL;

	if (fread(&w, 4, 1, f) != 1 || fread(&h, 4, 1, f) != 1) return NULL;
	w = ntohl(w);
	h = ntohl(h);
	if (!ff_dimensions_valid(w, h)) return NULL;

	row_bytes = ((size_t)w + 7) / 8;
	bitmap = malloc((size_t)row_bytes * h);
	if (!bitmap) return NULL;
	if (fread(bitmap, 1, (size_t)row_bytes * h, f) != (size_t)row_bytes * h) {
		free(bitmap);
		return NULL;
	}

	img = ff_new(w, h);
	if (!img) { free(bitmap); return NULL; }

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			size_t bidx = (size_t)y * row_bytes + x / 8;
			int boff = 7 - (int)(x % 8);
			uint16_t val = ((bitmap[bidx] >> boff) & 1) ? 0xffff : 0x0000;
			uint16_t *p = img->px + ((size_t)y * w + x) * 4;
			p[0] = val; p[1] = val; p[2] = val; p[3] = 0xffff;
		}
	}

	free(bitmap);
	return img;
}

static FFImage *
fp_load(const char *path)
{
	FILE *f = fopen(path, "rb");
	FFImage *img;
	if (!f) return NULL;
	img = fp_load_fp(f);
	fclose(f);
	return img;
}

static FFImage *
fp_load_mem(const unsigned char *buf, size_t len)
{
	char     magic[8];
	uint32_t w, h, x, y;
	size_t   row_bytes;
	uint8_t *bitmap;
	FFImage *img;
	size_t   pos = 0;

	if (len < 16) return NULL;
	memcpy(magic, buf + pos, 8); pos += 8;
	if (memcmp(magic, FP_MAGIC, 8) != 0) return NULL;
	memcpy(&w, buf + pos, 4); pos += 4;
	memcpy(&h, buf + pos, 4); pos += 4;
	w = ntohl(w); h = ntohl(h);
	if (!ff_dimensions_valid(w, h)) return NULL;
	row_bytes = ((size_t)w + 7) / 8;
	if ((size_t)row_bytes * h > len - pos) return NULL;
	bitmap = malloc((size_t)row_bytes * h);
	if (!bitmap) return NULL;
	memcpy(bitmap, buf + pos, (size_t)row_bytes * h);
	img = ff_new(w, h);
	if (!img) { free(bitmap); return NULL; }
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			size_t bidx = (size_t)y * row_bytes + x / 8;
			int boff = 7 - (int)(x % 8);
			uint16_t val = ((bitmap[bidx] >> boff) & 1) ? 0xffff : 0x0000;
			uint16_t *p  = img->px + ((size_t)y * w + x) * 4;
			p[0] = val; p[1] = val; p[2] = val; p[3] = 0xffff;
		}
	}
	free(bitmap);
	return img;
}

#if COWARD_MODE
/* Returns 1 if every pixel's RGB channels are already 0x0000 or 0xffff. */
static int
fp_image_is_1bit(const FFImage *img)
{
	size_t i, npx = (size_t)img->w * img->h;
	for (i = 0; i < npx; i++) {
		uint16_t r = img->px[i*4+0];
		uint16_t g = img->px[i*4+1];
		uint16_t b = img->px[i*4+2];
		if ((r != 0 && r != 0xffff) ||
		    (g != 0 && g != 0xffff) ||
		    (b != 0 && b != 0xffff))
			return 0;
	}
	return 1;
}
#endif /* COWARD_MODE */

static int
fp_save(const char *path, const FFImage *img)
{
	FILE *f;
	uint32_t w, h, x, y;
	size_t row_bytes;
	uint8_t *bitmap;
	uint32_t bw, bh;
	int ok;

	if (!img || !img->px || !ff_dimensions_valid(img->w, img->h)) return -1;
	f = fopen(path, "wb");
	if (!f) return -1;

	w = img->w;
	h = img->h;
	row_bytes = ((size_t)w + 7) / 8;

	bitmap = calloc((size_t)row_bytes * h, 1);
	if (!bitmap) { fclose(f); return -1; }

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			const uint16_t *p = img->px + ((size_t)y * w + x) * 4;
			if (fp_pixel_is_white(p[0], p[1], p[2])) {
				size_t bidx = (size_t)y * row_bytes + x / 8;
				int boff = 7 - (int)(x % 8);
				bitmap[bidx] |= (uint8_t)(1 << boff);
			}
		}
	}

	ok = fwrite(FP_MAGIC, 1, 8, f) == 8;
	bw = htonl(w);
	bh = htonl(h);
	ok = ok && fwrite(&bw, 4, 1, f) == 1 &&
	     fwrite(&bh, 4, 1, f) == 1 &&
	     fwrite(bitmap, 1, row_bytes * h, f) == row_bytes * h;
	free(bitmap);
	if (fclose(f) != 0) ok = 0;
	return ok ? 0 : -1;
}

/* fp_save_dithered — Floyd-Steinberg dither to 1-bit then write 1fpirate.
   Uses the same BT.601 luma weights as fp_pixel_is_white. */
static int
fp_save_dithered(const char *path, const FFImage *img)
{
	FILE     *f;
	uint32_t  w, h, x, y, bw, bh;
	int       ok;
	size_t    row_bytes;
	uint8_t  *bitmap;
	int32_t  *err;

	if (!img || !img->px || !ff_dimensions_valid(img->w, img->h)) return -1;
	f = fopen(path, "wb");
	if (!f) return -1;

	w = img->w;
	h = img->h;
	row_bytes = ((size_t)w + 7) / 8;

	bitmap = calloc((size_t)row_bytes * h, 1);
	err    = calloc((size_t)w * h, sizeof(int32_t));
	if (!bitmap || !err) { free(bitmap); free(err); fclose(f); return -1; }

	/* seed error buffer with luma (0–65535) */
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			const uint16_t *p = img->px + ((size_t)y * w + x) * 4;
			uint32_t luma = ((uint32_t)p[0] * 19595u
			               + (uint32_t)p[1] * 38470u
			               + (uint32_t)p[2] *  7471u) >> 16;
			err[(size_t)y * w + x] = (int32_t)luma;
		}
	}

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			int32_t v = err[(size_t)y * w + x];
			int     white;
			int32_t e;
			if (v < 0)      v = 0;
			if (v > 0xffff) v = 0xffff;
			white = (v >= 0x8000);
			e     = v - (white ? (int32_t)0xffff : 0);
			if (white) {
				size_t bidx = (size_t)y * row_bytes + x / 8;
				int boff = 7 - (int)(x % 8);
				bitmap[bidx] |= (uint8_t)(1 << boff);
			}
			if (x + 1 < w)
				err[(size_t)y * w + (x + 1)]         += e * 7 / 16;
			if (y + 1 < h) {
				if (x > 0)
					err[((size_t)y+1) * w + (x - 1)] += e * 3 / 16;
				err[((size_t)y+1) * w + x]           += e * 5 / 16;
				if (x + 1 < w)
					err[((size_t)y+1) * w + (x + 1)] += e * 1 / 16;
			}
		}
	}

	ok = fwrite(FP_MAGIC, 1, 8, f) == 8;
	bw = htonl(w); bh = htonl(h);
	ok = ok && fwrite(&bw, 4, 1, f) == 1 &&
	     fwrite(&bh, 4, 1, f) == 1 &&
	     fwrite(bitmap, 1, row_bytes * h, f) == row_bytes * h;
	free(bitmap);
	free(err);
	if (fclose(f) != 0) ok = 0;
	return ok ? 0 : -1;
}

#endif /* ONEFIRATE_H */
