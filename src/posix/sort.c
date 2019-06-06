/******************************************************************************
 * sort.c: POSIX sort back-end
 ******************************************************************************
 * Copyright © 2019 VLC authors and VideoLAN
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
#include <vlc_sort.h>

#ifdef HAVE_BROKEN_QSORT_R

struct compat_arg
{
    int (*compar)(const void *, const void *, void *);
    void *arg;
};

static int compar_compat(void *arg, const void *a, const void *b)
{
    struct compat_arg *compat_arg = arg;
    return compat_arg->compar(a, b, compat_arg->arg);
}

/* Follow the FreeBSD prototype */
void vlc_qsort(void *base, size_t nmemb, size_t size,
               int (*compar)(const void *, const void *, void *),
               void *arg)
{
    struct compat_arg compat_arg = {
        .compar = compar,
        .arg = arg
    };
    qsort_r(base, nmemb, size, &compat_arg, compar_compat);
}

#else

/* Follow the POSIX prototype */
void vlc_qsort(void *base, size_t nmemb, size_t size,
               int (*compar)(const void *, const void *, void *),
               void *arg)
{
    qsort_r(base, nmemb, size, compar, arg);
}
#endif
