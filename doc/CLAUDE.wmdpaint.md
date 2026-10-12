# wmdpaint — raster paint program

Suckless spirit. Single C99 translation unit. SDL3+Nuklear UI, with an SDL2 compatibility path. Runs X11/Wayland/macOS/Arm. Farbfeld (`.ff`) is the primary native format; 1fpirate (`.1fpirate`) is the native monochrome format.

## Invocation

```
wmdpaint                # blank canvas
wmdpaint image.ff
wmdpaint image.png
cat image.ff | wmdpaint # reads farbfeld from stdin if stdin is not a tty
```

No other command-line flags.

## Tools (left panel)

| Label | Name | Description |
|-------|------|-------------|
| `SEL` | Select | Rectangle select; defines region for copy/cut. |
| `BRS` | Brush | Freehand paint with foreground colour. Options: size, shape (square/circle), mode (pencil hard-edge / soft feathered). |
| `ERA` | Eraser | Paints fully transparent pixels. Same size/shape options as brush. |
| `CLN` | Area copypasta | Ctrl+click or right-click sets source; drag copies pixels from the stable source region. |
| `AIR` | Airbrush | Spray: scatter (random pixels in radius) or soft (Gaussian-weighted translucent buildup). Flow density slider. |
| `LIN` | Shape | Click+drag: line, filled rect, or circle. Committed on mouse release. |
| `BKT` | Flood fill | BFS fill of connected same-colour region with foreground colour. |
| `TXT` | Text | Click to place text layer. Backspace/Return/Escape. Font list via mouse or Up/Down. Disabled with `NOTTF=1`. |
| `EYE` | Eyedropper | Samples composited pixel → sets foreground. Returns to brush after sampling. |
| `MOV` | Transform layer | Choose Move, Rotate, or 3D Transform. In 3D Transform, drag any of the four corner handles to perspective-warp the active layer. Arrow keys nudge 1 px; Shift+Arrow = 10 px. |
| `RSZ` | Resize | Drag on canvas to scale active layer. Bilinear resampling; destructive downscale. |

## Menus

### File
- **New Window** — spawns a new blank wmdpaint process (current canvas is unchanged)
- **Create...** — dialog: specify exact dimensions for a new canvas in the current window
- **Open in New Window...** — file browser; opens selected file in a new wmdpaint process; accepts `.ff`, `.1fpirate`, PNG, JPEG, BMP, TGA
- **Save** — saves to current path (`.ff` or `.1fpirate`); falls back to Save As if unsaved
- **Save As...** — file browser; `.ff` or `.1fpirate` only
- **Export...** — PNG/JPEG/BMP/TGA via `stb_image_write`; **only available with `SUCK_MORE=1`**
- **Make Layered Archive...** — writes a normal tar containing a manifest,
  standard farbfeld/1fpirate layers, and `merged.ff`, then uses the external
  `bzip2` program to produce `.wmd.bz2`. If bzip2 is unavailable, retains the
  `.wmd` tar for manual compression. This does not replace normal farbfeld Save.
- **Exit**

### Image
- **Info** — popup: canvas dimensions and pixel format
- **[ ] 1fpirate Mode** — converts layers to dithered black/white and keeps edits 1-bit while enabled. Turn it off to resume normal colour painting on top of the line art. The toggle is undoable, but lost colours are not. With `COWARD_MODE`, enabling warns before destroying colours.
- **Flatten Image** — composite visible layers into a single Background layer
- **Resize...** — scale entire canvas + all layers
- **Crop Canvas** — crops to the current selection rectangle (drawn with the `SEL` tool). Does nothing if no selection is active.

### Edit
- **Undo (Ctrl+Z) / Redo (Ctrl+Y)** — up to `UNDO_LEVELS` in-memory editing steps. History is discarded when the document is replaced or the program exits.
- **Copy** — composited selection → clipboard (also pushed to system clipboard for cross-window paste)
- **Cut** — copy + erase from active layer
- **Paste** — clipboard → new layer above active; auto-switches to Move tool. If the pasted layer's right/bottom edge exceeds the canvas, the canvas is automatically expanded to fit. Pulls from system clipboard first, so paste works across multiple wmdpaint windows (SDL3 only; uses MIME type `application/x-wmdpaint`)

### Layers
- **[ ] Expand on flatten** — when on, compositing grows canvas to all layer extents; off (default) clips at canvas bounds
- **Merge Layer Down** — composite the active layer into the layer below; does nothing on the bottom layer
- **Resize Layer...** — scale active layer independently
- **Rotate CW 90°** — rotate active layer 90° clockwise (swaps layer dimensions)
- **Rotate CCW 90°** — rotate active layer 90° counter-clockwise (swaps layer dimensions)

### View
- **Zoom In / Zoom Out** — increase/decrease canvas_zoom
- **[ ] Show Grid  G** — toggle pixel-boundary grid overlay (only visible at zoom ≥ `GRID_MIN_ZOOM`, default 4×)
- **Fit to Window  0** — zoom to fit canvas in the available area

### Colors
- **Color Picker** — floating picker panel (also opened by colour swatch in tool panel)
- **Brushes...** — open the custom brush picker (see [Custom brushes](#custom-brushes))

## Custom brushes

Custom brush shapes are loaded from `~/.config/wmdmedia/templates/brushes/`. Any file with a recognised extension is loaded as a 1-bit mask:

- `.1fpirate` — native 1-bit farbfeld (preferred)
- `.ff` — farbfeld (16-bit RGBA; converted to 1-bit via BT.601 luminance)
- `.png`, `.jpg`, `.jpeg`, `.bmp`, `.tga` — any format stb_image supports

**Mask generation:** pixels with alpha ≥ 50% and BT.601 luminance ≥ 50% become "on" bits (the brush paints there). Transparent or dark pixels are "off".

Open the picker via **Colors → Brushes...**. Click a thumbnail to make it the active brush; it replaces the standard square/circle brush for all drawing tools that use the brush shape. Click **None (circle/square)** to revert. Click **Reload Brushes** to rescan the directory without restarting.

Close the dialogue with the **✕** button or **Escape**.

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| `+` / `=` | Zoom in |
| `-` | Zoom out |
| `0` | Zoom to fit window |
| `G` | Toggle grid overlay |
| `L` | Toggle read-only lock (viewer mode) |
| `F` | Toggle fullscreen |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl`+drag | Pan canvas |
| `Ctrl+D` | Deselect (clear selection rectangle) |
| `Escape` | Close brushes picker |
| Arrow keys | Nudge active layer 1 px (Move tool) |
| `Shift`+Arrow | Nudge active layer 10 px (Move tool) |
| `Up` / `Down` | Step through font list (Text tool) |
| `Backspace` | Delete last character (Text tool) |
| `Return` | Insert newline (Text tool) |
| `Escape` | Commit text, stop typing (Text tool) |

## Mouse

| Action | Effect |
|--------|--------|
| Left drag | Draw with active tool |
| Right click | Set source point for Area copypasta only |
| Ctrl + left drag | Pan canvas (except Area copypasta; Ctrl+click selects its source) |
| Middle click + drag | Pan canvas (any tool) |
| Ctrl + click | Set source point (Area copypasta) |
| Scroll wheel | Zoom in/out |

## Layers

- Up to `MAX_LAYERS` independent RGBA pixel buffers with independent positions.
- Right panel: visibility checkbox, name (click = activate), reorder arrows, delete button, opacity slider (0–100%). Stack is top→bottom; index 0 = bottommost.
- Compositing: Porter-Duff "over" in stack order; each layer's opacity scales its alpha before compositing.
- Layers may extend beyond canvas bounds; "Expand on flatten" controls whether compositing absorbs overflow.

## File formats

| Extension | Format | Notes |
|-----------|--------|-------|
| `.ff` | Farbfeld | 16-bit RGBA big-endian. Full colour depth. Primary save format. |
| `.1fpirate` | 1fpirate | 1-bit monochrome; colour quantised via BT.601 luminance threshold. Saving colour data here is lossy. |
| `.png .jpg .bmp .tga` | stb_image | Load only by default. Export requires `SUCK_MORE=1`. |

Farbfeld and 1fpirate are first-class formats. `farbfeld.h` and `1fpirate.h` are single-header loaders/writers in `src/`. Farbfeld output uses the unmodified suckless specification: `farbfeld` magic, big-endian 32-bit dimensions, then row-major RGBA pixels with four big-endian 16-bit channels. Do not add private chunks or alternate encodings. Loader allocation checks may reject dimensions that cannot be represented safely, but must not change the wire format.

## Compile-time configuration

Paint-specific tunables: `src/config_paint.h`. Shared SDL/renderer settings: `src/config.h`.

| Define | Default | File | Description |
|--------|---------|------|-------------|
| `WIN_W`, `WIN_H` | 900×650 | `config.h` / `config_paint.h` | Initial window size |
| `BG_R/G/B` | 20,20,20 | `config.h` | Canvas background colour |
| `WMD_SDL_VERSION` | 3 | `config.h` | SDL backend: 3=SDL3, 2=SDL2 |
| `DEFAULT_W`, `DEFAULT_H` | 512×512 | `config_paint.h` | Blank canvas dimensions (File → New) |
| `CHECKER_SIZE` | 16 | `config_paint.h` | Transparency checkerboard square size (px) |
| `CHECKER_LIGHT`, `CHECKER_DARK` | 255, 192 | `config_paint.h` | Checkerboard colours |
| `LAYER_PANEL_W` | 200 | `config_paint.h` | Right panel width |
| `MAX_LAYERS` | 32 | `config_paint.h` | Max layers per document |
| `UNDO_LEVELS` | 32 | `config_paint.h` | Maximum in-memory undo and redo steps |
| `COWARD_MODE` | 0 | `config_paint.h` | 1 = confirmation dialogs on New/Exit and before lossy 1fpirate save |
| `SUCK_MORE` | 1 | `config_paint.h` | 1 = enables File → Export (PNG/JPEG/BMP/TGA) |
| `NOTTF` | 0 | `config_paint.h` | 1 = disables text tool (when stb_truetype unavailable) |
| `GRID_MIN_ZOOM` | 4.0 | `config_paint.h` | Minimum zoom at which Show Grid lines appear |
| `NO_OPENGL` | 0 | `config.h` | Disable OpenGL renderer hint |
| `VULKANTARD_MODE` | 0 | `config.h` | Prefer Vulkan (SDL3 only) |

For everyday use, enable in `config_paint.h`:
```c
#define COWARD_MODE 1
#define SUCK_MORE   1
```

## Building

```
make
```

Runtime dep: libSDL3 (or libSDL2 if `WMD_SDL_VERSION=2`).
Build deps: `xxd` (embeds `assets/icons.1fpirate` → `icons_data.h` and `assets/wmdlogo.png` → `logo_data.h`). `tinyxxd` is a sufficient substitute.

Icon and logo are embedded at build time; `assets/` is not needed at runtime.

## SDL3 text input note

SDL3 requires `SDL_StartTextInput(win)` before text events flow. In wmdpaint this is managed via `nk_sdl_update_TextInput(ctx)` called once per frame after `nk_input_end()`, wrapped as `WMD_NK_UPDATE_TEXT_INPUT(ctx)` in `sdl_wmd.h`. SDL2 does not need this call (the macro is a no-op there). If text input ever breaks in edit fields, verify this call is present in the render loop.

## Caveats

- Undo history is bounded by `UNDO_LEVELS` and exists only in memory; it is not saved in image or project files.
- Export hidden unless `SUCK_MORE=1`.
- Font scan (Text tool) happens once on first use; fonts installed after launch require restart.
- Saving colour image to `.1fpirate` is lossy; enable `COWARD_MODE` for a confirmation prompt.
- Built-in brush shapes are square and circle only; arbitrary shapes require a custom brush image.
- No per-layer blend mode controls (only opacity).

## Goals / design intent

Fast image editing on low-end hardware (primary test device: PineTab2). Gimp replacement for quick edits — not a full Gimp clone. Small binary (~600 KB), no dbus/glib/gtk dependencies, opens instantly.
