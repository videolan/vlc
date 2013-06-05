/*****************************************************************************
 * cpu.c: CPU detection code
 *****************************************************************************
 * Copyright (C) 1998-2004 VLC authors and VideoLAN
 * $Id$
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

#include <vlc_common.h>
#include <vlc_cpu.h>
#include "libvlc.h"

#include <assert.h>

#ifndef __linux__
#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#else
#include <errno.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#ifdef __ANDROID__
#include <cpu-features.h>
#endif

#if defined(__OpenBSD__) && defined(__powerpc__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#endif

static uint32_t cpu_flags;

#if defined (__i386__) || defined (__x86_64__) || defined (__powerpc__) \
 || defined (__ppc__) || defined (__ppc64__) || defined (__powerpc64__)
# if !defined (_WIN32) && !defined (__OS2__)
static bool vlc_CPU_check (const char *name, void (*func) (void))
{
    pid_t pid = fork();

    switch (pid)
    {
        case 0:
            signal (SIGILL, SIG_DFL);
            func ();
            //__asm__ __volatile__ ( code : : input );
            _exit (0);
        case -1:
            return false;
    }
    //i_capabilities |= (flag);

    int status;
    while( waitpid( pid, &status, 0 ) == -1 );

    if( WIFEXITED( status ) && WEXITSTATUS( status ) == 0 )
        return true;

    fprintf (stderr, "Warning: your CPU has %s instructions, but not your "
                     "operating system.\n", name);
    fprintf( stderr, "         some optimizations will be disabled unless "
                     "you upgrade your OS\n" );
    return false;
}

#if defined (CAN_COMPILE_SSE) && !defined (__SSE__)
VLC_SSE static void SSE_test (void)
{
    asm volatile ("xorps %%xmm0,%%xmm0\n" : : : "xmm0", "xmm1");
}
#endif
#if defined (CAN_COMPILE_3DNOW)
VLC_MMX static void ThreeD_Now_test (void)
{
    asm volatile ("pfadd %%mm0,%%mm0\n" "femms\n" : : : "mm0");
}
#endif

#if defined (CAN_COMPILE_ALTIVEC)
static void Altivec_test (void)
{
    asm volatile ("mtspr 256, %0\n" "vand %%v0, %%v0, %%v0\n" : : "r" (-1));
}
#endif

#else /* _WIN32 || __OS2__ */
# define vlc_CPU_check(name, func) (1)
#endif
#endif

/**
 * Determines the CPU capabilities and stores them in cpu_flags.
 * The result can be retrieved with vlc_CPU().
 */
void vlc_CPU_init (void)
{
    uint32_t i_capabilities = 0;

#if defined( __i386__ ) || defined( __x86_64__ )
     unsigned int i_eax, i_ebx, i_ecx, i_edx;
     bool b_amd;

    /* Needed for x86 CPU capabilities detection */
# if defined (__i386__) && defined (__PIC__)
#  define cpuid(reg) \
     asm volatile ("xchgl %%ebx,%1\n\t" \
                   "cpuid\n\t" \
                   "xchgl %%ebx,%1\n\t" \
                   : "=a" (i_eax), "=r" (i_ebx), "=c" (i_ecx), "=d" (i_edx) \
                   : "a" (reg) \
                   : "cc");
# else
#  define cpuid(reg) \
     asm volatile ("cpuid\n\t" \
                   : "=a" (i_eax), "=b" (i_ebx), "=c" (i_ecx), "=d" (i_edx) \
                   : "a" (reg) \
                   : "cc");
# endif
     /* Check if the OS really supports the requested instructions */
# if defined (__i386__) && !defined (__i486__) && !defined (__i586__) \
  && !defined (__i686__) && !defined (__pentium4__) \
  && !defined (__k6__) && !defined (__athlon__) && !defined (__k8__)
    /* check if cpuid instruction is supported */
    asm volatile ( "push %%ebx\n\t"
                   "pushf\n\t"
                   "pop %%eax\n\t"
                   "movl %%eax, %%ebx\n\t"
                   "xorl $0x200000, %%eax\n\t"
                   "push %%eax\n\t"
                   "popf\n\t"
                   "pushf\n\t"
                   "pop %%eax\n\t"
                   "movl %%ebx,%1\n\t"
                   "pop %%ebx\n\t"
                 : "=a" ( i_eax ),
                   "=r" ( i_ebx )
                 :
                 : "cc" );

    if( i_eax == i_ebx )
        goto out;
# endif

    /* the CPU supports the CPUID instruction - get its level */
    cpuid( 0x00000000 );

# if defined (__i386__) && !defined (__i586__) \
  && !defined (__i686__) && !defined (__pentium4__) \
  && !defined (__k6__) && !defined (__athlon__) && !defined (__k8__)
    if( !i_eax )
        goto out;
#endif

    /* borrowed from mpeg2dec */
    b_amd = ( i_ebx == 0x68747541 ) && ( i_ecx == 0x444d4163 )
                    && ( i_edx == 0x69746e65 );

    /* test for the MMX flag */
    cpuid( 0x00000001 );
# if !defined (__MMX__)
    if( ! (i_edx & 0x00800000) )
        goto out;
# endif
    i_capabilities |= VLC_CPU_MMX;

    if( i_edx & 0x02000000 )
        i_capabilities |= VLC_CPU_MMXEXT;
# if defined (CAN_COMPILE_SSE) && !defined (__SSE__)
    if (( i_edx & 0x02000000 ) && vlc_CPU_check ("SSE", SSE_test))
# endif
    {
        /*if( i_edx & 0x02000000 )*/
            i_capabilities |= VLC_CPU_SSE;
        if (i_edx & 0x04000000)
            i_capabilities |= VLC_CPU_SSE2;
        if (i_ecx & 0x00000001)
            i_capabilities |= VLC_CPU_SSE3;
        if (i_ecx & 0x00000200)
            i_capabilities |= VLC_CPU_SSSE3;
        if (i_ecx & 0x00080000)
            i_capabilities |= VLC_CPU_SSE4_1;
        if (i_ecx & 0x00100000)
            i_capabilities |= VLC_CPU_SSE4_2;
    }

    /* test for additional capabilities */
    cpuid( 0x80000000 );

    if( i_eax < 0x80000001 )
        goto out;

    /* list these additional capabilities */
    cpuid( 0x80000001 );

# if defined (CAN_COMPILE_3DNOW) && !defined (__3dNOW__)
    if ((i_edx & 0x80000000) && vlc_CPU_check ("3D Now!", ThreeD_Now_test))
# endif
        i_capabilities |= VLC_CPU_3dNOW;

    if( b_amd && ( i_edx & 0x00400000 ) )
        i_capabilities |= VLC_CPU_MMXEXT;
out:

#elif defined( __powerpc__ ) || defined( __ppc__ ) || defined( __powerpc64__ ) \
    || defined( __ppc64__ )

#   if defined(__APPLE__) || defined(__OpenBSD__)
#   if defined(__OpenBSD__)
    int selectors[2] = { CTL_MACHDEP, CPU_ALTIVEC };
#   else
    int selectors[2] = { CTL_HW, HW_VECTORUNIT };
#   endif
    int i_has_altivec = 0;
    size_t i_length = sizeof( i_has_altivec );
    int i_error = sysctl( selectors, 2, &i_has_altivec, &i_length, NULL, 0);

    if( i_error == 0 && i_has_altivec != 0 )
        i_capabilities |= VLC_CPU_ALTIVEC;

#   elif defined( CAN_COMPILE_ALTIVEC )
    if (vlc_CPU_check ("Altivec", Altivec_test))
        i_capabilities |= VLC_CPU_ALTIVEC;

#   endif

#elif defined ( __arm__)
# ifdef __ANDROID__
    if (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON)
        i_capabilities |= VLC_CPU_ARM_NEON;
# endif

#endif

    cpu_flags = i_capabilities;
}

/**
 * Retrieves pre-computed CPU capability flags
 */
unsigned vlc_CPU (void)
{
/* On Windows and OS/2,
 * initialized from DllMain() and _DLL_InitTerm() respectively, instead */
#if !defined(_WIN32) && !defined(__OS2__)
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once (&once, vlc_CPU_init);
#endif
    return cpu_flags;
}
#endif

void vlc_CPU_dump (vlc_object_t *obj)
{
    char buf[200], *p = buf;

#if defined (__i386__) || defined (__x86_64__)
    if (vlc_CPU_MMX()) p += sprintf (p, "MMX ");
    if (vlc_CPU_MMXEXT()) p += sprintf (p, "MMXEXT ");
    if (vlc_CPU_SSE()) p += sprintf (p, "SSE ");
    if (vlc_CPU_SSE2()) p += sprintf (p, "SSE2 ");
    if (vlc_CPU_SSE3()) p += sprintf (p, "SSE3 ");
    if (vlc_CPU_SSSE3()) p += sprintf (p, "SSSE3 ");
    if (vlc_CPU_SSE4_1()) p += sprintf (p, "SSE4.1 ");
    if (vlc_CPU_SSE4_2()) p += sprintf (p, "SSE4.2 ");
    if (vlc_CPU_SSE4A()) p += sprintf (p, "SSE4A ");
    if (vlc_CPU_AVX()) p += sprintf (p, "AVX ");
    if (vlc_CPU_AVX2()) p += sprintf (p, "AVX ");
    if (vlc_CPU_3dNOW()) p += sprintf (p, "3DNow! ");
    if (vlc_CPU_XOP()) p += sprintf (p, "XOP ");
    if (vlc_CPU_FMA4()) p += sprintf (p, "FMA4 ");

#elif defined (__powerpc__) || defined (__ppc__) || defined (__ppc64__)
    if (vlc_CPU_ALTIVEC())  p += sprintf (p, "AltiVec");

#elif defined (__arm__)
    if (vlc_CPU_ARM_NEON()) p += sprintf (p, "ARM_NEON ");

#endif

#if HAVE_FPU
    p += sprintf (p, "FPU ");
#endif

    if (p > buf)
        msg_Dbg (obj, "CPU has capabilities %s", buf);
}
