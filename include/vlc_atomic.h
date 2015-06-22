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

# ifndef __cplusplus
#  if (__STDC_VERSION__ >= 201112L) && defined(__STDC_NO_ATOMICS__)
#   error Atomic operations required!
#  endif
/*** Native C11 atomics ***/
#  include <stdatomic.h>

# else /* C++ */
/*** Native C++11 atomics ***/
#   include <atomic>
using std::atomic_is_lock_free;
using std::atomic_init;
using std::atomic_store;
using std::atomic_store_explicit;
using std::atomic_load;
using std::atomic_load_explicit;
using std::atomic_exchange;
using std::atomic_exchange_explicit;
using std::atomic_compare_exchange_strong;
using std::atomic_compare_exchange_strong_explicit;
using std::atomic_compare_exchange_weak;
using std::atomic_compare_exchange_weak_explicit;
using std::atomic_fetch_add;
using std::atomic_fetch_add_explicit;
using std::atomic_fetch_sub;
using std::atomic_fetch_sub_explicit;
using std::atomic_fetch_or;
using std::atomic_fetch_or_explicit;
using std::atomic_fetch_xor;
using std::atomic_fetch_xor_explicit;
using std::atomic_fetch_and;
using std::atomic_fetch_and_explicit;
using std::atomic_thread_fence;
using std::atomic_signal_fence;

using std::memory_order;
using std::memory_order_relaxed;
using std::memory_order_consume;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;

using std::atomic_flag;
typedef std::atomic<bool> atomic_bool;
typedef std::atomic<char> atomic_char;
typedef std::atomic<signed char> atomic_schar;
typedef std::atomic<unsigned char> atomic_uchar;
typedef std::atomic<short> atomic_short;
typedef std::atomic<unsigned short> atomic_ushort;
typedef std::atomic<int> atomic_int;
typedef std::atomic<unsigned int> atomic_uint;
typedef std::atomic<long> atomic_long;
typedef std::atomic<unsigned long> atomic_ulong;
typedef std::atomic<long long> atomic_llong;
typedef std::atomic<unsigned long long> atomic_ullong;
typedef std::atomic<char16_t> atomic_char16_t;
typedef std::atomic<char32_t> atomic_char32_t;
typedef std::atomic<wchar_t> atomic_wchar_t;
typedef std::atomic<int_least8_t> atomic_int_least8_t;
typedef std::atomic<uint_least8_t> atomic_uint_least8_t;
typedef std::atomic<int_least16_t> atomic_int_least16_t;
typedef std::atomic<uint_least16_t> atomic_uint_least16_t;
typedef std::atomic<int_least32_t> atomic_int_least32_t;
typedef std::atomic<uint_least32_t> atomic_uint_least32_t;
typedef std::atomic<int_least64_t> atomic_int_least64_t;
typedef std::atomic<uint_least64_t> atomic_uint_least64_t;
typedef std::atomic<int_fast8_t> atomic_int_fast8_t;
typedef std::atomic<uint_fast8_t> atomic_uint_fast8_t;
typedef std::atomic<int_fast16_t> atomic_int_fast16_t;
typedef std::atomic<uint_fast16_t> atomic_uint_fast16_t;
typedef std::atomic<int_fast32_t> atomic_int_fast32_t;
typedef std::atomic<uint_fast32_t> atomic_uint_fast32_t;
typedef std::atomic<int_fast64_t> atomic_int_fast64_t;
typedef std::atomic<uint_fast64_t> atomic_uint_fast64_t;
typedef std::atomic<intptr_t> atomic_intptr_t;
typedef std::atomic<uintptr_t> atomic_uintptr_t;
typedef std::atomic<size_t> atomic_size_t;
typedef std::atomic<ptrdiff_t> atomic_ptrdiff_t;
typedef std::atomic<intmax_t> atomic_intmax_t;
typedef std::atomic<uintmax_t> atomic_uintmax_t;

# endif /* C++ */

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
