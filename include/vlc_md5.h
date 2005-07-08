/*****************************************************************************
 * vlc_md5.h: MD5 hash
 *****************************************************************************
 * Copyright (C) 2004-2005 VideoLAN (Centrale Réseaux) and its contributors
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _VLC_MD5_H
# define _VLC_MD5_H

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
VLC_EXPORT(void, AddMD5, ( struct md5_s *, const uint8_t *, uint32_t ) );
VLC_EXPORT(void, EndMD5, ( struct md5_s * ) );
VLC_EXPORT(void, DigestMD5, ( struct md5_s *, uint32_t * ) );

#endif
