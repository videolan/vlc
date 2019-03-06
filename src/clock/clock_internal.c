/*****************************************************************************
 * clock_internal.c: Clock internal functions
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "clock_internal.h"

/*****************************************************************************
 * Long term average helpers
 *****************************************************************************/
void AvgInit(average_t *avg, int range)
{
    avg->range = range;
    AvgReset(avg);
}

void AvgClean(average_t * avg)
{
    VLC_UNUSED(avg);
}

void AvgReset(average_t *avg)
{
    avg->value = 0.0f;
    avg->count = 0;
}

void AvgUpdate(average_t *avg, double value)
{
    const int new_value_weight = 1;
    int average_weight;
    int divider;
    if (avg->count < avg->range)
    {
        average_weight = avg->count++;
        divider = avg->count;
    }
    else
    {
        average_weight = avg->range - 1;
        divider = avg->range;
    }

    const double tmp = average_weight * avg->value + new_value_weight * value;
    avg->value = tmp / divider;
}

double AvgGet(average_t *avg)
{
    return avg->value;
}

void AvgRescale(average_t *avg, int range)
{
    const double tmp = avg->value * avg->range;

    avg->range = range;
    avg->value = tmp / avg->range;
}
