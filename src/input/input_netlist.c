/*****************************************************************************
 * input_netlist.c: netlist management
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
#include "stream_control.h"
#include "input_ext-intf.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/*****************************************************************************
 * input_NetlistInit: allocates netlist buffers and init indexes
 *****************************************************************************/
int input_NetlistInit( input_thread_t * p_input, int i_nb_data, int i_nb_pes,
                       size_t i_buffer_size )
{
    /* p_input->p_method_data = malloc(sizeof(netlist_t)) */
    /* p_buffers = malloc(i_buffer_size*i_nb_data) */
    /* p_data = malloc(...*(i_nb_data + INPUT_READ_ONCE)) */
    /* p_pes = malloc(...*(i_nb_pes + INPUT_READ_ONCE)) */
    /* vlc_mutex_init */

    /* i_*_start = 0, i_*_end = i_nb_* */
    /* tous les paquets dans les netlists */
}

/*****************************************************************************
 * input_NetlistGetiovec: returns an iovec pointer for a readv() operation
 *****************************************************************************/
struct iovec * input_NetlistGetiovec( void * p_netlist )
{
    /* fonction la plus difficile, terminer par celle-la */
}

/*****************************************************************************
 * input_NetlistNewPacket: returns a free data_packet_t
 *****************************************************************************/
struct data_packet_s * input_NetlistNewPacket( void * p_netlist )
{
    /* cast p_netlist -> netlist_t */
    /* lock */
    /* return pp_free_data[i_data_start], i_data_start++ */
}

/*****************************************************************************
 * input_NetlistNewPES: returns a free pes_packet_t
 *****************************************************************************/
struct pes_packet_s * input_NetlistNewPES( void * p_netlist )
{
    /* pareil */
}

/*****************************************************************************
 * input_NetlistDeletePacket: puts a data_packet_t back into the netlist
 *****************************************************************************/
void input_NetlistDeletePacket( void * p_netlist, data_packet_t * p_data )
{
    /* pp_free_data[i_data_end] = p_data, i_data_end++ */
}

/*****************************************************************************
 * input_NetlistDeletePES: puts a pes_packet_t back into the netlist
 *****************************************************************************/
void input_NetlistDeletePES( void * p_netlist, pes_packet_s * p_pes )
{
    /* idem, plus detruire tous les data_packet_t dans p_pes->p_first,
     * p_pes->p_first->p_next, etc. */
}

/*****************************************************************************
 * input_NetlistEnd: frees all allocated structures
 *****************************************************************************/
void input_NetlistEnd( input_thread_t * )
{

}

