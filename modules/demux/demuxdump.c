/*****************************************************************************
 * demuxdump.c : Pseudo demux module for vlc (dump raw stream)
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: demuxdump.c,v 1.2 2002/12/18 14:17:10 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <sys/types.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate ( vlc_object_t * );
static int  Demux ( input_thread_t * );
static void Desactivate ( vlc_object_t * );

#define DUMP_BLOCKSIZE  16384

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Dump Demux input") );
    set_capability( "demux", 0 );
    add_category_hint( "File", NULL );
        add_string( "demuxdump-file", NULL, NULL, 
                    "dump file name", 
                    "file name for dumping raw stream read by demux" );
    set_callbacks( Activate, Desactivate );
    add_shortcut( "dump" );
vlc_module_end();

struct demux_sys_t
{
    char        *psz_name;
    FILE        *p_file;
    uint64_t    i_write;

    void        *p_demux_data_sav;
};

/*
 * Data reading functions
 */

/*****************************************************************************
 * Activate: initializes dump structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t      *p_input = (input_thread_t *)p_this;
    demux_sys_t         *p_demux;

    char                *psz_name;

    /* Set the demux function */
    p_input->pf_demux = Demux;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }
    
    psz_name = config_GetPsz( p_input, "demuxdump-file" );
    if( !psz_name || !*psz_name )
    {
        psz_name = strdup( "stream-demux.dump" );
    }

    p_demux = malloc( sizeof( demux_sys_t ) );
    memset( p_demux, 0, sizeof( demux_sys_t ) );

    if( !( p_demux->p_file = fopen( psz_name, "wb" ) ) )
    {
        msg_Err( p_input,
                 "cannot create `%s' for writing", 
                 psz_name );
        free( p_demux );
        return( -1 );
    }
    else
    {
        msg_Info( p_input,
                  "dumping raw stream to file `%s'", 
                  psz_name );
        p_demux->psz_name = psz_name;
    }
    
    p_demux->i_write = 0;
    p_demux->p_demux_data_sav = p_input->p_demux_data;

    if( p_input->stream.p_selected_program != NULL )
    {
        /* workaround for dvd access */
        msg_Warn( p_input, "demux data already initializated (by access?)" );
    }
    else
    {
    
        if( input_InitStream( p_input, 0 ) == -1 )
        {
            fclose( p_demux->p_file );
            free( p_demux );
            return( -1 );
        }
        input_AddProgram( p_input, 0, 0 );
        p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
        
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.p_selected_area->i_tell = 0;
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

    p_input->p_demux_data = p_demux;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    
    return( 0 );
}

/*****************************************************************************
 * Desctivate: initializes dump structures
 *****************************************************************************/
static void Desactivate ( vlc_object_t *p_this )
{
    input_thread_t      *p_input = (input_thread_t *)p_this;
    demux_sys_t         *p_demux = (demux_sys_t*)p_input->p_demux_data;
    
    msg_Info( p_input, 
              "closing %s ("I64Fd" Kbytes dumped)", 
              p_demux->psz_name,
              p_demux->i_write / 1024 );
   
    if( p_demux->p_file )
    {
        fclose( p_demux->p_file );
        p_demux->p_file = NULL;
    }
    if( p_demux->psz_name )
    {
        free( p_demux->psz_name );    
    }
    p_input->p_demux_data = p_demux->p_demux_data_sav;
    free( p_demux );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    demux_sys_t         *p_demux = (demux_sys_t*)p_input->p_demux_data;

    ssize_t         i_read;
    data_packet_t * p_data;
    int             i_write;

    p_input->p_demux_data = p_demux->p_demux_data_sav;
    i_read = input_SplitBuffer( p_input, &p_data, DUMP_BLOCKSIZE );
    p_input->p_demux_data = p_demux;
    
    if ( i_read <= 0 )
    {
        return i_read;
    }

    i_write = fwrite( p_data->p_payload_start,
                       1,
                       i_read,
                       p_demux->p_file );
    
    input_DeletePacket( p_input->p_method_data, p_data );

    if( i_write < 0 )
    {
        msg_Err( p_input, 
                 "failed to write %d bytes",
                 i_write );
        return( -1 );
    }
    else
    {
        msg_Dbg( p_input,
                 "dumped %d bytes",
                 i_write );
        p_demux->i_write += i_write;
    }


    if( (p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT)
         | (input_ClockManageControl( p_input, 
                      p_input->stream.p_selected_program,
                         (mtime_t)0 ) == PAUSE_S) )
    {
        msg_Warn( p_input, "synchro reinit" );
        p_input->stream.p_selected_program->i_synchro_state = SYNCHRO_OK;
    }

    return( 1 );
}

