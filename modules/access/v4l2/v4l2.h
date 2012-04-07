/*****************************************************************************
 * v4l2.h : Video4Linux2 input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2011 the VideoLAN team
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

#if defined (HAVE_LINUX_VIDEODEV2_H)
# include <linux/videodev2.h>
#elif defined (HAVE_SYS_VIDEOIO_H)
# include <sys/videoio.h>
#else
# error "No Video4Linux2 headers found."
#endif
#ifndef V4L2_CAP_DEVICE_CAPS
# warning Please update Video4Linux2 headers!
#endif

/* Hacks to compile with old headers */
#ifndef V4L2_CTRL_FLAG_VOLATILE /* 3.2 */
# define V4L2_CTRL_FLAG_VOLATILE 0x0080
# define V4L2_CID_POWER_LINE_FREQUENCY_AUTO 3
# define V4L2_STD_G (V4L2_STD_PAL_G|V4L2_STD_SECAM_G)
# define V4L2_STD_H (V4L2_STD_PAL_H|V4L2_STD_SECAM_H)
# define V4L2_STD_L (V4L2_STD_SECAM_L|V4L2_STD_SECAM_LC)
# define V4L2_STD_BG (V4L2_STD_B|V4L2_STD_G)
# define V4L2_STD_MTS (V4L2_STD_NTSC_M|V4L2_STD_PAL_M|V4L2_STD_PAL_N|\
                       V4L2_STD_PAL_Nc)
#endif
#ifndef V4L2_CID_ILLUMINATORS_1 /* 2.6.37 */
# define V4L2_CID_ILLUMINATORS_1 (V4L2_CID_BASE+38)
# define V4L2_CID_ILLUMINATORS_2 (V4L2_CID_BASE+37)
#endif
#ifndef V4L2_CID_CHROMA_GAIN /* 2.6.35 */
# define V4L2_CID_CHROMA_GAIN (V4L2_CID_BASE+36)
# define V4L2_COLORFX_VIVID 9
# define V4L2_COLORFX_SKIN_WHITEN 8
# define V4L2_COLORFX_GRASS_GREEN 7
# define V4L2_COLORFX_SKY_BLUE 6
# define V4L2_COLORFX_SKETCH 5
# define V4L2_COLORFX_EMBOSS 4
# define V4L2_COLORFX_NEGATIVE 3
#endif
#ifndef V4L2_CID_ROTATE /* 2.6.33 */
# define V4L2_CID_BG_COLOR (V4L2_CID_BASE+35)
# define V4L2_CID_ROTATE (V4L2_CID_BASE+34)
#endif

/* libv4l2 functions */
extern int v4l2_fd_open (int, int);
extern int (*v4l2_close) (int);
extern int (*v4l2_ioctl) (int, unsigned long int, ...);
extern ssize_t (*v4l2_read) (int, void *, size_t);
extern void * (*v4l2_mmap) (void *, size_t, int, int, int, int64_t);
extern int (*v4l2_munmap) (void *, size_t);

#define CFG_PREFIX "v4l2-"

/* TODO: remove this, use callbacks */
typedef enum {
    IO_METHOD_READ=1,
    IO_METHOD_MMAP,
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

    uint32_t i_block_flags;

    es_out_id_t *p_es;

    vlc_v4l2_ctrl_t *controls;
};

struct buffer_t
{
    void *  start;
    size_t  length;
};

/* video.c */
void ParseMRL(vlc_object_t *, const char *);
int SetupInput (vlc_object_t *, int fd);
int SetupFormat (vlc_object_t *, int, uint32_t,
                 struct v4l2_format *, struct v4l2_streamparm *);
#define SetupFormat(o,fd,fcc,fmt,p) \
        SetupFormat(VLC_OBJECT(o),fd,fcc,fmt,p)

int InitMmap (vlc_object_t *, demux_sys_t *, int);
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
