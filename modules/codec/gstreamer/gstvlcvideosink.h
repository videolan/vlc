/*****************************************************************************
 * gstvlcvideosink.h: VLC gstreamer video sink
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id:
 *
 * Author: Vikram Fugro <vikram.fugro@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifndef VLC_GST_VIDEO_SINK_H
#define VLC_GST_VIDEO_SINK_H

#include <gst/gst.h>
#include <gst/gstallocator.h>

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/base/gstbasesink.h>

#include <vlc_codec.h>

typedef struct _GstVlcVideoSink GstVlcVideoSink;
typedef struct _GstVlcVideoSinkClass GstVlcVideoSinkClass;

#define GST_TYPE_VLC_VIDEO_SINK \
    (gst_vlc_video_sink_get_type())
#define GST_VLC_VIDEO_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VLC_VIDEO_SINK, \
        GstVlcVideoSink))
#define GST_VLC_VIDEO_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VLC_VIDEO_SINK, \
        GstVlcVideoSinkClass))
#define GST_IS_VLC_VIDEO_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VLC_VIDEO_SINK))
#define GST_IS_VLC_VIDEO_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VLC_VIDEO_SINK))

struct _GstVlcVideoSink
{
    GstBaseSink parent;

    GstAllocator *p_allocator;
    GstVideoInfo vinfo;

    decoder_t *p_dec;

    //FIXME: caps_signal
    gboolean (*new_caps) ( GstElement *p_ele, GstCaps *p_caps,
            gpointer p_data );
};

struct _GstVlcVideoSinkClass
{
    GstBaseSinkClass parent_class;

    //FIXME: caps_signal
#if 0
    gboolean (*new_caps) ( GstElement *p_ele, GstCaps *p_caps,
            gpointer p_data );
#endif
    void (*new_buffer) ( GstElement *p_ele, GstBuffer *p_buffer,
            gpointer p_data );
};

GType gst_vlc_video_sink_get_type (void);

#endif /* __GST_VLC_VIDEO_SINK_H__ */
