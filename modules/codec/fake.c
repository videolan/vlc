/*****************************************************************************
 * fake.c: decoder reading from a fake stream, outputting a fixed image
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/decoder.h>

#include "vlc_image.h"
#include "vlc_filter.h"
#include "charset.h"

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
    "Path of the image file for fake input." )
#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_( \
    "Output video width." )
#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_( \
    "Output video height." )
#define KEEP_AR_TEXT N_("Keep aspect ratio")
#define KEEP_AR_LONGTEXT N_( \
    "Consider width and height as maximum values." )
#define ASPECT_RATIO_TEXT N_("Background aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_( \
    "Aspect ratio of the image file (4:3, 16:9). Default is square pixels." )
#define DEINTERLACE_TEXT N_("Deinterlace video")
#define DEINTERLACE_LONGTEXT N_( \
    "Deinterlace the image after loading it." )
#define DEINTERLACE_MODULE_TEXT N_("Deinterlace module")
#define DEINTERLACE_MODULE_LONGTEXT N_( \
    "Deinterlace module to use." )

static char *ppsz_deinterlace_type[] =
{
    "deinterlace", "ffmpeg-deinterlace"
};

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
    add_integer( "fake-width", 0, NULL, WIDTH_TEXT,
                 WIDTH_LONGTEXT, VLC_TRUE );
    add_integer( "fake-height", 0, NULL, HEIGHT_TEXT,
                 HEIGHT_LONGTEXT, VLC_TRUE );
    add_bool( "fake-keep-ar", 0, NULL, KEEP_AR_TEXT, KEEP_AR_LONGTEXT,
              VLC_TRUE );
    add_string( "fake-aspect-ratio", "", NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_LONGTEXT, VLC_TRUE );
    add_bool( "fake-deinterlace", 0, NULL, DEINTERLACE_TEXT,
              DEINTERLACE_LONGTEXT, VLC_FALSE );
    add_string( "fake-deinterlace-module", "deinterlace", NULL,
                DEINTERLACE_MODULE_TEXT, DEINTERLACE_MODULE_LONGTEXT,
                VLC_FALSE );
        change_string_list( ppsz_deinterlace_type, 0, 0 );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    vlc_value_t val;
    image_handler_t *p_handler;
    video_format_t fmt_in, fmt_out;
    picture_t *p_image;
    char *psz_file, *psz_local;
    vlc_bool_t b_keep_ar;
    int i_aspect = 0;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('f','a','k','e') )
    {
        return VLC_EGENERIC;
    }

    var_Create( p_dec, "fake-file", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "fake-file", &val );
    if( val.psz_string == NULL || !*val.psz_string )
    {
        if( val.psz_string ) free( val.psz_string );
        msg_Err( p_dec, "specify a file with --fake-file=..." );
        return VLC_EGENERIC;
    }
    psz_file = val.psz_string;

    memset( &fmt_in, 0, sizeof(fmt_in) );
    memset( &fmt_out, 0, sizeof(fmt_out) );
    fmt_out.i_chroma = VLC_FOURCC('I','4','2','0');

    var_Create( p_dec, "fake-keep-ar", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "fake-keep-ar", &val );
    b_keep_ar = val.b_bool;

    var_Create( p_dec, "fake-width", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_dec, "fake-height", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_dec, "fake-aspect-ratio",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    var_Get( p_dec, "fake-aspect-ratio", &val );
    if ( val.psz_string )
    {
        char *psz_parser = strchr( val.psz_string, ':' );

        if( psz_parser )
        {
            *psz_parser++ = '\0';
            i_aspect = atoi( val.psz_string )
                                   * VOUT_ASPECT_FACTOR / atoi( psz_parser );
        }
        free( val.psz_string );
    }

    if ( !b_keep_ar )
    {
        var_Get( p_dec, "fake-width", &val );
        fmt_out.i_width = val.i_int;
        var_Get( p_dec, "fake-height", &val );
        fmt_out.i_height = val.i_int;
    }

    p_handler = image_HandlerCreate( p_dec );
    psz_local = ToLocale( psz_file );
    p_image = image_ReadUrl( p_handler, psz_local, &fmt_in, &fmt_out );
    LocaleFree( psz_local );
    image_HandlerDelete( p_handler );

    if ( p_image == NULL )
    {
        msg_Err( p_dec, "unable to read image file %s", psz_file );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_dec, "file %s loaded successfully", psz_file );

    if ( psz_file ) free( psz_file );

    if ( b_keep_ar )
    {
        picture_t *p_old = p_image;
        int i_width, i_height;

        var_Get( p_dec, "fake-width", &val );
        i_width = val.i_int;
        var_Get( p_dec, "fake-height", &val );
        i_height = val.i_int;

        if ( i_width && i_height )
        {
            int i_image_ar = fmt_out.i_width * VOUT_ASPECT_FACTOR
                              / fmt_out.i_height;
            int i_region_ar = i_width * VOUT_ASPECT_FACTOR / i_height;
            fmt_in = fmt_out;

            if ( i_aspect == i_image_ar )
            {
                fmt_out.i_width = i_width;
                fmt_out.i_height = i_height;
            }
            else if ( i_image_ar > i_region_ar )
            {
                fmt_out.i_width = i_width;
                fmt_out.i_height = i_width * VOUT_ASPECT_FACTOR
                                    / i_image_ar;
                i_aspect = i_image_ar;
            }
            else
            {
                fmt_out.i_height = i_height;
                fmt_out.i_width = i_height * i_image_ar
                                    / VOUT_ASPECT_FACTOR;
                i_aspect = i_image_ar;
            }

            p_handler = image_HandlerCreate( p_dec );
            p_image = image_Convert( p_handler, p_old, &fmt_in, &fmt_out );
            image_HandlerDelete( p_handler );

            if ( p_image == NULL )
            {
                msg_Warn( p_dec, "couldn't load resizing module" );
                p_image = p_old;
                fmt_out = fmt_in;
            }
            else
            {
                p_old->pf_release( p_old );
            }
        }
    }

    if ( i_aspect )
    {
        fmt_out.i_aspect = i_aspect;
    }
    else
    {
        fmt_out.i_aspect = fmt_out.i_width
                            * VOUT_ASPECT_FACTOR / fmt_out.i_height;
    }

    var_Create( p_dec, "fake-deinterlace", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "fake-deinterlace", &val );
    if ( val.b_bool )
    {
        picture_t *p_old = p_image;

        var_Create( p_dec, "fake-deinterlace-module",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_dec, "fake-deinterlace-module", &val );

        p_handler = image_HandlerCreate( p_dec );
        p_image = image_Filter( p_handler, p_old, &fmt_out, val.psz_string );
        image_HandlerDelete( p_handler );
        if ( val.psz_string != NULL ) free( val.psz_string );

        if ( p_image == NULL )
        {
            msg_Warn( p_dec, "couldn't load deinterlace module" );
            p_image = p_old;
        }
        else
        {
            p_old->pf_release( p_old );
        }
    }

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','2','0');
    p_dec->fmt_out.video = fmt_out;

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;
    p_dec->p_sys = (decoder_sys_t *)p_image;

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    picture_t *p_image = (picture_t *)p_dec->p_sys;
    picture_t *p_pic;

    if( pp_block == NULL || !*pp_block ) return NULL;
    p_pic = p_dec->pf_vout_buffer_new( p_dec );
    if( p_pic == NULL )
    {
        msg_Err( p_dec, "cannot get picture" );
        goto error;
    }

    vout_CopyPicture( p_dec, p_pic, p_image );
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
    picture_t *p_image = (picture_t *)p_dec->p_sys;

    if( p_image != NULL )
        p_image->pf_release( p_image );
}
