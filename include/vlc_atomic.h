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

# else

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
#  if defined (_MSC_VER)
/* Some atomic operations of the Interlocked API are only
   available for desktop apps. Thus we define the atomic types to
   be at least 32 bits wide. */
typedef      int_least32_t atomic_flag;
typedef      int_least32_t atomic_bool;
typedef      int_least32_t atomic_char;
typedef      int_least32_t atomic_schar;
typedef     uint_least32_t atomic_uchar;
typedef      int_least32_t atomic_short;
typedef     uint_least32_t atomic_ushort;
#  else
typedef          bool      atomic_flag;
typedef          bool      atomic_bool;
typedef          char      atomic_char;
typedef   signed char      atomic_schar;
typedef unsigned char      atomic_uchar;
typedef          short     atomic_short;
typedef unsigned short     atomic_ushort;
#  endif
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

# if defined (__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || (defined (__clang__) && (defined (__x86_64__) || defined (__i386__)))

/*** Intel/GCC atomics ***/

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

# elif defined (__GNUC__)

/*** No atomics ***/

#  define atomic_store(object,desired) \
    do { \
        typeof (object) _obj = (object); \
        typeof (*object) _des = (desired); \
        vlc_global_lock(VLC_ATOMIC_MUTEX); \
        *_obj = _des; \
        vlc_global_unlock(VLC_ATOMIC_MUTEX); \
    } while (0)
#  define atomic_store_explicit(object,desired,order) \
    atomic_store(object,desired)

#  define atomic_load(object) \
({ \
    typeof (object) _obj = (object); \
    typeof (*object) _old; \
    vlc_global_lock(VLC_ATOMIC_MUTEX); \
    _old = *_obj; \
    vlc_global_unlock(VLC_ATOMIC_MUTEX); \
    _old; \
})
#  define atomic_load_explicit(object,order) \
    atomic_load(object)

#  define atomic_exchange(object,desired) \
({ \
    typeof (object) _obj = (object); \
    typeof (*object) _des = (desired); \
    typeof (*object) _old; \
    vlc_global_lock(VLC_ATOMIC_MUTEX); \
    _old = *_obj; \
    *_obj = _des; \
    vlc_global_unlock(VLC_ATOMIC_MUTEX); \
    _old; \
})
#  define atomic_exchange_explicit(object,desired,order) \
    atomic_exchange(object,desired)

#  define atomic_compare_exchange_strong(object,expected,desired) \
({ \
    typeof (object) _obj = (object); \
    typeof (object) _exp = (expected); \
    typeof (*object) _des = (desired); \
    bool ret; \
    vlc_global_lock(VLC_ATOMIC_MUTEX); \
    ret = *_obj == *_exp; \
    if (ret) \
        *_obj = _des; \
    else \
        *_exp = *_obj; \
    vlc_global_unlock(VLC_ATOMIC_MUTEX); \
    ret; \
})
#  define atomic_compare_exchange_strong_explicit(object,expected,desired,order) \
    atomic_compare_exchange_strong(object, expected, desired)
#  define atomic_compare_exchange_weak(object,expected,desired) \
    atomic_compare_exchange_strong(object, expected, desired)
#  define atomic_compare_exchange_weak_explicit(object,expected,desired,order) \
    atomic_compare_exchange_weak(object, expected, desired)

#  define atomic_fetch_OP(object,desired,op) \
({ \
    typeof (object) _obj = (object); \
    typeof (*object) _des = (desired); \
    typeof (*object) _old; \
    vlc_global_lock(VLC_ATOMIC_MUTEX); \
    _old = *_obj; \
    *_obj = (*_obj) op (_des); \
    vlc_global_unlock(VLC_ATOMIC_MUTEX); \
    _old; \
})

#  define atomic_fetch_add(object,operand) \
    atomic_fetch_OP(object,operand,+)
#  define atomic_fetch_add_explicit(object,operand,order) \
    atomic_fetch_add(object,operand)

#  define atomic_fetch_sub(object,operand) \
    atomic_fetch_OP(object,operand,-)
#  define atomic_fetch_sub_explicit(object,operand,order) \
    atomic_fetch_sub(object,operand)

#  define atomic_fetch_or(object,operand) \
    atomic_fetch_OP(object,operand,|)
#  define atomic_fetch_or_explicit(object,operand,order) \
    atomic_fetch_or(object,operand)

#  define atomic_fetch_xor(object,operand) \
    atomic_fetch_OP(object,operand,^)
#  define atomic_fetch_xor_explicit(object,operand,order) \
    atomic_fetch_sub(object,operand)

#  define atomic_fetch_and(object,operand) \
    atomic_fetch_OP(object,operand,&)
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

# elif defined (_MSC_VER)

# include <windows.h>

/*** Use the Interlocked API. ***/

/* Define macros in order to dispatch to the correct function depending on the type.
   Several ranges are need because some operations are not implemented for all types. */
#  define atomic_type_dispatch_32_64(operation, object, ...) \
    (sizeof(*object) == 4 ? operation((LONG *)object, __VA_ARGS__) : \
    sizeof(*object) == 8 ? operation##64((LONGLONG *)object, __VA_ARGS__) : \
    (abort(), 0))

#  define atomic_type_dispatch_16_64(operation, object, ...) \
    (sizeof(*object) == 2 ? operation##16((short *)object, __VA_ARGS__) : \
    atomic_type_dispatch_32_64(operation, object, __VA_ARGS__))

#  define atomic_type_dispatch_8_64(operation, object, ...) \
    (sizeof(*object) == 1 ? operation##8((char *)object, __VA_ARGS__) : \
    atomic_type_dispatch_16_64(operation, object, __VA_ARGS__))

#  define atomic_store(object,desired) \
    atomic_type_dispatch_16_64(InterlockedExchange, object, desired)
#  define atomic_store_explicit(object,desired,order) \
    atomic_store(object, desired)

#  define atomic_load(object) \
    atomic_type_dispatch_16_64(InterlockedCompareExchange, object, 0, 0)
#  define atomic_load_explicit(object,order) \
    atomic_load(object)

#  define atomic_exchange(object,desired) \
    atomic_type_dispatch_16_64(InterlockedExchange, object, desired)
#  define atomic_exchange_explicit(object,desired,order) \
    atomic_exchange(object, desired)

#  define atomic_compare_exchange_strong(object,expected,desired) \
    atomic_type_dispatch_16_64(InterlockedCompareExchange, object, *expected, desired) == *expected
#  define atomic_compare_exchange_strong_explicit(object,expected,desired,order) \
    atomic_compare_exchange_strong(object, expected, desired)
#  define atomic_compare_exchange_weak(object,expected,desired) \
    atomic_compare_exchange_strong(object, expected, desired)
#  define atomic_compare_exchange_weak_explicit(object,expected,desired,order) \
    atomic_compare_exchange_weak(object, expected, desired)

#  define atomic_fetch_add(object,operand) \
    atomic_type_dispatch_32_64(InterlockedExchangeAdd, object, operand)
#  define atomic_fetch_add_explicit(object,operand,order) \
    atomic_fetch_add(object, operand)

#  define atomic_fetch_sub(object,operand) \
    atomic_type_dispatch_32_64(InterlockedExchangeAdd, object, -(LONGLONG)operand)
#  define atomic_fetch_sub_explicit(object,operand,order) \
    atomic_fetch_sub(object, operand)

#  define atomic_fetch_or(object,operand) \
    atomic_type_dispatch_8_64(InterlockedOr, object, operand)
#  define atomic_fetch_or_explicit(object,operand,order) \
    atomic_fetch_or(object, operand)

#  define atomic_fetch_xor(object,operand) \
    atomic_type_dispatch_8_64(InterlockedXor, object, operand)
#  define atomic_fetch_xor_explicit(object,operand,order) \
    atomic_fetch_sub(object, operand)

#  define atomic_fetch_and(object,operand) \
    atomic_type_dispatch_8_64(InterlockedAnd, object, operand)
#  define atomic_fetch_and_explicit(object,operand,order) \
    atomic_fetch_and(object, operand)

#  define atomic_flag_test_and_set(object) \
    atomic_exchange(object, true)

#  define atomic_flag_test_and_set_explicit(object,order) \
    atomic_flag_test_and_set(object)

#  define atomic_flag_clear(object) \
    atomic_store(object, false)

#  define atomic_flag_clear_explicit(object,order) \
    atomic_flag_clear(object)

# else
#  error FIXME: implement atomic operations for this compiler.
# endif
# endif

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

#endif
