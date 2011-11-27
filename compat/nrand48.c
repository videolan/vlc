/*****************************************************************************
 * nrand48.c: POSIX erand48(), jrand48() and nrand48() replacements
 *****************************************************************************
 * Copyright © 2010 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <inttypes.h>

static uint64_t iterate48 (unsigned short subi[3])
{
    const uint64_t a = UINT64_C(0x5DEECE66D);
    const unsigned c = 13;
    const uint64_t mask = UINT64_C(0xFFFFFFFFFFFF); // 48 bits

    uint64_t x = ((uint64_t)subi[0] << 32)
               | ((uint32_t)subi[1] << 16)
               | subi[2];

    x *= a;
    x += c;
    x &= mask;

    subi[0] = (x >> 32) & 0xFFFF;
    subi[1] = (x >> 16) & 0xFFFF;
    subi[2] = (x >>  0) & 0XFFFF;

    return x;
}

double erand48 (unsigned short subi[3])
{
    uint64_t r = iterate48 (subi);
    return ((double)r) / 281474976710655.;
}

long jrand48 (unsigned short subi[3])
{
    return ((int64_t)iterate48 (subi)) >> 16;
}

long nrand48 (unsigned short subi[3])
{
    return iterate48 (subi) >> 17;
}
