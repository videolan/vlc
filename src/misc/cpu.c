/*****************************************************************************
 * cpu.c: CPU detection code
 *****************************************************************************
 * Copyright (C) 1998-2004 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.eduEujenio>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_cpu.h>
#include <vlc_memstream.h>
#include <vlc_modules.h>
#include "libvlc.h"

#include <assert.h>

#if defined(_MSC_VER) && !defined(__clang__)
# include <intrin.h> // __cpuid
#endif

#if defined(__OpenBSD__) && defined(__powerpc__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#endif

/**
 * Determines the CPU capabilities.
 */
VLC_WEAK unsigned vlc_CPU_raw(void)
{
    uint32_t i_capabilities = 0;

#if defined( __i386__ ) || defined( __x86_64__ )
    unsigned int i_eax, i_ebx, i_ecx, i_edx;

    /* Needed for x86 CPU capabilities detection */
#if defined(_MSC_VER) && !defined(__clang__)
# define cpuid(reg)  \
    do { \
        int cpuInfo[4]; \
        __cpuid(cpuInfo, reg); \
        i_eax = cpuInfo[0]; i_ebx = cpuInfo[1]; i_ecx = cpuInfo[2]; i_edx = cpuInfo[3]; \
    } while(0)
#else // !_MSC_VER
# define cpuid(reg) \
    asm ("cpuid" \
         : "=a" (i_eax), "=b" (i_ebx), "=c" (i_ecx), "=d" (i_edx) \
         : "a" (reg) \
         : "cc");
#endif // !_MSC_VER

     /* Check if the OS really supports the requested instructions */
# if defined (__i386__) && !defined (__i586__) \
  && !defined (__i686__) && !defined (__pentium4__) \
  && !defined (__k6__) && !defined (__athlon__) && !defined (__k8__)
#  if !defined (__i486__)
    /* Check if CPUID is supported by setting ID flag bit 21. */
    unsigned int after, before;
    asm ("pushf\n\t"
         "pop %0\n\t"
         "movl %0, %1\n\t"
         "xorl $0x200000, %0\n\t"
         "push %0\n\t"
         "popf\n\t"
         "pushf\n\t"
         "pop %0\n\t"
         : "=&r" (after), "=&r" (before)
         :
         : "cc");

    if( after == before )
        goto out;
# endif

    /* the CPU supports the CPUID instruction - get its level */
    cpuid( 0x00000000 );

    if( !i_eax )
        goto out;
#endif

    cpuid( 0x00000001 );

    if (i_edx & 0x04000000)
        i_capabilities |= VLC_CPU_SSE2;
    if (i_ecx & 0x00000001)
        i_capabilities |= VLC_CPU_SSE3;
    if (i_ecx & 0x00000200)
        i_capabilities |= VLC_CPU_SSSE3;
    if (i_ecx & 0x00080000)
        i_capabilities |= VLC_CPU_SSE4_1;

    /* test for additional capabilities */
    cpuid( 0x80000000 );

    if( i_eax < 0x80000001 )
        goto out;

    /* list these additional capabilities */
    cpuid( 0x80000001 );

out:

#elif defined( __powerpc__ ) || defined( __ppc__ ) || defined( __powerpc64__ ) \
    || defined( __ppc64__ )

#   if defined(__OpenBSD__)
    int selectors[2] = { CTL_MACHDEP, CPU_ALTIVEC };
    int i_has_altivec = 0;
    size_t i_length = sizeof( i_has_altivec );
    int i_error = sysctl( selectors, 2, &i_has_altivec, &i_length, NULL, 0);

    if( i_error == 0 && i_has_altivec != 0 )
        i_capabilities |= VLC_CPU_ALTIVEC;
#   endif

#endif
    return i_capabilities;
}

unsigned vlc_CPU(void)
{
    static atomic_uint cpu_flags = ATOMIC_VAR_INIT(-1U);
    unsigned flags = atomic_load_explicit(&cpu_flags, memory_order_relaxed);

    if (unlikely(flags == -1U)) {
        flags = vlc_CPU_raw();
        atomic_store_explicit(&cpu_flags, flags, memory_order_relaxed);
    }

    return flags;
}

void vlc_CPU_dump (vlc_object_t *obj)
{
    struct vlc_memstream stream;

    vlc_memstream_open(&stream);

#if defined (__i386__) || defined (__x86_64__)
    if (vlc_CPU_SSE2())
        vlc_memstream_puts(&stream, "SSE2 ");
    if (vlc_CPU_SSE3())
        vlc_memstream_puts(&stream, "SSE3 ");
    if (vlc_CPU_SSSE3())
        vlc_memstream_puts(&stream, "SSSE3 ");
    if (vlc_CPU_SSE4_1())
        vlc_memstream_puts(&stream, "SSE4.1 ");
    if (vlc_CPU_AVX())
        vlc_memstream_puts(&stream, "AVX ");
    if (vlc_CPU_AVX2())
        vlc_memstream_puts(&stream, "AVX2 ");

#elif defined (__powerpc__) || defined (__ppc__) || defined (__ppc64__)
    if (vlc_CPU_ALTIVEC())
        vlc_memstream_puts(&stream, "AltiVec");

#elif defined (__arm__)
    if (vlc_CPU_ARM_NEON())
        vlc_memstream_puts(&stream, "ARM_NEON ");

#elif defined (__riscv)
    if (vlc_CPU_RV_V())
        vlc_memstream_puts(&stream, "V ");

#endif

#if HAVE_FPU
    vlc_memstream_puts(&stream, "FPU ");
#endif

    if (vlc_memstream_close(&stream) == 0)
    {
        msg_Dbg (obj, "CPU has capabilities %s", stream.ptr);
        free(stream.ptr);
    }
}

void vlc_CPU_functions_init(const char *capability, void *restrict funcs)
{
    module_t **mods;
    ssize_t n = vlc_module_match(capability, NULL, false, &mods, NULL);

    /* Descending order so higher priorities override the lower ones */
    for (ssize_t i = n - 1; i >= 0; i--) {
        void (*init)(void *) = vlc_module_map(NULL, mods[i]);
        if (likely(init != NULL))
            init(funcs);
    }

    free(mods);
}
