/*****************************************************************************
 * aout_common.h: audio output inner functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: aout_common.h,v 1.9 2002/01/15 11:51:11 asmax Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Cyril Deguet <asmax@via.ecp.fr>   
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


/* Biggest difference allowed between scheduled playing date and actual date 
   (in microseconds) */
#define MAX_DELTA 10000


/* Creating as many aout_Thread functions as configurations was one solution,
 * examining the different cases in the Thread loop of an unique function was
 * another. I chose the first solution. */
void aout_U8Thread            ( aout_thread_t * p_aout );
void aout_S8Thread            ( aout_thread_t * p_aout );
void aout_U16Thread           ( aout_thread_t * p_aout );
void aout_S16Thread           ( aout_thread_t * p_aout );
void aout_SpdifThread         ( aout_thread_t * p_aout );
void aout_FillBuffer          ( aout_thread_t * p_aout, aout_fifo_t * p_fifo );


/* Generic main thread function "aout_XXXThread"
 */
#define DECLARE_AOUT_THREAD( format, type, buffer_copy )                      \
void aout_##format##Thread( aout_thread_t * p_aout )                          \
{                                                                             \
                                                                              \
    int i_fifo;                                                               \
    long l_buffer, l_buffer_limit, l_bytes;                                   \
                                                                              \
    /* As the s32_buffer was created with calloc(), we don't have to set this \
     * memory to zero and we can immediately jump into the thread's loop */   \
    while ( ! p_aout->b_die )                                                 \
    {                                                                         \
        vlc_mutex_lock( &p_aout->fifos_lock );                                \
                                                                              \
        for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )                 \
        {                                                                     \
            if( p_aout->fifo[i_fifo].b_die )                                  \
            {                                                                 \
                aout_FreeFifo( &p_aout->fifo[i_fifo] );                       \
            }                                                                 \
            else                                                              \
            {                                                                 \
                aout_FillBuffer( p_aout, &p_aout->fifo[i_fifo] );             \
            }                                                                 \
        }                                                                     \
                                                                              \
        vlc_mutex_unlock( &p_aout->fifos_lock );                              \
                                                                              \
        l_buffer_limit = p_aout->l_units << p_aout->b_stereo;                 \
                                                                              \
        for ( l_buffer = 0; l_buffer < l_buffer_limit; l_buffer++ )           \
        {                                                                     \
            ((type *)p_aout->buffer)[l_buffer] = (type)( buffer_copy *        \
                    p_aout->i_volume / 256 );                                 \
             p_aout->s32_buffer[l_buffer] = 0;                                \
        }                                                                     \
                                                                              \
        l_bytes = p_aout->pf_getbufinfo( p_aout, l_buffer_limit );            \
                                                                              \
        p_aout->date = mdate() + ((((mtime_t)((l_bytes + 4 *                  \
                p_aout->i_latency) /                                          \
                (sizeof(type) << p_aout->b_stereo))) * 1000000) /             \
                ((mtime_t)p_aout->l_rate)) + p_main->i_desync;                \
                                                                              \
        p_aout->pf_play( p_aout, (byte_t *)p_aout->buffer, l_buffer_limit *   \
                sizeof(type) );                                               \
                                                                              \
        if ( l_bytes > (l_buffer_limit * sizeof(type)) )                      \
        {                                                                     \
            msleep( p_aout->l_msleep );                                       \
        }                                                                     \
    }                                                                         \
                                                                              \
    vlc_mutex_lock( &p_aout->fifos_lock );                                    \
                                                                              \
    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )                     \
    {                                                                         \
        aout_FreeFifo( &p_aout->fifo[i_fifo] );                               \
    }                                                                         \
                                                                              \
    vlc_mutex_unlock( &p_aout->fifos_lock );                                  \
                                                                              \
}                                                                             \


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
 * UpdateIncrement
 *****************************************************************************/
static __inline__ void UpdateIncrement( aout_increment_t * p_increment, 
        long * p_integer )
{
    if( (p_increment->l_remainder += p_increment->l_euclidean_remainder) >= 0 )
    {
        *p_integer += p_increment->l_euclidean_integer + 1;
        p_increment->l_remainder -= p_increment->l_euclidean_denominator;
    }
    else
    {
        *p_integer += p_increment->l_euclidean_integer;
    }
}

