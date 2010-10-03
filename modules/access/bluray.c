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

    add_integer( "bluray-caching", 1000, NULL,
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
    int i_bd_delay;
};

static ssize_t blurayRead   (access_t *, uint8_t *, size_t);
static int     bluraySeek   (access_t *, uint64_t);
static int     blurayControl(access_t *, int, va_list);
static int     bluraySetTitle(access_t *p_access, int i_tile);

/*****************************************************************************
 * blurayOpen: module init function
 *****************************************************************************/
static int blurayOpen( vlc_object_t *object )
{
    access_t *p_access = (access_t*)object;

    access_sys_t *p_sys;
    char *pos_title;
    int i_title = 0;
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

    /* store current bd_path */
    strncpy(bd_path, p_access->psz_location, sizeof(bd_path));
    bd_path[PATH_MAX - 1] = '\0';

    if ( (pos_title = strrchr(bd_path, ':')) ) {
        /* found character ':' for title information */
        *(pos_title++) = '\0';
        i_title = atoi(pos_title);
    }

    p_sys->bluray = bd_open(bd_path, NULL);
    if ( !p_sys->bluray ) {
        free(p_sys);
        return VLC_EGENERIC;
    }

    /* set start title number */
    if ( bluraySetTitle(p_access, i_title) != VLC_SUCCESS ) {
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

    /* bd_close( NULL ) can crash */
    assert(p_sys->bluray);
    bd_close(p_sys->bluray);
    free(p_sys);
}


/*****************************************************************************
 * bluraySetTitle: select new BD title
 *****************************************************************************/
static int bluraySetTitle(access_t *p_access, int i_title)
{
    access_sys_t *p_sys = p_access->p_sys;

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
            break;
        }
        case ACCESS_GET_META:
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
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

