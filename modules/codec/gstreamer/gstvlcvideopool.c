/*****************************************************************************
 * gstvlcvideopool.c: VLC pictures managed by GstBufferPool
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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
#include "gstvlcvideopool.h"

#include <vlc_common.h>

/* bufferpool */
static void gst_vlc_video_pool_finalize( GObject *p_object );

#define gst_vlc_video_pool_parent_class parent_class
G_DEFINE_TYPE (GstVlcVideoPool, gst_vlc_video_pool,
    GST_TYPE_BUFFER_POOL);

static const gchar** gst_vlc_video_pool_get_options (GstBufferPool *p_pool)
{
    VLC_UNUSED( p_pool );

    static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT, NULL
    };

    return options;
}

static gboolean gst_vlc_video_pool_set_config( GstBufferPool *p_pool,
        GstStructure *p_config )
{
    GstVlcVideoPool *p_vpool = GST_VLC_VIDEO_POOL_CAST( p_pool );
    GstCaps *p_caps;
    GstVideoInfo info;
    GstVideoAlignment align;
    guint size, min_buffers, max_buffers;
    GstAllocator *p_allocator;
    GstAllocationParams params;

    if( !gst_buffer_pool_config_get_params( p_config, &p_caps, &size,
                &min_buffers, &max_buffers ))
        goto wrong_config;
    if( p_caps == NULL )
        goto no_caps;

    gst_buffer_pool_config_get_allocator( p_config, &p_allocator, &params );
    if( p_allocator )
    {
        if( !GST_IS_VLC_PICTURE_PLANE_ALLOCATOR( p_allocator ))
            goto unsupported_allocator;
        else
        {
            if( p_vpool->p_allocator )
                gst_object_unref( p_vpool->p_allocator );
            p_vpool->p_allocator = gst_object_ref ( p_allocator );
        }
    }

    /* now parse the caps from the config */
    if ( !gst_video_info_from_caps( &info, p_caps ))
        goto wrong_caps;

    /* enable metadata based on config of the pool */
    p_vpool->b_add_metavideo =
        gst_buffer_pool_config_has_option( p_config,
                GST_BUFFER_POOL_OPTION_VIDEO_META );

    p_vpool->b_need_aligned =
        gst_buffer_pool_config_has_option( p_config,
                GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT );

    if( p_vpool->b_need_aligned )
    {
        p_vpool->b_add_metavideo = true;
        gst_buffer_pool_config_get_video_alignment( p_config, &align );
    }
    else
         gst_video_alignment_reset( &align );

    // FIXME: the gst decoders' min buffers may not be equal to the number
    // of buffers it actually allocates. Also the max buffers here could
    // be zero. Moreover even if it was right, need to check if it can be
    // communicated to the vout (including the dpb_size it calculates in
    // src/input/decoder.c).
    p_vpool->p_dec->i_extra_picture_buffers = 16;

    if( !gst_vlc_picture_plane_allocator_query_format( p_vpool->p_allocator,
                &info, &align, p_caps))
        goto unknown_format;

    if( p_vpool->b_need_aligned )
        gst_buffer_pool_config_set_video_alignment( p_config, &align);

    if( p_vpool->p_caps )
        gst_caps_unref( p_vpool->p_caps );
    p_vpool->p_caps = gst_caps_ref( p_caps );
    p_vpool->info = info;
    p_vpool->align = align;

    msg_Dbg( p_vpool->p_dec, "setting the following config on the pool: %s, \
            size: %lu, min buffers: %u, max buffers: %u", gst_caps_to_string( p_caps ),
            info.size, min_buffers, max_buffers );

    gst_buffer_pool_config_set_params( p_config, p_caps, info.size,
            min_buffers, max_buffers );

    return GST_BUFFER_POOL_CLASS (parent_class)->set_config( p_pool, p_config );

    /* ERRORS */
wrong_config:
    {
        msg_Err(p_vpool->p_dec, "wrong pool config" );
        return FALSE;
    }
no_caps:
    {
        msg_Err(p_vpool->p_dec, "no input caps in config" );
        return FALSE;
    }
wrong_caps:
    {
        msg_Err(p_vpool->p_dec, "invalid caps" );
        return FALSE;
    }
unknown_format:
    {
        msg_Err(p_vpool->p_dec, "format unsupported" );
        return FALSE;
    }
unsupported_allocator:
    {
        msg_Err(p_vpool->p_dec, "allocator unsupported" );
        return FALSE;
    }
}

static GstFlowReturn gst_vlc_video_pool_acquire_buffer( GstBufferPool *p_pool,
        GstBuffer **p_buffer, GstBufferPoolAcquireParams *p_params )
{
    GstVlcVideoPool *p_vpool = GST_VLC_VIDEO_POOL_CAST( p_pool );
    GstFlowReturn result;

    result = GST_BUFFER_POOL_CLASS( parent_class)->acquire_buffer( p_pool,
            p_buffer, p_params );

    if( result == GST_FLOW_OK &&
            !gst_vlc_picture_plane_allocator_hold( p_vpool->p_allocator,
                *p_buffer ))
        result = GST_FLOW_EOS;

    return result;
}

static void gst_vlc_video_pool_release_buffer( GstBufferPool *p_pool,
        GstBuffer *p_buffer )
{
    GstVlcVideoPool* p_vpool = GST_VLC_VIDEO_POOL_CAST( p_pool );

    gst_vlc_picture_plane_allocator_release( p_vpool->p_allocator, p_buffer );

    GST_BUFFER_POOL_CLASS( parent_class )->release_buffer( p_pool, p_buffer );

    return;
}

static void gst_vlc_video_pool_free_buffer( GstBufferPool *p_pool,
        GstBuffer *p_buffer )
{
    GstVlcVideoPool* p_vpool = GST_VLC_VIDEO_POOL_CAST( p_pool );

    gst_vlc_picture_plane_allocator_release( p_vpool->p_allocator, p_buffer );

    msg_Dbg( p_vpool->p_dec, "freed buffer %p", p_buffer );

    GST_BUFFER_POOL_CLASS( parent_class )->free_buffer( p_pool, p_buffer );

    return;
}

static GstFlowReturn gst_vlc_video_pool_alloc_buffer( GstBufferPool *p_pool,
        GstBuffer **p_buffer, GstBufferPoolAcquireParams *p_params)
{
    VLC_UNUSED( p_params );

    GstVlcVideoPool *p_vpool = GST_VLC_VIDEO_POOL_CAST( p_pool );
    GstVideoInfo *p_info = &p_vpool->info;

    *p_buffer = gst_buffer_new( );

    if( !gst_vlc_picture_plane_allocator_alloc( p_vpool->p_allocator,
                *p_buffer ))
    {
        msg_Err( p_vpool->p_dec, "buffer allocation failed" );
        return GST_FLOW_EOS;
    }

    if( p_vpool->b_add_metavideo )
    {
        msg_Dbg( p_vpool->p_dec, "meta video enabled" );
        gst_buffer_add_video_meta_full( *p_buffer, GST_VIDEO_FRAME_FLAG_NONE,
                GST_VIDEO_INFO_FORMAT( p_info ), GST_VIDEO_INFO_WIDTH( p_info ),
                GST_VIDEO_INFO_HEIGHT( p_info ),
                GST_VIDEO_INFO_N_PLANES( p_info ),
                p_info->offset, p_info->stride );
    }

    msg_Dbg( p_vpool->p_dec, "allocated buffer %p", *p_buffer );

    return GST_FLOW_OK;
}

static gboolean gst_vlc_video_pool_start( GstBufferPool *p_pool )
{
    GstVlcVideoPool *p_vpool = GST_VLC_VIDEO_POOL_CAST( p_pool );

    if( !gst_vlc_set_vout_fmt( &p_vpool->info, &p_vpool->align,
                p_vpool->p_caps, p_vpool->p_dec ))
        return FALSE;

    return GST_BUFFER_POOL_CLASS( parent_class )->start( p_pool );
}

static void gst_vlc_video_pool_class_init( GstVlcVideoPoolClass *p_klass )
{
    GObjectClass *p_gobject_class = ( GObjectClass* )p_klass;
    GstBufferPoolClass *p_gstbufferpool_class = ( GstBufferPoolClass* )p_klass;

    p_gobject_class->finalize = gst_vlc_video_pool_finalize;

    p_gstbufferpool_class->start = gst_vlc_video_pool_start;
    p_gstbufferpool_class->get_options = gst_vlc_video_pool_get_options;
    p_gstbufferpool_class->set_config = gst_vlc_video_pool_set_config;
    p_gstbufferpool_class->alloc_buffer = gst_vlc_video_pool_alloc_buffer;
    p_gstbufferpool_class->free_buffer = gst_vlc_video_pool_free_buffer;
    p_gstbufferpool_class->acquire_buffer = gst_vlc_video_pool_acquire_buffer;
    p_gstbufferpool_class->release_buffer = gst_vlc_video_pool_release_buffer;
}

static void gst_vlc_video_pool_init( GstVlcVideoPool *p_pool )
{
    VLC_UNUSED( p_pool );
}

static void gst_vlc_video_pool_finalize( GObject *p_object )
{
    GstVlcVideoPool *p_pool = GST_VLC_VIDEO_POOL_CAST( p_object );

    gst_object_unref( p_pool->p_allocator );

    G_OBJECT_CLASS( parent_class )->finalize( p_object );
}

GstVlcVideoPool* gst_vlc_video_pool_new(
    GstAllocator *p_allocator, decoder_t *p_dec )
{
    GstVlcVideoPool *p_pool;

    if( !GST_IS_VLC_PICTURE_PLANE_ALLOCATOR( p_allocator ))
    {
        msg_Err( p_dec, "unspported allocator for pool" );
        return NULL;
    }

    p_pool = g_object_new( GST_TYPE_VLC_VIDEO_POOL, NULL );
    p_pool->p_allocator = gst_object_ref( p_allocator );
    p_pool->p_dec = p_dec;

    return p_pool;
}
