/*****************************************************************************
 * input_ts.c: TS demux and netlist management
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
#include "time_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  TSProbe     ( struct input_thread_s * );
static void TSRead      ( struct input_thread_s * );
static int  TSInit      ( struct input_thread_s * );
static void TSEnd       ( struct input_thread_s * );
static struct data_packet_s * NewPacket ( struct input_thread_s *,
                                          size_t );
static void DeletePacket( struct input_thread_s *,
                          struct data_packet_s * );
static void DeletePES   ( struct input_thread_s *,
                          struct pes_packet_s * );

/*****************************************************************************
 * TSProbe: verifies that the stream is a TS stream
 *****************************************************************************/
static int TSProbe( input_thread_t * p_input )
{
    /* verify that the first byte is 0x47 */
}

static void TSInit( input_thread_t * p_input )
{
    /* Initialize netlist and TS structures */
}
