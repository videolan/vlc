/*****************************************************************************
 * aout_common.h: audio output inner functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: aout_common.h,v 1.2 2001/03/21 13:42:34 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
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

/* Creating as many aout_Thread functions as configurations was one solution,
 * examining the different cases in the Thread loop of an unique function was
 * another. I chose the first solution. */
void aout_U8MonoThread        ( aout_thread_t * p_aout );
void aout_U8StereoThread      ( aout_thread_t * p_aout );
void aout_S8MonoThread        ( aout_thread_t * p_aout );
void aout_S8StereoThread      ( aout_thread_t * p_aout );
void aout_U16MonoThread       ( aout_thread_t * p_aout );
void aout_U16StereoThread     ( aout_thread_t * p_aout );
void aout_S16MonoThread       ( aout_thread_t * p_aout );
void aout_S16StereoThread     ( aout_thread_t * p_aout );

#define UPDATE_INCREMENT( increment, integer ) \
    if ( ((increment).l_remainder += (increment).l_euclidean_remainder) >= 0 )\
    { \
        (integer) += (increment).l_euclidean_integer + 1; \
        (increment).l_remainder -= (increment).l_euclidean_denominator; \
    } \
    else \
    { \
        (integer) += (increment).l_euclidean_integer; \
    }

#define FIFO p_aout->fifo[i_fifo]

/*****************************************************************************
 * InitializeIncrement
 *****************************************************************************/
static __inline__ void InitializeIncrement( aout_increment_t * p_increment,
                                            long l_numerator,
                                            long l_denominator )
{
    p_increment->l_remainder = -l_denominator;

    p_increment->l_euclidean_integer = 0;
    while ( l_numerator >= l_denominator )
    {
        p_increment->l_euclidean_integer++;
        l_numerator -= l_denominator;
    }

    p_increment->l_euclidean_remainder = l_numerator;

    p_increment->l_euclidean_denominator = l_denominator;
}

/*****************************************************************************
 * NextFrame
 *****************************************************************************/
static __inline__ int NextFrame( aout_thread_t * p_aout, aout_fifo_t * p_fifo,
                                 mtime_t aout_date )
{
    long l_units, l_rate;

    /* We take the lock */
    vlc_mutex_lock( &p_fifo->data_lock );

    /* Are we looking for a dated start frame ? */
    if ( !p_fifo->b_start_frame )
    {
        while ( p_fifo->l_start_frame != p_fifo->l_end_frame )
        {
            if ( p_fifo->date[p_fifo->l_start_frame] != LAST_MDATE )
            {
                p_fifo->b_start_frame = 1;
                p_fifo->l_next_frame = (p_fifo->l_start_frame + 1) & AOUT_FIFO_SIZE;
                p_fifo->l_unit = p_fifo->l_start_frame * (p_fifo->l_frame_size >> (p_fifo->b_stereo));
                break;
            }
            p_fifo->l_start_frame = (p_fifo->l_start_frame + 1) & AOUT_FIFO_SIZE;
        }

        if ( p_fifo->l_start_frame == p_fifo->l_end_frame )
        {
            vlc_mutex_unlock( &p_fifo->data_lock );
            return( -1 );
        }
    }

    /* We are looking for the next dated frame */
    /* FIXME : is the output fifo full ?? */
    while ( !p_fifo->b_next_frame )
    {
        while ( p_fifo->l_next_frame != p_fifo->l_end_frame )
        {
            if ( p_fifo->date[p_fifo->l_next_frame] != LAST_MDATE )
            {
                p_fifo->b_next_frame = 1;
                break;
            }
            p_fifo->l_next_frame = (p_fifo->l_next_frame + 1) & AOUT_FIFO_SIZE;
        }

        while ( p_fifo->l_next_frame == p_fifo->l_end_frame )
        {
            vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
            if ( p_fifo->b_die )
            {
                vlc_mutex_unlock( &p_fifo->data_lock );
                return( -1 );
            }
        }
    }

    l_units = ((p_fifo->l_next_frame - p_fifo->l_start_frame) & AOUT_FIFO_SIZE) * (p_fifo->l_frame_size >> (p_fifo->b_stereo));

    l_rate = p_fifo->l_rate + ((aout_date - p_fifo->date[p_fifo->l_start_frame]) / 256);
    intf_DbgMsg( "aout debug: %lli (%li);", aout_date - p_fifo->date[p_fifo->l_start_frame], l_rate );

    InitializeIncrement( &p_fifo->unit_increment, l_rate, p_aout->l_rate );

    p_fifo->l_units = (((l_units - (p_fifo->l_unit -
        (p_fifo->l_start_frame * (p_fifo->l_frame_size >> (p_fifo->b_stereo)))))
        * p_aout->l_rate) / l_rate) + 1;

    /* We release the lock before leaving */
    vlc_mutex_unlock( &p_fifo->data_lock );
    return( 0 );
}

