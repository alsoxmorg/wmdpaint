/* create.h — preset canvas sizes for File -> Create
 *
 * To add a size: append a row to canvas_presets[] and bump NPRESETS.
 * Pixel dimensions for paper/manga are given at the dpi noted in the name.
 */
#ifndef CREATE_H
#define CREATE_H

#include <stdint.h>

typedef struct {
    const char *category;
    const char *name;
    uint32_t    w, h;
} CanvasPreset;

#define NPRESETS 12

static const CanvasPreset canvas_presets[NPRESETS] = {
    /* ---- Screen -------------------------------------------------- */
    { "Screen", "Phone portrait",     1080, 1920 },
    { "Screen", "HD 720p",            1280,  720 },
    { "Screen", "Full HD 1080p",      1920, 1080 },
    { "Screen", "QHD 1440p",          2560, 1440 },
    { "Screen", "4K UHD",             3840, 2160 },
    /* ---- Paper @ 300 dpi ----------------------------------------- */
    { "Paper",  "Letter (8.5x11\")",  2550, 3300 },
    { "Paper",  "A4",                 2480, 3508 },
    { "Paper",  "A5",                 1748, 2480 },
    /* ---- Manga @ 350 dpi ----------------------------------------- */
    { "Manga",  "B5 page",            2508, 3543 },
    { "Manga",  "B6 pocket",          1772, 2508 },
    /* ---- Square -------------------------------------------------- */
    { "Square", "512 x 512",           512,  512 },
    { "Square", "1024 x 1024",        1024, 1024 },
};

#endif /* CREATE_H */
