/*****************************************************************************
 * cpu.c: CPU detection code
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.eduEujenio>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#ifdef HAVE_SIGNAL_H
#   include <signal.h>                            /* SIGHUP, SIGINT, SIGKILL */
#   include <setjmp.h>                                    /* longjmp, setjmp */
#endif

#include "libvlc.h"

#if defined(__APPLE__) && (defined(__ppc__) || defined(__ppc64__))
#include <sys/sysctl.h>
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_SIGNAL_H
static void SigHandler   ( int );
#endif

/*****************************************************************************
 * Global variables - they're needed for signal handling
 *****************************************************************************/
#ifdef HAVE_SIGNAL_H
static jmp_buf env;
static int     i_illegal;
#if defined( __i386__ ) || defined( __x86_64__ )
static const char *psz_capability;
#endif
#endif

/*****************************************************************************
 * CPUCapabilities: get the CPU capabilities
 *****************************************************************************
 * This function is called to list extensions the CPU may have.
 *****************************************************************************/
uint32_t CPUCapabilities( void )
{
    volatile uint32_t i_capabilities = CPU_CAPABILITY_NONE;

#if defined(__APPLE__) && (defined(__ppc__) || defined(__ppc64__))
    int selectors[2] = { CTL_HW, HW_VECTORUNIT };
    int i_has_altivec = 0;
    size_t i_length = sizeof( i_has_altivec );
    int i_error = sysctl( selectors, 2, &i_has_altivec, &i_length, NULL, 0);

    i_capabilities |= CPU_CAPABILITY_FPU;

    if( i_error == 0 && i_has_altivec != 0 )
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;

    return i_capabilities;

#elif defined( __i386__ ) || defined( __x86_64__ )
    volatile unsigned int  i_eax, i_ebx, i_ecx, i_edx;
    volatile bool    b_amd;

    /* Needed for x86 CPU capabilities detection */
#   if defined( __x86_64__ )
#       define cpuid( reg )                    \
            asm volatile ( "cpuid\n\t"         \
                           "movl %%ebx,%1\n\t" \
                         : "=a" ( i_eax ),     \
                           "=b" ( i_ebx ),     \
                           "=c" ( i_ecx ),     \
                           "=d" ( i_edx )      \
                         : "a"  ( reg )        \
                         : "cc" );
#   else
#       define cpuid( reg )                    \
            asm volatile ( "push %%ebx\n\t"    \
                           "cpuid\n\t"         \
                           "movl %%ebx,%1\n\t" \
                           "pop %%ebx\n\t"     \
                         : "=a" ( i_eax ),     \
                           "=r" ( i_ebx ),     \
                           "=c" ( i_ecx ),     \
                           "=d" ( i_edx )      \
                         : "a"  ( reg )        \
                         : "cc" );
#   endif

#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW ) \
     && defined( HAVE_SIGNAL_H )
    void (*pf_sigill) (int) = signal( SIGILL, SigHandler );
#   endif

    i_capabilities |= CPU_CAPABILITY_FPU;

#   if defined( __i386__ )
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
    {
#       if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW ) \
            && defined( HAVE_SIGNAL_H )
        signal( SIGILL, pf_sigill );
#       endif
        return i_capabilities;
    }
#   else
    /* x86_64 supports cpuid instruction, so we dont need to check it */
#   endif

    i_capabilities |= CPU_CAPABILITY_486;

    /* the CPU supports the CPUID instruction - get its level */
    cpuid( 0x00000000 );

    if( !i_eax )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW ) \
     && defined( HAVE_SIGNAL_H )
        signal( SIGILL, pf_sigill );
#   endif
        return i_capabilities;
    }

    /* FIXME: this isn't correct, since some 486s have cpuid */
    i_capabilities |= CPU_CAPABILITY_586;

    /* borrowed from mpeg2dec */
    b_amd = ( i_ebx == 0x68747541 ) && ( i_ecx == 0x444d4163 )
                    && ( i_edx == 0x69746e65 );

    /* test for the MMX flag */
    cpuid( 0x00000001 );

    if( ! (i_edx & 0x00800000) )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW ) \
     && defined( HAVE_SIGNAL_H )
        signal( SIGILL, pf_sigill );
#   endif
        return i_capabilities;
    }

    i_capabilities |= CPU_CAPABILITY_MMX;

    if( i_edx & 0x02000000 )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;

#   ifdef CAN_COMPILE_SSE
        /* We test if OS supports the SSE instructions */
        psz_capability = "SSE";
        i_illegal = 0;

        if( setjmp( env ) == 0 )
        {
            /* Test a SSE instruction */
            __asm__ __volatile__ ( "xorps %%xmm0,%%xmm0\n" : : );
        }

        if( i_illegal == 0 )
        {
            i_capabilities |= CPU_CAPABILITY_SSE;
        }
#   endif
    }

    if( i_edx & 0x04000000 )
    {
#   if defined(CAN_COMPILE_SSE)
        /* We test if OS supports the SSE instructions */
        psz_capability = "SSE2";
        i_illegal = 0;

        if( setjmp( env ) == 0 )
        {
            /* Test a SSE2 instruction */
            __asm__ __volatile__ ( "movupd %%xmm0, %%xmm0\n" : : );
        }

        if( i_illegal == 0 )
        {
            i_capabilities |= CPU_CAPABILITY_SSE2;
        }
#   endif
    }

    /* test for additional capabilities */
    cpuid( 0x80000000 );

    if( i_eax < 0x80000001 )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW ) \
     && defined( HAVE_SIGNAL_H )
        signal( SIGILL, pf_sigill );
#   endif
        return i_capabilities;
    }

    /* list these additional capabilities */
    cpuid( 0x80000001 );

#   ifdef CAN_COMPILE_3DNOW
    if( i_edx & 0x80000000 )
    {
        psz_capability = "3D Now!";
        i_illegal = 0;

        if( setjmp( env ) == 0 )
        {
            /* Test a 3D Now! instruction */
            __asm__ __volatile__ ( "pfadd %%mm0,%%mm0\n" "femms\n" : : );
        }

        if( i_illegal == 0 )
        {
            i_capabilities |= CPU_CAPABILITY_3DNOW;
        }
    }
#   endif

    if( b_amd && ( i_edx & 0x00400000 ) )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;
    }

#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW ) \
     && defined( HAVE_SIGNAL_H )
    signal( SIGILL, pf_sigill );
#   endif
    return i_capabilities;

#elif defined( __powerpc__ ) || defined( __ppc__ ) || defined( __ppc64__ )

#   ifdef CAN_COMPILE_ALTIVEC && defined( HAVE_SIGNAL_H )
    void (*pf_sigill) (int) = signal( SIGILL, SigHandler );

    i_capabilities |= CPU_CAPABILITY_FPU;

    i_illegal = 0;

    if( setjmp( env ) == 0 )
    {
        asm volatile ("mtspr 256, %0\n\t"
                      "vand %%v0, %%v0, %%v0"
                      :
                      : "r" (-1));
    }

    if( i_illegal == 0 )
    {
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;
    }

    signal( SIGILL, pf_sigill );
#   else
    (void)SigHandler; /* Don't complain about dead code here */
#   endif

    return i_capabilities;

#elif defined( __sparc__ )

    i_capabilities |= CPU_CAPABILITY_FPU;
    return i_capabilities;

#elif defined( _MSC_VER ) && !defined( UNDER_CE )
    i_capabilities |= CPU_CAPABILITY_FPU;
    return i_capabilities;

#else
    /* default behaviour */
    return i_capabilities;

#endif
}

/*****************************************************************************
 * SigHandler: system signal handler
 *****************************************************************************
 * This function is called when an illegal instruction signal is received by
 * the program. We use this function to test OS and CPU capabilities
 *****************************************************************************/
#if defined( HAVE_SIGNAL_H )
static void SigHandler( int i_signal )
{
    /* Acknowledge the signal received */
    i_illegal = 1;

#ifdef HAVE_SIGRELSE
    sigrelse( i_signal );
#else
    VLC_UNUSED( i_signal );
#endif

#if defined( __i386__ )
    fprintf( stderr, "warning: your CPU has %s instructions, but not your "
                     "operating system.\n", psz_capability );
    fprintf( stderr, "         some optimizations will be disabled unless "
                     "you upgrade your OS\n" );
#   if defined( __linux__ )
    fprintf( stderr, "         (for instance Linux kernel 2.4.x or later)\n" );
#   endif
#endif

    longjmp( env, 1 );
}
#endif


uint32_t cpu_flags = 0;


/*****************************************************************************
 * vlc_CPU: get pre-computed CPU capability flags
 ****************************************************************************/
unsigned vlc_CPU (void)
{
    return cpu_flags;
}

static vlc_memcpy_t pf_vlc_memcpy = memcpy;
static vlc_memset_t pf_vlc_memset = memset;

void vlc_fastmem_register (vlc_memcpy_t cpy, vlc_memset_t set)
{
    if (cpy)
        pf_vlc_memcpy = cpy;
    if (set)
        pf_vlc_memset = set;
}

/**
 * vlc_memcpy: fast CPU-dependent memcpy
 */
void *vlc_memcpy (void *tgt, const void *src, size_t n)
{
    return pf_vlc_memcpy (tgt, src, n);
}

/**
 * vlc_memset: fast CPU-dependent memset
 */
void *vlc_memset (void *tgt, int c, size_t n)
{
    return pf_vlc_memset (tgt, c, n);
}
