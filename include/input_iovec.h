/*****************************************************************************
 * input_iovec.h: iovec structure and readv() replacement
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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
 * iovec structure: vectored data entry
 *****************************************************************************/
struct iovec
{
    void *iov_base;     /* Pointer to data. */
    size_t iov_len;     /* Length of data.  */
};

/*****************************************************************************
 * readv: readv() replacement for iovec-impaired C libraries
 *****************************************************************************/
static __inline int readv( int i_fd, struct iovec *p_iovec, int i_count )
{
    int i_index, i_len, i_total = 0;
    char *p_base;

    for( i_index = i_count; i_index; i_index-- )
    {
        register signed int i_bytes;

        i_len  = p_iovec->iov_len;
        p_base = p_iovec->iov_base;

        /* Loop is unrolled one time to spare the (i_bytes < 0) test */
        if( i_len > 0 )
        {
            i_bytes = read( i_fd, p_base, i_len );

            if( ( i_total == 0 ) && ( i_bytes < 0 ) )
            {
                return -1;
            }

            if( i_bytes <= 0 )
            {
                return i_total;
            }

            i_len   -= i_bytes;
            i_total += i_bytes;
            p_base  += i_bytes;

            while( i_len > 0 )
            {
                i_bytes = read( i_fd, p_base, i_len );

                if( i_bytes <= 0 )
                {
                    return i_total;
                }

                i_len   -= i_bytes;
                i_total += i_bytes;
                p_base  += i_bytes;
            }
        }

        p_iovec++;
    }

    return i_total;
}

