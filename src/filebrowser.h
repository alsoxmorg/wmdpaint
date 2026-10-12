/* filebrowser.h — minimal POSIX directory navigator for Open/Save dialogs.
 *
 * Usage:
 *   fb_init(FB_MODE_OPEN, 0)   — open browser in open mode (all files)
 *   fb_init(FB_MODE_SAVE, 1)   — open browser in save mode (.ff only)
 *   fb_result(buf, sizeof buf) — copy selected full path into buf
 *   fb_open                    — 1 while the browser is showing
 */
#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FB_MAX_ENTRIES 1024
#define FB_MODE_OPEN   0
#define FB_MODE_SAVE   1

/* what a FB_MODE_SAVE dialog does on confirm (interpreted by the caller) */
#define FB_SAVE_NATIVE 0   /* Save As: farbfeld / 1fpirate by extension */
#define FB_SAVE_IMAGE  1   /* Export:  png/jpg/... via stb_image_write   */
#define FB_SAVE_1FP    2   /* Export as 1fpirate (dithered)             */
#define FB_SAVE_ARCHIVE 3  /* Layered .wmd.bz2 archive                  */

static char fb_cwd[4096]          = "";
static char fb_dirs [FB_MAX_ENTRIES][256];
static char fb_files[FB_MAX_ENTRIES][256];
static int  fb_ndirs   = 0;
static int  fb_nfiles  = 0;
static char fb_sel[256]      = "";   /* highlighted file name */
static char fb_savename[256] = "";   /* editable name in save mode */
static int  fb_open    = 0;
static int  fb_mode    = FB_MODE_OPEN;
static int  fb_save_kind = FB_SAVE_NATIVE; /* meaning of a save confirm */
static int  fb_ff_only = 0;          /* restrict file list to .ff */
static int  fb_show_hidden = 0;      /* show dotfiles when set */

/* $HOME, or "/" if the environment doesn't define it */
static const char *
fb_home(void)
{
    const char *h = getenv("HOME");
    return (h && h[0]) ? h : "/";
}

static int
fb_cmp256(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

static void
fb_scan(void)
{
    DIR           *d;
    struct dirent *ent;

    fb_ndirs = fb_nfiles = 0;
    fb_sel[0] = '\0';

    d = opendir(fb_cwd);
    if (!d) return;
    while ((ent = readdir(d)) != NULL) {
        const char *nm = ent->d_name;
        if (strcmp(nm, ".") == 0) continue;
        /* hide dotfiles unless asked; ".." stays so you can navigate up */
        if (!fb_show_hidden && nm[0] == '.' && strcmp(nm, "..") != 0)
            continue;
        if (ent->d_type == DT_DIR) {
            if (fb_ndirs < FB_MAX_ENTRIES)
                snprintf(fb_dirs[fb_ndirs++], 256, "%s", nm);
        } else {
            size_t l = strlen(nm);
            if (fb_ff_only && (l < 3 || strcmp(nm + l - 3, ".ff") != 0))
                continue;
            if (fb_nfiles < FB_MAX_ENTRIES)
                snprintf(fb_files[fb_nfiles++], 256, "%s", nm);
        }
    }
    closedir(d);
    qsort(fb_dirs,  (size_t)fb_ndirs,  sizeof fb_dirs[0],  fb_cmp256);
    qsort(fb_files, (size_t)fb_nfiles, sizeof fb_files[0], fb_cmp256);
}

static void
fb_navigate(const char *subdir)
{
    if (strcmp(subdir, "..") == 0) {
        size_t len = strlen(fb_cwd);
        if (len > 1) {
            char *p = fb_cwd + len - 1;
            while (p > fb_cwd && *p != '/') p--;
            if (p == fb_cwd) p[1] = '\0';
            else             p[0] = '\0';
        }
    } else {
        size_t cwdlen = strlen(fb_cwd);
        size_t sublen = strlen(subdir);
        if (cwdlen + 1 + sublen < sizeof fb_cwd) {
            fb_cwd[cwdlen] = '/';
            memcpy(fb_cwd + cwdlen + 1, subdir, sublen + 1);
        }
    }
    fb_scan();
}

/* jump straight to an absolute path (used by the sidebar shortcuts).
 * The directory must exist and be readable, otherwise cwd is left unchanged. */
static void
fb_goto(const char *abspath)
{
    DIR   *d;
    char   tmp[4096];
    size_t l;

    if (!abspath || !abspath[0]) return;
    snprintf(tmp, sizeof tmp, "%s", abspath);
    l = strlen(tmp);
    while (l > 1 && tmp[l - 1] == '/') tmp[--l] = '\0';

    d = opendir(tmp);
    if (!d) return;
    closedir(d);

    snprintf(fb_cwd, sizeof fb_cwd, "%s", tmp);
    fb_scan();
}

/* jump to $HOME, or a sub-directory of it (e.g. "Desktop", "Downloads").
 * Pass NULL/"" for HOME itself. */
static void
fb_goto_home_sub(const char *sub)
{
    char p[4096];
    if (sub && sub[0]) snprintf(p, sizeof p, "%s/%s", fb_home(), sub);
    else               snprintf(p, sizeof p, "%s", fb_home());
    fb_goto(p);
}

/* open the browser, seeding cwd from hint_path (or cwd if NULL) */
static void
fb_init(int mode, int ff_only, const char *hint_path)
{
    fb_mode    = mode;
    fb_save_kind = FB_SAVE_NATIVE;
    fb_ff_only = ff_only;
    fb_sel[0]  = '\0';
    fb_savename[0] = '\0';
    fb_open    = 1;

    if (hint_path && hint_path[0]) {
        /* seed to the directory containing the hinted file */
        const char *sl = strrchr(hint_path, '/');
        if (sl && sl != hint_path) {
            size_t len = (size_t)(sl - hint_path);
            if (len < sizeof fb_cwd) {
                memcpy(fb_cwd, hint_path, len);
                fb_cwd[len] = '\0';
            }
        }
        /* prefill the editable name with the hinted file's basename */
        if (mode == FB_MODE_SAVE) {
            const char *base = sl ? sl + 1 : hint_path;
            snprintf(fb_savename, sizeof fb_savename, "%.255s", base);
        }
    }
    if (!fb_cwd[0] && !getcwd(fb_cwd, sizeof fb_cwd))
        snprintf(fb_cwd, sizeof fb_cwd, "/");

    fb_scan();
}

/* build the full path of the confirmed selection into out */
static void
fb_result(char *out, size_t outsz)
{
    const char *name = (fb_mode == FB_MODE_SAVE) ? fb_savename : fb_sel;
    if (!name[0]) { out[0] = '\0'; return; }
    /* auto-append .ff when in save/ff_only mode and extension is missing */
    if (fb_ff_only && fb_mode == FB_MODE_SAVE) {
        size_t l = strlen(name);
        if (l < 3 || strcmp(name + l - 3, ".ff") != 0)
            snprintf(out, outsz, "%s/%s.ff", fb_cwd, name);
        else
            snprintf(out, outsz, "%s/%s", fb_cwd, name);
    } else {
        snprintf(out, outsz, "%s/%s", fb_cwd, name);
    }
}

#endif /* FILEBROWSER_H */
