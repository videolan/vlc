/*****************************************************************************
 * v4l2.h : Video4Linux2 input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2011 the VideoLAN team
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

#include <vlc_common.h>

#if defined(HAVE_LINUX_VIDEODEV2_H)
#   include <linux/videodev2.h>
#elif defined(HAVE_SYS_VIDEOIO_H)
#   include <sys/videoio.h>
#else
#   error "No Video4Linux2 headers found."
#endif

/* Hacks to compile with old headers */
#ifndef V4L2_CTRL_FLAG_VOLATILE /* 3.2 */
# warning Please update Video4Linux2 headers!
# define V4L2_CTRL_FLAG_VOLATILE 0x0080
#endif
#ifdef __linux__
# include <linux/version.h>
# if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
#  define V4L2_CTRL_TYPE_BITMASK 8
# endif
#endif
#ifndef V4L2_CID_ILLUMINATORS_1 /* 2.6.37 */
# define V4L2_CID_ILLUMINATORS_1 (V4L2_CID_BASE+38)
# define V4L2_CID_ILLUMINATORS_2 (V4L2_CID_BASE+37)
#endif
#ifndef V4L2_CID_CHROMA_GAIN /* 2.6.35 */
# define V4L2_CID_CHROMA_GAIN (V4L2_CID_BASE+36)
#endif
#ifndef V4L2_CID_ROTATE /* 2.6.33 */
# define V4L2_CID_BG_COLOR (V4L2_CID_BASE+35)
# define V4L2_CID_ROTATE (V4L2_CID_BASE+34)
#endif


#ifdef HAVE_LIBV4L2
#   include <libv4l2.h>
#else
#   define v4l2_close close
#   define v4l2_dup dup
#   define v4l2_ioctl ioctl
#   define v4l2_read read
#   define v4l2_mmap mmap
#   define v4l2_munmap munmap
#endif

#define CFG_PREFIX "v4l2-"

/* TODO: remove this, use callbacks */
typedef enum {
    IO_METHOD_READ=1,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
} io_method;

typedef struct vlc_v4l2_ctrl vlc_v4l2_ctrl_t;

/* TODO: move this to access.c and demux.c (separately) */
struct demux_sys_t
{
    int  i_fd;

    /* Video */
    io_method io;

    struct buffer_t *p_buffers;
    unsigned int i_nbuffers;
#define blocksize i_nbuffers /* HACK HACK */

    int i_fourcc;
    uint32_t i_block_flags;

    es_out_id_t *p_es;

    vlc_v4l2_ctrl_t *controls;

#ifdef HAVE_LIBV4L2
    bool b_libv4l2;
#endif
};

struct buffer_t
{
    void *  start;
    size_t  length;
};

/* video.c */
void ParseMRL(vlc_object_t *, const char *);
int OpenVideo(vlc_object_t *, demux_sys_t *, bool);
block_t* GrabVideo(vlc_object_t *, demux_sys_t *);

/* demux.c */
int DemuxOpen(vlc_object_t *);
void DemuxClose(vlc_object_t *);
float GetAbsoluteMaxFrameRate(vlc_object_t *, int fd, uint32_t fmt);
void GetMaxDimensions(vlc_object_t *, int fd, uint32_t fmt, float fps_min,
                      uint32_t *pwidth, uint32_t *pheight);

/* access.c */
int AccessOpen(vlc_object_t *);
void AccessClose(vlc_object_t *);

/* controls.c */
vlc_v4l2_ctrl_t *ControlsInit(vlc_object_t *, int fd);
void ControlsDeinit(vlc_object_t *, vlc_v4l2_ctrl_t *);
