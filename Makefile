CC     = cc
WMD_SDL_VER != awk '/define WMD_SDL_VERSION/{print $$3}' src/config.h
SDL_PACKAGE != test "$(WMD_SDL_VER)" = 3 && echo sdl3 || echo sdl2
SDL_CONFIG  != test "$(WMD_SDL_VER)" = 3 && echo sdl3-config || echo sdl2-config
SDL_CFLAGS  != $(SDL_CONFIG) --cflags 2>/dev/null || pkg-config --cflags $(SDL_PACKAGE) 2>/dev/null
SDL_LIBS    != $(SDL_CONFIG) --libs 2>/dev/null || pkg-config --libs $(SDL_PACKAGE) 2>/dev/null
NK_RENDERER != test "$(WMD_SDL_VER)" = 3 && echo nuklear_sdl3_renderer.h || echo nuklear_sdl_renderer.h

CONFIG_CFLAGS =
ifneq ($(VULKANTARD_MODE),)
CONFIG_CFLAGS += -DVULKANTARD_MODE=$(VULKANTARD_MODE)
endif

CFLAGS  = -std=c99 -Wall -Wextra -O2 -Isrc $(SDL_CFLAGS) $(CONFIG_CFLAGS)
LDFLAGS = $(SDL_LIBS) -lm

PAINT_DEPS  = src/config_paint.h src/busy.h src/create.h src/filebrowser.h src/farbfeld.h \
              src/1fpirate.h src/layer.h src/canvas_render.h src/icons_data.h src/logo_data.h \
              src/stb_image_write.h src/stb_truetype.h src/wmdarchive.h \
              src/bitmap_font_data.h

BIN = wmdpaint

all: $(BIN)

$(BIN): src/wmdpaint.c src/stb_image.h src/nuklear.h src/$(NK_RENDERER) src/config.h src/sdl_wmd.h $(PAINT_DEPS)
	$(CC) $(CFLAGS) -o $@ src/wmdpaint.c $(LDFLAGS)

vulkan:
	$(MAKE) clean
	$(MAKE) VULKANTARD_MODE=1 $(BIN)

novulkan:
	$(MAKE) clean
	$(MAKE) VULKANTARD_MODE=0 $(BIN)

opengl: novulkan

PREFIX  ?= /usr/local
DESTDIR ?=

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -f $(BIN) tests/test_wmdarchive tests/bench_canvas tests/test_canvas_render tests/test_paint_stroke tests/test_layer_resize tests/test_image_formats tests/test_document_ops tests/test_busy

tests/test_wmdarchive: tests/test_wmdarchive.c src/config_paint.h src/layer.h src/wmdarchive.h
	$(CC) -std=c99 -Wall -Wextra -Wno-unused-function -O2 -Isrc -o $@ $<

test: tests/test_wmdarchive tests/test_canvas_render tests/test_paint_stroke tests/test_layer_resize tests/test_image_formats tests/test_document_ops tests/test_busy
	./tests/test_wmdarchive
	./tests/test_canvas_render
	./tests/test_paint_stroke
	./tests/test_layer_resize
	./tests/test_image_formats
	./tests/test_document_ops
	./tests/test_busy

check-config:
	$(MAKE) clean
	$(MAKE) CONFIG_CFLAGS='-DCOWARD_MODE=1' wmdpaint
	$(MAKE) clean
	$(MAKE) CONFIG_CFLAGS='-DNOTTF=1' wmdpaint
	$(MAKE) clean

ui-test: all
	./tests/ui_smoke.sh

.PHONY: all vulkan novulkan opengl clean install uninstall test check-config ui-test

tests/bench_canvas: tests/bench_canvas.c src/canvas_render.h src/layer.h src/farbfeld.h src/sdl_wmd.h src/config.h
	$(CC) $(CFLAGS) -Wno-unused-function -o $@ $< $(LDFLAGS)

bench: tests/bench_canvas
	./tests/bench_canvas

.PHONY: bench

tests/test_canvas_render: tests/test_canvas_render.c src/canvas_render.h src/layer.h src/farbfeld.h
	$(CC) -std=c99 -Wall -Wextra -Wno-unused-function -O2 -Isrc -o $@ $<

# Include the application to verify real callbacks without opening a window.
# -O0 keeps this integration check quick; timings live in the -O2 benchmark.
tests/test_paint_stroke: tests/test_paint_stroke.c src/wmdpaint.c $(PAINT_DEPS) src/config.h src/sdl_wmd.h src/stb_image.h src/nuklear.h src/$(NK_RENDERER)
	$(CC) $(CFLAGS) -O0 -Wno-unused-function -o $@ $< $(LDFLAGS)

tests/test_layer_resize: tests/test_layer_resize.c src/wmdpaint.c $(PAINT_DEPS) src/config.h src/sdl_wmd.h src/stb_image.h src/nuklear.h src/$(NK_RENDERER)
	$(CC) $(CFLAGS) -O0 -Wno-unused-function -o $@ $< $(LDFLAGS)

tests/test_image_formats: tests/test_image_formats.c src/farbfeld.h src/1fpirate.h
	$(CC) -std=c99 -Wall -Wextra -Wno-unused-function -O2 -Isrc -o $@ $<

tests/test_document_ops: tests/test_document_ops.c src/wmdpaint.c $(PAINT_DEPS) src/config.h src/sdl_wmd.h src/stb_image.h src/nuklear.h src/$(NK_RENDERER)
	$(CC) $(CFLAGS) -O0 -Wno-unused-function -o $@ $< $(LDFLAGS)

tests/test_busy: tests/test_busy.c src/busy.h src/bitmap_font_data.h src/config_paint.h src/config.h src/sdl_wmd.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
