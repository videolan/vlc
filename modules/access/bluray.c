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

#if defined (HAVE_MNTENT_H) && defined(HAVE_SYS_STAT_H)
# include <mntent.h>
# include <sys/stat.h>
#endif

#ifdef __APPLE__
# include <sys/stat.h>
# include <sys/param.h>
# include <sys/ucred.h>
# include <sys/mount.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>                      /* demux_t */
#include <vlc_input.h>                      /* Seekpoints, chapters */
#include <vlc_atomic.h>
#include <vlc_dialog.h>                     /* BD+/AACS warnings */
#include <vlc_vout.h>                       /* vout_PutSubpicture / subpicture_t */
#include <vlc_url.h>                        /* vlc_path2uri */
#include <vlc_iso_lang.h>

/* FIXME we should find a better way than including that */
#include "../../src/text/iso-639_def.h"


#include <libbluray/bluray.h>
#include <libbluray/bluray-version.h>
#include <libbluray/keys.h>
#include <libbluray/meta_data.h>
#include <libbluray/overlay.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define BD_MENU_TEXT        N_("Blu-ray menus")
#define BD_MENU_LONGTEXT    N_("Use Blu-ray menus. If disabled, "\
                                "the movie will start directly")
#define BD_REGION_TEXT      N_("Region code")
#define BD_REGION_LONGTEXT  N_("Blu-Ray player region code. "\
                                "Some discs can be played only with a correct region code.")

static const char *const ppsz_region_code[] = {
    "A", "B", "C" };
static const char *const ppsz_region_code_text[] = {
    "Region A", "Region B", "Region C" };

#define REGION_DEFAULT   1   /* Index to region list. Actual region code is (1<<REGION_DEFAULT) */
#define LANGUAGE_DEFAULT ("eng")

/* Callbacks */
static int  blurayOpen (vlc_object_t *);
static void blurayClose(vlc_object_t *);

vlc_module_begin ()
    set_shortname(N_("Blu-ray"))
    set_description(N_("Blu-ray Disc support (libbluray)"))

    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    set_capability("access_demux", 200)
    add_bool("bluray-menu", false, BD_MENU_TEXT, BD_MENU_LONGTEXT, false)
    add_string("bluray-region", ppsz_region_code[REGION_DEFAULT], BD_REGION_TEXT, BD_REGION_LONGTEXT, false)
        change_string_list(ppsz_region_code, ppsz_region_code_text)

    add_shortcut("bluray", "file")

    set_callbacks(blurayOpen, blurayClose)
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
    int                 i_channel;
    OverlayStatus       status;
    subpicture_region_t *p_regions;
    int                 width, height;
} bluray_overlay_t;

struct  demux_sys_t
{
    BLURAY              *bluray;

    /* Titles */
    unsigned int        i_title;
    unsigned int        i_longest_title;
    input_title_t       **pp_title;

    vlc_mutex_t             pl_info_lock;
    BLURAY_TITLE_INFO      *p_pl_info;
    const BLURAY_CLIP_INFO *p_clip_info;

    /* Meta information */
    const META_DL       *p_meta;

    /* Menus */
    bluray_overlay_t    *p_overlays[MAX_OVERLAY];
    bool                b_menu;
    bool                b_menu_open;
    bool                b_popup_available;
    mtime_t             i_still_end_time;

    /* */
    vout_thread_t       *p_vout;

    /* TS stream */
    es_out_t            *p_out;
    vlc_array_t         es;
    int                 i_audio_stream; /* Selected audio stream. -1 if default */
    int                 i_spu_stream;   /* Selected subtitle stream. -1 if default */
    int                 i_video_stream;
    stream_t            *p_parser;
    bool                b_flushed;

    /* Used to store bluray disc path */
    char                *psz_bd_path;
};

struct subpicture_updater_sys_t
{
    bluray_overlay_t    *p_overlay;
};

/* Get a 3 char code
 * FIXME: partiallyy duplicated from src/input/es_out.c
 */
static const char *DemuxGetLanguageCode( demux_t *p_demux, const char *psz_var )
{
    const iso639_lang_t *pl;
    char *psz_lang;
    char *p;

    psz_lang = var_CreateGetString( p_demux, psz_var );
    if( !psz_lang )
        return LANGUAGE_DEFAULT;

    /* XXX: we will use only the first value
     * (and ignore other ones in case of a list) */
    if( ( p = strchr( psz_lang, ',' ) ) )
        *p = '\0';

    for( pl = p_languages; pl->psz_eng_name != NULL; pl++ )
    {
        if( *psz_lang == '\0' )
            continue;
        if( !strcasecmp( pl->psz_eng_name, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_1, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_2T, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_2B, psz_lang ) )
            break;
    }

    free( psz_lang );

    if( pl->psz_eng_name != NULL )
        return pl->psz_iso639_2T;

    return LANGUAGE_DEFAULT;
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static es_out_t *esOutNew(demux_t *p_demux);

static int   blurayControl(demux_t *, int, va_list);
static int   blurayDemux(demux_t *);

static void  blurayInitTitles(demux_t *p_demux, int menu_titles);
static int   bluraySetTitle(demux_t *p_demux, int i_title);

static void  blurayOverlayProc(void *ptr, const BD_OVERLAY * const overlay);
static void  blurayArgbOverlayProc(void *ptr, const BD_ARGB_OVERLAY * const overlay);

static int   onMouseEvent(vlc_object_t *p_vout, const char *psz_var,
                          vlc_value_t old, vlc_value_t val, void *p_data);

static void  blurayResetParser(demux_t *p_demux);

#define FROM_TICKS(a) (a*CLOCK_FREQ / INT64_C(90000))
#define TO_TICKS(a)   (a*INT64_C(90000)/CLOCK_FREQ)
#define CUR_LENGTH    p_sys->pp_title[p_demux->info.i_title]->i_length

/* */
static void FindMountPoint(char **file)
{
    char *device = *file;
#if defined (HAVE_MNTENT_H) && defined (HAVE_SYS_STAT_H)
    /* bd path may be a symlink (e.g. /dev/dvd -> /dev/sr0), so make sure
     * we look up the real device */
    char *bd_device = realpath(device, NULL);
    if (bd_device == NULL)
        return;

    struct stat st;
    if (lstat (bd_device, &st) == 0 && S_ISBLK (st.st_mode)) {
        FILE *mtab = setmntent ("/proc/self/mounts", "r");
        struct mntent *m, mbuf;
        char buf [8192];

        while ((m = getmntent_r (mtab, &mbuf, buf, sizeof(buf))) != NULL) {
            if (!strcmp (m->mnt_fsname, bd_device)) {
                free(device);
                *file = strdup(m->mnt_dir);
                break;
            }
        }
        endmntent (mtab);
    }
    free(bd_device);

#elif defined(__APPLE__)
    struct stat st;
    if (!stat (device, &st) && S_ISBLK (st.st_mode)) {
        int fs_count = getfsstat (NULL, 0, MNT_NOWAIT);
        if (fs_count > 0) {
            struct statfs mbuf[128];
            getfsstat (mbuf, fs_count * sizeof(mbuf[0]), MNT_NOWAIT);
            for (int i = 0; i < fs_count; ++i)
                if (!strcmp (mbuf[i].f_mntfromname, device)) {
                    free(device);
                    *file = strdup(mbuf[i].f_mntonname);
                    return;
                }
        }
    }
#else
# warning Disc device to mount point not implemented
    VLC_UNUSED( device );
#endif
}

static void blurayReleaseVout(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if (p_sys->p_vout != NULL) {
        var_DelCallback(p_sys->p_vout, "mouse-moved", onMouseEvent, p_demux);
        var_DelCallback(p_sys->p_vout, "mouse-clicked", onMouseEvent, p_demux);
        vlc_object_release(p_sys->p_vout);
        p_sys->p_vout = NULL;
    }
}

/*****************************************************************************
 * cache current playlist (title) information
 *****************************************************************************/

static void setTitleInfo(demux_sys_t *p_sys, BLURAY_TITLE_INFO *info)
{
    vlc_mutex_lock(&p_sys->pl_info_lock);

    if (p_sys->p_pl_info) {
        bd_free_title_info(p_sys->p_pl_info);
    }
    p_sys->p_pl_info   = info;
    p_sys->p_clip_info = NULL;

    if (p_sys->p_pl_info && p_sys->p_pl_info->clip_count) {
        p_sys->p_clip_info = &p_sys->p_pl_info->clips[0];
    }

    vlc_mutex_unlock(&p_sys->pl_info_lock);
}

/*****************************************************************************
 * blurayOpen: module init function
 *****************************************************************************/
static int blurayOpen(vlc_object_t *object)
{
    demux_t *p_demux = (demux_t*)object;
    demux_sys_t *p_sys;

    const char *error_msg = NULL;
#define BLURAY_ERROR(s) do { error_msg = s; goto error; } while(0)

    if (strcmp(p_demux->psz_access, "bluray")) {
        // TODO BDMV support, once we figure out what to do in libbluray
        return VLC_EGENERIC;
    }

    /* */
    p_demux->p_sys = p_sys = calloc(1, sizeof(*p_sys));
    if (unlikely(!p_sys))
        return VLC_ENOMEM;

    p_sys->i_audio_stream = -1;
    p_sys->i_spu_stream = -1;
    p_sys->i_video_stream = -1;
    p_sys->i_still_end_time = 0;

    /* init demux info fields */
    p_demux->info.i_update    = 0;
    p_demux->info.i_title     = 0;
    p_demux->info.i_seekpoint = 0;

    TAB_INIT(p_sys->i_title, p_sys->pp_title);

    /* store current bd path */
    if (p_demux->psz_file)
        p_sys->psz_bd_path = strdup(p_demux->psz_file);

    /* If we're passed a block device, try to convert it to the mount point. */
    FindMountPoint(&p_sys->psz_bd_path);

    p_sys->bluray = bd_open(p_sys->psz_bd_path, NULL);
    if (!p_sys->bluray) {
        free(p_sys->psz_bd_path);
        free(p_sys);
        return VLC_EGENERIC;
    }

    vlc_mutex_init(&p_sys->pl_info_lock);

    /* Warning the user about AACS/BD+ */
    const BLURAY_DISC_INFO *disc_info = bd_get_disc_info(p_sys->bluray);

    /* Is it a bluray? */
    if (!disc_info->bluray_detected)
        BLURAY_ERROR(_("Path doesn't appear to be a Blu-ray"));

    msg_Info(p_demux, "First play: %i, Top menu: %i\n"
                      "HDMV Titles: %i, BD-J Titles: %i, Other: %i",
             disc_info->first_play_supported, disc_info->top_menu_supported,
             disc_info->num_hdmv_titles, disc_info->num_bdj_titles,
             disc_info->num_unsupported_titles);

    /* AACS */
    if (disc_info->aacs_detected) {
        msg_Dbg(p_demux, "Disc is using AACS");
        if (!disc_info->libaacs_detected)
            BLURAY_ERROR(_("This Blu-ray Disc needs a library for AACS decoding"
                      ", and your system does not have it."));
        if (!disc_info->aacs_handled) {
            if (disc_info->aacs_error_code) {
                switch (disc_info->aacs_error_code) {
                case BD_AACS_CORRUPTED_DISC:
                    BLURAY_ERROR(_("Blu-ray Disc is corrupted."));
                case BD_AACS_NO_CONFIG:
                    BLURAY_ERROR(_("Missing AACS configuration file!"));
                case BD_AACS_NO_PK:
                    BLURAY_ERROR(_("No valid processing key found in AACS config file."));
                case BD_AACS_NO_CERT:
                    BLURAY_ERROR(_("No valid host certificate found in AACS config file."));
                case BD_AACS_CERT_REVOKED:
                    BLURAY_ERROR(_("AACS Host certificate revoked."));
                case BD_AACS_MMC_FAILED:
                    BLURAY_ERROR(_("AACS MMC failed."));
                }
            }
        }
    }

    /* BD+ */
    if (disc_info->bdplus_detected) {
        msg_Dbg(p_demux, "Disc is using BD+");
        if (!disc_info->libbdplus_detected)
            BLURAY_ERROR(_("This Blu-ray Disc needs a library for BD+ decoding"
                      ", and your system does not have it."));
        if (!disc_info->bdplus_handled)
            BLURAY_ERROR(_("Your system BD+ decoding library does not work. "
                      "Missing configuration?"));
    }

    /* set player region code */
    char *psz_region = var_InheritString(p_demux, "bluray-region");
    unsigned int region = psz_region ? (psz_region[0] - 'A') : REGION_DEFAULT;
    free(psz_region);
    bd_set_player_setting(p_sys->bluray, BLURAY_PLAYER_SETTING_REGION_CODE, 1<<region);

    /* set preferred languages */
    const char *psz_code = DemuxGetLanguageCode( p_demux, "audio-language" );
    bd_set_player_setting_str(p_sys->bluray, BLURAY_PLAYER_SETTING_AUDIO_LANG, psz_code);
    psz_code = DemuxGetLanguageCode( p_demux, "sub-language" );
    bd_set_player_setting_str(p_sys->bluray, BLURAY_PLAYER_SETTING_PG_LANG,    psz_code);
    psz_code = DemuxGetLanguageCode( p_demux, "menu-language" );
    bd_set_player_setting_str(p_sys->bluray, BLURAY_PLAYER_SETTING_MENU_LANG,  psz_code);

    /* Get titles and chapters */
    p_sys->p_meta = bd_get_meta(p_sys->bluray);
    if (!p_sys->p_meta)
        msg_Warn(p_demux, "Failed to get meta info.");

    p_sys->b_menu = var_InheritBool(p_demux, "bluray-menu");

    blurayInitTitles(p_demux, disc_info->num_hdmv_titles + disc_info->num_bdj_titles + 1/*Top Menu*/ + 1/*First Play*/);

    /*
     * Initialize the event queue, so we can receive events in blurayDemux(Menu).
     */
    bd_get_event(p_sys->bluray, NULL);

    /* Registering overlay event handler */
    bd_register_overlay_proc(p_sys->bluray, p_demux, blurayOverlayProc);
    if (unlikely(!p_demux->p_input)) {
        msg_Err(p_demux, "Could not get parent input");
        goto error;
    }

    if (p_sys->b_menu) {

        /* Register ARGB overlay handler for BD-J */
        if (disc_info->num_bdj_titles)
            bd_register_argb_overlay_proc(p_sys->bluray, p_demux, blurayArgbOverlayProc, NULL);

        /* libbluray will start playback from "First-Title" title */
        if (bd_play(p_sys->bluray) == 0)
            BLURAY_ERROR(_("Failed to start bluray playback. Please try without menu support."));

    } else {
        /* set start title number */
        if (bluraySetTitle(p_demux, p_sys->i_longest_title) != VLC_SUCCESS) {
            msg_Err(p_demux, "Could not set the title %d", p_sys->i_longest_title);
            goto error;
        }
    }

    vlc_array_init(&p_sys->es);
    p_sys->p_out = esOutNew(p_demux);
    if (unlikely(p_sys->p_out == NULL))
        goto error;

    blurayResetParser(p_demux);
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
#undef BLURAY_ERROR
}


/*****************************************************************************
 * blurayClose: module destroy function
 *****************************************************************************/
static void blurayClose(vlc_object_t *object)
{
    demux_t *p_demux = (demux_t*)object;
    demux_sys_t *p_sys = p_demux->p_sys;

    setTitleInfo(p_sys, NULL);

    /*
     * Close libbluray first.
     * This will close all the overlays before we release p_vout
     * bd_close(NULL) can crash
     */
    assert(p_sys->bluray);
    bd_close(p_sys->bluray);

    blurayReleaseVout(p_demux);

    if (p_sys->p_parser)
        stream_Delete(p_sys->p_parser);
    if (p_sys->p_out != NULL)
        es_out_Delete(p_sys->p_out);
    assert(vlc_array_count(&p_sys->es) == 0);
    vlc_array_clear(&p_sys->es);

    /* Titles */
    for (unsigned int i = 0; i < p_sys->i_title; i++)
        vlc_input_title_Delete(p_sys->pp_title[i]);
    TAB_CLEAN(p_sys->i_title, p_sys->pp_title);

    vlc_mutex_destroy(&p_sys->pl_info_lock);

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

static int  findEsPairIndex(demux_sys_t *p_sys, int i_id)
{
    for (int i = 0; i < vlc_array_count(&p_sys->es); ++i)
        if (((fmt_es_pair_t*)vlc_array_item_at_index(&p_sys->es, i))->i_id == i_id)
            return i;

    return -1;
}

static int  findEsPairIndexByEs(demux_sys_t *p_sys, es_out_id_t *p_es)
{
    for (int i = 0; i < vlc_array_count(&p_sys->es); ++i)
        if (((fmt_es_pair_t*)vlc_array_item_at_index(&p_sys->es, i))->p_es == p_es)
            return i;

    return -1;
}

static void setStreamLang(es_format_t *p_fmt,
                          const BLURAY_STREAM_INFO *p_streams, int i_stream_count)
{
    for (int i = 0; i < i_stream_count; i++) {
        if (p_fmt->i_id == p_streams[i].pid) {
            free(p_fmt->psz_language);
            p_fmt->psz_language = strndup((const char *)p_streams[i].lang, 3);
            return;
        }
    }
}

static es_out_id_t *esOutAdd(es_out_t *p_out, const es_format_t *p_fmt)
{
    demux_sys_t *p_sys = p_out->p_sys->p_demux->p_sys;
    es_format_t fmt;

    es_format_Copy(&fmt, p_fmt);

    vlc_mutex_lock(&p_sys->pl_info_lock);

    switch (fmt.i_cat) {
    case VIDEO_ES:
        if (p_sys->i_video_stream != -1 && p_sys->i_video_stream != p_fmt->i_id)
            fmt.i_priority = ES_PRIORITY_NOT_SELECTABLE;
        break ;
    case AUDIO_ES:
        if (p_sys->i_audio_stream != -1 && p_sys->i_audio_stream != p_fmt->i_id)
            fmt.i_priority = ES_PRIORITY_NOT_SELECTABLE;
        if (p_sys->p_clip_info)
            setStreamLang(&fmt, p_sys->p_clip_info->audio_streams, p_sys->p_clip_info->audio_stream_count);
        break ;
    case SPU_ES:
        if (p_sys->i_spu_stream != -1 && p_sys->i_spu_stream != p_fmt->i_id)
            fmt.i_priority = ES_PRIORITY_NOT_SELECTABLE;
        if (p_sys->p_clip_info)
            setStreamLang(&fmt, p_sys->p_clip_info->pg_streams, p_sys->p_clip_info->pg_stream_count);
        break ;
    }

    vlc_mutex_unlock(&p_sys->pl_info_lock);

    es_out_id_t *p_es = es_out_Add(p_out->p_sys->p_demux->out, &fmt);
    if (p_fmt->i_id >= 0) {
        /* Ensure we are not overriding anything */
        int idx = findEsPairIndex(p_sys, p_fmt->i_id);
        if (idx == -1) {
            fmt_es_pair_t *p_pair = malloc(sizeof(*p_pair));
            if (likely(p_pair != NULL)) {
                p_pair->i_id = p_fmt->i_id;
                p_pair->p_es = p_es;
                msg_Info(p_out->p_sys->p_demux, "Adding ES %d", p_fmt->i_id);
                vlc_array_append(&p_sys->es, p_pair);
            }
        }
    }
    es_format_Clean(&fmt);
    return p_es;
}

static int esOutSend(es_out_t *p_out, es_out_id_t *p_es, block_t *p_block)
{
    return es_out_Send(p_out->p_sys->p_demux->out, p_es, p_block);
}

static void esOutDel(es_out_t *p_out, es_out_id_t *p_es)
{
    int idx = findEsPairIndexByEs(p_out->p_sys->p_demux->p_sys, p_es);
    if (idx >= 0) {
        free(vlc_array_item_at_index(&p_out->p_sys->p_demux->p_sys->es, idx));
        vlc_array_remove(&p_out->p_sys->p_demux->p_sys->es, idx);
    }
    es_out_Del(p_out->p_sys->p_demux->out, p_es);
}

static int esOutControl(es_out_t *p_out, int i_query, va_list args)
{
    return es_out_vaControl(p_out->p_sys->p_demux->out, i_query, args);
}

static void esOutDestroy(es_out_t *p_out)
{
    for (int i = 0; i < vlc_array_count(&p_out->p_sys->p_demux->p_sys->es); ++i)
        free(vlc_array_item_at_index(&p_out->p_sys->p_demux->p_sys->es, i));
    vlc_array_clear(&p_out->p_sys->p_demux->p_sys->es);
    free(p_out->p_sys);
    free(p_out);
}

static es_out_t *esOutNew(demux_t *p_demux)
{
    assert(vlc_array_count(&p_demux->p_sys->es) == 0);
    es_out_t    *p_out = malloc(sizeof(*p_out));
    if (unlikely(p_out == NULL))
        return NULL;

    p_out->pf_add       = esOutAdd;
    p_out->pf_control   = esOutControl;
    p_out->pf_del       = esOutDel;
    p_out->pf_destroy   = esOutDestroy;
    p_out->pf_send      = esOutSend;

    p_out->p_sys = malloc(sizeof(*p_out->p_sys));
    if (unlikely(p_out->p_sys == NULL)) {
        free(p_out);
        return NULL;
    }
    p_out->p_sys->p_demux = p_demux;
    return p_out;
}

/*****************************************************************************
 * subpicture_updater_t functions:
 *****************************************************************************/
static int subpictureUpdaterValidate(subpicture_t *p_subpic,
                                      bool b_fmt_src, const video_format_t *p_fmt_src,
                                      bool b_fmt_dst, const video_format_t *p_fmt_dst,
                                      mtime_t i_ts)
{
    VLC_UNUSED(b_fmt_src);
    VLC_UNUSED(b_fmt_dst);
    VLC_UNUSED(p_fmt_src);
    VLC_UNUSED(p_fmt_dst);
    VLC_UNUSED(i_ts);

    subpicture_updater_sys_t *p_upd_sys = p_subpic->updater.p_sys;
    bluray_overlay_t         *p_overlay = p_upd_sys->p_overlay;

    vlc_mutex_lock(&p_overlay->lock);
    int res = p_overlay->status == Outdated;
    vlc_mutex_unlock(&p_overlay->lock);
    return res;
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
    if (!p_src) {
        vlc_mutex_unlock(&p_overlay->lock);
        return;
    }

    subpicture_region_t **p_dst = &p_subpic->p_region;
    while (p_src != NULL) {
        *p_dst = subpicture_region_Copy(p_src);
        if (*p_dst == NULL)
            break;
        p_dst = &(*p_dst)->p_next;
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
    free(p_subpic->updater.p_sys);
 }

static subpicture_t *bluraySubpictureCreate(bluray_overlay_t *p_ov)
{
    subpicture_updater_sys_t *p_upd_sys = malloc(sizeof(*p_upd_sys));
    if (unlikely(p_upd_sys == NULL)) {
        return NULL;
    }

    p_upd_sys->p_overlay = p_ov;

    subpicture_updater_t updater = {
        .pf_validate = subpictureUpdaterValidate,
        .pf_update   = subpictureUpdaterUpdate,
        .pf_destroy  = subpictureUpdaterDestroy,
        .p_sys       = p_upd_sys,
    };

    subpicture_t *p_pic = subpicture_New(&updater);
    if (p_pic == NULL) {
        free(p_upd_sys);
        return NULL;
    }

    p_pic->i_original_picture_width = p_ov->width;
    p_pic->i_original_picture_height = p_ov->height;
    p_pic->b_ephemer = true;
    p_pic->b_absolute = true;

    return p_pic;
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
        vlc_assert_unreachable();
    }
    return VLC_SUCCESS;
}

static int sendKeyEvent(demux_sys_t *p_sys, unsigned int key)
{
    mtime_t now = mdate();
    if (bd_user_input(p_sys->bluray, now, key) < 0)
        return VLC_EGENERIC;

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
    subpicture_region_ChainDelete(p_overlay->p_regions);
    free(p_overlay);
}

static void blurayCloseOverlay(demux_t *p_demux, int plane)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bluray_overlay_t *ov = p_sys->p_overlays[plane];

    if (ov != NULL) {
        if (p_sys->p_vout && ov->i_channel != -1) {
            vout_FlushSubpictureChannel(p_sys->p_vout, ov->i_channel);
        }

        blurayCleanOverlayStruct(ov);
        p_sys->p_overlays[plane] = NULL;
    }

    for (int i = 0; i < MAX_OVERLAY; i++)
        if (p_sys->p_overlays[i])
            return;

    /* All overlays have been closed */
    blurayReleaseVout(p_demux);
}

/*
 * Mark the overlay as "ToDisplay" status.
 * This will not send the overlay to the vout instantly, as the vout
 * may not be acquired (not acquirable) yet.
 * If is has already been acquired, the overlay has already been sent to it,
 * therefore, we only flag the overlay as "Outdated"
 */
static void blurayActivateOverlay(demux_t *p_demux, int plane)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bluray_overlay_t *ov = p_sys->p_overlays[plane];

    /*
     * If the overlay is already displayed, mark the picture as outdated.
     * We must NOT use vout_PutSubpicture if a picture is already displayed.
     */
    vlc_mutex_lock(&ov->lock);
    if (ov->status >= Displayed && p_sys->p_vout) {
        ov->status = Outdated;
        vlc_mutex_unlock(&ov->lock);
        return;
    }

    /*
     * Mark the overlay as available, but don't display it right now.
     * the blurayDemuxMenu will send it to vout, as it may be unavailable when
     * the overlay is computed
     */
    ov->status = ToDisplay;
    vlc_mutex_unlock(&ov->lock);
}

static void blurayInitOverlay(demux_t *p_demux, int plane, int width, int height)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    assert(p_sys->p_overlays[plane] == NULL);

    bluray_overlay_t *ov = calloc(1, sizeof(*ov));
    if (unlikely(ov == NULL))
        return;

    ov->width = width;
    ov->height = height;
    ov->i_channel = -1;

    vlc_mutex_init(&ov->lock);

    p_sys->p_overlays[plane] = ov;
}

/**
 * Destroy every regions in the subpicture.
 * This is done in two steps:
 * - Wiping our private regions list
 * - Flagging the overlay as outdated, so the changes are replicated from
 *   the subpicture_updater_t::pf_update
 * This doesn't destroy the subpicture, as the overlay may be used again by libbluray.
 */
static void blurayClearOverlay(demux_t *p_demux, int plane)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bluray_overlay_t *ov = p_sys->p_overlays[plane];

    vlc_mutex_lock(&ov->lock);

    subpicture_region_ChainDelete(ov->p_regions);
    ov->p_regions = NULL;
    ov->status = Outdated;

    vlc_mutex_unlock(&ov->lock);
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
        video_format_Setup(&fmt, VLC_CODEC_YUVP, ov->w, ov->h, ov->w, ov->h, 1, 1);

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
    for (int y = 0; y < ov->h; y++)
        for (int x = 0; x < ov->w;) {
            plane_t *p = &p_reg->p_picture->p[0];
            memset(&p->p_pixels[y * p->i_pitch + x], img->color, img->len);
            x += img->len;
            img++;
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
    demux_sys_t *p_sys = p_demux->p_sys;

    if (!overlay) {
        msg_Info(p_demux, "Closing overlays.");
        if (p_sys->p_vout)
            for (int i = 0; i < MAX_OVERLAY; i++)
                blurayCloseOverlay(p_demux, i);
        return;
    }

    switch (overlay->cmd) {
    case BD_OVERLAY_INIT:
        msg_Info(p_demux, "Initializing overlay");
        blurayInitOverlay(p_demux, overlay->plane, overlay->w, overlay->h);
        break;
    case BD_OVERLAY_CLOSE:
        blurayClearOverlay(p_demux, overlay->plane);
        blurayCloseOverlay(p_demux, overlay->plane);
        break;
    case BD_OVERLAY_CLEAR:
        blurayClearOverlay(p_demux, overlay->plane);
        break;
    case BD_OVERLAY_FLUSH:
        blurayActivateOverlay(p_demux, overlay->plane);
        break;
    case BD_OVERLAY_DRAW:
        blurayDrawOverlay(p_demux, overlay);
        break;
    default:
        msg_Warn(p_demux, "Unknown BD overlay command: %u", overlay->cmd);
        break;
    }
}

/*
 * ARGB overlay (BD-J)
 */
static void blurayInitArgbOverlay(demux_t *p_demux, int plane, int width, int height)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    blurayInitOverlay(p_demux, plane, width, height);

    if (!p_sys->p_overlays[plane]->p_regions) {
        video_format_t fmt;
        video_format_Init(&fmt, 0);
        video_format_Setup(&fmt, VLC_CODEC_RGBA, width, height, width, height, 1, 1);

        p_sys->p_overlays[plane]->p_regions = subpicture_region_New(&fmt);
    }
}

static void blurayDrawArgbOverlay(demux_t *p_demux, const BD_ARGB_OVERLAY* const ov)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    vlc_mutex_lock(&p_sys->p_overlays[ov->plane]->lock);

    /* Find a region to update */
    subpicture_region_t *p_reg = p_sys->p_overlays[ov->plane]->p_regions;
    if (!p_reg) {
        vlc_mutex_unlock(&p_sys->p_overlays[ov->plane]->lock);
        return;
    }

    /* Now we can update the region */
    const uint32_t *src0 = ov->argb;
    uint8_t        *dst0 = p_reg->p_picture->p[0].p_pixels +
                           p_reg->p_picture->p[0].i_pitch * ov->y +
                           ov->x * 4;

    for (int y = 0; y < ov->h; y++) {
        // XXX: add support for this format ? Should be possible with OPENGL/VDPAU/...
        // - or add libbluray option to select the format ?
        for (int x = 0; x < ov->w; x++) {
            dst0[x*4  ] = src0[x]>>16; /* R */
            dst0[x*4+1] = src0[x]>>8;  /* G */
            dst0[x*4+2] = src0[x];     /* B */
            dst0[x*4+3] = src0[x]>>24; /* A */
        }

        src0 += ov->stride;
        dst0 += p_reg->p_picture->p[0].i_pitch;
    }

    vlc_mutex_unlock(&p_sys->p_overlays[ov->plane]->lock);
    /*
     * /!\ The region is now stored in our internal list, but not in the subpicture /!\
     */
}

static void blurayArgbOverlayProc(void *ptr, const BD_ARGB_OVERLAY *const overlay)
{
    demux_t *p_demux = (demux_t*)ptr;

    switch (overlay->cmd) {
    case BD_ARGB_OVERLAY_INIT:
        blurayInitArgbOverlay(p_demux, overlay->plane, overlay->w, overlay->h);
        break;
    case BD_ARGB_OVERLAY_CLOSE:
        blurayClearOverlay(p_demux, overlay->plane);
        blurayCloseOverlay(p_demux, overlay->plane);
        break;
    case BD_ARGB_OVERLAY_FLUSH:
        blurayActivateOverlay(p_demux, overlay->plane);
        break;
    case BD_ARGB_OVERLAY_DRAW:
        blurayDrawArgbOverlay(p_demux, overlay);
        break;
    default:
        msg_Warn(p_demux, "Unknown BD ARGB overlay command: %u", overlay->cmd);
        break;
    }
}

static void bluraySendOverlayToVout(demux_t *p_demux, bluray_overlay_t *p_ov)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    assert(p_ov != NULL);
 
    subpicture_t *p_pic = bluraySubpictureCreate(p_ov);
    if (!p_pic) {
        return;
    }

    p_pic->i_start = p_pic->i_stop = mdate();
    p_pic->i_channel = vout_RegisterSubpictureChannel(p_sys->p_vout);
    p_ov->i_channel = p_pic->i_channel;

    /*
     * After this point, the picture should not be accessed from the demux thread,
     * as it is held by the vout thread.
     * This must be done only once per subpicture, ie. only once between each
     * blurayInitOverlay & blurayCloseOverlay call.
     */
    vout_PutSubpicture(p_sys->p_vout, p_pic);

    /*
     * Mark the picture as Outdated, as it contains no region for now.
     * This will make the subpicture_updater_t call pf_update
     */
    p_ov->status = Outdated;
}

static void blurayUpdateTitleInfo(input_title_t *t, BLURAY_TITLE_INFO *title_info)
{
    t->i_length = FROM_TICKS(title_info->duration);

    if (!t->i_seekpoint) {
        for (unsigned int j = 0; j < title_info->chapter_count; j++) {
            seekpoint_t *s = vlc_seekpoint_New();
            if (!s) {
                break;
            }
            s->i_time_offset = title_info->chapters[j].offset;

            TAB_APPEND(t->i_seekpoint, t->seekpoint, s);
        }
    }
}

static void blurayInitTitles(demux_t *p_demux, int menu_titles)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* get and set the titles */
    unsigned i_title = menu_titles;

    if (!p_sys->b_menu) {
        i_title = bd_get_titles(p_sys->bluray, TITLES_RELEVANT, 60);
        p_sys->i_longest_title = bd_get_main_title(p_sys->bluray);
    }

    for (unsigned int i = 0; i < i_title; i++) {
        input_title_t *t = vlc_input_title_New();
        if (!t)
            break;

        if (!p_sys->b_menu) {
            BLURAY_TITLE_INFO *title_info = bd_get_title_info(p_sys->bluray, i, 0);
            blurayUpdateTitleInfo(t, title_info);
            bd_free_title_info(title_info);

        } else if (i == 0) {
            t->psz_name = strdup(_("Top Menu"));
        } else if (i == i_title - 1) {
            t->psz_name = strdup(_("First Play"));
        }

        TAB_APPEND(p_sys->i_title, p_sys->pp_title, t);
    }
}

static void blurayResetParser(demux_t *p_demux)
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

    if (!p_sys->p_parser)
        msg_Err(p_demux, "Failed to create TS demuxer");
}

/*****************************************************************************
 * bluraySetTitle: select new BD title
 *****************************************************************************/
static int bluraySetTitle(demux_t *p_demux, int i_title)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if (p_sys->b_menu) {
        if (i_title <= 0) {
            msg_Dbg(p_demux, "Playing TopMenu Title");
        } else if (i_title >= (int)p_sys->i_title - 1) {
            msg_Dbg(p_demux, "Playing FirstPlay Title");
            i_title = BLURAY_TITLE_FIRST_PLAY;
        } else {
            msg_Dbg(p_demux, "Playing Title %i", i_title);
        }

        if (bd_play_title(p_sys->bluray, i_title) == 0) {
            msg_Err(p_demux, "cannot play bd title '%d'", i_title);
            return VLC_EGENERIC;
        }

        return VLC_SUCCESS;
    }

    /* Looking for the main title, ie the longest duration */
    if (i_title < 0)
        i_title = p_sys->i_longest_title;
    else if ((unsigned)i_title > p_sys->i_title)
        return VLC_EGENERIC;

    msg_Dbg(p_demux, "Selecting Title %i", i_title);

    if (bd_select_title(p_sys->bluray, i_title) == 0) {
        msg_Err(p_demux, "cannot select bd title '%d'", i_title);
        return VLC_EGENERIC;
    }

    blurayResetParser(p_demux);

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
         pb_bool = (bool*)va_arg(args, bool *);
         *pb_bool = true;
         break;

    case DEMUX_GET_PTS_DELAY:
        pi_64 = (int64_t*)va_arg(args, int64_t *);
        *pi_64 = INT64_C(1000) * var_InheritInteger(p_demux, "disc-caching");
        break;

    case DEMUX_SET_PAUSE_STATE:
        /* Nothing to do */
        break;

    case DEMUX_SET_TITLE:
    {
        int i_title = (int)va_arg(args, int);
        if (bluraySetTitle(p_demux, i_title) != VLC_SUCCESS)
            return VLC_EGENERIC;
        break;
    }
    case DEMUX_SET_SEEKPOINT:
    {
        int i_chapter = (int)va_arg(args, int);
        bd_seek_chapter(p_sys->bluray, i_chapter);
        p_demux->info.i_update = INPUT_UPDATE_SEEKPOINT;
        break;
    }

    case DEMUX_GET_TITLE_INFO:
    {
        input_title_t ***ppp_title = (input_title_t***)va_arg(args, input_title_t***);
        int *pi_int             = (int*)va_arg(args, int*);
        int *pi_title_offset    = (int*)va_arg(args, int*);
        int *pi_chapter_offset  = (int*)va_arg(args, int*);

        /* */
        *pi_title_offset   = 0;
        *pi_chapter_offset = 0;

        /* Duplicate local title infos */
        *pi_int = p_sys->i_title;
        *ppp_title = malloc(p_sys->i_title * sizeof(input_title_t *));
        for (unsigned int i = 0; i < p_sys->i_title; i++)
            (*ppp_title)[i] = vlc_input_title_Duplicate(p_sys->pp_title[i]);

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
        double *pf_position = (double*)va_arg(args, double *);
        *pf_position = p_demux->info.i_title < (int)p_sys->i_title && CUR_LENGTH > 0 ?
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

        if (meta->thumb_count > 0 && meta->thumbnails) {
            char *psz_thumbpath;
            if (asprintf(&psz_thumbpath, "%s" DIR_SEP "BDMV" DIR_SEP "META" DIR_SEP "DL" DIR_SEP "%s",
                          p_sys->psz_bd_path, meta->thumbnails[0].path) > 0) {
                char *psz_thumburl = vlc_path2uri(psz_thumbpath, "file");
                if (unlikely(psz_thumburl == NULL)) {
                    free(psz_thumbpath);
                    return VLC_ENOMEM;
                }

                vlc_meta_SetArtURL(p_meta, psz_thumburl);
                free(psz_thumburl);
            }
            free(psz_thumbpath);
        }

        return VLC_SUCCESS;
    }

    case DEMUX_NAV_ACTIVATE:
        if (p_sys->b_popup_available && !p_sys->b_menu_open) {
            return sendKeyEvent(p_sys, BD_VK_POPUP);
        }
        return sendKeyEvent(p_sys, BD_VK_ENTER);
    case DEMUX_NAV_UP:
        return sendKeyEvent(p_sys, BD_VK_UP);
    case DEMUX_NAV_DOWN:
        return sendKeyEvent(p_sys, BD_VK_DOWN);
    case DEMUX_NAV_LEFT:
        return sendKeyEvent(p_sys, BD_VK_LEFT);
    case DEMUX_NAV_RIGHT:
        return sendKeyEvent(p_sys, BD_VK_RIGHT);

    case DEMUX_CAN_RECORD:
    case DEMUX_GET_FPS:
    case DEMUX_SET_GROUP:
    case DEMUX_HAS_UNSUPPORTED_META:
    case DEMUX_GET_ATTACHMENTS:
        return VLC_EGENERIC;
    default:
        msg_Warn(p_demux, "unimplemented query (%d) in control", query);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * libbluray event handling
 *****************************************************************************/

static void streamFlush( demux_sys_t *p_sys )
{
    /*
     * MPEG-TS demuxer does not flush last video frame if size of PES packet is unknown.
     * Packet is flushed only when TS packet with PUSI flag set is received.
     *
     * Fix this by emitting (video) ts packet with PUSI flag set.
     * Add video sequence end code to payload so that also video decoder is flushed.
     * Set PES packet size in the payload so that it will be sent to decoder immediately.
     */

    if (p_sys->b_flushed)
        return;

    block_t *p_block = block_Alloc(192);
    if (!p_block)
        return;

    static const uint8_t seq_end_pes[] = {
        0x00, 0x00, 0x01, 0xe0, 0x00, 0x07, 0x80, 0x00, 0x00,  /* PES header */
        0x00, 0x00, 0x01, 0xb7,                                /* PES payload: sequence end */
    };
    static const uint8_t vid_pusi_ts[] = {
        0x00, 0x00, 0x00, 0x00,                /* TP extra header (ATC) */
        0x47, 0x50, 0x11, 0x30,                /* TP header */
        (192 - (4 + 5) - sizeof(seq_end_pes)), /* adaptation field length */
        0x80,                                  /* adaptation field: discontinuity indicator */
    };

    memset(p_block->p_buffer, 0, 192);
    memcpy(p_block->p_buffer, vid_pusi_ts, sizeof(vid_pusi_ts));
    memcpy(p_block->p_buffer + 192 - sizeof(seq_end_pes), seq_end_pes, sizeof(seq_end_pes));
    p_block->i_buffer = 192;

    /* set correct sequence end code */
    vlc_mutex_lock(&p_sys->pl_info_lock);
    if (p_sys->p_clip_info != NULL) {
        if (p_sys->p_clip_info->video_streams[0].coding_type > 2) {
            /* VC1 / H.264 sequence end */
            p_block->p_buffer[191] = 0x0a;
        }
    }
    vlc_mutex_unlock(&p_sys->pl_info_lock);

    stream_DemuxSend(p_sys->p_parser, p_block);
    p_sys->b_flushed = true;
}

static void blurayResetStillImage( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if (p_sys->i_still_end_time) {
        p_sys->i_still_end_time = 0;

        blurayResetParser(p_demux);
        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
    }
}

static void blurayStillImage( demux_t *p_demux, unsigned i_timeout )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* time period elapsed ? */
    if (p_sys->i_still_end_time > 0 && p_sys->i_still_end_time <= mdate()) {
        msg_Dbg(p_demux, "Still image end");
        bd_read_skip_still(p_sys->bluray);

        blurayResetStillImage(p_demux);
        return;
    }

    /* show last frame as still image */
    if (!p_sys->i_still_end_time) {
        if (i_timeout) {
            msg_Dbg(p_demux, "Still image (%d seconds)", i_timeout);
            p_sys->i_still_end_time = mdate() + i_timeout * CLOCK_FREQ;
        } else {
            msg_Dbg(p_demux, "Still image (infinite)");
            p_sys->i_still_end_time = -1;
        }

        /* flush demuxer and decoder (there won't be next video packet starting with ts PUSI) */
        streamFlush(p_sys);

        /* stop buffering */
        bool b_empty;
        es_out_Control( p_demux->out, ES_OUT_GET_EMPTY, &b_empty );
    }

    /* avoid busy loops (read returns no data) */
    msleep( 40000 );
}

static void blurayStreamSelect(demux_t *p_demux, uint32_t i_type, uint32_t i_id)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_pid = -1;

    vlc_mutex_lock(&p_sys->pl_info_lock);

    if (p_sys->p_clip_info) {

        /* The param we get is the real stream id, not an index, ie. it starts from 1 */
        i_id--;
        if (i_type == BD_EVENT_AUDIO_STREAM) {
            if (i_id < p_sys->p_clip_info->audio_stream_count) {
                i_pid = p_sys->p_clip_info->audio_streams[i_id].pid;
                p_sys->i_audio_stream = i_pid;
            }
        } else if (i_type == BD_EVENT_PG_TEXTST_STREAM) {
            if (i_id < p_sys->p_clip_info->pg_stream_count) {
                i_pid = p_sys->p_clip_info->pg_streams[i_id].pid;
                p_sys->i_spu_stream = i_pid;
            }
        }
    }

    vlc_mutex_unlock(&p_sys->pl_info_lock);

    if (i_pid > 0) {
        int i_idx = findEsPairIndex(p_sys, i_pid);
        if (i_idx >= 0) {
            es_out_id_t *p_es = vlc_array_item_at_index(&p_sys->es, i_idx);
            es_out_Control(p_demux->out, ES_OUT_SET_ES, p_es);
        }
    }
}

static void blurayUpdatePlaylist(demux_t *p_demux, unsigned i_playlist)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    blurayResetParser(p_demux);

    /* read title info and init some values */
    if (!p_sys->b_menu)
        p_demux->info.i_title = bd_get_current_title(p_sys->bluray);
    p_demux->info.i_seekpoint = 0;
    p_demux->info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;

    BLURAY_TITLE_INFO *p_title_info = bd_get_playlist_info(p_sys->bluray, i_playlist, 0);
    if (p_title_info) {
        blurayUpdateTitleInfo(p_sys->pp_title[p_demux->info.i_title], p_title_info);
    }
    setTitleInfo(p_sys, p_title_info);

    blurayResetStillImage(p_demux);
}

static void blurayUpdateCurrentClip(demux_t *p_demux, uint32_t clip)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    vlc_mutex_lock(&p_sys->pl_info_lock);

    p_sys->p_clip_info = NULL;
    p_sys->i_video_stream = -1;

    if (p_sys->p_pl_info && clip < p_sys->p_pl_info->clip_count) {

        p_sys->p_clip_info = &p_sys->p_pl_info->clips[clip];

    /* Let's assume a single video track for now.
     * This may brake later, but it's enough for now.
     */
        assert(p_sys->p_clip_info->video_stream_count >= 1);
        p_sys->i_video_stream = p_sys->p_clip_info->video_streams[0].pid;
    }

    vlc_mutex_unlock(&p_sys->pl_info_lock);

    blurayResetStillImage(p_demux);
}

static void blurayHandleEvent(demux_t *p_demux, const BD_EVENT *e)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    switch (e->event) {
    case BD_EVENT_TITLE:
        if (e->param == BLURAY_TITLE_FIRST_PLAY)
            p_demux->info.i_title = p_sys->i_title - 1;
        else
            p_demux->info.i_title = e->param;
        /* this is feature title, we don't know yet which playlist it will play (if any) */
        setTitleInfo(p_sys, NULL);
        /* reset title infos here ? */
        p_demux->info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT; /* might be BD-J title with no video */
        break;
    case BD_EVENT_PLAYLIST:
        /* Start of playlist playback (?????.mpls) */
        blurayUpdatePlaylist(p_demux, e->param);
        break;
    case BD_EVENT_PLAYITEM:
        blurayUpdateCurrentClip(p_demux, e->param);
        break;
    case BD_EVENT_CHAPTER:
        p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
        p_demux->info.i_seekpoint = e->param;
        break;
    case BD_EVENT_ANGLE:
        break;
    case BD_EVENT_MENU:
        p_sys->b_menu_open = e->param;
        break;
    case BD_EVENT_POPUP:
        p_sys->b_popup_available = e->param;
        /* TODO: show / hide pop-up menu button in gui ? */
        break;

    /*
     * stream selection events
     */
    case BD_EVENT_AUDIO_STREAM:
    case BD_EVENT_PG_TEXTST_STREAM:
        blurayStreamSelect(p_demux, e->event, e->param);
        break;
    case BD_EVENT_IG_STREAM:
        break;

    /*
     * playback control events
     */
    case BD_EVENT_STILL_TIME:
        blurayStillImage(p_demux, e->param);
        break;
    case BD_EVENT_DISCONTINUITY:
        /* reset demuxer (partially decoded PES packets must be dropped) */
        blurayResetParser(p_demux);
        break;
    case BD_EVENT_IDLE:
        /* nothing to do (ex. BD-J is preparing menus, waiting user input or running animation) */
        /* avoid busy loop (bd_read() returns no data) */
        msleep( 40000 );
        break;

    default:
        msg_Warn(p_demux, "event: %d param: %d", e->event, e->param);
        break;
    }
}

#define BD_TS_PACKET_SIZE (192)
#define NB_TS_PACKETS (200)

static int blurayDemux(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    BD_EVENT e;

    block_t *p_block = block_Alloc(NB_TS_PACKETS * (int64_t)BD_TS_PACKET_SIZE);
    if (!p_block)
        return -1;

    int nread;

    if (p_sys->b_menu == false) {
        while (bd_get_event(p_sys->bluray, &e))
            blurayHandleEvent(p_demux, &e);

        nread = bd_read(p_sys->bluray, p_block->p_buffer,
                        NB_TS_PACKETS * BD_TS_PACKET_SIZE);
    } else {
        nread = bd_read_ext(p_sys->bluray, p_block->p_buffer,
                            NB_TS_PACKETS * BD_TS_PACKET_SIZE, &e);
        while (e.event != BD_EVENT_NONE) {
            blurayHandleEvent(p_demux, &e);
            bd_get_event(p_sys->bluray, &e);
        }
    }

    for (int i = 0; i < MAX_OVERLAY; i++) {
        bluray_overlay_t *ov = p_sys->p_overlays[i];
        if (!ov) {
            continue;
        }
        vlc_mutex_lock(&ov->lock);
        bool display = ov->status == ToDisplay;
        vlc_mutex_unlock(&ov->lock);
        if (display) {
            if (p_sys->p_vout == NULL)
                p_sys->p_vout = input_GetVout(p_demux->p_input);
            if (p_sys->p_vout != NULL) {
                var_AddCallback(p_sys->p_vout, "mouse-moved", onMouseEvent, p_demux);
                var_AddCallback(p_sys->p_vout, "mouse-clicked", onMouseEvent, p_demux);
                bluraySendOverlayToVout(p_demux, ov);
            }
        }
    }

    if (nread <= 0) {
        block_Release(p_block);
        if (nread < 0)
            return -1;
        return 1;
    }

    p_block->i_buffer = nread;

    stream_DemuxSend(p_sys->p_parser, p_block);

    p_sys->b_flushed = false;

    return 1;
}
