/*****************************************************************************
 * netlist.c: input thread
 * Manages the TS and PES netlists (see netlist.h).
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */

#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <errno.h>                                                  /* errno */

#include "threads.h"
#include "common.h"
#include "config.h"
#include "mtime.h"
#include "intf_msg.h"
#include "debug.h"
#include "input.h"
#include "input_netlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/*****************************************************************************
 * input_NetlistOpen: initialize the netlists buffers
 *****************************************************************************/
int input_NetlistInit( input_thread_t *p_input )
{
    int                 i_base, i_packets, i_iovec;

    /* Initialize running indexes. */
#ifdef INPUT_LIFO_TS_NETLIST
    p_input->netlist.i_ts_index = INPUT_TS_READ_ONCE;
#else
    p_input->netlist.i_ts_start = 0;
    p_input->netlist.i_ts_end = 0;
#endif
#ifdef INPUT_LIFO_PES_NETLIST
    p_input->netlist.i_pes_index = 1; /* We allocate one PES at a time */
#else
    p_input->netlist.i_pes_start = 0;
    p_input->netlist.i_pes_end = 0;
#endif

    /* Initialize all iovec from the TS netlist with the length of a packet */
    for( i_iovec = 0; i_iovec < INPUT_MAX_TS + INPUT_TS_READ_ONCE; i_iovec++ )
    {
        p_input->netlist.p_ts_free[i_iovec].iov_len = TS_PACKET_SIZE;
    }

    /* Allocate a big piece of memory to contain the INPUT_MAX_TS TS packets */
    if( ( p_input->netlist.p_ts_packets = malloc( (INPUT_MAX_TS + 1)
                                             * sizeof(ts_packet_t) ) ) == NULL )
    {
        intf_ErrMsg("input error: can't allocate TS netlist buffer (%s)\n",
                    strerror(errno) );
        return( -1 );
    }

  /* Allocate a big piece of memory to contain the INPUT_MAX_PES PES packets */
    if( !( p_input->netlist.p_pes_packets = malloc( (INPUT_MAX_PES + 1)
                                             * sizeof(pes_packet_t) ) ) )
    {
        intf_ErrMsg("input error: can't allocate PES netlist buffer (%s)\n",
                    strerror(errno) );
        free( p_input->netlist.p_ts_packets );
        return( -1 );
    }

    /* Insert TS packets into the TS netlist */
#ifdef INPUT_LIFO_TS_NETLIST
    i_base = p_input->netlist.i_ts_index;
#else
    i_base = p_input->netlist.i_ts_start;
#endif
    /* i_base is now the base address to locate free packets in the netlist */

    for( i_packets = 0; i_packets < INPUT_MAX_TS + 1; i_packets++ )
    {
        p_input->netlist.p_ts_free[i_base + i_packets].iov_base
                          = (void *)(p_input->netlist.p_ts_packets + i_packets);
        /* Initialize TS length. */
        (p_input->netlist.p_ts_packets[i_packets]).i_payload_end = TS_PACKET_SIZE;
    }

    /* Insert PES packets into the netlist */
#ifdef INPUT_LIFO_PES_NETLIST
    i_base = p_input->netlist.i_pes_index;
#else
    i_base = p_input->netlist.i_pes_start;
#endif
    /* i_base is now the base address to locate free packets in the netlist */

    for( i_packets = 0; i_packets < INPUT_MAX_PES + 1; i_packets++ )
    {
        p_input->netlist.p_pes_free[i_base + i_packets]
                              = p_input->netlist.p_pes_packets + i_packets;
    }

    /* the p_pes_header_save buffer is allocated on the fly by the PES
       demux if needed, and freed with the PES packet when the netlist
       is destroyed. We initialise the field to NULL so that the demux
       can determine if it has already allocated this buffer or not. */
    for( i_packets = 0; i_packets < INPUT_MAX_PES + 1; i_packets++ )
    {
      p_input->netlist.p_pes_packets[i_packets].p_pes_header_save = NULL;
    }

    return( 0 );
}

/*****************************************************************************
 * input_NetlistClean: clean the netlists buffers
 *****************************************************************************/
void input_NetlistEnd( input_thread_t *p_input )
{
    int i;

    /* free TS netlist */
    free( p_input->netlist.p_ts_packets );

    /* free the pes_buffer_save buffers of the PES packets if they have
       been allocated */
    for( i = 0; i < INPUT_MAX_PES + 1; i++ )
    {
        byte_t* p_buffer = p_input->netlist.p_pes_packets[i].p_pes_header_save;
        if(p_buffer)
            free(p_buffer);
    }

    /* free PES netlist */
    free( p_input->netlist.p_pes_packets );
}

