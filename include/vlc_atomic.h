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

/*** Native C11 atomics ***/
#  include <stdatomic.h>

# elif defined (__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || (defined (__clang__) && (defined (__x86_64__) || defined (__i386__)))

/*** Intel/GCC atomics ***/

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
 * 8-byte atomic types (or not efficiently). */
typedef          bool      atomic_flag;
typedef          bool      atomic_bool;
typedef          char      atomic_char;
typedef   signed char      atomic_schar;
typedef unsigned char      atomic_uchar;
typedef          short     atomic_short;
typedef unsigned short     atomic_ushort;
typedef          int       atomic_int;
typedef unsigned int       atomic_uint;
typedef          long      atomic_long;
typedef unsigned long      atomic_ulong;
typedef          long long atomic_llong;
typedef unsigned long long atomic_ullong;
//typedef          char16_t  atomic_char16_t;
//typedef          char32_t  atomic_char32_t;
typedef          wchar_t   atomic_wchar_t;
typedef       int_least8_t atomic_int_least8_t;
typedef      uint_least8_t atomic_uint_least8_t;
typedef      int_least16_t atomic_int_least16_t;
typedef     uint_least16_t atomic_uint_least16_t;
typedef      int_least32_t atomic_int_least32_t;
typedef     uint_least32_t atomic_uint_least32_t;
typedef      int_least64_t atomic_int_least64_t;
typedef     uint_least64_t atomic_uint_least64_t;
typedef       int_fast8_t atomic_int_fast8_t;
typedef      uint_fast8_t atomic_uint_fast8_t;
typedef      int_fast16_t atomic_int_fast16_t;
typedef     uint_fast16_t atomic_uint_fast16_t;
typedef      int_fast32_t atomic_int_fast32_t;
typedef     uint_fast32_t atomic_uint_fast32_t;
typedef      int_fast64_t atomic_int_fast64_t;
typedef     uint_fast64_t atomic_uint_fast64_t;
typedef          intptr_t atomic_intptr_t;
typedef         uintptr_t atomic_uintptr_t;
typedef            size_t atomic_size_t;
typedef         ptrdiff_t atomic_ptrdiff_t;
typedef          intmax_t atomic_intmax_t;
typedef         uintmax_t atomic_uintmax_t;

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

#  define atomic_exchange(object,desired) \
({  \
    typeof (object) _obj = (object); \
    typeof (*object) _old; \
    do \
        _old = atomic_load(_obj); \
    while (!__sync_bool_compare_and_swap(_obj, _old, (desired))); \
    _old; \
})

#  define atomic_exchange_explicit(object,desired,order) \
    atomic_exchange(object,desired)

#  define atomic_compare_exchange(object,expected,desired) \
({  \
    typeof (object) _exp = (expected); \
    typeof (*object) _old = *_exp; \
    *_exp = __sync_val_compare_and_swap((object), _old, (desired)); \
    *_exp == _old; \
})

#  define atomic_compare_exchange_strong(object,expected,desired) \
    atomic_compare_exchange(object, expected, desired)

#  define atomic_compare_exchange_strong_explicit(object,expected,desired,order) \
    atomic_compare_exchange_strong(object, expected, desired)

#  define atomic_compare_exchange_weak(object,expected,desired) \
    atomic_compare_exchange(object, expected, desired)

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

# else

/*** No atomics ***/

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

typedef uintptr_t atomic_generic_t;
typedef atomic_generic_t atomic_flag;
typedef atomic_generic_t atomic_bool;
//typedef atomic_generic_t atomic_char;
//typedef atomic_generic_t atomic_schar;
typedef atomic_generic_t atomic_uchar;
//typedef atomic_generic_t atomic_short;
typedef atomic_generic_t atomic_ushort;
//typedef atomic_generic_t atomic_int;
typedef atomic_generic_t atomic_uint;
//typedef atomic_generic_t atomic_long;
typedef atomic_generic_t atomic_ulong;
//typedef atomic_generic_t atomic_llong;
//typedef atomic_generic_t atomic_ullong;
//typedef atomic_generic_t atomic_char16_t;
//typedef atomic_generic_t atomic_char32_t;
//typedef atomic_generic_t atomic_wchar_t;
//typedef atomic_generic_t atomic_int_least8_t;
typedef atomic_generic_t atomic_uint_least8_t;
//typedef atomic_generic_t atomic_int_least16_t;
typedef atomic_generic_t atomic_uint_least16_t;
//typedef atomic_generic_t atomic_int_least32_t;
typedef atomic_generic_t atomic_uint_least32_t;
//typedef atomic_generic_t atomic_int_least64_t;
//typedef atomic_generic_t atomic_uint_least64_t;
//typedef atomic_generic_t atomic_int_fast8_t;
typedef atomic_generic_t atomic_uint_fast8_t;
//typedef atomic_generic_t atomic_int_fast16_t;
typedef atomic_generic_t atomic_uint_fast16_t;
//typedef atomic_generic_t atomic_int_fast32_t;
typedef atomic_generic_t atomic_uint_fast32_t;
//typedef atomic_generic_t atomic_int_fast64_t
//typedef atomic_generic_t atomic_uint_fast64_t;
//typedef atomic_generic_t atomic_intptr_t;
typedef atomic_generic_t atomic_uintptr_t;
typedef atomic_generic_t atomic_size_t;
//typedef atomic_generic_t atomic_ptrdiff_t;
//typedef atomic_generic_t atomic_intmax_t;
//typedef atomic_generic_t atomic_uintmax_t;

#define atomic_store(object,desired) \
    do { \
        vlc_global_lock(VLC_ATOMIC_MUTEX); \
        *(object) = (desired); \
        vlc_global_unlock(VLC_ATOMIC_MUTEX); \
    } while (0)

#  define atomic_store_explicit(object,desired,order) \
    atomic_store(object,desired)

static inline uintptr_t atomic_load(atomic_generic_t *object)
{
    uintptr_t value;

    vlc_global_lock(VLC_ATOMIC_MUTEX);
    value = *object;
    vlc_global_unlock(VLC_ATOMIC_MUTEX);
    return value;
}
#  define atomic_load_explicit(object,order) \
    atomic_load(object)

static inline
uintptr_t atomic_exchange(atomic_generic_t *object, uintptr_t desired)
{
    uintptr_t value;

    vlc_global_lock(VLC_ATOMIC_MUTEX);
    value = *object;
    *object = desired;
    vlc_global_unlock(VLC_ATOMIC_MUTEX);
    return value;
}
#  define atomic_exchange_explicit(object,desired,order) \
    atomic_exchange(object,desired)

static inline
bool vlc_atomic_compare_exchange(atomic_generic_t *object,
                                 uintptr_t *expected, uintptr_t desired)
{
    bool ret;

    vlc_global_lock(VLC_ATOMIC_MUTEX);
    ret = *object == *expected;
    if (ret)
        *object = desired;
    else
        *expected = *object;
    vlc_global_unlock(VLC_ATOMIC_MUTEX);
    return ret;
}
#  define atomic_compare_exchange_strong(object,expected,desired) \
    vlc_atomic_compare_exchange(object, expected, desired)
#  define atomic_compare_exchange_strong_explicit(object,expected,desired,order) \
    atomic_compare_exchange_strong(object, expected, desired)
#  define atomic_compare_exchange_weak(object,expected,desired) \
    vlc_atomic_compare_exchange(object, expected, desired)
#  define atomic_compare_exchange_weak_explicit(object,expected,desired,order) \
    atomic_compare_exchange_weak(object, expected, desired)

static inline
uintmax_t atomic_fetch_add(atomic_generic_t *object, uintptr_t operand)
{
    uintptr_t value;

    vlc_global_lock(VLC_ATOMIC_MUTEX);
    value = *object;
    *object += operand;
    vlc_global_unlock(VLC_ATOMIC_MUTEX);
    return value;
}
#  define atomic_fetch_add_explicit(object,operand,order) \
    atomic_fetch_add(object,operand)

static inline
uintptr_t atomic_fetch_sub(atomic_generic_t *object, uintptr_t operand)
{
    uintptr_t value;

    vlc_global_lock(VLC_ATOMIC_MUTEX);
    value = *object;
    *object -= operand;
    vlc_global_unlock(VLC_ATOMIC_MUTEX);
    return value;
}
#  define atomic_fetch_sub_explicit(object,operand,order) \
    atomic_fetch_sub(object,operand)

static inline
uintptr_t atomic_fetch_or(atomic_generic_t *object, uintptr_t operand)
{
    uintptr_t value;

    vlc_global_lock(VLC_ATOMIC_MUTEX);
    value = *object;
    *object |= operand;
    vlc_global_unlock(VLC_ATOMIC_MUTEX);
    return value;
}
#  define atomic_fetch_or_explicit(object,operand,order) \
    atomic_fetch_or(object,operand)

static inline
uintptr_t atomic_fetch_xor(atomic_generic_t *object, uintptr_t operand)
{
    uintptr_t value;

    vlc_global_lock(VLC_ATOMIC_MUTEX);
    value = *object;
    *object ^= operand;
    vlc_global_unlock(VLC_ATOMIC_MUTEX);
    return value;
}
#  define atomic_fetch_xor_explicit(object,operand,order) \
    atomic_fetch_sub(object,operand)

static inline
uintptr_t atomic_fetch_and(atomic_generic_t *object, uintptr_t operand)
{
    uintptr_t value;

    vlc_global_lock(VLC_ATOMIC_MUTEX);
    value = *object;
    *object &= operand;
    vlc_global_unlock(VLC_ATOMIC_MUTEX);
    return value;
}
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

/**
 * Memory storage space for an atom. Never access it directly.
 */
typedef union
{
    atomic_uintptr_t u;
} vlc_atomic_t;

/** Static initializer for \ref vlc_atomic_t */
# define VLC_ATOMIC_INIT(val) { (val) }

/* All functions return the atom value _after_ the operation. */
static inline uintptr_t vlc_atomic_get(vlc_atomic_t *atom)
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
    atomic_compare_exchange_strong(&atom->u, &u, v);
    return u;
}

/** Helper to retrieve a single precision from an atom. */
static inline float vlc_atomic_getf(vlc_atomic_t *atom)
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
