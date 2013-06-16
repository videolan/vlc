/*****************************************************************************
 * demux.c : V4L2 raw video demux module for vlc
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

#include <math.h>
#include <errno.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif
#include <poll.h>

#include <vlc_common.h>
#include <vlc_demux.h>

#include "v4l2.h"

struct demux_sys_t
{
    int fd;
    vlc_thread_t thread;

    struct buffer_t *bufv;
    union
    {
        uint32_t bufc;
        uint32_t blocksize;
    };
    uint32_t block_flags;

    es_out_id_t *es;
    vlc_v4l2_ctrl_t *controls;
    mtime_t start;

#ifdef ZVBI_COMPILED
    vlc_v4l2_vbi_t *vbi;
#endif
};

static void *UserPtrThread (void *);
static void *MmapThread (void *);
static void *ReadThread (void *);
static int DemuxControl( demux_t *, int, va_list );
static int InitVideo (demux_t *, int fd, uint32_t caps);

int DemuxOpen( vlc_object_t *obj )
{
    demux_t *demux = (demux_t *)obj;

    demux_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    demux->p_sys = sys;
#ifdef ZVBI_COMPILED
    sys->vbi = NULL;
#endif

    ParseMRL( obj, demux->psz_location );

    char *path = var_InheritString (obj, CFG_PREFIX"dev");
    if (unlikely(path == NULL))
        goto error; /* probably OOM */

    uint32_t caps;
    int fd = OpenDevice (obj, path, &caps);
    free (path);
    if (fd == -1)
        goto error;
    sys->fd = fd;

    if (InitVideo (demux, fd, caps))
    {
        v4l2_close (fd);
        goto error;
    }

    sys->controls = ControlsInit (VLC_OBJECT(demux), fd);
    sys->start = mdate ();
    demux->pf_demux = NULL;
    demux->pf_control = DemuxControl;
    demux->info.i_update = 0;
    demux->info.i_title = 0;
    demux->info.i_seekpoint = 0;
    return VLC_SUCCESS;
error:
    free (sys);
    return VLC_EGENERIC;
}

typedef struct
{
    uint32_t v4l2;
    vlc_fourcc_t vlc;
    uint8_t bpp; /**< Bytes per pixel (largest plane) */
    uint32_t red;
    uint32_t green;
    uint32_t blue;
} vlc_v4l2_fmt_t;

/* NOTE: Currently vlc_v4l2_fmt_rank() assumes format are sorted in order of
 * decreasing preference. */
static const vlc_v4l2_fmt_t v4l2_fmts[] =
{
    /* Planar YUV 4:2:0 */
    { V4L2_PIX_FMT_YUV420,  VLC_CODEC_I420, 1, 0, 0, 0 },
    { V4L2_PIX_FMT_YVU420,  VLC_CODEC_YV12, 1, 0, 0, 0 },
    { V4L2_PIX_FMT_YUV422P, VLC_CODEC_I422, 1, 0, 0, 0 },
    /* Packed YUV 4:2:2 */
    { V4L2_PIX_FMT_YUYV,    VLC_CODEC_YUYV, 2, 0, 0, 0 },
    { V4L2_PIX_FMT_UYVY,    VLC_CODEC_UYVY, 2, 0, 0, 0 },
    { V4L2_PIX_FMT_YVYU,    VLC_CODEC_YVYU, 2, 0, 0, 0 },
    { V4L2_PIX_FMT_VYUY,    VLC_CODEC_VYUY, 2, 0, 0, 0 },

    { V4L2_PIX_FMT_YUV411P, VLC_CODEC_I411, 1, 0, 0, 0 },

    { V4L2_PIX_FMT_YUV410,  VLC_CODEC_I410, 1, 0, 0, 0 },
//  { V4L2_PIX_FMT_YVU410     },

//  { V4L2_PIX_FMT_NV24,      },
//  { V4L2_PIX_FMT_NV42,      },
//  { V4L2_PIX_FMT_NV16,    VLC_CODEC_NV16, 1, 0, 0, 0 },
//  { V4L2_PIX_FMT_NV61,    VLC_CODEC_NV61, 1, 0, 0, 0 },
    { V4L2_PIX_FMT_NV12,    VLC_CODEC_NV12, 1, 0, 0, 0 },
    { V4L2_PIX_FMT_NV21,    VLC_CODEC_NV21, 1, 0, 0, 0 },

    /* V4L2-documented but VLC-unsupported misc. YUV formats */
//  { V4L2_PIX_FMT_Y41P       },
//  { V4L2_PIX_FMT_NV12MT,    },
//  { V4L2_PIX_FMT_M420,      },

    /* Packed RGB */
#ifdef WORDS_BIGENDIAN
    { V4L2_PIX_FMT_RGB32,   VLC_CODEC_RGB32, 4, 0xFF00, 0xFF0000, 0xFF000000 },
    { V4L2_PIX_FMT_BGR32,   VLC_CODEC_RGB32, 4, 0xFF000000, 0xFF0000, 0xFF00 },
    { V4L2_PIX_FMT_RGB24,   VLC_CODEC_RGB24, 3, 0xFF0000, 0x00FF00, 0x0000FF },
    { V4L2_PIX_FMT_BGR24,   VLC_CODEC_RGB24, 3, 0x0000FF, 0x00FF00, 0xFF0000 },
//  { V4L2_PIX_FMT_BGR666,    },
//  { V4L2_PIX_FMT_RGB565,    },
    { V4L2_PIX_FMT_RGB565X, VLC_CODEC_RGB16, 2,  0x001F,   0x07E0,   0xF800 },
//  { V4L2_PIX_FMT_RGB555,    },
    { V4L2_PIX_FMT_RGB555X, VLC_CODEC_RGB15, 2,  0x001F,   0x03E0,   0x7C00 },
//  { V4L2_PIX_FMT_RGB444,  VLC_CODEC_RGB12, 2,  0x000F,   0xF000,   0x0F00 },
#else
    { V4L2_PIX_FMT_RGB32,   VLC_CODEC_RGB32, 4, 0x0000FF, 0x00FF00, 0xFF0000 },
    { V4L2_PIX_FMT_BGR32,   VLC_CODEC_RGB32, 4, 0xFF0000, 0x00FF00, 0x0000FF },
    { V4L2_PIX_FMT_RGB24,   VLC_CODEC_RGB24, 3, 0x0000FF, 0x00FF00, 0xFF0000 },
    { V4L2_PIX_FMT_BGR24,   VLC_CODEC_RGB24, 3, 0xFF0000, 0x00FF00, 0x0000FF },
//  { V4L2_PIX_FMT_BGR666,    },
    { V4L2_PIX_FMT_RGB565,  VLC_CODEC_RGB16, 2,   0x001F,   0x07E0,   0xF800 },
//  { V4L2_PIX_FMT_RGB565X,   },
    { V4L2_PIX_FMT_RGB555,  VLC_CODEC_RGB15, 2,   0x001F,   0x03E0,   0x7C00 },
//  { V4L2_PIX_FMT_RGB555X,   },
//  { V4L2_PIX_FMT_RGB444,  VLC_CODEC_RGB12, 2,   0x0F00,   0x00F0,   0x000F },
#endif
//  { V4L2_PIX_FMT_RGB332,  VLC_CODEC_RGB8,  1,      0xC0,     0x38,     0x07 },

    /* Bayer (sub-sampled RGB). Not supported. */
//  { V4L2_PIX_FMT_SBGGR16,  }
//  { V4L2_PIX_FMT_SRGGB12,  }
//  { V4L2_PIX_FMT_SGRBG12,  }
//  { V4L2_PIX_FMT_SGBRG12,  }
//  { V4L2_PIX_FMT_SBGGR12,  }
//  { V4L2_PIX_FMT_SRGGB10,  }
//  { V4L2_PIX_FMT_SGRBG10,  }
//  { V4L2_PIX_FMT_SGBRG10,  }
//  { V4L2_PIX_FMT_SBGGR10,  }
//  { V4L2_PIX_FMT_SBGGR8,   }
//  { V4L2_PIX_FMT_SGBRG8,   }
//  { V4L2_PIX_FMT_SGRBG8,   }
//  { V4L2_PIX_FMT_SRGGB8,   }

    /* Compressed data types */
    { V4L2_PIX_FMT_JPEG,    VLC_CODEC_MJPG, 0, 0, 0, 0 },
    { V4L2_PIX_FMT_H264,    VLC_CODEC_H264, 0, 0, 0, 0 },
    /* FIXME: fill p_extra for avc1... */
//  { V4L2_PIX_FMT_H264_NO_SC, VLC_FOURCC('a','v','c','1'), 0, 0, 0, 0 }
    { V4L2_PIX_FMT_MPEG4,   VLC_CODEC_MP4V, 0, 0, 0, 0 },
    { V4L2_PIX_FMT_XVID,    VLC_CODEC_MP4V, 0, 0, 0, 0 },
    { V4L2_PIX_FMT_H263,    VLC_CODEC_H263, 0, 0, 0, 0 },
    { V4L2_PIX_FMT_MPEG2,   VLC_CODEC_MPGV, 0, 0, 0, 0 },
    { V4L2_PIX_FMT_MPEG1,   VLC_CODEC_MPGV, 0, 0, 0, 0 },
    { V4L2_PIX_FMT_VC1_ANNEX_G, VLC_CODEC_VC1, 0, 0, 0, 0 },
    { V4L2_PIX_FMT_VC1_ANNEX_L, VLC_CODEC_VC1, 0, 0, 0, 0 },
    //V4L2_PIX_FMT_MPEG -> use access

    /* Reserved formats */
    { V4L2_PIX_FMT_MJPEG,   VLC_CODEC_MJPG, 0, 0, 0, 0 },
    //V4L2_PIX_FMT_DV -> use access

    /* Grey scale */
//  { V4L2_PIX_FMT_Y16,       },
//  { V4L2_PIX_FMT_Y12,       },
//  { V4L2_PIX_FMT_Y10,       },
//  { V4L2_PIX_FMT_Y10BPACK,  },
    { V4L2_PIX_FMT_GREY,    VLC_CODEC_GREY, 1, 0, 0, 0 },
};

static const vlc_v4l2_fmt_t *vlc_from_v4l2_fourcc (uint32_t fourcc)
{
     for (size_t i = 0; i < sizeof (v4l2_fmts) / sizeof (v4l2_fmts[0]); i++)
         if (v4l2_fmts[i].v4l2 == fourcc)
             return v4l2_fmts + i;
     return NULL;
}

static size_t vlc_v4l2_fmt_rank (const vlc_v4l2_fmt_t *fmt)
{
    if (fmt == NULL)
        return SIZE_MAX;

    ptrdiff_t d = fmt - v4l2_fmts;
    assert (d >= 0);
    assert (d < (ptrdiff_t)(sizeof (v4l2_fmts) / sizeof (v4l2_fmts[0])));
    return d;
}

static vlc_fourcc_t var_InheritFourCC (vlc_object_t *obj, const char *varname)
{
    char *str = var_InheritString (obj, varname);
    if (str == NULL)
        return 0;

    vlc_fourcc_t fourcc = vlc_fourcc_GetCodecFromString (VIDEO_ES, str);
    if (fourcc == 0)
        msg_Err (obj, "invalid codec %s", str);
    free (str);
    return fourcc;
}
#define var_InheritFourCC(o, v) var_InheritFourCC(VLC_OBJECT(o), v)

static void GetAR (int fd, unsigned *restrict num, unsigned *restrict den)
{
    struct v4l2_cropcap cropcap = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

    /* TODO: get CROPCAP only once (see ResetCrop()). */
    if (v4l2_ioctl (fd, VIDIOC_CROPCAP, &cropcap) < 0)
    {
        *num = *den = 1;
        return;
    }
    *num = cropcap.pixelaspect.numerator;
    *den = cropcap.pixelaspect.denominator;
}

static int InitVideo (demux_t *demux, int fd, uint32_t caps)
{
    demux_sys_t *sys = demux->p_sys;
    v4l2_std_id std;

    if (!(caps & V4L2_CAP_VIDEO_CAPTURE))
    {
        msg_Err (demux, "not a video capture device");
        return -1;
    }

    if (SetupInput (VLC_OBJECT(demux), fd, &std))
        return -1;

    /* Picture format negotiation */
    const vlc_v4l2_fmt_t *selected = NULL;
    vlc_fourcc_t reqfourcc = var_InheritFourCC (demux, CFG_PREFIX"chroma");
    bool native = false;

    for (struct v4l2_fmtdesc codec = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
         v4l2_ioctl (fd, VIDIOC_ENUM_FMT, &codec) >= 0;
         codec.index++)
    {   /* Enumerate available chromas */
        const vlc_v4l2_fmt_t *dsc = vlc_from_v4l2_fourcc (codec.pixelformat);

        msg_Dbg (demux, " %s %s format %4.4s (%4.4s): %s",
              (codec.flags & V4L2_FMT_FLAG_EMULATED) ? "emulates" : "supports",
              (codec.flags & V4L2_FMT_FLAG_COMPRESSED) ? "compressed" : "raw",
                 (char *)&codec.pixelformat,
                 (dsc != NULL) ? (const char *)&dsc->vlc : "N.A.",
                 codec.description);

        if (dsc == NULL)
            continue; /* ignore VLC-unsupported codec */

        if (dsc->vlc == reqfourcc)
        {
            msg_Dbg (demux, "  matches the requested format");
            selected = dsc;
            break; /* always select the requested format if found */
        }

        if (codec.flags & V4L2_FMT_FLAG_EMULATED)
        {
            if (native)
                continue; /* ignore emulated format if possible */
        }
        else
            native = true;

        if (vlc_v4l2_fmt_rank (dsc) > vlc_v4l2_fmt_rank (selected))
            continue; /* ignore if rank is worse */

        selected = dsc;
    }

    if (selected == NULL)
    {
        msg_Err (demux, "cannot negotiate supported video format");
        return -1;
    }
    msg_Dbg (demux, "selected format %4.4s (%4.4s)",
             (const char *)&selected->v4l2, (const char *)&selected->vlc);

    /* Find best resolution and frame rate available */
    struct v4l2_format fmt;
    struct v4l2_streamparm parm;
    if (SetupFormat (demux, fd, selected->v4l2, &fmt, &parm))
        return -1;

    /* Print extra info */
    msg_Dbg (demux, "%d bytes maximum for complete image",
             fmt.fmt.pix.sizeimage);
    /* Check interlacing */
    sys->block_flags = 0;
    switch (fmt.fmt.pix.field)
    {
        case V4L2_FIELD_NONE:
            msg_Dbg (demux, "Interlacing setting: progressive");
            break;
        case V4L2_FIELD_TOP:
            msg_Dbg (demux, "Interlacing setting: top field only");
            break;
        case V4L2_FIELD_BOTTOM:
            msg_Dbg (demux, "Interlacing setting: bottom field only");
            break;
        case V4L2_FIELD_INTERLACED:
            msg_Dbg (demux, "Interlacing setting: interleaved");
            /*if (NTSC)
                sys->block_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            else*/
                sys->block_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case V4L2_FIELD_SEQ_TB:
            msg_Dbg (demux, "Interlacing setting: sequential top bottom (TODO)");
            break;
        case V4L2_FIELD_SEQ_BT:
            msg_Dbg (demux, "Interlacing setting: sequential bottom top (TODO)");
            break;
        case V4L2_FIELD_ALTERNATE:
            msg_Dbg (demux, "Interlacing setting: alternate fields (TODO)");
            fmt.fmt.pix.height *= 2;
            break;
        case V4L2_FIELD_INTERLACED_TB:
            msg_Dbg (demux, "Interlacing setting: interleaved top bottom");
            sys->block_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case V4L2_FIELD_INTERLACED_BT:
            msg_Dbg (demux, "Interlacing setting: interleaved bottom top");
            sys->block_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            break;
        default:
            msg_Warn (demux, "Interlacing setting: unknown type (%d)",
                      fmt.fmt.pix.field);
            break;
    }

    /* Declare our unique elementary (video) stream */
    es_format_t es_fmt;

    es_format_Init (&es_fmt, VIDEO_ES, selected->vlc);
    es_fmt.video.i_rmask = selected->red;
    es_fmt.video.i_gmask = selected->green;
    es_fmt.video.i_bmask = selected->blue;
    es_fmt.video.i_visible_width = fmt.fmt.pix.width;
    if (fmt.fmt.pix.bytesperline != 0 && selected->bpp != 0)
        es_fmt.video.i_width = fmt.fmt.pix.bytesperline / selected->bpp;
    else
        es_fmt.video.i_width = fmt.fmt.pix.width;
    es_fmt.video.i_height = fmt.fmt.pix.height;
    es_fmt.video.i_frame_rate = parm.parm.capture.timeperframe.denominator;
    es_fmt.video.i_frame_rate_base = parm.parm.capture.timeperframe.numerator;
    GetAR (fd, &es_fmt.video.i_sar_num, &es_fmt.video.i_sar_den);

    msg_Dbg (demux, "added new video ES %4.4s %ux%u", (char *)&es_fmt.i_codec,
             es_fmt.video.i_width, es_fmt.video.i_height);
    msg_Dbg (demux, " frame rate: %u/%u", es_fmt.video.i_frame_rate,
             es_fmt.video.i_frame_rate_base);
    msg_Dbg (demux, " aspect ratio: %u/%u", es_fmt.video.i_sar_num,
             es_fmt.video.i_sar_den);
    sys->es = es_out_Add (demux->out, &es_fmt);

    /* Init I/O method */
    void *(*entry) (void *);
    if (caps & V4L2_CAP_STREAMING)
    {
        if (0 /* BROKEN */ && StartUserPtr (VLC_OBJECT(demux), fd) == 0)
        {
            /* In principles, mmap() will pad the length to a multiple of the
             * page size, so there is no need to care. Nevertheless with the
             * page size, block->i_size can be set optimally. */
            const long pagemask = sysconf (_SC_PAGE_SIZE) - 1;

            sys->blocksize = (fmt.fmt.pix.sizeimage + pagemask) & ~pagemask;
            sys->bufv = NULL;
            entry = UserPtrThread;
            msg_Dbg (demux, "streaming with %"PRIu32"-bytes user buffers",
                     sys->blocksize);
        }
        else /* fall back to memory map */
        {
            sys->bufc = 4;
            sys->bufv = StartMmap (VLC_OBJECT(demux), fd, &sys->bufc);
            if (sys->bufv == NULL)
                return -1;
            entry = MmapThread;
            msg_Dbg (demux, "streaming with %"PRIu32" memory-mapped buffers",
                     sys->bufc);
        }
    }
    else if (caps & V4L2_CAP_READWRITE)
    {
        sys->blocksize = fmt.fmt.pix.sizeimage;
        sys->bufv = NULL;
        entry = ReadThread;
        msg_Dbg (demux, "reading %"PRIu32" bytes at a time", sys->blocksize);
    }
    else
    {
        msg_Err (demux, "no supported capture method");
        return -1;
    }

#ifdef ZVBI_COMPILED
    if (std & V4L2_STD_NTSC_M)
    {
        char *vbi_path = var_InheritString (demux, CFG_PREFIX"vbidev");
        if (vbi_path != NULL)
            sys->vbi = OpenVBI (demux, vbi_path);
        free(vbi_path);
    }
#endif

    if (vlc_clone (&sys->thread, entry, demux, VLC_THREAD_PRIORITY_INPUT))
    {
#ifdef ZVBI_COMPILED
        if (sys->vbi != NULL)
            CloseVBI (sys->vbi);
#endif
        if (sys->bufv != NULL)
            StopMmap (sys->fd, sys->bufv, sys->bufc);
        return -1;
    }
    return 0;
}

void DemuxClose( vlc_object_t *obj )
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    vlc_cancel (sys->thread);
    vlc_join (sys->thread, NULL);
    if (sys->bufv != NULL)
        StopMmap (sys->fd, sys->bufv, sys->bufc);
    ControlsDeinit( obj, sys->controls );
    v4l2_close (sys->fd);

#ifdef ZVBI_COMPILED
    if (sys->vbi != NULL)
        CloseVBI (sys->vbi);
#endif

    free( sys );
}

/** Allocates and queue a user buffer using mmap(). */
static block_t *UserPtrQueue (vlc_object_t *obj, int fd, size_t length)
{
    void *ptr = mmap (NULL, length, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
    {
        msg_Err (obj, "cannot allocate %zu-bytes buffer: %m", length);
        return NULL;
    }

    block_t *block = block_mmap_Alloc (ptr, length);
    if (unlikely(block == NULL))
    {
        munmap (ptr, length);
        return NULL;
    }

    struct v4l2_buffer buf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_USERPTR,
        .m = {
            .userptr = (uintptr_t)ptr,
        },
        .length = length,
    };

    if (v4l2_ioctl (fd, VIDIOC_QBUF, &buf) < 0)
    {
        msg_Err (obj, "cannot queue buffer: %m");
        block_Release (block);
        return NULL;
    }
    return block;
}

static void *UserPtrThread (void *data)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;
    int fd = sys->fd;
    struct pollfd ufd[2];
    nfds_t numfds = 1;

    ufd[0].fd = fd;
    ufd[0].events = POLLIN;

    int canc = vlc_savecancel ();
    for (;;)
    {
        struct v4l2_buffer buf = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_USERPTR,
        };
        block_t *block = UserPtrQueue (VLC_OBJECT(demux), fd, sys->blocksize);
        if (block == NULL)
            break;

        /* Wait for data */
        vlc_restorecancel (canc);
        block_cleanup_push (block);
        while (poll (ufd, numfds, -1) == -1)
           if (errno != EINTR)
               msg_Err (demux, "poll error: %m");
        vlc_cleanup_pop ();
        canc = vlc_savecancel ();

        if (v4l2_ioctl (fd, VIDIOC_DQBUF, &buf) < 0)
        {
            msg_Err (demux, "cannot dequeue buffer: %m");
            block_Release (block);
            continue;
        }

        assert (block->p_buffer == (void *)buf.m.userptr);
        block->i_buffer = buf.length;
        block->i_pts = block->i_dts = GetBufferPTS (&buf);
        block->i_flags |= sys->block_flags;
        es_out_Control (demux->out, ES_OUT_SET_PCR, block->i_pts);
        es_out_Send (demux->out, sys->es, block);
    }
    vlc_restorecancel (canc); /* <- hmm, this is purely cosmetic */
    return NULL;
}

static void *MmapThread (void *data)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;
    int fd = sys->fd;
    struct pollfd ufd[2];
    nfds_t numfds = 1;

    ufd[0].fd = fd;
    ufd[0].events = POLLIN;

#ifdef ZVBI_COMPILED
    if (sys->vbi != NULL)
    {
        ufd[1].fd = GetFdVBI (sys->vbi);
        ufd[1].events = POLLIN;
        numfds++;
    }
#endif

    for (;;)
    {
        /* Wait for data */
        if (poll (ufd, numfds, -1) == -1)
        {
           if (errno != EINTR)
               msg_Err (demux, "poll error: %m");
           continue;
        }

        if( ufd[0].revents )
        {
            int canc = vlc_savecancel ();
            block_t *block = GrabVideo (VLC_OBJECT(demux), fd, sys->bufv);
            if (block != NULL)
            {
                block->i_flags |= sys->block_flags;
                es_out_Control (demux->out, ES_OUT_SET_PCR, block->i_pts);
                es_out_Send (demux->out, sys->es, block);
            }
            vlc_restorecancel (canc);
        }
#ifdef ZVBI_COMPILED
        if (sys->vbi != NULL && ufd[1].revents)
            GrabVBI (demux, sys->vbi);
#endif
    }

    assert (0);
}

static void *ReadThread (void *data)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;
    int fd = sys->fd;
    struct pollfd ufd[2];
    nfds_t numfds = 1;

    ufd[0].fd = fd;
    ufd[0].events = POLLIN;

#ifdef ZVBI_COMPILED
    if (sys->vbi != NULL)
    {
        ufd[1].fd = GetFdVBI (sys->vbi);
        ufd[1].events = POLLIN;
        numfds++;
    }
#endif

    for (;;)
    {
        /* Wait for data */
        if (poll (ufd, numfds, -1) == -1)
        {
           if (errno != EINTR)
               msg_Err (demux, "poll error: %m");
           continue;
        }

        if( ufd[0].revents )
        {
            block_t *block = block_Alloc (sys->blocksize);
            if (unlikely(block == NULL))
            {
                msg_Err (demux, "read error: %m");
                v4l2_read (fd, NULL, 0); /* discard frame */
                continue;
            }
            block->i_pts = block->i_dts = mdate ();
            block->i_flags |= sys->block_flags;

            int canc = vlc_savecancel ();
            ssize_t val = v4l2_read (fd, block->p_buffer, block->i_buffer);
            if (val != -1)
            {
                block->i_buffer = val;
                es_out_Control (demux->out, ES_OUT_SET_PCR, block->i_pts);
                es_out_Send (demux->out, sys->es, block);
            }
            else
                block_Release (block);
            vlc_restorecancel (canc);
        }
#ifdef ZVBI_COMPILED
        if (sys->vbi != NULL && ufd[1].revents)
            GrabVBI (demux, sys->vbi);
#endif
    }
    assert (0);
}

static int DemuxControl( demux_t *demux, int query, va_list args )
{
    demux_sys_t *sys = demux->p_sys;

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
            *va_arg (args, int64_t *) = mdate() - sys->start;
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}
