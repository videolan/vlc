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
        char *p = line, *cap;
        uint_fast32_t core_caps = 0;

#if defined (__arm__)
        unsigned ver;
        if (sscanf (line, "Processor\t: ARMv%u", &ver) >= 1 && ver >= 6)
            core_caps |= VLC_CPU_ARMv6;
#endif
        if (strncmp (line, CPU_FLAGS, strlen (CPU_FLAGS)))
            continue;

        while ((cap = strsep (&p, " ")) != NULL)
        {
#if defined (__arm__)
            if (!strcmp (cap, "neon"))
                core_caps |= VLC_CPU_ARM_NEON;

#elif defined (__i386__) || defined (__x86_64__)
            if (!strcmp (cap, "mmx"))
                core_caps |= VLC_CPU_MMX;
            if (!strcmp (cap, "sse"))
                core_caps |= VLC_CPU_SSE | VLC_CPU_MMXEXT;
            if (!strcmp (cap, "mmxext"))
                core_caps |= VLC_CPU_MMXEXT;
            if (!strcmp (cap, "sse2"))
                core_caps |= VLC_CPU_SSE2;
            if (!strcmp (cap, "pni"))
                core_caps |= VLC_CPU_SSE3;
            if (!strcmp (cap, "ssse3"))
                core_caps |= VLC_CPU_SSSE3;
            if (!strcmp (cap, "sse4_1"))
                core_caps |= VLC_CPU_SSE4_1;
            if (!strcmp (cap, "sse4_2"))
                core_caps |= VLC_CPU_SSE4_2;
            if (!strcmp (cap, "sse4a"))
                core_caps |= VLC_CPU_SSE4A;
            if (!strcmp (cap, "avx"))
                core_caps |= VLC_CPU_AVX;
            if (!strcmp (cap, "avx2"))
                core_caps |= VLC_CPU_AVX2;
            if (!strcmp (cap, "3dnow"))
                core_caps |= VLC_CPU_3dNOW;
            if (!strcmp (cap, "xop"))
                core_caps |= VLC_CPU_XOP;
            if (!strcmp (cap, "fma4"))
                core_caps |= VLC_CPU_FMA4;

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
