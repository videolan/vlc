/*****************************************************************************
 * clock_internal.h: Clock internal functions
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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

#include <vlc_common.h>

/*****************************************************************************
 * Structures
 *****************************************************************************/

 /**
 * This structure holds long term average
 */
typedef struct
{
    mtime_t i_value;
    int     i_residue;

    int     i_count;
    int     i_divider;
} average_t;

void    AvgInit( average_t *, int i_divider );
void    AvgClean( average_t * );

void    AvgReset( average_t * );
void    AvgUpdate( average_t *, mtime_t i_value );
mtime_t AvgGet( average_t * );
void    AvgRescale( average_t *, int i_divider );

/* */
typedef struct
{
    mtime_t i_stream;
    mtime_t i_system;
} clock_point_t;

static inline clock_point_t clock_point_Create( mtime_t i_stream, mtime_t i_system )
{
    clock_point_t p = { .i_stream = i_stream, .i_system = i_system };
    return p;
}

