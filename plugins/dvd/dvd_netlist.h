/*****************************************************************************
 * dvd_netlist.h: Specific netlist structures for DVD packets
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000, 2001 VideoLAN
 * $Id: dvd_netlist.h,v 1.2 2001/03/03 07:07:01 stef Exp $
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
 * netlist_t: structure to manage a netlist
 *****************************************************************************/
typedef struct dvd_netlist_s
{
    vlc_mutex_t             lock;

    size_t                  i_buffer_size;

    /* Buffers */
    byte_t *                p_buffers;                 /* Big malloc'ed area */
    data_packet_t *         p_data;                        /* malloc'ed area */
    pes_packet_t *          p_pes;                         /* malloc'ed area */

    /* FIFOs of free packets */
    data_packet_t **        pp_free_data;
    pes_packet_t **         pp_free_pes;
    struct iovec *          p_free_iovec;
    
    /* FIFO size */
    unsigned int            i_nb_iovec;
    unsigned int            i_nb_pes;
    unsigned int            i_nb_data;

    /* Index */
    unsigned int            i_iovec_start, i_iovec_end;
    unsigned int            i_data_start, i_data_end;
    unsigned int            i_pes_start, i_pes_end;

    /* Reference counters for iovec */
    unsigned int *          pi_refcount;

    /* Nb of packets read once */
    unsigned int            i_read_once;

} dvd_netlist_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
struct dvd_netlist_s *  DVDNetlistInit( int , int, int, size_t, int );
struct iovec *          DVDGetiovec( void * p_method_data );
void                    DVDMviovec( void * , int, struct data_packet_s **);
struct data_packet_s *  DVDNewPtr( void * );
struct data_packet_s *  DVDNewPacket( void *, size_t );
struct pes_packet_s *   DVDNewPES( void * );
void                    DVDDeletePacket( void *, struct data_packet_s * );
void                    DVDDeletePES( void *, struct pes_packet_s * );
void                    DVDNetlistEnd( struct dvd_netlist_s * );
