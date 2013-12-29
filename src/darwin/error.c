/*****************************************************************************
 * error.c: Darwin error messages handling
 *****************************************************************************
 * Copyright © 2006-2013 Rémi Denis-Courmont
 *           © 2013 Felix Paul Kühne
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

#include <stdlib.h>
#include <errno.h>

#include <vlc_common.h>

const char *vlc_strerror_c(int errnum)
{
    /* We cannot simply use strerror() here, since it is not thread-safe. */
    if ((unsigned)errnum < (unsigned)sys_nerr)
        return sys_errlist[errnum];

    return _("Unknown error");
}

const char *vlc_strerror(int errnum)
{
    return vlc_strerror_c(errnum);
}
