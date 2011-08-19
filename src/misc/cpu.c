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
#include <signal.h>
#else
#include <errno.h>
#endif
#include <assert.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#include "libvlc.h"

static uint32_t cpu_flags;

#if defined( __i386__ ) || defined( __x86_64__ ) || defined( __powerpc__ ) \
 || defined( __ppc__ ) || defined( __ppc64__ ) || defined( __powerpc64__ )
# ifndef WIN32
static bool check_OS_capability( const char *psz_capability, pid_t pid )
{
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
}

#  define check_capability(name, flag, code)   \
     do {                                      \
        pid_t pid = fork();                    \
        if( pid == 0 )                         \
        {                                      \
            signal(SIGILL, SIG_DFL);           \
            __asm__ __volatile__ ( code : : ); \
            _exit(0);                          \
        }                                      \
        if( check_OS_capability((name), pid )) \
            i_capabilities |= (flag);          \
     } while(0)

# else /* WIN32 */
#  define check_capability(name, flag, code)   \
        i_capabilities |= (flag);
# endif
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
    i_capabilities |= CPU_CAPABILITY_MMX;

# if defined (__SSE__)
    i_capabilities |= CPU_CAPABILITY_MMXEXT | CPU_CAPABILITY_SSE;
# else
    if( i_edx & 0x02000000 )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;

#   ifdef CAN_COMPILE_SSE
        check_capability( "SSE", CPU_CAPABILITY_SSE,
                          "xorps %%xmm0,%%xmm0\n" );
#   endif
    }
# endif

# if defined (__SSE2__)
    i_capabilities |= CPU_CAPABILITY_SSE2;
# elif defined (CAN_COMPILE_SSE2)
    if( i_edx & 0x04000000 )
        check_capability( "SSE2", CPU_CAPABILITY_SSE2,
                          "movupd %%xmm0, %%xmm0\n" );
# endif

# if defined (__SSE3__)
    i_capabilities |= CPU_CAPABILITY_SSE3;
# elif defined (CAN_COMPILE_SSE3)
    if( i_ecx & 0x00000001 )
        check_capability( "SSE3", CPU_CAPABILITY_SSE3,
                          "movsldup %%xmm1, %%xmm0\n" );
# endif

# if defined (__SSSE3__)
    i_capabilities |= CPU_CAPABILITY_SSSE3;
# elif defined (CAN_COMPILE_SSSE3)
    if( i_ecx & 0x00000200 )
        check_capability( "SSSE3", CPU_CAPABILITY_SSSE3,
                          "pabsw %%xmm1, %%xmm0\n" );
# endif

# if defined (__SSE4_1__)
    i_capabilities |= CPU_CAPABILITY_SSE4_1;
# elif defined (CAN_COMPILE_SSE4_1)
    if( i_ecx & 0x00080000 )
        check_capability( "SSE4.1", CPU_CAPABILITY_SSE4_1,
                          "pmaxsb %%xmm1, %%xmm0\n" );
# endif

# if defined (__SSE4_2__)
    i_capabilities |= CPU_CAPABILITY_SSE4_2;
# elif defined (CAN_COMPILE_SSE4_2)
    if( i_ecx & 0x00100000 )
        check_capability( "SSE4.2", CPU_CAPABILITY_SSE4_2,
                          "pcmpgtq %%xmm1, %%xmm0\n" );
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
        check_capability( "3D Now!", CPU_CAPABILITY_3DNOW,
                          "pfadd %%mm0,%%mm0\n" "femms\n" );
# endif

    if( b_amd && ( i_edx & 0x00400000 ) )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;
    }
out:

#elif defined (__arm__)

# if defined (__ARM_NEON__)
    i_capabilities |= CPU_CAPABILITY_NEON;
# elif defined (CAN_COMPILE_NEON)
#  define NEED_RUNTIME_CPU_CHECK 1
# endif

# ifdef NEED_RUNTIME_CPU_CHECK
#  if defined (__linux__)
    FILE *info = fopen ("/proc/cpuinfo", "rt");
    if (info != NULL)
    {
        char *line = NULL;
        size_t linelen = 0;

        while (getline (&line, &linelen, info) != -1)
        {
             const char *cap;

             if (strncmp (line, "Features\t:", 10))
                 continue;
#   if defined (CAN_COMPILE_NEON) && !defined (__ARM_NEON__)
             cap = strstr (line + 10, " neon");
             if (cap != NULL && (cap[5] == '\0' || cap[5] == ' '))
                 i_capabilities |= CPU_CAPABILITY_NEON;
#   endif
             break;
        }
        fclose (info);
        free (line);
    }
#  else
#   warning Run-time CPU detection missing: optimizations disabled!
#  endif
# endif

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
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;

#   elif defined( CAN_COMPILE_ALTIVEC )
    pid_t pid = fork();
    if( pid == 0 )
    {
        signal(SIGILL, SIG_DFL);
        asm volatile ("mtspr 256, %0\n\t"
                      "vand %%v0, %%v0, %%v0"
                      :
                      : "r" (-1));
        _exit(0);
    }

    if( check_OS_capability( "Altivec", pid ) )
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;

#   endif

#endif

    cpu_flags = i_capabilities;
}

/**
 * Retrieves pre-computed CPU capability flags
 */
unsigned vlc_CPU (void)
{
#ifndef WIN32 /* On Windows, initialized from DllMain() instead */
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once (&once, vlc_CPU_init);
#endif
    return cpu_flags;
}

void vlc_CPU_dump (vlc_object_t *obj)
{
    const unsigned flags = vlc_CPU();
    char buf[200], *p = buf;

#define PRINT_CAPABILITY( capability, string ) \
    if (flags & (capability)) \
        p += sprintf (p, "%s ", (string) )

#if defined (__i386__) || defined (__x86_64__)
    PRINT_CAPABILITY(CPU_CAPABILITY_MMX, "MMX");
    PRINT_CAPABILITY(CPU_CAPABILITY_3DNOW, "3DNow!");
    PRINT_CAPABILITY(CPU_CAPABILITY_MMXEXT, "MMXEXT");
    PRINT_CAPABILITY(CPU_CAPABILITY_SSE, "SSE");
    PRINT_CAPABILITY(CPU_CAPABILITY_SSE2, "SSE2");
    PRINT_CAPABILITY(CPU_CAPABILITY_SSE3, "SSE3");
    PRINT_CAPABILITY(CPU_CAPABILITY_SSSE3, "SSSE3");
    PRINT_CAPABILITY(CPU_CAPABILITY_SSE4_1, "SSE4.1");
    PRINT_CAPABILITY(CPU_CAPABILITY_SSE4_2, "SSE4.2");
    PRINT_CAPABILITY(CPU_CAPABILITY_SSE4A,  "SSE4A");

#elif defined (__powerpc__) || defined (__ppc__) || defined (__ppc64__)
    PRINT_CAPABILITY(CPU_CAPABILITY_ALTIVEC, "AltiVec");

#elif defined (__arm__)
    PRINT_CAPABILITY(CPU_CAPABILITY_NEON, "NEONv1");

#endif

#if HAVE_FPU
    p += sprintf (p, "FPU ");
#endif

    if (p > buf)
        msg_Dbg (obj, "CPU has capabilities %s", buf);
}


static vlc_memcpy_t pf_vlc_memcpy = memcpy;

void vlc_fastmem_register (vlc_memcpy_t cpy)
{
    assert (cpy != NULL);
    pf_vlc_memcpy = cpy;
}

/**
 * vlc_memcpy: fast CPU-dependent memcpy
 */
void *vlc_memcpy (void *tgt, const void *src, size_t n)
{
    return pf_vlc_memcpy (tgt, src, n);
}

/**
 * Returned an aligned pointer on newly allocated memory.
 * \param alignment must be a power of 2 and a multiple of sizeof(void*)
 * \param size is the size of the usable memory returned.
 *
 * It must not be freed directly, *base must.
 */
void *vlc_memalign(void **base, size_t alignment, size_t size)
{
    assert(alignment >= sizeof(void*));
    for (size_t t = alignment; t > 1; t >>= 1)
        assert((t&1) == 0);
#if defined(HAVE_POSIX_MEMALIGN)
    if (posix_memalign(base, alignment, size)) {
        *base = NULL;
        return NULL;
    }
    return *base;
#elif defined(HAVE_MEMALIGN)
    return *base = memalign(alignment, size);
#else
    unsigned char *p = *base = malloc(size + alignment - 1);
    if (!p)
        return NULL;
    return (void*)((uintptr_t)(p + alignment - 1) & ~(alignment - 1));
#endif
}
