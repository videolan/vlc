/*****************************************************************************
 * bluray.c: Blu-ray disc support plugin
 *****************************************************************************
 * Copyright © 2010-2011 VideoLAN, VLC authors and libbluray AUTHORS
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <limits.h>                         /* PATH_MAX */
#if defined (HAVE_MNTENT_H) && defined(HAVE_SYS_STAT_H)
#include <mntent.h>
#include <sys/stat.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>                      /* demux_t */
#include <vlc_input.h>                      /* Seekpoints, chapters */
#include <vlc_dialog.h>                     /* BD+/AACS warnings */
#include <vlc_vout.h>                       /* vout_PutSubpicture / subpicture_t */

#include <libbluray/bluray.h>
#include <libbluray/meta_data.h>
#include <libbluray/overlay.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define BD_MENU_TEXT        N_( "Bluray menus" )
#define BD_MENU_LONGTEXT    N_( "Use bluray menus. If disabled, "\
                                "the movie will start directly" )

/* Callbacks */
static int  blurayOpen ( vlc_object_t * );
static void blurayClose( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("BluRay") )
    set_description( N_("Blu-Ray Disc support (libbluray)") )

    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_capability( "access_demux", 200)
    add_bool( "bluray-menu", false, BD_MENU_TEXT, BD_MENU_LONGTEXT, false )

    add_shortcut( "bluray", "file" )

    set_callbacks( blurayOpen, blurayClose )
vlc_module_end ()

/* libbluray's overlay.h defines 2 types of overlay (bd_overlay_plane_e). */
#define MAX_OVERLAY 2

typedef enum OverlayStatus {
    Closed = 0,
    ToDisplay,  //Used to mark the overlay to be displayed the first time.
    Displayed,
    Outdated    //used to update the overlay after it has been sent to the vout
} OverlayStatus;

typedef struct bluray_overlay_t
{
    VLC_GC_MEMBERS

    vlc_mutex_t         lock;
    subpicture_t        *p_pic;
    OverlayStatus       status;
    subpicture_region_t *p_regions;
} bluray_overlay_t;

struct  demux_sys_t
{
    BLURAY              *bluray;

    /* Titles */
    unsigned int        i_title;
    unsigned int        i_longest_title;
    input_title_t       **pp_title;

    /* Menus */
    bluray_overlay_t    *p_overlays[MAX_OVERLAY];
    int                 current_overlay; // -1 if no current overlay;
    bool                b_menu;

    /* */
    input_thread_t      *p_input;
    vout_thread_t       *p_vout;

    /* TS stream */
    stream_t            *p_parser;
};

struct subpicture_updater_sys_t
{
    bluray_overlay_t    *p_overlay;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     blurayControl(demux_t *, int, va_list);
static int     blurayDemux(demux_t *);

static int     blurayInitTitles(demux_t *p_demux );
static int     bluraySetTitle(demux_t *p_demux, int i_title);

static void    blurayOverlayProc(void *ptr, const BD_OVERLAY * const overlay);

#define FROM_TICKS(a) (a*CLOCK_FREQ / INT64_C(90000))
#define TO_TICKS(a)   (a*INT64_C(90000)/CLOCK_FREQ)
#define CUR_LENGTH    p_sys->pp_title[p_demux->info.i_title]->i_length

/*****************************************************************************
 * blurayOpen: module init function
 *****************************************************************************/
static int blurayOpen( vlc_object_t *object )
{
    demux_t *p_demux = (demux_t*)object;
    demux_sys_t *p_sys;

    char *pos_title;
    int i_title = -1;
    char bd_path[PATH_MAX] = { '\0' };
    const char *error_msg = NULL;

    if (strcmp(p_demux->psz_access, "bluray")) {
        // TODO BDMV support, once we figure out what to do in libbluray
        return VLC_EGENERIC;
    }

    /* */
    p_demux->p_sys = p_sys = calloc(1, sizeof(*p_sys));
    if (unlikely(!p_sys)) {
        return VLC_ENOMEM;
    }
    p_sys->current_overlay = -1;

    /* init demux info fields */
    p_demux->info.i_update    = 0;
    p_demux->info.i_title     = 0;
    p_demux->info.i_seekpoint = 0;

    TAB_INIT( p_sys->i_title, p_sys->pp_title );

    /* store current bd_path */
    if (p_demux->psz_file) {
        strncpy(bd_path, p_demux->psz_file, sizeof(bd_path));
        bd_path[PATH_MAX - 1] = '\0';
    }

#if defined (HAVE_MNTENT_H) && defined (HAVE_SYS_STAT_H)
    /* If we're passed a block device, try to convert it to the mount point. */
    struct stat st;
    if ( !stat (bd_path, &st)) {
        if (S_ISBLK (st.st_mode)) {
            FILE* mtab = setmntent ("/proc/self/mounts", "r");
            struct mntent* m;
            struct mntent mbuf;
            char buf [8192];
            while ((m = getmntent_r (mtab, &mbuf, buf, sizeof(buf))) != NULL) {
                if (!strcmp (m->mnt_fsname, bd_path)) {
                    strncpy (bd_path, m->mnt_dir, sizeof(bd_path));
                    bd_path[sizeof(bd_path) - 1] = '\0';
                    break;
                }
            }
            endmntent (mtab);
        }
    }
#endif /* HAVE_MNTENT_H && HAVE_SYS_STAT_H */
    p_sys->bluray = bd_open(bd_path, NULL);
    if (!p_sys->bluray) {
        free(p_sys);
        return VLC_EGENERIC;
    }

    /* Warning the user about AACS/BD+ */
    const BLURAY_DISC_INFO *disc_info = bd_get_disc_info(p_sys->bluray);
    msg_Info(p_demux, "First play: %i, Top menu: %i\n"
                      "HDMV Titles: %i, BD-J Titles: %i, Other: %i",
             disc_info->first_play_supported, disc_info->top_menu_supported,
             disc_info->num_hdmv_titles, disc_info->num_bdj_titles,
             disc_info->num_unsupported_titles);

    /* AACS */
    if (disc_info->aacs_detected) {
        if (!disc_info->libaacs_detected) {
            error_msg = _("This Blu-Ray Disc needs a library for AACS decoding, "
                      "and your system does not have it.");
            goto error;
        }
        if (!disc_info->aacs_handled) {
            error_msg = _("Your system AACS decoding library does not work. "
                      "Missing keys?");
            goto error;
        }
    }

    /* BD+ */
    if (disc_info->bdplus_detected) {
        if (!disc_info->libbdplus_detected) {
            error_msg = _("This Blu-Ray Disc needs a library for BD+ decoding, "
                      "and your system does not have it.");
            goto error;
        }
        if (!disc_info->bdplus_handled) {
            error_msg = _("Your system BD+ decoding library does not work. "
                      "Missing configuration?");
            goto error;
        }
    }

    /* Get titles and chapters */
    if (blurayInitTitles(p_demux) != VLC_SUCCESS) {
        goto error;
    }

    /*
     * Initialize the event queue, so we can receive events in blurayDemux(Menu).
     */
    bd_get_event(p_sys->bluray, NULL);

    p_sys->b_menu = var_InheritBool( p_demux, "bluray-menu" );
    if ( p_sys->b_menu )
    {
        p_sys->p_input = demux_GetParentInput(p_demux);
        if (unlikely(!p_sys->p_input))
            goto error;

        /* libbluray will start playback from "First-Title" title */
        bd_play(p_sys->bluray);

        /* Registering overlay event handler */
        bd_register_overlay_proc(p_sys->bluray, p_demux, blurayOverlayProc);
    }
    else
    {
        /* get title request */
        if ((pos_title = strrchr(bd_path, ':'))) {
            /* found character ':' for title information */
            *(pos_title++) = '\0';
            i_title = atoi(pos_title);
        }

        /* set start title number */
        if (bluraySetTitle(p_demux, i_title) != VLC_SUCCESS) {
            msg_Err( p_demux, "Could not set the title %d", i_title );
            goto error;
        }
    }

    p_sys->p_parser   = stream_DemuxNew(p_demux, "ts", p_demux->out);
    if (!p_sys->p_parser) {
        msg_Err(p_demux, "Failed to create TS demuxer");
        goto error;
    }

    p_demux->pf_control = blurayControl;
    p_demux->pf_demux   = blurayDemux;

    return VLC_SUCCESS;

error:
    if (error_msg)
        dialog_Fatal(p_demux, _("Blu-Ray error"), "%s", error_msg);
    blurayClose(object);
    return VLC_EGENERIC;
}


/*****************************************************************************
 * blurayClose: module destroy function
 *****************************************************************************/
static void blurayClose( vlc_object_t *object )
{
    demux_t *p_demux = (demux_t*)object;
    demux_sys_t *p_sys = p_demux->p_sys;

    /*
     * Close libbluray first.
     * This will close all the overlays before we release p_vout
     * bd_close( NULL ) can crash
     */
    assert(p_sys->bluray);
    bd_close(p_sys->bluray);

    if (p_sys->p_vout != NULL)
        vlc_object_release(p_sys->p_vout);
    if (p_sys->p_input != NULL)
        vlc_object_release(p_sys->p_input);
    if (p_sys->p_parser)
        stream_Delete(p_sys->p_parser);

    /* Titles */
    for (unsigned int i = 0; i < p_sys->i_title; i++)
        vlc_input_title_Delete(p_sys->pp_title[i]);
    TAB_CLEAN( p_sys->i_title, p_sys->pp_title );

    free(p_sys);
}

/*****************************************************************************
 * subpicture_updater_t functions:
 *****************************************************************************/
static int subpictureUpdaterValidate( subpicture_t *p_subpic,
                                      bool b_fmt_src, const video_format_t *p_fmt_src,
                                      bool b_fmt_dst, const video_format_t *p_fmt_dst,
                                      mtime_t i_ts )
{
    VLC_UNUSED( b_fmt_src );
    VLC_UNUSED( b_fmt_dst );
    VLC_UNUSED( p_fmt_src );
    VLC_UNUSED( p_fmt_dst );
    VLC_UNUSED( i_ts );

    subpicture_updater_sys_t *p_upd_sys = p_subpic->updater.p_sys;
    bluray_overlay_t         *p_overlay = p_upd_sys->p_overlay;

    vlc_mutex_lock(&p_overlay->lock);
    int res = p_overlay->status == Outdated;
    vlc_mutex_unlock(&p_overlay->lock);
    return res;
}

/* This should probably be moved to subpictures.c afterward */
static subpicture_region_t* subpicture_region_Clone(subpicture_region_t *p_region_src)
{
    if (!p_region_src)
        return NULL;
    subpicture_region_t *p_region_dst = subpicture_region_New(&p_region_src->fmt);
    if (unlikely(!p_region_dst))
        return NULL;

    p_region_dst->i_x      = p_region_src->i_x;
    p_region_dst->i_y      = p_region_src->i_y;
    p_region_dst->i_align  = p_region_src->i_align;
    p_region_dst->i_alpha  = p_region_src->i_alpha;

    p_region_dst->psz_text = p_region_src->psz_text ? strdup(p_region_src->psz_text) : NULL;
    p_region_dst->psz_html = p_region_src->psz_html ? strdup(p_region_src->psz_html) : NULL;
    if (p_region_src->p_style != NULL) {
        p_region_dst->p_style = malloc(sizeof(*p_region_dst->p_style));
        p_region_dst->p_style = text_style_Copy(p_region_dst->p_style,
                                                p_region_src->p_style);
    }

    //Palette is already copied by subpicture_region_New, we just have to duplicate p_pixels
    for (int i = 0; i < p_region_src->p_picture->i_planes; i++)
        memcpy(p_region_dst->p_picture->p[i].p_pixels,
               p_region_src->p_picture->p[i].p_pixels,
               p_region_src->p_picture->p[i].i_lines * p_region_src->p_picture->p[i].i_pitch);
    return p_region_dst;
}

static void subpictureUpdaterUpdate(subpicture_t *p_subpic,
                                    const video_format_t *p_fmt_src,
                                    const video_format_t *p_fmt_dst,
                                    mtime_t i_ts)
{
    VLC_UNUSED(p_fmt_src);
    VLC_UNUSED(p_fmt_dst);
    VLC_UNUSED(i_ts);
    subpicture_updater_sys_t *p_upd_sys = p_subpic->updater.p_sys;
    bluray_overlay_t         *p_overlay = p_upd_sys->p_overlay;

    /*
     * When this function is called, all p_subpic regions are gone.
     * We need to duplicate our regions (stored internaly) to this subpic.
     */
    vlc_mutex_lock(&p_overlay->lock);

    subpicture_region_t *p_src = p_overlay->p_regions;
    if (!p_src)
        return;

    subpicture_region_t **p_dst = &(p_subpic->p_region);
    while (p_src != NULL) {
        *p_dst = subpicture_region_Clone(p_src);
        if (*p_dst == NULL)
            break ;
        p_dst = &((*p_dst)->p_next);
        p_src = p_src->p_next;
    }
    if (*p_dst != NULL)
        (*p_dst)->p_next = NULL;
    p_overlay->status = Displayed;
    vlc_mutex_unlock(&p_overlay->lock);
}

static void subpictureUpdaterDestroy(subpicture_t *p_subpic)
{
    vlc_gc_decref(p_subpic->updater.p_sys->p_overlay);
}

/*****************************************************************************
 * libbluray overlay handling:
 *****************************************************************************/
static void blurayCleanOverayStruct(gc_object_t *p_gc)
{
    bluray_overlay_t *p_overlay = vlc_priv(p_gc, bluray_overlay_t);

    /*
     * This will be called when destroying the picture.
     * Don't delete it again from here!
     */
    vlc_mutex_destroy(&p_overlay->lock);
    subpicture_region_Delete(p_overlay->p_regions);
    free(p_overlay);
}

static void blurayCloseAllOverlays(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_demux->p_sys->current_overlay = -1;
    if (p_sys->p_vout != NULL) {
        for (int i = 0; i < 0; i++) {
            if (p_sys->p_overlays[i] != NULL) {
                vout_FlushSubpictureChannel(p_sys->p_vout,
                                            p_sys->p_overlays[i]->p_pic->i_channel);
                vlc_gc_decref(p_sys->p_overlays[i]);
                p_sys->p_overlays[i] = NULL;
            }
        }
        vlc_object_release(p_sys->p_vout);
        p_sys->p_vout = NULL;
    }
}

/*
 * Mark the overlay as "ToDisplay" status.
 * This will not send the overlay to the vout instantly, as the vout
 * may not be acquired (not acquirable) yet.
 * If is has already been acquired, the overlay has already been sent to it,
 * therefore, we only flag the overlay as "Outdated"
 */
static void blurayActivateOverlay(demux_t *p_demux, const BD_OVERLAY* const ov)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /*
     * If the overlay is already displayed, mark the picture as outdated.
     * We must NOT use vout_PutSubpicture if a picture is already displayed.
     */
    vlc_mutex_lock(&p_sys->p_overlays[ov->plane]->lock);
    if ((p_sys->p_overlays[ov->plane]->status == Displayed ||
            p_sys->p_overlays[ov->plane]->status == Outdated)
            && p_sys->p_vout) {
        p_sys->p_overlays[ov->plane]->status = Outdated;
        vlc_mutex_unlock(&p_sys->p_overlays[ov->plane]->lock);
        return ;
    }
    /*
     * Mark the overlay as available, but don't display it right now.
     * the blurayDemuxMenu will send it to vout, as it may be unavailable when
     * the overlay is computed
     */
    p_sys->current_overlay = ov->plane;
    p_sys->p_overlays[ov->plane]->status = ToDisplay;
    vlc_mutex_unlock(&p_sys->p_overlays[ov->plane]->lock);
}

static void blurayInitOverlay(demux_t *p_demux, const BD_OVERLAY* const ov)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    assert(p_sys->p_overlays[ov->plane] == NULL);

    p_sys->p_overlays[ov->plane] = calloc(1, sizeof(**p_sys->p_overlays));
    if (unlikely(!p_sys->p_overlays[ov->plane]))
        return;

    subpicture_updater_sys_t *p_upd_sys = malloc(sizeof(*p_upd_sys));
    if (unlikely(!p_upd_sys)) {
        free(p_sys->p_overlays[ov->plane]);
        p_sys->p_overlays[ov->plane] = NULL;
        return;
    }
    vlc_gc_init(p_sys->p_overlays[ov->plane], blurayCleanOverayStruct);
    /* Incrementing refcounter: vout + demux */
    vlc_gc_incref(p_sys->p_overlays[ov->plane]);

    p_upd_sys->p_overlay = p_sys->p_overlays[ov->plane];
    subpicture_updater_t updater = {
        .pf_validate = subpictureUpdaterValidate,
        .pf_update   = subpictureUpdaterUpdate,
        .pf_destroy  = subpictureUpdaterDestroy,
        .p_sys       = p_upd_sys,
    };
    p_sys->p_overlays[ov->plane]->p_pic = subpicture_New(&updater);
    p_sys->p_overlays[ov->plane]->p_pic->i_original_picture_width = ov->w;
    p_sys->p_overlays[ov->plane]->p_pic->i_original_picture_height = ov->h;
    p_sys->p_overlays[ov->plane]->p_pic->b_ephemer = true;
    p_sys->p_overlays[ov->plane]->p_pic->b_absolute = true;
}

/**
 * Destroy every regions in the subpicture.
 * This is done in two steps:
 * - Wiping our private regions list
 * - Flagging the overlay as outdated, so the changes are replicated from
 *   the subpicture_updater_t::pf_update
 * This doesn't destroy the subpicture, as the overlay may be used again by libbluray.
 */
static void blurayClearOverlay(demux_t *p_demux, const BD_OVERLAY* const ov)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    vlc_mutex_lock(&p_sys->p_overlays[ov->plane]->lock);

    subpicture_region_ChainDelete(p_sys->p_overlays[ov->plane]->p_regions);
    p_sys->p_overlays[ov->plane]->p_regions = NULL;
    p_sys->p_overlays[ov->plane]->status = Outdated;
    vlc_mutex_unlock(&p_sys->p_overlays[ov->plane]->lock);
}

/*
 * This will draw to the overlay by adding a region to our region list
 * This will have to be copied to the subpicture used to render the overlay.
 */
static void blurayDrawOverlay(demux_t *p_demux, const BD_OVERLAY* const ov)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /*
     * Compute a subpicture_region_t.
     * It will be copied and sent to the vout later.
     */
    if (!ov->img)
        return;

    vlc_mutex_lock(&p_sys->p_overlays[ov->plane]->lock);

    /* Find a region to update */
    subpicture_region_t *p_reg = p_sys->p_overlays[ov->plane]->p_regions;
    subpicture_region_t *p_last = NULL;
    while (p_reg != NULL) {
        p_last = p_reg;
        if (p_reg->i_x == ov->x && p_reg->i_y == ov->y &&
                p_reg->fmt.i_width == ov->w && p_reg->fmt.i_height == ov->h)
            break;
        p_reg = p_reg->p_next;
    }

    /* If there is no region to update, create a new one. */
    if (!p_reg) {
        video_format_t fmt;
        video_format_Init(&fmt, 0);
        video_format_Setup(&fmt, VLC_CODEC_YUVP, ov->w, ov->h, 1, 1);

        p_reg = subpicture_region_New(&fmt);
        p_reg->i_x = ov->x;
        p_reg->i_y = ov->y;
        /* Append it to our list. */
        if (p_last != NULL)
            p_last->p_next = p_reg;
        else /* If we don't have a last region, then our list empty */
            p_sys->p_overlays[ov->plane]->p_regions = p_reg;
    }

    /* Now we can update the region, regardless it's an update or an insert */
    const BD_PG_RLE_ELEM *img = ov->img;
    for (int y = 0; y < ov->h; y++) {
        for (int x = 0; x < ov->w;) {
            memset(p_reg->p_picture->p[0].p_pixels +
                   y * p_reg->p_picture->p[0].i_pitch + x,
                   img->color, img->len);
            x += img->len;
            img++;
        }
    }
    if (ov->palette) {
        p_reg->fmt.p_palette->i_entries = 256;
        for (int i = 0; i < 256; ++i) {
            p_reg->fmt.p_palette->palette[i][0] = ov->palette[i].Y;
            p_reg->fmt.p_palette->palette[i][1] = ov->palette[i].Cb;
            p_reg->fmt.p_palette->palette[i][2] = ov->palette[i].Cr;
            p_reg->fmt.p_palette->palette[i][3] = ov->palette[i].T;
        }
    }
    vlc_mutex_unlock(&p_sys->p_overlays[ov->plane]->lock);
    /*
     * /!\ The region is now stored in our internal list, but not in the subpicture /!\
     */
}

static void blurayOverlayProc(void *ptr, const BD_OVERLAY *const overlay)
{
    demux_t *p_demux = (demux_t*)ptr;

    if (!overlay) {
        msg_Info(p_demux, "Closing overlay.");
        blurayCloseAllOverlays(p_demux);
        return;
    }
    switch (overlay->cmd) {
        case BD_OVERLAY_INIT:
            msg_Info(p_demux, "Initializing overlay");
            blurayInitOverlay(p_demux, overlay);
            break;
        case BD_OVERLAY_CLEAR:
            blurayClearOverlay(p_demux, overlay);
            break;
        case BD_OVERLAY_FLUSH:
            blurayActivateOverlay(p_demux, overlay);
            break;
        case BD_OVERLAY_DRAW:
            blurayDrawOverlay(p_demux, overlay);
            break;
        default:
            msg_Warn(p_demux, "Unknown BD overlay command: %u", overlay->cmd);
            break;
    }
}

static void bluraySendOverlayToVout(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    assert(p_sys->current_overlay >= 0 &&
           p_sys->p_overlays[p_sys->current_overlay] != NULL &&
           p_sys->p_overlays[p_sys->current_overlay]->p_pic != NULL);

    p_sys->p_overlays[p_sys->current_overlay]->p_pic->i_start =
        p_sys->p_overlays[p_sys->current_overlay]->p_pic->i_stop = mdate();
    p_sys->p_overlays[p_sys->current_overlay]->p_pic->i_channel =
        vout_RegisterSubpictureChannel(p_sys->p_vout);
    /*
     * After this point, the picture should not be accessed from the demux thread,
     * as it's hold by the vout thread.
     * This must be done only once per subpicture, ie. only once between each
     * blurayInitOverlay & blurayCloseOverlay call.
     */
    vout_PutSubpicture(p_sys->p_vout, p_sys->p_overlays[p_sys->current_overlay]->p_pic);
    /*
     * Mark the picture as Outdated, as it contains no region for now.
     * This will make the subpicture_updater_t call pf_update
     */
    p_sys->p_overlays[p_sys->current_overlay]->status = Outdated;
}

static int blurayInitTitles(demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* get and set the titles */
    unsigned i_title = bd_get_titles(p_sys->bluray, TITLES_RELEVANT, 60);
    int64_t duration = 0;

    for (unsigned int i = 0; i < i_title; i++) {
        input_title_t *t = vlc_input_title_New();
        if (!t)
            break;

        BLURAY_TITLE_INFO *title_info = bd_get_title_info(p_sys->bluray, i, 0);
        if (!title_info)
            break;
        t->i_length = FROM_TICKS(title_info->duration);

        if (t->i_length > duration) {
            duration = t->i_length;
            p_sys->i_longest_title = i;
        }

        for ( unsigned int j = 0; j < title_info->chapter_count; j++) {
            seekpoint_t *s = vlc_seekpoint_New();
            if (!s)
                break;
            s->i_time_offset = title_info->chapters[j].offset;

            TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
        }
        TAB_APPEND( p_sys->i_title, p_sys->pp_title, t );
        bd_free_title_info(title_info);
    }
    return VLC_SUCCESS;
}

static void blurayResetParser( demux_t *p_demux )
{
    /*
     * This is a hack and will have to be removed.
     * The parser should be flushed, and not destroy/created each time
     * we are changing title.
     */
    demux_sys_t *p_sys = p_demux->p_sys;
    if (!p_sys->p_parser)
        return;
    stream_Delete(p_sys->p_parser);
    p_sys->p_parser = stream_DemuxNew(p_demux, "ts", p_demux->out);
    if (!p_sys->p_parser) {
        msg_Err(p_demux, "Failed to create TS demuxer");
    }
}

static void blurayUpdateTitle( demux_t *p_demux, int i_title )
{
    blurayResetParser(p_demux);
    if (i_title >= p_demux->p_sys->i_title)
        return;

    /* read title info and init some values */
    p_demux->info.i_title = i_title;
    p_demux->info.i_seekpoint = 0;
    p_demux->info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
}

/*****************************************************************************
 * bluraySetTitle: select new BD title
 *****************************************************************************/
static int bluraySetTitle(demux_t *p_demux, int i_title)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Looking for the main title, ie the longest duration */
    if (i_title < 0)
        i_title = p_sys->i_longest_title;
    else if ((unsigned)i_title > p_sys->i_title)
        return VLC_EGENERIC;

    msg_Dbg( p_demux, "Selecting Title %i", i_title);

    /* Select Blu-Ray title */
    if (bd_select_title(p_demux->p_sys->bluray, i_title) == 0 ) {
        msg_Err(p_demux, "cannot select bd title '%d'", p_demux->info.i_title);
        return VLC_EGENERIC;
    }
    blurayUpdateTitle( p_demux, i_title );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * blurayControl: handle the controls
 *****************************************************************************/
static int blurayControl(demux_t *p_demux, int query, va_list args)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool     *pb_bool;
    int64_t  *pi_64;

    switch (query) {
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
             pb_bool = (bool*)va_arg( args, bool * );
             *pb_bool = true;
             break;

        case DEMUX_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 =
                INT64_C(1000) * var_InheritInteger( p_demux, "disc-caching" );
            break;

        case DEMUX_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case DEMUX_SET_TITLE:
        {
            int i_title = (int)va_arg( args, int );
            if (bluraySetTitle(p_demux, i_title) != VLC_SUCCESS)
                return VLC_EGENERIC;
            break;
        }
        case DEMUX_SET_SEEKPOINT:
        {
            int i_chapter = (int)va_arg( args, int );
            bd_seek_chapter( p_sys->bluray, i_chapter );
            p_demux->info.i_update = INPUT_UPDATE_SEEKPOINT;
            break;
        }

        case DEMUX_GET_TITLE_INFO:
        {
            input_title_t ***ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            int *pi_int             = (int*)va_arg( args, int* );
            int *pi_title_offset    = (int*)va_arg( args, int* );
            int *pi_chapter_offset  = (int*)va_arg( args, int* );

            /* */
            *pi_title_offset   = 0;
            *pi_chapter_offset = 0;

            /* Duplicate local title infos */
            *pi_int = p_sys->i_title;
            *ppp_title = calloc( p_sys->i_title, sizeof(input_title_t **) );
            for( unsigned int i = 0; i < p_sys->i_title; i++ )
                (*ppp_title)[i] = vlc_input_title_Duplicate( p_sys->pp_title[i]);

            return VLC_SUCCESS;
        }

        case DEMUX_GET_LENGTH:
        {
            int64_t *pi_length = (int64_t*)va_arg(args, int64_t *);
            *pi_length = p_demux->info.i_title < p_sys->i_title ? CUR_LENGTH : 0;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_TIME:
        {
            int64_t i_time = (int64_t)va_arg(args, int64_t);
            bd_seek_time(p_sys->bluray, TO_TICKS(i_time));
            return VLC_SUCCESS;
        }
        case DEMUX_GET_TIME:
        {
            int64_t *pi_time = (int64_t*)va_arg(args, int64_t *);
            *pi_time = (int64_t)FROM_TICKS(bd_tell_time(p_sys->bluray));
            return VLC_SUCCESS;
        }

        case DEMUX_GET_POSITION:
        {
            double *pf_position = (double*)va_arg( args, double * );
            *pf_position = p_demux->info.i_title < p_sys->i_title ?
                        (double)FROM_TICKS(bd_tell_time(p_sys->bluray))/CUR_LENGTH : 0.0;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_POSITION:
        {
            double f_position = (double)va_arg(args, double);
            bd_seek_time(p_sys->bluray, TO_TICKS(f_position*CUR_LENGTH));
            return VLC_SUCCESS;
        }

        case DEMUX_GET_META:
        {
            const struct meta_dl *meta = bd_get_meta(p_sys->bluray);
            if(!meta)
                return VLC_EGENERIC;

            vlc_meta_t *p_meta = (vlc_meta_t *) va_arg (args, vlc_meta_t*);

            if (!EMPTY_STR(meta->di_name)) vlc_meta_SetTitle(p_meta, meta->di_name);

            if (!EMPTY_STR(meta->language_code)) vlc_meta_AddExtra(p_meta, "Language", meta->language_code);
            if (!EMPTY_STR(meta->filename)) vlc_meta_AddExtra(p_meta, "Filename", meta->filename);
            if (!EMPTY_STR(meta->di_alternative)) vlc_meta_AddExtra(p_meta, "Alternative", meta->di_alternative);

            // if (meta->di_set_number > 0) vlc_meta_SetTrackNum(p_meta, meta->di_set_number);
            // if (meta->di_num_sets > 0) vlc_meta_AddExtra(p_meta, "Discs numbers in Set", meta->di_num_sets);

            if (meta->thumb_count > 0 && meta->thumbnails) {
                vlc_meta_SetArtURL(p_meta, meta->thumbnails[0].path);
            }

            return VLC_SUCCESS;
        }

        case DEMUX_CAN_RECORD:
        case DEMUX_GET_FPS:
        case DEMUX_SET_GROUP:
        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_GET_ATTACHMENTS:
            return VLC_EGENERIC;
        default:
            msg_Warn( p_demux, "unimplemented query (%d) in control", query );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void blurayHandleEvent( demux_t *p_demux, const BD_EVENT *e )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    switch (e->event)
    {
        case BD_EVENT_TITLE:
            blurayUpdateTitle( p_demux, e->param );
            break;
        case BD_EVENT_PLAYITEM:
            break;
        case BD_EVENT_AUDIO_STREAM:
            break;
        case BD_EVENT_CHAPTER:
            p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
            p_demux->info.i_seekpoint = 0;
            break;
        case BD_EVENT_ANGLE:
        case BD_EVENT_IG_STREAM:
        default:
            msg_Warn( p_demux, "event: %d param: %d", e->event, e->param );
            break;
    }
}

static void blurayHandleEvents( demux_t *p_demux )
{
    BD_EVENT e;

    while (bd_get_event(p_demux->p_sys->bluray, &e))
    {
        blurayHandleEvent(p_demux, &e);
    }
}

#define BD_TS_PACKET_SIZE (192)
#define NB_TS_PACKETS (200)

static int blurayDemux(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    block_t *p_block = block_New(p_demux, NB_TS_PACKETS * (int64_t)BD_TS_PACKET_SIZE);
    if (!p_block) {
        return -1;
    }

    int nread = -1;
    if (p_sys->b_menu == false) {
        blurayHandleEvents(p_demux);
        nread = bd_read(p_sys->bluray, p_block->p_buffer,
                NB_TS_PACKETS * BD_TS_PACKET_SIZE);
        if (nread < 0) {
            block_Release(p_block);
            return nread;
        }
    }
    else {
        BD_EVENT e;
        nread = bd_read_ext( p_sys->bluray, p_block->p_buffer,
                NB_TS_PACKETS * BD_TS_PACKET_SIZE, &e );
        if ( nread == 0 ) {
            if ( e.event == BD_EVENT_NONE )
                msg_Info( p_demux, "We reached the end of a title" );
            else
                blurayHandleEvent( p_demux, &e );
            block_Release(p_block);
            return 1;
        }
        if (p_sys->current_overlay != -1)
        {
            vlc_mutex_lock(&p_sys->p_overlays[p_sys->current_overlay]->lock);
            if (p_sys->p_overlays[p_sys->current_overlay]->status == ToDisplay) {
                vlc_mutex_unlock(&p_sys->p_overlays[p_sys->current_overlay]->lock);
                if (p_sys->p_vout == NULL)
                    p_sys->p_vout = input_GetVout(p_sys->p_input);
                if (p_sys->p_vout != NULL) {
                    bluraySendOverlayToVout(p_demux);
                }
            } else
                vlc_mutex_unlock(&p_sys->p_overlays[p_sys->current_overlay]->lock);
        }
    }

    p_block->i_buffer = nread;

    stream_DemuxSend( p_sys->p_parser, p_block );

    return 1;
}
