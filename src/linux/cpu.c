/*****************************************************************************
 * linux/cpu.c: CPU detection code for Linux
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
#ifdef HAVE_SYS_AUXV_H
# include <sys/auxv.h>
#endif
#ifndef AT_HWCAP /* ancient libc, fallback to kernel header */
# include <linux/auxvec.h>
#endif
#include <vlc_common.h>
#include <vlc_cpu.h>

#if defined (__aarch64__)
unsigned vlc_CPU_raw(void)
{
    unsigned int flags = 0;
    const unsigned long hwcap = getauxval(AT_HWCAP);
    //const unsigned long hwcap2 = getauxval(AT_HWCAP2); // TODO: SVE2

    /* HWCAP_FP (HAVE_FPU) is statically assumed. */
# ifdef HWCAP_ASIMD
    if (hwcap & HWCAP_ASIMD)
        flags |= VLC_CPU_ARM_NEON;
# endif
# ifdef HWCAP_SVE
    if (hwcap & HWCAP_SVE)
        flags |= VLC_CPU_ARM_SVE;
# endif
    return flags;
}

#elif defined (__arm__)
/* On AArch32, glibc's <bits/hwcap.h> uses different HWCAP names than the
 * kernel and other libc's. Include the kernel header manually. */
# include <asm/hwcap.h>

unsigned vlc_CPU_raw(void)
{
    unsigned int flags = 0;
    const unsigned long hwcap = getauxval(AT_HWCAP);

    /* TLS implies ARMv6, Thumb-EE and VFP imply ARMv7 */
    if (hwcap & (HWCAP_TLS|HWCAP_THUMBEE|HWCAP_VFP))
        flags |= VLC_CPU_ARMv6; /* SIMD */
    if (hwcap & HWCAP_NEON)
        flags |= VLC_CPU_ARM_NEON; /* Advanced SIMD */
    return flags;
}

#elif defined (__powerpc__) /* both 32- and 64-bit */
unsigned vlc_CPU_raw(void)
{
    const unsigned long hwcap = getauxval(AT_HWCAP);
    unsigned int flags = 0;

    if (hwcap & PPC_FEATURE_HAS_ALTIVEC)
        flags |= VLC_CPU_ALTIVEC;

    return flags;
}

#elif defined (__riscv)
# define HWCAP_RV(letter) (1LU << ((letter) - 'A'))

unsigned vlc_CPU_raw(void)
{
    const unsigned long hwcap = getauxval(AT_HWCAP);
    unsigned int flags = 0;

    if (hwcap & HWCAP_RV('V'))
        flags |= VLC_CPU_RV_V;

    return flags;
}

#elif defined (__i386__) || defined (__x86_64__)
unsigned vlc_CPU_raw(void)
{
    FILE *info = fopen ("/proc/cpuinfo", "rte");
    if (info == NULL)
        return 0;

    char *line = NULL;
    size_t linelen = 0;
    uint_fast32_t all_caps = 0xFFFFFFFF;

    while (getline (&line, &linelen, info) != -1)
    {
        char *p, *cap;
        uint_fast32_t core_caps = 0;

        if (strncmp(line, "flags", 5))
            continue;

        p = line + 5;
        p += strspn(p, "\t");
        if (*p != ':')
            continue;

        while ((cap = strsep (&p, " ")) != NULL)
        {
            if (!strcmp (cap, "sse2"))
                core_caps |= VLC_CPU_SSE2;
            if (!strcmp (cap, "pni"))
                core_caps |= VLC_CPU_SSE3;
            if (!strcmp (cap, "ssse3"))
                core_caps |= VLC_CPU_SSSE3;
            if (!strcmp (cap, "sse4_1"))
                core_caps |= VLC_CPU_SSE4_1;
            if (!strcmp (cap, "avx"))
                core_caps |= VLC_CPU_AVX;
            if (!strcmp (cap, "avx2"))
                core_caps |= VLC_CPU_AVX2;
        }

        /* Take the intersection of capabilities of each processor */
        all_caps &= core_caps;
    }
    fclose (info);
    free (line);

    if (all_caps == 0xFFFFFFFF) /* Error parsing of cpuinfo? */
        all_caps = 0; /* Do not assume any capability! */

    return all_caps;
}
#endif
