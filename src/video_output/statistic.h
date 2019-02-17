/*****************************************************************************
 * statistic.h : vout statistic
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
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
# define LIBVLC_VOUT_STATISTIC_H
# include <stdatomic.h>

/* NOTE: Both statistics are atomic on their own, so one might be older than
 * the other one. Currently, only one of them is updated at a time, so this
 * is a non-issue. */
typedef struct {
    atomic_uint displayed;
    atomic_uint lost;
} vout_statistic_t;

static inline void vout_statistic_Init(vout_statistic_t *stat)
{
    atomic_init(&stat->displayed, 0);
    atomic_init(&stat->lost, 0);
}

static inline void vout_statistic_Clean(vout_statistic_t *stat)
{
    (void) stat;
}

static inline void vout_statistic_GetReset(vout_statistic_t *stat,
                                           unsigned *restrict displayed,
                                           unsigned *restrict lost)
{
    *displayed = atomic_exchange_explicit(&stat->displayed, 0,
                                          memory_order_relaxed);
    *lost = atomic_exchange_explicit(&stat->lost, 0, memory_order_relaxed);
}

static inline void vout_statistic_AddDisplayed(vout_statistic_t *stat,
                                               int displayed)
{
    atomic_fetch_add_explicit(&stat->displayed, displayed,
                              memory_order_relaxed);
}

static inline void vout_statistic_AddLost(vout_statistic_t *stat, int lost)
{
    atomic_fetch_add_explicit(&stat->lost, lost, memory_order_relaxed);
}

#endif
