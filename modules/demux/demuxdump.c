/*****************************************************************************
 * demuxdump.c : Pseudo demux module for vlc (dump raw stream)
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FILE_TEXT N_("Filename of dump")
#define FILE_LONGTEXT N_( \
    "Specify a file name to which the raw stream will be dumped." )
#define APPEND_TEXT N_("Append")
#define APPEND_LONGTEXT N_( \
    "If the file exists and this option is selected, the existing file " \
    "will not be overwritten." )

static int  Open( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin();
    set_shortname("Dump");
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( _("Filedump demuxer") );
    set_capability( "demux2", 0 );
    add_file( "demuxdump-file", "stream-demux.dump", NULL, FILE_TEXT,
              FILE_LONGTEXT, VLC_FALSE );
    add_bool( "demuxdump-append", 0, NULL, APPEND_TEXT, APPEND_LONGTEXT,
              VLC_FALSE );
    set_callbacks( Open, Close );
    add_shortcut( "dump" );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t * );
static int Control( demux_t *, int,va_list );

#define DUMP_BLOCKSIZE  16384

struct demux_sys_t
{
    char        *psz_file;
    FILE        *p_file;
    uint64_t    i_write;

    uint8_t     buffer[DUMP_BLOCKSIZE];
};

/*
 * Data reading functions
 */

/*****************************************************************************
 * Open: initializes dump structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    char        *psz_mode;
    vlc_value_t val;
    vlc_bool_t  b_append;

    /* Accept only if forced */
    if( strcasecmp( p_demux->psz_demux, "dump" ) )
        return VLC_EGENERIC;

    var_Create( p_demux, "demuxdump-append", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );
    var_Get( p_demux, "demuxdump-append", &val );
    b_append = val.b_bool;
    if ( b_append )
        psz_mode = "ab";
    else
        psz_mode = "wb";

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->i_write = 0;
    p_sys->p_file = NULL;
    p_sys->psz_file = var_CreateGetString( p_demux, "demuxdump-file" );
    if( *p_sys->psz_file == '\0' )
    {
        msg_Warn( p_demux, "no dump file name given" );
        return VLC_EGENERIC;
    }

    if( !strcmp( p_sys->psz_file, "-" ) )
    {
        msg_Info( p_demux, "dumping raw stream to standard output" );
        p_sys->p_file = stdout;
    }
    else if( ( p_sys->p_file = fopen( p_sys->psz_file, psz_mode ) ) == NULL )
    {
        msg_Err( p_demux, "cannot create `%s' for writing", p_sys->psz_file );

        free( p_sys );
        return VLC_EGENERIC;
    }
    msg_Info( p_demux, "%s raw stream to file `%s'",
              b_append ? "appending" : "dumping", p_sys->psz_file );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{

    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    msg_Info( p_demux ,"closing %s ("I64Fd" Kbytes dumped)", p_sys->psz_file,
              p_sys->i_write / 1024 );

    if( p_sys->p_file != stdout )
    {
        fclose( p_sys->p_file );
    }
    free( p_sys->psz_file );

    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    int i_data;

    /* I'm pretty sure that stream_Peek,stream_Read( , NULL ) would be faster*/
    i_data = stream_Read( p_demux->s, p_sys->buffer, DUMP_BLOCKSIZE );
    if ( i_data <= 0 )
        return i_data;

    i_data = fwrite( p_sys->buffer, 1, i_data, p_sys->p_file );

    if( i_data == 0 )
    {
        msg_Err( p_demux, "failed to write data" );
        return -1;
    }
#if 0
    msg_Dbg( p_demux, "dumped %d bytes", i_data );
#endif

    p_sys->i_write += i_data;

    return 1;
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return demux2_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );
}

