/*****************************************************************************
 * fake.c: decoder reading from a fake stream, outputting a fixed image
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <vlc/vlc.h>
#include <vlc/decoder.h>

#include "vlc_image.h"

/*****************************************************************************
 * decoder_sys_t : fake decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    picture_t *p_image;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static picture_t *DecodeBlock  ( decoder_t *, block_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FILE_TEXT N_("Image file")
#define FILE_LONGTEXT N_( \
    "Path of the image file when using the fake input." )
#define ASPECT_RATIO_TEXT N_("Background aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_( \
    "Aspect ratio of the image file (4:3, 16:9)." )

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_VCODEC );
    set_shortname( _("Fake") );
    set_description( _("Fake video decoder") );
    set_capability( "decoder", 1000 );
    set_callbacks( OpenDecoder, CloseDecoder );
    add_shortcut( "fake" );

    add_file( "fake-file", "", NULL, FILE_TEXT,
                FILE_LONGTEXT, VLC_FALSE );
    add_string( "fake-aspect-ratio", "4:3", NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_LONGTEXT, VLC_TRUE );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    vlc_value_t val;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('f','a','k','e') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }

    var_Create( p_dec, "fake-file", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "fake-file", &val );
    if( val.psz_string != NULL && *val.psz_string )
    {
        image_handler_t *p_handler = image_HandlerCreate( p_dec );
        video_format_t fmt_in, fmt_out;

        memset( &fmt_in, 0, sizeof(fmt_in) );
        memset( &fmt_out, 0, sizeof(fmt_out) );
        fmt_out.i_chroma = VLC_FOURCC('Y', 'V', '1', '2');
        p_sys->p_image = image_ReadUrl( p_handler, val.psz_string,
                                        &fmt_in, &fmt_out );
        image_HandlerDelete( p_handler );

        if( p_sys->p_image == NULL )
        {
            msg_Err( p_dec, "unable to read image file %s", val.psz_string );
            free( p_dec->p_sys );
            return VLC_EGENERIC;
        }
        msg_Dbg( p_dec, "file %s loaded successfully", val.psz_string );
        p_dec->fmt_out.video = fmt_out;
    }
    if( val.psz_string ) free( val.psz_string );

    var_Create( p_dec, "fake-aspect-ratio",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "fake-aspect-ratio", &val );
    if ( val.psz_string )
    {
        char *psz_parser = strchr( val.psz_string, ':' );

        if( psz_parser )
        {
            *psz_parser++ = '\0';
            p_dec->fmt_out.video.i_aspect = atoi( val.psz_string )
                                   * VOUT_ASPECT_FACTOR / atoi( psz_parser );
        }
        else
        {
            msg_Warn( p_dec, "bad aspect ratio %s", val.psz_string );
            p_dec->fmt_out.video.i_aspect = 4 * VOUT_ASPECT_FACTOR / 3;
        }

        free( val.psz_string );
    }
    else
    {
        p_dec->fmt_out.video.i_aspect = 4 * VOUT_ASPECT_FACTOR / 3;
    }

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->fmt_out.i_codec = VLC_FOURCC('Y','V','1','2');

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;

    if( pp_block == NULL || !*pp_block ) return NULL;
    p_pic = p_dec->pf_vout_buffer_new( p_dec );
    if( p_pic == NULL )
    {
        msg_Err( p_dec, "cannot get picture" );
        goto error;
    }

    vout_CopyPicture( p_dec, p_pic, p_sys->p_image );
    p_pic->date = (*pp_block)->i_pts;

error:
    block_Release( *pp_block );
    *pp_block = NULL;

    return p_pic;
}

/*****************************************************************************
 * CloseDecoder: fake decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_image != NULL )
        p_sys->p_image->pf_release( p_sys->p_image );
    free( p_sys );
}
