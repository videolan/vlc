/*****************************************************************************
 * input_ts.c: TS demux and netlist management
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: input_ts.c,v 1.2 2001/02/14 15:58:29 henri Exp $
 *
 * Authors: 
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
#include "defs.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>


#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "modules.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"
#include "input_ts.h"

#include "mpeg_system.h"
#include "input_netlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  TSProbe     ( probedata_t * );
static int  TSRead      ( struct input_thread_s *,
                          data_packet_t * p_packets[INPUT_READ_ONCE] );
static void TSInit      ( struct input_thread_s * );
static void TSEnd       ( struct input_thread_s * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void input_getfunctions( function_list_t * p_function_list )
{
#define input p_function_list->functions.input
    p_function_list->pf_probe = TSProbe;
    input.pf_init             = TSInit;
    input.pf_open             = input_FileOpen;
    input.pf_close            = input_FileClose;
    input.pf_end              = TSEnd;
    input.pf_read             = TSRead;
    input.pf_demux            = input_DemuxTS;
    input.pf_new_packet       = input_NetlistNewPacket;
    input.pf_new_pes          = input_NetlistNewPES;
    input.pf_delete_packet    = input_NetlistDeletePacket;
    input.pf_delete_pes       = input_NetlistDeletePES;
    input.pf_rewind           = NULL;
    input.pf_seek             = NULL;
#undef input
}

/*****************************************************************************
 * TSProbe: verifies that the stream is a TS stream
 *****************************************************************************/
static int TSProbe( probedata_t * p_data )
{
    if( TestMethod( INPUT_METHOD_VAR, "ts" ) )
    {
        return( 999 );
    }

    /* verify that the first byte is 0x47 */
    return 0;
}

/*****************************************************************************
 * TSInit: initializes TS structures
 *****************************************************************************/
static void TSInit( input_thread_t * p_input )
{
    /* Initialize netlist and TS structures */
    thread_ts_data_t    * p_method;
    pgrm_ts_data_t      * p_pgrm_demux;
    es_descriptor_t     * kludge1;

    /* Initialise structure */
    p_method = malloc( sizeof( thread_ts_data_t ) );
    if( p_method == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }
 
    p_input->p_plugin_data = (void *)p_method;
    p_input->p_method_data = NULL;
 
    
    /* XXX : For the time being, only file, after, i'll do the network */ 
    
    /* Initialize netlist */
    if( input_NetlistInit( p_input, NB_DATA, NB_PES, TS_PACKET_SIZE, 
                INPUT_READ_ONCE ) )
    {
        intf_ErrMsg( "Could not initialize netlist" );
        return;
    }
   
    /* Initialize the stream */
    input_InitStream( p_input, sizeof( stream_ts_data_t ) );

    /* FIXME : PSIDemux and PSIDecode */
    /* Add audio and video programs */
    /* p_input->stream.pp_programs[0] = */
    input_AddProgram( p_input, 0, sizeof( pgrm_ts_data_t ) );
    p_pgrm_demux = 
        (pgrm_ts_data_t *)p_input->stream.pp_programs[0]->p_demux_data;
    p_pgrm_demux->i_pcr_pid = 0x78;

    kludge1 = input_AddES( p_input, p_input->stream.pp_programs[0], 
                           0x78, sizeof( es_ts_data_t ) );

    // kludge
    kludge1->i_type = MPEG2_VIDEO_ES;

    input_SelectES( p_input, kludge1 );

    vlc_mutex_lock( &(p_input->stream.stream_lock) );
    p_input->stream.pp_programs[0]->b_is_ok = 1;
    vlc_mutex_unlock( &(p_input->stream.stream_lock) );

//debug
intf_ErrMsg("End of TSINIT");    
}

/*****************************************************************************
 * TSEnd: frees unused data
 *****************************************************************************/
static void TSEnd( input_thread_t * p_input )
{

}

/*****************************************************************************
 * TSRead: reads data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 if everything went well, and 1 in case of
 * EOF.
 *****************************************************************************/
static int TSRead( input_thread_t * p_input,
                   data_packet_t * pp_packets[INPUT_READ_ONCE] )
{
    unsigned int i_read, i_loop;
    struct iovec * p_iovec;
    
    memset( pp_packets, 0, INPUT_READ_ONCE*sizeof(data_packet_t *) );
    
    p_iovec = input_NetlistGetiovec( p_input->p_method_data );
    
    if ( p_iovec == NULL )
    {
        return( -1 ); /* empty netlist */
    } 

    i_read = readv( p_input->i_handle, p_iovec, INPUT_READ_ONCE );
    
    if( i_read == -1 )
    {
        intf_ErrMsg( "Could not readv" );
        return( -1 );
    }

    input_NetlistMviovec( p_input->p_method_data, 
            (int)(i_read/TS_PACKET_SIZE) , pp_packets );

    /* check correct TS header */
    for( i_loop=0; i_loop < (int)(i_read/TS_PACKET_SIZE); i_loop++ )
    {
        if( pp_packets[i_loop]->p_buffer[0] != 0x47 )
            intf_ErrMsg( "Bad TS Packet (starcode != 0x47)." );
    }
    return 0;
}
