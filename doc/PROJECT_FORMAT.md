# Proposed wmdpaint project format

## Goal

Give wmdpaint a native layered project format without adopting the complexity
of GIMP's XCF format.

Farbfeld remains wmdpaint's primary native image format. The project container
does not modify farbfeld or 1fpirate. It only collects standard image files and
associates them with layer and canvas metadata.

## Proposed layout

A project is a restricted ustar archive containing files such as:

```text
manifest
layers/000-background.ff
layers/001-ink.1fpirate
layers/002-colour.ff
layers/003-text.ff
merged.ff
```

Every `.ff` entry is an ordinary, specification-compliant farbfeld file that
can be extracted and used by other farbfeld tools. Every `.1fpirate` entry is
likewise an ordinary 1fpirate image.

`merged.ff` is an optional flattened preview and recovery image.

## Manifest example

```text
wmdpaint 1
width 512
height 512
active 2

layer layers/000-background.ff
name Background
x 0
y 0
visible 1
opacity 1.0

layer layers/001-ink.1fpirate
name Ink
x -12
y 8
visible 1
opacity 0.85
```

The initial manifest needs to record only:

- format version
- canvas width and height
- active layer
- layer order
- layer image path
- layer name
- layer x and y position
- visibility
- opacity

Text layers can initially be stored as rendered farbfeld layers. More metadata
can be introduced later if wmdpaint gains editable text objects or additional
layer properties.

## Container choice

Use uncompressed POSIX ustar as the canonical container. Implement only the
small subset needed by wmdpaint:

- regular files only
- fixed, relative paths
- no symbolic links or hard links
- no absolute paths
- no `..` path components
- no device files
- no archive extraction onto the filesystem while loading
- strict entry-count and size validation
- reject unsupported tar extensions instead of guessing

The loader should read entries directly into memory rather than behaving like
a general-purpose archive extractor. This avoids path traversal and link
problems.

Compression can remain external and optional:

```sh
bzip2 picture.wmd
gzip picture.wmd
zstd picture.wmd
```

This keeps bzip2, gzip, and zstd out of wmdpaint's mandatory dependencies. A
future version could recognize a compressed stream and invoke an external
decompressor, but that is not necessary for the first implementation.

## Layer encoding rules

- Use farbfeld for ordinary layers.
- Use 1fpirate only when a layer is genuinely monochrome and 1-bit.
- Never silently quantize a colour layer merely to reduce project size.
- Preserve all four 16-bit farbfeld channels exactly.
- Do not add private chunks or metadata to embedded farbfeld files.
- Store layer metadata in the manifest, not inside the image payload.

## Saving safely

Project saving should be atomic:

1. Write the complete archive to a temporary file in the destination
   directory.
2. Check all image and archive writes.
3. Flush and close the temporary file.
4. Rename it over the destination only after the archive is complete.

Failure must leave the previous project file intact.

## Forward compatibility

Manifest keys should be versioned. Readers may ignore unknown optional keys,
but must reject unknown required features. This allows future additions without
silently loading a project incorrectly.

For example:

```text
optional grid-size 16
required layer-groups 1
```

An older reader could ignore the first line but must reject the second if it
does not understand layer groups.

## File extension

The uncompressed archive uses `.wmd`. **File → Make Layered Archive...**
normally produces `.wmd.bz2`: wmdpaint writes the tar itself and runs the
external `bzip2` program to compress it. No bzip2 library is linked into
wmdpaint. If the program cannot be found or compression fails, the valid
uncompressed `.wmd` archive is retained so the user can compress it manually.

Normal **Save** and **Save As** continue to save ordinary farbfeld (or
1fpirate when explicitly selected). Making a layered archive is a separate
operation and does not replace the first-class farbfeld workflow.

## Why not XCF?

XCF serializes much of GIMP's internal document model. It includes multiple
format versions, offset-linked structures, tiled compression, numerous colour
precisions and models, layer groups, masks, channels, paths, blend and
compositing modes, profiles, text metadata, and historical compatibility
quirks.

A restricted ustar archive of standard farbfeld layers provides the parts
wmdpaint currently needs while remaining inspectable, recoverable, and small
enough to implement carefully.
