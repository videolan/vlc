/*****************************************************************************
 * stream_control.h: structures of the input exported everywhere
 * This header provides a structure so that everybody knows the state
 * of the reading.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: stream_control.h,v 1.7 2002/01/07 02:12:29 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

/* Structures exported to interface, input and decoders */

/*****************************************************************************
 * stream_ctrl_t
 *****************************************************************************
 * Describe the state of a program stream.
 *****************************************************************************/
typedef struct stream_ctrl_s
{
    vlc_mutex_t             control_lock;

    int                     i_status;
    /* if i_status == FORWARD_S or BACKWARD_S */
    int                     i_rate;

    boolean_t               b_mute;
    boolean_t               b_grayscale;           /* use color or grayscale */
    int                     i_smp; /* number of symmetrical threads to launch
                                    * to decode the video | 0 == disabled    */
} stream_ctrl_t;

/* Possible status : */
#define UNDEF_S             0
#define PLAYING_S           1
#define PAUSE_S             2
#define FORWARD_S           3
#define BACKWARD_S          4
#define REWIND_S            5                /* Not supported for the moment */
#define NOT_STARTED_S       10
#define START_S             11

#define DEFAULT_RATE        1000
#define MINIMAL_RATE        31              /* Up to 32/1 */
#define MAXIMAL_RATE        8000            /* Up to 1/8 */
