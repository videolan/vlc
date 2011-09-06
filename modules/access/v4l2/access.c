/*****************************************************************************
 * access.c : V4L2 compressed byte stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2011 the VideoLAN team
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Richard Hosking <richard at hovis dot net>
 *          Antoine Cellerier <dionoea at videolan d.t org>
 *          Dennis Lou <dlou99 at yahoo dot com>
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

#include "v4l2.h"
#include <vlc_access.h>

#include <errno.h>
#include <poll.h>

static block_t *AccessRead( access_t * );
static ssize_t AccessReadStream( access_t *, uint8_t *, size_t );
static int AccessControl( access_t *, int, va_list );

int AccessOpen( vlc_object_t *obj )
{
    access_t *access = (access_t *)obj;

    /* Only when selected */
    if( *access->psz_access == '\0' ) return VLC_EGENERIC;

    access_InitFields( access );

    demux_sys_t *sys = calloc( 1, sizeof( demux_sys_t ));
    if( unlikely(sys == NULL) )
        return VLC_ENOMEM;
    access->p_sys = (access_sys_t *)sys;

    ParseMRL( obj, access->psz_location );
    sys->i_fd = OpenVideo( obj, sys, false );
    if( sys->i_fd == -1 )
    {
        free( sys );
        return VLC_EGENERIC;
    }

    if( sys->io == IO_METHOD_READ )
        access->pf_read = AccessReadStream;
    else
        access->pf_block = AccessRead;
    access->pf_seek = NULL;
    access->pf_control = AccessControl;
    return VLC_SUCCESS;
}

void AccessClose( vlc_object_t *obj )
{
    access_t *access = (access_t *)obj;
    demux_sys_t *sys = (demux_sys_t *)access->p_sys;

    v4l2_close( sys->i_fd );
    free( sys );
}

static block_t *AccessRead( access_t *access )
{
    demux_sys_t *sys = (demux_sys_t *)access->p_sys;

    struct pollfd fd;
    fd.fd = sys->i_fd;
    fd.events = POLLIN|POLLPRI;
    fd.revents = 0;

    /* Wait for data */
    /* FIXME: kill timeout */
    if( poll( &fd, 1, 500 ) <= 0 )
        return NULL;

    block_t *block = GrabVideo( VLC_OBJECT(access), sys );
    if( block != NULL )
    {
        block->i_pts = block->i_dts = mdate();
        block->i_flags |= sys->i_block_flags;
    }
    return block;
}

static ssize_t AccessReadStream( access_t *access, uint8_t *buf, size_t len )
{
    demux_sys_t *sys = (demux_sys_t *)access->p_sys;
    struct pollfd ufd;
    int i_ret;

    ufd.fd = sys->i_fd;
    ufd.events = POLLIN;

    if( access->info.b_eof )
        return 0;

    /* FIXME: kill timeout and vlc_object_alive() */
    do
    {
        if( !vlc_object_alive(access) )
            return 0;

        ufd.revents = 0;
    }
    while( ( i_ret = poll( &ufd, 1, 500 ) ) == 0 );

    if( i_ret < 0 )
    {
        if( errno != EINTR )
            msg_Err( access, "poll error: %m" );
        return -1;
    }

    i_ret = v4l2_read( sys->i_fd, buf, len );
    if( i_ret == 0 )
        access->info.b_eof = true;
    else if( i_ret > 0 )
        access->info.i_pos += i_ret;

    return i_ret;
}

static int AccessControl( access_t *access, int query, va_list args )
{
    switch( query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = false;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            *va_arg(args,int64_t *) = INT64_C(1000)
                * var_InheritInteger( access, "live-caching" );
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
        case ACCESS_GET_META:
            return VLC_EGENERIC;

        default:
            msg_Warn( access, "Unimplemented query %d in control", query );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}
