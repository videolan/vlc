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
#include <sys/mman.h>

#include <vlc_common.h>
#include <vlc_block.h>

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

int SetupInput (vlc_object_t *obj, int fd, v4l2_std_id *std)
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
 * @param min_it minimum frame internal [IN]
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

#undef SetupFormat
/**
 * Finds the best possible frame rate and resolution.
 * @param fourcc pixel format
 * @param fmt V4L2 capture format [OUT]
 * @param parm V4L2 capture streaming parameters [OUT]
 * @return 0 on success, -1 on failure.
 */
int SetupFormat (vlc_object_t *obj, int fd, uint32_t fourcc,
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
        msg_Dbg (obj, " requested frame internal: %"PRIu32"/%"PRIu32,
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
                FindMaxRate (obj, fd, fmt, &min_it, &cur_it);

                int64_t c = fcmp (&cur_it, &best_it);
                uint64_t area = fse.discrete.width * fse.discrete.height;
                if (c < 0 || (c == 0 && area > best_area))
                {
                    best_it = cur_it;
                    best_area = area;
                    fmt->fmt.pix.width = fse.discrete.width;
                    fmt->fmt.pix.height = fse.discrete.height;
                }

                fse.index++;
            }
            while (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMESIZES, &fse) >= 0);

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
            for (width =  fse.stepwise.min_width;
                 width <= fse.stepwise.max_width;
                 width += fse.stepwise.step_width)
                for (height =  fse.stepwise.min_height;
                     height <= fse.stepwise.max_height;
                     height += fse.stepwise.step_height)
                {
                    struct v4l2_fract cur_it;

                    FindMaxRate (obj, fd, fmt, &min_it, &cur_it);

                    int64_t c = fcmp (&cur_it, &best_it);
                    uint64_t area = width * height;

                    if (c < 0 || (c == 0 && area > best_area))
                    {
                        best_it = cur_it;
                        best_area = area;
                        fmt->fmt.pix.width = width;
                        fmt->fmt.pix.height = height;
                    }
                }

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

vlc_tick_t GetBufferPTS (const struct v4l2_buffer *buf)
{
    vlc_tick_t pts;

    switch (buf->flags & V4L2_BUF_FLAG_TIMESTAMP_MASK)
    {
        case V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC:
            pts = vlc_tick_from_timeval( &buf->timestamp );
            break;
        case V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN:
        default:
            pts = vlc_tick_now ();
            break;
    }
    return pts;
}

/*****************************************************************************
 * GrabVideo: Grab a video frame
 *****************************************************************************/
block_t *GrabVideo (vlc_object_t *demux, int fd,
                    const struct buffer_t *restrict bufv)
{
    struct v4l2_buffer buf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    /* Wait for next frame */
    if (v4l2_ioctl (fd, VIDIOC_DQBUF, &buf) < 0)
    {
        switch (errno)
        {
            case EAGAIN:
                return NULL;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                msg_Err (demux, "dequeue error: %s", vlc_strerror_c(errno));
                return NULL;
        }
    }

    /* Copy frame */
    block_t *block = block_Alloc (buf.bytesused);
    if (unlikely(block == NULL))
        return NULL;
    block->i_pts = block->i_dts = GetBufferPTS (&buf);
    memcpy (block->p_buffer, bufv[buf.index].start, buf.bytesused);

    /* Unlock */
    if (v4l2_ioctl (fd, VIDIOC_QBUF, &buf) < 0)
    {
        msg_Err (demux, "queue error: %s", vlc_strerror_c(errno));
        block_Release (block);
        return NULL;
    }
    return block;
}

/**
 * Allocates user pointer buffers, and start streaming.
 */
int StartUserPtr (vlc_object_t *obj, int fd)
{
    struct v4l2_requestbuffers reqbuf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_USERPTR,
        .count = 2,
    };

    if (v4l2_ioctl (fd, VIDIOC_REQBUFS, &reqbuf) < 0)
    {
        msg_Dbg (obj, "cannot reserve user buffers: %s",
                 vlc_strerror_c(errno));
        return -1;
    }
    if (v4l2_ioctl (fd, VIDIOC_STREAMON, &reqbuf.type) < 0)
    {
        msg_Err (obj, "cannot start streaming: %s", vlc_strerror_c(errno));
        return -1;
    }
    return 0;
}

/**
 * Allocates memory-mapped buffers, queues them and start streaming.
 * @param n requested buffers count [IN], allocated buffers count [OUT]
 * @return array of allocated buffers (use free()), or NULL on error.
 */
struct buffer_t *StartMmap (vlc_object_t *obj, int fd, uint32_t *restrict n)
{
    struct v4l2_requestbuffers req = {
        .count = *n,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    if (v4l2_ioctl (fd, VIDIOC_REQBUFS, &req) < 0)
    {
        msg_Err (obj, "cannot allocate buffers: %s", vlc_strerror_c(errno));
        return NULL;
    }

    if (req.count < 2)
    {
        msg_Err (obj, "cannot allocate enough buffers");
        return NULL;
    }

    struct buffer_t *bufv = vlc_alloc (req.count, sizeof (*bufv));
    if (unlikely(bufv == NULL))
        return NULL;

    uint32_t bufc = 0;
    while (bufc < req.count)
    {
        struct v4l2_buffer buf = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
            .index = bufc,
        };

        if (v4l2_ioctl (fd, VIDIOC_QUERYBUF, &buf) < 0)
        {
            msg_Err (obj, "cannot query buffer %"PRIu32": %s", bufc,
                     vlc_strerror_c(errno));
            goto error;
        }

        bufv[bufc].start = v4l2_mmap (NULL, buf.length, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fd, buf.m.offset);
        if (bufv[bufc].start == MAP_FAILED)
        {
            msg_Err (obj, "cannot map buffer %"PRIu32": %s", bufc,
                     vlc_strerror_c(errno));
            goto error;
        }
        bufv[bufc].length = buf.length;
        bufc++;

        /* Some drivers refuse to queue buffers before they are mapped. Bug? */
        if (v4l2_ioctl (fd, VIDIOC_QBUF, &buf) < 0)
        {
            msg_Err (obj, "cannot queue buffer %"PRIu32": %s", bufc,
                     vlc_strerror_c(errno));
            goto error;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v4l2_ioctl (fd, VIDIOC_STREAMON, &type) < 0)
    {
        msg_Err (obj, "cannot start streaming: %s", vlc_strerror_c(errno));
        goto error;
    }
    *n = bufc;
    return bufv;
error:
    StopMmap (fd, bufv, bufc);
    return NULL;
}

void StopMmap (int fd, struct buffer_t *bufv, uint32_t bufc)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* STREAMOFF implicitly dequeues all buffers */
    v4l2_ioctl (fd, VIDIOC_STREAMOFF, &type);
    for (uint32_t i = 0; i < bufc; i++)
        v4l2_munmap (bufv[i].start, bufv[i].length);
    free (bufv);
}
