/*****************************************************************************
 * input_ts.h: structures of the input not exported to other modules
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_ts.h,v 1.10 2001/07/12 23:06:54 gbazin Exp $
 *
 * Authors: Henri Fallon <henri@via.ecp.fr>
 *          Boris Dorès <babal@via.ecp.fr>
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

#define NB_DATA 16384 
#define NB_PES  8192

#define BUFFER_SIZE (7 * TS_PACKET_SIZE)

/*****************************************************************************
 * thread_ts_data_t: private input data
 *****************************************************************************/
typedef struct thread_ts_data_s
{ 
    /* The file descriptor we select() on */
    fd_set fds;
    
#if defined( WIN32 )
    char p_buffer[ BUFFER_SIZE ];      /* temporary buffer for readv_network */
    int  i_length;                               /* length of the UDP packet */
    int  i_offset;           /* number of bytes already read from the buffer */
#endif
    
} thread_ts_data_t;

/*****************************************************************************
 * network readv() replacement for iovec-impaired C libraries
 *****************************************************************************/
#if defined(WIN32)
static __inline__ int read_network( int i_fd, char * p_base,
                                    thread_ts_data_t *p_sys, int i_len )
{
    int i_bytes;

    if( p_sys->i_offset >= p_sys->i_length )
    {
        p_sys->i_length = recv( i_fd, p_sys->p_buffer, BUFFER_SIZE, 0 );
        if ( p_sys->i_length == SOCKET_ERROR )
        {
            return -1;
        }
        p_sys->i_offset = 0;
    }

    if( i_len <= p_sys->i_length - p_sys->i_offset )
    {
         i_bytes = i_len;
    }
    else
    {
         i_bytes = p_sys->i_length - p_sys->i_offset;
    }

    memcpy( p_base, p_sys->p_buffer + p_sys->i_offset, i_bytes );
    p_sys->i_offset += i_bytes;

    return i_bytes;
}

static __inline__ int readv_network( int i_fd, struct iovec *p_iovec,
                                     int i_count, thread_ts_data_t *p_sys )
{
    int i_index, i_len, i_total = 0;
    u8 *p_base;

    for( i_index = i_count; i_index; i_index-- )
    {
        register signed int i_bytes;

        i_len  = p_iovec->iov_len;
        p_base = p_iovec->iov_base;

        /* Loop is unrolled one time to spare the (i_bytes <= 0) test */
        if( i_len > 0 )
        {
            i_bytes = read_network( i_fd, p_base, p_sys, i_len );

            if( ( i_total == 0 ) && ( i_bytes < 0 ) )
            {
                return -1;
            }

            if( i_bytes <= 0 )
            {
                return i_total;
            }

            i_len -= i_bytes; i_total += i_bytes; p_base += i_bytes;

            while( i_len > 0 )
            {
                i_bytes = read_network( i_fd, p_base, p_sys, i_len );

                if( i_bytes <= 0 )
                {
                    return i_total;
                }

                i_len -= i_bytes; i_total += i_bytes; p_base += i_bytes;
            }
        }

        p_iovec++;
    }

    return i_total;
}
#endif

