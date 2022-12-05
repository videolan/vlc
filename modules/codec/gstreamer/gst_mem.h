/*****************************************************************************
 * gst_mem.h: GStreamer Memory picture context for VLC
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Author: Yann Lochet <yann@l0chet.fr>
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

#ifndef VLC_GST_MEM_H
#define VLC_GST_MEM_H

#include <vlc_picture.h>

#include <gst/gst.h>
#include <gst/video/video.h>

struct gst_mem_pic_context
{
    picture_context_t s;
    GstBuffer *p_buf;
};

#endif
