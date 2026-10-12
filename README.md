# wmdpaint

`wmdpaint` is a small, fast raster image editor built in the suckless spirit.
It uses C99, SDL, and the Nuklear immediate-mode GUI, with farbfeld as its
primary native image format.

This is a practical paint program rather than a framework: configuration is
compiled in, the application is built as one translation unit, assets are
embedded in the executable, and there are no plugins, scripting runtimes, or
runtime configuration files.

## Features

- 16-bit RGBA image data and spec-compliant farbfeld input/output
- Layers with visibility, opacity, alpha lock, Normal/Multiply/Screen/Overlay
  blending, ordering, independent size, and position
- Layer thumbnails, renaming, duplication, reordering, merging, and floating selections
- Brush, eraser, flood fill, selection, text, shapes, eyedropper, clone stamp,
  airbrush, move, resize, crop, rotate, and canvas scaling tools
- On-canvas layer resize handles with an aspect-ratio toggle and numeric sizing
- Nearest / Smooth sampling for layer and canvas scaling, up to 65,535 pixels per dimension
- Painting constrained to the selection mask, including clone, fill, and shapes
- Built-in brush sizes up to 1,024 pixels, with slider and numeric sizing
- Custom image-based brushes
- Zoom, pan, pixel grid, fullscreen, and read-only viewer mode
- Undo and redo snapshots for painting, transforms, layer operations, and layer properties
- Escape cancels an active move, rotate, scale, or perspective transform
- SDL3 system clipboard support between wmdpaint windows
- SDL3 by default, with an SDL2 compatibility build
- PNG, JPEG, BMP, and TGA import, with optional conventional-format export
- Built-in 8x16 bitmap font fallback for systems with no installed TTF fonts
- Layered `.wmd.bz2` project archives made from standard farbfeld and
  1fpirate entries

## Farbfeld is native

[Farbfeld](https://tools.suckless.org/farbfeld/) is a first-class citizen in
wmdpaint, not an intermediate compatibility format. The editor keeps pixels as
16-bit RGBA values internally and reads and writes the standard farbfeld
layout directly:

```
"farbfeld"                     8-byte magic
width, height                  unsigned 32-bit big-endian
red, green, blue, alpha        unsigned 16-bit big-endian per channel
```

There are no private chunks, extensions, alternate channel orders, or reduced
channel depths in `.ff` files written by wmdpaint. Defensive validation in the
loader may reject dimensions that cannot be represented safely in memory; it
must never change the farbfeld wire format.

The related `.1fpirate` format is available for deliberately monochrome,
1-bit work. Saving colour artwork as `.1fpirate` is lossy. Conventional image
formats are useful for interchange, but farbfeld remains the preferred format
for lossless editing and suckless tool pipelines.

Farbfeld streams can be used directly:

```sh
wmdpaint image.ff
cat image.ff | wmdpaint
png2ff < image.png | wmdpaint
```

## Build

Requirements:

- a C99 compiler
- SDL3 development files, or SDL2 when configured in `src/config.h`
- `make`

Build from the repository root:

```sh
make
```

The result is `./wmdpaint`. Vendored headers and generated asset headers are
already present; there is no fetch step.

Run the format, archive, rendering, stroke, resize, and document tests and compile
the supported configuration variants with:

```sh
make test
make check-config
make ui-test
```

The regression tests include scalar blend comparisons and headless SDL checks
that batched strokes preserve pixels and monochrome dithering. Layer-resize
checks cover aspect ratios, anchors, undo/redo, cancellation, and canvas
expansion. Format tests check exact wire bytes, truncation, invalid dimensions,
and write errors; document tests cover crop, canvas sizing, offsets, and layer
history. No display is needed for `make test`.

Measure the compositor, RGBA8 conversion, and software texture uploads with:

```sh
make bench
./tests/bench_canvas mixed
```

The benchmark uses only C99 and existing SDL timing APIs. See
[PERFORMANCE.md](doc/PERFORMANCE.md) for scenarios, results, and limitations.

`check-config` covers both confirmation prompts enabled and the build with the
text tool disabled. `ui-test` launches the application under Xvfb and checks
that SDL and the GUI initialize successfully; it skips cleanly when Xvfb is
not installed.

Install under `/usr/local` by default:

```sh
make install
```

Use `PREFIX` and `DESTDIR` for packaged installs.

## Use

```sh
./wmdpaint
./wmdpaint image.ff
./wmdpaint image.1fpirate
./wmdpaint image.png
```

There are no command-line options. When standard input is a pipe, wmdpaint
reads a farbfeld stream from it.

Normal **Save** and **Save As** preserve the current format. Saving a path
ending in `.wmd` or `.wmd.bz2` preserves the complete layered document and it
can be opened again later. Saving an ordinary image format writes the
composited image, as expected for PNG/JPEG/BMP/TGA/farbfeld output.

**File → Make Layered Archive...** is also available as an explicit project
save command. It creates a standard tar archive containing a manifest, one
farbfeld or 1fpirate file per layer, and a flattened `merged.ff`, then
compresses it with the external `bzip2` utility. If `bzip2` is unavailable,
wmdpaint keeps the uncompressed `.wmd` archive and reports the fallback.

By default, New, Open, and Exit do not ask for confirmation. Set
`COWARD_MODE` to `1` in `src/config_paint.h` if you prefer safety prompts when
discarding unsaved work. Failed saves and exports always leave the document
marked modified and display an error instead of claiming success.

The manual page source is [wmdpaint.1](wmdpaint.1). See the [user reference](doc/wmdpaint.md) and [development guide](doc/HACKING.txt).

## Configuration

Edit and rebuild:

- `src/config.h` for the SDL version, renderer selection, window defaults, and
  shared settings
- `src/config_paint.h` for paint tools, panels, layer limits, export support,
  and other wmdpaint-specific settings

Notable switches include `WMD_SDL_VERSION`, `MAX_LAYERS`, `COWARD_MODE`,
`SUCK_MORE`, and `NOTTF`.

The default build does not prefer SDL3's Vulkan renderer and still asks SDL for
OpenGL unless `NO_OPENGL` is enabled. This matters on old GPUs/drivers where a
Vulkan probe can crash Xorg before SDL can fall back. To force this
old-GPU-friendly path even when `src/config.h` is changed, use either target:

```sh
make novulkan
make opengl
```

Equivalent direct form:

```sh
make clean
make VULKANTARD_MODE=0
```

To build with SDL3 Vulkan preference enabled:

```sh
make vulkan
```

## Source layout

- `src/wmdpaint.c` — application state, UI, tools, and render loop
- `src/farbfeld.h` — standard farbfeld reader/writer and in-memory image type
- `src/1fpirate.h` — monochrome 1fpirate reader/writer
- `src/layer.h` — layer storage and compositing
- `src/sdl_wmd.h` — SDL2/SDL3 compatibility layer
- `src/config.h`, `src/config_paint.h` — compile-time configuration
- `doc/` — user and developer documentation
- `assets/` — source artwork and bundled fonts

The application is intentionally compiled as a single translation unit from
`src/wmdpaint.c`.

## License

Original wmdpaint code is available under the [MIT License](LICENSE).
Third-party code and assets remain under their respective licenses. See
[LICENSE_REVIEW.md](LICENSE_REVIEW.md) for the current dependency audit and
items whose provenance or redistribution terms should be confirmed before a
release.

## Design direction

The goal is a responsive GIMP replacement for quick edits and pixel work,
especially on modest hardware—not a clone of every GIMP subsystem. New
features should earn their complexity, preserve direct farbfeld workflows,
and avoid mandatory heavyweight dependencies.
