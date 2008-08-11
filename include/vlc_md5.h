/*****************************************************************************
 * vlc_md5.h: MD5 hash
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Sam Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_MD5_H
# define VLC_MD5_H

/**
 * \file
 * This file defines functions and structures for handling md5 checksums
 */

/*****************************************************************************
 * md5_s: MD5 message structure
 *****************************************************************************
 * This structure stores the static information needed to compute an MD5
 * hash. It has an extra data buffer to allow non-aligned writes.
 *****************************************************************************/
struct md5_s
{
    uint64_t i_bits;      /* Total written bits */
    uint32_t p_digest[4]; /* The MD5 digest */
    uint32_t p_data[16];  /* Buffer to cache non-aligned writes */
};

VLC_EXPORT(void, InitMD5, ( struct md5_s * ) );
VLC_EXPORT(void, AddMD5, ( struct md5_s *, const void *, size_t ) );
VLC_EXPORT(void, EndMD5, ( struct md5_s * ) );

/**
 * Returns a char representation of the md5 hash, as shown by UNIX md5 or
 * md5sum tools.
 */
static inline char * psz_md5_hash( struct md5_s *md5_s )
{
    char *psz = malloc( 33 ); /* md5 string is 32 bytes + NULL character */
    if( !psz ) return NULL;

    int i;
    for ( i = 0; i < 4; i++ )
    {
        sprintf( &psz[8*i], "%02x%02x%02x%02x",
            md5_s->p_digest[i] & 0xff,
            ( md5_s->p_digest[i] >> 8 ) & 0xff,
            ( md5_s->p_digest[i] >> 16 ) & 0xff,
            md5_s->p_digest[i] >> 24
        );
    }
    psz[32] = '\0';

    return psz;
}

#endif
