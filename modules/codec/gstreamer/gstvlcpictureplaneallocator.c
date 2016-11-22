/*****************************************************************************
 * gstvlcpictureplaneallocator.c: VLC pictures wrapped by GstAllocator
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
#include <gst/gst.h>

#include "gstvlcpictureplaneallocator.h"

#include <vlc_common.h>

#define gst_vlc_picture_plane_allocator_parent_class parent_class
G_DEFINE_TYPE (GstVlcPicturePlaneAllocator, gst_vlc_picture_plane_allocator, \
        GST_TYPE_ALLOCATOR);

static void gst_vlc_picture_plane_allocator_finalize( GObject *p_object );
static GstMemory* gst_vlc_picture_plane_allocator_dummy_alloc(
        GstAllocator* p_allocator, gsize i_size,
        GstAllocationParams *p_params );
static void gst_vlc_picture_plane_allocator_free( GstAllocator *p_allocator,
        GstMemory *p_gmem);
static gpointer gst_vlc_picture_plane_map( GstMemory *p_gmem,
        gsize i_maxsize, GstMapFlags flags );
static gboolean gst_vlc_picture_plane_unmap( GstVlcPicturePlane *p_mem );
static GstMemory* gst_vlc_picture_plane_copy(
        GstVlcPicturePlane *p_mem, gssize i_offset, gssize i_size );

#define GST_VLC_PICTURE_PLANE_ALLOCATOR_NAME "vlcpictureplane"

static void gst_vlc_picture_plane_allocator_class_init(
    GstVlcPicturePlaneAllocatorClass *p_klass )
{
    GObjectClass *p_gobject_class;
    GstAllocatorClass *p_allocator_class;

    p_gobject_class = (GObjectClass*) p_klass;
    p_allocator_class = (GstAllocatorClass*) p_klass;

    p_gobject_class->finalize = gst_vlc_picture_plane_allocator_finalize;

    p_allocator_class->alloc = gst_vlc_picture_plane_allocator_dummy_alloc;
    p_allocator_class->free = gst_vlc_picture_plane_allocator_free;
}

static void gst_vlc_picture_plane_allocator_init(
        GstVlcPicturePlaneAllocator *p_allocator )
{
    GstAllocator *p_alloc = GST_ALLOCATOR_CAST( p_allocator );

    p_alloc->mem_type = GST_VLC_PICTURE_PLANE_ALLOCATOR_NAME;
    p_alloc->mem_map = (GstMemoryMapFunction) gst_vlc_picture_plane_map;
    p_alloc->mem_unmap = (GstMemoryUnmapFunction) gst_vlc_picture_plane_unmap;
    p_alloc->mem_copy = (GstMemoryShareFunction) gst_vlc_picture_plane_copy;
    /* fallback is_span */

    GST_OBJECT_FLAG_SET( p_allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC );
}

static void gst_vlc_picture_plane_allocator_finalize( GObject *p_object )
{
    GstVlcPicturePlaneAllocator *p_alloc = GST_VLC_PICTURE_PLANE_ALLOCATOR(
            p_object );
    VLC_UNUSED( p_alloc );

    G_OBJECT_CLASS (parent_class)->finalize( p_object );
}

static GstMemory* gst_vlc_picture_plane_allocator_dummy_alloc(
        GstAllocator* p_allocator, gsize i_size, GstAllocationParams *p_params )
{
    VLC_UNUSED( p_allocator );
    VLC_UNUSED( i_size );
    VLC_UNUSED( p_params );
    return NULL;
}

static void gst_vlc_picture_plane_allocator_free( GstAllocator *p_allocator,
        GstMemory *p_gmem)
{
    VLC_UNUSED( p_allocator );
    GstVlcPicturePlane *p_mem = (GstVlcPicturePlane*) p_gmem;
    g_slice_free( GstVlcPicturePlane, p_mem );
}

static gpointer gst_vlc_picture_plane_map( GstMemory *p_gmem,
    gsize i_maxsize, GstMapFlags flags )
{
    VLC_UNUSED( i_maxsize );
    VLC_UNUSED( flags );
    GstVlcPicturePlane* p_mem = (GstVlcPicturePlane*) p_gmem;

    if( p_mem->p_pic )
        return (gpointer) (p_mem->p_plane->p_pixels + p_mem->parent.offset);
    else
        return NULL;
}

static gboolean gst_vlc_picture_plane_unmap(
        GstVlcPicturePlane *p_mem )
{
    VLC_UNUSED( p_mem );
    return TRUE;
}

static GstMemory* gst_vlc_picture_plane_copy(
        GstVlcPicturePlane *p_mem, gssize i_offset, gssize i_size )
{
    VLC_UNUSED( p_mem );
    VLC_UNUSED( i_offset );
    VLC_UNUSED( i_size );
    return NULL;
}

void gst_vlc_picture_plane_allocator_release(
    GstVlcPicturePlaneAllocator *p_allocator, GstBuffer *p_buffer )
{
    VLC_UNUSED( p_allocator );

    GstVlcPicturePlane* p_mem =
        (GstVlcPicturePlane*) gst_buffer_peek_memory( p_buffer, 0 );
    guint i_plane;

    if( p_mem->p_pic )
    {
        picture_Release( p_mem->p_pic );

        for( i_plane = 0; i_plane < gst_buffer_n_memory( p_buffer );
                i_plane++ )
        {
            p_mem = (GstVlcPicturePlane*) gst_buffer_peek_memory ( p_buffer,
                    i_plane );
            p_mem->p_pic = NULL;
            p_mem->p_plane = NULL;
        }
    }
}

bool gst_vlc_picture_plane_allocator_hold(
    GstVlcPicturePlaneAllocator *p_allocator, GstBuffer *p_buffer )
{
    picture_t* p_pic = NULL;
    decoder_t* p_dec = p_allocator->p_dec;
    GstVlcPicturePlane *p_mem;
    int i_plane;

    if( !decoder_UpdateVideoFormat( p_dec ) )
        p_pic = decoder_NewPicture( p_dec );
    if( !p_pic )
    {
        msg_Err( p_allocator->p_dec, "failed to acquire picture from vout" );
        return false;
    }

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_mem = (GstVlcPicturePlane*) gst_buffer_peek_memory ( p_buffer,
                i_plane );
        p_mem->p_pic = p_pic;
        p_mem->p_plane = &p_pic->p[ i_plane ];
    }

    return true;
}

bool gst_vlc_picture_plane_allocator_alloc(
        GstVlcPicturePlaneAllocator *p_allocator,
        GstBuffer *p_buffer )
{
    int i_plane;
    gsize i_max_size, i_align, i_offset, i_size;
    picture_t *p_pic;

    p_pic = &p_allocator->pic_info;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        GstVlcPicturePlane *p_mem =
            (GstVlcPicturePlane*) g_slice_new0( GstVlcPicturePlane );

        i_size = p_pic->p[ i_plane ].i_pitch *
            p_pic->p[ i_plane ].i_lines;
        i_max_size = p_pic->p[ i_plane ].i_pitch *
            p_pic->p[ i_plane ].i_lines;
        i_align = 0;
        i_offset = 0;

        gst_memory_init( GST_MEMORY_CAST( p_mem ), GST_MEMORY_FLAG_NO_SHARE,
                GST_ALLOCATOR_CAST( p_allocator ), NULL, i_max_size,
                i_align, i_offset, i_size );
        gst_buffer_append_memory( p_buffer, (GstMemory*) p_mem );
    }

    return true;
}

bool gst_vlc_set_vout_fmt( GstVideoInfo *p_info, GstVideoAlignment *p_align,
        GstCaps *p_caps, decoder_t *p_dec )
{
    es_format_t *p_outfmt = &p_dec->fmt_out;
    video_format_t *p_voutfmt = &p_dec->fmt_out.video;
    GstStructure *p_str = gst_caps_get_structure( p_caps, 0 );
    vlc_fourcc_t i_chroma;
    int i_padded_width, i_padded_height;

    i_chroma = p_outfmt->i_codec = vlc_fourcc_GetCodecFromString(
            VIDEO_ES,
            gst_structure_get_string( p_str, "format" ) );
    if( !i_chroma )
    {
        msg_Err( p_dec, "video chroma type not supported" );
        return false;
    }

    i_padded_width = GST_VIDEO_INFO_WIDTH( p_info ) + p_align->padding_left +
        p_align->padding_right;
    i_padded_height = GST_VIDEO_INFO_HEIGHT( p_info ) + p_align->padding_top +
        p_align->padding_bottom;

    video_format_Setup( p_voutfmt, i_chroma, i_padded_width, i_padded_height,
            GST_VIDEO_INFO_WIDTH( p_info ), GST_VIDEO_INFO_HEIGHT( p_info ),
            GST_VIDEO_INFO_PAR_N( p_info ), GST_VIDEO_INFO_PAR_D( p_info ));
    p_voutfmt->i_x_offset = p_align->padding_left;
    p_voutfmt->i_y_offset = p_align->padding_top;

    p_voutfmt->i_frame_rate = GST_VIDEO_INFO_FPS_N( p_info );
    p_voutfmt->i_frame_rate_base = GST_VIDEO_INFO_FPS_D( p_info );

    return true;
}

static bool gst_vlc_video_info_from_vout( GstVideoInfo *p_info,
        GstVideoAlignment *p_align, GstCaps *p_caps, decoder_t *p_dec,
        picture_t *p_pic_info )
{
    const GstVideoFormatInfo *p_vinfo = p_info->finfo;
    picture_t *p_pic = NULL;
    int i;

    /* Ensure the queue is empty */
    gst_vlc_dec_ensure_empty_queue( p_dec );
    gst_video_info_align( p_info, p_align );

    if( !gst_vlc_set_vout_fmt( p_info, p_align, p_caps, p_dec ))
    {
        msg_Err( p_dec, "failed to set output format to vout" );
        return false;
    }

    /* Acquire a picture and release it. This is to get the picture
     * stride/offsets info for the Gstreamer decoder looking to use
     * downstream bufferpool directly; Zero-Copy */
    if( !decoder_UpdateVideoFormat( p_dec ) )
        p_pic = decoder_NewPicture( p_dec );
    if( !p_pic )
    {
        msg_Err( p_dec, "failed to acquire picture from vout; for pic info" );
        return false;
    }

    /* reject if strides don't match */
    for( i = 0; i < p_pic->i_planes; i++ )
        if( p_info->stride[i] != p_pic->p[i].i_pitch )
            goto strides_mismatch;

    p_info->offset[0] = 0;
    for( i = 1; i < p_pic->i_planes; i++ )
    {
        p_info->offset[i] = p_info->offset[i-1] +
            p_pic->p[i-1].i_pitch * p_pic->p[i-1].i_lines;
    }
    GST_VIDEO_INFO_SIZE( p_info ) = p_info->offset[i-1] +
        p_pic->p[i-1].i_pitch * p_pic->p[i-1].i_lines;

    for( i = 0; i < p_pic->i_planes; i++ )
    {
        int i_v_edge, i_h_edge;

        i_h_edge =
            GST_VIDEO_FORMAT_INFO_SCALE_WIDTH( p_vinfo, i,
                    p_align->padding_left);
        i_v_edge =
            GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT( p_vinfo, i,
                    p_align->padding_top);

        p_info->offset[i] += ( i_v_edge * p_info->stride[i] ) +
            ( i_h_edge * GST_VIDEO_FORMAT_INFO_PSTRIDE( p_vinfo, i ));
    }

    memcpy( p_pic_info, p_pic, sizeof( picture_t ));
    picture_Release( p_pic );

    return true;

strides_mismatch:
    msg_Err( p_dec, "strides mismatch" );
    picture_Release( p_pic );
    return false;
}

bool gst_vlc_picture_plane_allocator_query_format(
        GstVlcPicturePlaneAllocator *p_allocator, GstVideoInfo *p_info,
        GstVideoAlignment *p_align, GstCaps *p_caps )
{
    decoder_t *p_dec = p_allocator->p_dec;
    video_format_t v_fmt;
    picture_t *p_pic_info = &p_allocator->pic_info;

    /* Back up the original format; as this is just a query  */
    v_fmt = p_dec->fmt_out.video;
    video_format_Init( &p_dec->fmt_out.video, 0 );

    bool b_ret =
        gst_vlc_video_info_from_vout( p_info, p_align, p_caps, p_dec,
                p_pic_info );

    video_format_Clean( &p_dec->fmt_out.video );

    /* Restore the original format; as this was just a query  */
    p_dec->fmt_out.video = v_fmt;

    if( !b_ret )
    {
        msg_Err( p_allocator->p_dec, "failed to get the vout info" );
        return false;
    }

    return true;
}

GstVlcPicturePlaneAllocator* gst_vlc_picture_plane_allocator_new(
        decoder_t *p_dec )
{
    GstVlcPicturePlaneAllocator *p_allocator;

    p_allocator = g_object_new( GST_TYPE_VLC_PICTURE_PLANE_ALLOCATOR, NULL);
    p_allocator->p_dec = p_dec;

    return p_allocator;
}
