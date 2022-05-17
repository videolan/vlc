/*****************************************************************************
 * vlc_cpu.h: CPU capabilities
 *****************************************************************************
 * Copyright (C) 1998-2009 VLC authors and VideoLAN
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

/**
 * \file
 * This file provides CPU features detection.
 */

#ifndef VLC_CPU_H
# define VLC_CPU_H 1

/**
 * Retrieves CPU capability flags.
 */
VLC_API unsigned vlc_CPU(void);

/**
 * Computes CPU capability flags.
 *
 * Do not call this function directly.
 * Call vlc_CPU() instead, which caches the correct value.
 */
unsigned vlc_CPU_raw(void);

# if defined (__i386__) || defined (__x86_64__)
#  define HAVE_FPU 1
#  define VLC_CPU_SSE2   0x00000080
#  define VLC_CPU_SSE3   0x00000100
#  define VLC_CPU_SSSE3  0x00000200
#  define VLC_CPU_SSE4_1 0x00000400
#  define VLC_CPU_AVX    0x00002000
#  define VLC_CPU_AVX2   0x00004000

# if defined (__SSE__)
#  define VLC_SSE
# else
#  define VLC_SSE __attribute__ ((__target__ ("sse")))
# endif

# ifdef __SSE2__
#  define vlc_CPU_SSE2() (1)
# else
#  define vlc_CPU_SSE2() ((vlc_CPU() & VLC_CPU_SSE2) != 0)
# endif

# ifdef __SSE3__
#  define vlc_CPU_SSE3() (1)
# else
#  define vlc_CPU_SSE3() ((vlc_CPU() & VLC_CPU_SSE3) != 0)
# endif

# ifdef __SSSE3__
#  define vlc_CPU_SSSE3() (1)
# else
#  define vlc_CPU_SSSE3() ((vlc_CPU() & VLC_CPU_SSSE3) != 0)
# endif

# ifdef __SSE4_1__
#  define vlc_CPU_SSE4_1() (1)
# else
#  define vlc_CPU_SSE4_1() ((vlc_CPU() & VLC_CPU_SSE4_1) != 0)
# endif

# ifdef __AVX__
#  define vlc_CPU_AVX() (1)
#  define VLC_AVX
# else
#  define vlc_CPU_AVX() ((vlc_CPU() & VLC_CPU_AVX) != 0)
#  define VLC_AVX __attribute__ ((__target__ ("avx")))
# endif

# ifdef __AVX2__
#  define vlc_CPU_AVX2() (1)
# else
#  define vlc_CPU_AVX2() ((vlc_CPU() & VLC_CPU_AVX2) != 0)
# endif

# elif defined (__ppc__) || defined (__ppc64__) || defined (__powerpc__)
#  define HAVE_FPU 1
#  define VLC_CPU_ALTIVEC 2

#  ifdef ALTIVEC
#   define vlc_CPU_ALTIVEC() (1)
#   define VLC_ALTIVEC
#  else
#   define vlc_CPU_ALTIVEC() ((vlc_CPU() & VLC_CPU_ALTIVEC) != 0)
#   define VLC_ALTIVEC __attribute__ ((__target__ ("altivec")))
#  endif

# elif defined (__arm__)
#  if defined (__VFP_FP__) && !defined (__SOFTFP__)
#   define HAVE_FPU 1
#  else
#   define HAVE_FPU 0
#  endif
#  define VLC_CPU_ARMv6    4
#  define VLC_CPU_ARM_NEON 2

#  if defined (__ARM_ARCH_7A__)
#   define VLC_CPU_ARM_ARCH 7
#  elif defined (__ARM_ARCH_6__) || defined (__ARM_ARCH_6T2__)
#   define VLC_CPU_ARM_ARCH 6
#  else
#   define VLC_CPU_ARM_ARCH 4
#  endif

#  if (VLC_CPU_ARM_ARCH >= 6)
#   define vlc_CPU_ARMv6() (1)
#  else
#   define vlc_CPU_ARMv6() ((vlc_CPU() & VLC_CPU_ARMv6) != 0)
#  endif

#  ifdef __ARM_NEON__
#   define vlc_CPU_ARM_NEON() (1)
#  else
#   define vlc_CPU_ARM_NEON() ((vlc_CPU() & VLC_CPU_ARM_NEON) != 0)
#  endif

# elif defined (__aarch64__)
#  define HAVE_FPU 1
#  define VLC_CPU_ARM_NEON 0x1
#  define VLC_CPU_ARM_SVE  0x2

#  ifdef __ARM_NEON
#   define vlc_CPU_ARM_NEON() (1)
#  else
#   define vlc_CPU_ARM_NEON() ((vlc_CPU() & VLC_CPU_ARM_NEON) != 0)
#  endif

#  ifdef __ARM_FEATURE_SVE
#   define vlc_CPU_ARM_SVE()   (1)
#  else
#   define vlc_CPU_ARM_SVE()   ((vlc_CPU() & VLC_CPU_ARM_SVE) != 0)
#  endif

# elif defined (__sparc__)
#  define HAVE_FPU 1

# elif defined (__mips_hard_float)
#  define HAVE_FPU 1

# elif defined (__riscv)
#  ifdef __riscv_flen
#   define HAVE_FPU 1
#  endif
#  define VLC_CPU_RV_V 0x1

#  ifdef __riscv_v
#   define vlc_CPU_RV_V() (1)
#  else
#   define vlc_CPU_RV_V() ((vlc_CPU() & VLC_CPU_RV_V) != 0)
#  endif

# else
/**
 * Are single precision floating point operations "fast"?
 * If this preprocessor constant is zero, floating point should be avoided
 * (especially relevant for audio codecs).
 */
#  define HAVE_FPU 0

# endif

/**
 * Initialises DSP functions.
 *
 * This helper looks for accelerated Digital Signal Processing functions
 * identified by the supplied type name. Those functions ares typically
 * implemented using architecture-specific assembler code with
 * Single Instruction Multiple Data (SIMD) opcodes for faster processing.
 *
 * The exact purposes and semantics of the DSP functions is uniquely identified
 * by a nul-terminated string.
 *
 * \note This function should not be used directly. It is recommended to use
 * the convenience wrapper vlc_CPU_functions_init_once() instead.
 *
 * \param name nul-terminated type identifier (cannot be NULL)
 * \param [inout] funcs type-specific data structure to be initialised
 */
VLC_API void vlc_CPU_functions_init(const char *name, void *restrict funcs);

# ifndef __cplusplus
/**
 * Initialises DSP functions once.
 *
 * This is a convenience wrapper for vlc_CPU_functions_init().
 * It only initialises the functions the first time it is evaluated.
 */
static inline void vlc_CPU_functions_init_once(const char *name,
                                               void *restrict funcs)
{
    static vlc_once_t once = VLC_STATIC_ONCE;

    if (!vlc_once_begin(&once)) {
        vlc_CPU_functions_init(name, funcs);
        vlc_once_complete(&once);
    }
}
# endif

#define set_cpu_funcs(name, activate, priority) \
    set_callback(VLC_CHECKED_TYPE(void (*)(void *), activate)) \
    set_capability(name, priority)

#endif /* !VLC_CPU_H */
