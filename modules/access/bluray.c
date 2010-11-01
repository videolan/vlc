/*****************************************************************************
 * bluray.c: Blu-ray disc support plugin
 *****************************************************************************
 * Copyright (C) 2010 VideoLAN, VLC authors and libbluray AUTHORS
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#include <assert.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_messages.h>
#include <vlc_input.h>
#include <vlc_dialog.h>

#include <libbluray/bluray.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( "Caching value for BDs. This "\
                             "value should be set in milliseconds." )

/* Callbacks */
static int  blurayOpen ( vlc_object_t * );
static void blurayClose( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("BluRay") )
    set_description( N_("Blu-Ray Disc support (libbluray)") )

    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_capability( "access", 60 )

    add_integer( "bluray-caching", 1000,
        CACHING_TEXT, CACHING_LONGTEXT, true )

    add_shortcut( "bluray" )
    add_shortcut( "file" )

    set_callbacks( blurayOpen, blurayClose )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

struct access_sys_t
{
    BLURAY *bluray; /* */

    /* Titles */
    unsigned int i_title;
    unsigned int i_longest_title;
    input_title_t   **pp_title;

    int i_bd_delay;
};

static ssize_t blurayRead   (access_t *, uint8_t *, size_t);
static int     bluraySeek   (access_t *, uint64_t);
static int     blurayControl(access_t *, int, va_list);
static int     blurayInitTitles(access_t *p_access );
static int     bluraySetTitle(access_t *p_access, int i_title);

/*****************************************************************************
 * blurayOpen: module init function
 *****************************************************************************/
static int blurayOpen( vlc_object_t *object )
{
    access_t *p_access = (access_t*)object;

    access_sys_t *p_sys;
    char *pos_title;
    int i_title = -1;
    char bd_path[PATH_MAX];

    if( strcmp( p_access->psz_access, "bluray" ) ) {
        // TODO BDMV support, once we figure out what to do in libbluray
        return VLC_EGENERIC;
    }

    /* init access fields */
    access_InitFields(p_access);

    /* register callback function for communication */
    ACCESS_SET_CALLBACKS(blurayRead, NULL, blurayControl, bluraySeek);

    p_access->p_sys = p_sys = malloc(sizeof(access_sys_t));
    if (unlikely(!p_sys)) {
        return VLC_ENOMEM;
    }

    TAB_INIT( p_sys->i_title, p_sys->pp_title );

    /* store current bd_path */
    strncpy(bd_path, p_access->psz_location, sizeof(bd_path));
    bd_path[PATH_MAX - 1] = '\0';

    p_sys->bluray = bd_open(bd_path, NULL);
    if ( !p_sys->bluray ) {
        free(p_sys);
        return VLC_EGENERIC;
    }

    /* Warning the user about AACS/BD+ */
    const BLURAY_DISC_INFO *disc_info = bd_get_disc_info(p_sys->bluray);
    msg_Dbg (p_access, "First play: %i, Top menu: %i\n"
                       "HDMV Titles: %i, BDJ Titles: %i, Other: %i",
             disc_info->first_play_supported, disc_info->top_menu_supported,
             disc_info->num_hdmv_titles, disc_info->num_bdj_titles,
             disc_info->num_unsupported_titles);

    /* AACS */
    if (disc_info->aacs_detected) {
        if (!disc_info->libaacs_detected) {
            dialog_Fatal (p_access, _("Blu-Ray error"),
                    _("This Blu-Ray Disc needs a library for AACS decoding, "
                      "and your system does not have it."));
            blurayClose(object);
            return VLC_EGENERIC;
        }
        if (!disc_info->aacs_handled) {
            dialog_Fatal (p_access, _("Blu-Ray error"),
                    _("Your system AACS decoding library does not work. "
                      "Missing keys?"));
            blurayClose(object);
            return VLC_EGENERIC;
        }
    }

    /* BD+ */
    if (disc_info->bdplus_detected) {
        if (!disc_info->libbdplus_detected) {
            dialog_Fatal (p_access, _("Blu-Ray error"),
                    _("This Blu-Ray Disc needs a library for BD+ decoding, "
                      "and your system does not have it."));
            blurayClose(object);
            return VLC_EGENERIC;
        }
        if (!disc_info->bdplus_handled) {
            dialog_Fatal (p_access, _("Blu-Ray error"),
                    _("Your system BD+ decoding library does not work. "
                      "Missing configuration?"));
            blurayClose(object);
            return VLC_EGENERIC;
        }
    }

    /* Get titles and chapters */
    if (blurayInitTitles(p_access) != VLC_SUCCESS) {
        blurayClose(object);
        return VLC_EGENERIC;
    }

    /* get title request */
    if ( (pos_title = strrchr(bd_path, ':')) ) {
        /* found character ':' for title information */
        *(pos_title++) = '\0';
        i_title = atoi(pos_title);
    }

    /* set start title number */
    if ( bluraySetTitle(p_access, i_title) != VLC_SUCCESS ) {
        msg_Err( p_access, "Could not set the title %d", i_title );
        blurayClose(object);
        return VLC_EGENERIC;
    }

    p_sys->i_bd_delay = var_InheritInteger(p_access, "bluray-caching");

    return VLC_SUCCESS;
}


/*****************************************************************************
 * blurayClose: module destroy function
 *****************************************************************************/
static void blurayClose( vlc_object_t *object )
{
    access_t *p_access = (access_t*)object;
    access_sys_t *p_sys = p_access->p_sys;

    /* Titles */
    for (unsigned int i = 0; i < p_sys->i_title; i++)
        vlc_input_title_Delete(p_sys->pp_title[i]);
    TAB_CLEAN( p_sys->i_title, p_sys->pp_title );

    /* bd_close( NULL ) can crash */
    assert(p_sys->bluray);
    bd_close(p_sys->bluray);
    free(p_sys);
}

static int blurayInitTitles(access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    /* get and set the titles */
    unsigned i_title = bd_get_titles(p_sys->bluray, TITLES_RELEVANT);
    int64_t duration = 0;

    for (unsigned int i = 0; i < i_title; i++) {
        input_title_t *t = vlc_input_title_New();
        if (!t)
            break;

        BLURAY_TITLE_INFO *title_info = bd_get_title_info(p_sys->bluray,i);
        if (!title_info)
            break;
        t->i_length = title_info->duration * CLOCK_FREQ / INT64_C(90000);

        if (t->i_length > duration) {
            duration = t->i_length;
            p_sys->i_longest_title = i;
        }

        for ( unsigned int j = 0; j < title_info->chapter_count; j++) {
            seekpoint_t *s = vlc_seekpoint_New();
            if( !s )
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
static int bluraySetTitle(access_t *p_access, int i_title)
{
    access_sys_t *p_sys = p_access->p_sys;

    /* Looking for the main title, ie the longest duration */
    if (i_title == -1)
        i_title = p_sys->i_longest_title;

    msg_Dbg( p_access, "Selecting Title %i", i_title);

    /* Select Blu-Ray title */
    if ( bd_select_title(p_access->p_sys->bluray, i_title) == 0 ) {
        msg_Err( p_access, "cannot select bd title '%d'", p_access->info.i_title);
        return VLC_EGENERIC;
    }

    /* read title length and init some values */
    p_access->info.i_title = i_title;
    p_access->info.i_size  = bd_get_title_size(p_sys->bluray);
    p_access->info.i_pos   = 0;
    p_access->info.b_eof   = false;
    p_access->info.i_seekpoint = 0;
    p_access->info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;

    return VLC_SUCCESS;
}


/*****************************************************************************
 * blurayControl: handle the controls
 *****************************************************************************/
static int blurayControl(access_t *p_access, int query, va_list args)
{
    access_sys_t *p_sys = p_access->p_sys;
    bool     *pb_bool;
    int64_t  *pi_64;

    switch (query) {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
             pb_bool = (bool*)va_arg( args, bool * );
             *pb_bool = true;
             break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = p_sys->i_bd_delay;
            break;

        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_SET_TITLE:
        {
            int i_title = (int)va_arg( args, int );
            if( bluraySetTitle( p_access, i_title ) != VLC_SUCCESS )
                return VLC_EGENERIC;
            break;
        }
        case ACCESS_SET_SEEKPOINT:
        {
            int i_chapter = (int)va_arg( args, int );
            bd_seek_chapter( p_sys->bluray, i_chapter );
            p_access->info.i_update = INPUT_UPDATE_SEEKPOINT;
            break;
        }

        case ACCESS_GET_TITLE_INFO:
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
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
        case ACCESS_GET_META:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query (%d) in control", query );
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


/*****************************************************************************
 * bluraySeek: seek to the given position
 *****************************************************************************/
static int bluraySeek(access_t *p_access, uint64_t position)
{
    access_sys_t *p_sys = p_access->p_sys;

    p_access->info.i_pos = bd_seek(p_sys->bluray, position);
    p_access->info.b_eof = false;

    return VLC_SUCCESS;
}


/*****************************************************************************
 * blurayRead: read BD data into buffer
 *****************************************************************************/
static ssize_t blurayRead(access_t *p_access, uint8_t *data, size_t size)
{
    access_sys_t *p_sys = p_access->p_sys;
    int nread;

    if (p_access->info.b_eof) {
        return 0;
    }

    /* read data into buffer with given length */
    nread = bd_read(p_sys->bluray, data, size);

    if( nread == 0 ) {
        p_access->info.b_eof = true;
    }
    else if( nread > 0 ) {
        p_access->info.i_pos += nread;
    }

    return nread;
}

