# wmdpaint contributor notes

## Project

`wmdpaint` is a suckless-style raster paint program written in C99. It uses
Nuklear over SDL3 by default and retains an SDL2 compatibility path. The
application compiles as one translation unit from `src/wmdpaint.c`; logical
subsystems live in small headers.

Farbfeld (`.ff`) is the primary native format. 1fpirate (`.1fpirate`) is its
deliberately 1-bit monochrome sibling. PNG, JPEG, BMP, TGA, and other stb
formats are interchange formats rather than replacements for farbfeld.

## Build

```sh
make
make clean
```

Run with `./wmdpaint [file]`. A farbfeld stream may also be supplied on
standard input. Vendored dependencies and generated asset headers are checked
in; the Makefile has no `fetch` or `distclean` target.

`src/config.h` selects SDL2 or SDL3. `src/config_paint.h` contains paint-only
configuration. Both are compile-time configuration and require rebuilding.

## Architecture

| File | Role |
|---|---|
| `src/wmdpaint.c` | Window, event loop, application state, UI, tools, and rendering |
| `src/farbfeld.h` | `FFImage` plus standard farbfeld load/save functions |
| `src/1fpirate.h` | 1-bit monochrome image load/save functions |
| `src/layer.h` | Layer buffers and clipped compositing |
| `src/canvas_render.h` | Shared clipped rendering and RGBA8 conversion |
| `src/wmdarchive.h` | Restricted tar writer and optional external bzip2 compression |
| `src/sdl_wmd.h` | SDL2/SDL3 compatibility wrappers |
| `src/filebrowser.h` | Immediate-mode file browser |
| `src/create.h` | New-canvas dialog |
| `src/config.h` | Shared renderer and platform configuration |
| `src/config_paint.h` | Paint-specific compile-time configuration |

Per frame, input is passed to Nuklear, menus and panels are built, canvas tool
input is handled, modified layers are composited, dirty pixels are uploaded to
the SDL texture, and the canvas plus UI are rendered.

The pixel pipeline is:

```
drawing tool -> Layer.px -> canvas_composite() -> FFImage.px
             -> RGBA8 conversion -> streaming SDL texture
```

Dirty rectangles limit recomposition, conversion, and texture upload during
ordinary strokes. A full composition is still used for operations that affect
the whole document.

## Farbfeld invariant

Do not invent a wmdpaint-specific farbfeld variant. Files written by
`src/farbfeld.h` must comply with the suckless farbfeld specification exactly:

- eight bytes containing `farbfeld`
- unsigned 32-bit big-endian width and height
- width × height pixels in row-major order
- red, green, blue, and alpha for each pixel
- unsigned 16-bit big-endian value for every channel

`FFImage.px` stores the same RGBA channel values as native-endian `uint16_t`
words in memory. Endian conversion occurs only at the I/O boundary.

Bounds and overflow checks are encouraged. They protect allocation and parsing
from dimensions that do not fit in memory; they do not alter the on-disk
format, impose a smaller valid farbfeld specification, or reduce colour depth.

## Conventions

- Preserve the C99, single-translation-unit build.
- Prefer direct, small functions over new abstraction layers.
- Keep tool state file-local in `src/wmdpaint.c` unless a subsystem has a clear
  independent boundary.
- Do not edit vendored Nuklear or stb headers for application behavior.
- Keep internal colour and alpha values at 16-bit precision.
- Use farbfeld as the default lossless editing path.
- Treat non-farbfeld export as optional convenience functionality.
- Preserve layered `.wmd`/`.wmd.bz2` paths through normal Save/Save As;
  ordinary image paths save the composited image.
- Avoid new mandatory runtime services, plugin systems, or configuration
  parsers.
- Update `README.md`, `doc/wmdpaint.md`, and the manual page when user-visible
  behavior changes.

## Important configuration

- `WMD_SDL_VERSION`: 3 by default; 2 selects the legacy SDL2 backend
- `MAX_LAYERS`: compile-time layer limit
- `COWARD_MODE`: confirmation prompts for destructive/lossy actions
- `SUCK_MORE`: enables conventional image export
- `NOTTF`: removes the text tool
- `SLOWPOKE_MODE`: optional frame delay for constrained hardware
- `VULKANTARD_MODE`: prefer Vulkan with SDL3
