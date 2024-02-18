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

#  define __ckd_func(op, r, a, b) \
    _Generic (*(r), \
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

#  define ckd_add(r, a, b) __ckd_func(add, r, a, b)
#  define ckd_sub(r, a, b) __ckd_func(sub, r, a, b)
#  define ckd_mul(r, a, b) __ckd_func(mul, r, a, b)
# endif
#endif /* __STDC_VERSION_STDCKDINT_H__ */
