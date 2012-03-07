/*****************************************************************************
 * demux.c : V4L2 raw video demux module for vlc
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

#include "v4l2.h"
#include <vlc_demux.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>

static int DemuxControl( demux_t *, int, va_list );
static int Demux( demux_t * );

int DemuxOpen( vlc_object_t *obj )
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = calloc( 1, sizeof( demux_sys_t ) );
    if( unlikely(sys == NULL) )
        return VLC_ENOMEM;
    demux->p_sys = sys;

    ParseMRL( obj, demux->psz_location );
    sys->i_fd = OpenVideo( obj, sys, true );
    if( sys->i_fd == -1 )
    {
        free( sys );
        return VLC_EGENERIC;
    }

    demux->pf_demux = Demux;
    demux->pf_control = DemuxControl;
    demux->info.i_update = 0;
    demux->info.i_title = 0;
    demux->info.i_seekpoint = 0;
    return VLC_SUCCESS;
}

void DemuxClose( vlc_object_t *obj )
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;
    int fd = sys->i_fd;

    /* Stop video capture */
    switch( sys->io )
    {
        case IO_METHOD_READ:
            /* Nothing to do */
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
        {
            /* NOTE: Some buggy drivers hang if buffers are not unmapped before
             * streamoff */
            for( unsigned i = 0; i < sys->i_nbuffers; i++ )
            {
                struct v4l2_buffer buf = {
                    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                    .memory = ( sys->io == IO_METHOD_USERPTR ) ?
                    V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP,
                };
                v4l2_ioctl( fd, VIDIOC_DQBUF, &buf );
            }
            enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            v4l2_ioctl( sys->i_fd, VIDIOC_STREAMOFF, &buf_type );
            break;
        }
    }

    /* Free Video Buffers */
    if( sys->p_buffers ) {
        switch( sys->io )
        {
        case IO_METHOD_READ:
            free( sys->p_buffers[0].start );
            break;

        case IO_METHOD_MMAP:
            for( unsigned i = 0; i < sys->i_nbuffers; ++i )
                v4l2_munmap( sys->p_buffers[i].start,
                             sys->p_buffers[i].length );
            break;

        case IO_METHOD_USERPTR:
            for( unsigned i = 0; i < sys->i_nbuffers; ++i )
               free( sys->p_buffers[i].start );
            break;
        }
        free( sys->p_buffers );
    }

    ControlsDeinit( obj, sys->controls );
    v4l2_close( fd );
    free( sys );
}

static int DemuxControl( demux_t *demux, int query, va_list args )
{
    switch( query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg(args,int64_t *) = INT64_C(1000)
                * var_InheritInteger( demux, "live-caching" );
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, int64_t * ) = mdate();
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}

/** Gets a frame in read/write mode */
static block_t *BlockRead( vlc_object_t *obj, int fd, size_t size )
{
    block_t *block = block_Alloc( size );
    if( unlikely(block == NULL) )
        return NULL;

    ssize_t val = v4l2_read( fd, block->p_buffer, size );
    if( val == -1 )
    {
        block_Release( block );
        switch( errno )
        {
            case EAGAIN:
                return NULL;
            case EIO: /* could be ignored per specification */
                /* fall through */
            default:
                msg_Err( obj, "cannot read frame: %m" );
                return NULL;
        }
    }
    block->i_buffer = val;
    return block;
}

static int Demux( demux_t *demux )
{
    demux_sys_t *sys = demux->p_sys;
    struct pollfd ufd;

    ufd.fd = sys->i_fd;
    ufd.events = POLLIN|POLLPRI;
    /* Wait for data */
    /* FIXME: remove timeout */
    while( poll( &ufd, 1, 500 ) == -1 )
        if( errno != EINTR )
        {
            msg_Err( demux, "poll error: %m" );
            return -1;
        }

    if( ufd.revents == 0 )
        return 1;

    block_t *block;

    if( sys->io == IO_METHOD_READ )
        block = BlockRead( VLC_OBJECT(demux), ufd.fd, sys->blocksize );
    else
        block = GrabVideo( VLC_OBJECT(demux), sys );
    if( block == NULL )
        return 1;

    block->i_pts = block->i_dts = mdate();
    block->i_flags |= sys->i_block_flags;
    es_out_Control( demux->out, ES_OUT_SET_PCR, block->i_pts );
    es_out_Send( demux->out, sys->p_es, block );
    return 1;
}

static float GetMaxFPS( vlc_object_t *obj, int fd, uint32_t pixel_format,
                        uint32_t width, uint32_t height )
{
#ifdef VIDIOC_ENUM_FRAMEINTERVALS
    /* This is new in Linux 2.6.19 */
    struct v4l2_frmivalenum fie = {
        .pixel_format = pixel_format,
        .width = width,
        .height = height,
    };

    if( v4l2_ioctl( fd, VIDIOC_ENUM_FRAMEINTERVALS, &fie ) < 0 )
        return -1.;

    switch( fie.type )
    {
        case V4L2_FRMIVAL_TYPE_DISCRETE:
        {
            float max = -1.;
            do
            {
                float fps = (float)fie.discrete.denominator
                          / (float)fie.discrete.numerator;
                if( fps > max )
                    max = fps;
                msg_Dbg( obj, "  discrete frame interval %"PRIu32"/%"PRIu32
                         " supported",
                         fie.discrete.numerator, fie.discrete.denominator );
                fie.index++;
            } while( v4l2_ioctl( fd, VIDIOC_ENUM_FRAMEINTERVALS, &fie ) >= 0 );
            return max;
        }

        case V4L2_FRMIVAL_TYPE_STEPWISE:
        case V4L2_FRMIVAL_TYPE_CONTINUOUS:
            msg_Dbg( obj, "  frame intervals from %"PRIu32"/%"PRIu32
                    "to %"PRIu32"/%"PRIu32" supported",
                    fie.stepwise.min.numerator, fie.stepwise.min.denominator,
                    fie.stepwise.max.numerator, fie.stepwise.max.denominator );
            if( fie.type == V4L2_FRMIVAL_TYPE_STEPWISE )
                msg_Dbg( obj, "  with %"PRIu32"/%"PRIu32" step",
                         fie.stepwise.step.numerator,
                         fie.stepwise.step.denominator );
            return __MAX( (float)fie.stepwise.max.denominator
                        / (float)fie.stepwise.max.numerator,
                          (float)fie.stepwise.min.denominator
                        / (float)fie.stepwise.min.numerator );
    }
#endif
    return -1.;
}

float GetAbsoluteMaxFrameRate( vlc_object_t *obj, int fd,
                               uint32_t pixel_format )
{
#ifdef VIDIOC_ENUM_FRAMESIZES
    /* This is new in Linux 2.6.19 */
    struct v4l2_frmsizeenum fse = {
        .pixel_format = pixel_format
    };

    if( v4l2_ioctl( fd, VIDIOC_ENUM_FRAMESIZES, &fse ) < 0 )
        return -1.;

    float max = -1.;
    switch( fse.type )
    {
      case V4L2_FRMSIZE_TYPE_DISCRETE:
        do
        {
            float fps = GetMaxFPS( obj, fd, pixel_format,
                                   fse.discrete.width, fse.discrete.height );
            if( fps > max )
                max = fps;
            fse.index++;
        } while( v4l2_ioctl( fd, VIDIOC_ENUM_FRAMESIZES, &fse ) >= 0 );
        break;

      case V4L2_FRMSIZE_TYPE_STEPWISE:
      case V4L2_FRMSIZE_TYPE_CONTINUOUS:
        msg_Dbg( obj, " sizes from %"PRIu32"x%"PRIu32" "
                 "to %"PRIu32"x%"PRIu32" supported",
                 fse.stepwise.min_width, fse.stepwise.min_height,
                 fse.stepwise.max_width, fse.stepwise.max_height );
        if( fse.type == V4L2_FRMSIZE_TYPE_STEPWISE )
            msg_Dbg( obj, "  with %"PRIu32"x%"PRIu32" steps",
                     fse.stepwise.step_width, fse.stepwise.step_height );

        for( uint32_t width =  fse.stepwise.min_width;
                      width <= fse.stepwise.max_width;
                      width += fse.stepwise.step_width )
            for( uint32_t height =  fse.stepwise.min_height;
                          height <= fse.stepwise.max_width;
                          height += fse.stepwise.step_height )
            {
                float fps = GetMaxFPS( obj, fd, pixel_format, width, height );
                if( fps > max )
                    max = fps;
            }
        break;
    }
    return max;
#else
    return -1.;
#endif
}

void GetMaxDimensions( vlc_object_t *obj, int fd, uint32_t pixel_format,
                       float fps_min, uint32_t *pwidth, uint32_t *pheight )
{
    *pwidth = 0;
    *pheight = 0;

#ifdef VIDIOC_ENUM_FRAMESIZES
    /* This is new in Linux 2.6.19 */
    struct v4l2_frmsizeenum fse = {
        .pixel_format = pixel_format
    };

    if( v4l2_ioctl( fd, VIDIOC_ENUM_FRAMESIZES, &fse ) < 0 )
        return;

    switch( fse.type )
    {
      case V4L2_FRMSIZE_TYPE_DISCRETE:
        do
        {
            msg_Dbg( obj, " discrete size %"PRIu32"x%"PRIu32" supported",
                     fse.discrete.width, fse.discrete.height );

            float fps = GetMaxFPS( obj, fd, pixel_format,
                                   fse.discrete.width, fse.discrete.height );
            if( fps >= fps_min && fse.discrete.width > *pwidth )
            {
                *pwidth = fse.discrete.width;
                *pheight = fse.discrete.height;
            }
            fse.index++;
        }
        while( v4l2_ioctl( fd, VIDIOC_ENUM_FRAMESIZES, &fse ) >= 0 );
        break;

      case V4L2_FRMSIZE_TYPE_STEPWISE:
      case V4L2_FRMSIZE_TYPE_CONTINUOUS:
        msg_Dbg( obj, " sizes from %"PRIu32"x%"PRIu32" "
                 "to %"PRIu32"x%"PRIu32" supported",
                 fse.stepwise.min_width, fse.stepwise.min_height,
                 fse.stepwise.max_width, fse.stepwise.max_height );
        if( fse.type == V4L2_FRMSIZE_TYPE_STEPWISE )
            msg_Dbg( obj, "  with %"PRIu32"x%"PRIu32" steps",
                     fse.stepwise.step_width, fse.stepwise.step_height );

        for( uint32_t width =  fse.stepwise.min_width;
                      width <= fse.stepwise.max_width;
                      width += fse.stepwise.step_width )
            for( uint32_t height = fse.stepwise.min_height;
                          height <= fse.stepwise.max_width;
                          height += fse.stepwise.step_height )
            {
                float fps = GetMaxFPS( obj, fd, pixel_format, width, height );
                if( fps >= fps_min && width > *pwidth )
                {
                    *pwidth = width;
                    *pheight = height;
                }
            }
        break;
    }
#endif
}
