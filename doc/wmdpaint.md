# Using wmdpaint

wmdpaint is a small C99 raster editor. Run `./wmdpaint [file]`; there are no
command-line options. Piped standard input is read as farbfeld. Configuration
is compiled from `src/config.h` and `src/config_paint.h`.

## Files and layers

File → New creates a default canvas; Create lets you choose dimensions.
Open replaces the document, Open as Layers imports into the current stack,
and Open in New Window opens a separate editor.

Save and Save As use the chosen path's format. Use `.wmd` or `.wmd.bz2` to
preserve layers and their properties. Ordinary image files contain the
composited result. Farbfeld preserves 16-bit RGBA; 1fpirate reduces colour to
black/white and cannot store transparency. Conventional image export requires
`SUCK_MORE=1`. Make Layered Archive provides an explicit project save command;
compression uses external bzip2, with an uncompressed fallback.

The layer panel selects the drawing target and controls visibility, opacity,
alpha lock, blend mode, name, and stacking order. Layer menu commands add,
duplicate, delete, reorder, merge, flatten, flip, and rotate layers. Layers
have independent sizes and positions. Expand on flatten allows compositing
to enlarge the canvas for layer extents; it is off by default.

## Drawing and selections

The tool strip contains selection, brush, eraser, clone, airbrush, de-color,
shapes, bucket fill, text, eyedropper, move, resize, and crop tools. Options
appear in floating panels. Text is absent when built with `NOTTF=1`; an
embedded bitmap font is available if no TrueType fonts are found.

Selection options include rectangle, ellipse, colour selection, and magic
wand. Select provides All, None, Invert, Float, Fill, and Delete. Copy reads
composited pixels; Cut deletes from the active layer; Paste creates a layer.
SDL3 supports system clipboard exchange and conventional image input.
Brush, eraser, clone, airbrush, de-color, fill, and shapes obey the active
selection mask, including holes and offset layers. With no selection, painting
is unrestricted. Flood fill stays within the connected selected region.

Built-in brushes have a 1–1,024 pixel diameter. The wider size slider and
exact numeric size field are in the color picker. `BRUSH_SIZE_MAX` in
`src/config_paint.h` sets the maximum; image brushes keep their native size.

Clone uses Ctrl+click or right-click to set the source. The de-color brush
makes nontransparent RGB black while retaining alpha. Colors provides
colour picking, brushes, greyscale, 1-bit conversion, and inversion.
1fpirate mode dithers existing colours destructively; turning the mode off
allows colour painting again, but undoing the toggle does not recover the
original colours.

## Sizing and navigation

Resize (RSZ) and Move → Scale share four yellow corner handles. Drag a corner
to resize the active layer with the opposite corner fixed. Keep aspect ratio
is on by default; uncheck it to change width and height independently. The
Resize panel also provides linked numeric dimensions and Layer/Canvas apply
buttons. Panels can be moved to expose a handle. Each drag is one undo action;
Escape cancels it. Previews sample the original raster. Nearest copies source pixels exactly for
pixel art; Smooth uses bilinear interpolation and is the default. The shared
choice appears in Resize, Move → Scale, and Image → Scale Canvas.

Layer and canvas sizing controls support dimensions up to 65,535 pixels
(`SCALE_SIZE_MAX` in `src/config_paint.h`). Canvas scaling prepares all layer
rasters before applying changes; failed allocations leave the document intact.

Image → Resize Canvas changes the canvas bounds while preserving layer pixels
and positions. Scale Canvas scales the document and its layers. Crop Canvas
crops to the current selection bounds and discards pixels outside them.
The crop tool also supports dragging a crop rectangle. Fit Canvas to Layers
grows the canvas to encompass layer extents.

Pan with middle-button drag, or Ctrl+left drag except with the clone tool.
Scroll zooms around the pointer. `0` fits the image and `1` shows actual size;
both reset panning. Help → About displays application information.

| Shortcut | Action |
|---|---|
| Ctrl+S / Ctrl+Shift+S | Save / Save As |
| Ctrl+Q | Exit |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Ctrl+C / Ctrl+X / Ctrl+V | Copy / Cut / Paste |
| Ctrl+A / Ctrl+D | Select all / Clear selection |
| Shift+Backspace / Delete | Fill / Delete selection |
| + or = / - | Zoom in / out |
| 0 / 1 | Fit / Actual size |
| Ctrl+F | Pixel grid |
| Ctrl+L | Canvas lock (viewer mode) |
| F11 | Fullscreen |
| Arrow / Shift+Arrow | Move-tool nudge by 1 / 10 pixels |
| Escape | Cancel active transform or close a popup |

Undo history is bounded by `UNDO_LEVELS` and stored in memory. New, Open, and
Exit do not prompt by default; enable `COWARD_MODE=1` for confirmation dialogs.
Failed saves keep the document modified and display an error.

See [README](../README.md) for building and [PROJECT_FORMAT.md](PROJECT_FORMAT.md)
for layered archive details.
