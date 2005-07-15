/*****************************************************************************
 * mosaic_bridge.c:
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/sout.h>
#include <vlc/decoder.h>

#include "vlc_image.h"

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
    int i_sar_num, i_sar_den;
    char *psz_id;
    vlc_bool_t b_inited;
};

#define PICTURE_RING_SIZE 4
struct decoder_owner_sys_t
{
    picture_t *pp_pics[PICTURE_RING_SIZE];
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

static void video_del_buffer( decoder_t *, picture_t * );
static picture_t *video_new_buffer( decoder_t * );
static void video_link_picture_decoder( decoder_t *, picture_t * );
static void video_unlink_picture_decoder( decoder_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ID_TEXT N_("ID")
#define ID_LONGTEXT N_( \
    "Specify an identifier string for this subpicture" )

#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_( \
    "Allows you to specify the output video width." )
#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_( \
    "Allows you to specify the output video height." )
#define RATIO_TEXT N_("Sample aspect ratio")
#define RATIO_LONGTEXT N_( \
    "Sample aspect ratio of the destination (1:1, 3:4, 2:3)." )

#define SOUT_CFG_PREFIX "sout-mosaic-bridge-"

vlc_module_begin();
    set_shortname( _( "Mosaic bridge" ) );
    set_description(_("Mosaic bridge stream output") );
    set_capability( "sout stream", 0 );
    add_shortcut( "mosaic-bridge" );

    add_string( SOUT_CFG_PREFIX "id", "Id", NULL, ID_TEXT, ID_LONGTEXT,
                VLC_FALSE );
    add_integer( SOUT_CFG_PREFIX "width", 0, NULL, WIDTH_TEXT,
                 WIDTH_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "height", 0, NULL, HEIGHT_TEXT,
                 HEIGHT_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "sar", "1:1", NULL, RATIO_TEXT,
                RATIO_LONGTEXT, VLC_FALSE );

    set_callbacks( Open, Close );

    var_Create( p_module->p_libvlc, "mosaic-lock", VLC_VAR_MUTEX );
vlc_module_end();

static const char *ppsz_sout_options[] = {
    "id", "width", "height", "sar", NULL
};

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t *p_sys;
    libvlc_t *p_libvlc = p_this->p_libvlc;
    vlc_value_t val;

    sout_CfgParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                   p_stream->p_cfg );

    p_sys          = malloc( sizeof( sout_stream_sys_t ) );
    p_stream->p_sys = p_sys;
    p_sys->b_inited = VLC_FALSE;

    var_Get( p_libvlc, "mosaic-lock", &val );
    p_sys->p_lock = val.p_address;

    var_Get( p_stream, SOUT_CFG_PREFIX "id", &val );
    p_sys->psz_id = val.psz_string;

    var_Get( p_stream, SOUT_CFG_PREFIX "height", &val );
    p_sys->i_height = val.i_int; 

    var_Get( p_stream, SOUT_CFG_PREFIX "width", &val );
    p_sys->i_width = val.i_int; 

    var_Get( p_stream, SOUT_CFG_PREFIX "sar", &val );
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
    p_sys->p_decoder->pf_vout_buffer_new = video_new_buffer;
    p_sys->p_decoder->pf_vout_buffer_del = video_del_buffer;
    p_sys->p_decoder->pf_picture_link    = video_link_picture_decoder;
    p_sys->p_decoder->pf_picture_unlink  = video_unlink_picture_decoder;
    p_sys->p_decoder->p_owner = malloc( sizeof(decoder_owner_sys_t) );
    for( i = 0; i < PICTURE_RING_SIZE; i++ )
        p_sys->p_decoder->p_owner->pp_pics[i] = 0;
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
        libvlc_t *p_libvlc = p_stream->p_libvlc;
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

    msg_Dbg( p_stream, "mosaic bridge id=%s pos=%d", p_es->psz_id, i );

    return (sout_stream_id_t *)p_sys;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    bridge_t *p_bridge;
    bridged_es_t *p_es;
    vlc_bool_t b_last_es = VLC_TRUE;
    int i;

    if ( !p_sys->b_inited )
    {
        return VLC_SUCCESS;
    }

    if ( p_sys->p_decoder )
    {
        if( p_sys->p_decoder->p_module )
            module_Unneed( p_sys->p_decoder, p_sys->p_decoder->p_module );
        vlc_object_detach( p_sys->p_decoder );
        vlc_object_destroy( p_sys->p_decoder );
    }

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
        libvlc_t *p_libvlc = p_stream->p_libvlc;
        for ( i = 0; i < p_bridge->i_es_num; i++ )
            free( p_bridge->pp_es[i] );
        free( p_bridge->pp_es );
        free( p_bridge );
        var_Destroy( p_libvlc, "mosaic-struct" );
    }

    vlc_mutex_unlock( p_sys->p_lock );

    if ( p_sys->i_height || p_sys->i_width )
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

        if ( p_sys->i_height || p_sys->i_width )
        {
            video_format_t fmt_out = {0}, fmt_in = {0};
            fmt_in = p_sys->p_decoder->fmt_out.video;

            fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');

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
            p_new_pic = (picture_t*)malloc( sizeof(picture_t) );
            vout_AllocatePicture( p_stream, p_new_pic, p_pic->format.i_chroma,
                                  p_pic->format.i_width, p_pic->format.i_height,
                                  p_sys->p_decoder->fmt_out.video.i_aspect );

            vout_CopyPicture( p_stream, p_new_pic, p_pic );
        }

        p_new_pic->i_refcount = 1;
        p_new_pic->i_status = DESTROYED_PICTURE;
        p_new_pic->i_type   = DIRECT_PICTURE;
        p_new_pic->p_sys = (picture_sys_t *)p_new_pic->pf_release;
        p_new_pic->pf_release = ReleasePicture;
        p_new_pic->date = p_pic->date;

        p_pic->pf_release( p_pic );
        PushPicture( p_stream, p_new_pic );
    }

    return VLC_SUCCESS;
}

struct picture_sys_t
{
    vlc_object_t *p_owner;
};

static void video_release_buffer( picture_t *p_pic )
{
    if( p_pic && !p_pic->i_refcount && p_pic->pf_release && p_pic->p_sys )
    {
        video_del_buffer( (decoder_t *)p_pic->p_sys->p_owner, p_pic );
    }
    else if( p_pic && p_pic->i_refcount > 0 ) p_pic->i_refcount--;
}

static picture_t *video_new_buffer( decoder_t *p_dec )
{
    picture_t **pp_ring = p_dec->p_owner->pp_pics;
    picture_t *p_pic;
    int i;

    /* Find an empty space in the picture ring buffer */
    for( i = 0; i < PICTURE_RING_SIZE; i++ )
    {
        if( pp_ring[i] != 0 && pp_ring[i]->i_status == DESTROYED_PICTURE )
        {
            pp_ring[i]->i_status = RESERVED_PICTURE;
            return pp_ring[i];
        }
    }
    for( i = 0; i < PICTURE_RING_SIZE; i++ )
    {
        if( pp_ring[i] == 0 ) break;
    }

    if( i == PICTURE_RING_SIZE )
    {
        msg_Err( p_dec, "decoder/filter is leaking pictures, "
                 "resetting its ring buffer" );

        for( i = 0; i < PICTURE_RING_SIZE; i++ )
        {
            pp_ring[i]->pf_release( pp_ring[i] );
        }

        i = 0;
    }

    p_pic = malloc( sizeof(picture_t) );
    p_dec->fmt_out.video.i_chroma = p_dec->fmt_out.i_codec;
    vout_AllocatePicture( VLC_OBJECT(p_dec), p_pic,
                          p_dec->fmt_out.video.i_chroma,
                          p_dec->fmt_out.video.i_width,
                          p_dec->fmt_out.video.i_height,
                          p_dec->fmt_out.video.i_aspect );

    if( !p_pic->i_planes )
    {
        free( p_pic );
        return 0;
    }

    p_pic->pf_release = video_release_buffer;
    p_pic->p_sys = malloc( sizeof(picture_sys_t) );
    p_pic->p_sys->p_owner = VLC_OBJECT(p_dec);
    p_pic->i_status = RESERVED_PICTURE;

    pp_ring[i] = p_pic;

    return p_pic;
}

static void video_del_buffer( decoder_t *p_this, picture_t *p_pic )
{
    p_pic->i_refcount = 0;
    p_pic->i_status = DESTROYED_PICTURE;
}

static void video_link_picture_decoder( decoder_t *p_dec, picture_t *p_pic )
{
    p_pic->i_refcount++;
}

static void video_unlink_picture_decoder( decoder_t *p_dec, picture_t *p_pic )
{
    video_release_buffer( p_pic );
}

