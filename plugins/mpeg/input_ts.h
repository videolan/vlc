/*****************************************************************************
 * input_ts.h: structures of the input not exported to other modules
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_ts.h,v 1.5 2001/05/28 04:23:52 sam Exp $
 *
 * Authors: Henri Fallon <henri@via.ecp.fr>
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

/* Will be used whne NetworkOpen is ready */
typedef struct thread_ts_data_s { 
    
    // FILE *                  stream;
    fd_set s_fdset;
    
} thread_ts_data_t;

#ifdef WIN32
static __inline__ int readv( int i_fd, struct iovec *p_iovec, int i_count )
{
    int i_index, i_len, i_total = 0;
    char *p_base;

    for( i_index = i_count; i_index; i_index-- )
    {
        i_len  = p_iovec->iov_len;
        p_base = p_iovec->iov_base;

        while( i_len > 0 )
        {
            register signed int i_bytes;
            i_bytes = read( i_fd, p_base, i_len );

            if( i_total == 0 )
            {
                if( i_bytes < 0 )
                {
                    intf_ErrMsg( "input error: read failed on socket" );
                    return -1;
                }
            }

            if( i_bytes <= 0 )
            {
                return i_total;
            }

            i_len   -= i_bytes;
            i_total += i_bytes;
            p_base  += i_bytes;
        }

        p_iovec++;
    }

    return i_total;
}
#endif

