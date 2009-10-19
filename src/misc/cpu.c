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
#include <vlc_cpu.h>

#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "libvlc.h"

#if defined(__APPLE__) && (defined(__ppc__) || defined(__ppc64__))
#include <sys/sysctl.h>
#endif

#if defined( __i386__ ) || defined( __x86_64__ ) || defined( __powerpc__ ) \
 || defined( __ppc__ ) || defined( __ppc64__ ) || defined( __powerpc64__ )
static bool check_OS_capability( const char *psz_capability, pid_t pid )
{
#ifndef WIN32
    int status;

    if( pid == -1 )
        return false; /* fail safe :-/ */

    while( waitpid( pid, &status, 0 ) == -1 );

    if( WIFEXITED( status ) && WEXITSTATUS( status ) == 0 )
        return true;

    fprintf( stderr, "warning: your CPU has %s instructions, but not your "
                     "operating system.\n", psz_capability );
    fprintf( stderr, "         some optimizations will be disabled unless "
                     "you upgrade your OS\n" );
    return false;
#else
# warning FIXME!
# define fork() (errno = ENOSYS, -1)
    (void)pid;
    (void)psz_capability;
    return true;
#endif
}
#endif

/*****************************************************************************
 * CPUCapabilities: get the CPU capabilities
 *****************************************************************************
 * This function is called to list extensions the CPU may have.
 *****************************************************************************/
uint32_t CPUCapabilities( void )
{
    uint32_t i_capabilities = 0;

#if defined( __i386__ ) || defined( __x86_64__ )
     unsigned int i_eax, i_ebx, i_ecx, i_edx;
     bool b_amd;

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
    i_capabilities |= CPU_CAPABILITY_MMX;

# if defined (__SSE__)
    i_capabilities |= CPU_CAPABILITY_MMXEXT | CPU_CAPABILITY_SSE;
# else
    if( i_edx & 0x02000000 )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;

#   ifdef CAN_COMPILE_SSE
        /* We test if OS supports the SSE instructions */
        pid_t pid = fork();
        if( pid == 0 )
        {
            /* Test a SSE instruction */
            __asm__ __volatile__ ( "xorps %%xmm0,%%xmm0\n" : : );
            exit(0);
        }
        if( check_OS_capability( "SSE", pid ) )
            i_capabilities |= CPU_CAPABILITY_SSE;
#   endif
    }
# endif

# if defined (__SSE2__)
    i_capabilities |= CPU_CAPABILITY_SSE2;
# elif defined (CAN_COMPILE_SSE)
    if( i_edx & 0x04000000 )
    {
        /* We test if OS supports the SSE2 instructions */
        pid_t pid = fork();
        if( pid == 0 )
        {
            /* Test a SSE2 instruction */
            __asm__ __volatile__ ( "movupd %%xmm0, %%xmm0\n" : : );
            exit(0);
        }
        if( check_OS_capability( "SSE2", pid ) )
            i_capabilities |= CPU_CAPABILITY_SSE2;
    }
# endif

# if defined (__SSE3__)
    i_capabilities |= CPU_CAPABILITY_SSE3;
# elif defined (CAN_COMPILE_SSE3)
    if( i_ecx & 0x00000001 )
    {
        /* We test if OS supports the SSE3 instructions */
        pid_t pid = fork();
        if( pid == 0 )
        {
            /* Test a SSE3 instruction */
            __asm__ __volatile__ ( "movsldup %%xmm1, %%xmm0\n" : : );
            exit(0);
        }
        if( check_OS_capability( "SSE3", pid ) )
            i_capabilities |= CPU_CAPABILITY_SSE3;
    }
# endif

    /* test for additional capabilities */
    cpuid( 0x80000000 );

    if( i_eax < 0x80000001 )
        goto out;

    /* list these additional capabilities */
    cpuid( 0x80000001 );

# if defined (__3dNOW__)
    i_capabilities |= CPU_CAPABILITY_3DNOW;
# elif defined (CAN_COMPILE_3DNOW)
    if( i_edx & 0x80000000 )
    {
        pid_t pid = fork();
        if( pid == 0 )
        {
            /* Test a 3D Now! instruction */
            __asm__ __volatile__ ( "pfadd %%mm0,%%mm0\n" "femms\n" : : );
            exit(0);
        }
        if( check_OS_capability( "3D Now!", pid ) )
            i_capabilities |= CPU_CAPABILITY_3DNOW;
    }
# endif

    if( b_amd && ( i_edx & 0x00400000 ) )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;
    }
out:

#elif defined( __arm__ )
#   if defined( __ARM_NEON__ )
    i_capabilities |= CPU_CAPABILITY_NEON;
#   endif

#elif defined( __powerpc__ ) || defined( __ppc__ ) || defined( __powerpc64__ ) \
    || defined( __ppc64__ )

#   if defined(__APPLE__)
    int selectors[2] = { CTL_HW, HW_VECTORUNIT };
    int i_has_altivec = 0;
    size_t i_length = sizeof( i_has_altivec );
    int i_error = sysctl( selectors, 2, &i_has_altivec, &i_length, NULL, 0);

    if( i_error == 0 && i_has_altivec != 0 )
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;

#   elif defined( CAN_COMPILE_ALTIVEC )
    pid_t pid = fork();
    if( pid == 0 )
    {
        asm volatile ("mtspr 256, %0\n\t"
                      "vand %%v0, %%v0, %%v0"
                      :
                      : "r" (-1));
        exit(0);
    }

    if( check_OS_capability( "Altivec", pid ) )
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;

#   endif

#endif
    return i_capabilities;
}

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
