/*****************************************************************************
 * gstvlcvideopool.h: VLC pictures managed by GstBufferPool
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
#ifndef VLC_GST_VIDEO_POOL_H
#define VLC_GST_VIDEO_POOL_H

#include <gst/gstbufferpool.h>
#include <gst/video/gstvideopool.h>

#include "gstvlcpictureplaneallocator.h"

typedef struct _GstVlcVideoPool GstVlcVideoPool;
typedef struct _GstVlcVideoPoolClass GstVlcVideoPoolClass;

/* buffer pool functions */
#define GST_TYPE_VLC_VIDEO_POOL      (gst_vlc_video_pool_get_type())
#define GST_IS_VLC_VIDEO_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                        GST_TYPE_VLC_VIDEO_POOL))
#define GST_VLC_VIDEO_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                        GST_TYPE_VLC_VIDEO_POOL, \
                                        GstVlcVideoPool))
#define GST_VLC_VIDEO_POOL_CAST(obj) ((GstVlcVideoPool*)(obj))

struct _GstVlcVideoPool
{
    GstBufferPool bufferpool;
    GstVlcPicturePlaneAllocator *p_allocator;

    GstCaps *p_caps;
    GstVideoInfo info;
    GstVideoAlignment align;
    bool b_add_metavideo;
    bool b_need_aligned;

    decoder_t *p_dec;
};

struct _GstVlcVideoPoolClass
{
    GstBufferPoolClass parent_class;
};

GType gst_vlc_video_pool_get_type( void );
GstVlcVideoPool* gst_vlc_video_pool_new(
        GstAllocator *p_allocator, decoder_t *p_dec );

#endif /*__GST_VLC_VIDEO_POOL_H__*/
