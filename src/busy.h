/* Cooperative activity display. All SDL drawing stays on the UI thread;
 * checkpoints never dispatch editor actions against a half-finished document. */
#ifndef WMD_BUSY_H
#define WMD_BUSY_H

static SDL_Texture *busy_frame;
static int busy_frame_w, busy_frame_h, busy_frame_ready, busy_active, busy_visible;
static uint64_t busy_started, busy_last_draw;

static void
busy_frame_begin(void)
{
  if (!busy_frame || busy_frame_w != win_w || busy_frame_h != win_h) {
    if (busy_frame) SDL_DestroyTexture(busy_frame);
    busy_frame = SDL_CreateTexture(sdl_ren, SDL_PIXELFORMAT_RGBA8888,
                                   SDL_TEXTUREACCESS_TARGET, win_w, win_h);
    busy_frame_w = win_w; busy_frame_h = win_h; busy_frame_ready = 0;
  }
  if (busy_frame) {
    SDL_SetRenderTarget(sdl_ren, busy_frame);
    if (SDL_GetRenderTarget(sdl_ren) != busy_frame) busy_frame_ready = 0;
  }
}

static void
busy_frame_present(void)
{
  if (busy_frame && SDL_GetRenderTarget(sdl_ren) == busy_frame) {
    SDL_SetRenderTarget(sdl_ren, NULL);
    SDL_SetRenderClipRect(sdl_ren, NULL);
    WMD_RenderCopy(sdl_ren, busy_frame, NULL, NULL);
    busy_frame_ready = 1;
  }
  SDL_RenderPresent(sdl_ren);
}

static void
busy_begin(void)
{
  busy_active = 1; busy_visible = 0;
  busy_started = SDL_GetTicks(); busy_last_draw = busy_started;
}

static void
busy_draw(uint64_t now)
{
  static const int dx[8] = {0, 3, 4, 3, 0, -3, -4, -3};
  static const int dy[8] = {-4, -3, 0, 3, 4, 3, 0, -3};
  const char *label = "Working";
  SDL_Texture *target;
  SDL_Rect clip, bar = {win_w - 92, win_h - STATUS_H, 92, STATUS_H};
  unsigned char r, g, b, a;
  int clipped, i, gx, gy, phase = (int)((now / 100) % 8);
  if (!busy_active || !busy_frame_ready || !busy_frame || !sdl_ren) return;
  target = SDL_GetRenderTarget(sdl_ren);
  SDL_GetRenderDrawColor(sdl_ren, &r, &g, &b, &a);
  SDL_GetRenderClipRect(sdl_ren, &clip);
  clipped = SDL_RenderClipEnabled(sdl_ren);
  SDL_SetRenderTarget(sdl_ren, NULL);
  SDL_SetRenderClipRect(sdl_ren, NULL);
  WMD_RenderCopy(sdl_ren, busy_frame, NULL, NULL);
  SDL_SetRenderDrawColor(sdl_ren, 35, 35, 35, 255);
  WMD_RenderFillRect(sdl_ren, &bar);
  SDL_SetRenderDrawColor(sdl_ren, 230, 205, 100, 255);
  /* Embedded 8x16 PSF glyphs keep the indicator available in NOTTF builds. */
  for (i = 0; label[i]; i++) {
    const unsigned char *glyph = wmd_bitmap_font_psf + 32 + (unsigned char)label[i] * 16;
    for (gy = 0; gy < 16; gy++) for (gx = 0; gx < 8; gx++)
      if (glyph[gy] & (0x80u >> gx))
        WMD_RenderDrawPoint(sdl_ren, bar.x + 4 + i * 8 + gx,
                            bar.y + (STATUS_H - 16) / 2 + gy);
  }
  for (i = 0; i < 8; i++) {
    int brightness = 70 + ((i - phase + 8) % 8) * 23;
    SDL_Rect dot = {bar.x + 77 + dx[i], bar.y + STATUS_H / 2 + dy[i], 2, 2};
    SDL_SetRenderDrawColor(sdl_ren, brightness, brightness, 80, 255);
    WMD_RenderFillRect(sdl_ren, &dot);
  }
  SDL_RenderPresent(sdl_ren);
  SDL_SetRenderTarget(sdl_ren, target);
  SDL_SetRenderClipRect(sdl_ren, clipped ? &clip : NULL);
  SDL_SetRenderDrawColor(sdl_ren, r, g, b, a);
  if (sdl_win) SDL_PumpEvents(); /* queue input; dispatch it only after completion */
  busy_visible = 1; busy_last_draw = now;
}

static void
busy_checkpoint(void)
{
  uint64_t now;
  if (!busy_active || !busy_frame_ready) return;
  now = SDL_GetTicks();
  /* Fast jobs do not flash; longer jobs refresh at most ten times a second. */
  if (now - busy_started >= 150 && now - busy_last_draw >= 100) busy_draw(now);
}

/* Undo snapshots can themselves be large jobs. Bound each memory-copy slice. */
static void
busy_copy(void *dst, const void *src, size_t bytes)
{
  size_t offset = 0;
  while (offset < bytes) {
    size_t part = bytes - offset;
    if (part > 1024 * 1024) part = 1024 * 1024;
    busy_checkpoint();
    memcpy((unsigned char *)dst + offset, (const unsigned char *)src + offset, part);
    offset += part;
  }
  busy_checkpoint();
}

/* Opaque file codecs have no row checkpoints. Show activity before entering
 * them, and retain it until the next completed frame is presented. */
static void
busy_file_hint(void)
{
  if (!busy_visible) busy_draw(SDL_GetTicks());
}

static void
busy_end(void)
{
  busy_active = 0;
}

static void
busy_free(void)
{
  if (busy_frame) SDL_DestroyTexture(busy_frame);
  busy_frame = NULL; busy_frame_ready = busy_active = busy_visible = 0;
}
#endif
