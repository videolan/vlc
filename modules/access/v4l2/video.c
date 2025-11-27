/*****************************************************************************
 * video.c : Video4Linux2 input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 * Copyright (C) 2011-2012 Rémi Denis-Courmont
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
#include <sys/ioctl.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_es.h>

#include "v4l2.h"

static int SetupStandard (vlc_object_t *obj, int fd,
                          const struct v4l2_input *restrict input,
                          v4l2_std_id *restrict std)
{
    if (!(input->capabilities & V4L2_IN_CAP_STD))
    {
        msg_Dbg (obj, "no video standard selection");
        *std = V4L2_STD_UNKNOWN;
        return 0;
    }

    *std = var_InheritStandard (obj, CFG_PREFIX"standard");
    if (*std == V4L2_STD_UNKNOWN)
    {
        msg_Warn (obj, "video standard not set");

        /* Grab the currently selected standard */
        if (v4l2_ioctl (fd, VIDIOC_G_STD, std) < 0)
            msg_Err (obj, "cannot get video standard");
        return 0;
    }
    if (v4l2_ioctl (fd, VIDIOC_S_STD, std) < 0)
    {
        msg_Err (obj, "cannot set video standard 0x%"PRIx64": %s",
                 (uint64_t)*std, vlc_strerror_c(errno));
        return -1;
    }
    msg_Dbg (obj, "video standard set to 0x%"PRIx64":", (uint64_t)*std);
    return 0;
}

static int SetupAudio (vlc_object_t *obj, int fd,
                       const struct v4l2_input *restrict input)
{
    if (input->audioset == 0)
    {
        msg_Dbg (obj, "no audio input available");
        return 0;
    }
    msg_Dbg (obj, "available audio inputs: 0x%08"PRIX32, input->audioset);

    uint32_t idx = var_InheritInteger (obj, CFG_PREFIX"audio-input");
    if (idx == (uint32_t)-1)
    {
        msg_Dbg (obj, "no audio input selected");
        return 0;
    }
    if (((1 << idx) & input->audioset) == 0)
    {
        msg_Warn (obj, "skipped unavailable audio input %"PRIu32, idx);
        return -1;
    }

    /* TODO: Enumerate other selectable audio inputs. How to expose them? */
    struct v4l2_audio enumaudio = { .index = idx };

    if (v4l2_ioctl (fd, VIDIOC_ENUMAUDIO, &enumaudio) < 0)
    {
        msg_Err (obj, "cannot get audio input %"PRIu32" properties: %s", idx,
                 vlc_strerror_c(errno));
        return -1;
    }

    msg_Dbg (obj, "audio input %s (%"PRIu32") is %s"
             " (capabilities: 0x%08"PRIX32")", enumaudio.name, enumaudio.index,
             (enumaudio.capability & V4L2_AUDCAP_STEREO) ? "Stereo" : "Mono",
             enumaudio.capability);
    if (enumaudio.capability & V4L2_AUDCAP_AVL)
        msg_Dbg (obj, " supports Automatic Volume Level");

    /* TODO: AVL mode */
    struct v4l2_audio audio = { .index = idx };

    if (v4l2_ioctl (fd, VIDIOC_S_AUDIO, &audio) < 0)
    {
        msg_Err (obj, "cannot select audio input %"PRIu32": %s", idx,
                 vlc_strerror_c(errno));
        return -1;
    }
    msg_Dbg (obj, "selected audio input %"PRIu32, idx);
    return 0;
}

int SetupTuner (vlc_object_t *obj, int fd, uint32_t idx)
{
    struct v4l2_tuner tuner = { .index = idx };

    if (v4l2_ioctl (fd, VIDIOC_G_TUNER, &tuner) < 0)
    {
        msg_Err (obj, "cannot get tuner %"PRIu32" properties: %s", idx,
                 vlc_strerror_c(errno));
        return -1;
    }

    /* NOTE: This is overkill. Only video devices currently work, so the
     * type is always analog TV. */
    const char *typename, *mult;
    switch (tuner.type)
    {
        case V4L2_TUNER_RADIO:
            typename = "Radio";
            break;
        case V4L2_TUNER_ANALOG_TV:
            typename = "Analog TV";
            break;
        default:
            typename = "unknown";
    }
    mult = (tuner.capability & V4L2_TUNER_CAP_LOW) ? "" : "k";

    msg_Dbg (obj, "tuner %s (%"PRIu32") is %s", tuner.name, tuner.index,
             typename);
    msg_Dbg (obj, " ranges from %u.%u %sHz to %u.%u %sHz",
             (tuner.rangelow * 125) >> 1, (tuner.rangelow & 1) * 5, mult,
             (tuner.rangehigh * 125) >> 1, (tuner.rangehigh & 1) * 5,
             mult);

    /* TODO: only set video standard if the tuner requires it */

    /* Configure the audio mode */
    /* TODO: Ideally, L1 would be selected for stereo tuners, and L1_L2
     * for mono tuners. When dual-mono is detected after tuning on a stereo
     * tuner, we would fallback to L1_L2 too. Then we would flag dual-mono
     * for the audio E/S. Unfortunately, we have no access to the audio E/S
     * here (it belongs in the slave audio input...). */
    tuner.audmode = var_InheritInteger (obj, CFG_PREFIX"tuner-audio-mode");
    memset (tuner.reserved, 0, sizeof (tuner.reserved));

    if (tuner.capability & V4L2_TUNER_CAP_LANG1)
        msg_Dbg (obj, " supports primary audio language");
    else if (tuner.audmode == V4L2_TUNER_MODE_LANG1)
    {
        msg_Warn (obj, " falling back to stereo mode");
        tuner.audmode = V4L2_TUNER_MODE_STEREO;
    }
    if (tuner.capability & V4L2_TUNER_CAP_LANG2)
        msg_Dbg (obj, " supports secondary audio language or program");
    if (tuner.capability & V4L2_TUNER_CAP_STEREO)
        msg_Dbg (obj, " supports stereo audio");
    else if (tuner.audmode == V4L2_TUNER_MODE_STEREO)
    {
        msg_Warn (obj, " falling back to mono mode");
        tuner.audmode = V4L2_TUNER_MODE_MONO;
    }

    if (v4l2_ioctl (fd, VIDIOC_S_TUNER, &tuner) < 0)
    {
        msg_Err (obj, "cannot set tuner %"PRIu32" audio mode: %s", idx,
                 vlc_strerror_c(errno));
        return -1;
    }
    msg_Dbg (obj, "tuner %"PRIu32" audio mode %u set", idx, tuner.audmode);

    /* Tune to the requested frequency */
    uint32_t freq = var_InheritInteger (obj, CFG_PREFIX"tuner-frequency");
    if (freq != (uint32_t)-1)
    {
        struct v4l2_frequency frequency = {
            .tuner = idx,
            .type = tuner.type,
            .frequency = freq * 2 / 125,
        };

        if (v4l2_ioctl (fd, VIDIOC_S_FREQUENCY, &frequency) < 0)
        {
            msg_Err (obj, "cannot tune tuner %"PRIu32
                     " to frequency %u %sHz: %s", idx, freq, mult,
                     vlc_strerror_c(errno));
            return -1;
        }
        msg_Dbg (obj, "tuner %"PRIu32" tuned to frequency %"PRIu32" %sHz",
                 idx, freq, mult);
    }
    else
        msg_Dbg (obj, "tuner not tuned");
    return 0;
}

static int ResetCrop (vlc_object_t *obj, int fd)
{
    struct v4l2_cropcap cropcap = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

    /* In theory, this ioctl() must work for all video capture devices.
     * In practice, it does not. */
    if (v4l2_ioctl (fd, VIDIOC_CROPCAP, &cropcap) < 0)
    {
        msg_Dbg (obj, "cannot get cropping properties: %s",
                 vlc_strerror_c(errno));
        return 0;
    }

    /* Reset to the default cropping rectangle */
    struct v4l2_crop crop = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .c = cropcap.defrect,
    };

    if (v4l2_ioctl (fd, VIDIOC_S_CROP, &crop) < 0)
    {
        msg_Warn (obj, "cannot reset cropping limits: %s",
                  vlc_strerror_c(errno));
        return -1;
    }
    return 0;
}

static int SetupInput(vlc_object_t *obj, int fd, v4l2_std_id *std)
{
    struct v4l2_input input;

    input.index = var_InheritInteger (obj, CFG_PREFIX"input");
    if (v4l2_ioctl (fd, VIDIOC_ENUMINPUT, &input) < 0)
    {
        msg_Err (obj, "invalid video input %"PRIu32": %s", input.index,
                 vlc_strerror_c(errno));
        return -1;
    }

    const char *typename = "unknown";
    switch (input.type)
    {
        case V4L2_INPUT_TYPE_TUNER:
            typename = "tuner";
            break;
        case V4L2_INPUT_TYPE_CAMERA:
            typename = "camera";
            break;
    }

    msg_Dbg (obj, "video input %s (%"PRIu32") is %s", input.name,
             input.index, typename);

    /* Select input */
    if (v4l2_ioctl (fd, VIDIOC_S_INPUT, &input.index) < 0)
    {
        msg_Err (obj, "cannot select input %"PRIu32": %s", input.index,
                 vlc_strerror_c(errno));
        return -1;
    }
    msg_Dbg (obj, "selected input %"PRIu32, input.index);

    SetupStandard (obj, fd, &input, std);

    switch (input.type)
    {
        case V4L2_INPUT_TYPE_TUNER:
            msg_Dbg (obj, "tuning required: tuner %"PRIu32, input.tuner);
            SetupTuner (obj, fd, input.tuner);
            break;
        case V4L2_INPUT_TYPE_CAMERA:
            msg_Dbg (obj, "no tuning required (analog baseband input)");
            break;
        default:
            msg_Err (obj, "unknown input tuning type %"PRIu32, input.type);
            break; // hopefully we can stream regardless...
    }

    SetupAudio (obj, fd, &input);
    return 0;
}

/** Compares two V4L2 fractions. */
static int64_t fcmp (const struct v4l2_fract *a,
                     const struct v4l2_fract *b)
{
    return (uint64_t)a->numerator * b->denominator
         - (uint64_t)b->numerator * a->denominator;
}

static const struct v4l2_fract infinity = { 1, 0 };
static const struct v4l2_fract zero = { 0, 1 };

/**
 * Finds the highest frame rate up to a specific limit possible with a certain
 * V4L2 format.
 * @param fmt V4L2 capture format [IN]
 * @param min_it minimum frame interval [IN]
 * @param it V4L2 frame interval [OUT]
 * @return 0 on success, -1 on error.
 */
static int FindMaxRate (vlc_object_t *obj, int fd,
                        const struct v4l2_format *restrict fmt,
                        const struct v4l2_fract *restrict min_it,
                        struct v4l2_fract *restrict it)
{
    struct v4l2_frmivalenum fie = {
        .pixel_format = fmt->fmt.pix.pixelformat,
        .width = fmt->fmt.pix.width,
        .height = fmt->fmt.pix.height,
    };
    /* Mind that maximum rate means minimum interval */

    if (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMEINTERVALS, &fie) < 0)
    {
        msg_Dbg (obj, "  unknown frame intervals: %s", vlc_strerror_c(errno));
        /* Frame intervals cannot be enumerated. Set the format and then
         * get the streaming parameters to figure out the default frame
         * interval. This is not necessarily the maximum though. */
        struct v4l2_format dummy_fmt = *fmt;
        struct v4l2_streamparm parm = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

        if (v4l2_ioctl (fd, VIDIOC_S_FMT, &dummy_fmt) < 0
         || v4l2_ioctl (fd, VIDIOC_G_PARM, &parm) < 0)
        {
            *it = infinity;
            return -1;
        }

        *it = parm.parm.capture.timeperframe;
        msg_Dbg (obj, "  %s frame interval: %"PRIu32"/%"PRIu32,
                 (parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
                 ? "default" : "constant", it->numerator, it->denominator);
    }
    else
    switch (fie.type)
    {
        case V4L2_FRMIVAL_TYPE_DISCRETE:
            *it = infinity;
            do
            {
                if (fcmp (&fie.discrete, min_it) >= 0
                 && fcmp (&fie.discrete, it) < 0)
                    *it = fie.discrete;
                fie.index++;
            }
            while (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMEINTERVALS, &fie) >= 0);

            msg_Dbg (obj, "  %s frame interval: %"PRIu32"/%"PRIu32,
                     "discrete", it->numerator, it->denominator);
            break;

        case V4L2_FRMIVAL_TYPE_STEPWISE:
        case V4L2_FRMIVAL_TYPE_CONTINUOUS:
            msg_Dbg (obj, "  frame intervals from %"PRIu32"/%"PRIu32
                     " to %"PRIu32"/%"PRIu32" supported",
                     fie.stepwise.min.numerator, fie.stepwise.min.denominator,
                     fie.stepwise.max.numerator, fie.stepwise.max.denominator);
            if (fie.type == V4L2_FRMIVAL_TYPE_STEPWISE)
                msg_Dbg (obj, "  with %"PRIu32"/%"PRIu32" step",
                         fie.stepwise.step.numerator,
                         fie.stepwise.step.denominator);

            if (fcmp (&fie.stepwise.max, min_it) < 0)
            {
                *it = infinity;
                return -1;
            }

            if (fcmp (&fie.stepwise.min, min_it) >= 0)
            {
                *it = fie.stepwise.min;
                break;
            }

            if (fie.type == V4L2_FRMIVAL_TYPE_CONTINUOUS)
            {
                *it = *min_it;
                break;
            }

            it->numerator *= fie.stepwise.step.denominator;
            it->denominator *= fie.stepwise.step.denominator;
            while (fcmp (it, min_it) < 0)
                it->numerator += fie.stepwise.step.numerator;
            break;
    }
    return 0;
}

/**
 * Finds the best possible frame rate and resolution.
 * @param fourcc pixel format
 * @param fmt V4L2 capture format [OUT]
 * @param parm V4L2 capture streaming parameters [OUT]
 * @return 0 on success, -1 on failure.
 */
static int SetupFormat(vlc_object_t *obj, int fd, uint32_t fourcc,
                       struct v4l2_format *restrict fmt,
                       struct v4l2_streamparm *restrict parm)
{
    memset (fmt, 0, sizeof (*fmt));
    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    memset (parm, 0, sizeof (*parm));
    parm->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (v4l2_ioctl (fd, VIDIOC_G_FMT, fmt) < 0)
    {
        msg_Err (obj, "cannot get default format: %s", vlc_strerror_c(errno));
        return -1;
    }
    fmt->fmt.pix.pixelformat = fourcc;

    struct v4l2_frmsizeenum fse = {
        .pixel_format = fourcc,
    };
    struct v4l2_fract best_it = infinity, min_it;
    uint64_t best_area = 0;

    if (var_InheritURational(obj, &min_it.denominator, &min_it.numerator,
                             CFG_PREFIX"fps") == VLC_SUCCESS)
        msg_Dbg (obj, " requested frame interval: %"PRIu32"/%"PRIu32,
                 min_it.numerator, min_it.denominator);
    else
        min_it = zero;

    uint32_t width = var_InheritInteger (obj, CFG_PREFIX"width");
    uint32_t height = var_InheritInteger (obj, CFG_PREFIX"height");
    if (width > 0 && height > 0)
    {
        fmt->fmt.pix.width = width;
        fmt->fmt.pix.height = height;
        msg_Dbg (obj, " requested frame size: %"PRIu32"x%"PRIu32,
                 width, height);
        FindMaxRate (obj, fd, fmt, &min_it, &best_it);
    }
    else
    if (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMESIZES, &fse) < 0)
    {
        /* Fallback to current format, try to maximize frame rate */
        msg_Dbg (obj, " unknown frame sizes: %s", vlc_strerror_c(errno));
        msg_Dbg (obj, " current frame size: %"PRIu32"x%"PRIu32,
                 fmt->fmt.pix.width, fmt->fmt.pix.height);
        FindMaxRate (obj, fd, fmt, &min_it, &best_it);
    }
    else
    switch (fse.type)
    {
        case V4L2_FRMSIZE_TYPE_DISCRETE:
            do
            {
                struct v4l2_fract cur_it;

                msg_Dbg (obj, " frame size %"PRIu32"x%"PRIu32,
                         fse.discrete.width, fse.discrete.height);
                fmt->fmt.pix.width = fse.discrete.width;
                fmt->fmt.pix.height = fse.discrete.height;
                FindMaxRate (obj, fd, fmt, &min_it, &cur_it);

                int64_t c = fcmp (&cur_it, &best_it);
                uint64_t area = fse.discrete.width * fse.discrete.height;
                if (c < 0 || (c == 0 && area > best_area))
                {
                    best_it = cur_it;
                    best_area = area;
                    width = fmt->fmt.pix.width;
                    height = fmt->fmt.pix.height;
                }

                fse.index++;
            }
            while (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMESIZES, &fse) >= 0);

            fmt->fmt.pix.width = width;
            fmt->fmt.pix.height = height;
            msg_Dbg (obj, " best discrete frame size: %"PRIu32"x%"PRIu32,
                     fmt->fmt.pix.width, fmt->fmt.pix.height);
            break;

        case V4L2_FRMSIZE_TYPE_STEPWISE:
        case V4L2_FRMSIZE_TYPE_CONTINUOUS:
            msg_Dbg (obj, " frame sizes from %"PRIu32"x%"PRIu32" to "
                     "%"PRIu32"x%"PRIu32" supported",
                     fse.stepwise.min_width, fse.stepwise.min_height,
                     fse.stepwise.max_width, fse.stepwise.max_height);
            if (fse.type == V4L2_FRMSIZE_TYPE_STEPWISE)
                msg_Dbg (obj, "  with %"PRIu32"x%"PRIu32" steps",
                         fse.stepwise.step_width, fse.stepwise.step_height);

            /* FIXME: slow and dumb */
            for (fmt->fmt.pix.width =  fse.stepwise.min_width;
                 fmt->fmt.pix.width <= fse.stepwise.max_width;
                 fmt->fmt.pix.width += fse.stepwise.step_width)
                for (fmt->fmt.pix.height =  fse.stepwise.min_height;
                     fmt->fmt.pix.height <= fse.stepwise.max_height;
                     fmt->fmt.pix.height += fse.stepwise.step_height)
                {
                    struct v4l2_fract cur_it;

                    FindMaxRate (obj, fd, fmt, &min_it, &cur_it);

                    int64_t c = fcmp (&cur_it, &best_it);
                    uint64_t area = fmt->fmt.pix.width * fmt->fmt.pix.height;

                    if (c < 0 || (c == 0 && area > best_area))
                    {
                        best_it = cur_it;
                        best_area = area;
                        width = fmt->fmt.pix.width;
                        height = fmt->fmt.pix.height;
                    }
                }

            fmt->fmt.pix.width = width;
            fmt->fmt.pix.height = height;
            msg_Dbg (obj, " best frame size: %"PRIu32"x%"PRIu32,
                     fmt->fmt.pix.width, fmt->fmt.pix.height);
            break;
    }

    /* Set the final format */
    if (v4l2_ioctl (fd, VIDIOC_S_FMT, fmt) < 0)
    {
        msg_Err (obj, "cannot set format: %s", vlc_strerror_c(errno));
        return -1;
    }

    /* Now that the final format is set, fetch and override parameters */
    if (v4l2_ioctl (fd, VIDIOC_G_PARM, parm) < 0)
    {
        msg_Err (obj, "cannot get streaming parameters: %s",
                 vlc_strerror_c(errno));
        memset (parm, 0, sizeof (*parm));
        parm->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    parm->parm.capture.capturemode = 0; /* normal video mode */
    parm->parm.capture.extendedmode = 0;
    if (best_it.denominator != 0)
        parm->parm.capture.timeperframe = best_it;
    if (v4l2_ioctl (fd, VIDIOC_S_PARM, parm) < 0)
        msg_Warn (obj, "cannot set streaming parameters: %s",
                  vlc_strerror_c(errno));

    ResetCrop (obj, fd); /* crop depends on frame size */

    return 0;
}

typedef struct
{
    uint32_t v4l2;
    vlc_fourcc_t vlc; /**< VLC FOURCC, 0 if muxed bitstream */
    uint8_t bpp; /**< Bytes per pixel (first plane) */
} vlc_v4l2_fmt_t;

/* NOTE: Currently vlc_v4l2_fmt_rank() assumes format are sorted in order of
 * decreasing preference. */
static const vlc_v4l2_fmt_t v4l2_fmts[] =
{
    /* Planar YUV 4:2:0 */
    { V4L2_PIX_FMT_YUV420,  VLC_CODEC_I420, 1 },
    { V4L2_PIX_FMT_YVU420,  VLC_CODEC_YV12, 1 },
    { V4L2_PIX_FMT_YUV422P, VLC_CODEC_I422, 1 },
    /* Packed YUV 4:2:2 */
    { V4L2_PIX_FMT_YUYV,    VLC_CODEC_YUYV, 2 },
    { V4L2_PIX_FMT_UYVY,    VLC_CODEC_UYVY, 2 },
    { V4L2_PIX_FMT_YVYU,    VLC_CODEC_YVYU, 2 },
    { V4L2_PIX_FMT_VYUY,    VLC_CODEC_VYUY, 2 },

    { V4L2_PIX_FMT_YUV411P, VLC_CODEC_I411, 1 },

    { V4L2_PIX_FMT_YUV410,  VLC_CODEC_I410, 1 },
//  { V4L2_PIX_FMT_YVU410     },

    { V4L2_PIX_FMT_NV24,    VLC_CODEC_NV24, 1 },
    { V4L2_PIX_FMT_NV42,    VLC_CODEC_NV42, 1 },
    { V4L2_PIX_FMT_NV16,    VLC_CODEC_NV16, 1 },
    { V4L2_PIX_FMT_NV61,    VLC_CODEC_NV61, 1 },
    { V4L2_PIX_FMT_NV12,    VLC_CODEC_NV12, 1 },
    { V4L2_PIX_FMT_NV21,    VLC_CODEC_NV21, 1 },

    /* V4L2-documented but VLC-unsupported misc. YUV formats */
//  { V4L2_PIX_FMT_Y41P       },
//  { V4L2_PIX_FMT_NV12MT,    },
//  { V4L2_PIX_FMT_M420,      },

    /* Packed RGB */
    { V4L2_PIX_FMT_ABGR32,   VLC_CODEC_BGRA, 4 },
    { V4L2_PIX_FMT_ARGB32,   VLC_CODEC_ARGB, 4 },

    { V4L2_PIX_FMT_XBGR32,   VLC_CODEC_BGRX, 4 },
    { V4L2_PIX_FMT_XRGB32,   VLC_CODEC_XRGB, 4 },

    { V4L2_PIX_FMT_BGR32,    VLC_CODEC_BGRA, 4 },
    { V4L2_PIX_FMT_RGB32,    VLC_CODEC_ARGB, 4 },

    { V4L2_PIX_FMT_RGB24,   VLC_CODEC_RGB24, 3 },
    { V4L2_PIX_FMT_BGR24,   VLC_CODEC_BGR24, 3 },

    { V4L2_PIX_FMT_RGB565,   VLC_CODEC_RGB565LE, 2 },
    { V4L2_PIX_FMT_RGB565X,  VLC_CODEC_RGB565BE, 2 },

    { V4L2_PIX_FMT_XRGB555,  VLC_CODEC_RGB555LE, 2 },
    { V4L2_PIX_FMT_XRGB555X, VLC_CODEC_RGB555BE, 2  },

    // { V4L2_PIX_FMT_ARGB555,  VLC_CODEC_RGB555LE, 2 },
    // { V4L2_PIX_FMT_ARGB555X, VLC_CODEC_RGB555BE, 2 },

    // { V4L2_PIX_FMT_RGB555,   VLC_CODEC_RGB555LE, 2 },
    // { V4L2_PIX_FMT_RGB555X,  VLC_CODEC_RGB555BE, 2 },

    { V4L2_PIX_FMT_RGB332,  VLC_CODEC_RGB332,  1 },

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
    { V4L2_PIX_FMT_JPEG,    VLC_CODEC_MJPG, 0 },
    { V4L2_PIX_FMT_H264,    VLC_CODEC_H264, 0 },
    /* FIXME: fill p_extra for avc1... */
//  { V4L2_PIX_FMT_H264_NO_SC, VLC_FOURCC('a','v','c','1'), 0 }
    { V4L2_PIX_FMT_MPEG4,   VLC_CODEC_MP4V, 0 },
    { V4L2_PIX_FMT_XVID,    VLC_CODEC_MP4V, 0 },
    { V4L2_PIX_FMT_H263,    VLC_CODEC_H263, 0 },
    { V4L2_PIX_FMT_MPEG2,   VLC_CODEC_MPGV, 0 },
    { V4L2_PIX_FMT_MPEG1,   VLC_CODEC_MPGV, 0 },
    { V4L2_PIX_FMT_VC1_ANNEX_G, VLC_CODEC_VC1, 0 },
    { V4L2_PIX_FMT_VC1_ANNEX_L, VLC_CODEC_VC1, 0 },
    { V4L2_PIX_FMT_MPEG,    0,              0 },

    /* Reserved formats */
    { V4L2_PIX_FMT_MJPEG,   VLC_CODEC_MJPG, 0 },
    { V4L2_PIX_FMT_DV,      0,              0 },

    /* Grey scale */
    { V4L2_PIX_FMT_Y16,     VLC_CODEC_GREY_16L, 2 },
    { V4L2_PIX_FMT_Y12,     VLC_CODEC_GREY_12L, 2 },
    { V4L2_PIX_FMT_Y10,     VLC_CODEC_GREY_10L, 2 },
//  { V4L2_PIX_FMT_Y10BPACK,  },
    { V4L2_PIX_FMT_GREY,    VLC_CODEC_GREY, 1 },
};

static const vlc_v4l2_fmt_t *vlc_from_v4l2_fourcc (uint32_t fourcc)
{
     for (size_t i = 0; i < ARRAY_SIZE(v4l2_fmts); i++)
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
    assert (d < (ptrdiff_t)(ARRAY_SIZE(v4l2_fmts)));
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

int SetupVideo(vlc_object_t *obj, int fd, uint32_t caps,
               es_format_t *restrict es_fmt,
               uint32_t *restrict block_size, uint32_t *restrict block_flags)
{
    v4l2_std_id std;

    if (!(caps & V4L2_CAP_VIDEO_CAPTURE))
    {
        msg_Err(obj, "not a video capture device");
        return -1;
    }

    if (SetupInput(obj, fd, &std))
        return -1;

    /* Picture format negotiation */
    const vlc_v4l2_fmt_t *selected = NULL;
    vlc_fourcc_t reqfourcc = var_InheritFourCC(obj, CFG_PREFIX"chroma");
    bool native = false;

    for (struct v4l2_fmtdesc codec = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
         v4l2_ioctl (fd, VIDIOC_ENUM_FMT, &codec) >= 0;
         codec.index++)
    {   /* Enumerate available chromas */
        const vlc_v4l2_fmt_t *dsc = vlc_from_v4l2_fourcc(codec.pixelformat);

        msg_Dbg(obj, " %s %s format %4.4s (%4.4s): %s",
              (codec.flags & V4L2_FMT_FLAG_EMULATED) ? "emulates" : "supports",
              (codec.flags & V4L2_FMT_FLAG_COMPRESSED) ? "compressed" : "raw",
                (char *)&codec.pixelformat,
                (dsc != NULL) ? (const char *)&dsc->vlc : "N.A.",
                codec.description);

        if (dsc == NULL)
            continue; /* ignore VLC-unsupported codec */

        if (dsc->vlc == reqfourcc)
        {
            msg_Dbg(obj, "  matches the requested format");
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

        if (vlc_v4l2_fmt_rank (dsc) > vlc_v4l2_fmt_rank(selected))
            continue; /* ignore if rank is worse */

        selected = dsc;
    }

    if (selected == NULL)
    {
        msg_Err(obj, "cannot negotiate supported video format");
        return -1;
    }
    msg_Dbg(obj, "selected format %4.4s (%4.4s)",
            (const char *)&selected->v4l2, (const char *)&selected->vlc);

    /* Find best resolution and frame rate available */
    struct v4l2_format fmt;
    struct v4l2_streamparm parm;
    if (SetupFormat(obj, fd, selected->v4l2, &fmt, &parm))
        return -1;

    /* Print extra info */
    msg_Dbg(obj, "%d bytes maximum for complete image",
            fmt.fmt.pix.sizeimage);
    /* Check interlacing */
    *block_flags = 0;
    switch (fmt.fmt.pix.field)
    {
        case V4L2_FIELD_NONE:
            msg_Dbg(obj, "Interlacing setting: progressive");
            break;
        case V4L2_FIELD_TOP:
            msg_Dbg(obj, "Interlacing setting: top field only");
            *block_flags = BLOCK_FLAG_TOP_FIELD_FIRST
                           | BLOCK_FLAG_SINGLE_FIELD;
            break;
        case V4L2_FIELD_BOTTOM:
            msg_Dbg(obj, "Interlacing setting: bottom field only");
            *block_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST
                           | BLOCK_FLAG_SINGLE_FIELD;
            break;
        case V4L2_FIELD_INTERLACED:
            msg_Dbg(obj, "Interlacing setting: interleaved");
            /*if (NTSC)
                *block_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            else*/
                *block_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case V4L2_FIELD_SEQ_TB:
            msg_Dbg(obj, "Interlacing setting: sequential top bottom (TODO)");
            break;
        case V4L2_FIELD_SEQ_BT:
            msg_Dbg(obj, "Interlacing setting: sequential bottom top (TODO)");
            break;
        case V4L2_FIELD_ALTERNATE:
            msg_Dbg(obj, "Interlacing setting: alternate fields (TODO)");
            fmt.fmt.pix.height *= 2;
            break;
        case V4L2_FIELD_INTERLACED_TB:
            msg_Dbg(obj, "Interlacing setting: interleaved top bottom");
            *block_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case V4L2_FIELD_INTERLACED_BT:
            msg_Dbg(obj, "Interlacing setting: interleaved bottom top");
            *block_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            break;
        default:
            msg_Warn(obj, "Interlacing setting: unknown type (%d)",
                     fmt.fmt.pix.field);
            break;
    }

    /* Setup our unique elementary (video) stream format */
    es_format_Init(es_fmt, VIDEO_ES, selected->vlc);
    es_fmt->video.i_chroma = selected->vlc;
    es_fmt->video.i_visible_width = fmt.fmt.pix.width;
    if (fmt.fmt.pix.bytesperline != 0 && selected->bpp != 0)
        es_fmt->video.i_width = fmt.fmt.pix.bytesperline / selected->bpp;
    else
        es_fmt->video.i_width = fmt.fmt.pix.width;
    es_fmt->video.i_visible_height =
    es_fmt->video.i_height = fmt.fmt.pix.height;
    es_fmt->video.i_frame_rate = parm.parm.capture.timeperframe.denominator;
    es_fmt->video.i_frame_rate_base = parm.parm.capture.timeperframe.numerator;
    GetAR(fd, &es_fmt->video.i_sar_num, &es_fmt->video.i_sar_den);

    msg_Dbg(obj, "color primaries: %u", fmt.fmt.pix.colorspace);
    switch (fmt.fmt.pix.colorspace)
    {
        case V4L2_COLORSPACE_DEFAULT:
            break;
        case V4L2_COLORSPACE_SMPTE170M:
            es_fmt->video.primaries = COLOR_PRIMARIES_BT601_525;
            es_fmt->video.transfer = TRANSFER_FUNC_BT709;
            es_fmt->video.space = COLOR_SPACE_BT601;
            break;
        case V4L2_COLORSPACE_SMPTE240M: /* not supported */
            break;
        case V4L2_COLORSPACE_REC709:
            es_fmt->video.primaries = COLOR_PRIMARIES_BT709;
            es_fmt->video.transfer = TRANSFER_FUNC_BT709;
            es_fmt->video.space = COLOR_SPACE_BT709;
            break;
        case V4L2_COLORSPACE_470_SYSTEM_M:
            es_fmt->video.transfer = TRANSFER_FUNC_BT709;
            es_fmt->video.space = COLOR_SPACE_BT601;
            break;
        case V4L2_COLORSPACE_470_SYSTEM_BG:
            es_fmt->video.primaries = COLOR_PRIMARIES_BT601_625;
            es_fmt->video.transfer = TRANSFER_FUNC_BT709;
            es_fmt->video.space = COLOR_SPACE_BT601;
            break;
        case V4L2_COLORSPACE_JPEG:
            es_fmt->video.primaries = COLOR_PRIMARIES_SRGB;
            es_fmt->video.transfer = TRANSFER_FUNC_SRGB;
            es_fmt->video.space = COLOR_SPACE_BT601;
            es_fmt->video.color_range = COLOR_RANGE_FULL;
            break;
        case V4L2_COLORSPACE_SRGB:
            es_fmt->video.primaries = COLOR_PRIMARIES_SRGB;
            es_fmt->video.transfer = TRANSFER_FUNC_SRGB;
            es_fmt->video.space = COLOR_SPACE_UNDEF; /* sYCC unsupported */
            break;
        case V4L2_COLORSPACE_ADOBERGB: /* not supported */
            es_fmt->video.space = COLOR_SPACE_BT601;
            break;
        case V4L2_COLORSPACE_BT2020:
            es_fmt->video.primaries = COLOR_PRIMARIES_BT2020;
            es_fmt->video.transfer = TRANSFER_FUNC_BT2020;
            es_fmt->video.space = COLOR_SPACE_BT2020;
            break;
        case V4L2_COLORSPACE_RAW:
            es_fmt->video.transfer = TRANSFER_FUNC_LINEAR;
            break;
        case V4L2_COLORSPACE_DCI_P3:
            es_fmt->video.primaries = COLOR_PRIMARIES_DCI_P3;
            es_fmt->video.transfer = TRANSFER_FUNC_UNDEF;
            es_fmt->video.space = COLOR_SPACE_BT2020;
            break;
        default:
            msg_Warn(obj, "unknown color space %u", fmt.fmt.pix.colorspace);
            break;
    }

    msg_Dbg(obj, "transfer function: %u", fmt.fmt.pix.xfer_func);
    switch (fmt.fmt.pix.xfer_func)
    {
        case V4L2_XFER_FUNC_DEFAULT:
            /* If transfer function is default, the transfer function is
             * inferred from the colorspace value for backward compatibility.
             * See V4L2 documentation for details. */
            break;
        case V4L2_XFER_FUNC_709:
            es_fmt->video.transfer = TRANSFER_FUNC_BT709;
            break;
        case V4L2_XFER_FUNC_SRGB:
            es_fmt->video.transfer = TRANSFER_FUNC_SRGB;
            break;
        case V4L2_XFER_FUNC_ADOBERGB:
        case V4L2_XFER_FUNC_SMPTE240M:
            es_fmt->video.transfer = TRANSFER_FUNC_UNDEF;
            break;
        case V4L2_XFER_FUNC_NONE:
            es_fmt->video.transfer = TRANSFER_FUNC_LINEAR;
            break;
        case V4L2_XFER_FUNC_DCI_P3:
        case V4L2_XFER_FUNC_SMPTE2084:
            es_fmt->video.transfer = TRANSFER_FUNC_UNDEF;
            break;
        default:
            msg_Warn(obj, "unknown transfer function %u",
                     fmt.fmt.pix.xfer_func);
            break;
    }

    msg_Dbg(obj, "YCbCr encoding: %u", fmt.fmt.pix.ycbcr_enc);
    switch (fmt.fmt.pix.ycbcr_enc)
    {
        case V4L2_YCBCR_ENC_DEFAULT:
            /* Same as transfer function - use color space value */
            break;
        case V4L2_YCBCR_ENC_601:
            es_fmt->video.space = COLOR_SPACE_BT601;
            break;
        case V4L2_YCBCR_ENC_709:
            es_fmt->video.space = COLOR_SPACE_BT709;
            break;
        case V4L2_YCBCR_ENC_XV601:
        case V4L2_YCBCR_ENC_XV709:
        case V4L2_YCBCR_ENC_SYCC:
            break;
        case V4L2_YCBCR_ENC_BT2020:
            es_fmt->video.space = COLOR_SPACE_BT2020;
            break;
        case V4L2_YCBCR_ENC_BT2020_CONST_LUM:
        case V4L2_YCBCR_ENC_SMPTE240M:
            break;
        default:
            msg_Err(obj, "unknown YCbCr encoding: %u", fmt.fmt.pix.ycbcr_enc);
            break;
    }

    msg_Dbg(obj, "quantization: %u", fmt.fmt.pix.quantization);
    switch (fmt.fmt.pix.quantization)
    {
        case V4L2_QUANTIZATION_DEFAULT:
            break;
        case V4L2_QUANTIZATION_FULL_RANGE:
            es_fmt->video.color_range = COLOR_RANGE_FULL;
            break;
        case V4L2_QUANTIZATION_LIM_RANGE:
            es_fmt->video.color_range = COLOR_RANGE_LIMITED;
            break;
        default:
            msg_Err(obj, "unknown quantization: %u", fmt.fmt.pix.quantization);
            break;
    }

    msg_Dbg(obj, "added new video ES %4.4s %ux%u (%ux%u)",
            (char *)&es_fmt->i_codec,
            es_fmt->video.i_visible_width, es_fmt->video.i_visible_height,
            es_fmt->video.i_width, es_fmt->video.i_height);
    msg_Dbg(obj, " frame rate: %u/%u", es_fmt->video.i_frame_rate,
            es_fmt->video.i_frame_rate_base);
    msg_Dbg(obj, " aspect ratio: %u/%u", es_fmt->video.i_sar_num,
            es_fmt->video.i_sar_den);
    *block_size = fmt.fmt.pix.sizeimage;
    return 0;
}
