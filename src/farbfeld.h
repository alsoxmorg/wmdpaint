/*
 * farbfeld image I/O
 * Format: "farbfeld" magic (8B), width BE32, height BE32,
 *         then w*h pixels each as 4 x uint16_t BE (RGBA).
 * All pixel values are stored native-endian in FFImage.px;
 * conversion happens only on load/save.
 */
#ifndef FARBFELD_H
#define FARBFELD_H

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	uint32_t w, h;
	uint16_t *px; /* RGBA, 4 channels per pixel, native endian */
} FFImage;

static FFImage *
ff_new(uint32_t w, uint32_t h)
{
	FFImage *img = malloc(sizeof *img);
	if (!img) return NULL;
	img->w = w;
	img->h = h;
	img->px = calloc((size_t)w * h * 4, sizeof(uint16_t));
	if (!img->px) { free(img); return NULL; }
	return img;
}

static void
ff_free(FFImage *img)
{
	if (img) { free(img->px); free(img); }
}

static FFImage *
ff_load_fp(FILE *f)
{
	FFImage *img;
	char magic[8];
	uint32_t w, h;
	size_t npx, i;

	if (fread(magic, 1, 8, f) != 8 || memcmp(magic, "farbfeld", 8) != 0)
		return NULL;
	if (fread(&w, 4, 1, f) != 1 || fread(&h, 4, 1, f) != 1)
		return NULL;

	w = ntohl(w); h = ntohl(h);
	if (!w || !h) return NULL;

	img = ff_new(w, h);
	if (!img) return NULL;

	npx = (size_t)w * h * 4;
	if (fread(img->px, sizeof(uint16_t), npx, f) != npx) {
		ff_free(img);
		return NULL;
	}

	for (i = 0; i < npx; i++)
		img->px[i] = ntohs(img->px[i]);

	return img;
}

static FFImage *
ff_load(const char *path)
{
	FILE *f;
	FFImage *img;
	f = fopen(path, "rb");
	if (!f) return NULL;
	img = ff_load_fp(f);
	fclose(f);
	return img;
}

static int
ff_save_fp(FILE *f, const FFImage *img)
{
	uint32_t w, h;
	size_t npx, i;
	uint16_t *buf;
	int ok;

	w = htonl(img->w);
	h = htonl(img->h);
	fwrite("farbfeld", 1, 8, f);
	fwrite(&w, 4, 1, f);
	fwrite(&h, 4, 1, f);

	npx = (size_t)img->w * img->h * 4;
	buf = malloc(npx * sizeof(uint16_t));
	if (!buf) return -1;

	for (i = 0; i < npx; i++)
		buf[i] = htons(img->px[i]);

	ok = (fwrite(buf, sizeof(uint16_t), npx, f) == npx) ? 0 : -1;
	free(buf);
	return ok;
}

static int
ff_save(const char *path, const FFImage *img)
{
	FILE *f;
	int ok;
	f = fopen(path, "wb");
	if (!f) return -1;
	ok = ff_save_fp(f, img);
	fclose(f);
	return ok;
}

#endif /* FARBFELD_H */
