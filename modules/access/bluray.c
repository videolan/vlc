/*****************************************************************************
 * bluray.c: Blu-ray disc support plugin
 *****************************************************************************
 * Copyright © 2010-2012 VideoLAN, VLC authors and libbluray AUTHORS
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *          Hugo Beauzée-Luyssen <hugo@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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

#ifdef __APPLE__
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>                      /* demux_t */
#include <vlc_input.h>                      /* Seekpoints, chapters */
#include <vlc_atomic.h>
#include <vlc_dialog.h>                     /* BD+/AACS warnings */
#include <vlc_vout.h>                       /* vout_PutSubpicture / subpicture_t */
#include <vlc_url.h>                        /* vlc_path2uri */

#include <libbluray/bluray.h>
#include <libbluray/keys.h>
#include <libbluray/meta_data.h>
#include <libbluray/overlay.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define BD_MENU_TEXT        N_( "Blu-ray menus" )
#define BD_MENU_LONGTEXT    N_( "Use Blu-ray menus. If disabled, "\
                                "the movie will start directly" )

/* Callbacks */
static int  blurayOpen ( vlc_object_t * );
static void blurayClose( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Blu-ray") )
    set_description( N_("Blu-ray Disc support (libbluray)") )

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
    atomic_flag         released_once;
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
    unsigned int        i_current_clip;
    input_title_t       **pp_title;

    /* Meta informations */
    const META_DL       *p_meta;

    /* Menus */
    bluray_overlay_t    *p_overlays[MAX_OVERLAY];
    int                 current_overlay; // -1 if no current overlay;
    bool                b_menu;

    /* */
    input_thread_t      *p_input;
    vout_thread_t       *p_vout;

    /* TS stream */
    es_out_t            *p_out;
    vlc_array_t         es;
    int                 i_audio_stream; /* Selected audio stream. -1 if default */
    int                 i_video_stream;
    stream_t            *p_parser;

    /* Used to store bluray disc path */
    char                *psz_bd_path;
};

struct subpicture_updater_sys_t
{
    bluray_overlay_t    *p_overlay;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static es_out_t *esOutNew( demux_t *p_demux );

static int   blurayControl(demux_t *, int, va_list);
static int   blurayDemux(demux_t *);

static int   blurayInitTitles(demux_t *p_demux );
static int   bluraySetTitle(demux_t *p_demux, int i_title);

static void  blurayOverlayProc(void *ptr, const BD_OVERLAY * const overlay);

static int   onMouseEvent(vlc_object_t *p_vout, const char *psz_var,
                          vlc_value_t old, vlc_value_t val, void *p_data);

static void  blurayResetParser(demux_t *p_demux);

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
    p_sys->i_audio_stream = -1;
    p_sys->i_video_stream = -1;

    /* init demux info fields */
    p_demux->info.i_update    = 0;
    p_demux->info.i_title     = 0;
    p_demux->info.i_seekpoint = 0;

    TAB_INIT( p_sys->i_title, p_sys->pp_title );

    /* store current bd path */
    if (p_demux->psz_file) {
        p_sys->psz_bd_path = strndup(p_demux->psz_file, strlen(p_demux->psz_file));
    }

#if defined (HAVE_MNTENT_H) && defined (HAVE_SYS_STAT_H)
    /* If we're passed a block device, try to convert it to the mount point. */
    struct stat st;
    if ( !stat (p_sys->psz_bd_path, &st)) {
        if (S_ISBLK (st.st_mode)) {
            FILE* mtab = setmntent ("/proc/self/mounts", "r");
            struct mntent* m;
            struct mntent mbuf;
            char buf [8192];
            /* bd path may be a symlink (e.g. /dev/dvd -> /dev/sr0), so make
             * sure we look up the real device */
            char* bd_device = realpath(p_sys->psz_bd_path, NULL);
            while ((m = getmntent_r (mtab, &mbuf, buf, sizeof(buf))) != NULL) {
                if (!strcmp (m->mnt_fsname, (bd_device == NULL ? p_sys->psz_bd_path : bd_device))) {
                    p_sys->psz_bd_path = strndup(m->mnt_dir, strlen(m->mnt_dir));
                    break;
                }
            }
            free(bd_device);
            endmntent (mtab);
        }
    }
#endif /* HAVE_MNTENT_H && HAVE_SYS_STAT_H */
#ifdef __APPLE__
    /* If we're passed a block device, try to convert it to the mount point. */
    struct stat st;
    if ( !stat (p_sys->psz_bd_path, &st)) {
        if (S_ISBLK (st.st_mode)) {
            struct statfs mbuf[128];
            int fs_count;

            if ( (fs_count = getfsstat (NULL, 0, MNT_NOWAIT)) > 0 ) {
                getfsstat (mbuf, fs_count * sizeof(mbuf[0]), MNT_NOWAIT);
                for ( int i = 0; i < fs_count; ++i) {
                    if (!strcmp (mbuf[i].f_mntfromname, p_sys->psz_bd_path)) {
                        p_sys->psz_bd_path = strndup(mbuf[i].f_mntonname, strlen(mbuf[i].f_mntonname));
                    }
                }
            }
        }
    }
#endif
    p_sys->bluray = bd_open(p_sys->psz_bd_path, NULL);
    if (!p_sys->bluray) {
        free(p_sys);
        return VLC_EGENERIC;
    }

    /* Warning the user about AACS/BD+ */
    const BLURAY_DISC_INFO *disc_info = bd_get_disc_info(p_sys->bluray);

    /* Is it a bluray? */
    if (!disc_info->bluray_detected) {
        error_msg = "Path doesn't appear to be a Blu-ray";
        goto error;
    }

    msg_Info(p_demux, "First play: %i, Top menu: %i\n"
                      "HDMV Titles: %i, BD-J Titles: %i, Other: %i",
             disc_info->first_play_supported, disc_info->top_menu_supported,
             disc_info->num_hdmv_titles, disc_info->num_bdj_titles,
             disc_info->num_unsupported_titles);

    /* AACS */
    if (disc_info->aacs_detected) {
        if (!disc_info->libaacs_detected) {
            error_msg = _("This Blu-ray Disc needs a library for AACS decoding, "
                      "and your system does not have it.");
            goto error;
        }
        if (!disc_info->aacs_handled) {
#ifdef BD_AACS_CORRUPTED_DISC
            if (disc_info->aacs_error_code) {
                switch (disc_info->aacs_error_code) {
                    case BD_AACS_CORRUPTED_DISC:
                        error_msg = _("Blu-ray Disc is corrupted.");
                        break;
                    case BD_AACS_NO_CONFIG:
                        error_msg = _("Missing AACS configuration file!");
                        break;
                    case BD_AACS_NO_PK:
                        error_msg = _("No valid processing key found in AACS config file.");
                        break;
                    case BD_AACS_NO_CERT:
                        error_msg = _("No valid host certificate found in AACS config file.");
                        break;
                    case BD_AACS_CERT_REVOKED:
                        error_msg = _("AACS Host certificate revoked.");
                        break;
                    case BD_AACS_MMC_FAILED:
                        error_msg = _("AACS MMC failed.");
                        break;
                }
                goto error;
            }
#else
            error_msg = _("Your system AACS decoding library does not work. "
                      "Missing keys?");
            goto error;
#endif /* BD_AACS_CORRUPTED_DISC */
        }
    }

    /* BD+ */
    if (disc_info->bdplus_detected) {
        if (!disc_info->libbdplus_detected) {
            error_msg = _("This Blu-ray Disc needs a library for BD+ decoding, "
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
    p_sys->p_meta = bd_get_meta(p_sys->bluray);
    if (!p_sys->p_meta)
        msg_Warn(p_demux, "Failed to get meta info." );

    if (blurayInitTitles(p_demux) != VLC_SUCCESS) {
        goto error;
    }

    /*
     * Initialize the event queue, so we can receive events in blurayDemux(Menu).
     */
    bd_get_event(p_sys->bluray, NULL);

    p_sys->b_menu = var_InheritBool(p_demux, "bluray-menu");
    if (p_sys->b_menu) {
        p_sys->p_input = demux_GetParentInput(p_demux);
        if (unlikely(!p_sys->p_input)) {
            error_msg = "Could not get parent input";
            goto error;
        }

        /* libbluray will start playback from "First-Title" title */
        if (bd_play(p_sys->bluray) == 0) {
            error_msg = "Failed to start bluray playback. Please try without menu support.";
            goto error;
        }
        /* Registering overlay event handler */
        bd_register_overlay_proc(p_sys->bluray, p_demux, blurayOverlayProc);
    } else {
        /* set start title number */
        if (bluraySetTitle(p_demux, p_sys->i_longest_title) != VLC_SUCCESS) {
            msg_Err( p_demux, "Could not set the title %d", p_sys->i_longest_title );
            goto error;
        }
    }

    vlc_array_init(&p_sys->es);
    p_sys->p_out = esOutNew( p_demux );
    if (unlikely(p_sys->p_out == NULL)) {
        goto error;
    }

    blurayResetParser( p_demux );
    if (!p_sys->p_parser) {
        msg_Err(p_demux, "Failed to create TS demuxer");
        goto error;
    }

    p_demux->pf_control = blurayControl;
    p_demux->pf_demux   = blurayDemux;

    return VLC_SUCCESS;

error:
    if (error_msg)
        dialog_Fatal(p_demux, _("Blu-ray error"), "%s", error_msg);
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

    if (p_sys->p_vout != NULL) {
        var_DelCallback(p_sys->p_vout, "mouse-moved", &onMouseEvent, p_demux);
        var_DelCallback(p_sys->p_vout, "mouse-clicked", &onMouseEvent, p_demux);
        vlc_object_release(p_sys->p_vout);
    }
    if (p_sys->p_input != NULL)
        vlc_object_release(p_sys->p_input);
    if (p_sys->p_parser)
        stream_Delete(p_sys->p_parser);
    if (p_sys->p_out != NULL)
        es_out_Delete(p_sys->p_out);
    assert( vlc_array_count(&p_sys->es) == 0 );
    vlc_array_clear( &p_sys->es );

    /* Titles */
    for (unsigned int i = 0; i < p_sys->i_title; i++)
        vlc_input_title_Delete(p_sys->pp_title[i]);
    TAB_CLEAN( p_sys->i_title, p_sys->pp_title );

    free(p_sys->psz_bd_path);
    free(p_sys);
}

/*****************************************************************************
 * Elementary streams handling
 *****************************************************************************/

struct es_out_sys_t {
    demux_t *p_demux;
};

typedef struct  fmt_es_pair {
    int         i_id;
    es_out_id_t *p_es;
}               fmt_es_pair_t;

static int  findEsPairIndex( demux_sys_t *p_sys, int i_id )
{
    for ( int i = 0; i < vlc_array_count(&p_sys->es); ++i ) {
        if ( ((fmt_es_pair_t*)vlc_array_item_at_index(&p_sys->es, i))->i_id == i_id )
            return i;
    }
    return -1;
}

static int  findEsPairIndexByEs( demux_sys_t *p_sys, es_out_id_t *p_es )
{
    for ( int i = 0; i < vlc_array_count(&p_sys->es); ++i ) {
        if ( ((fmt_es_pair_t*)vlc_array_item_at_index(&p_sys->es, i))->p_es == p_es )
            return i;
    }
    return -1;
}

static es_out_id_t *esOutAdd( es_out_t *p_out, const es_format_t *p_fmt )
{
    demux_sys_t *p_sys = p_out->p_sys->p_demux->p_sys;
    es_format_t fmt;

    es_format_Copy(&fmt, p_fmt);
    switch (fmt.i_cat)
    {
    case VIDEO_ES:
        if ( p_sys->i_video_stream != -1 && p_sys->i_video_stream != p_fmt->i_id )
            fmt.i_priority = -2;
        break ;
    case AUDIO_ES:
        if ( p_sys->i_audio_stream != -1 && p_sys->i_audio_stream != p_fmt->i_id )
            fmt.i_priority = -2;
        break ;
    case SPU_ES:
        break ;
    }

    es_out_id_t *p_es = es_out_Add( p_out->p_sys->p_demux->out, &fmt );
    if ( p_fmt->i_id >= 0 ) {
        /* Ensure we are not overriding anything */
        int idx = findEsPairIndex(p_sys, p_fmt->i_id);
        if ( idx == -1 ) {
            fmt_es_pair_t *p_pair = malloc( sizeof(*p_pair) );
            if ( likely(p_pair != NULL) ) {
                p_pair->i_id = p_fmt->i_id;
                p_pair->p_es = p_es;
                msg_Info( p_out->p_sys->p_demux, "Adding ES %d", p_fmt->i_id );
                vlc_array_append(&p_sys->es, p_pair);
            }
        }
    }
    es_format_Clean(&fmt);
    return p_es;
}

static int esOutSend( es_out_t *p_out, es_out_id_t *p_es, block_t *p_block )
{
    return es_out_Send( p_out->p_sys->p_demux->out, p_es, p_block );
}

static void esOutDel( es_out_t *p_out, es_out_id_t *p_es )
{
    int idx = findEsPairIndexByEs( p_out->p_sys->p_demux->p_sys, p_es );
    if (idx >= 0)
    {
        free( vlc_array_item_at_index( &p_out->p_sys->p_demux->p_sys->es, idx) );
        vlc_array_remove(&p_out->p_sys->p_demux->p_sys->es, idx);
    }
    es_out_Del( p_out->p_sys->p_demux->out, p_es );
}

static int esOutControl( es_out_t *p_out, int i_query, va_list args )
{
    return es_out_vaControl( p_out->p_sys->p_demux->out, i_query, args );
}

static void esOutDestroy( es_out_t *p_out )
{
    for ( int i = 0; i < vlc_array_count(&p_out->p_sys->p_demux->p_sys->es); ++i )
        free( vlc_array_item_at_index(&p_out->p_sys->p_demux->p_sys->es, i) );
    vlc_array_clear(&p_out->p_sys->p_demux->p_sys->es);
    free( p_out->p_sys );
    free( p_out );
}

static es_out_t *esOutNew( demux_t *p_demux )
{
    assert( vlc_array_count(&p_demux->p_sys->es) == 0 );
    es_out_t    *p_out = malloc( sizeof(*p_out) );
    if ( unlikely(p_out == NULL) )
        return NULL;

    p_out->pf_add       = &esOutAdd;
    p_out->pf_control   = &esOutControl;
    p_out->pf_del       = &esOutDel;
    p_out->pf_destroy   = &esOutDestroy;
    p_out->pf_send      = &esOutSend;

    p_out->p_sys = malloc( sizeof(*p_out->p_sys) );
    if ( unlikely( p_out->p_sys == NULL ) ) {
        free( p_out );
        return NULL;
    }
    p_out->p_sys->p_demux = p_demux;
    return p_out;
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
    {
        vlc_mutex_unlock(&p_overlay->lock);
        return;
    }

    subpicture_region_t **p_dst = &(p_subpic->p_region);
    while (p_src != NULL) {
        *p_dst = subpicture_region_Clone(p_src);
        if (*p_dst == NULL)
            break;
        p_dst = &((*p_dst)->p_next);
        p_src = p_src->p_next;
    }
    if (*p_dst != NULL)
        (*p_dst)->p_next = NULL;
    p_overlay->status = Displayed;
    vlc_mutex_unlock(&p_overlay->lock);
}

static void blurayCleanOverlayStruct(bluray_overlay_t *);

static void subpictureUpdaterDestroy(subpicture_t *p_subpic)
{
    blurayCleanOverlayStruct(p_subpic->updater.p_sys->p_overlay);
}

/*****************************************************************************
 * User input events:
 *****************************************************************************/
static int onMouseEvent(vlc_object_t *p_vout, const char *psz_var, vlc_value_t old,
                        vlc_value_t val, void *p_data)
{
    demux_t     *p_demux = (demux_t*)p_data;
    demux_sys_t *p_sys   = p_demux->p_sys;
    mtime_t     now      = mdate();
    VLC_UNUSED(old);
    VLC_UNUSED(p_vout);

    if (psz_var[6] == 'm')   //Mouse moved
        bd_mouse_select(p_sys->bluray, now, val.coords.x, val.coords.y);
    else if (psz_var[6] == 'c') {
        bd_mouse_select(p_sys->bluray, now, val.coords.x, val.coords.y);
        bd_user_input(p_sys->bluray, now, BD_VK_MOUSE_ACTIVATE);
    } else {
        assert(0);
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * libbluray overlay handling:
 *****************************************************************************/
static void blurayCleanOverlayStruct(bluray_overlay_t *p_overlay)
{
    if (!atomic_flag_test_and_set(&p_overlay->released_once))
        return;
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
                blurayCleanOverlayStruct(p_sys->p_overlays[i]);
                p_sys->p_overlays[i] = NULL;
            }
        }
        var_DelCallback(p_sys->p_vout, "mouse-moved", &onMouseEvent, p_demux);
        var_DelCallback(p_sys->p_vout, "mouse-clicked", &onMouseEvent, p_demux);
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
        return;
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
    /* two references: vout + demux */
    p_sys->p_overlays[ov->plane]->released_once = ATOMIC_FLAG_INIT;

    p_upd_sys->p_overlay = p_sys->p_overlays[ov->plane];
    subpicture_updater_t updater = {
        .pf_validate = subpictureUpdaterValidate,
        .pf_update   = subpictureUpdaterUpdate,
        .pf_destroy  = subpictureUpdaterDestroy,
        .p_sys       = p_upd_sys,
    };
    vlc_mutex_init(&p_sys->p_overlays[ov->plane]->lock);
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
     * as it is held by the vout thread.
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
    if (p_sys->p_parser)
        stream_Delete(p_sys->p_parser);
    p_sys->p_parser = stream_DemuxNew(p_demux, "ts", p_sys->p_out);
    if (!p_sys->p_parser) {
        msg_Err(p_demux, "Failed to create TS demuxer");
    }
}

static void blurayUpdateTitle(demux_t *p_demux, unsigned i_title)
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
            *pi_length = p_demux->info.i_title < (int)p_sys->i_title ? CUR_LENGTH : 0;
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
            *pf_position = p_demux->info.i_title < (int)p_sys->i_title ?
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
            vlc_meta_t *p_meta = (vlc_meta_t *) va_arg (args, vlc_meta_t*);
            const META_DL *meta = p_sys->p_meta;
            if (meta == NULL)
                return VLC_EGENERIC;

            if (!EMPTY_STR(meta->di_name)) vlc_meta_SetTitle(p_meta, meta->di_name);

            if (!EMPTY_STR(meta->language_code)) vlc_meta_AddExtra(p_meta, "Language", meta->language_code);
            if (!EMPTY_STR(meta->filename)) vlc_meta_AddExtra(p_meta, "Filename", meta->filename);
            if (!EMPTY_STR(meta->di_alternative)) vlc_meta_AddExtra(p_meta, "Alternative", meta->di_alternative);

            // if (meta->di_set_number > 0) vlc_meta_SetTrackNum(p_meta, meta->di_set_number);
            // if (meta->di_num_sets > 0) vlc_meta_AddExtra(p_meta, "Discs numbers in Set", meta->di_num_sets);

            if (meta->thumb_count > 0 && meta->thumbnails)
            {
                char *psz_thumbpath;
                if( asprintf( &psz_thumbpath, "%s" DIR_SEP "BDMV" DIR_SEP "META" DIR_SEP "DL" DIR_SEP "%s",
                              p_sys->psz_bd_path, meta->thumbnails[0].path ) > 0 )
                {
                    char *psz_thumburl = vlc_path2uri( psz_thumbpath, "file" );
                    if( unlikely(psz_thumburl == NULL) )
                        return VLC_ENOMEM;

                    vlc_meta_SetArtURL( p_meta, psz_thumburl );
                    free( psz_thumburl );
                }
                free( psz_thumbpath );
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

static void     blurayUpdateCurrentClip( demux_t *p_demux, uint32_t clip )
{
    if (clip == 0xFF)
        return ;
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->i_current_clip = clip;
    BLURAY_TITLE_INFO   *info = bd_get_title_info(p_sys->bluray,
                                        bd_get_current_title(p_sys->bluray), 0);
    if ( info == NULL )
        return ;
    /* Let's assume a single video track for now.
     * This may brake later, but it's enough for now.
     */
    assert(info->clips[p_sys->i_current_clip].video_stream_count >= 1);
    p_sys->i_video_stream = info->clips[p_sys->i_current_clip].video_streams[0].pid;
    bd_free_title_info(info);
}

static void blurayHandleEvent( demux_t *p_demux, const BD_EVENT *e )
{
    demux_sys_t     *p_sys = p_demux->p_sys;

    switch (e->event)
    {
        case BD_EVENT_TITLE:
            blurayUpdateTitle(p_demux, e->param);
            break;
        case BD_EVENT_PLAYITEM:
            blurayUpdateCurrentClip(p_demux, e->param);
            break;
        case BD_EVENT_AUDIO_STREAM:
        {
            if ( e->param == 0xFF )
                break ;
            BLURAY_TITLE_INFO   *info = bd_get_title_info(p_sys->bluray,
                                       bd_get_current_title(p_sys->bluray), 0);
            if ( info == NULL )
                break ;
            /* The param we get is the real stream id, not an index, ie. it starts from 1 */
            int pid = info->clips[p_sys->i_current_clip].audio_streams[e->param - 1].pid;
            int idx = findEsPairIndex( p_sys, pid );
            if ( idx >= 0 ) {
                es_out_id_t *p_es = vlc_array_item_at_index(&p_sys->es, idx);
                es_out_Control( p_demux->out, ES_OUT_SET_ES, p_es );
            }
            bd_free_title_info( info );
            p_sys->i_audio_stream = pid;
            break ;
        }
        case BD_EVENT_CHAPTER:
            p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
            p_demux->info.i_seekpoint = e->param;
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

    block_t *p_block = block_Alloc(NB_TS_PACKETS * (int64_t)BD_TS_PACKET_SIZE);
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
    } else {
        BD_EVENT e;
        nread = bd_read_ext(p_sys->bluray, p_block->p_buffer,
                            NB_TS_PACKETS * BD_TS_PACKET_SIZE, &e);
        if (nread < 0)
        {
            block_Release(p_block);
            return -1;
        }
        if (nread == 0) {
            if (e.event == BD_EVENT_NONE)
                msg_Info(p_demux, "We reached the end of a title");
            else
                blurayHandleEvent(p_demux, &e);
            block_Release(p_block);
            return 1;
        }
        if (p_sys->current_overlay != -1) {
            vlc_mutex_lock(&p_sys->p_overlays[p_sys->current_overlay]->lock);
            if (p_sys->p_overlays[p_sys->current_overlay]->status == ToDisplay) {
                vlc_mutex_unlock(&p_sys->p_overlays[p_sys->current_overlay]->lock);
                if (p_sys->p_vout == NULL)
                    p_sys->p_vout = input_GetVout(p_sys->p_input);
                if (p_sys->p_vout != NULL) {
                    var_AddCallback(p_sys->p_vout, "mouse-moved", &onMouseEvent, p_demux);
                    var_AddCallback(p_sys->p_vout, "mouse-clicked", &onMouseEvent, p_demux);
                    bluraySendOverlayToVout(p_demux);
                }
            } else
                vlc_mutex_unlock(&p_sys->p_overlays[p_sys->current_overlay]->lock);
        }
    }

    p_block->i_buffer = nread;

    stream_DemuxSend(p_sys->p_parser, p_block);

    return 1;
}
