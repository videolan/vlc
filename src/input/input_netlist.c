/*****************************************************************************
 * input_netlist.c: netlist management
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: input_netlist.c,v 1.28 2001/01/07 03:56:40 henri Exp $
 *
 * Authors: Henri Fallon <henri@videolan.org>
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
#include <sys/types.h>
#include <sys/uio.h>                                         /* struct iovec */
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "threads.h"                                                /* mutex */
#include "mtime.h"
#include "intf_msg.h"                                           /* intf_*Msg */

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input_netlist.h"
#include "input.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/*****************************************************************************
 * input_NetlistInit: allocates netlist buffers and init indexes
 *****************************************************************************/
int input_NetlistInit( input_thread_t * p_input, int i_nb_data, int i_nb_pes,
                       size_t i_buffer_size )
{
    unsigned int i_loop;
    netlist_t * p_netlist;

    /* First we allocate and initialise our netlist struct */
    p_input->p_method_data = malloc(sizeof(netlist_t));
    if ( p_input->p_method_data == NULL )
    {
        intf_ErrMsg("Unable to malloc the netlist struct");
        return (-1);
    }
    
    p_netlist = (netlist_t *) p_input->p_method_data;
    
    /* allocate the buffers */ 
    p_netlist->p_buffers = 
        (byte_t *) malloc(i_buffer_size* i_nb_data );
    if ( p_netlist->p_buffers == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (1)");
        return (-1);
    }
    
    p_netlist->p_data = 
        (data_packet_t *) malloc(sizeof(data_packet_t)*(i_nb_data));
    if ( p_netlist->p_data == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (2)");
        return (-1);
    }
    
    p_netlist->p_pes = 
        (pes_packet_t *) malloc(sizeof(pes_packet_t)*(i_nb_pes));
    if ( p_netlist->p_pes == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (3)");
        return (-1);
    }
    
    /* allocate the FIFOs */
    p_netlist->pp_free_data = 
        (data_packet_t **) malloc (i_nb_data * sizeof(data_packet_t *) );
    if ( p_netlist->pp_free_data == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (4)");
    }
    p_netlist->pp_free_pes = 
        (pes_packet_t **) malloc (i_nb_pes * sizeof(pes_packet_t *) );
    if ( p_netlist->pp_free_pes == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (5)");
    }
    
    p_netlist->p_free_iovec = ( struct iovec * )
        malloc( (i_nb_data + INPUT_READ_ONCE) * sizeof(struct iovec) );
    if ( p_netlist->p_free_iovec == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (6)");
    }
    
    /* Fill the data FIFO */
    for ( i_loop = 0; i_loop < i_nb_data; i_loop++ )
    {
        p_netlist->pp_free_data[i_loop] = 
            p_netlist->p_data + i_loop;

        p_netlist->pp_free_data[i_loop]->p_buffer = 
            p_netlist->p_buffers + i_loop * i_buffer_size;
        
        p_netlist->pp_free_data[i_loop]->p_payload_end =
            p_netlist->pp_free_data[i_loop]->p_buffer + i_buffer_size;
    }
    /* Fill the PES FIFO */
    for ( i_loop = 0; i_loop < i_nb_pes ; i_loop++ )
    {
        p_netlist->pp_free_pes[i_loop] = 
            p_netlist->p_pes + i_loop;
    }
   
    /* Deal with the iovec */
    for ( i_loop = 0; i_loop < i_nb_data; i_loop++ )
    {
        p_netlist->p_free_iovec[i_loop].iov_base = 
            p_netlist->p_buffers + i_loop * i_buffer_size;
   
        p_netlist->p_free_iovec[i_loop].iov_len = i_buffer_size;
    }
    
    /* vlc_mutex_init */
    vlc_mutex_init (&p_netlist->lock);
    
    /* initialize indexes */
    p_netlist->i_data_start = 0;
    p_netlist->i_data_end = i_nb_data - 1;

    p_netlist->i_pes_start = 0;
    p_netlist->i_pes_end = i_nb_pes - 1;

    p_netlist->i_nb_data = i_nb_data;
    p_netlist->i_nb_pes = i_nb_pes;
    p_netlist->i_buffer_size = i_buffer_size;

    return (0); /* Everything went all right */
}

/*****************************************************************************
 * input_NetlistGetiovec: returns an iovec pointer for a readv() operation
 *****************************************************************************/
struct iovec * input_NetlistGetiovec( void * p_method_data )
{
    netlist_t * p_netlist;

    /* cast */
    p_netlist = ( netlist_t * ) p_method_data;
    
    /* check */
    if ( 
     (p_netlist->i_data_end - p_netlist->i_data_start + p_netlist->i_nb_data)
     %p_netlist->i_nb_data < INPUT_READ_ONCE )
    {
        intf_ErrMsg("Empty iovec FIFO. Unable to allocate memory");
        return (NULL);
    }

    /* readv only takes contiguous buffers 
     * so, as a solution, we chose to have a FIFO a bit longer
     * than i_nb_data, and copy the begining of the FIFO to its end
     * if the readv needs to go after the end */
    if( p_netlist->i_nb_data - p_netlist->i_data_start < INPUT_READ_ONCE )
        memcpy( &p_netlist->p_free_iovec[p_netlist->i_nb_data], 
                p_netlist->p_free_iovec, 
                INPUT_READ_ONCE-(p_netlist->i_nb_data-p_netlist->i_data_start)
                * sizeof(struct iovec *)
              );
 
    /* Initialize payload start and end */
    p_netlist->pp_free_data[p_netlist->i_data_start]->p_payload_start 
        = p_netlist->pp_free_data[p_netlist->i_data_start]->p_buffer;
 
    p_netlist->pp_free_data[p_netlist->i_data_start]->p_payload_end 
        = p_netlist->pp_free_data[p_netlist->i_data_start]->p_payload_start
        + p_netlist->i_buffer_size;

    return &p_netlist->p_free_iovec[p_netlist->i_data_start];

}

/*****************************************************************************
 * input_NetlistMviovec: move the iovec pointer after a readv() operation
 *****************************************************************************/
void input_NetlistMviovec( void * p_method_data, size_t i_nb_iovec )
{
    netlist_t * p_netlist;

    /* cast */
    p_netlist = (netlist_t *) p_method_data;
    
    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );
    
    p_netlist->i_data_start += i_nb_iovec;
    p_netlist->i_data_start %= p_netlist->i_nb_data;

    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);
    
}

/*****************************************************************************
 * input_NetlistNewPacket: returns a free data_packet_t
 *****************************************************************************/
struct data_packet_s * input_NetlistNewPacket( void * p_method_data,
                                               size_t i_buffer_size )
{    
    netlist_t * p_netlist; 
    struct data_packet_s * p_return;
    
    /* cast */
    p_netlist = ( netlist_t * ) p_method_data; 

#ifdef DEBUG
    if( i_buffer_size > p_netlist->i_buffer_size )
    {
        /* This should not happen */
        intf_ErrMsg( "Netlist packet too small !" );
        return NULL;
    }
#endif

    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );
        
    /* check */
    if ( p_netlist->i_data_start == p_netlist->i_data_end )
    {
        intf_ErrMsg("Empty Data FIFO in netlist. Unable to allocate memory");
        return ( NULL );
    }
    
    p_return = (p_netlist->pp_free_data[p_netlist->i_data_start]);
    p_netlist->i_data_start++;
    p_netlist->i_data_start %= p_netlist->i_nb_data;

    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);

    if (i_buffer_size < p_netlist->i_buffer_size) 
    {
        p_return->p_payload_end = p_return->p_payload_start + i_buffer_size;
    }
   
    /* initialize data */
    p_return->p_next = NULL;
    p_return->b_discard_payload = 0;
    
    p_return->p_payload_start = p_return->p_buffer;
    p_return->p_payload_end = p_return->p_payload_start + i_buffer_size;
    
    return ( p_return );
}

/*****************************************************************************
 * input_NetlistNewPES: returns a free pes_packet_t
 *****************************************************************************/
struct pes_packet_s * input_NetlistNewPES( void * p_method_data )
{
    netlist_t * p_netlist;
    pes_packet_t * p_return;
    
    /* cast */ 
    p_netlist = (netlist_t *) p_method_data;
    
    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );
    
    /* check */
    if ( p_netlist->i_pes_start == p_netlist->i_pes_end )
    {
        intf_ErrMsg("Empty PES FIFO in netlist - Unable to allocate memory");
        return ( NULL );
    }

    /* allocate */
    p_return = p_netlist->pp_free_pes[p_netlist->i_pes_start];
    p_netlist->i_pes_start++;
    p_netlist->i_pes_start %= p_netlist->i_nb_pes; 
   
    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);
    
    /* initialize PES */
    p_return->b_messed_up = 
        p_return->b_data_alignment = 
        p_return->b_discontinuity = 
        p_return->i_pts = p_return->i_dts = 0;
    p_return->i_pes_size = 0;
    p_return->p_first = NULL;
   
    return ( p_return );
}

/*****************************************************************************
 * input_NetlistDeletePacket: puts a data_packet_t back into the netlist
 *****************************************************************************/
void input_NetlistDeletePacket( void * p_method_data, data_packet_t * p_data )
{
    netlist_t * p_netlist;
    
    /* cast */
    p_netlist = (netlist_t *) p_method_data;

    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );

    /* Delete data_packet */
    p_netlist->i_data_end ++;
    p_netlist->i_data_end %= p_netlist->i_nb_data;
    p_netlist->pp_free_data[p_netlist->i_data_end] = p_data;
    
    p_netlist->p_free_iovec[p_netlist->i_data_end].iov_base = p_data->p_buffer;
    
    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);
}

/*****************************************************************************
 * input_NetlistDeletePES: puts a pes_packet_t back into the netlist
 *****************************************************************************/
void input_NetlistDeletePES( void * p_method_data, pes_packet_t * p_pes )
{
    netlist_t * p_netlist; 
    data_packet_t * p_current_packet;
    
    /* cast */
    p_netlist = (netlist_t *)p_method_data;

    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );

    /* delete free  p_pes->p_first, p_next ... */
    p_current_packet = p_pes->p_first;
    while ( p_current_packet != NULL )
    {
        /* copy of NetListDeletePacket, duplicate code avoid many locks */

        p_netlist->i_data_end ++;
        p_netlist->i_data_end %= p_netlist->i_nb_data;
        p_netlist->pp_free_data[p_netlist->i_data_end] = p_current_packet;
        
        p_netlist->p_free_iovec[p_netlist->i_data_end].iov_base 
            = p_netlist->p_data->p_buffer;
    
        p_current_packet = p_current_packet->p_next;
    }
 
    /* delete our current PES packet */
    p_netlist->i_pes_end ++;
    p_netlist->i_pes_end %= p_netlist->i_nb_pes;
    p_netlist->pp_free_pes[p_netlist->i_pes_end] = p_pes;
    
    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);
}

/*****************************************************************************
 * input_NetlistEnd: frees all allocated structures
 *****************************************************************************/
void input_NetlistEnd( input_thread_t * p_input)
{
    netlist_t * p_netlist;

    /* cast */
    p_netlist = ( netlist_t * ) p_input->p_method_data;
    
    /* destroy the mutex lock */
    vlc_mutex_destroy (&p_netlist->lock);
    
    /* free the FIFO, the buffer, and the netlist structure */
    free (p_netlist->pp_free_data);
    free (p_netlist->pp_free_pes);
    free (p_netlist->p_pes);
    free (p_netlist->p_data);
    free (p_netlist->p_buffers);

    /* free the netlist */
    free (p_netlist);
}
