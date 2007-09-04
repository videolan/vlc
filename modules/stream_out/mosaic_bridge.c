/*****************************************************************************
 * mosaic_bridge.c:
 *****************************************************************************
 * Copyright (C) 2004-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */

#include <vlc/vlc.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_codec.h>

#include <vlc_image.h>
#include <vlc_filter.h>

#include "../video_filter/mosaic.h"

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct sout_stream_sys_t
{
    bridged_es_t *p_es;
    vlc_mutex_t *p_lock;

    decoder_t       *p_decoder;
    image_handler_t *p_image; /* filter for resizing */
    int i_height, i_width;
    unsigned int i_sar_num, i_sar_den;
    char *psz_id;
    vlc_bool_t b_inited;

    int i_chroma; /* force image format chroma */

    filter_t **pp_vfilters;
    int i_vfilters;
};

#define PICTURE_RING_SIZE 4
struct decoder_owner_sys_t
{
    picture_t *pp_pics[PICTURE_RING_SIZE];

    /* Current format in use by the output */
    video_format_t video;
};

typedef void (* pf_release_t)( picture_t * );
static void ReleasePicture( picture_t *p_pic )
{
    p_pic->i_refcount--;

    if ( p_pic->i_refcount <= 0 )
    {
        if ( p_pic->p_sys != NULL )
        {
            pf_release_t pf_release = (pf_release_t)p_pic->p_sys;
            p_pic->p_sys = NULL;
            pf_release( p_pic );
        }
        else
        {
            if( p_pic && p_pic->p_data_orig ) free( p_pic->p_data_orig );
            if( p_pic ) free( p_pic );
        }
    }
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, block_t * );

inline static void video_del_buffer_decoder( decoder_t *, picture_t * );
inline static void video_del_buffer_filter( filter_t *, picture_t * );
static void video_del_buffer( vlc_object_t *, picture_t * );

inline static picture_t *video_new_buffer_decoder( decoder_t * );
inline static picture_t *video_new_buffer_filter( filter_t * );
static picture_t *video_new_buffer( vlc_object_t *, decoder_owner_sys_t *,
                                    es_format_t *, void (*)( picture_t * ) );

static void video_link_picture_decoder( decoder_t *, picture_t * );
static void video_unlink_picture_decoder( decoder_t *, picture_t * );
static int MosaicBridgeCallback( vlc_object_t *, char const *,
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
    "Video filters will be applied to the video stream." );

#define CHROMA_TEXT N_("Image chroma")
#define CHROMA_LONGTEXT N_( \
    "Force the use of a specific chroma. Use YUVA if you're planning " \
    "to use the Alphamask or Bluescreen video filter." );

#define CFG_PREFIX "sout-mosaic-bridge-"

vlc_module_begin();
    set_shortname( _( "Mosaic bridge" ) );
    set_description(_("Mosaic bridge stream output") );
    set_capability( "sout stream", 0 );
    add_shortcut( "mosaic-bridge" );

    add_string( CFG_PREFIX "id", "Id", NULL, ID_TEXT, ID_LONGTEXT,
                VLC_FALSE );
    add_integer( CFG_PREFIX "width", 0, NULL, WIDTH_TEXT,
                 WIDTH_LONGTEXT, VLC_TRUE );
    add_integer( CFG_PREFIX "height", 0, NULL, HEIGHT_TEXT,
                 HEIGHT_LONGTEXT, VLC_TRUE );
    add_string( CFG_PREFIX "sar", "1:1", NULL, RATIO_TEXT,
                RATIO_LONGTEXT, VLC_FALSE );
    add_string( CFG_PREFIX "chroma", 0, NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                VLC_FALSE );

    add_module_list( CFG_PREFIX "vfilter", "video filter2",
                     NULL, NULL, VFILTER_TEXT, VFILTER_LONGTEXT, VLC_FALSE );

    set_callbacks( Open, Close );
vlc_module_end();

static const char *ppsz_sout_options[] = {
    "id", "width", "height", "sar", "vfilter", "chroma", NULL
};

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t        *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t    *p_sys;
    vlc_object_t         *p_libvlc = VLC_OBJECT( p_this->p_libvlc );
    vlc_value_t           val;

    config_ChainParse( p_stream, CFG_PREFIX, ppsz_sout_options,
                       p_stream->p_cfg );

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    if( !p_sys )
    {
        return VLC_ENOMEM;
    }

    p_stream->p_sys = p_sys;
    p_sys->b_inited = VLC_FALSE;

    var_Create( p_libvlc, "mosaic-lock", VLC_VAR_MUTEX );
    var_Get( p_libvlc, "mosaic-lock", &val );
    p_sys->p_lock = val.p_address;

    var_Get( p_stream, CFG_PREFIX "id", &val );
    p_sys->psz_id = val.psz_string;

    p_sys->i_height =
        var_CreateGetIntegerCommand( p_stream, CFG_PREFIX "height" );
    var_AddCallback( p_stream, CFG_PREFIX "height", MosaicBridgeCallback,
                     p_stream );

    p_sys->i_width =
        var_CreateGetIntegerCommand( p_stream, CFG_PREFIX "width" );
    var_AddCallback( p_stream, CFG_PREFIX "width", MosaicBridgeCallback,
                     p_stream );

    var_Get( p_stream, CFG_PREFIX "sar", &val );
    if ( val.psz_string )
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

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    p_stream->p_sout->i_out_pace_nocontrol--;

    if ( p_sys->psz_id )
        free( p_sys->psz_id );

    free( p_sys );
}

static sout_stream_id_t * Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    bridge_t *p_bridge;
    bridged_es_t *p_es;
    char *psz_chain, *psz_parser;
    int i;

    if ( p_sys->b_inited )
    {
        return NULL;
    }

    /* Create decoder object */
    p_sys->p_decoder = vlc_object_create( p_stream, VLC_OBJECT_DECODER );
    vlc_object_attach( p_sys->p_decoder, p_stream );
    p_sys->p_decoder->p_module = NULL;
    p_sys->p_decoder->fmt_in = *p_fmt;
    p_sys->p_decoder->b_pace_control = VLC_FALSE;
    p_sys->p_decoder->fmt_out = p_sys->p_decoder->fmt_in;
    p_sys->p_decoder->fmt_out.i_extra = 0;
    p_sys->p_decoder->fmt_out.p_extra = 0;
    p_sys->p_decoder->pf_decode_video = 0;
    p_sys->p_decoder->pf_vout_buffer_new = video_new_buffer_decoder;
    p_sys->p_decoder->pf_vout_buffer_del = video_del_buffer_decoder;
    p_sys->p_decoder->pf_picture_link    = video_link_picture_decoder;
    p_sys->p_decoder->pf_picture_unlink  = video_unlink_picture_decoder;
    p_sys->p_decoder->p_owner = malloc( sizeof(decoder_owner_sys_t) );
    for( i = 0; i < PICTURE_RING_SIZE; i++ )
        p_sys->p_decoder->p_owner->pp_pics[i] = 0;
    p_sys->p_decoder->p_owner->video = p_fmt->video;
    //p_sys->p_decoder->p_cfg = p_sys->p_video_cfg;

    p_sys->p_decoder->p_module =
        module_Need( p_sys->p_decoder, "decoder", "$codec", 0 );

    if( !p_sys->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find decoder" );
        vlc_object_detach( p_sys->p_decoder );
        vlc_object_destroy( p_sys->p_decoder );
        return NULL;
    }

    p_sys->b_inited = VLC_TRUE;
    vlc_mutex_lock( p_sys->p_lock );

    p_bridge = GetBridge( p_stream );
    if ( p_bridge == NULL )
    {
        vlc_object_t *p_libvlc = VLC_OBJECT( p_stream->p_libvlc );
        vlc_value_t val;

        p_bridge = malloc( sizeof( bridge_t ) );

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
        p_bridge->pp_es = realloc( p_bridge->pp_es,
                                   (p_bridge->i_es_num + 1)
                                     * sizeof(bridged_es_t *) );
        p_bridge->i_es_num++;
        p_bridge->pp_es[i] = malloc( sizeof(bridged_es_t) );
    }

    p_sys->p_es = p_es = p_bridge->pp_es[i];

    //p_es->fmt = *p_fmt;
    p_es->psz_id = p_sys->psz_id;
    p_es->p_picture = NULL;
    p_es->pp_last = &p_es->p_picture;
    p_es->b_empty = VLC_FALSE;

    vlc_mutex_unlock( p_sys->p_lock );

    if ( p_sys->i_height || p_sys->i_width )
    {
        p_sys->p_image = image_HandlerCreate( p_stream );
    }
    else
    {
        p_sys->p_image = NULL;
    }

    msg_Dbg( p_stream, "mosaic bridge id=%s pos=%d", p_es->psz_id, i );

    /* Create user specified video filters */
    psz_chain = var_GetNonEmptyString( p_stream, CFG_PREFIX "vfilter" );
    msg_Dbg( p_stream, "psz_chain: %s\n", psz_chain );
    {
        config_chain_t *p_cfg;
        for( p_cfg = p_stream->p_cfg; p_cfg != NULL; p_cfg = p_cfg->p_next )
        {
            msg_Dbg( p_stream, " - %s\n", p_cfg->psz_value );
        }
    }
    p_sys->i_vfilters = 0;
    p_sys->pp_vfilters = NULL;
    psz_parser = psz_chain;
    while( psz_parser && *psz_parser )
    {
        config_chain_t *p_cfg;
        char *psz_name;
        filter_t **pp_vfilter;
        psz_parser = config_ChainCreate( &psz_name, &p_cfg, psz_parser );
        p_sys->i_vfilters++;
        p_sys->pp_vfilters =
            (filter_t **)realloc( p_sys->pp_vfilters,
                                  p_sys->i_vfilters * sizeof(filter_t *) );
        pp_vfilter = p_sys->pp_vfilters+(p_sys->i_vfilters - 1);
        *pp_vfilter = vlc_object_create( p_stream, VLC_OBJECT_FILTER );
        vlc_object_attach( *pp_vfilter, p_stream );
        (*pp_vfilter)->pf_vout_buffer_new = video_new_buffer_filter;
        (*pp_vfilter)->pf_vout_buffer_del = video_del_buffer_filter;
        (*pp_vfilter)->fmt_in = p_sys->p_decoder->fmt_out;
        if( p_sys->i_chroma )
            (*pp_vfilter)->fmt_in.video.i_chroma = p_sys->i_chroma;
        (*pp_vfilter)->fmt_out = (*pp_vfilter)->fmt_in;
        (*pp_vfilter)->p_cfg = p_cfg;
        (*pp_vfilter)->p_module =
            module_Need( *pp_vfilter, "video filter2", psz_name, VLC_TRUE );
        if( (*pp_vfilter)->p_module )
        {
            /* It worked! */
            (*pp_vfilter)->p_owner = (filter_owner_sys_t *)
                p_sys->p_decoder->p_owner;
            msg_Err( p_stream, "Added video filter %s to the chain",
                     psz_name );
        }
        else
        {
            /* Crap ... we didn't find a filter */
            msg_Warn( p_stream,
                      "no video filter matching name \"%s\" found",
                      psz_name );
            vlc_object_detach( *pp_vfilter );
            vlc_object_destroy( *pp_vfilter );
            p_sys->i_vfilters--;
        }
    }
    free( psz_chain );

    return (sout_stream_id_t *)p_sys;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    bridge_t *p_bridge;
    bridged_es_t *p_es;
    vlc_bool_t b_last_es = VLC_TRUE;
    filter_t **pp_vfilter, **pp_end;
    int i;

    if ( !p_sys->b_inited )
    {
        return VLC_SUCCESS;
    }

    if ( p_sys->p_decoder != NULL )
    {
        picture_t **pp_ring = p_sys->p_decoder->p_owner->pp_pics;

        if( p_sys->p_decoder->p_module )
            module_Unneed( p_sys->p_decoder, p_sys->p_decoder->p_module );
        vlc_object_detach( p_sys->p_decoder );
        vlc_object_destroy( p_sys->p_decoder );

        for( i = 0; i < PICTURE_RING_SIZE; i++ )
        {
            if ( pp_ring[i] != NULL )
            {
                if ( pp_ring[i]->p_data_orig != NULL )
                    free( pp_ring[i]->p_data_orig );
                free( pp_ring[i]->p_sys );
                free( pp_ring[i] );
            }
        }
    }

    /* Destroy user specified video filters */
    pp_vfilter = p_sys->pp_vfilters;
    pp_end = pp_vfilter + p_sys->i_vfilters;
    for( ; pp_vfilter < pp_end; pp_vfilter++ )
    {
        vlc_object_detach( *pp_vfilter );
        if( (*pp_vfilter)->p_module )
            module_Unneed( *pp_vfilter, (*pp_vfilter)->p_module );
        vlc_object_destroy( *pp_vfilter );
    }
    free( p_sys->pp_vfilters );

    vlc_mutex_lock( p_sys->p_lock );

    p_bridge = GetBridge( p_stream );
    p_es = p_sys->p_es;

    p_es->b_empty = VLC_TRUE;
    while ( p_es->p_picture )
    {
        picture_t *p_next = p_es->p_picture->p_next;
        p_es->p_picture->pf_release( p_es->p_picture );
        p_es->p_picture = p_next;
    }

    for ( i = 0; i < p_bridge->i_es_num; i++ )
    {
        if ( !p_bridge->pp_es[i]->b_empty )
        {
            b_last_es = VLC_FALSE;
            break;
        }
    }

    if ( b_last_es )
    {
        vlc_object_t *p_libvlc = VLC_OBJECT( p_stream->p_libvlc );
        for ( i = 0; i < p_bridge->i_es_num; i++ )
            free( p_bridge->pp_es[i] );
        free( p_bridge->pp_es );
        free( p_bridge );
        var_Destroy( p_libvlc, "mosaic-struct" );
    }

    vlc_mutex_unlock( p_sys->p_lock );

    if ( p_sys->p_image )
    {
        image_HandlerDelete( p_sys->p_image );
    }

    p_sys->b_inited = VLC_FALSE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * PushPicture : push a picture in the mosaic-struct structure
 *****************************************************************************/
static void PushPicture( sout_stream_t *p_stream, picture_t *p_picture )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    bridged_es_t *p_es = p_sys->p_es;

    vlc_mutex_lock( p_sys->p_lock );

    *p_es->pp_last = p_picture;
    p_picture->p_next = NULL;
    p_es->pp_last = &p_picture->p_next;

    vlc_mutex_unlock( p_sys->p_lock );
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    picture_t *p_pic;

    if ( (sout_stream_sys_t *)id != p_sys )
    {
        block_ChainRelease( p_buffer );
        return VLC_SUCCESS;
    }

    while ( (p_pic = p_sys->p_decoder->pf_decode_video( p_sys->p_decoder,
                                                        &p_buffer )) )
    {
        picture_t *p_new_pic;

        if( p_sys->i_height || p_sys->i_width )
        {
            video_format_t fmt_out, fmt_in;

            memset( &fmt_in, 0, sizeof(video_format_t) );
            memset( &fmt_out, 0, sizeof(video_format_t) );
            fmt_in = p_sys->p_decoder->fmt_out.video;


            if( p_sys->i_chroma )
                fmt_out.i_chroma = p_sys->i_chroma;
            else
                fmt_out.i_chroma = VLC_FOURCC('I','4','2','0');

            if ( !p_sys->i_height )
            {
                fmt_out.i_width = p_sys->i_width;
                fmt_out.i_height = (p_sys->i_width * VOUT_ASPECT_FACTOR
                    * p_sys->i_sar_num / p_sys->i_sar_den / fmt_in.i_aspect)
                      & ~0x1;
            }
            else if ( !p_sys->i_width )
            {
                fmt_out.i_height = p_sys->i_height;
                fmt_out.i_width = (p_sys->i_height * fmt_in.i_aspect
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
                                       p_pic, &fmt_in, &fmt_out );
            if ( p_new_pic == NULL )
            {
                msg_Err( p_stream, "image conversion failed" );
                continue;
            }
        }
        else
        {
            /* TODO: chroma conversion if needed */

            p_new_pic = (picture_t*)malloc( sizeof(picture_t) );
            if( p_new_pic == NULL )
            {
                msg_Err( p_stream, "image conversion failed" );
                continue;
            }

            if( vout_AllocatePicture(
                                  p_stream, p_new_pic, p_pic->format.i_chroma,
                                  p_pic->format.i_width, p_pic->format.i_height,
                                  p_sys->p_decoder->fmt_out.video.i_aspect )
                != VLC_SUCCESS )
            {
                free( p_new_pic );
                msg_Err( p_stream, "image allocation failed" );
                continue;
            }

            vout_CopyPicture( p_stream, p_new_pic, p_pic );
        }

        p_new_pic->i_refcount = 1;
        p_new_pic->i_status = DESTROYED_PICTURE;
        p_new_pic->i_type   = DIRECT_PICTURE;
        p_new_pic->p_sys = (picture_sys_t *)p_new_pic->pf_release;
        p_new_pic->pf_release = ReleasePicture;
        p_new_pic->date = p_pic->date;
        p_pic->pf_release( p_pic );

        if( p_sys->pp_vfilters )
        {
            /* Apply user specified video filters */
            filter_t **pp_vfilter = p_sys->pp_vfilters;
            filter_t **pp_end = pp_vfilter + p_sys->i_vfilters;
            for( ; pp_vfilter < pp_end; pp_vfilter++ )
            {
                (*pp_vfilter)->fmt_in.i_codec = p_new_pic->format.i_chroma;
                (*pp_vfilter)->fmt_out.i_codec = p_new_pic->format.i_chroma;
                (*pp_vfilter)->fmt_in.video = p_new_pic->format;
                (*pp_vfilter)->fmt_out.video = p_new_pic->format;
                p_new_pic = (*pp_vfilter)->pf_video_filter( *pp_vfilter,
                                                             p_new_pic );
                if( !p_new_pic )
                {
                    msg_Err( p_stream, "video filter failed" );
                    break;
                }
            }
            if( !p_new_pic ) continue;
        }

        PushPicture( p_stream, p_new_pic );
    }

    return VLC_SUCCESS;
}

struct picture_sys_t
{
    vlc_object_t *p_owner;
    vlc_bool_t b_dead;
};

static void video_release_buffer_decoder( picture_t *p_pic )
{
    if( p_pic && !p_pic->i_refcount && p_pic->pf_release && p_pic->p_sys )
    {
        video_del_buffer_decoder( (decoder_t *)p_pic->p_sys->p_owner, p_pic );
    }
    else if( p_pic && p_pic->i_refcount > 0 ) p_pic->i_refcount--;
}

static void video_release_buffer_filter( picture_t *p_pic )
{
    if( p_pic && !p_pic->i_refcount && p_pic->pf_release && p_pic->p_sys )
    {
        video_del_buffer_filter( (filter_t *)p_pic->p_sys->p_owner, p_pic );
    }
    else if( p_pic && p_pic->i_refcount > 0 ) p_pic->i_refcount--;
}

inline static picture_t *video_new_buffer_decoder( decoder_t *p_dec )
{
    return video_new_buffer( VLC_OBJECT( p_dec ),
                             (decoder_owner_sys_t *)p_dec->p_owner,
                             &p_dec->fmt_out,
                             video_release_buffer_decoder );
}

inline static picture_t *video_new_buffer_filter( filter_t *p_filter )
{
    return video_new_buffer( VLC_OBJECT( p_filter ),
                             (decoder_owner_sys_t *)p_filter->p_owner,
                             &p_filter->fmt_out,
                             video_release_buffer_filter );
}

static picture_t *video_new_buffer( vlc_object_t *p_this,
                                    decoder_owner_sys_t *p_sys,
                                    es_format_t *fmt_out,
                                    void ( *pf_release )( picture_t * ) )
{
    picture_t **pp_ring = p_sys->pp_pics;
    picture_t *p_pic;
    int i;

    if( fmt_out->video.i_width != p_sys->video.i_width ||
        fmt_out->video.i_height != p_sys->video.i_height ||
        fmt_out->video.i_chroma != p_sys->video.i_chroma ||
        fmt_out->video.i_aspect != p_sys->video.i_aspect )
    {
        if( !fmt_out->video.i_sar_num ||
            !fmt_out->video.i_sar_den )
        {
            fmt_out->video.i_sar_num =
                fmt_out->video.i_aspect * fmt_out->video.i_height;

            fmt_out->video.i_sar_den =
                VOUT_ASPECT_FACTOR * fmt_out->video.i_width;
        }

        vlc_ureduce( &fmt_out->video.i_sar_num,
                     &fmt_out->video.i_sar_den,
                     fmt_out->video.i_sar_num,
                     fmt_out->video.i_sar_den, 0 );

        if( !fmt_out->video.i_visible_width ||
            !fmt_out->video.i_visible_height )
        {
            fmt_out->video.i_visible_width = fmt_out->video.i_width;
            fmt_out->video.i_visible_height = fmt_out->video.i_height;
        }

        fmt_out->video.i_chroma = fmt_out->i_codec;
        p_sys->video = fmt_out->video;

        for( i = 0; i < PICTURE_RING_SIZE; i++ )
        {
            if ( pp_ring[i] != NULL )
            {
                if ( pp_ring[i]->i_status == DESTROYED_PICTURE )
                {
                    if ( pp_ring[i]->p_data_orig != NULL )
                        free( pp_ring[i]->p_data_orig );
                    free( pp_ring[i]->p_sys );
                    free( pp_ring[i] );
                }
                else
                {
                    pp_ring[i]->p_sys->b_dead = VLC_TRUE;
                }
                pp_ring[i] = NULL;
            }
        }
    }

    /* Find an empty space in the picture ring buffer */
    for( i = 0; i < PICTURE_RING_SIZE; i++ )
    {
        if( pp_ring[i] != NULL && pp_ring[i]->i_status == DESTROYED_PICTURE )
        {
            pp_ring[i]->i_status = RESERVED_PICTURE;
            return pp_ring[i];
        }
    }
    for( i = 0; i < PICTURE_RING_SIZE; i++ )
    {
        if( pp_ring[i] == NULL ) break;
    }

    if( i == PICTURE_RING_SIZE )
    {
        msg_Err( p_this, "decoder/filter is leaking pictures, "
                 "resetting its ring buffer" );

        for( i = 0; i < PICTURE_RING_SIZE; i++ )
        {
            pp_ring[i]->pf_release( pp_ring[i] );
        }

        i = 0;
    }

    p_pic = malloc( sizeof(picture_t) );
    fmt_out->video.i_chroma = fmt_out->i_codec;
    vout_AllocatePicture( p_this, p_pic,
                          fmt_out->video.i_chroma,
                          fmt_out->video.i_width,
                          fmt_out->video.i_height,
                          fmt_out->video.i_aspect );

    if( !p_pic->i_planes )
    {
        free( p_pic );
        return NULL;
    }

    p_pic->pf_release = pf_release;
    p_pic->p_sys = malloc( sizeof(picture_sys_t) );
    p_pic->p_sys->p_owner = p_this;
    p_pic->p_sys->b_dead = VLC_FALSE;
    p_pic->i_status = RESERVED_PICTURE;

    pp_ring[i] = p_pic;

    return p_pic;
}

inline static void video_del_buffer_decoder( decoder_t *p_this,
                                             picture_t *p_pic )
{
    video_del_buffer( VLC_OBJECT( p_this ), p_pic );
}

inline static void video_del_buffer_filter( filter_t *p_this,
                                            picture_t *p_pic )
{
    video_del_buffer( VLC_OBJECT( p_this ), p_pic );
}

static void video_del_buffer( vlc_object_t *p_this, picture_t *p_pic )
{
    p_pic->i_refcount = 0;
    p_pic->i_status = DESTROYED_PICTURE;
    if ( p_pic->p_sys->b_dead )
    {
        if ( p_pic->p_data_orig != NULL )
            free( p_pic->p_data_orig );
        free( p_pic->p_sys );
        free( p_pic );
    }
}

static void video_link_picture_decoder( decoder_t *p_dec, picture_t *p_pic )
{
    p_pic->i_refcount++;
}

static void video_unlink_picture_decoder( decoder_t *p_dec, picture_t *p_pic )
{
    video_release_buffer_decoder( p_pic );
}


/**********************************************************************
 * Callback to update (some) params on the fly
 **********************************************************************/
static int MosaicBridgeCallback( vlc_object_t *p_this, char const *psz_var,
                                 vlc_value_t oldval, vlc_value_t newval,
                                 void *p_data )
{
    sout_stream_t *p_stream = (sout_stream_t *)p_data;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i_ret = VLC_SUCCESS;

#define VAR_IS( a ) !strcmp( psz_var, CFG_PREFIX a )
    if( VAR_IS( "height" ) )
    {
        /* We create the handler before updating the value in p_sys
         * so we don't have to worry about locking */
        if( !p_sys->p_image && newval.i_int )
            p_sys->p_image = image_HandlerCreate( p_stream );
        p_sys->i_height = newval.i_int;
    }
    else if( VAR_IS( "width" ) )
    {
        /* We create the handler before updating the value in p_sys
         * so we don't have to worry about locking */
        if( !p_sys->p_image && newval.i_int )
            p_sys->p_image = image_HandlerCreate( p_stream );
        p_sys->i_width = newval.i_int;
    }
#undef VAR_IS

    return i_ret;
}
