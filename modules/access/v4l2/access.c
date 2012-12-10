/*****************************************************************************
 * access.c : V4L2 compressed byte stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2011 VLC authors and VideoLAN
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Richard Hosking <richard at hovis dot net>
 *          Antoine Cellerier <dionoea at videolan d.t org>
 *          Dennis Lou <dlou99 at yahoo dot com>
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
#include <errno.h>
#include <poll.h>

#include <vlc_common.h>
#include <vlc_access.h>

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

static block_t *MMapBlock (access_t *);
static block_t *ReadBlock (access_t *);
static int AccessControl( access_t *, int, va_list );
static int InitVideo(access_t *, int, uint32_t);

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

    uint32_t caps;
    int fd = OpenDevice (obj, path, &caps);
    free (path);
    if (fd == -1)
        goto error;
    sys->fd = fd;

    if (InitVideo (access, fd, caps))
    {
        v4l2_close (fd);
        goto error;
    }

    sys->controls = ControlsInit (VLC_OBJECT(access), fd);
    access->pf_seek = NULL;
    access->pf_control = AccessControl;
    return VLC_SUCCESS;
error:
    free (sys);
    return VLC_EGENERIC;
}

int InitVideo (access_t *access, int fd, uint32_t caps)
{
    access_sys_t *sys = access->p_sys;

    if (!(caps & V4L2_CAP_VIDEO_CAPTURE))
    {
        msg_Err (access, "not a video capture device");
        return -1;
    }

    v4l2_std_id std;
    if (SetupInput (VLC_OBJECT(access), fd, &std))
        return -1;

    /* NOTE: The V4L access_demux expects a VLC FOURCC as "chroma". It is used to set the
     * es_format_t structure correctly. However, the V4L access (*here*) has no use for a
     * VLC FOURCC and expects a V4L2 format directly instead. That is confusing :-( */
    uint32_t pixfmt = 0;
    char *fmtstr = var_InheritString (access, CFG_PREFIX"chroma");
    if (fmtstr != NULL && strlen (fmtstr) <= 4)
    {
        memcpy (&pixfmt, fmtstr, strlen (fmtstr));
        free (fmtstr);
    }
    else
    /* Use the default^Wprevious format if none specified */
    {
        struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
        if (v4l2_ioctl (fd, VIDIOC_G_FMT, &fmt) < 0)
        {
            msg_Err (access, "cannot get default format: %m");
            return -1;
        }
        pixfmt = fmt.fmt.pix.pixelformat;
    }
    msg_Dbg (access, "selected format %4.4s", (const char *)&pixfmt);

    struct v4l2_format fmt;
    struct v4l2_streamparm parm;
    if (SetupFormat (access, fd, pixfmt, &fmt, &parm))
        return -1;

    msg_Dbg (access, "%"PRIu32" bytes for complete image", fmt.fmt.pix.sizeimage);
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
    if (caps & V4L2_CAP_STREAMING)
    {
        sys->bufc = 4;
        sys->bufv = StartMmap (VLC_OBJECT(access), fd, &sys->bufc);
        if (sys->bufv == NULL)
            return -1;
        access->pf_block = MMapBlock;
    }
    else if (caps & V4L2_CAP_READWRITE)
    {
        sys->blocksize = fmt.fmt.pix.sizeimage;
        sys->bufv = NULL;
        access->pf_block = ReadBlock;
    }
    else
    {
        msg_Err (access, "no supported capture method");
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

/* Wait for data */
static int AccessPoll (access_t *access)
{
    access_sys_t *sys = access->p_sys;
    struct pollfd ufd;

    ufd.fd = sys->fd;
    ufd.events = POLLIN;

    switch (poll (&ufd, 1, 500))
    {
        case -1:
            if (errno == EINTR)
        case 0:
            /* FIXME: kill this case (arbitrary timeout) */
                return -1;
            msg_Err (access, "poll error: %m");
            access->info.b_eof = true;
            return -1;
    }
    return 0;
}


static block_t *MMapBlock (access_t *access)
{
    access_sys_t *sys = access->p_sys;

    if (AccessPoll (access))
        return NULL;

    block_t *block = GrabVideo (VLC_OBJECT(access), sys->fd, sys->bufv);
    if( block != NULL )
    {
        block->i_pts = block->i_dts = mdate();
        block->i_flags |= sys->block_flags;
    }
    return block;
}

static block_t *ReadBlock (access_t *access)
{
    access_sys_t *sys = access->p_sys;

    if (AccessPoll (access))
        return NULL;

    block_t *block = block_Alloc (sys->blocksize);
    if (unlikely(block == NULL))
        return NULL;

    ssize_t val = v4l2_read (sys->fd, block->p_buffer, block->i_buffer);
    if (val < 0)
    {
        block_Release (block);
        msg_Err (access, "cannot read buffer: %m");
        access->info.b_eof = true;
        return NULL;
    }

    block->i_buffer = val;
    access->info.i_pos += val;
    return block;
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
