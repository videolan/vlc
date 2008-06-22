/*****************************************************************************
 * cmml.c : CMML annotations/metadata decoder
 *****************************************************************************
 * Copyright (C) 2003-2004 Commonwealth Scientific and Industrial Research
 *                         Organisation (CSIRO) Australia
 * Copyright (C) 2004 the VideoLAN team
 *
 * $Id$
 *
 * Author: Andre Pang <Andre.Pang@csiro.au>
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
#include <vlc_input.h>
#include <vlc_codec.h>
#include <vlc_osd.h>
#include <vlc_charset.h>
#include <vlc_interface.h>
#include "xtag.h"

#undef  CMML_DEBUG

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    intf_thread_t *     p_intf;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int           OpenDecoder   ( vlc_object_t * );
static void          CloseDecoder  ( vlc_object_t * );

static subpicture_t *DecodeBlock   ( decoder_t *, block_t ** );

static void          ParseText     ( decoder_t *, block_t * );

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
int  OpenIntf  ( vlc_object_t * );
void CloseIntf ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("CMML annotations decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, CloseDecoder );
    add_shortcut( "cmml" );

    add_submodule();
        set_capability( "interface", 0 );
        set_callbacks( OpenIntf, CloseIntf );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    input_thread_t * p_input;
    decoder_sys_t *p_sys;
    vlc_value_t val;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('c','m','m','l') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_decode_sub = DecodeBlock;

#ifdef CMML_DEBUG
    msg_Dbg( p_dec, "i am at %p", p_dec );
#endif

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        return VLC_EGENERIC;
    }

    /* Let other interested modules know that we're a CMML decoder
     * We have to set this variable on the input thread, because there's
     * typically more than one decoder running so we can't find the CMML
     * decoder succesfully with vlc_object_find.  (Any hints on how to achieve
     * this would be rather appreciated ;) */
    p_input = vlc_object_find( p_dec, VLC_OBJECT_INPUT, FIND_ANYWHERE );
#ifdef CMML_DEBUG
    msg_Dbg( p_dec, "p_input is at %p", p_input );
#endif
    val.p_address = p_dec;
    var_Create( p_input, "has-cmml-decoder",
                VLC_VAR_ADDRESS|VLC_VAR_DOINHERIT );
    if( var_Set( p_input, "has-cmml-decoder", val ) != VLC_SUCCESS )
    {
        msg_Dbg( p_dec, "var_Set of has-cmml-decoder failed" );
    }
    vlc_object_release( p_input );

    /* initialise the CMML responder interface */
    p_sys->p_intf = intf_Create( p_dec, "cmml" );
    intf_RunThread( p_sys->p_intf );

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete subtitles units.
 ****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    subpicture_t *p_spu;

    if( !pp_block || *pp_block == NULL )
    {
        return NULL;
    }

    ParseText( p_dec, *pp_block );

    block_Release( *pp_block );
    *pp_block = NULL;

    /* allocate an empty subpicture to return.  the actual subpicture
     * displaying is done in the DisplayAnchor function in intf.c (called from
     * DisplayPendingAnchor, which in turn is called from the main RunIntf
     * loop). */
    p_spu = p_dec->pf_spu_buffer_new( p_dec );
    if( !p_spu )
    {
        msg_Dbg( p_dec, "couldn't allocate new subpicture" );
        return NULL;
    }

    return p_spu;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    intf_thread_t *p_intf;

    /* Destroy the interface object/thread */
    p_intf = vlc_object_find( p_dec, VLC_OBJECT_INTF, FIND_CHILD );
    if( p_intf != NULL )
    {
#ifdef CMML_DEBUG
        msg_Dbg( p_dec, "CMML decoder is freeing interface thread" );
#endif
        intf_StopThread( p_intf );
        vlc_object_detach( p_intf );
        vlc_object_release( p_intf );
        vlc_object_release( p_intf );
    }

    p_sys->p_intf = NULL;

    free( p_sys );
}

/*****************************************************************************
 * ParseText: parse an text subtitle packet and send it to the video output
 *****************************************************************************/
static void ParseText( decoder_t *p_dec, block_t *p_block )
{
    char *psz_subtitle, *psz_cmml, *psz_url;
    XTag *p_clip_parser, *p_anchor;
    vlc_value_t val;

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts == 0 )
    {
        msg_Warn( p_dec, "subtitle without a date" );
        return;
    }

    /* Check validity of packet data */
    if( p_block->i_buffer <= 1 ||  p_block->p_buffer[0] == '\0' )
    {
        msg_Warn( p_dec, "empty subtitle" );
        return;
    }

    /* get anchor text from CMML */

    /* Copy the whole CMML tag into our own buffer:
       allocate i_buffer bytes + 1 for the terminating \0 */
    if ( (psz_cmml = malloc( p_block->i_buffer + 1 )) == NULL )
        return;
    psz_cmml = memcpy( psz_cmml, p_block->p_buffer, p_block->i_buffer );
    psz_cmml[p_block->i_buffer] = '\0'; /* terminate the string */
#ifdef CMML_DEBUG
    msg_Dbg( p_dec, "psz_cmml is \"%s\"", psz_cmml );
#endif
 
    /* Parse the <clip> part of the CMML */
    p_clip_parser = xtag_new_parse( psz_cmml, p_block->i_buffer );
    if( !p_clip_parser )
    {
        msg_Warn( p_dec, "couldn't initialise <clip> parser" );
        free( psz_cmml );
        return;
    }

    /* Parse the anchor tag and get its contents */
    p_anchor = xtag_first_child( p_clip_parser, "a" );
    if( p_anchor != NULL )
    {
        psz_subtitle = xtag_get_pcdata( p_anchor );
    }
    else
    {
        psz_subtitle = strdup( " " );
    }

#ifdef CMML_DEBUG
    msg_Dbg( p_dec, "psz_subtitle is \"%s\"", psz_subtitle );
#endif

    /* get URL from the current clip, if one exists */
    psz_url = xtag_get_attribute( p_anchor, "href" );
#ifdef CMML_DEBUG
    msg_Dbg( p_dec, "psz_url is \"%s\"", psz_url );
#endif
    if( psz_url )
    {
        char *psz_tmp = strdup( psz_url );
 
        val.p_address = psz_tmp;
        if( var_Set( p_dec, "psz-current-anchor-url", val ) != VLC_SUCCESS )
        {
            (void) var_Create( p_dec, "psz-current-anchor-url",
                               VLC_VAR_ADDRESS|VLC_VAR_DOINHERIT );
            msg_Dbg( p_dec, "creating psz-current-anchor-url" );
            if( var_Set( p_dec, "psz-current-anchor-url", val ) != VLC_SUCCESS )
                msg_Dbg( p_dec, "var_Set of psz-current-anchor-url failed" );
        }
    }

    if( psz_subtitle )
    {
        char *psz_tmp = strdup( psz_subtitle );

        val.p_address = psz_tmp;
        if( var_Set( p_dec, "psz-current-anchor-description", val ) != VLC_SUCCESS )
        {
            (void) var_Create( p_dec, "psz-current-anchor-description",
                               VLC_VAR_ADDRESS|VLC_VAR_DOINHERIT );
            msg_Dbg( p_dec, "creating psz-current-anchor-description" );
            if( var_Set( p_dec, "psz-current-anchor-description", val ) != VLC_SUCCESS )
                msg_Dbg( p_dec, "var_Set of psz-current-anchor-description failed" );
        }

    }

    free( psz_subtitle );
    free( psz_cmml );
    free( p_anchor );
    free( p_clip_parser );
    free( psz_url );
}

