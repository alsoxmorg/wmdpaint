/* wmdpaint configuration — edit and recompile
 * Includes shared config.h for SDL version, renderer flags, and BG colours,
 * then adds paint-specific tunables. */
#include "config.h"

/* wmdpaint wants a shorter default window */
#undef WIN_H
#define WIN_H 650

/* menu bar height */
#define MENU_H          22

/* left tool panel width: single column of 44px buttons + padding */
#define TOOL_W          52
#define TOOL_H          52  /* toolbar height when TOOL_ON_TOP=1 */
#define TOOL_ON_TOP      0 /* put the toolbar on top, right below the menu, old school */

/* floating panel anchor — adapts to toolbar position */
#if TOOL_ON_TOP
# define POPUP_X  4
# define POPUP_Y  (MENU_H + TOOL_H)
#else
# define POPUP_X  (TOOL_W + 4)
# define POPUP_Y  MENU_H
#endif


/* default new image dimensions */
#define DEFAULT_W      512
#define DEFAULT_H      512

/* transparency checkerboard */
#define CHECKER_SIZE    16
#define CHECKER_LIGHT  255
#define CHECKER_DARK   192

/* layer panel width (right side) */
#define LAYER_PANEL_W  200

/* status bar height (bottom of window) */
#undef  STATUS_H
#define STATUS_H        18

/* maximum number of layers */
#define MAX_LAYERS      32

/* in-memory undo steps; history is never saved with the document */
#define UNDO_LEVELS     32

/* set to 1 to enable confirmation prompts on New and Exit */
#ifndef COWARD_MODE
#define COWARD_MODE      0
#endif

/* set to 1 to enable non-farbfeld export (PNG/JPEG/BMP/TGA via stb_image_write) */
#ifndef SUCK_MORE
#define SUCK_MORE        1
#endif

/* set to 1 to disable the text tool (e.g. no stb_truetype.h available) */
#ifndef NOTTF
#define NOTTF            0
#endif

/* minimum canvas_zoom level at which the grid overlay becomes visible */
#define GRID_MIN_ZOOM   4.0f
