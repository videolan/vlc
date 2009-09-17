/*****************************************************************************
 * invmem_decoder.c: memory video driver for vlc
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Robert Paciorek <robert@opcode.eu.org> <http://opcode.eu.org/bercik>
 * based on:
 *  - vmem video output module (Gildas Bazin <gbazin@videolan.org>)
 *  - png video decodec module (Sam Hocevar <sam@zoy.org>)
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include <vlc_image.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static picture_t *DecodeBlock  ( decoder_t *, block_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define T_WIDTH N_( "Width" )
#define LT_WIDTH N_( "Video memory buffer width." )

#define T_HEIGHT N_( "Height" )
#define LT_HEIGHT N_( "Video memory buffer height." )

#define T_LOCK N_( "Lock function" )
#define LT_LOCK N_( "Address of the locking callback function. This " \
                    "function must return a valid memory address for use " \
                    "by the video renderer." )

#define T_UNLOCK N_( "Unlock function" )
#define LT_UNLOCK N_( "Address of the unlocking callback function" )

#define T_DATA N_( "Callback data" )
#define LT_DATA N_( "Data for the locking and unlocking functions" )

#define T_CHROMA N_("Chroma")
#define LT_CHROMA N_("Output chroma for the memory image as a 4-character " \
                      "string, eg. \"RV32\".")

#define INVMEM_HELP N_( "This module make possible making video stream from raw-image " \
                        "generating (to memory) from rendering program uses libvlc. " \
                        "To use this module from libvlc set --codec to invmem, "\
                        "set all --invmem-* options in vlc_argv an use " \
                        "libvlc_media_new(libvlc, \"fake://\", &ex);. " \
                        "Besides is simillar to vmem video output module." )

vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_shortname( N_("Memory video decoder") )
    set_description( N_("Memory video decoder") )
    set_help( INVMEM_HELP )
    set_capability( "decoder", 50 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "invmem" )

    add_integer( "invmem-width", 0, NULL, T_WIDTH, LT_WIDTH, false )
    add_integer( "invmem-height", 0, NULL, T_HEIGHT, LT_HEIGHT, false )
    add_string( "invmem-lock", "0", NULL, T_LOCK, LT_LOCK, true )
    add_string( "invmem-unlock", "0", NULL, T_UNLOCK, LT_UNLOCK, true )
    add_string( "invmem-data", "0", NULL, T_DATA, LT_DATA, true )
    add_string( "invmem-chroma", "RV24", NULL, T_CHROMA, LT_CHROMA, true)
vlc_module_end()


struct decoder_sys_t
{
    void * (*pf_lock) (void *);
    void (*pf_unlock) (void *);
    void *p_data;

    int i_width;
    int i_height;
    int i_pitch;

    vlc_fourcc_t i_chroma;
};


/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    char *psz_tmp;
    int pitch;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('f','a','k','e'))
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    // get parameters
    char* chromaStr = var_CreateGetString( p_dec, "invmem-chroma" );
    p_sys->i_width = var_CreateGetInteger( p_this, "invmem-width" );
    p_sys->i_height = var_CreateGetInteger( p_this, "invmem-height" );
    if( p_sys->i_width == 0 || p_sys->i_height == 0 )
    {
        msg_Err( p_dec, "--invmem-width and --invmem-height must be > 0" );
        goto error;
    }

    psz_tmp = var_CreateGetString( p_dec, "invmem-lock" );
    p_sys->pf_lock = (void * (*) (void *))(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    psz_tmp = var_CreateGetString( p_dec, "invmem-unlock" );
    p_sys->pf_unlock = (void (*) (void *))(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    psz_tmp = var_CreateGetString( p_dec, "invmem-data" );
    p_sys->p_data = (void *)(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    if( !p_sys->pf_lock || !p_sys->pf_unlock )
    {
        msg_Err( p_dec, "Invalid lock or unlock callbacks" );
        goto error;
    }

    if ( chromaStr == NULL )
    {
        msg_Err( p_dec, "Invalid invmem-chroma string." );
        goto error;
    }
    const vlc_fourcc_t chroma = vlc_fourcc_GetCodecFromString( VIDEO_ES, chromaStr );

    if ( !chroma )
    {
        msg_Err( p_dec, "invmem-chroma should be 4 characters long." );
        goto error;
    }

    /* Set output properties */
    switch (chroma)
    {
    case VLC_CODEC_RGB15:
        p_dec->fmt_out.video.i_rmask = 0x001f;
        p_dec->fmt_out.video.i_gmask = 0x03e0;
        p_dec->fmt_out.video.i_bmask = 0x7c00;
        pitch = p_sys->i_width * 2;
        break;
    case VLC_CODEC_RGB16:
        p_dec->fmt_out.video.i_rmask = 0x001f;
        p_dec->fmt_out.video.i_gmask = 0x07e0;
        p_dec->fmt_out.video.i_bmask = 0xf800;
        pitch = p_sys->i_width * 2;
        break;
    case VLC_CODEC_RGB24:
        p_dec->fmt_out.video.i_rmask = 0xff0000;
        p_dec->fmt_out.video.i_gmask = 0x00ff00;
        p_dec->fmt_out.video.i_bmask = 0x0000ff;
        pitch = p_sys->i_width * 3;
        break;
    case VLC_CODEC_RGB32:
        p_dec->fmt_out.video.i_rmask = 0xff0000;
        p_dec->fmt_out.video.i_gmask = 0x00ff00;
        p_dec->fmt_out.video.i_bmask = 0x0000ff;
        pitch = p_sys->i_width * 4;
        break;
    default:
        p_dec->fmt_out.video.i_rmask = 0;
        p_dec->fmt_out.video.i_gmask = 0;
        p_dec->fmt_out.video.i_bmask = 0;
        pitch = 0;
        msg_Warn( p_dec, "Unknown chroma %s", chromaStr );
        goto error;
    }

    free( chromaStr );

    p_dec->fmt_out.i_codec = chroma;
    p_dec->fmt_out.video.i_width = p_dec->p_sys->i_width;
    p_dec->fmt_out.video.i_height = p_dec->p_sys->i_height;
    p_dec->fmt_out.video.i_aspect = VOUT_ASPECT_FACTOR * p_dec->p_sys->i_width / p_dec->p_sys->i_height;
    p_dec->fmt_out.i_cat = VIDEO_ES;

    p_sys->i_pitch = pitch;

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;

    return VLC_SUCCESS;
error:
    free( p_sys );
    free( chromaStr );
    return VLC_EGENERIC;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with a complete compressed frame.
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    picture_t*  p_pic;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    // create new picture
    p_pic = decoder_NewPicture( p_dec );
    if ( !p_pic ) return NULL;
    p_pic->b_force = true;
    p_pic->p->i_pitch = p_dec->p_sys->i_pitch;
    p_pic->date = p_block->i_pts > 0 ? p_block->i_pts : p_block->i_dts;

    // lock input and copy to picture
    p_pic->p->p_pixels = p_sys->pf_lock( p_dec->p_sys->p_data );

    // unlock input
    p_sys->pf_unlock( p_dec->p_sys->p_data );

    block_Release( *pp_block ); *pp_block = NULL;
    return p_pic;
}

/*****************************************************************************
 * CloseDecoder: invmem decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    free( p_sys );
}
