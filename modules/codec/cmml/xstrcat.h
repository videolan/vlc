/*****************************************************************************
 * xstrcat.h: strcat with realloc
 *****************************************************************************
 * Copyright (C) 2004 Commonwealth Scientific and Industrial Research
 *                    Organisation (CSIRO) Australia
 * Copyright (C) 2004 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Andre Pang <Andre.Pang@csiro.au>
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

#ifndef __XSTRCAT_H__
#define __XSTRCAT_H__
# include <string.h>
# include <stdlib.h>

/* like strcat, but realloc's enough memory for the new string too */

static inline
char *xstrcat( char *psz_string, const char *psz_to_append )
{
    size_t i_new_string_length = strlen( psz_string ) +
        strlen( psz_to_append ) + 1;

    psz_string = (char *) realloc( psz_string, i_new_string_length );

    return strcat( psz_string, psz_to_append );
}

#endif /* __XSTRCAT_H__ */

