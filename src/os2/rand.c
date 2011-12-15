/*****************************************************************************
 * rand.c : non-predictible random bytes generator
 *****************************************************************************
 * Copyright (C) 2011 KO Myung-Hun <komh@chollian.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_rand.h>

void vlc_rand_bytes (void *buf, size_t len)
{
    QWORD qwTime;
    uint8_t *p_buf = (uint8_t *)buf;

    while (len > 0)
    {
        DosTmrQueryTime( &qwTime );

        *p_buf++ = ( uint8_t )( qwTime.ulLo * rand());
        len--;
    }
}
