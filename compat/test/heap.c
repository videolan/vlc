/*****************************************************************************
 * aligned_alloc test case
 *****************************************************************************
 * Copyright © 2019 Rémi Denis-Courmont
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

#include "config.h"
#undef NDEBUG
#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

struct big_align_struct {
    long long ll;
    double d;
};

/* Supported alignments. Others are undefined (ISO C11 §7.22.3, §J.2). */
static const size_t alignv[] = {
    alignof (char),
    alignof (short),
    alignof (int),
    alignof (long),
    alignof (long long),
    alignof (float),
    alignof (double),
    alignof (struct big_align_struct),
    alignof (void *),
    alignof (max_align_t),
};

static const size_t alignc = sizeof (alignv) / sizeof (alignv[0]);

static void test_posix_memalign(size_t align, size_t size)
{
    void *p;
    int val = posix_memalign(&p, align, size);

    if (align >= sizeof (void *)) {
        if (val == 0) {
            assert(((uintptr_t)p & (align - 1)) == 0);
            free(p);
        }
    } else
        assert(val != 0);
}

int main(void)
{
    void *p;

    /* aligned_alloc() */

    for (size_t i = 0; i < alignc; i++) {
        size_t align = alignv[i];

        assert((align & (align - 1)) == 0); /* must be a power of two */

        p = aligned_alloc(alignv[i], 0);
        free(p); /* must free {aligned_,c,m,c,re}alloc() allocations */

        for (size_t j = 0; j < alignc; j++) {
             size_t size = alignv[j];

             if (size < align)
                 continue; /* size must be a multiple of alignment */

             p = aligned_alloc(align, size);
             assert(p != NULL); /* small non-zero bytes allocation */
             assert(((uintptr_t)p & (align - 1)) == 0);
             free(p);
        }
    }

    /* posix_memalign() */

    for (size_t i = 0; i < 21; i++) {
        size_t align = (size_t)1 << i;

        test_posix_memalign(align, 0);
        test_posix_memalign(align, 1);
        test_posix_memalign(align, align - 1);
        test_posix_memalign(align, align);
        test_posix_memalign(align, align * 2);
    }

    return 0;
}
