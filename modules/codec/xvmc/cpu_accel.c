/* $Id$
 * cpu_accel.c
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <inttypes.h>

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"

static inline uint32_t arch_accel( void )
{
    uint32_t eax, ebx, ecx, edx;
    int AMD;
    uint32_t caps;

#if defined(__x86_64__) || (!defined(PIC) && !defined(__PIC__))
#define cpuid(op,eax,ebx,ecx,edx) \
    __asm__ ("cpuid"        \
         : "=a" (eax),      \
           "=b" (ebx),      \
           "=c" (ecx),      \
           "=d" (edx)       \
         : "a" (op)         \
         : "cc")
#else /* PIC version : save ebx (not needed on x86_64) */
#define cpuid(op,eax,ebx,ecx,edx) \
    __asm__ ("push %%ebx\n\t"   \
         "cpuid\n\t"            \
         "movl %%ebx,%1\n\t"    \
         "pop %%ebx"        \
         : "=a" (eax),      \
           "=r" (ebx),      \
           "=c" (ecx),      \
           "=d" (edx)       \
         : "a" (op)         \
         : "cc")
#endif

#ifndef __x86_64__ /* x86_64 supports the cpuid op */
    __asm__ ("pushf\n\t"
        "pushf\n\t"
        "pop %0\n\t"
        "movl %0,%1\n\t"
        "xorl $0x200000,%0\n\t"
        "push %0\n\t"
        "popf\n\t"
        "pushf\n\t"
        "pop %0\n\t"
        "popf"
        : "=r" (eax),
          "=r" (ebx)
        :
        : "cc");

    if (eax == ebx) /* no cpuid */
        return 0;
#endif

    cpuid (0x00000000, eax, ebx, ecx, edx);
    if (!eax) /* vendor string only */
        return 0;

    AMD = (ebx == 0x68747541) && (ecx == 0x444d4163) && (edx == 0x69746e65);

    cpuid (0x00000001, eax, ebx, ecx, edx);
    if (! (edx & 0x00800000))	/* no MMX */
        return 0;

    caps = MPEG2_ACCEL_X86_MMX;
    if (edx & 0x02000000)	/* SSE - identical to AMD MMX extensions */
        caps = MPEG2_ACCEL_X86_MMX | MPEG2_ACCEL_X86_MMXEXT;

    cpuid (0x80000000, eax, ebx, ecx, edx);
    if (eax < 0x80000001)	/* no extended capabilities */
        return caps;

    cpuid (0x80000001, eax, ebx, ecx, edx);

    if (edx & 0x80000000)
        caps |= MPEG2_ACCEL_X86_3DNOW;

    if (AMD && (edx & 0x00400000))	/* AMD MMX extensions */
        caps |= MPEG2_ACCEL_X86_MMXEXT;

    return caps;
}

uint32_t mpeg2_detect_accel (void)
{
    uint32_t accel;

    accel = 0;
    accel = arch_accel ();

    return accel;
}
