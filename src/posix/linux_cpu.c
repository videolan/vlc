/*****************************************************************************
 * linux_cpu.c: CPU detection code for Linux
 *****************************************************************************
 * Copyright (C) 2012 Rémi Denis-Courmont
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

#include <stdio.h>
#include <string.h>
#include <vlc_common.h>
#include <vlc_cpu.h>

#undef CPU_FLAGS
#if defined (__arm__)
# define CPU_FLAGS "Features\t:"

#elif defined (__i386__) || defined (__x86_64__)
# define CPU_FLAGS "flags\t\t:"

#elif defined (__powerpc__) || defined (__powerpc64__)
# define CPU_FLAGS "cpu\t\t:"

#endif

#ifdef CPU_FLAGS
static uint32_t cpu_flags = 0;

static void vlc_CPU_init (void)
{
    FILE *info = fopen ("/proc/cpuinfo", "rt");
    if (info == NULL)
        return;

    char *line = NULL;
    size_t linelen = 0;
    uint_fast32_t all_caps = 0xFFFFFFFF;

    while (getline (&line, &linelen, info) != -1)
    {
        if (strncmp (line, CPU_FLAGS, strlen (CPU_FLAGS)))
            continue;

        char *p = line, *cap;
        uint_fast32_t core_caps = 0;

        while ((cap = strsep (&p, " ")) != NULL)
        {
#if defined (__arm__)
            if (!strcmp (cap, "neon"))
                core_caps |= VLC_CPU_ARM_NEON;

#elif defined (__i386__) || defined (__x86_64__)
# ifndef __MMX__
            if (!strcmp (cap, "mmx"))
                core_caps |= CPU_CAPABILITY_MMX;
# endif
# ifndef __SSE__
            if (!strcmp (cap, "sse"))
                core_caps |= CPU_CAPABILITY_SSE | CPU_CAPABILITY_MMXEXT;
            if (!strcmp (cap, "mmxext"))
                core_caps |= CPU_CAPABILITY_MMXEXT;
# endif
# ifndef __SSE2__
            if (!strcmp (cap, "sse2"))
                core_caps |= CPU_CAPABILITY_SSE2;
# endif
# ifndef __SSE3__
            if (!strcmp (cap, "pni"))
                core_caps |= CPU_CAPABILITY_SSE3;
# endif
# ifndef __SSSE3__
            if (!strcmp (cap, "ssse3"))
                core_caps |= CPU_CAPABILITY_SSSE3;
# endif
# ifndef __SSE4_1__
            if (!strcmp (cap, "sse4_1"))
                core_caps |= CPU_CAPABILITY_SSE4_1;
# endif
# ifndef __SSE4_2__
            if (!strcmp (cap, "sse4_2"))
                core_caps |= CPU_CAPABILITY_SSE4_1;
# endif
            if (!strcmp (cap, "sse4a"))
                core_caps |= CPU_CAPABILITY_SSE4A;
# ifndef __3dNOW__
            if (!strcmp (cap, "3dnow"))
                core_caps |= CPU_CAPABILITY_3DNOW;
# endif

#elif defined (__powerpc__) || defined (__powerpc64__)
            if (!strcmp (cap, "altivec supported"))
                core_caps |= VLC_CPU_ALTIVEC;
#endif
        }

        /* Take the intersection of capabilities of each processor */
        all_caps &= core_caps;
    }
    fclose (info);
    free (line);

    if (all_caps == 0xFFFFFFFF) /* Error parsing of cpuinfo? */
        all_caps = 0; /* Do not assume any capability! */

    /* Always enable capabilities that were forced during compilation */
#if defined (__i386__) || defined (__x86_64__)
# ifdef __MMX__
    all_caps |= CPU_CAPABILITY_MMX;
# endif
# ifdef __SSE__
    all_caps |= CPU_CAPABILITY_SSE | CPU_CAPABILITY_MMXEXT;
# endif
# ifdef __SSE2__
    all_caps |= CPU_CAPABILITY_SSE2;
# endif
# ifdef __SSE3__
    all_caps |= CPU_CAPABILITY_SSE3;
# endif
# ifdef __SSSE3__
    all_caps |= CPU_CAPABILITY_SSSE3;
# endif
# ifdef __SSE4_1__
    all_caps |= CPU_CAPABILITY_SSE4_1;
# endif
# ifdef __SSE4_2__
    all_caps |= CPU_CAPABILITY_SSE4_2;
# endif
# ifdef __3dNOW__
    all_caps |= CPU_CAPABILITY_3DNOW;
# endif

#endif
    cpu_flags = all_caps;
}

unsigned vlc_CPU (void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    pthread_once (&once, vlc_CPU_init);
    return cpu_flags;
}
#else /* CPU_FLAGS */
unsigned vlc_CPU (void)
{
    return 0;
}
#endif
