/*****************************************************************************
 * error.c: Android error messages formatting
 *****************************************************************************
 * Copyright © 2014 Edward Wang
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

#include <string.h>

#include <vlc_common.h>

const char* vlc_strerror(int errnum)
{
    return vlc_strerror_c(errnum);
}

const char* vlc_strerror_c(int errnum)
{
    static __thread char android_buf[100];
    strerror_r(errnum, android_buf, 100);
    return android_buf;
}
