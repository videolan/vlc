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

#else
#undef CPU_FLAGS
#if defined (__arm__)
# define CPU_FLAGS "Features"

#elif defined (__i386__) || defined (__x86_64__)
# define CPU_FLAGS "flags"

#elif defined (__powerpc__) || defined (__powerpc64__)
# define CPU_FLAGS "cpu"

#elif defined (__riscv)
# include <vlc_strings.h>
# define CPU_FLAGS "isa"

static unsigned vlc_CPU_RV_isa_parse(const char *isa)
{
    unsigned caps = 0;
    int c;

    if (vlc_ascii_tolower((unsigned char)isa[0]) != 'r'
     || vlc_ascii_tolower((unsigned char)isa[1]) != 'v')
        return 0;

    isa += 2;

    if (strncmp(isa, "32", 2) == 0 || strncmp(isa, "64", 2) == 0)
        isa += 2;
    else if (strncmp(isa, "128", 3) == 0)
        isa += 3;
    else
        return 0;

    while ((c = vlc_ascii_tolower((unsigned char)*isa)) != '\0') {
        size_t extlen = 1;

        switch (c) {
            case '_':
                break;

            case 'z':
            case 's':
            case 'h':
            case 'x':
                extlen = 1 + strcspn(isa + 1, "_");
                break;

            default:
                if (((unsigned)(c - 'a')) > 'y')
                    return 0;

                while (isa[extlen] && ((unsigned)(isa[extlen] - '0')) < 10)
                    extlen++;

                if (vlc_ascii_tolower(isa[extlen]) == 'p') {
                    extlen++;

                    while (isa[extlen] && ((unsigned)(isa[extlen] - '0')) < 10)
                        extlen++;
                }
        }

        /* TODO: Zve extensions */
        if (c == 'v')
            caps |= VLC_CPU_RV_V;

        isa += extlen;
    }

    return caps;
}
#endif

#ifdef CPU_FLAGS
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

#if defined (__arm__)
        unsigned ver;
        if (sscanf (line, "Processor\t: ARMv%u", &ver) >= 1 && ver >= 6)
            core_caps |= VLC_CPU_ARMv6;
#endif
        if (strncmp (line, CPU_FLAGS, strlen (CPU_FLAGS)))
            continue;

        p = line + strlen(CPU_FLAGS);
        p += strspn(p, "\t");
        if (*p != ':')
            continue;

#if defined (__riscv)
        p += strspn(p, "\t ");
        core_caps = vlc_CPU_RV_isa_parse(p);
#else
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
            if (!strcmp (cap, "xop"))
                core_caps |= VLC_CPU_XOP;
            if (!strcmp (cap, "fma4"))
                core_caps |= VLC_CPU_FMA4;

#elif defined (__powerpc__) || defined (__powerpc64__)
            if (!strcmp (cap, "altivec supported"))
                core_caps |= VLC_CPU_ALTIVEC;
#endif
        }
#endif

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
#endif
