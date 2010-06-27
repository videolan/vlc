/*****************************************************************************
 * atomic.c:
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_atomic.h>

#if defined (__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
/* GCC intrinsics */

uintptr_t vlc_atomic_get (const vlc_atomic_t *atom)
{
    __sync_synchronize ();
    return atom->u;
}

uintptr_t vlc_atomic_set (vlc_atomic_t *atom, uintptr_t v)
{
    atom->u = v;
    __sync_synchronize ();
    return v;
}

uintptr_t vlc_atomic_add (vlc_atomic_t *atom, uintptr_t v)
{
    return __sync_add_and_fetch (&atom->u, v);
}

#else
/* Worst-case fallback implementation with a mutex */

static vlc_mutex_t lock = VLC_STATIC_MUTEX;

uintptr_t vlc_atomic_get (const vlc_atomic_t *atom)
{
    uintptr_t v;

    vlc_mutex_lock (&lock);
    v = atom->u;
    vlc_mutex_unlock (&lock);
    return v;
}

uintptr_t vlc_atomic_set (vlc_atomic_t *atom, uintptr_t v)
{
    vlc_mutex_lock (&lock);
    atom->u = v;
    vlc_mutex_unlock (&lock);
    return v;
}

uintptr_t vlc_atomic_add (vlc_atomic_t *atom, uintptr_t v)
{
    vlc_mutex_lock (&lock);
    atom->u += v;
    v = atom->u;
    vlc_mutex_unlock (&lock);
    return v;
}

#endif
