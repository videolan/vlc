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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>                      /* demux_t */
#include <vlc_input.h>                      /* Seekpoints, chapters */
#include <vlc_dialog.h>                     /* BD+/AACS warnings */

#include <libbluray/bluray.h>
#include <libbluray/meta_data.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Callbacks */
static int  blurayOpen ( vlc_object_t * );
static void blurayClose( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("BluRay") )
    set_description( N_("Blu-Ray Disc support (libbluray)") )

    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_capability( "access_demux", 200)

    add_shortcut( "bluray", "file" )

    set_callbacks( blurayOpen, blurayClose )
vlc_module_end ()


struct demux_sys_t
{
    BLURAY         *bluray;

    /* Titles */
    unsigned int    i_title;
    unsigned int    i_longest_title;
    input_title_t **pp_title;

    /* TS stream */
    stream_t       *p_parser;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     blurayControl(demux_t *, int, va_list);
static int     blurayDemux  (demux_t *);

static int     blurayInitTitles(demux_t *p_demux );
static int     bluraySetTitle(demux_t *p_demux, int i_title);

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
    char bd_path[PATH_MAX];
    const char *error_msg = NULL;

    if (strcmp(p_demux->psz_access, "bluray")) {
        // TODO BDMV support, once we figure out what to do in libbluray
        return VLC_EGENERIC;
    }

    /* */
    p_demux->p_sys = p_sys = malloc(sizeof(*p_sys));
    if (unlikely(!p_sys)) {
        return VLC_ENOMEM;
    }
    p_sys->p_parser = NULL;

    /* init demux info fields */
    p_demux->info.i_update    = 0;
    p_demux->info.i_title     = 0;
    p_demux->info.i_seekpoint = 0;

    TAB_INIT( p_sys->i_title, p_sys->pp_title );

    /* store current bd_path */
    strncpy(bd_path, p_demux->psz_file, sizeof(bd_path));
    bd_path[PATH_MAX - 1] = '\0';

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

    if (p_sys->p_parser)
        stream_Delete(p_sys->p_parser);

    /* Titles */
    for (unsigned int i = 0; i < p_sys->i_title; i++)
        vlc_input_title_Delete(p_sys->pp_title[i]);
    TAB_CLEAN( p_sys->i_title, p_sys->pp_title );

    /* bd_close( NULL ) can crash */
    assert(p_sys->bluray);
    bd_close(p_sys->bluray);
    free(p_sys);
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

    /* read title info and init some values */
    p_demux->info.i_title = i_title;
    p_demux->info.i_seekpoint = 0;
    p_demux->info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;

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
            *pi_length = CUR_LENGTH;
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
            *pf_position = (double)FROM_TICKS(bd_tell_time(p_sys->bluray))/CUR_LENGTH;
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
            struct meta_dl *meta = bd_get_meta(p_sys->bluray);
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


#define BD_TS_PACKET_SIZE (192)
#define NB_TS_PACKETS (200)

static int blurayDemux(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    block_t *p_block = block_New(p_demux, NB_TS_PACKETS * (int64_t)BD_TS_PACKET_SIZE);
    if (!p_block) {
        return -1;
    }

    int nread = bd_read(p_sys->bluray, p_block->p_buffer,
                        NB_TS_PACKETS * BD_TS_PACKET_SIZE);
    if (nread < 0) {
        block_Release(p_block);
        return nread;
    }

    p_block->i_buffer = nread;

    stream_DemuxSend( p_sys->p_parser, p_block );

    return 1;
}
