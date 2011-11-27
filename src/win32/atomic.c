/*****************************************************************************
 * atomic.c:
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_atomic.h>

#include <windows.h>

uintptr_t vlc_atomic_get (const vlc_atomic_t *atom)
{
    return atom->u;
}

uintptr_t vlc_atomic_set (vlc_atomic_t *atom, uintptr_t v)
{
#if defined (WIN64)
    InterlockedExchange64 (&atom->u, v);
#else
    InterlockedExchange (&atom->u, v);
#endif
    return v;
}

uintptr_t vlc_atomic_add (vlc_atomic_t *atom, uintptr_t v)
{
#if defined (WIN64)
    return InterlockedExchangeAdd64 (&atom->s, v) + v;
#else
    return InterlockedExchangeAdd (&atom->s, v) + v;
#endif
}

uintptr_t vlc_atomic_swap (vlc_atomic_t *atom, uintptr_t v)
{
#if defined (WIN64)
    return InterlockedExchange64 (&atom->s, v);
#else
    return InterlockedExchange (&atom->s, v);
#endif
}

uintptr_t vlc_atomic_compare_swap (vlc_atomic_t *atom,
                                   uintptr_t oldval, uintptr_t newval)
{
#if defined (WIN64)
    return InterlockedCompareExchange64 (&atom->s, newval, oldval);
#else
    return InterlockedCompareExchange (&atom->s, newval, oldval);
#endif
}
