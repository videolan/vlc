/*****************************************************************************
 * dvd_netlist.c: Specific netlist for DVD packets
 *****************************************************************************
 * The original is in src/input.
 * There is only one major change from input_netlist.c : data is now a
 * pointer to an offset in iovec ; and iovec has a reference counter. It
 * will only be given back to netlist when refcount is zero.
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000, 2001 VideoLAN
 * $Id: dvd_netlist.c,v 1.12 2001/07/17 09:48:07 massiot Exp $
 *
 * Authors: Henri Fallon <henri@videolan.org>
 *          Stéphane Borel <stef@videolan.org>
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
#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if defined( WIN32 )
#   include <io.h>
#   include "input_iovec.h"
#else
#   include <sys/uio.h>                                      /* struct iovec */
#endif

#include "config.h"
#include "common.h"
#include "threads.h"                                                /* mutex */
#include "mtime.h"
#include "intf_msg.h"                                           /* intf_*Msg */

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "dvd_netlist.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/*****************************************************************************
 * DVDNetlistInit: allocates netlist buffers and init indexes
 * ---
 * Changes from input_NetList: we have to give the length of the buffer which
 * is different from i_nb_data now, since we may have several data pointers
 * in one iovec. Thus we can only delete an iovec when its refcount is 0.
 * We only received a buffer with a GetIovec whereas NewPacket gives a pointer.
 *
 * Warning: i_nb_iovec, i_nb_data, i_nb_pes have to be 2^x
 *****************************************************************************/
dvd_netlist_t * DVDNetlistInit( int i_nb_iovec, int i_nb_data, int i_nb_pes,
                                size_t i_buffer_size, int i_read_once )
{
    unsigned int        i_loop;
    dvd_netlist_t *     p_netlist;

    /* First we allocate and initialise our netlist struct */
    p_netlist = malloc( sizeof(dvd_netlist_t) );
    if ( p_netlist == NULL )
    {
        intf_ErrMsg("Unable to malloc the DVD netlist struct");
        free( p_netlist );
        return NULL;
    }
    
    /* Nb of packets read once by input */
    p_netlist->i_read_once = i_read_once;
    
    /* allocate the buffers */ 
    p_netlist->p_buffers = malloc( i_nb_iovec *i_buffer_size );
    if ( p_netlist->p_buffers == NULL )
    {
        intf_ErrMsg ("Unable to malloc in DVD netlist initialization (1)");
        free( p_netlist->p_buffers );
        free( p_netlist );
        return NULL;
    }
    
    /* table of pointers to data packets */
    p_netlist->p_data = malloc( i_nb_data *sizeof(data_packet_t) );
    if ( p_netlist->p_data == NULL )
    {
        intf_ErrMsg ("Unable to malloc in DVD netlist initialization (2)");
        free( p_netlist->p_buffers );
        free( p_netlist->p_data );
        free( p_netlist );
        return NULL;
    }
    
    /* table of pointer to PES packets */
    p_netlist->p_pes = malloc( i_nb_pes *sizeof(pes_packet_t) );
    if ( p_netlist->p_pes == NULL )
    {
        intf_ErrMsg ("Unable to malloc in DVD netlist initialization (3)");
        free( p_netlist->p_buffers );
        free( p_netlist->p_data );
        free( p_netlist->p_pes );
        free( p_netlist );
        return NULL;
    }
    
    /* allocate the FIFOs : tables of free pointers */
    p_netlist->pp_free_data = 
                        malloc( i_nb_data *sizeof(data_packet_t *) );
    if ( p_netlist->pp_free_data == NULL )
    {
        intf_ErrMsg ("Unable to malloc in DVD netlist initialization (4)");
        free( p_netlist->p_buffers );
        free( p_netlist->p_data );
        free( p_netlist->p_pes );
        free( p_netlist->pp_free_data );
        free( p_netlist );
        return NULL;
    }
    p_netlist->pp_free_pes = 
                        malloc( i_nb_pes *sizeof(pes_packet_t *) );
    if ( p_netlist->pp_free_pes == NULL )
    {
        intf_ErrMsg ("Unable to malloc in DVD netlist initialization (5)");
        free( p_netlist->p_buffers );
        free( p_netlist->p_data );
        free( p_netlist->p_pes );
        free( p_netlist->pp_free_data );
        free( p_netlist->pp_free_pes );
        free( p_netlist );
        return NULL;
    }
    
    p_netlist->p_free_iovec =
        malloc( (i_nb_iovec + p_netlist->i_read_once) * sizeof(struct iovec) );
    if ( p_netlist->p_free_iovec == NULL )
    {
        intf_ErrMsg ("Unable to malloc in DVD netlist initialization (6)");
        free( p_netlist->p_buffers );
        free( p_netlist->p_data );
        free( p_netlist->p_pes );
        free( p_netlist->pp_free_data );
        free( p_netlist->pp_free_pes );
        free( p_netlist->p_free_iovec );
        free( p_netlist );
        return NULL;
    }

    /* table for reference counter of iovecs */
    p_netlist->pi_refcount = malloc( i_nb_iovec *sizeof(int) );
    if ( p_netlist->pi_refcount == NULL )
    {
        intf_ErrMsg ("Unable to malloc in DVD netlist initialization (7)");
        free( p_netlist->p_buffers );
        free( p_netlist->p_data );
        free( p_netlist->p_pes );
        free( p_netlist->pp_free_data );
        free( p_netlist->pp_free_pes );
        free( p_netlist->p_free_iovec );
        free( p_netlist->pi_refcount );
        free( p_netlist );
        return NULL;
    }

    /* Fill the data FIFO */
    for ( i_loop = 0; i_loop < i_nb_data; i_loop++ )
    {
        p_netlist->pp_free_data[i_loop] = 
            p_netlist->p_data + i_loop;
    }

    /* Fill the PES FIFO */
    for ( i_loop = 0; i_loop < i_nb_pes ; i_loop++ )
    {
        p_netlist->pp_free_pes[i_loop] = 
            p_netlist->p_pes + i_loop;
    }
   
    /* Deal with the iovec */
    for ( i_loop = 0; i_loop < i_nb_iovec; i_loop++ )
    {
        p_netlist->p_free_iovec[i_loop].iov_base = 
            p_netlist->p_buffers + i_loop * i_buffer_size;
   
        p_netlist->p_free_iovec[i_loop].iov_len = i_buffer_size;
    }

    /* initialize reference counters */
    memset( p_netlist->pi_refcount, 0, i_nb_iovec *sizeof(int) );
   
    /* vlc_mutex_init */
    vlc_mutex_init (&p_netlist->lock);
    
    /* initialize indexes */
    p_netlist->i_iovec_start = 0;
    p_netlist->i_iovec_end = i_nb_iovec - 1;

    p_netlist->i_data_start = 0;
    p_netlist->i_data_end = i_nb_data - 1;

    p_netlist->i_pes_start = 0;
    p_netlist->i_pes_end = i_nb_pes - 1;

    /* we give (nb - 1) to use & instead of %
     * if you really need nb you have to add 1 */
    p_netlist->i_nb_iovec = i_nb_iovec - 1;
    p_netlist->i_nb_data = i_nb_data - 1;
    p_netlist->i_nb_pes = i_nb_pes - 1;
    p_netlist->i_buffer_size = i_buffer_size;

    return p_netlist; /* Everything went all right */
}

/*****************************************************************************
 * DVDGetiovec: returns an iovec pointer for a readv() operation
 *****************************************************************************
 * We return an iovec vector, so that readv can read many packets at a time.
 * pp_data will be set to direct to the fifo pointer in DVDMviovec, which
 * will allow us to get the corresponding data_packet.
 *****************************************************************************/
struct iovec * DVDGetiovec( void * p_method_data )
{
    dvd_netlist_t *     p_netlist;

    /* cast */
    p_netlist = (dvd_netlist_t *)p_method_data;
    
    /* check that we have enough free iovec */
    if( (
     (p_netlist->i_iovec_end - p_netlist->i_iovec_start)
        & p_netlist->i_nb_iovec ) < p_netlist->i_read_once )
    {
        intf_WarnMsg( 12, "input info: waiting for free iovec" );
        msleep( INPUT_IDLE_SLEEP );

        while( (
         (p_netlist->i_iovec_end - p_netlist->i_iovec_start)
            & p_netlist->i_nb_iovec ) < p_netlist->i_read_once )
        {
            msleep( INPUT_IDLE_SLEEP );
        }

        intf_WarnMsg( 12, "input info: found free iovec" );
    }

    if( (
     (p_netlist->i_data_end - p_netlist->i_data_start)
        & p_netlist->i_nb_data ) < p_netlist->i_read_once )
    {
        intf_WarnMsg( 12, "input info: waiting for free data packet" );
        msleep( INPUT_IDLE_SLEEP );

        while( (
         (p_netlist->i_data_end - p_netlist->i_data_start)
            & p_netlist->i_nb_data ) < p_netlist->i_read_once )
        {
            msleep( INPUT_IDLE_SLEEP );
        }

        intf_WarnMsg( 12, "input info: found free data packet" );
    }

    /* readv only takes contiguous buffers 
     * so, as a solution, we chose to have a FIFO a bit longer
     * than i_nb_data, and copy the begining of the FIFO to its end
     * if the readv needs to go after the end */
    if( p_netlist->i_nb_iovec - p_netlist->i_iovec_start + 1 <
                                                    p_netlist->i_read_once )
    {
        memcpy( &p_netlist->p_free_iovec[p_netlist->i_nb_iovec + 1], 
                p_netlist->p_free_iovec, 
                (p_netlist->i_read_once -
                    (p_netlist->i_nb_iovec + 1 - p_netlist->i_iovec_start))
                    * sizeof(struct iovec)
              );

    }

    return p_netlist->p_free_iovec + p_netlist->i_iovec_start;

}

/*****************************************************************************
 * DVDMviovec: move the iovec pointer by one after a readv() operation and
 * gives a data_packet corresponding to iovec in p_data
 *****************************************************************************/
void DVDMviovec( void * p_method_data, int i_nb_iovec,
                 struct data_packet_s ** pp_data )
{
    dvd_netlist_t *     p_netlist;
    unsigned int        i_loop = 0;

    /* cast */
    p_netlist = (dvd_netlist_t *)p_method_data;
    
    /* lock */
    vlc_mutex_lock( &p_netlist->lock );

    /* Fills a table of pointers to packets associated with the io_vec's */
    while( i_loop < i_nb_iovec )
    {
        pp_data[i_loop] = p_netlist->pp_free_data[p_netlist->i_data_start];
        
        pp_data[i_loop]->p_buffer =
                    p_netlist->p_free_iovec[p_netlist->i_iovec_start].iov_base;
        
        pp_data[i_loop]->pi_refcount = p_netlist->pi_refcount +
                                       p_netlist->i_iovec_start;

        p_netlist->i_iovec_start ++;
        p_netlist->i_iovec_start &= p_netlist->i_nb_iovec;

        p_netlist->i_data_start ++;
        p_netlist->i_data_start &= p_netlist->i_nb_data;

        i_loop ++;
    }

    /* unlock */
    vlc_mutex_unlock( &p_netlist->lock );
    
}

/*****************************************************************************
 * DVDNewPtr: returns a free data_packet_t
 * Gives a pointer ; its fields need to be initialized
 *****************************************************************************/
struct data_packet_s * DVDNewPtr( void * p_method_data )
{    
    dvd_netlist_t *         p_netlist; 
    struct data_packet_s *  p_return;
    
    /* cast */
    p_netlist = (dvd_netlist_t *)p_method_data; 

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
    p_netlist->i_data_start &= p_netlist->i_nb_data;

    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);

    return ( p_return );
}

/*****************************************************************************
 * DVDNewPacket: returns a free data_packet_t, and takes a corresponding
 * storage iovec
 *****************************************************************************/
struct data_packet_s * DVDNewPacket( void * p_method_data,
                                     size_t i_buffer_size )
{
    dvd_netlist_t *         p_netlist;
    struct data_packet_s *  p_packet;
//intf_ErrMsg( "netlist: New packet" );
    /* cast */
    p_netlist = (dvd_netlist_t *)p_method_data;
    
    /* lock */
    vlc_mutex_lock( &p_netlist->lock );

     /* check */
    if ( p_netlist->i_iovec_start == p_netlist->i_iovec_end )
    {
        intf_ErrMsg("Empty io_vec FIFO in netlist. Unable to allocate memory");
        return ( NULL );
    }

    if ( p_netlist->i_data_start == p_netlist->i_data_end )
    {
        intf_ErrMsg("Empty Data FIFO in netlist. Unable to allocate memory");
        return ( NULL );
    }


    /* Gives an io_vec and associated data */
    p_packet = p_netlist->pp_free_data[p_netlist->i_data_start];
        
    p_packet->p_buffer =
              p_netlist->p_free_iovec[p_netlist->i_iovec_start].iov_base;
        
    p_packet->p_payload_start = p_packet->p_buffer;
        
    p_packet->p_payload_end =
              p_packet->p_buffer + i_buffer_size;

    p_packet->p_next = NULL;
    p_packet->b_discard_payload = 0;

    p_packet->pi_refcount = p_netlist->pi_refcount + p_netlist->i_iovec_start;
    (*p_packet->pi_refcount)++;

    p_netlist->i_iovec_start ++;
    p_netlist->i_iovec_start &= p_netlist->i_nb_iovec;

    p_netlist->i_data_start ++;
    p_netlist->i_data_start &= p_netlist->i_nb_data;

    /* unlock */
    vlc_mutex_unlock( &p_netlist->lock );

    return p_packet;
}

/*****************************************************************************
 * DVDNewPES: returns a free pes_packet_t
 *****************************************************************************/
struct pes_packet_s * DVDNewPES( void * p_method_data )
{
    dvd_netlist_t *     p_netlist;
    pes_packet_t *      p_return;
    
//intf_ErrMsg( "netlist: New pes" );
    /* cast */ 
    p_netlist = (dvd_netlist_t *)p_method_data;
    
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
    p_netlist->i_pes_start &= p_netlist->i_nb_pes; 
   
    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);
    
    /* initialize PES */
    p_return->b_data_alignment = 0;
    p_return->b_discontinuity = 0; 
    p_return->i_pts = 0;
    p_return->i_dts = 0;
    p_return->i_pes_size = 0;
    p_return->p_first = NULL;

    return ( p_return );
}

/*****************************************************************************
 * DVDDeletePacket: puts a data_packet_t back into the netlist
 *****************************************************************************/
void DVDDeletePacket( void * p_method_data, data_packet_t * p_data )
{
    dvd_netlist_t * p_netlist;
    
    /* cast */
    p_netlist = (dvd_netlist_t *) p_method_data;

    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );


   /* Delete data_packet */
    p_netlist->i_data_end ++;
    p_netlist->i_data_end &= p_netlist->i_nb_data;
    
    p_netlist->pp_free_data[p_netlist->i_data_end] = p_data;

    p_data->p_next = NULL;
    p_data->b_discard_payload = 0;

    /* Update reference counter */
    (*p_data->pi_refcount)--;

    if( (*p_data->pi_refcount) == 0 )
    {

        p_netlist->i_iovec_end++;
        p_netlist->i_iovec_end &= p_netlist->i_nb_iovec;
        p_netlist->p_free_iovec[p_netlist->i_iovec_end].iov_base =
                                                            p_data->p_buffer;
    }
 
    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);
}

/*****************************************************************************
 * DVDDeletePES: puts a pes_packet_t back into the netlist
 *****************************************************************************/
void DVDDeletePES( void * p_method_data, pes_packet_t * p_pes )
{
    dvd_netlist_t *     p_netlist; 
    data_packet_t *     p_current_packet;
    data_packet_t *     p_next_packet;
    
    /* cast */
    p_netlist = (dvd_netlist_t *)p_method_data;

    /* lock */
    vlc_mutex_lock ( &p_netlist->lock );

    /* delete free  p_pes->p_first, p_next ... */
    p_current_packet = p_pes->p_first;
    while ( p_current_packet != NULL )
    {
        /* copy of NetListDeletePacket, duplicate code avoid many locks */

        p_netlist->i_data_end ++;
        p_netlist->i_data_end &= p_netlist->i_nb_data;

        /* re initialize */
        p_current_packet->p_payload_start = p_current_packet->p_buffer;
        
        p_netlist->pp_free_data[p_netlist->i_data_end] = p_current_packet;

        /* Update reference counter */
        (*p_current_packet->pi_refcount)--;

        if( (*p_current_packet->pi_refcount) <= 0 )
        {
            p_netlist->i_iovec_end++;
            p_netlist->i_iovec_end &= p_netlist->i_nb_iovec;
            p_netlist->p_free_iovec[p_netlist->i_iovec_end].iov_base =
                    p_current_packet->p_buffer;
        }
    
        p_next_packet = p_current_packet->p_next;
        p_current_packet->p_next = NULL;
        p_current_packet->b_discard_payload = 0;
        p_current_packet = p_next_packet;
    }
 
    /* delete our current PES packet */
    p_netlist->i_pes_end ++;
    p_netlist->i_pes_end &= p_netlist->i_nb_pes;
    p_netlist->pp_free_pes[p_netlist->i_pes_end] = p_pes;
    
    /* unlock */
    vlc_mutex_unlock (&p_netlist->lock);

}

/*****************************************************************************
 * DVDNetlistEnd: frees all allocated structures
 *****************************************************************************/
void DVDNetlistEnd( dvd_netlist_t * p_netlist )
{
    /* destroy the mutex lock */
    vlc_mutex_destroy( &p_netlist->lock );
    
    /* free the FIFO, the buffer, and the netlist structure */
    free( p_netlist->pp_free_data );
    free( p_netlist->pp_free_pes );
    free( p_netlist->pi_refcount );
    free( p_netlist->p_pes );
    free( p_netlist->p_data );
    free( p_netlist->p_buffers );

    /* free the netlist */
    free( p_netlist );
}
