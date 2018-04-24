/*****************************************************************************
 * demuxdump.c : Pseudo demux module for vlc (dump raw stream)
 *****************************************************************************
 * Copyright (C) 2001-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_sout.h>

#define ACCESS_TEXT N_("Dump module")
#define FILE_TEXT N_("Dump filename")
#define FILE_LONGTEXT N_( \
    "Name of the file to which the raw stream will be dumped." )
#define APPEND_TEXT N_("Append to existing file")
#define APPEND_LONGTEXT N_( \
    "If the file already exists, it will not be overwritten." )

static int  Open( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin ()
    set_shortname("Dump")
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("File dumper") )
    set_capability( "demux", 0 )
    add_module("demuxdump-access", "sout access", "file",
               ACCESS_TEXT, ACCESS_TEXT)
    add_savefile("demuxdump-file", "stream-demux.dump",
                 FILE_TEXT, FILE_LONGTEXT)
    add_bool( "demuxdump-append", false, APPEND_TEXT, APPEND_LONGTEXT,
              false )
    set_callbacks( Open, Close )
    add_shortcut( "dump" )
vlc_module_end ()

#define DUMP_BLOCKSIZE  16384

static int Demux( demux_t * );
static int Control( demux_t *, int,va_list );

/**
 * Initializes the raw dump pseudo-demuxer.
 */
static int Open( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t*)p_this;

    /* Accept only if forced */
    if( !p_demux->obj.force )
        return VLC_EGENERIC;

    char *access = var_InheritString( p_demux, "demuxdump-access" );
    if( access == NULL )
        return VLC_EGENERIC;

    /* --sout-file-append (defaults to false) */
    var_Create( p_demux, "sout-file-append", VLC_VAR_BOOL );
    if( var_InheritBool( p_demux, "demuxdump-append" ) )
        var_SetBool( p_demux, "sout-file-append", true );
    /* --sout-file-format (always false) */
    var_Create( p_demux, "sout-file-format", VLC_VAR_BOOL );

    char *path = var_InheritString( p_demux, "demuxdump-file" );
    if( path == NULL )
    {
        free( access );
        msg_Err( p_demux, "no dump file name given" );
        return VLC_EGENERIC;
    }

    sout_access_out_t *out = sout_AccessOutNew( p_demux, access, path );
    free( path );
    free( access );
    if( out == NULL )
    {
        msg_Err( p_demux, "cannot create output" );
        return VLC_EGENERIC;
    }

    p_demux->p_sys = (void *)out;
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    return VLC_SUCCESS;
}

/**
 * Destroys the pseudo-demuxer.
 */
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    sout_access_out_t *out = (void *)p_demux->p_sys;

    sout_AccessOutDelete( out );
}

/**
 * Copy data from input stream to dump file.
 */
static int Demux( demux_t *p_demux )
{
    sout_access_out_t *out = (void *)p_demux->p_sys;

    block_t *block = block_Alloc( DUMP_BLOCKSIZE );
    if( unlikely(block == NULL) )
        return -1;

    int rd = vlc_stream_Read( p_demux->s, block->p_buffer, DUMP_BLOCKSIZE );
    if ( rd <= 0 )
    {
        block_Release( block );
        return rd;
    }
    block->i_buffer = rd;

    size_t wr = sout_AccessOutWrite( out, block );
    if( wr != (size_t)rd )
    {
        msg_Err( p_demux, "cannot write data" );
        return -1;
    }
    return 1;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );
}
