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

VLC_API unsigned vlc_CPU(void);

# if defined (__i386__) || defined (__x86_64__)
#  define HAVE_FPU 1
#  define CPU_CAPABILITY_MMX     (1<<3)
#  define CPU_CAPABILITY_3DNOW   (1<<4)
#  define CPU_CAPABILITY_MMXEXT  (1<<5)
#  define CPU_CAPABILITY_SSE     (1<<6)
#  define CPU_CAPABILITY_SSE2    (1<<7)
#  define CPU_CAPABILITY_SSE3    (1<<8)
#  define CPU_CAPABILITY_SSSE3   (1<<9)
#  define CPU_CAPABILITY_SSE4_1  (1<<10)
#  define CPU_CAPABILITY_SSE4_2  (1<<11)
#  define CPU_CAPABILITY_SSE4A   (1<<12)

# if defined (__MMX__)
#  define VLC_MMX
# elif VLC_GCC_VERSION(4, 4)
#  define VLC_MMX __attribute__ ((__target__ ("mmx")))
# else
#  define VLC_MMX VLC_MMX_is_not_implemented_on_this_compiler
# endif

# if defined (__SSE__)
#  define VLC_SSE
# elif VLC_GCC_VERSION(4, 4)
#  define VLC_SSE __attribute__ ((__target__ ("sse")))
# else
#  define VLC_SSE VLC_SSE_is_not_implemented_on_this_compiler
# endif

# elif defined (__ppc__) || defined (__ppc64__) || defined (__powerpc__)
#  define HAVE_FPU 1
#  define VLC_CPU_ALTIVEC 2

#  ifdef ALTIVEC
#   define vlc_CPU_ALTIVEC() (1)
#  else
#   define vlc_CPU_ALTIVEC() ((vlc_CPU() & VLC_CPU_ALTIVEC) != 0)
#  endif

# elif defined (__arm__)
#  if defined (__VFP_FP__) && !defined (__SOFTFP__)
#   define HAVE_FPU 1
#  else
#   define HAVE_FPU 0
#  endif
#  define VLC_CPU_ARM_NEON 2

#  ifdef __ARM_NEON__
#   define vlc_CPU_ARM_NEON() (1)
#  else
#   define vlc_CPU_ARM_NEON() ((vlc_CPU() & VLC_CPU_ARM_NEON) != 0)
#  endif

# elif defined (__sparc__)
#  define HAVE_FPU 1

# else
/**
 * Are single precision floating point operations "fast"?
 * If this preprocessor constant is zero, floating point should be avoided
 * (especially relevant for audio codecs).
 */
#  define HAVE_FPU 0

# endif

#endif /* !VLC_CPU_H */
