/*****************************************************************************
 * mosaic_bridge.c:
 *****************************************************************************
 * Copyright (C) 2004-2007 VLC authors and VideoLAN
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_codec.h>
#include <vlc_meta.h>

#include <vlc_filter.h>
#include <vlc_modules.h>

#include "../spu/mosaic.h"

/*****************************************************************************
 * Local structures
 *****************************************************************************/

struct decoder_owner
{
    decoder_t dec;
    es_format_t fmt_in;
    es_format_t fmt_out;
    vlc_decoder_device *dec_dev;
    vlc_video_context *vctx;
    sout_stream_t *p_stream;

    filter_chain_t *filters;
    bool need_filter_reset;

    bridged_es_t *p_es;
};

typedef struct
{
    struct decoder_owner *decoder_ref;
    int i_height, i_width;
    unsigned int i_sar_num, i_sar_den;
    char *psz_id;

    vlc_fourcc_t i_chroma; /* force image format chroma */

    char *filters_config;

    vlc_mutex_t var_lock;
} sout_stream_sys_t;

#define CFG_PREFIX "sout-mosaic-bridge-"

static inline struct decoder_owner *dec_get_owner( decoder_t *p_dec )
{
    return container_of( p_dec, struct decoder_owner, dec );
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void decoder_queue_video( decoder_t *p_dec, picture_t *p_pic );
static int video_update_format_decoder( decoder_t *p_dec, vlc_video_context * );
static picture_t *video_new_buffer_filter( filter_t * );

static int Control(sout_stream_t *stream, int query, va_list args)
{
    (void) stream;

    switch (query)
    {
        case SOUT_STREAM_IS_SYNCHRONOUS:
            *va_arg(args, bool *) = true;
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void Flush( sout_stream_t *stream, void *id)
{
    struct decoder_owner *owner = id;

    filter_chain_VideoFlush( owner->filters );

    (void)stream;
}

static vlc_decoder_device * MosaicHoldDecoderDevice( struct decoder_owner *p_owner )
{
    if ( p_owner->dec_dev == NULL )
    {
        p_owner->dec_dev = vlc_decoder_device_Create(&p_owner->dec.obj, NULL);
    }
    return p_owner->dec_dev ? vlc_decoder_device_Hold(p_owner->dec_dev) : NULL;
}

static vlc_decoder_device * video_get_decoder_device( decoder_t *p_dec )
{
    if( !var_InheritBool( p_dec, "hw-dec" ) )
        return NULL;

    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    return MosaicHoldDecoderDevice(p_owner);
}

static void ReleaseDecoder( decoder_t *p_dec )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    if ( p_owner->dec_dev )
    {
        vlc_decoder_device_Release( p_owner->dec_dev );
        p_owner->dec_dev = NULL;
    }
    es_format_Clean( &p_owner->fmt_in );
    es_format_Clean( &p_owner->fmt_out );
    decoder_Clean( p_dec );
    if ( p_owner->filters != NULL )
        filter_chain_Delete( p_owner->filters );
    if ( p_owner->vctx != NULL )
        vlc_video_context_Release( p_owner->vctx );
    vlc_object_delete( p_dec );
}


static vlc_decoder_device * video_filter_hold_device(vlc_object_t *o, void *sys)
{
    VLC_UNUSED(o);
    struct decoder_owner *p_owner = sys;
    return MosaicHoldDecoderDevice(p_owner);
}

static void *
Add( sout_stream_t *p_stream, const es_format_t *p_fmt, const char *es_id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    bridge_t *p_bridge;
    bridged_es_t *p_es;
    int i;

    if( p_sys->decoder_ref != NULL || p_fmt->i_cat != VIDEO_ES )
        return NULL;

    /* Create decoder object */
    struct decoder_owner *p_owner = vlc_object_create( p_stream, sizeof( *p_owner ) );
    if( !p_owner )
        return NULL;
    decoder_Init( &p_owner->dec, &p_owner->fmt_in, p_fmt );
    p_owner->dec.b_frame_drop_allowed = true;

    /* Create user specified video filters */
    static const struct filter_video_callbacks cbs =
    {
        video_new_buffer_filter, video_filter_hold_device,
    };

    msg_Dbg( p_stream,
             "User filter config chain: '%s'",
             (p_sys->filters_config != NULL) ? p_sys->filters_config : "" );
    filter_owner_t owner = {
        .video = &cbs,
        .sys = p_owner,
    };

    p_owner->filters = filter_chain_NewVideo( p_stream, false, &owner );
    if( unlikely(p_owner->filters == NULL) )
    {
        ReleaseDecoder(&p_owner->dec);
        return NULL;
    }
    p_owner->need_filter_reset = true;

    static const struct decoder_owner_callbacks dec_cbs =
    {
        .video = {
            .get_device = video_get_decoder_device,
            .format_update = video_update_format_decoder,
            .queue = decoder_queue_video,
        },
    };
    p_owner->dec.cbs = &dec_cbs;

    es_format_Init( &p_owner->fmt_out, VIDEO_ES, 0 );
    p_owner->p_stream = p_stream;
    p_owner->vctx = NULL;

    decoder_LoadModule( &p_owner->dec, false, true );

    if( p_owner->dec.p_module == NULL )
    {
        msg_Err( p_stream, "cannot find decoder" );
        ReleaseDecoder( &p_owner->dec );
        return NULL;
    }

    vlc_global_lock( VLC_MOSAIC_MUTEX );

    p_bridge = GetBridge( p_stream );
    if ( p_bridge == NULL )
    {
        vlc_object_t *p_libvlc = VLC_OBJECT( vlc_object_instance(p_stream) );
        vlc_value_t val;

        p_bridge = xmalloc( sizeof( bridge_t ) );

        var_Create( p_libvlc, "mosaic-struct", VLC_VAR_ADDRESS );
        val.p_address = p_bridge;
        var_Set( p_libvlc, "mosaic-struct", val );

        p_bridge->i_es_num = 0;
        p_bridge->pp_es = NULL;
    }

    for ( i = 0; i < p_bridge->i_es_num; i++ )
    {
        if ( p_bridge->pp_es[i]->b_empty )
            break;
    }

    if ( i == p_bridge->i_es_num )
    {
        p_bridge->pp_es = xrealloc( p_bridge->pp_es,
                          (p_bridge->i_es_num + 1) * sizeof(bridged_es_t *) );
        p_bridge->i_es_num++;
        p_bridge->pp_es[i] = xmalloc( sizeof(bridged_es_t) );
    }

    p_owner->p_es = p_es = p_bridge->pp_es[i];
    vlc_mutex_lock( &p_sys->var_lock );
    p_sys->decoder_ref = p_owner;
    vlc_mutex_unlock( &p_sys->var_lock );

    p_es->i_alpha = var_GetInteger( p_stream, CFG_PREFIX "alpha" );
    p_es->i_x = var_GetInteger( p_stream, CFG_PREFIX "x" );
    p_es->i_y = var_GetInteger( p_stream, CFG_PREFIX "y" );

    //p_es->fmt = *p_fmt;
    p_es->psz_id = p_sys->psz_id;
    vlc_picture_chain_Init( &p_es->pictures );
    p_es->b_empty = false;

    vlc_global_unlock( VLC_MOSAIC_MUTEX );

    msg_Dbg( p_stream, "mosaic bridge id=%s pos=%d", p_es->psz_id, i );

    (void)es_id;
    return p_owner;
}

static void Del( sout_stream_t *p_stream, void *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    struct decoder_owner *owner = id;
    bridge_t *p_bridge;
    bridged_es_t *p_es;
    bool b_last_es = true;
    int i;

    vlc_global_lock( VLC_MOSAIC_MUTEX );

    p_bridge = GetBridge( p_stream );
    p_es = owner->p_es;

    p_es->b_empty = true;
    while ( !vlc_picture_chain_IsEmpty( &p_es->pictures ) )
    {
        picture_t *es_picture = vlc_picture_chain_PopFront( &p_es->pictures );
        picture_Release( es_picture );
    }

    for ( i = 0; i < p_bridge->i_es_num; i++ )
    {
        if ( !p_bridge->pp_es[i]->b_empty )
        {
            b_last_es = false;
            break;
        }
    }

    if ( b_last_es )
    {
        vlc_object_t *p_libvlc = VLC_OBJECT( vlc_object_instance(p_stream) );
        for ( i = 0; i < p_bridge->i_es_num; i++ )
            free( p_bridge->pp_es[i] );
        free( p_bridge->pp_es );
        free( p_bridge );
        var_Destroy( p_libvlc, "mosaic-struct" );
    }

    vlc_mutex_lock( &p_sys->var_lock );
    p_sys->decoder_ref = NULL;
    vlc_mutex_unlock( &p_sys->var_lock );
    ReleaseDecoder( &owner->dec );

    vlc_global_unlock( VLC_MOSAIC_MUTEX );
}

static void ApplyRescale( video_format_t *dst,
                          const video_format_t *src,
                          const sout_stream_sys_t *sys )
{
    const unsigned dec_out_aspect = (int64_t)VOUT_ASPECT_FACTOR *
                                    src->i_sar_num * src->i_width /
                                    (src->i_sar_den * src->i_height);
    if ( sys->i_height == 0 )
    {
        dst->i_width = sys->i_width;
        dst->i_height = (sys->i_width * VOUT_ASPECT_FACTOR * sys->i_sar_num /
                         sys->i_sar_den / dec_out_aspect) &
                        ~0x1;
    }
    else if ( sys->i_width == 0 )
    {
        dst->i_height = sys->i_height;
        dst->i_width = (sys->i_height * dec_out_aspect * sys->i_sar_den /
                        sys->i_sar_num / VOUT_ASPECT_FACTOR) &
                       ~0x1;
    }
    else
    {
        dst->i_width = sys->i_width;
        dst->i_height = sys->i_height;
        dst->i_sar_num = sys->i_sar_num;
        dst->i_sar_den = sys->i_sar_den;
    }
    dst->i_visible_width = dst->i_width;
    dst->i_visible_height = dst->i_height;
}

static int ResetFilterChain(struct decoder_owner *owner)
{
    sout_stream_sys_t *sys = owner->p_stream->p_sys;
    es_format_t rescaled;
    es_format_InitFromVideo( &rescaled, &owner->fmt_out.video );

    rescaled.video.i_chroma = sys->i_chroma;
    if ( sys->i_width != 0 || sys->i_height != 0 )
        ApplyRescale( &rescaled.video, &owner->fmt_out.video, sys );

    filter_chain_Reset(
        owner->filters, &owner->fmt_out, owner->vctx, &rescaled );

    int status = filter_chain_AppendConverter( owner->filters, &rescaled );
    if ( status != VLC_SUCCESS )
        goto end;

    if ( sys->filters_config != NULL )
        status = filter_chain_AppendFromString( owner->filters, sys->filters_config );

    if ( status >= 0 )
    {
        owner->need_filter_reset = false;
        status = VLC_SUCCESS;
    }

end:
    es_format_Clean( &rescaled );
    return status;
}

static void decoder_queue_video( decoder_t *p_dec, picture_t *p_pic )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_sys_t *sys = p_owner->p_stream->p_sys;

    int status = VLC_SUCCESS;
    vlc_mutex_lock( &sys->var_lock );
    if ( p_owner->need_filter_reset )
    {
        status = ResetFilterChain( p_owner );
    }
    vlc_mutex_unlock( &sys->var_lock );

    if ( status != VLC_SUCCESS )
    {
        picture_Release( p_pic );
        return;
    }

    p_pic = filter_chain_VideoFilter( p_owner->filters, p_pic );

    /* push the picture in the mosaic-struct structure */
    bridged_es_t *p_es = p_owner->p_es;
    vlc_global_lock( VLC_MOSAIC_MUTEX );
    vlc_picture_chain_Append( &p_es->pictures, p_pic );
    vlc_global_unlock( VLC_MOSAIC_MUTEX );
}

static int Send( sout_stream_t *p_stream, void *id, vlc_frame_t *frame )
{
    struct decoder_owner *owner = id;

    int ret = owner->dec.pf_decode( &owner->dec, frame );
    return ret == VLCDEC_SUCCESS ? VLC_SUCCESS : VLC_EGENERIC;
    (void)p_stream;
}

static int video_update_format_decoder( decoder_t *p_dec, vlc_video_context *vctx )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );

    if ( video_format_IsSimilar(&p_dec->fmt_out.video,
                                &p_owner->fmt_out.video) &&
         vctx == p_owner->vctx )
        return VLC_SUCCESS;

    es_format_Clean( &p_owner->fmt_out );
    es_format_Copy( &p_owner->fmt_out, &p_dec->fmt_out );

    sout_stream_sys_t *sys = p_owner->p_stream->p_sys;
    vlc_mutex_lock( &sys->var_lock );
    if ( p_owner->vctx != NULL )
    {
        vlc_video_context_Release( p_owner->vctx );
        p_owner->vctx = NULL;
    }
    if ( vctx != NULL )
        p_owner->vctx = vlc_video_context_Hold( vctx );
    const int status = ResetFilterChain( p_owner );
    vlc_mutex_unlock( &sys->var_lock );
    return status;
}

static picture_t *video_new_buffer_filter( filter_t *p_filter )
{
    return picture_NewFromFormat( &p_filter->fmt_out.video );
}

/**********************************************************************
 * Callback to update (some) params on the fly
 **********************************************************************/
static int HeightCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);
    sout_stream_t *p_stream = (sout_stream_t *)p_data;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    vlc_mutex_lock( &p_sys->var_lock );
    struct decoder_owner *owner = p_sys->decoder_ref;
    if ( p_sys->i_height != newval.i_int && owner != NULL )
        owner->need_filter_reset = true;
    p_sys->i_height = newval.i_int;
    vlc_mutex_unlock( &p_sys->var_lock );

    return VLC_SUCCESS;
}

static int WidthCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);
    sout_stream_t *p_stream = (sout_stream_t *)p_data;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    vlc_mutex_lock( &p_sys->var_lock );
    struct decoder_owner *owner = p_sys->decoder_ref;
    if ( p_sys->i_width != newval.i_int && owner != NULL )
        owner->need_filter_reset = true;
    p_sys->i_width = newval.i_int;
    vlc_mutex_unlock( &p_sys->var_lock );

    return VLC_SUCCESS;
}

static int alphaCallback( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);
    sout_stream_t *p_stream = (sout_stream_t *)p_data;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    vlc_global_lock( VLC_MOSAIC_MUTEX );
    if( p_sys->decoder_ref && p_sys->decoder_ref->p_es )
        p_sys->decoder_ref->p_es->i_alpha = newval.i_int;
    vlc_global_unlock( VLC_MOSAIC_MUTEX );

    return VLC_SUCCESS;
}

static int xCallback( vlc_object_t *p_this, char const *psz_var,
                      vlc_value_t oldval, vlc_value_t newval,
                      void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);
    sout_stream_t *p_stream = (sout_stream_t *)p_data;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    vlc_global_lock( VLC_MOSAIC_MUTEX );
    if( p_sys->decoder_ref && p_sys->decoder_ref->p_es )
        p_sys->decoder_ref->p_es->i_x = newval.i_int;
    vlc_global_unlock( VLC_MOSAIC_MUTEX );

    return VLC_SUCCESS;
}

static int yCallback( vlc_object_t *p_this, char const *psz_var,
                      vlc_value_t oldval, vlc_value_t newval,
                      void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);
    sout_stream_t *p_stream = (sout_stream_t *)p_data;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    vlc_global_lock( VLC_MOSAIC_MUTEX );
    if( p_sys->decoder_ref && p_sys->decoder_ref->p_es )
        p_sys->decoder_ref->p_es->i_y = newval.i_int;
    vlc_global_unlock( VLC_MOSAIC_MUTEX );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* Delete the callbacks */
    var_DelCallback( p_stream, CFG_PREFIX "height", HeightCallback, p_stream );
    var_DelCallback( p_stream, CFG_PREFIX "width", WidthCallback, p_stream );
    var_DelCallback( p_stream, CFG_PREFIX "alpha", alphaCallback, p_stream );
    var_DelCallback( p_stream, CFG_PREFIX "x", xCallback, p_stream );
    var_DelCallback( p_stream, CFG_PREFIX "y", yCallback, p_stream );

    free( p_sys->psz_id );
    free( p_sys->filters_config );

    free( p_sys );
}

static const struct sout_stream_operations ops = {
    .add = Add,
    .del = Del,
    .send = Send,
    .control = Control,
    .flush = Flush,
    .close = Close,
};

static const char *const ppsz_sout_options[] = {
    "id", "width", "height", "sar", "vfilter", "chroma", "alpha", "x", "y", NULL
};

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t        *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t    *p_sys;
    vlc_value_t           val;

    config_ChainParse( p_stream, CFG_PREFIX, ppsz_sout_options,
                       p_stream->p_cfg );

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_stream->p_sys = p_sys;

    p_sys->psz_id = var_CreateGetString( p_stream, CFG_PREFIX "id" );

    p_sys->decoder_ref = NULL;

    p_sys->i_height =
        var_CreateGetIntegerCommand( p_stream, CFG_PREFIX "height" );
    var_AddCallback( p_stream, CFG_PREFIX "height", HeightCallback, p_stream );

    p_sys->i_width =
        var_CreateGetIntegerCommand( p_stream, CFG_PREFIX "width" );
    var_AddCallback( p_stream, CFG_PREFIX "width", WidthCallback, p_stream );

    var_Get( p_stream, CFG_PREFIX "sar", &val );
    if( val.psz_string )
    {
        char *psz_parser = strchr( val.psz_string, ':' );

        if( psz_parser )
        {
            *psz_parser++ = '\0';
            p_sys->i_sar_num = atoi( val.psz_string );
            p_sys->i_sar_den = atoi( psz_parser );
            vlc_ureduce( &p_sys->i_sar_num, &p_sys->i_sar_den,
                         p_sys->i_sar_num, p_sys->i_sar_den, 0 );
        }
        else
        {
            msg_Warn( p_stream, "bad aspect ratio %s", val.psz_string );
            p_sys->i_sar_num = p_sys->i_sar_den = 1;
        }

        free( val.psz_string );
    }
    else
    {
        p_sys->i_sar_num = p_sys->i_sar_den = 1;
    }

    p_sys->i_chroma = VLC_CODEC_I420;
    val.psz_string = var_GetNonEmptyString( p_stream, CFG_PREFIX "chroma" );
    if( val.psz_string && strlen( val.psz_string ) >= 4 )
    {
        memcpy( &p_sys->i_chroma, val.psz_string, 4 );
        msg_Dbg( p_stream, "Forcing image chroma to 0x%.8x (%4.4s)", p_sys->i_chroma, (char*)&p_sys->i_chroma );
        if ( vlc_fourcc_GetChromaDescription( p_sys->i_chroma ) == NULL )
        {
            msg_Err( p_stream, "Unknown chroma" );
            free( val.psz_string );
            free( p_sys );
            return VLC_EINVAL;
        }
    }
    free( val.psz_string );

    p_sys->filters_config = var_GetNonEmptyString( p_stream, CFG_PREFIX "vfilter" );

    vlc_mutex_init( &p_sys->var_lock );

#define INT_COMMAND( a ) do { \
    var_Create( p_stream, CFG_PREFIX #a, \
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND ); \
    var_AddCallback( p_stream, CFG_PREFIX #a, a ## Callback, \
                     p_stream ); } while(0)
    INT_COMMAND( alpha );
    INT_COMMAND( x );
    INT_COMMAND( y );

#undef INT_COMMAND

    p_stream->ops = &ops;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ID_TEXT N_("ID")
#define ID_LONGTEXT N_( \
    "Specify an identifier string for this subpicture" )

#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_( \
    "Output video width." )
#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_( \
    "Output video height." )
#define RATIO_TEXT N_("Sample aspect ratio")
#define RATIO_LONGTEXT N_( \
    "Sample aspect ratio of the destination (1:1, 3:4, 2:3)." )

#define VFILTER_TEXT N_("Video filter")
#define VFILTER_LONGTEXT N_( \
    "Video filters will be applied to the video stream." )

#define CHROMA_TEXT N_("Image chroma")
#define CHROMA_LONGTEXT N_( \
    "Force the use of a specific chroma. Use YUVA if you're planning " \
    "to use the Alphamask or Bluescreen video filter." )

#define ALPHA_TEXT N_("Transparency")
#define ALPHA_LONGTEXT N_( \
    "Transparency of the mosaic picture." )

#define X_TEXT N_("X offset")
#define X_LONGTEXT N_( \
    "X coordinate of the upper left corner in the mosaic if non negative." )

#define Y_TEXT N_("Y offset")
#define Y_LONGTEXT N_( \
    "Y coordinate of the upper left corner in the mosaic if non negative." )

vlc_module_begin ()
    set_shortname( N_( "Mosaic bridge" ) )
    set_description(N_("Mosaic bridge stream output") )
    set_capability( "sout output", 0 )
    add_shortcut( "mosaic-bridge" )

    set_subcategory( SUBCAT_SOUT_STREAM )

    add_string( CFG_PREFIX "id", "Id", ID_TEXT, ID_LONGTEXT )
    add_integer( CFG_PREFIX "width", 0, WIDTH_TEXT,
                 WIDTH_LONGTEXT )
    add_integer( CFG_PREFIX "height", 0, HEIGHT_TEXT,
                 HEIGHT_LONGTEXT )
    add_string( CFG_PREFIX "sar", "1:1", RATIO_TEXT,
                RATIO_LONGTEXT )
    add_string( CFG_PREFIX "chroma", NULL, CHROMA_TEXT, CHROMA_LONGTEXT )

    add_module_list(CFG_PREFIX "vfilter", "video filter", NULL,
                    VFILTER_TEXT, VFILTER_LONGTEXT)

    add_integer_with_range( CFG_PREFIX "alpha", 255, 0, 255,
                            ALPHA_TEXT, ALPHA_LONGTEXT )
    add_integer( CFG_PREFIX "x", -1, X_TEXT, X_LONGTEXT )
    add_integer( CFG_PREFIX "y", -1, Y_TEXT, Y_LONGTEXT )

    set_callback( Open )
vlc_module_end ()
