/*****************************************************************************
 * ddummy.c: dummy decoder plugin for vlc.
 *****************************************************************************
 * Copyright (C) 2002 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <vlc_codec.h>
#include <vlc_fs.h>

#define SAVE_TEXT N_("Save raw codec data")
#define SAVE_LONGTEXT N_( \
    "Save the raw codec data if you have selected/forced the dummy " \
    "decoder in the main options." )

static int OpenDecoder( vlc_object_t * );
static int OpenDecoderDump( vlc_object_t * );
static void CloseDecoder( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Dummy") )
    set_description( N_("Dummy decoder") )
    set_capability( "spu decoder", 0 )
    set_callbacks( OpenDecoder, CloseDecoder )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    add_bool( "dummy-save-es", false, SAVE_TEXT, SAVE_LONGTEXT )
    add_shortcut( "dummy" )
    add_submodule()
    add_shortcut( "dummy" )
    set_capability( "video decoder", 0 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_submodule()
    add_shortcut( "dummy" )
    set_capability( "audio decoder", 0 )
    set_callbacks( OpenDecoder, CloseDecoder )

    add_submodule ()
    set_section( N_( "Dump decoder" ), NULL )
    set_description( N_("Dump decoder") )
    set_capability( "spu decoder", -1 )
    set_callbacks( OpenDecoderDump, CloseDecoder )
    add_shortcut( "dump" )
    add_submodule()
    add_shortcut( "dump")
    set_capability( "video decoder", 0 )
    set_callbacks( OpenDecoderDump, CloseDecoder )
    add_submodule()
    add_shortcut( "dump")
    set_capability( "audio decoder", 0 )
    set_callbacks( OpenDecoderDump, CloseDecoder )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int DecodeBlock( decoder_t *p_dec, block_t *p_block );

/*****************************************************************************
 * OpenDecoder: Open the decoder
 *****************************************************************************/
static int OpenDecoderCommon( vlc_object_t *p_this, bool b_force_dump )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    char psz_file[10 + 3 * sizeof (p_dec)];

    snprintf( psz_file, sizeof( psz_file), "stream.%p", (void *)p_dec );

    if( !b_force_dump )
        b_force_dump = var_InheritBool( p_dec, "dummy-save-es" );
    if( b_force_dump )
    {
        FILE *stream = vlc_fopen( psz_file, "wb" );
        if( stream == NULL )
        {
            msg_Err( p_dec, "cannot create `%s'", psz_file );
            return VLC_EGENERIC;
        }
        msg_Dbg( p_dec, "dumping stream to file `%s'", psz_file );
        p_dec->p_sys = (void *)stream;
    }
    else
        p_dec->p_sys = NULL;

    /* Set callbacks */
    p_dec->pf_decode = DecodeBlock;

    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );

    return VLC_SUCCESS;
}

static int OpenDecoder( vlc_object_t *p_this )
{
    return OpenDecoderCommon( p_this, false );
}

static int  OpenDecoderDump( vlc_object_t *p_this )
{
    return OpenDecoderCommon( p_this, true );
}

/****************************************************************************
 * RunDecoder: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    FILE *stream = (void *)p_dec->p_sys;

    if( !p_block ) return VLCDEC_SUCCESS;

    if( stream != NULL
     && p_block->i_buffer > 0
     && !(p_block->i_flags & (BLOCK_FLAG_CORRUPTED)) )
    {
        fwrite( p_block->p_buffer, 1, p_block->i_buffer, stream );
        msg_Dbg( p_dec, "dumped %zu bytes", p_block->i_buffer );
    }
    block_Release( p_block );

    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    FILE *stream = (void *)p_dec->p_sys;

    if( stream != NULL )
        fclose( stream );
}
