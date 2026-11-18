# Canvas performance

The editor and headless benchmark share the CPU rendering code in
`src/canvas_render.h` and `src/layer.h`. There are no new dependencies,
threads, image formats, or runtime settings.

## Reproduce

```sh
make bench
./tests/bench_canvas mixed
make test
```

`bench` measures 3072×2048 canvases with four full-sized layers and
2048×2048 canvases with sixteen full-sized layers. Each case reports full
updates and 64×64 dirty updates, separating compositing, 16-bit-to-8-bit
conversion, and SDL texture upload time. Dense opaque Normal layers and
sparse Normal layers represent painted documents; `mixed` exercises
translucent layers at 75% opacity using all four blend modes.

Each case initializes the full buffer and texture before timing, runs a
warm-up update, then averages five full updates or 200 dirty updates.
Document allocation and initialization are outside the measured interval.
The software renderer needs no window or display server. Uploads use the
same rectangle pointer and full-canvas pitch as the editor.

The default run also compares the render work for a horizontal segment of
64 overlapping 64×64 stamps: 64 separate updates versus one 127×64 union.
This isolates compositing/conversion/upload and excludes painting and undo.

Sixteen full-sized 2048×2048 RGBA16 layers consume about 512 MiB before
canvas, conversion buffer, and SDL texture storage. Run on an otherwise idle
machine with enough memory; swapping and concurrent work distort timings.
These are elapsed measurements, not pass/fail performance assertions.

## Changes

- Skip layers at zero opacity.
- Specialize Normal layers at full opacity once per layer: skip transparent
  pixels and copy fully opaque pixels directly. Translucent pixels retain
  the original arithmetic. Layers with other opacities or blend modes use
  the original general loop, avoiding extra per-pixel shortcut branches.
- Convert each dirty row as a contiguous channel span instead of computing
  four indexed assignments for each pixel.
- During a colour-mode Bresenham stroke segment, accumulate the dirty bounds
  and composite/convert/upload once after all stamps. Single stamps remain
  immediate. The caller still takes one undo snapshot at stroke start.
- Keep monochrome strokes on the original per-stamp rendering path. Dithering
  modifies layer pixels, so postponing it could change the result.

Canvas expansion and other full-document operations still request a full
recompose. Textures and conversion buffers remain persistent between updates.
Batching uses two integer flags and existing rectangle bounds; no extra image
cache or per-layer metadata is needed.

## Initial measurements

Measured in the development environment on 2026-10-11: aarch64, GCC 16.1.1,
`-O2`, SDL 3.4.16, software renderer. Absolute times describe this environment,
not expected desktop responsiveness. The original and optimized loops use
the same data, sizes, and timed stages.

| Full update | Composite before | Composite after | Conversion before | Conversion after |
|---|---:|---:|---:|---:|
| 3072×2048, 4 opaque layers | 659.49 ms | 137.10 ms | 78.71 ms | 16.47 ms |
| 2048×2048, 16 opaque layers | 1751.90 ms | 312.12 ms | 53.68 ms | 11.33 ms |
| 3072×2048, 4 sparse layers | 664.83 ms | 260.58 ms | 78.86 ms | 18.57 ms |
| 2048×2048, 16 sparse layers | 1728.26 ms | 570.83 ms | 53.16 ms | 11.02 ms |

These samples show about 4.8–5.6× faster full compositing for opaque Normal
layers and 2.5–3.0× faster full compositing for sparse Normal layers. Full-frame
conversion is about 4.2–4.8× faster. Software upload time is still around
8–15 ms; the optimization reduces how often uploads are requested in a stroke.

For fully translucent mixed-blend stacks, an initial version of the shortcut
branches regressed full compositing by about 16%. The final implementation
specializes only full-opacity Normal layers and retains the general loop:

| Mixed translucent full update | Original general loop | Final implementation |
|---|---:|---:|
| 3072×2048, 4 layers | 909.94 ms | 906.17 ms |
| 2048×2048, 16 layers | 2405.26 ms | 2414.33 ms |

The mixed comparison uses the same optimized conversion helper on both sides
so it isolates compositing. Differences here are below 1% in this sample.

For sixteen sparse layers, the 64-stamp horizontal rendering comparison was
127.93 ms for per-stamp updates and 2.87 ms for a union update (about 45× less
render work). This excludes stamp painting, undo, and other UI work, so it is
not an end-to-end stroke speedup claim. Large diagonal unions or different
stamp sizes can give different results.

Small-region timings are more variable and should be compared over repeated
runs rather than treated as precise ratios.

## Validation and limits

`make test` compares optimized blending with an independent scalar reference
across offsets, clips, opacities, transparent/opaque/partial pixels, and all
blend modes. It also checks partial recomposition and conversion against a
full update and verifies untouched pixels are preserved.

The stroke integration test includes the actual application callbacks and
runs them against the original per-stamp stroke loop using a headless SDL
software renderer. It checks layer, composite, and RGBA8 bytes for brush,
eraser, clone, airbrush, decolour, and custom brushes, including reverse
strokes, clipped/offset layers, alpha lock, soft modes, and monochrome mode.
It counts texture uploads and checks zero-length/no-op strokes.

Software texture-upload timings do not measure a real GPU, driver, rendering,
or presentation. Undo snapshots, thumbnails, input processing, and file I/O
are not included. Full-frame blending of many translucent layers remains
expensive. A long diagonal stroke can produce a large union rectangle;
monochrome dithering and undo snapshot memory are further profiling targets.
