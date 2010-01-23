/*****************************************************************************
 * snapshot.c : snapshot plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <oaubert@lisi.univ-lyon1.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * This module is a pseudo video output that offers the possibility to
 * keep a cache of low-res snapshots.
 * The snapshot structure is defined in include/snapshot.h
 * In order to access the current snapshot cache, object variables are used:
 *   vout-snapshot-list-pointer : the pointer on the first element in the list
 *   vout-snapshot-datasize     : size of a snapshot
 *                           (also available in snapshot_t->i_datasize)
 *   vout-snapshot-cache-size   : size of the cache list
 *
 * It is used for the moment by the CORBA module and a specialized
 * python-vlc binding.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include <vlc_input.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define WIDTH_TEXT N_("Snapshot width")
#define WIDTH_LONGTEXT N_("Width of the snapshot image.")

#define HEIGHT_TEXT N_("Snapshot height")
#define HEIGHT_LONGTEXT N_("Height of the snapshot image.")

#define CHROMA_TEXT N_("Chroma")
#define CHROMA_LONGTEXT N_("Output chroma for the snapshot image " \
                            "(a 4 character string, like \"RV32\").")

#define CACHE_TEXT N_("Cache size (number of images)")
#define CACHE_LONGTEXT N_("Snapshot cache size (number of images to keep).")

static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_description(N_("Snapshot output"))
    set_shortname(N_("Snapshot"))

    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 0)

    add_integer("vout-snapshot-width", 320, NULL, WIDTH_TEXT, WIDTH_LONGTEXT, false)
    add_integer("vout-snapshot-height", 200, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT, false)
    add_string("vout-snapshot-chroma", "RV32", NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true)
        add_deprecated_alias("snapshot-chroma")
    add_integer("vout-snapshot-cache-size", 50, NULL, CACHE_TEXT, CACHE_LONGTEXT, true)
        add_deprecated_alias("snapshot-cache-size")

    set_callbacks(Open, Close)
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Display(vout_display_t *, picture_t *);
static int            Control(vout_display_t *, int, va_list);
static void           Manage (vout_display_t *);

typedef struct {
  mtime_t date;         /* Presentation time */
  int     width;          /* In pixels */
  int     height;         /* In pixels */
  int     data_size;    /* In bytes */
  uint8_t *data;        /* Data area */
} snapshot_t;

struct vout_display_sys_t {
    int            count;  /* Size of the cache */
    snapshot_t     **snapshot;      /* List of available snapshots */
    int            index;           /* Index of the next available list member */
    int            data_size;       /* Size of an image */
    picture_pool_t *pool;

    input_thread_t *input;          /* The input thread */
};

/* */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    /* Allocate instance and initialize some members */
    vd->sys = sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    char *chroma_fmt = var_InheritString(vd, "vout-snapshot-chroma");
    const vlc_fourcc_t chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, chroma_fmt);
    free(chroma_fmt);

    if (!chroma) {
        msg_Err(vd, "snapshot-chroma should be 4 characters long");
        free(sys);
        return VLC_EGENERIC;
    }

    const int width  = var_InheritInteger(vd, "vout-snapshot-width");
    const int height = var_InheritInteger(vd, "vout-snapshot-height");
    if (width <= 0 || height <= 0) {
        msg_Err(vd, "snapshot-width/height are invalid");
        free(sys);
        return VLC_EGENERIC;
    }

    /* */
    video_format_t fmt = vd->fmt;
    fmt.i_chroma = chroma;
    fmt.i_width  = width;
    fmt.i_height = height;
    fmt.i_rmask  = 0;
    fmt.i_gmask  = 0;
    fmt.i_bmask  = 0;
    video_format_FixRgb(&fmt);

    picture_t *picture = picture_NewFromFormat(&fmt);
    if (!picture) {
        free(sys);
        return VLC_EGENERIC;
    }
    sys->pool = picture_pool_New(1, &picture);
    if (!sys->pool) {
        picture_Release(picture);
        free(sys);
        return VLC_EGENERIC;
    }
    sys->data_size = 0;
    for (int i = 0; i < picture->i_planes; i++) {
        const plane_t *plane = &picture->p[i];
        sys->data_size += plane->i_visible_pitch *
                          plane->i_visible_lines *
                          plane->i_pixel_pitch;
    }

    sys->index = 0;
    sys->count = var_InheritInteger(vd, "vout-snapshot-cache-size");

    /* FIXME following code leaks in case of error */

    if (sys->count < 2) {
        msg_Err(vd, "vout-snapshot-cache-size must be at least 1.");
        return VLC_EGENERIC;
    }

    sys->snapshot = calloc(sys->count, sizeof(*sys->snapshot));

    if (!sys->snapshot)
        return VLC_ENOMEM;

    /* Initialize the structures for the circular buffer */
    for (int index = 0; index < sys->count; index++) {
        snapshot_t *snapshot = malloc(sizeof(*snapshot));

        if (!snapshot)
            return VLC_ENOMEM;

        snapshot->date      = VLC_TS_INVALID;
        snapshot->width     = fmt.i_width;
        snapshot->height    = fmt.i_height;
        snapshot->data_size = sys->data_size;
        snapshot->data      = malloc(sys->data_size);
        if (!snapshot->data) {
            free(snapshot);
            return VLC_ENOMEM;
        }
        sys->snapshot[index] = snapshot;
    }

    /* */
    var_Create(vd, "vout-snapshot-width", VLC_VAR_INTEGER);
    var_Create(vd, "vout-snapshot-height", VLC_VAR_INTEGER);
    var_Create(vd, "vout-snapshot-datasize", VLC_VAR_INTEGER);
    var_Create(vd, "vout-snapshot-cache-size", VLC_VAR_INTEGER);
    var_Create(vd, "vout-snapshot-list-pointer", VLC_VAR_ADDRESS);

    var_SetInteger(vd, "vout-snapshot-width", fmt.i_width);
    var_SetInteger(vd, "vout-snapshot-height", fmt.i_height);
    var_SetInteger(vd, "vout-snapshot-datasize", sys->data_size);
    var_SetInteger(vd, "vout-snapshot-cache-size", sys->count);
    var_SetAddress(vd, "vout-snapshot-list-pointer", sys->snapshot);

    /* Get the p_input pointer (to access video times) */
    sys->input = vlc_object_find(vd, VLC_OBJECT_INPUT, FIND_PARENT);
    if (!sys->input)
        return VLC_ENOOBJ;

    if (var_Create(sys->input, "vout-snapshot-id", VLC_VAR_ADDRESS)) {
        msg_Err(vd, "Cannot create vout-snapshot-id variable in p_input(%p).",
                 sys->input);
        return VLC_EGENERIC;
    }

    /* Register the snapshot vout module at the input level */
    if (var_SetAddress(sys->input, "vout-snapshot-id", vd)) {
        msg_Err(vd, "Cannot register vout-snapshot-id in p_input(%p).",
                 sys->input);
        return VLC_EGENERIC;
    }

    /* */
    vout_display_info_t info = vd->info;
    info.has_hide_mouse = true;

    /* */
    vd->fmt     = fmt;
    vd->info    = info;
    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = Manage;

    /* */
    vout_display_SendEventFullscreen(vd, false);
    return VLC_SUCCESS;
}

/* */
static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    var_Destroy(sys->input, "vout-snapshot-id");

    vlc_object_release(sys->input);
    var_Destroy(vd, "vout-snapshot-width");
    var_Destroy(vd, "vout-snapshot-height");
    var_Destroy(vd, "vout-snapshot-datasize");

    for (int index = 0 ; index < sys->count; index++) {
        free(sys->snapshot[index]->data);
        free(sys->snapshot[index]);
    }
    free(sys->snapshot);

    picture_pool_Delete(sys->pool);

    free(sys);
}

/*****************************************************************************
 *
 *****************************************************************************/
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    VLC_UNUSED(count);
    return vd->sys->pool;
}

/* Return the position in ms from the start of the movie */
static mtime_t snapshot_GetMovietime(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->input)
        return VLC_TS_INVALID;
    return var_GetTime(sys->input, "time") / 1000;
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    const int index = sys->index;

    /* FIXME a lock or an event of some sort would be needed to protect
     * the content of sys->snapshot vs external users */
    uint8_t *p_dst = sys->snapshot[index]->data;
    for (int i = 0; i < picture->i_planes; i++) {
        const plane_t *plane = &picture->p[i];
        for( int y = 0; y < plane->i_visible_lines; y++) {
            vlc_memcpy(p_dst, &plane->p_pixels[y*plane->i_pitch],
                       plane->i_visible_pitch);
            p_dst += plane->i_visible_pitch;
        }
    }
    sys->snapshot[index]->date = snapshot_GetMovietime(vd);

    sys->index = (index + 1) % sys->count;
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(vd);
    switch (query) {
    case VOUT_DISPLAY_CHANGE_FULLSCREEN: {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);
        if (cfg->is_fullscreen)
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }
    default:
        return VLC_EGENERIC;
    }
}

static void Manage (vout_display_t *vd)
{
    VLC_UNUSED(vd);
}

