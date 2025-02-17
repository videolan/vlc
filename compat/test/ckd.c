/*****************************************************************************
 * stdckdint.h test case
 *****************************************************************************
 * Copyright © 2025 VideoLabs, VLC authors and VideoLAN
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

#include <limits.h>
#undef NDEBUG
#include <assert.h>

#if defined (TEST_CKD)
# ifdef HAVE_CKD
#  include "../include/stdckdint.h" /* Force include on the compiler path */
# else
#  define SKIP /* can't test it */
# endif
#elif defined (TEST_BUILTIN)
# if defined(__GNUC__) || defined(__clang__)
#  undef __STDC_VERSION_STDCKDINT_H__
#  include "../stdckdint/stdckdint.h" /* Force include on the compat path */
# else
#  define SKIP /* can't test it */
# endif
#elif defined (TEST_COMPAT)
# undef __STDC_VERSION_STDCKDINT_H__
# undef __GNUC__
# undef __clang__
# include "../stdckdint/stdckdint.h"
#else
# error TEST_ not defined
#endif

int main(void)
{
#ifdef SKIP
    return 77;
#else
    unsigned long long ures;
    long long res;

    /* multiplication */

    /* zero */
    assert(!ckd_mul(&ures, 0, 0) && ures == 0);
    assert(!ckd_mul(&ures, 0, 1) && ures == 0);
    assert(!ckd_mul(&ures, 1, 0) && ures == 0);

    /* small cases */
    assert(!ckd_mul(&ures, 2, 3) && ures == 6);
    assert(!ckd_mul(&res, -3, 3) && res == -9);
    assert(!ckd_mul(&res, -3, -3) && res == 9);

    /* near positive overflow */
    assert(!ckd_mul(&res, LLONG_MAX, 1) && res == LLONG_MAX);
    assert(!ckd_mul(&res, LLONG_MAX / 2, 2) && res == LLONG_MAX / 2 * 2);
    assert(ckd_mul(&res, LLONG_MAX, 2) && res == -2);
    assert(!ckd_mul(&res, LLONG_MAX, -1) && res == -LLONG_MAX);

    /* near negative overflow */
    assert(!ckd_mul(&res, LLONG_MIN, 1) && res == LLONG_MIN);
    assert(ckd_mul(&res, LLONG_MIN, -1) && res == LLONG_MIN);

    /* additions */

    /* small cases */
    assert(!ckd_add(&ures, 0, 0) && ures == 0);
    assert(!ckd_add(&ures, 0, 1) && ures == 1);
    assert(!ckd_add(&ures, 1, 0) && ures == 1);
    assert(!ckd_add(&ures, 1, 1) && ures == 2);

    /* big edge cases */
    assert(!ckd_add(&ures, ULLONG_MAX, 0ULL) && ures == ULLONG_MAX);
    assert(!ckd_add(&ures, ULLONG_MAX - 1ULL, 1ULL) && ures == ULLONG_MAX);
    assert(ckd_add(&ures, ULLONG_MAX, 1ULL) && ures == 0);
    assert(ckd_add(&res, LLONG_MAX, 1ULL) && res == LLONG_MIN);

    /* subtractions */

    /* small cases */
    assert(!ckd_sub(&ures, 0, 0) && ures == 0);
    assert(!ckd_sub(&ures, 1, 0) && ures == 1);
    assert(!ckd_sub(&ures, 1, 1) && ures == 0);

    /* 0 - 1 */
    assert(ckd_sub(&ures, 0, 1) && ures == ULLONG_MAX);
    assert(!ckd_sub(&res, 0, 1) && res == -1);

    /* edge cases: */
    assert(!ckd_sub(&ures, ULLONG_MAX, 0) && ures == ULLONG_MAX);
    assert(!ckd_sub(&ures, ULLONG_MAX, 1) && ures == ULLONG_MAX - 1ULL);
    assert(ckd_sub(&ures, 0ULL, ULLONG_MAX) && ures == 1);
    assert(ckd_sub(&ures, 2, 3) && ures == ULLONG_MAX);
    assert(ckd_sub(&res, LLONG_MIN, 1) && res == LLONG_MAX);

    return 0;
#endif
}
