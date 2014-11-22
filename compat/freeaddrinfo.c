/*****************************************************************************
 * freeaddrinfo.c: freeaddrinfo() replacement functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * Copyright (C) 2002-2007 Rémi Denis-Courmont
 * Copyright (C) 2011-2014 KO Myung-Hun
 *
 * Authors: KO Myung-Hun <komh@chollian.net>
 *          Rémi Denis-Courmont <rem # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

/*
 * This function must be used to free the memory allocated by getaddrinfo().
 */
void freeaddrinfo (struct addrinfo *res)
{
    while (res != NULL)
    {
        struct addrinfo *next = res->ai_next;

        free (res->ai_canonname);
        free (res->ai_addr);
        free (res);
        res = next;
    }
}
