/*
 * Copyright (C) 2024 Rémi Denis-Courmont
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
 */

#ifndef __STDC_VERSION_STDCKDINT_H__
# define __STDC_VERSION_STDCKDINT_H__ 202311L

# if defined(__GNUC__) || defined(__clang__)
#  define ckd_add(r, a, b) __builtin_add_overflow(a, b, r)
#  define ckd_sub(r, a, b) __builtin_sub_overflow(a, b, r)
#  define ckd_mul(r, a, b) __builtin_mul_overflow(a, b, r)
# else
#  include <limits.h>

#  define __ckd_unsigned(suffix, type, MAX) \
static inline _Bool __ckd_add_##suffix(type *r, type a, type b) \
{ \
    *r = a + b; \
    return ((type)(a + b)) < a; \
} \
\
static inline _Bool __ckd_sub_##suffix(type *r, type a, type b) \
{ \
    *r = a - b; \
    return a < b; \
} \
\
static inline _Bool __ckd_mul_##suffix(type *r, type a, type b) \
{ \
    *r = a * b; \
    return b > 0 && a > (MAX / b); \
}

#  define __ckd_signed_common(suffix, type, MIN, MAX) \
static inline _Bool __ckd_add_##suffix(type *r, type a, type b) \
{ \
    union suffix ua = { .v = a }; \
    union suffix ub = { .v = b }; \
    union suffix ur = { .uv = ua.uv + ub.uv }; \
    *r = ur.v; \
    if ((b > 0 && a > (MAX - b)) || (b < 0 && a < (MIN - b))) \
        return 1; \
    return 0; \
} \
\
static inline _Bool __ckd_sub_##suffix(type *r, type a, type b) \
{ \
    union suffix ua = { .v = a }; \
    union suffix ub = { .v = b }; \
    union suffix ur = { .uv = ua.uv - ub.uv }; \
    *r = ur.v; \
    if ((b < 0 && a > (MAX + b)) || (b > 0 && a < (MIN + b))) \
        return 1; \
    return 0; \
} \
\
static inline _Bool __ckd_mul_##suffix(type *r, type a, type b) \
{ \
    union suffix ua = { .v = a }; \
    union suffix ub = { .v = b }; \
    union suffix ur = { .uv = ua.uv * ub.uv }; \
    *r = ur.v; \
    if (a > 0) { \
        if (b > 0) { \
            if (a > (MAX / b)) return 1; \
        } else if (b < 0) { \
            if (b < (MIN / a)) return 1; \
        } \
    } else if (a < 0) { \
        if (b > 0) { \
            if (a < (MIN / b)) return 1; \
        } else if (b < 0) { \
            if (b < (MAX / a)) return 1; \
        } \
    } \
    return 0; \
}

#  define __ckd_signed(suffix, type, MIN, MAX) \
union suffix { \
    unsigned type uv; \
    type v; \
}; \
__ckd_signed_common(suffix, type, MIN, MAX)

#  define __ckd_signed_forced(suffix, type, MIN, MAX) \
union suffix { \
    unsigned type uv; \
    signed type v; \
}; \
__ckd_signed_common(suffix, signed type, MIN, MAX)

#  define __ckd_func(op, r, a, b) \
    _Generic (*(r), \
        signed char:        __ckd_##op##_sc((signed char *)(r), a, b), \
        short:              __ckd_##op##_ss((short *)(r), a, b), \
        int:                __ckd_##op##_si((int *)(r), a, b), \
        long:               __ckd_##op##_sl((long *)(r), a, b), \
        long long:          __ckd_##op##_sll((long long *)(r), a, b), \
        unsigned char:      __ckd_##op##_uc((unsigned char *)(r), a, b), \
        unsigned short:     __ckd_##op##_us((unsigned short *)(r), a, b), \
        unsigned int:       __ckd_##op##_ui((unsigned int *)(r), a, b), \
        unsigned long:      __ckd_##op##_ul((unsigned long *)(r), a, b), \
        unsigned long long: __ckd_##op##_ull((unsigned long long *)(r), a, b))

__ckd_unsigned(uc,  unsigned char,      UCHAR_MAX)
__ckd_unsigned(us,  unsigned short,     USHRT_MAX)
__ckd_unsigned(ui,  unsigned int,       UINT_MAX)
__ckd_unsigned(ul,  unsigned long,      ULONG_MAX)
__ckd_unsigned(ull, unsigned long long, ULLONG_MAX)

__ckd_signed_forced(sc,  char, SCHAR_MIN, SCHAR_MAX)
__ckd_signed(ss,  short,       SHRT_MIN,  SHRT_MAX)
__ckd_signed(si,  int,         INT_MIN,   INT_MAX)
__ckd_signed(sl,  long,        LONG_MIN,  LONG_MAX)
__ckd_signed(sll, long long,   LLONG_MIN, LLONG_MAX)

#  define ckd_add(r, a, b) __ckd_func(add, r, a, b)
#  define ckd_sub(r, a, b) __ckd_func(sub, r, a, b)
#  define ckd_mul(r, a, b) __ckd_func(mul, r, a, b)
# endif
#endif /* __STDC_VERSION_STDCKDINT_H__ */
