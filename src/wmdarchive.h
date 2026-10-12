/* wmdarchive.h -- layered wmdpaint archive writer.
 *
 * A .wmd is an ordinary POSIX ustar archive.  A .wmd.bz2 is that same byte
 * stream compressed by the external bzip2 program.  Pixel entries remain
 * standard farbfeld or 1fpirate files; this container does not alter either
 * image format.
 */
#ifndef WMDARCHIVE_H
#define WMDARCHIVE_H

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "1fpirate.h"

typedef struct {
	FFImage *canvas;
	Layer   *layers[MAX_LAYERS];
	int      nlayers;
	int      active_layer;
} WMDDocument;

static void
wmd_document_free(WMDDocument *doc)
{
	int i;
	if (!doc) return;
	ff_free(doc->canvas);
	doc->canvas = NULL;
	for (i = 0; i < MAX_LAYERS; i++) {
		layer_free(doc->layers[i]);
		doc->layers[i] = NULL;
	}
	doc->nlayers = 0;
	doc->active_layer = 0;
}

static int
wmd_write_all(FILE *f, const void *buf, size_t len)
{
	return len == 0 || fwrite(buf, 1, len, f) == len;
}

static int
wmd_octal(unsigned char *dst, size_t width, unsigned long value)
{
	size_t i;
	if (width < 2) return 0;
	memset(dst, '0', width);
	dst[width - 1] = '\0';
	for (i = width - 1; i > 0 && value; ) {
		dst[--i] = (unsigned char)('0' + value % 8);
		value /= 8;
	}
	return value == 0;
}

static int
wmd_tar_header(FILE *f, const char *name, size_t size)
{
	unsigned char h[512];
	unsigned int sum = 0;
	size_t i, n = strlen(name);
	if (n == 0 || n >= 100) return 0;
	memset(h, 0, sizeof h);
	memcpy(h, name, n);
	if (!wmd_octal(h + 100, 8, 0644) || !wmd_octal(h + 108, 8, 0) ||
	    !wmd_octal(h + 116, 8, 0) ||
	    !wmd_octal(h + 124, 12, (unsigned long)size) ||
	    !wmd_octal(h + 136, 12, (unsigned long)time(NULL))) return 0;
	memset(h + 148, ' ', 8);
	h[156] = '0';
	memcpy(h + 257, "ustar", 5);
	memcpy(h + 263, "00", 2);
	memcpy(h + 265, "wmdpaint", 8);
	memcpy(h + 297, "wmdpaint", 8);
	for (i = 0; i < sizeof h; i++) sum += h[i];
	snprintf((char *)h + 148, 7, "%06o", sum);
	h[154] = '\0'; h[155] = ' ';
	return wmd_write_all(f, h, sizeof h);
}

static int
wmd_tar_pad(FILE *f, size_t size)
{
	static const unsigned char zero[512] = {0};
	size_t pad = (512 - size % 512) % 512;
	return wmd_write_all(f, zero, pad);
}

static int
wmd_layer_is_1fp(const Layer *l)
{
	size_t i, n = (size_t)l->w * l->h;
	for (i = 0; i < n; i++) {
		const uint16_t *p = l->px + i * 4;
		if (p[3] != 0xffff || p[0] != p[1] || p[1] != p[2] ||
		    (p[0] != 0 && p[0] != 0xffff)) return 0;
	}
	return 1;
}

static int
wmd_tar_ff(FILE *f, const char *name, uint32_t w, uint32_t h,
	   const uint16_t *px)
{
	unsigned char head[16], out[8192];
	uint32_t be;
	size_t i, n = (size_t)w * h * 4, at = 0;
	if (n > (SIZE_MAX - 16) / 2 || !wmd_tar_header(f, name, 16 + n * 2))
		return 0;
	memcpy(head, "farbfeld", 8);
	be = htonl(w); memcpy(head + 8, &be, 4);
	be = htonl(h); memcpy(head + 12, &be, 4);
	if (!wmd_write_all(f, head, sizeof head)) return 0;
	for (i = 0; i < n; i++) {
		out[at++] = (unsigned char)(px[i] >> 8);
		out[at++] = (unsigned char)px[i];
		if (at == sizeof out) {
			if (!wmd_write_all(f, out, at)) return 0;
			at = 0;
		}
	}
	return wmd_write_all(f, out, at) && wmd_tar_pad(f, 16 + n * 2);
}

static int
wmd_tar_1fp(FILE *f, const char *name, const Layer *l)
{
	unsigned char head[16], row[8192];
	uint32_t be, y, x, rb = (l->w + 7) / 8;
	size_t size = 16 + (size_t)rb * l->h;
	if (rb > sizeof row || !wmd_tar_header(f, name, size)) return 0;
	memcpy(head, "1fpirate", 8);
	be = htonl(l->w); memcpy(head + 8, &be, 4);
	be = htonl(l->h); memcpy(head + 12, &be, 4);
	if (!wmd_write_all(f, head, sizeof head)) return 0;
	for (y = 0; y < l->h; y++) {
		memset(row, 0, rb);
		for (x = 0; x < l->w; x++)
			if (l->px[((size_t)y * l->w + x) * 4] == 0xffff)
				row[x / 8] |= (unsigned char)(1u << (7 - x % 8));
		if (!wmd_write_all(f, row, rb)) return 0;
	}
	return wmd_tar_pad(f, size);
}

static void
wmd_safe_name(char *dst, size_t cap, const char *src)
{
	size_t i = 0;
	while (*src && i + 1 < cap) {
		unsigned char c = (unsigned char)*src++;
		dst[i++] = (c < 32 || c == 127) ? ' ' : (char)c;
	}
	dst[i] = '\0';
}

static int
wmd_write_tar(const char *path, const FFImage *canvas, Layer **layers,
	      int nlayers, int active_layer)
{
	FILE *f;
	char manifest[16384], safe[128], entry[100];
	size_t used = 0;
	int i, ok = 0;
	static const unsigned char end[1024] = {0};
	f = fopen(path, "wb");
	if (!f) return 0;
	used += (size_t)snprintf(manifest + used, sizeof manifest - used,
		"wmdpaint 1\nwidth %u\nheight %u\nactive %d\nlayers %d\n\n",
		canvas->w, canvas->h, active_layer, nlayers);
	for (i = 0; i < nlayers; i++) {
		int mono = wmd_layer_is_1fp(layers[i]);
		wmd_safe_name(safe, sizeof safe, layers[i]->name);
		snprintf(entry, sizeof entry, "layers/%03d.%s", i,
		         mono ? "1fpirate" : "ff");
		if (used >= sizeof manifest) goto done;
		used += (size_t)snprintf(manifest + used, sizeof manifest - used,
			"layer %s\nname %s\nx %d\ny %d\nvisible %d\nopacity %.9g\nalpha-lock %d\nblend %d\n\n",
			entry, safe, layers[i]->x, layers[i]->y,
			layers[i]->visible, (double)layers[i]->opacity,
			layers[i]->alpha_lock, layers[i]->blend_mode);
	}
	if (used >= sizeof manifest || !wmd_tar_header(f, "manifest", used) ||
	    !wmd_write_all(f, manifest, used) || !wmd_tar_pad(f, used)) goto done;
	for (i = 0; i < nlayers; i++) {
		int mono = wmd_layer_is_1fp(layers[i]);
		snprintf(entry, sizeof entry, "layers/%03d.%s", i,
		         mono ? "1fpirate" : "ff");
		if (mono) {
			if (!wmd_tar_1fp(f, entry, layers[i])) goto done;
		} else if (!wmd_tar_ff(f, entry, layers[i]->w, layers[i]->h,
		                       layers[i]->px)) goto done;
	}
	if (!wmd_tar_ff(f, "merged.ff", canvas->w, canvas->h, canvas->px) ||
	    !wmd_write_all(f, end, sizeof end)) goto done;
	ok = 1;
done:
	if (fclose(f) != 0) ok = 0;
	if (!ok) unlink(path);
	return ok;
}

static int
wmd_find_bzip2(char *out, size_t cap)
{
	const char *path = getenv("PATH"), *p;
	char dir[4096];
	if (!path) return 0;
	while (*path) {
		p = strchr(path, ':');
		if (!p) p = path + strlen(path);
		if ((size_t)(p - path) < sizeof dir) {
			size_t n = (size_t)(p - path);
			if (n) memcpy(dir, path, n); else { dir[0] = '.'; n = 1; }
			dir[n] = '\0';
			if (snprintf(out, cap, "%s/bzip2", dir) < (int)cap &&
			    access(out, X_OK) == 0) return 1;
		}
		path = *p ? p + 1 : p;
	}
	return 0;
}

static int
wmd_suffix(const char *path, const char *suffix)
{
	size_t n, m;
	if (!path || !suffix) return 0;
	n = strlen(path); m = strlen(suffix);
	return n >= m && strcmp(path + n - m, suffix) == 0;
}

static unsigned long
wmd_tar_size(const unsigned char *h)
{
	unsigned long value = 0;
	int i;
	for (i = 124; i < 136 && h[i] == ' '; i++) {}
	for (; i < 136 && h[i] >= '0' && h[i] <= '7'; i++)
		value = value * 8 + (unsigned long)(h[i] - '0');
	return value;
}

/* Read one regular file from a ustar stream. Returns 1 for an entry, 0 at
 * the end marker, and -1 for a malformed/truncated archive. */
static int
wmd_tar_entry(FILE *f, char *name, size_t name_cap,
	              unsigned char **data, size_t *data_len)
{
	unsigned char h[512];
	unsigned long size;
	size_t pad;
	int all_zero = 1;

	if (fread(h, 1, sizeof h, f) != sizeof h) return -1;
	for (size_t i = 0; i < sizeof h; i++)
		if (h[i] != 0) { all_zero = 0; break; }
	if (all_zero) return 0;
	if (h[156] != '0' && h[156] != '\0') return -1;
	if (strlen((const char *)h) >= name_cap) return -1;
	snprintf(name, name_cap, "%s", (const char *)h);
	size = wmd_tar_size(h);
	if (size > SIZE_MAX || size > (unsigned long)SIZE_MAX - 1) return -1;
	*data_len = (size_t)size;
	*data = NULL;
	if (*data_len) {
		*data = malloc(*data_len);
		if (!*data || fread(*data, 1, *data_len, f) != *data_len) {
			free(*data); *data = NULL; return -1;
		}
	}
	pad = (512 - (*data_len % 512)) % 512;
	if (pad && fseek(f, (long)pad, SEEK_CUR) != 0) {
		free(*data); *data = NULL; return -1;
	}
	return 1;
}

static FFImage *
wmd_image_bytes(const unsigned char *data, size_t len)
{
	FILE *f;
	FFImage *img;
	if (!data || !len) return NULL;
	f = tmpfile();
	if (!f || fwrite(data, 1, len, f) != len) {
		if (f) fclose(f);
		return NULL;
	}
	rewind(f);
	img = ff_load_fp(f);
	if (!img) { rewind(f); img = fp_load_fp(f); }
	fclose(f);
	return img;
}

typedef struct {
	char entry[100];
	char name[32];
	int x, y, visible;
	float opacity;
	int alpha_lock, blend_mode;
} WMDLayerMeta;

static int
wmd_parse_manifest(const unsigned char *data, size_t len,
	                   WMDDocument *doc, WMDLayerMeta *meta)
{
	char *copy, *line, *saveptr = NULL;
	int current = -1, i;
	if (!data || !len || len > 1024 * 1024) return 0;
	copy = malloc(len + 1);
	if (!copy) return 0;
	memcpy(copy, data, len); copy[len] = '\0';
	for (i = 0; i < MAX_LAYERS; i++) {
		meta[i].entry[0] = meta[i].name[0] = '\0';
		meta[i].x = meta[i].y = 0; meta[i].visible = 1; meta[i].opacity = 1.0f;
		meta[i].alpha_lock = 0; meta[i].blend_mode = LAYER_BLEND_NORMAL;
	}
	doc->nlayers = 0; doc->active_layer = 0;
	for (line = strtok_r(copy, "\n", &saveptr); line;
	     line = strtok_r(NULL, "\n", &saveptr)) {
		if (strncmp(line, "width ", 6) == 0) doc->canvas->w = (uint32_t)strtoul(line + 6, NULL, 10);
		else if (strncmp(line, "height ", 7) == 0) doc->canvas->h = (uint32_t)strtoul(line + 7, NULL, 10);
		else if (strncmp(line, "active ", 7) == 0) doc->active_layer = atoi(line + 7);
		else if (strncmp(line, "layers ", 7) == 0) doc->nlayers = atoi(line + 7);
		else if (strncmp(line, "layer ", 6) == 0 && doc->nlayers <= MAX_LAYERS) {
			current++;
			if (current >= MAX_LAYERS) { free(copy); return 0; }
			snprintf(meta[current].entry, sizeof meta[current].entry, "%s", line + 6);
			meta[current].visible = 1; meta[current].opacity = 1.0f;
		}
		else if (current >= 0 && strncmp(line, "name ", 5) == 0)
			snprintf(meta[current].name, sizeof meta[current].name, "%s", line + 5);
		else if (current >= 0 && strncmp(line, "x ", 2) == 0) meta[current].x = atoi(line + 2);
		else if (current >= 0 && strncmp(line, "y ", 2) == 0) meta[current].y = atoi(line + 2);
		else if (current >= 0 && strncmp(line, "visible ", 8) == 0) meta[current].visible = atoi(line + 8) != 0;
		else if (current >= 0 && strncmp(line, "opacity ", 8) == 0) meta[current].opacity = (float)strtod(line + 8, NULL);
		else if (current >= 0 && strncmp(line, "alpha-lock ", 11) == 0) meta[current].alpha_lock = atoi(line + 11) != 0;
		else if (current >= 0 && strncmp(line, "blend ", 6) == 0) meta[current].blend_mode = atoi(line + 6);
	}
	free(copy);
	if (doc->canvas->w == 0 || doc->canvas->h == 0 ||
	    doc->nlayers < 1 || doc->nlayers > MAX_LAYERS || current + 1 != doc->nlayers)
		return 0;
	if (doc->active_layer < 0 || doc->active_layer >= doc->nlayers)
		doc->active_layer = doc->nlayers - 1;
	return 1;
}

static FILE *
wmd_open_tar_stream(const char *path, FILE **owned_tmp)
{
	FILE *f;
	*owned_tmp = NULL;
	if (!wmd_suffix(path, ".bz2")) return fopen(path, "rb");
	{
		char bz[4096];
		int in, outfd;
		pid_t pid;
		int status;
		if (!wmd_find_bzip2(bz, sizeof bz)) return NULL;
		f = tmpfile();
		if (!f) return NULL;
		in = open(path, O_RDONLY);
		if (in < 0) { fclose(f); return NULL; }
		outfd = fileno(f);
		pid = fork();
		if (pid == 0) {
			if (dup2(in, STDIN_FILENO) < 0 || dup2(outfd, STDOUT_FILENO) < 0) _exit(126);
			close(in);
			execl(bz, "bzip2", "-dc", (char *)NULL);
			_exit(127);
		}
		close(in);
		if (pid < 0 || waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			fclose(f); return NULL;
		}
		fflush(f); rewind(f); *owned_tmp = f; return f;
	}
}

/* Load the writer's .wmd or .wmd.bz2 container. */
static int
wmd_archive_load(const char *path, WMDDocument *doc)
{
	FILE *f, *tmp = NULL;
	char name[100];
	unsigned char *data = NULL, *manifest = NULL;
	size_t len = 0, manifest_len = 0;
	WMDLayerMeta meta[MAX_LAYERS];
	unsigned char *raw[MAX_LAYERS] = {0};
	size_t raw_len[MAX_LAYERS] = {0};
	char raw_name[MAX_LAYERS][100];
	int raw_count = 0, i, rc = 0;

	if (!wmd_suffix(path, ".wmd") && !wmd_suffix(path, ".wmd.bz2")) return 0;
	memset(doc, 0, sizeof *doc);
	f = wmd_open_tar_stream(path, &tmp);
	if (!f) return 0;
	while ((i = wmd_tar_entry(f, name, sizeof name, &data, &len)) > 0) {
		if (strcmp(name, "manifest") == 0) {
			free(manifest); manifest = data; manifest_len = len; data = NULL;
		} else if (strncmp(name, "layers/", 7) == 0) {
			if (raw_count >= MAX_LAYERS) { free(data); data = NULL; goto done; }
			snprintf(raw_name[raw_count], sizeof raw_name[raw_count], "%s", name);
			raw[raw_count] = data; raw_len[raw_count] = len;
			raw_count++; data = NULL;
		}
		free(data); data = NULL;
	}
	if (i < 0 || !manifest) goto done;
	doc->canvas = ff_new(1, 1);
	if (!doc->canvas || !wmd_parse_manifest(manifest, manifest_len, doc, meta)) goto done;
	free(doc->canvas->px); doc->canvas->px = calloc((size_t)doc->canvas->w * doc->canvas->h * 4, sizeof(uint16_t));
	if (!doc->canvas->px) goto done;
	for (i = 0; i < doc->nlayers; i++) {
		FFImage *img;
		int k;
		for (k = 0; k < raw_count; k++)
			if (strcmp(raw_name[k], meta[i].entry) == 0) break;
		if (k == raw_count) goto done;
		img = wmd_image_bytes(raw[k], raw_len[k]);
		if (!img) goto done;
		doc->layers[i] = layer_new(img->w, img->h);
		if (!doc->layers[i]) { ff_free(img); goto done; }
		memcpy(doc->layers[i]->px, img->px, (size_t)img->w * img->h * 4 * sizeof(uint16_t));
		doc->layers[i]->x = meta[i].x; doc->layers[i]->y = meta[i].y;
		doc->layers[i]->visible = meta[i].visible;
		doc->layers[i]->opacity = meta[i].opacity;
		doc->layers[i]->alpha_lock = meta[i].alpha_lock;
		doc->layers[i]->blend_mode = meta[i].blend_mode >= 0 && meta[i].blend_mode < LAYER_BLEND_COUNT
			? meta[i].blend_mode : LAYER_BLEND_NORMAL;
		snprintf(doc->layers[i]->name, sizeof doc->layers[i]->name, "%s", meta[i].name[0] ? meta[i].name : "Layer");
		ff_free(img);
	}
	rc = 1;
done:
	free(data); free(manifest);
	for (i = 0; i < MAX_LAYERS; i++) free(raw[i]);
	if (tmp) fclose(tmp); else if (f) fclose(f);
	if (!rc) wmd_document_free(doc);
	return rc;
}

/* Return 1 for compressed success, 2 for uncompressed fallback, 0 on error. */
static int
wmd_archive_save(const char *requested, const FFImage *canvas, Layer **layers,
	         int nlayers, int active_layer, char *actual, size_t actual_cap)
{
	char final[4096], tarpath[4096], tmptar[4096], tmpbz[4096], bz[4096];
	int have_bz, fd, outfd, status;
	pid_t pid;
	size_t len;
	if (!requested || !*requested || !canvas || nlayers < 1) return 0;
	snprintf(final, sizeof final, "%s", requested);
	len = strlen(final);
	if (len < 8 || strcmp(final + len - 8, ".wmd.bz2") != 0) {
		if (len >= 4 && strcmp(final + len - 4, ".wmd") == 0)
			snprintf(final + len, sizeof final - len, ".bz2");
		else
			snprintf(final + len, sizeof final - len, ".wmd.bz2");
	}
	have_bz = wmd_find_bzip2(bz, sizeof bz);
	if (have_bz) snprintf(tarpath, sizeof tarpath, "%.*s", (int)(strlen(final) - 4), final);
	else         snprintf(tarpath, sizeof tarpath, "%.*s", (int)(strlen(final) - 4), final);
	if (snprintf(tmptar, sizeof tmptar, "%s.tmp.XXXXXX", tarpath) >= (int)sizeof tmptar)
		return 0;
	fd = mkstemp(tmptar);
	if (fd < 0) return 0;
	close(fd);
	if (!wmd_write_tar(tmptar, canvas, layers, nlayers, active_layer)) return 0;
	if (!have_bz) {
		if (rename(tmptar, tarpath) != 0) { unlink(tmptar); return 0; }
		snprintf(actual, actual_cap, "%s", tarpath);
		return 2;
	}
	if (snprintf(tmpbz, sizeof tmpbz, "%s.tmp.XXXXXX", final) >= (int)sizeof tmpbz) {
		unlink(tmptar); return 0;
	}
	outfd = mkstemp(tmpbz);
	if (outfd < 0) { unlink(tmptar); return 0; }
	pid = fork();
	if (pid == 0) {
		int infd = open(tmptar, O_RDONLY);
		if (infd < 0 || dup2(infd, STDIN_FILENO) < 0 ||
		    dup2(outfd, STDOUT_FILENO) < 0) _exit(126);
		close(infd); close(outfd);
		execl(bz, "bzip2", "-c", (char *)NULL);
		_exit(127);
	}
	close(outfd);
	if (pid < 0 || waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0) {
		unlink(tmpbz);
		if (rename(tmptar, tarpath) != 0) { unlink(tmptar); return 0; }
		snprintf(actual, actual_cap, "%s", tarpath);
		return 2;
	}
	if (rename(tmpbz, final) != 0) {
		unlink(tmpbz); unlink(tmptar); return 0;
	}
	unlink(tmptar);
	snprintf(actual, actual_cap, "%s", final);
	return 1;
}

#endif /* WMDARCHIVE_H */
