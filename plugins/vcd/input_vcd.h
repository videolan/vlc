/*****************************************************************************
 * input_vcd.h: thread structure of the VCD plugin
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: input_vcd.h,v 1.1 2001/10/23 03:17:49 jobi Exp $
 *
 * Author: Johan Bilien <jobi@via.ecp.fr>
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




/*****************************************************************************
 * thread_vcd_data_t: VCD information
 *****************************************************************************/
typedef struct thread_vcd_data_s
{
    int                     vcdhandle;          // File descriptor
    int                     nb_tracks;          // Nb of tracks (titles)
    int                     current_track;      // Current track
    int                     current_sector;     // Current Sector
    int *                   tracks_sector;      // index of tracks
    boolean_t               b_end_of_track;     // if the end of track is 
                                                // reached
} thread_vcd_data_t ;


