/*****************************************************************************
 * vlc_atomic.h:
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

#ifdef __cplusplus
# error Not implemented in C++.
#endif

#ifndef VLC_ATOMIC_H
# define VLC_ATOMIC_H

/**
 * \file
 * Atomic operations do not require locking, but they are not very powerful.
 */

# include <assert.h>
# include <stdatomic.h>
# include <vlc_common.h>

typedef atomic_uint_least32_t vlc_atomic_float;

static inline void vlc_atomic_init_float(vlc_atomic_float *var, float f)
{
    union { float f; uint32_t i; } u;
    u.f = f;
    atomic_init(var, u.i);
}

/** Helper to retrieve a single precision from an atom. */
static inline float vlc_atomic_load_float(vlc_atomic_float *atom)
{
    union { float f; uint32_t i; } u;
    u.i = atomic_load(atom);
    return u.f;
}

/** Helper to store a single precision into an atom. */
static inline void vlc_atomic_store_float(vlc_atomic_float *atom, float f)
{
    union { float f; uint32_t i; } u;
    u.f = f;
    atomic_store(atom, u.i);
}

typedef struct vlc_atomic_rc_t {
    atomic_uint refs;
} vlc_atomic_rc_t;

/** Init the RC to 1 */
static inline void vlc_atomic_rc_init(vlc_atomic_rc_t *rc)
{
    atomic_init(&rc->refs, 1);
}

/** Increment the RC */
static inline void vlc_atomic_rc_inc(vlc_atomic_rc_t *rc)
{
    unsigned prev = atomic_fetch_add_explicit(&rc->refs, 1, memory_order_relaxed);
    vlc_assert(prev);
    VLC_UNUSED(prev);
}

/** Decrement the RC and return true if it reaches 0 */
static inline bool vlc_atomic_rc_dec(vlc_atomic_rc_t *rc)
{
    unsigned prev = atomic_fetch_sub_explicit(&rc->refs, 1, memory_order_acq_rel);
    vlc_assert(prev);
    return prev == 1;
}

#endif
