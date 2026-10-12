#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../src/config_paint.h"
#include "../src/layer.h"
#include "../src/wmdarchive.h"

static void
fill_layer(Layer *l, uint16_t r, uint16_t g, uint16_t b)
{
  size_t i;
  for (i = 0; i < (size_t)l->w * l->h; i++) {
    l->px[i * 4 + 0] = r;
    l->px[i * 4 + 1] = g;
    l->px[i * 4 + 2] = b;
    l->px[i * 4 + 3] = 0xffff;
  }
}

static void
check_roundtrip(const char *path)
{
  FFImage *canvas = ff_new(4, 3);
  Layer *layers[2] = { layer_new(4, 3), layer_new(2, 2) };
  WMDDocument loaded;
  char actual[4096];
  int result;

  assert(canvas && layers[0] && layers[1]);
  fill_layer(layers[0], 0, 0, 0);
  fill_layer(layers[1], 0xffff, 0, 0);
  snprintf(layers[0]->name, sizeof layers[0]->name, "Background");
  snprintf(layers[1]->name, sizeof layers[1]->name, "Red square");
  layers[1]->x = 1; layers[1]->y = 1;
  layers[1]->opacity = 0.5f;
  layers[1]->alpha_lock = 1;
  layers[1]->blend_mode = LAYER_BLEND_SCREEN;
  layers[0]->visible = 1; layers[1]->visible = 0;

  result = wmd_archive_save(path, canvas, layers, 2, 1, actual, sizeof actual);
  assert(result == 1 || result == 2);
  assert(wmd_archive_load(actual, &loaded));
  assert(loaded.canvas->w == 4 && loaded.canvas->h == 3);
  assert(loaded.nlayers == 2 && loaded.active_layer == 1);
  assert(strcmp(loaded.layers[1]->name, "Red square") == 0);
  assert(loaded.layers[1]->x == 1 && loaded.layers[1]->y == 1);
  assert(loaded.layers[1]->visible == 0);
  assert(loaded.layers[1]->opacity > 0.49f && loaded.layers[1]->opacity < 0.51f);
  assert(loaded.layers[1]->alpha_lock == 1);
  assert(loaded.layers[1]->blend_mode == LAYER_BLEND_SCREEN);
  assert(loaded.layers[1]->px[0] == 0xffff);

  wmd_document_free(&loaded);
  layer_free(layers[0]); layer_free(layers[1]);
  ff_free(canvas);
  unlink(actual);
  if (result == 1 && strcmp(actual, path) != 0) unlink(path);
}

int
main(void)
{
  FILE *bad = fopen("/tmp/wmdpaint-invalid.wmd", "wb");
  assert(bad);
  fputs("not a tar archive\n", bad);
  fclose(bad);
  {
    WMDDocument doc;
    assert(!wmd_archive_load("/tmp/wmdpaint-invalid.wmd", &doc));
  }
  unlink("/tmp/wmdpaint-invalid.wmd");
  check_roundtrip("/tmp/wmdpaint-roundtrip.wmd");
  puts("wmdarchive round-trip: ok");
  return 0;
}
