/*****************************************************************************
 * cpu.c: CPU detection code
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: cpu.c,v 1.10 2002/12/06 16:34:08 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>

#ifdef HAVE_SIGNAL_H
#   include <signal.h>                            /* SIGHUP, SIGINT, SIGKILL */
#   include <setjmp.h>                                    /* longjmp, setjmp */
#endif

#ifdef SYS_DARWIN
#   include <mach/mach.h>                               /* AltiVec detection */
#   include <mach/mach_error.h>       /* some day the header files||compiler *
                                                       will define it for us */
#   include <mach/bootstrap.h>
#endif

#include "vlc_cpu.h"

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
#if defined( __i386__ )
static char   *psz_capability;
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

#if defined( SYS_DARWIN )
    struct host_basic_info hi;
    kern_return_t          ret;
    host_name_port_t       host;

    int i_size;
    char *psz_name, *psz_subname;

    i_capabilities |= CPU_CAPABILITY_FPU;

    /* Should 'never' fail? */
    host = mach_host_self();

    i_size = sizeof( hi ) / sizeof( int );
    ret = host_info( host, HOST_BASIC_INFO, ( host_info_t )&hi, &i_size );

    if( ret != KERN_SUCCESS )
    {
        fprintf( stderr, "error: couldn't get CPU information\n" );
        return i_capabilities;
    }

    slot_name( hi.cpu_type, hi.cpu_subtype, &psz_name, &psz_subname );
    /* FIXME: need better way to detect newer proccessors.
     * could do strncmp(a,b,5), but that's real ugly */
    if( !strcmp(psz_name, "ppc7400") || !strcmp(psz_name, "ppc7450") )
    {
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;
    }

    return i_capabilities;

#elif defined( __i386__ )
    volatile unsigned int  i_eax, i_ebx, i_ecx, i_edx;
    volatile vlc_bool_t    b_amd;

    /* Needed for x86 CPU capabilities detection */
#   define cpuid( reg )                    \
        asm volatile ( "pushl %%ebx\n\t"   \
                       "cpuid\n\t"         \
                       "movl %%ebx,%1\n\t" \
                       "popl %%ebx\n\t"    \
                     : "=a" ( i_eax ),     \
                       "=r" ( i_ebx ),     \
                       "=c" ( i_ecx ),     \
                       "=d" ( i_edx )      \
                     : "a"  ( reg )        \
                     : "cc" );

#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW ) \
     && defined( HAVE_SIGNAL_H )
    void (*pf_sigill) (int) = signal( SIGILL, SigHandler );
#   endif

    i_capabilities |= CPU_CAPABILITY_FPU;

    /* test for a 486 CPU */
    asm volatile ( "pushl %%ebx\n\t"
                   "pushfl\n\t"
                   "popl %%eax\n\t"
                   "movl %%eax, %%ebx\n\t"
                   "xorl $0x200000, %%eax\n\t"
                   "pushl %%eax\n\t"
                   "popfl\n\t"
                   "pushfl\n\t"
                   "popl %%eax\n\t"
                   "movl %%ebx,%1\n\t"
                   "popl %%ebx\n\t"
                 : "=a" ( i_eax ),
                   "=r" ( i_ebx )
                 :
                 : "cc" );

    if( i_eax == i_ebx )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW ) \
     && defined( HAVE_SIGNAL_H )
        signal( SIGILL, pf_sigill );
#   endif
        return i_capabilities;
    }

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

#elif defined( __powerpc__ )

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
#   endif

    return i_capabilities;

#elif defined( __sparc__ )

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
#endif

#if defined( __i386__ )
    fprintf( stderr, "warning: your CPU has %s instructions, but not your "
                     "operating system.\n", psz_capability );
    fprintf( stderr, "         some optimizations will be disabled unless "
                     "you upgrade your OS\n" );
#   if defined( SYS_LINUX )
    fprintf( stderr, "         (for instance Linux kernel 2.4.x or later)\n" );
#   endif
#endif

    longjmp( env, 1 );
}
#endif

