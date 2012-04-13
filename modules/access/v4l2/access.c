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
 * it under the terms of the GNU Lesser General Public License as published by
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
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_fs.h>

#include "v4l2.h"

struct access_sys_t
{
    int fd;
    uint32_t block_flags;
    union
    {
        uint32_t bufc;
        uint32_t blocksize;
    };
    struct buffer_t *bufv;
    vlc_v4l2_ctrl_t *controls;
};

static block_t *AccessRead( access_t * );
static ssize_t AccessReadStream( access_t *, uint8_t *, size_t );
static int AccessControl( access_t *, int, va_list );
static int InitVideo(access_t *, int);

int AccessOpen( vlc_object_t *obj )
{
    access_t *access = (access_t *)obj;

    access_InitFields( access );

    access_sys_t *sys = calloc (1, sizeof (*sys));
    if( unlikely(sys == NULL) )
        return VLC_ENOMEM;
    access->p_sys = sys;

    ParseMRL( obj, access->psz_location );

    char *path = var_InheritString (obj, CFG_PREFIX"dev");
    if (unlikely(path == NULL))
        goto error; /* probably OOM */
    msg_Dbg (obj, "opening device '%s'", path);

    int rawfd = vlc_open (path, O_RDWR);
    if (rawfd == -1)
    {
        msg_Err (obj, "cannot open device '%s': %m", path);
        free (path);
        goto error;
    }
    free (path);

    int fd = v4l2_fd_open (rawfd, 0);
    if (fd == -1)
    {
        msg_Warn (obj, "cannot initialize user-space library: %m");
        /* fallback to direct kernel mode anyway */
        fd = rawfd;
    }
    sys->fd = fd;

    if (InitVideo (access, fd))
    {
        v4l2_close (fd);
        goto error;
    }

    access->pf_seek = NULL;
    access->pf_control = AccessControl;
    return VLC_SUCCESS;
error:
    free (sys);
    return VLC_EGENERIC;
}

int InitVideo (access_t *access, int fd)
{
    access_sys_t *sys = access->p_sys;

    /* Get device capabilites */
    struct v4l2_capability cap;
    if (v4l2_ioctl (fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        msg_Err (access, "cannot get device capabilities: %m");
        return -1;
    }

    msg_Dbg (access, "device %s using driver %s (version %u.%u.%u) on %s",
             cap.card, cap.driver, (cap.version >> 16) & 0xFF,
             (cap.version >> 8) & 0xFF, cap.version & 0xFF, cap.bus_info);
    msg_Dbg (access, "the device has the capabilities: 0x%08X",
             cap.capabilities);
    msg_Dbg (access, " (%c) Video Capture, (%c) Audio, (%c) Tuner, (%c) Radio",
             (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE  ? 'X':' '),
             (cap.capabilities & V4L2_CAP_AUDIO  ? 'X':' '),
             (cap.capabilities & V4L2_CAP_TUNER  ? 'X':' '),
             (cap.capabilities & V4L2_CAP_RADIO  ? 'X':' '));
    msg_Dbg (access, " (%c) Read/Write, (%c) Streaming, (%c) Asynchronous",
             (cap.capabilities & V4L2_CAP_READWRITE ? 'X':' '),
             (cap.capabilities & V4L2_CAP_STREAMING ? 'X':' '),
             (cap.capabilities & V4L2_CAP_ASYNCIO ? 'X':' '));

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        msg_Err (access, "not a video capture device");
        return -1;
    }

    if (SetupInput (VLC_OBJECT(access), fd))
        return -1;

    sys->controls = ControlsInit (VLC_OBJECT(access), fd);

    /* Try and find default resolution if not specified */
    struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    if (v4l2_ioctl (fd, VIDIOC_G_FMT, &fmt) < 0)
    {
        msg_Err (access, "cannot get default format: %m");
        return -1;
    }

    /* Print extra info */
    msg_Dbg (access, "%d bytes maximum for complete image",
             fmt.fmt.pix.sizeimage );
    /* Check interlacing */
    switch (fmt.fmt.pix.field)
    {
        case V4L2_FIELD_INTERLACED:
            msg_Dbg (access, "Interlacing setting: interleaved");
            /*if (NTSC)
                sys->block_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            else*/
                sys->block_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case V4L2_FIELD_INTERLACED_TB:
            msg_Dbg (access, "Interlacing setting: interleaved top bottom" );
            sys->block_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case V4L2_FIELD_INTERLACED_BT:
            msg_Dbg (access, "Interlacing setting: interleaved bottom top" );
            sys->block_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            break;
        default:
            break;
    }

    /* Init I/O method */
    if (cap.capabilities & V4L2_CAP_STREAMING)
    {
        sys->bufc = 4;
        sys->bufv = StartMmap (VLC_OBJECT(access), fd, &sys->bufc);
        if (sys->bufv == NULL)
            return -1;
        access->pf_block = AccessRead;
    }
    else if (cap.capabilities & V4L2_CAP_READWRITE)
    {
        sys->blocksize = fmt.fmt.pix.sizeimage;
        sys->bufv = NULL;
        access->pf_read = AccessReadStream;
    }
    else
    {
        msg_Err (access, "no supported I/O method");
        return -1;
    }
    return 0;
}

void AccessClose( vlc_object_t *obj )
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = access->p_sys;

    if (sys->bufv != NULL)
        StopMmap (sys->fd, sys->bufv, sys->bufc);
    ControlsDeinit( obj, sys->controls );
    v4l2_close (sys->fd);
    free( sys );
}

static block_t *AccessRead( access_t *access )
{
    access_sys_t *sys = access->p_sys;

    struct pollfd fd;
    fd.fd = sys->fd;
    fd.events = POLLIN;
    fd.revents = 0;

    /* Wait for data */
    /* FIXME: kill timeout */
    if( poll( &fd, 1, 500 ) <= 0 )
        return NULL;

    block_t *block = GrabVideo (VLC_OBJECT(access), sys->fd, sys->bufv);
    if( block != NULL )
    {
        block->i_pts = block->i_dts = mdate();
        block->i_flags |= sys->block_flags;
    }
    return block;
}

static ssize_t AccessReadStream( access_t *access, uint8_t *buf, size_t len )
{
    access_sys_t *sys = access->p_sys;
    struct pollfd ufd;
    int i_ret;

    ufd.fd = sys->fd;
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

    i_ret = v4l2_read (sys->fd, buf, len);
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
