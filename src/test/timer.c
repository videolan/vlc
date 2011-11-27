/*****************************************************************************
 * timer.c: Test for timer API
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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

#include <vlc_common.h>

#include <stdio.h>
#include <stdlib.h>
#undef NDEBUG
#include <assert.h>

struct timer_data
{
    vlc_timer_t timer;
    vlc_mutex_t lock;
    unsigned count;
};

static void callback (void *ptr)
{
    struct timer_data *data = ptr;

    vlc_mutex_lock (&data->lock);
    data->count += 1 + vlc_timer_getoverrun (data->timer);
    vlc_mutex_unlock (&data->lock);
}


int main (void)
{
    struct timer_data data;
    int val;

    vlc_mutex_init (&data.lock);
    data.count = 0;

    val = vlc_timer_create (&data.timer, callback, &data);
    assert (val == 0);

    /* Relative timer */
    vlc_timer_schedule (data.timer, false, 1, CLOCK_FREQ / 10);
    msleep (CLOCK_FREQ);
    vlc_mutex_lock (&data.lock);
    data.count += vlc_timer_getoverrun (data.timer);
    printf ("Count = %u\n", data.count);
    assert (data.count >= 10);
    data.count = 0;
    vlc_mutex_unlock (&data.lock);
    vlc_timer_schedule (data.timer, false, 0, 0);

    /* Absolute timer */
    mtime_t now = mdate ();

    vlc_timer_schedule (data.timer, true, now, CLOCK_FREQ / 10);
    msleep (CLOCK_FREQ);
    vlc_mutex_lock (&data.lock);
    data.count += vlc_timer_getoverrun (data.timer);
    printf ("Count = %u\n", data.count);
    assert (data.count >= 10);
    vlc_mutex_unlock (&data.lock);

    vlc_timer_destroy (data.timer);
    vlc_mutex_destroy (&data.lock);

    return 0;
}
