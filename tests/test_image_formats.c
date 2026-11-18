#define _POSIX_C_SOURCE 200809L
#include "../src/1fpirate.h"
#include <assert.h>
#include <unistd.h>

static FILE *bytes(const unsigned char *data, size_t n)
{
  FILE *f = tmpfile(); assert(f);
  assert(fwrite(data, 1, n, f) == n); rewind(f); return f;
}

int main(void)
{
  const unsigned char golden[] = {
    'f','a','r','b','f','e','l','d',0,0,0,1,0,0,0,1,
    0x12,0x34,0xab,0xcd,0,1,0x80,0
  };
  unsigned char mono[] = {
    '1','f','p','i','r','a','t','e',0,0,0,9,0,0,0,2,0xaa,0x80,0x55,0
  };
  unsigned char bad[sizeof golden], out[sizeof golden];
  char path[] = "/tmp/wmdpaint-format-XXXXXX";
  size_t n; int fd = mkstemp(path); FILE *f; FFImage *a, *b;
  assert(fd >= 0); close(fd);
  f = bytes(golden, sizeof golden); a = ff_load_fp(f); fclose(f);
  assert(a && a->w == 1 && a->h == 1);
  assert(a->px[0] == 0x1234 && a->px[1] == 0xabcd &&
         a->px[2] == 1 && a->px[3] == 0x8000);
  assert(ff_save(path, a) == 0); b = ff_load(path); assert(b);
  assert(memcmp(a->px, b->px, 8) == 0); ff_free(b);
  f = fopen(path, "rb"); assert(f && fread(out, 1, sizeof out, f) == sizeof out);
  fclose(f); assert(memcmp(out, golden, sizeof out) == 0);
  for (n = 0; n < sizeof golden; n++) {
    f = bytes(golden, n); assert(!ff_load_fp(f)); fclose(f);
  }
  for (n = 0; n < sizeof mono; n++) {
    f = bytes(mono, n); assert(!fp_load_fp(f)); fclose(f);
    assert(!fp_load_mem(mono, n));
  }
  memcpy(bad, golden, sizeof bad); bad[0] = 'X';
  f = bytes(bad, sizeof bad); assert(!ff_load_fp(f)); fclose(f);
  for (n = 0; n < 2; n++) {
    memcpy(bad, golden, sizeof bad);
    memset(bad + 8, n ? 0xff : 0, 8);
    f = bytes(bad, sizeof bad); assert(!ff_load_fp(f)); fclose(f);
    memcpy(bad, mono, 8);
    f = bytes(bad, sizeof bad); assert(!fp_load_fp(f)); fclose(f);
    assert(!fp_load_mem(bad, sizeof bad));
  }
  assert(!ff_new(0, 1) && !ff_new(UINT32_MAX, UINT32_MAX));
  b = fp_load_mem(mono, sizeof mono); assert(b);
  for (n = 0; n < 18; n++) {
    uint16_t v = ((n % 9 + n / 9) % 2 == 0) ? 65535 : 0;
    assert(b->px[n*4] == v && b->px[n*4+1] == v &&
           b->px[n*4+2] == v && b->px[n*4+3] == 65535);
  }
  assert(fp_save(path, b) == 0);
  f = fopen(path, "rb"); assert(f && fread(out, 1, sizeof mono, f) == sizeof mono);
  fclose(f); assert(memcmp(out, mono, sizeof mono) == 0);
  assert(fp_save_dithered(path, b) == 0);
  f = fopen(path, "rb"); assert(f && fread(out, 1, sizeof mono, f) == sizeof mono);
  fclose(f); assert(memcmp(out, mono, sizeof mono) == 0);
  ff_free(b);
  /* Both encoders quantize colour and discard alpha. */
  a->px[0] = a->px[1] = a->px[2] = 65535; a->px[3] = 0;
  assert(fp_save(path, a) == 0); b = fp_load(path); assert(b);
  assert(b->px[0] == 65535 && b->px[3] == 65535); ff_free(b);
  if (access("/dev/full", W_OK) == 0) {
    assert(ff_save("/dev/full", a) == -1);
    assert(fp_save("/dev/full", a) == -1);
    assert(fp_save_dithered("/dev/full", a) == -1);
    f = fopen("/dev/full", "wb"); assert(f); setbuf(f, NULL);
    assert(ff_save_fp(f, a) == -1); fclose(f);
  }
  ff_free(a); unlink(path);
  puts("image formats: wire bytes, round trips, truncation, dimensions, write errors ok");
  return 0;
}
