/*****************************************************************************
 * gstvlcvideosink.c: VLC gstreamer video sink
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
#include "gstvlcvideosink.h"

#include <vlc_common.h>

enum
{
    SIGNAL_NEW_CAPS,
    SIGNAL_NEW_BUFFER,
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_ALLOCATOR,
    PROP_ID,
    PROP_USE_POOL
};

static guint gst_vlc_video_sink_signals[ LAST_SIGNAL ] = { 0 };

static GstStaticPadTemplate sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
        GST_PAD_SINK,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS ("video/x-raw, "
            "framerate = (fraction) [ 0, MAX ], "
            "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

static gboolean gst_vlc_video_sink_setcaps( GstBaseSink *p_bsink,
        GstCaps *p_caps );
static gboolean gst_vlc_video_sink_propose_allocation( GstBaseSink *p_bsink,
        GstQuery *p_query);
static GstFlowReturn gst_vlc_video_sink_chain( GstBaseSink *p_vsink,
        GstBuffer *p_buffer );

static void gst_vlc_video_sink_set_property( GObject *p_object, guint prop_id,
        const GValue *p_value, GParamSpec *p_pspec );
static void gst_vlc_video_sink_get_property( GObject *p_object, guint prop_id,
        GValue *p_value, GParamSpec *p_pspec );
static void gst_vlc_video_sink_finalize( GObject *p_obj );

#define gst_vlc_video_sink_parent_class parent_class
G_DEFINE_TYPE( GstVlcVideoSink, gst_vlc_video_sink, GST_TYPE_BASE_SINK );

static void gst_vlc_video_sink_class_init( GstVlcVideoSinkClass *p_klass )
{
    GObjectClass *p_gobject_class;
    GstElementClass *p_gstelement_class;
    GstBaseSinkClass *p_gstbasesink_class;

    p_gobject_class = (GObjectClass*) p_klass;
    p_gstelement_class = (GstElementClass*) p_klass;
    p_gstbasesink_class = (GstBaseSinkClass*) p_klass;

    p_gobject_class->set_property = gst_vlc_video_sink_set_property;
    p_gobject_class->get_property = gst_vlc_video_sink_get_property;
    p_gobject_class->finalize = gst_vlc_video_sink_finalize;

    g_object_class_install_property( G_OBJECT_CLASS( p_klass ), PROP_USE_POOL,
            g_param_spec_boolean( "use-pool", "Use-Pool", "Use downstream VLC video output pool",
                FALSE, G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
                G_PARAM_STATIC_STRINGS ));

    g_object_class_install_property( G_OBJECT_CLASS( p_klass ), PROP_ALLOCATOR,
            g_param_spec_pointer( "allocator", "Allocator", "VlcPictureAllocator",
                G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
                G_PARAM_STATIC_STRINGS ));

    g_object_class_install_property( G_OBJECT_CLASS( p_klass ), PROP_ID,
            g_param_spec_pointer( "id", "Id", "ID",
                G_PARAM_WRITABLE | GST_PARAM_MUTABLE_READY |
                G_PARAM_STATIC_STRINGS ));

   //FIXME caps_signal: GObject signal didn't seem to work when return
   //value is expected, so went with the native callback mechanism here.
#if 0
    gst_vlc_video_sink_signals[ SIGNAL_NEW_CAPS ] =
        g_signal_new( "new-caps", G_TYPE_FROM_CLASS( p_klass ),
                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET( GstVlcVideoSinkClass,
                    new_caps ), g_signal_accumulator_true_handled,
                NULL, g_cclosure_marshal_generic,
                G_TYPE_BOOLEAN, 1, GST_TYPE_CAPS );
#endif
    gst_vlc_video_sink_signals[ SIGNAL_NEW_BUFFER ] =
        g_signal_new( "new-buffer", G_TYPE_FROM_CLASS( p_klass ),
                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET( GstVlcVideoSinkClass,
                    new_buffer ), NULL, NULL, g_cclosure_marshal_generic,
                G_TYPE_NONE, 1, GST_TYPE_BUFFER );

    gst_element_class_add_pad_template( p_gstelement_class,
            gst_static_pad_template_get( &sink_template ));

    gst_element_class_set_static_metadata( p_gstelement_class,
            "VLC Video Sink", "Sink/Video",
            "Video Sink for VLC video decoders",
            "Vikram Fugro <vikram.fugro@gmail.com>" );

    p_gstbasesink_class->set_caps = gst_vlc_video_sink_setcaps;
    p_gstbasesink_class->propose_allocation =
        gst_vlc_video_sink_propose_allocation;

    p_gstbasesink_class->render = gst_vlc_video_sink_chain;
}

static gboolean gst_vlc_video_sink_setcaps( GstBaseSink *p_basesink,
        GstCaps *p_caps )
{
    GstVlcVideoSink *p_vsink = GST_VLC_VIDEO_SINK( p_basesink );
    GstVideoInfo info;
    gboolean b_ret = FALSE;

   //FIXME caps_signal
#if 0
    GValue ret = { 0 };
    GValue args[2] = { {0}, {0} };
#endif

    if( !gst_video_info_from_caps( &info, p_caps ))
        return FALSE;

    p_vsink->vinfo = info;

    //FIXME caps_signal
#if 0
    g_value_init( &args[0], GST_TYPE_ELEMENT );
    g_value_set_object( &args[0], p_vsink );
    g_value_init( &args[1], GST_TYPE_CAPS );
    g_value_set_boxed( &args[1], p_caps );

    g_signal_emitv( args, gst_vlc_video_sink_signals[ SIGNAL_NEW_CAPS ],
            0, &b_ret );
#else
    b_ret = p_vsink->new_caps( GST_ELEMENT_CAST( p_vsink ), p_caps,
            (gpointer) p_vsink->p_dec );
#endif

    return b_ret;
}

static void gst_vlc_video_sink_init( GstVlcVideoSink *p_vlc_video_sink )
{
    p_vlc_video_sink->b_use_pool = FALSE;
    gst_base_sink_set_sync( GST_BASE_SINK( p_vlc_video_sink), FALSE );
}

static void gst_vlc_video_sink_finalize( GObject *p_obj )
{
    GstVlcVideoSink *p_vsink = GST_VLC_VIDEO_SINK( p_obj );

    if( p_vsink->p_allocator )
        gst_object_unref( p_vsink->p_allocator );

    G_OBJECT_CLASS( parent_class)->finalize( p_obj );
}

static GstVlcVideoPool* gst_vlc_video_sink_create_pool(
        GstVlcVideoSink *p_vsink, GstCaps *p_caps, gsize i_size, gint i_min )
{
    GstVlcVideoPool *p_pool;
    GstStructure *p_config;

    p_pool = gst_vlc_video_pool_new( p_vsink->p_allocator, p_vsink->p_dec );

    p_config = gst_buffer_pool_get_config( GST_BUFFER_POOL_CAST( p_pool ));
    gst_buffer_pool_config_set_params( p_config, p_caps, i_size, i_min, 0);

    if( !gst_buffer_pool_set_config( GST_BUFFER_POOL_CAST( p_pool ), p_config ))
        goto config_failed;

    return p_pool;

config_failed:
    {
        gst_object_unref (p_pool);
        return NULL;
    }
}

static gboolean gst_vlc_video_sink_propose_allocation( GstBaseSink* p_bsink,
        GstQuery* p_query )
{
    GstVlcVideoSink *p_vsink = GST_VLC_VIDEO_SINK( p_bsink );
    GstCaps *p_caps;
    gboolean b_need_pool;
    GstBufferPool* p_pool = NULL;
    gsize i_size;

    gst_query_parse_allocation (p_query, &p_caps, &b_need_pool);
    if( p_caps == NULL )
        goto no_caps;

    if( p_vsink->b_use_pool && b_need_pool )
    {
        GstVideoInfo info;

        if( !gst_video_info_from_caps( &info, p_caps ))
            goto invalid_caps;

        p_pool = (GstBufferPool*) gst_vlc_video_sink_create_pool( p_vsink,
                p_caps, info.size, 2 );
        if( p_pool == NULL )
            goto no_pool;

        i_size = GST_VIDEO_INFO_SIZE( &GST_VLC_VIDEO_POOL_CAST( p_pool )->info);
    }

    if( p_pool )
    {
        /* we need at least 2 buffer because we hold on to the last one */
        gst_query_add_allocation_pool( p_query, p_pool, i_size, 2, 0);
        gst_object_unref (p_pool);
    }

    /* we support various metadata */
    gst_query_add_allocation_meta( p_query, GST_VIDEO_META_API_TYPE, NULL );

    return TRUE;

    /* ERRORS */
no_pool:
    {
        msg_Err( p_vsink->p_dec, "failed to create the pool" );
        return FALSE;
    }
no_caps:
    {
        msg_Err( p_vsink->p_dec, "no caps in allocation query" );
        return FALSE;
    }
invalid_caps:
    {
        msg_Err( p_vsink->p_dec, "invalid caps in allocation query" );
        return FALSE;
    }
}

static GstFlowReturn gst_vlc_video_sink_chain( GstBaseSink *p_bsink,
        GstBuffer *p_buffer )
{
    g_signal_emit( p_bsink,
        gst_vlc_video_sink_signals[ SIGNAL_NEW_BUFFER ], 0, p_buffer );

    return GST_FLOW_OK;
}

static void gst_vlc_video_sink_set_property( GObject *p_object, guint i_prop_id,
        const GValue *p_value, GParamSpec *p_pspec )
{
    VLC_UNUSED( p_pspec );

    GstVlcVideoSink *p_vsink = GST_VLC_VIDEO_SINK( p_object );

    switch( i_prop_id )
    {
        case PROP_ALLOCATOR:
        {
            GstAllocator *p_allocator = (GstAllocator*) g_value_get_pointer(
                    p_value );
            if( GST_IS_VLC_PICTURE_PLANE_ALLOCATOR( p_allocator ))
            {
                if( p_vsink->p_allocator )
                    gst_object_unref( p_vsink->p_allocator );
                p_vsink->p_allocator = gst_object_ref( p_allocator );
            } else
                msg_Err( p_vsink->p_dec, "Invalid Allocator set");
        }
        break;

        case PROP_ID:
        {
            p_vsink->p_dec = (decoder_t*) g_value_get_pointer( p_value );
        }
        break;

        case PROP_USE_POOL:
        {
            p_vsink->b_use_pool = g_value_get_boolean( p_value );
        }
        break;

        default:
        break;
    }
}

static void gst_vlc_video_sink_get_property( GObject *p_object, guint i_prop_id,
    GValue *p_value, GParamSpec *p_pspec )
{
    VLC_UNUSED( p_pspec );

    GstVlcVideoSink *p_vsink = GST_VLC_VIDEO_SINK( p_object );

    switch( i_prop_id )
    {
        case PROP_ALLOCATOR:
            g_value_set_pointer( p_value, p_vsink->p_allocator );
        break;

        case PROP_USE_POOL:
            g_value_set_boolean( p_value, p_vsink->b_use_pool );
        break;

        default:
        break;
   }
}
