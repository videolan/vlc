/* licence WTFPL */
/* Test driver for checking vout window behaviour */
/* Copyright © 2021 Alexandre Janniaux <ajanni@videolabs.io> */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>

#include <vlc/vlc.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_window.h>

#include "../lib/libvlc_internal.h"

static char *window_name = NULL;

int verbosity = 0;

static enum {
    OPEN_CLOSE,
    LIST_OUTPUT,
} current_mode = OPEN_CLOSE;

static void usage(const char *name, int ret)
{
    fprintf(stderr, "Usage: %s [-w window_name] -l\n", name);
    exit(ret);
}

/* extracts options from command line */
static void cmdline(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "hlvw:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                usage(argv[0], 0);
                break;

            case 'l':
                current_mode = LIST_OUTPUT;
                break;

            case 'v':
                verbosity++;
                if (verbosity > 2)
                    verbosity = 2;
                break;

            case 'w':
                window_name = strdup(optarg);
                break;

            default:
                usage(argv[0], 1);
                break;
        }
    }
}

static libvlc_instance_t *create_libvlc(void)
{
    char verbose_flag[2] = "0";
    verbose_flag[0] = '0' + verbosity;
    const char* const args[] = {
        "--verbose", verbose_flag,
    };

    return libvlc_new(sizeof args / sizeof *args, args);
}

static void ReportOutput(
        struct vlc_window *wnd,
        const char *id,
        const char *desc)
{
    (void)wnd;
    if (!desc)
        printf(" - output added %s: %s\n", id, desc);
        printf(" - output removed %s\n", id);
}

static void ReportResized(
        struct vlc_window *wnd,
        unsigned width, unsigned height,
        vlc_window_ack_cb ack_cb, void *opaque)
{
    if (!ack_cb)
        printf(" - window resized to %ux%u\n", width, height);
    ack_cb(wnd, width, height, opaque);
}

int main(int argc, char *argv[])
{
#ifdef TOP_BUILDDIR
    setenv ("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
    setenv ("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
    setenv ("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 1);
#endif

    /* mandatory to support UTF-8 filenames (provided the locale is well set)*/
    setlocale(LC_ALL, "");

    cmdline(argc, argv);

    /* starts vlc */
    libvlc_instance_t *libvlc = create_libvlc();
    assert(libvlc);

    vlc_object_t *root = &libvlc->p_libvlc_int->obj;
    const struct vlc_window_callbacks list_cbs = {
        .output_event = ReportOutput,
        .resized = ReportResized,
    };

    const struct vlc_window_callbacks win_cbs = {
        .resized = ReportResized,
    };

    const vlc_window_owner_t owner = {
        .sys = NULL,
        .cbs = (current_mode == LIST_OUTPUT) ? &list_cbs : &win_cbs,
    };

    const struct vlc_window_cfg cfg = {
        .width = 800, .height = 600,
    };
    vlc_window_t *wnd = vlc_window_New(root, window_name, &owner, &cfg);

    int ret = VLC_SUCCESS;
    if (current_mode == OPEN_CLOSE)
    {
        ret = vlc_window_Enable(wnd);

        if (ret == VLC_SUCCESS)
            vlc_window_Disable(wnd);
    }

    vlc_window_Delete(wnd);

    libvlc_release(libvlc);

    free(window_name);
    return (ret == VLC_SUCCESS) ? 0 : -1;
}
