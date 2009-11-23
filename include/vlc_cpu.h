/*****************************************************************************
 * vlc_cpu.h: CPU capabilities
 *****************************************************************************
 * Copyright (C) 1998-2009 the VideoLAN team
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

/**
 * \file
 * This file provides CPU-specific optimization flags.
 */

#ifndef VLC_CPU_H
# define VLC_CPU_H 1

# if defined (__i386__) || defined (__x86_64__)
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
# else
#  define CPU_CAPABILITY_MMX     (0)
#  define CPU_CAPABILITY_3DNOW   (0)
#  define CPU_CAPABILITY_MMXEXT  (0)
#  define CPU_CAPABILITY_SSE     (0)
#  define CPU_CAPABILITY_SSE2    (0)
#  define CPU_CAPABILITY_SSE3    (0)
#  define CPU_CAPABILITY_SSSE3   (0)
#  define CPU_CAPABILITY_SSE4_1  (0)
#  define CPU_CAPABILITY_SSE4_2  (0)
#  define CPU_CAPABILITY_SSE4A   (0)
# endif

# if defined (__ppc__) || defined (__ppc64__) || defined (__powerpc__)
#  define CPU_CAPABILITY_ALTIVEC (1<<16)
# else
#  define CPU_CAPABILITY_ALTIVEC (0)
# endif

# if defined (__arm__)
#  define CPU_CAPABILITY_NEON    (1<<24)
# else
#  define CPU_CAPABILITY_NEON    (0)
# endif

VLC_EXPORT( unsigned, vlc_CPU, ( void ) );

/** Are floating point operations fast?
 * If this bit is not set, you should try to use fixed-point instead.
 */
# if defined (__i386__) || defined (__x86_64__)
#  define HAVE_FPU 1

# elif defined (__powerpc__) || defined (__ppc__) || defined (__pc64__)
#  define HAVE_FPU 1

# elif defined (__arm__)
#  define HAVE_FPU 0 /* revisit later? */

# elif defined (__sparc__)
#  define HAVE_FPU 1

# else
#  define HAVE_FPU 0

# endif

typedef void *(*vlc_memcpy_t) (void *tgt, const void *src, size_t n);
typedef void *(*vlc_memset_t) (void *tgt, int c, size_t n);

VLC_EXPORT( void, vlc_fastmem_register, (vlc_memcpy_t cpy, vlc_memset_t set) );

#endif /* !VLC_CPU_H */

