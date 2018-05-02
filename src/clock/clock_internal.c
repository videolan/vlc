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
void AvgInit( average_t *p_avg, int i_divider )
{
    p_avg->i_divider = i_divider;
    AvgReset( p_avg );
}

void AvgClean( average_t *p_avg )
{
    VLC_UNUSED(p_avg);
}

void AvgReset( average_t *p_avg )
{
    p_avg->i_value = 0;
    p_avg->i_residue = 0;
    p_avg->i_count = 0;
}

void AvgUpdate( average_t *p_avg, mtime_t i_value )
{
    const int i_f0 = __MIN( p_avg->i_divider - 1, p_avg->i_count );
    const int i_f1 = p_avg->i_divider - i_f0;

    const mtime_t i_tmp = i_f0 * p_avg->i_value + i_f1 * i_value + p_avg->i_residue;

    p_avg->i_value   = i_tmp / p_avg->i_divider;
    p_avg->i_residue = i_tmp % p_avg->i_divider;

    p_avg->i_count++;
}

mtime_t AvgGet( average_t *p_avg )
{
    return p_avg->i_value;
}

void AvgRescale( average_t *p_avg, int i_divider )
{
    const mtime_t i_tmp = p_avg->i_value * p_avg->i_divider + p_avg->i_residue;

    p_avg->i_divider = i_divider;
    p_avg->i_value   = i_tmp / p_avg->i_divider;
    p_avg->i_residue = i_tmp % p_avg->i_divider;
}
