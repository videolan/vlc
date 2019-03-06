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
 * This structure holds long term moving average
 */
typedef struct
{
    double value; /* The average value */
    int count; /* The number of sample evaluated */
    int range; /* The maximum range of sample on which we calculate the average*/
} average_t;

void AvgInit(average_t *, int range);
void AvgClean(average_t *);

void AvgReset(average_t *);

/*  calculates (previous_average * (range - 1) + new_value)/range */
void AvgUpdate(average_t *, double value);

double AvgGet(average_t *);
void AvgRescale(average_t *, int range);

/* */
typedef struct
{
    vlc_tick_t system;
    vlc_tick_t stream;
} clock_point_t;

static inline clock_point_t clock_point_Create(vlc_tick_t system, vlc_tick_t stream)
{
    return (clock_point_t) { .system = system, .stream = stream };
}

