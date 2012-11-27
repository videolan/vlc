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
#include <vlc_fs.h>

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
    add_savefile( "demuxdump-file", "stream-demux.dump", FILE_TEXT,
                  FILE_LONGTEXT, false )
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
    if( !p_demux->b_force )
        return VLC_EGENERIC;

    const char *mode = "wb";
    if( var_InheritBool( p_demux, "demuxdump-append" ) )
        mode = "ab";

    char *path = var_InheritString( p_demux, "demuxdump-file" );
    if( path == NULL )
        return VLC_ENOMEM;

    FILE *stream;
    if( !strcmp( path, "-" ) )
    {
        msg_Info( p_demux, "dumping raw stream to standard output" );
        stream = stdout;
    }
    else
    {
        stream = vlc_fopen( path, mode );
        if( stream == NULL )
            msg_Err( p_demux, "cannot write `%s': %m", path );
        else
            msg_Info( p_demux, "writing raw stream to file `%s'", path );
        free( path );

        if( stream == NULL )
            return VLC_EGENERIC;
    }

    p_demux->p_sys = (void *)stream;
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
    FILE *stream = (void *)p_demux->p_sys;

    if( stream != stdout )
        fclose( stream );
}

/**
 * Copy data from input stream to dump file.
 */
static int Demux( demux_t *p_demux )
{
    FILE *stream = (void *)p_demux->p_sys;
    char buf[DUMP_BLOCKSIZE];

    int rd = stream_Read( p_demux->s, buf, sizeof (buf) );
    if ( rd <= 0 )
        return rd;

    size_t wr = fwrite( buf, 1, rd, stream );
    if( wr != (size_t)rd )
    {
        msg_Err( p_demux, "cannot write data: %m" );
        return -1;
    }
    return 1;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );
}
