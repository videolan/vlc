/*****************************************************************************
 * mpeg_ts.c : Transport Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: mpeg_ts.c,v 1.6 2002/03/15 17:17:35 sam Exp $
 *
 * Authors: Henri Fallon <henri@via.ecp.fr>
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <videolan/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

/*****************************************************************************
 * Constants
 *****************************************************************************/
#define TS_READ_ONCE 200

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list );
static int  TSInit      ( struct input_thread_s * );
static void TSEnd       ( struct input_thread_s * );
static int  TSDemux     ( struct input_thread_s * );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "ISO 13818-1 MPEG Transport Stream input" )
    ADD_CAPABILITY( DEMUX, 160 )
    ADD_SHORTCUT( "ts" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    input_getfunctions( &p_module->p_functions->demux );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list )
{
#define input p_function_list->functions.demux
    input.pf_init             = TSInit;
    input.pf_end              = TSEnd;
    input.pf_demux            = TSDemux;
    input.pf_rewind           = NULL;
#undef input
}

/*****************************************************************************
 * TSInit: initializes TS structures
 *****************************************************************************/
static int TSInit( input_thread_t * p_input )
{
    es_descriptor_t     * p_pat_es;
    es_ts_data_t        * p_demux_data;
    stream_ts_data_t    * p_stream_data;
    byte_t              * p_peek;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    /* Have a peep at the show. */
    if( input_Peek( p_input, &p_peek, 1 ) < 1 )
    {
        intf_ErrMsg( "input error: cannot peek() (mpeg_ts)" );
        return( -1 );
    }

    if( *p_peek != TS_SYNC_CODE )
    {
        if( *p_input->psz_demux && strncmp( p_input->psz_demux, "ts", 3 ) )
        {
            /* User forced */
            intf_ErrMsg( "input error: this doesn't seem like a TS stream, continuing" );
        }
        else
        {
            intf_WarnMsg( 2, "input: TS plug-in discarded (no sync)" );
            return( -1 );
        }
    }

    /* Adapt the bufsize for our only use. */
    if( p_input->i_mtu != 0 )
    {
        /* Have minimum granularity to avoid bottlenecks at the input level. */
        p_input->i_bufsize = (p_input->i_mtu / TS_PACKET_SIZE) * TS_PACKET_SIZE;
    }

    if( input_InitStream( p_input, sizeof( stream_ts_data_t ) ) == -1 )
    {
        return( -1 );
    }

    p_stream_data = (stream_ts_data_t *)p_input->stream.p_demux_data;
    p_stream_data->i_pat_version = PAT_UNINITIALIZED ;

    /* We'll have to catch the PAT in order to continue
     * Then the input will catch the PMT and then the others ES
     * The PAT es is indepedent of any program. */
    p_pat_es = input_AddES( p_input, NULL,
                            0x00, sizeof( es_ts_data_t ) );
    p_demux_data = (es_ts_data_t *)p_pat_es->p_demux_data;
    p_demux_data->b_psi = 1;
    p_demux_data->i_psi_type = PSI_IS_PAT;
    p_demux_data->p_psi_section = malloc(sizeof(psi_section_t));
    p_demux_data->p_psi_section->b_is_complete = 1;

    return( 0 );
}

/*****************************************************************************
 * TSEnd: frees unused data
 *****************************************************************************/
static void TSEnd( input_thread_t * p_input )
{
}

/*****************************************************************************
 * TSDemux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * packets.
 *****************************************************************************/
static int TSDemux( input_thread_t * p_input )
{
    int             i_read_once = (p_input->i_mtu ?
                                   p_input->i_bufsize / TS_PACKET_SIZE :
                                   TS_READ_ONCE);
    int             i;

    for( i = 0; i < i_read_once; i++ )
    {
        data_packet_t *     p_data;
        ssize_t             i_result;

        i_result = input_ReadTS( p_input, &p_data );

        if( i_result <= 0 )
        {
            return( i_result );
        }

        input_DemuxTS( p_input, p_data );
    }

    return( i_read_once );
}

