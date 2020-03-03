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
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_codec.h>
#include <vlc_meta.h>

#include <vlc_image.h>
#include <vlc_filter.h>
#include <vlc_modules.h>

#include "../spu/mosaic.h"

/*****************************************************************************
 * Local structures
 *****************************************************************************/
typedef struct
{
    bridged_es_t *p_es;

    decoder_t       *p_decoder;
    image_handler_t *p_image; /* filter for resizing */
    int i_height, i_width;
    unsigned int i_sar_num, i_sar_den;
    char *psz_id;
    bool b_inited;

    vlc_fourcc_t i_chroma; /* force image format chroma */

    filter_chain_t *p_vf2;
} sout_stream_sys_t;

struct decoder_owner
{
    decoder_t dec;
    vlc_decoder_device *dec_dev;
    sout_stream_t *p_stream;
};

static inline struct decoder_owner *dec_get_owner( decoder_t *p_dec )
{
    return container_of( p_dec, struct decoder_owner, dec );
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void *Add( sout_stream_t *, const es_format_t * );
static void  Del( sout_stream_t *, void * );
static int   Send( sout_stream_t *, void *, block_t * );

static void decoder_queue_video( decoder_t *p_dec, picture_t *p_pic );
static int video_update_format_decoder( decoder_t *p_dec, vlc_video_context * );
static picture_t *video_new_buffer_filter( filter_t * );

static int HeightCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int WidthCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int alphaCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int xCallback( vlc_object_t *, char const *,
                      vlc_value_t, vlc_value_t, void * );
static int yCallback( vlc_object_t *, char const *,
                      vlc_value_t, vlc_value_t, void * );

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

#define CFG_PREFIX "sout-mosaic-bridge-"

vlc_module_begin ()
    set_shortname( N_( "Mosaic bridge" ) )
    set_description(N_("Mosaic bridge stream output") )
    set_capability( "sout output", 0 )
    add_shortcut( "mosaic-bridge" )

    add_string( CFG_PREFIX "id", "Id", ID_TEXT, ID_LONGTEXT,
                false )
    add_integer( CFG_PREFIX "width", 0, WIDTH_TEXT,
                 WIDTH_LONGTEXT, true )
    add_integer( CFG_PREFIX "height", 0, HEIGHT_TEXT,
                 HEIGHT_LONGTEXT, true )
    add_string( CFG_PREFIX "sar", "1:1", RATIO_TEXT,
                RATIO_LONGTEXT, false )
    add_string( CFG_PREFIX "chroma", NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                false )

    add_module_list(CFG_PREFIX "vfilter", "video filter", NULL,
                    VFILTER_TEXT, VFILTER_LONGTEXT)

    add_integer_with_range( CFG_PREFIX "alpha", 255, 0, 255,
                            ALPHA_TEXT, ALPHA_LONGTEXT, false )
    add_integer( CFG_PREFIX "x", -1, X_TEXT, X_LONGTEXT, false )
    add_integer( CFG_PREFIX "y", -1, Y_TEXT, Y_LONGTEXT, false )

    set_callbacks( Open, Close )
vlc_module_end ()

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
    p_sys->b_inited = false;

    p_sys->psz_id = var_CreateGetString( p_stream, CFG_PREFIX "id" );

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

    p_sys->i_chroma = 0;
    val.psz_string = var_GetNonEmptyString( p_stream, CFG_PREFIX "chroma" );
    if( val.psz_string && strlen( val.psz_string ) >= 4 )
    {
        memcpy( &p_sys->i_chroma, val.psz_string, 4 );
        msg_Dbg( p_stream, "Forcing image chroma to 0x%.8x (%4.4s)", p_sys->i_chroma, (char*)&p_sys->i_chroma );
    }
    free( val.psz_string );

#define INT_COMMAND( a ) do { \
    var_Create( p_stream, CFG_PREFIX #a, \
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND ); \
    var_AddCallback( p_stream, CFG_PREFIX #a, a ## Callback, \
                     p_stream ); } while(0)
    INT_COMMAND( alpha );
    INT_COMMAND( x );
    INT_COMMAND( y );

#undef INT_COMMAND

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;
    p_stream->pace_nocontrol = true;

    return VLC_SUCCESS;
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
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    return MosaicHoldDecoderDevice(p_owner);
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* Delete the callbacks */
    var_DelCallback( p_stream, CFG_PREFIX "height", HeightCallback, p_stream );
    var_DelCallback( p_stream, CFG_PREFIX "width", WidthCallback, p_stream );
    var_DelCallback( p_stream, CFG_PREFIX "alpha", alphaCallback, p_stream );
    var_DelCallback( p_stream, CFG_PREFIX "x", xCallback, p_stream );
    var_DelCallback( p_stream, CFG_PREFIX "y", yCallback, p_stream );

    free( p_sys->psz_id );

    free( p_sys );
}

static void ReleaseDecoder( decoder_t *p_dec )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    if ( p_owner->dec_dev )
    {
        vlc_decoder_device_Release( p_owner->dec_dev );
        p_owner->dec_dev = NULL;
    }
    decoder_Destroy( p_dec );
}

static vlc_decoder_device * video_filter_hold_device(vlc_object_t *o, void *sys)
{
    VLC_UNUSED(o);
    struct decoder_owner *p_owner = sys;
    return MosaicHoldDecoderDevice(p_owner);
}

static void *Add( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    bridge_t *p_bridge;
    bridged_es_t *p_es;
    char *psz_chain;
    int i;

    if( p_sys->b_inited || p_fmt->i_cat != VIDEO_ES )
        return NULL;

    /* Create decoder object */
    struct decoder_owner *p_owner = vlc_object_create( p_stream, sizeof( *p_owner ) );
    if( !p_owner )
        return NULL;
    p_sys->p_decoder = &p_owner->dec;
    decoder_Init( p_sys->p_decoder, p_fmt );

    p_sys->p_decoder->b_frame_drop_allowed = true;
    p_sys->p_decoder->fmt_out = p_sys->p_decoder->fmt_in;
    p_sys->p_decoder->fmt_out.i_extra = 0;
    p_sys->p_decoder->fmt_out.p_extra = 0;
    p_sys->p_decoder->pf_decode = NULL;

    /* Create user specified video filters */
    static const struct filter_video_callbacks cbs =
    {
        video_new_buffer_filter, video_filter_hold_device,
    };

    psz_chain = var_GetNonEmptyString( p_stream, CFG_PREFIX "vfilter" );
    msg_Dbg( p_stream, "psz_chain: '%s'", psz_chain ? psz_chain : "");
    if( psz_chain )
    {
        filter_owner_t owner = {
            .video = &cbs,
            .sys = p_owner,
        };

        p_sys->p_vf2 = filter_chain_NewVideo( p_stream, false, &owner );
        free( psz_chain );
    }
    else
    {
        p_sys->p_vf2 = NULL;
    }

    static const struct decoder_owner_callbacks dec_cbs =
    {
        .video = {
            .get_device = video_get_decoder_device,
            .format_update = video_update_format_decoder,
            .queue = decoder_queue_video,
        },
    };
    p_sys->p_decoder->cbs = &dec_cbs;

    p_owner->p_stream = p_stream;
    //p_sys->p_decoder->p_cfg = p_sys->p_video_cfg;

    p_sys->p_decoder->p_module =
        module_need_var( p_sys->p_decoder, "video decoder", "codec" );

    if( !p_sys->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find decoder" );
        ReleaseDecoder( p_sys->p_decoder );
        return NULL;
    }

    p_sys->b_inited = true;
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

    p_sys->p_es = p_es = p_bridge->pp_es[i];

    p_es->i_alpha = var_GetInteger( p_stream, CFG_PREFIX "alpha" );
    p_es->i_x = var_GetInteger( p_stream, CFG_PREFIX "x" );
    p_es->i_y = var_GetInteger( p_stream, CFG_PREFIX "y" );

    //p_es->fmt = *p_fmt;
    p_es->psz_id = p_sys->psz_id;
    p_es->p_picture = NULL;
    p_es->pp_last = &p_es->p_picture;
    p_es->b_empty = false;

    vlc_global_unlock( VLC_MOSAIC_MUTEX );

    if ( p_sys->i_height || p_sys->i_width )
    {
        p_sys->p_image = image_HandlerCreate( p_stream );
    }
    else
    {
        p_sys->p_image = NULL;
    }

    msg_Dbg( p_stream, "mosaic bridge id=%s pos=%d", p_es->psz_id, i );

    return p_sys;
}

static void Del( sout_stream_t *p_stream, void *id )
{
    VLC_UNUSED(id);
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    bridge_t *p_bridge;
    bridged_es_t *p_es;
    bool b_last_es = true;
    int i;

    if( !p_sys->b_inited )
        return;

    ReleaseDecoder( p_sys->p_decoder );

    /* Destroy user specified video filters */
    if( p_sys->p_vf2 )
        filter_chain_Delete( p_sys->p_vf2 );

    vlc_global_lock( VLC_MOSAIC_MUTEX );

    p_bridge = GetBridge( p_stream );
    p_es = p_sys->p_es;

    p_es->b_empty = true;
    while ( p_es->p_picture )
    {
        picture_t *p_next = p_es->p_picture->p_next;
        picture_Release( p_es->p_picture );
        p_es->p_picture = p_next;
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

    vlc_global_unlock( VLC_MOSAIC_MUTEX );

    if ( p_sys->p_image )
    {
        image_HandlerDelete( p_sys->p_image );
    }

    p_sys->b_inited = false;
}

static void decoder_queue_video( decoder_t *p_dec, picture_t *p_pic )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_t *p_stream = p_owner->p_stream;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    picture_t *p_new_pic;
    const video_format_t *p_fmt_in = &p_sys->p_decoder->fmt_out.video;

    if( p_sys->i_height || p_sys->i_width )
    {
        video_format_t fmt_out;

        video_format_Init( &fmt_out, p_sys->i_chroma ? p_sys->i_chroma : VLC_CODEC_I420 );

        const unsigned i_fmt_in_aspect =
            (int64_t)VOUT_ASPECT_FACTOR *
            p_fmt_in->i_sar_num * p_fmt_in->i_width /
            (p_fmt_in->i_sar_den * p_fmt_in->i_height);
        if ( !p_sys->i_height )
        {
            fmt_out.i_width = p_sys->i_width;
            fmt_out.i_height = (p_sys->i_width * VOUT_ASPECT_FACTOR
                * p_sys->i_sar_num / p_sys->i_sar_den / i_fmt_in_aspect)
                  & ~0x1;
        }
        else if ( !p_sys->i_width )
        {
            fmt_out.i_height = p_sys->i_height;
            fmt_out.i_width = (p_sys->i_height * i_fmt_in_aspect
                * p_sys->i_sar_den / p_sys->i_sar_num / VOUT_ASPECT_FACTOR)
                  & ~0x1;
        }
        else
        {
            fmt_out.i_width = p_sys->i_width;
            fmt_out.i_height = p_sys->i_height;
        }
        fmt_out.i_visible_width = fmt_out.i_width;
        fmt_out.i_visible_height = fmt_out.i_height;

        p_new_pic = image_Convert( p_sys->p_image,
                                   p_pic, p_fmt_in, &fmt_out );
        video_format_Clean( &fmt_out );
        if( p_new_pic == NULL )
        {
            msg_Err( p_stream, "image conversion failed" );
            picture_Release( p_pic );
            return;
        }
    }
    else
    {
        /* TODO: chroma conversion if needed */
        video_format_t pic_fmt = p_pic->format;
        pic_fmt.i_sar_num = p_fmt_in->i_sar_num;
        pic_fmt.i_sar_den = p_fmt_in->i_sar_den;

        p_new_pic = picture_NewFromFormat( &pic_fmt );
        if( !p_new_pic )
        {
            picture_Release( p_pic );
            msg_Err( p_stream, "image allocation failed" );
            return;
        }

        picture_Copy( p_new_pic, p_pic );
    }
    picture_Release( p_pic );

    if( p_sys->p_vf2 )
        p_new_pic = filter_chain_VideoFilter( p_sys->p_vf2, p_new_pic );

    /* push the picture in the mosaic-struct structure */
    bridged_es_t *p_es = p_sys->p_es;
    vlc_global_lock( VLC_MOSAIC_MUTEX );
    *p_es->pp_last = p_new_pic;
    p_new_pic->p_next = NULL;
    p_es->pp_last = &p_new_pic->p_next;
    vlc_global_unlock( VLC_MOSAIC_MUTEX );
}

static int Send( sout_stream_t *p_stream, void *id, block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if ( (sout_stream_sys_t *)id != p_sys )
    {
        block_ChainRelease( p_buffer );
        return VLC_SUCCESS;
    }

    int ret = p_sys->p_decoder->pf_decode( p_sys->p_decoder, p_buffer );
    return ret == VLCDEC_SUCCESS ? VLC_SUCCESS : VLC_EGENERIC;
}

static int video_update_format_decoder( decoder_t *p_dec, vlc_video_context *vctx )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_sys_t *p_sys = p_owner->p_stream->p_sys;
    if ( p_sys->p_vf2 )
    {
        // update the filter after the format changed/is known
        char *psz_chain = var_GetNonEmptyString( p_owner->p_stream, CFG_PREFIX "vfilter" );
        msg_Dbg( p_owner->p_stream, "update filter: '%s'",
                 psz_chain ?  psz_chain : "" );
        if( psz_chain )
        {
            es_format_t fmt;
            es_format_InitFromVideo( &fmt, &p_dec->fmt_out.video );
            if( p_sys->i_chroma )
            {
                fmt.video.i_chroma = p_sys->i_chroma;
                vctx = NULL; // CPU chroma, no video context
            }
            filter_chain_Reset( p_sys->p_vf2, &fmt, vctx, &fmt );
            es_format_Clean( &fmt );
            filter_chain_AppendFromString( p_sys->p_vf2, psz_chain );
            free( psz_chain );
        }
    }
    return 0;
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

    /* We create the handler before updating the value in p_sys
     * so we don't have to worry about locking */
    if( !p_sys->p_image && newval.i_int )
        p_sys->p_image = image_HandlerCreate( p_stream );
    p_sys->i_height = newval.i_int;

    return VLC_SUCCESS;
}

static int WidthCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);
    sout_stream_t *p_stream = (sout_stream_t *)p_data;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* We create the handler before updating the value in p_sys
     * so we don't have to worry about locking */
    if( !p_sys->p_image && newval.i_int )
        p_sys->p_image = image_HandlerCreate( p_stream );
    p_sys->i_width = newval.i_int;

    return VLC_SUCCESS;
}

static int alphaCallback( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);
    sout_stream_t *p_stream = (sout_stream_t *)p_data;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if( p_sys->p_es )
        p_sys->p_es->i_alpha = newval.i_int;

    return VLC_SUCCESS;
}

static int xCallback( vlc_object_t *p_this, char const *psz_var,
                      vlc_value_t oldval, vlc_value_t newval,
                      void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);
    sout_stream_t *p_stream = (sout_stream_t *)p_data;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if( p_sys->p_es )
        p_sys->p_es->i_x = newval.i_int;

    return VLC_SUCCESS;
}

static int yCallback( vlc_object_t *p_this, char const *psz_var,
                      vlc_value_t oldval, vlc_value_t newval,
                      void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);
    sout_stream_t *p_stream = (sout_stream_t *)p_data;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if( p_sys->p_es )
        p_sys->p_es->i_y = newval.i_int;

    return VLC_SUCCESS;
}
