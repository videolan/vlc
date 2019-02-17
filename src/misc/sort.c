/******************************************************************************
 * sort.c: sort back-end
 ******************************************************************************
 * Copyright © 2019 VLC authors and VideoLAN
 * Copyright © 2018 Rémi Denis-Courmont
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

static thread_local struct
{
    int (*compar)(const void *, const void *, void *);
    void *arg;
} state;

static int compar_wrapper(const void *a, const void *b)
{
    return state.compar(a, b, state.arg);
}

/* Follow the upcoming POSIX prototype, coming from GNU/libc.
 * Note that this differs from the BSD prototype. */

VLC_WEAK void vlc_qsort(void *base, size_t nmemb, size_t size,
                        int (*compar)(const void *, const void *, void *),
                        void *arg)
{
    int (*saved_compar)(const void *, const void *, void *) = state.compar;
    void *saved_arg = state.arg;

    state.compar = compar;
    state.arg = arg;

    qsort(base, nmemb, size, compar_wrapper);

    /* Restore state for nested reentrant calls */
    state.compar = saved_compar;
    state.arg = saved_arg;
}
