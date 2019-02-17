/*****************************************************************************
 * t140.c : trivial T.140 text encoder
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_sout.h>

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("T.140 text encoder") )
    set_capability( "encoder", 100 )
    set_callbacks( Open, Close )
vlc_module_end ()


static block_t *Encode ( encoder_t *, subpicture_t * );


static int Open( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;

    switch( p_enc->fmt_out.i_codec )
    {
        case VLC_CODEC_SUBT:
            if( ( p_enc->fmt_out.subs.psz_encoding != NULL )
             && strcasecmp( p_enc->fmt_out.subs.psz_encoding, "utf8" )
             && strcasecmp( p_enc->fmt_out.subs.psz_encoding, "UTF-8" ) )
            {
                msg_Err( p_this, "Only UTF-8 encoding supported" );
                return VLC_EGENERIC;
            }
        case VLC_CODEC_ITU_T140:
            break;

        default:
            if( !p_enc->obj.force )
                return VLC_EGENERIC;

            p_enc->fmt_out.i_codec = VLC_CODEC_ITU_T140;
    }

    p_enc->p_sys = NULL;

    p_enc->pf_encode_sub = Encode;
    p_enc->fmt_out.i_cat = SPU_ES;
    return VLC_SUCCESS;
}


static void Close( vlc_object_t *p_this )
{
    (void)p_this;
}


static block_t *Encode( encoder_t *p_enc, subpicture_t *p_spu )
{
    VLC_UNUSED( p_enc );

    subpicture_region_t *p_region;
    block_t *p_block;
    size_t len = 0;

    if( p_spu == NULL )
        return NULL;

    p_region = p_spu->p_region;
    if( ( p_region == NULL )
     || ( p_region->fmt.i_chroma != VLC_CODEC_TEXT )
     || ( p_region->p_text == NULL )
     || ( p_region->p_text->psz_text == NULL) )
        return NULL;

    /* This should already be UTF-8 encoded, so not much effort... */
    for( const text_segment_t *p_segment = p_region->p_text;
                               p_segment; p_segment = p_segment->p_next )
    {
        if( p_segment->psz_text == NULL )
            continue;
        len += strlen( p_segment->psz_text );
    }

    p_block = block_Alloc( len + 1 );
    if( !p_block )
        return NULL;

    p_block->i_buffer = 0;
    for( const text_segment_t *p_segment = p_region->p_text;
                               p_segment; p_segment = p_segment->p_next )
    {
        if( p_segment->psz_text == NULL )
            continue;
        len = strlen( p_segment->psz_text );
        memcpy( &p_block->p_buffer[p_block->i_buffer],
                p_segment->psz_text, len );
        p_block->i_buffer += len;
    }
    p_block->p_buffer[p_block->i_buffer] = 0;

    p_block->i_pts = p_block->i_dts = p_spu->i_start;
    if( !p_spu->b_ephemer && ( p_spu->i_stop > p_spu->i_start ) )
        p_block->i_length = p_spu->i_stop - p_spu->i_start;

    return p_block;
}
