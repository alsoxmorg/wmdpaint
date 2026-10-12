/* wmdcbz configuration — edit and recompile */

/* initial window dimensions */
#define WIN_W     900
#define WIN_H     700

/* status bar height (top strip showing page info) */
#define STATUS_H   22

/* canvas background color (letterbox around page) */
#define BG_R  20
#define BG_G  20
#define BG_B  20

/* zoom step — each +/- multiplies or divides zoom by this factor */
#define ZOOM_STEP 1.15f

/* SDL backend version: 3 = SDL3 (default), 2 = SDL2 (legacy) */
#define WMD_SDL_VERSION 3

/* set to 1 on slow/ARM devices where vsync doesn't throttle the loop */
#define SLOWPOKE_MODE 0   /* New enhancments to code should make this*/
#define SLOWPOKE_MS   10  /* obsolete*/

/* set to 1 on systems with no GPU or broken OpenGL drivers */
#define NO_OPENGL 0  /* broken/does not build. - not recommended */

/* set to 1 to prefer the Vulkan renderer backend (SDL3 only).
 * Keep default 0: old GPUs/drivers can crash Xorg before SDL can fall back. */
#ifndef VULKANTARD_MODE
#define VULKANTARD_MODE 0
#endif

/*Below this line wmdmedia other apps.*/

/* WMD BLAST ONLY*/
/* dancer animation — directory of PNG frames, and scale divisor
 * (DANCER_SCALE 2 = half source resolution; 3 = third; etc.) */
#define DANCER_DIR   "assets/frames"
#define DANCER_SCALE 2


/* wmdpdf render DPI — higher is sharper but slower on first open */
#define PDF_DPI 150

/* wmdpdf gap between pages in two-page spread mode (source pixels at PDF_DPI) */
#define SPREAD_GAP 8

/* set to 0 to build without PDF support (no poppler-utils dependency; cbz/dir only) */
#define ENABLE_PDF_SUPPORT 1

/* wmddoomsday initial window dimensions */
#define CAL_WIN_W   720
#define CAL_WIN_H   460

/* wmddoomsday right-side event list panel width */
#define CAL_PANEL_W 220
