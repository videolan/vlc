/*****************************************************************************
 * statistic.h : vout statistic
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef LIBVLC_VOUT_STATISTIC_H
#define LIBVLC_VOUT_STATISTIC_H

typedef struct {
    vlc_spinlock_t spin;

    int displayed;
    int lost;
} vout_statistic_t;

static inline void vout_statistic_Init(vout_statistic_t *stat)
{
    vlc_spin_init(&stat->spin);
}
static inline void vout_statistic_Clean(vout_statistic_t *stat)
{
    vlc_spin_destroy(&stat->spin);
}
static inline void vout_statistic_GetReset(vout_statistic_t *stat, int *displayed, int *lost)
{
    vlc_spin_lock(&stat->spin);
    *displayed = stat->displayed;
    *lost      = stat->lost;

    stat->displayed = 0;
    stat->lost      = 0;
    vlc_spin_unlock(&stat->spin);
}
static inline void vout_statistic_Update(vout_statistic_t *stat, int displayed, int lost)
{
    vlc_spin_lock(&stat->spin);
    stat->displayed += displayed;
    stat->lost      += lost;
    vlc_spin_unlock(&stat->spin);
}

#endif
