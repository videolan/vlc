/*****************************************************************************
 * input_ts.c: TS demux and netlist management
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: input_ts.c,v 1.2 2000/12/21 15:01:08 massiot Exp $
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
#include <netinet/in.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"

#include "mpeg_system.h"
#include "input_netlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  TSProbe     ( struct input_thread_s * );
static int  TSRead      ( struct input_thread_s *,
                          data_packet_t * p_packets[INPUT_READ_ONCE] );
static void TSInit      ( struct input_thread_s * );
static void TSEnd       ( struct input_thread_s * );

/*****************************************************************************
 * TSProbe: verifies that the stream is a TS stream
 *****************************************************************************/
static int TSProbe( input_thread_t * p_input )
{
    /* verify that the first byte is 0x47 */
    return 1;
}

/*****************************************************************************
 * TSInit: initializes TS structures
 *****************************************************************************/
static void TSInit( input_thread_t * p_input )
{
    /* Initialize netlist and TS structures */
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
    return -1;
}

/*****************************************************************************
 * TSKludge: fakes a TS plugin (FIXME)
 *****************************************************************************/
input_capabilities_t * TSKludge( void )
{
    input_capabilities_t *  p_plugin;

    p_plugin = (input_capabilities_t *)malloc( sizeof(input_capabilities_t) );
    p_plugin->pf_probe = TSProbe;
    p_plugin->pf_init = TSInit;
    p_plugin->pf_end = TSEnd;
    p_plugin->pf_read = TSRead;
    p_plugin->pf_demux = input_DemuxTS; /* FIXME: use i_p_config_t ! */
    p_plugin->pf_new_packet = input_NetlistNewPacket;
    p_plugin->pf_new_pes = input_NetlistNewPES;
    p_plugin->pf_delete_packet = input_NetlistDeletePacket;
    p_plugin->pf_delete_pes = input_NetlistDeletePES;
    p_plugin->pf_rewind = NULL;
    p_plugin->pf_seek = NULL;

    return( p_plugin );
}

