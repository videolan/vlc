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

#ifndef VLC_ATOMIC_H
# define VLC_ATOMIC_H

/**
 * \file
 * Atomic operations do not require locking, but they are not very powerful.
 */

# if !defined (__cplusplus) && (__STDC_VERSION__ >= 201112L) \
  && !defined (__STDC_NO_ATOMICS__)
#  include <stdatomic.h>

# else /* if (???) */

#  define ATOMIC_FLAG_INIT false

#  define ATOMIC_VAR_INIT(value) (value)

#  define atomic_init(obj, value) \
    do { *(obj) = (value); } while(0)

#  define kill_dependency(y) \
    ((void)0)

#  define atomic_thread_fence(order) \
    __sync_synchronize()

#  define atomic_signal_fence(order) \
    ((void)0)

#  define atomic_is_lock_free(obj) \
    false

/* In principles, __sync_*() only supports int, long and long long and their
 * unsigned equivalents, i.e. 4-bytes and 8-bytes types, although GCC also
 * supports 1 and 2-bytes types. Some non-x86 architectures do not support
 * 8-byte atomic types (or not efficiently). So lets stick to (u)intptr_t. */
typedef  intptr_t          atomic_flag;
typedef  intptr_t          atomic_bool;
typedef  intptr_t          atomic_char;
typedef  intptr_t          atomic_schar;
typedef uintptr_t          atomic_uchar;
typedef  intptr_t          atomic_short;
typedef uintptr_t          atomic_ushort;
typedef  intptr_t          atomic_int;
typedef uintptr_t          atomic_uint;
//typedef   signed long atomic_long;
//typedef unsigned long atomic_ulong;
//typedef   signed long long atomic_llong;
//typedef unsigned long long atomic_ullong;
/* ... */
typedef  intptr_t          atomic_intptr_t;
typedef uintptr_t          atomic_uintptr_t;
typedef uintptr_t          atomic_size_t;
typedef  intptr_t          atomic_ptrdiff_t;
//typedef  intmax_t          atomic_intmax_t;
//typedef uintmax_t          atomic_uintmax_t;

#  define atomic_store(object,desired) \
    do { \
        *(object) = (desired); \
        __sync_synchronize(); \
    } while (0)

#  define atomic_store_explicit(object,desired,order) \
    atomic_store(object,desired)

#  define atomic_load(object) \
    (__sync_synchronize(), *(object))

#  define atomic_load_explicit(object,order) \
    atomic_load(object)

static inline
intptr_t vlc_atomic_exchange(volatile void *object, intptr_t desired)
{
    volatile intptr_t *ptr = (volatile intptr_t *)object;
    intptr_t old;
    /* NOTE: while __sync_lock_test_and_set() is an atomic exchange, its memory
     * order is too weak (acquire instead of sequentially consistent).
     * Because of that, for lack of both C11 _Generic() and GNU C compound
     * statements, atomic exchange needs a helper function.
     * Thus all atomic types must have the same size. */
    do
        old = atomic_load(ptr);
    while (!__sync_bool_compare_and_swap(ptr, old, desired));

    return old;
}

#  define atomic_exchange(object,desired) \
    vlc_atomic_exchange(object,desired)

#  define atomic_exchange_explicit(object,desired,order) \
    atomic_exchange(object,desired)

static inline
bool vlc_atomic_compare_exchange(volatile void *object, void *expected,
                                 intptr_t desired)
{
    volatile intptr_t *ptr = (volatile intptr_t *)object;
    intptr_t old = *(intptr_t *)expected;
    intptr_t val = __sync_val_compare_and_swap(ptr, old, desired);
    if (old != val)
    {
        *(intptr_t *)expected = val;
        return false;
    }
    return true;
}

#  define atomic_compare_exchange_strong(object,expected,desired) \
    vlc_atomic_compare_exchange(object, expected, desired)

#  define atomic_compare_exchange_strong_explicit(object,expected,desired,order) \
    atomic_compare_exchange_strong(object, expected, desired)

#  define atomic_compare_exchange_weak(object,expected,desired) \
    vlc_atomic_compare_exchange(object, expected, desired)

#  define atomic_compare_exchange_weak_explicit(object,expected,desired,order) \
    atomic_compare_exchange_weak(object, expected, desired)

#  define atomic_fetch_add(object,operand) \
    __sync_fetch_and_add(object, operand)

#  define atomic_fetch_add_explicit(object,operand,order) \
    atomic_fetch_add(object,operand)

#  define atomic_fetch_sub(object,operand) \
    __sync_fetch_and_sub(object, operand)

#  define atomic_fetch_sub_explicit(object,operand,order) \
    atomic_fetch_sub(object,operand)

#  define atomic_fetch_or(object,operand) \
    __sync_fetch_and_or(object, operand)

#  define atomic_fetch_or_explicit(object,operand,order) \
    atomic_fetch_or(object,operand)

#  define atomic_fetch_xor(object,operand) \
    __sync_fetch_and_sub(object, operand)

#  define atomic_fetch_xor_explicit(object,operand,order) \
    atomic_fetch_sub(object,operand)

#  define atomic_fetch_and(object,operand) \
    __sync_fetch_and_and(object, operand)

#  define atomic_fetch_and_explicit(object,operand,order) \
    atomic_fetch_and(object,operand)

#  define atomic_flag_test_and_set(object) \
    atomic_exchange(object, true)

#  define atomic_flag_test_and_set_explicit(object,order) \
    atomic_flag_test_and_set(object)

#  define atomic_flag_clear(object) \
    atomic_store(object, false)

#  define atomic_flag_clear_explicit(object,order) \
    atomic_flag_clear(object)

# endif

/** Static initializer for \ref vlc_atomic_t */
# define VLC_ATOMIC_INIT(val) { (val) }

/* All functions return the atom value _after_ the operation. */
static inline uintptr_t vlc_atomic_get(const vlc_atomic_t *atom)
{
    return atomic_load(&atom->u);
}

static inline uintptr_t vlc_atomic_set(vlc_atomic_t *atom, uintptr_t v)
{
    atomic_store(&atom->u, v);
    return v;
}

static inline uintptr_t vlc_atomic_add(vlc_atomic_t *atom, uintptr_t v)
{
    return atomic_fetch_add(&atom->u, v) + v;
}

static inline uintptr_t vlc_atomic_sub (vlc_atomic_t *atom, uintptr_t v)
{
    return atomic_fetch_sub (&atom->u, v) - v;
}

static inline uintptr_t vlc_atomic_inc (vlc_atomic_t *atom)
{
    return vlc_atomic_add (atom, 1);
}

static inline uintptr_t vlc_atomic_dec (vlc_atomic_t *atom)
{
    return vlc_atomic_sub (atom, 1);
}

static inline uintptr_t vlc_atomic_swap(vlc_atomic_t *atom, uintptr_t v)
{
    return atomic_exchange(&atom->u, v);
}

static inline uintptr_t vlc_atomic_compare_swap(vlc_atomic_t *atom,
                                                uintptr_t u, uintptr_t v)
{
    return atomic_compare_exchange_strong(&atom->u, &u, v);
}

/** Helper to retrieve a single precision from an atom. */
static inline float vlc_atomic_getf(const vlc_atomic_t *atom)
{
    union { float f; uintptr_t i; } u;
    u.i = vlc_atomic_get(atom);
    return u.f;
}

/** Helper to store a single precision into an atom. */
static inline float vlc_atomic_setf(vlc_atomic_t *atom, float f)
{
    union { float f; uintptr_t i; } u;
    u.f = f;
    vlc_atomic_set(atom, u.i);
    return f;
}

#endif
