/****************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2007 Michael Giacomelli
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

/*  fixed precision code.  We use a combination of Sign 15.16 and Sign.31
    precision here.

    The WMA decoder does not always follow this convention, and occasionally
    renormalizes values to other formats in order to maximize precision.
    However, only the two precisions above are provided in this file.

*/

#include <inttypes.h>

#define PRECISION       16
#define PRECISION64     16

#define fixtof64(x)       (float)((float)(x) / (float)(1 << PRECISION64))        //does not work on int64_t!
#define ftofix32(x)       ((int32_t)((x) * (float)(1 << PRECISION) + ((x) < 0 ? -0.5 : 0.5)))
#define itofix64(x)       (IntTo64(x))
#define itofix32(x)       ((x) << PRECISION)
#define fixtoi32(x)       ((x) >> PRECISION)
#define fixtoi64(x)       (IntFrom64(x))

/*fixed functions*/

int64_t IntTo64(int x);
int IntFrom64(int64_t x);
int32_t Fixed32From64(int64_t x);
int64_t Fixed32To64(int32_t x);
int64_t fixmul64byfixed(int64_t x, int32_t y);
int32_t fixdiv32(int32_t x, int32_t y);
int64_t fixdiv64(int64_t x, int64_t y);
int32_t fixsqrt32(int32_t x);
long fsincos(unsigned long phase, int32_t *cos);

#ifdef __arm__

/*Sign-15.16 format */

#define fixmul32(x, y)  \
    ({ int32_t __hi;  \
       uint32_t __lo;  \
       int32_t __result;  \
       asm ("smull   %0, %1, %3, %4\n\t"  \
            "movs    %0, %0, lsr %5\n\t"  \
            "adc    %2, %0, %1, lsl %6"  \
            : "=&r" (__lo), "=&r" (__hi), "=r" (__result)  \
            : "%r" (x), "r" (y),  \
              "M" (PRECISION), "M" (32 - PRECISION)  \
            : "cc");  \
       __result;  \
    })

#define fixmul32b(x, y)  \
    ({ int32_t __hi;  \
       uint32_t __lo;  \
       int32_t __result;  \
       asm ("smull   %0, %1, %3, %4\n\t"  \
            "movs    %2, %1, lsl #1"  \
            : "=&r" (__lo), "=&r" (__hi), "=r" (__result)  \
            : "%r" (x), "r" (y)  \
            : "cc");  \
       __result;  \
    })

#elif defined(CPU_COLDFIRE)

static inline int32_t fixmul32(int32_t x, int32_t y)
{
#if PRECISION != 16
#warning Coldfire fixmul32() only works for PRECISION == 16
#endif
    int32_t t1;
    asm (
        "mac.l   %[x], %[y], %%acc0  \n" /* multiply */
        "mulu.l  %[y], %[x]      \n"     /* get lower half, avoid emac stall */
        "movclr.l %%acc0, %[t1]  \n"     /* get higher half */
        "lsr.l   #1, %[t1]       \n"
        "move.w  %[t1], %[x]     \n"
        "swap    %[x]            \n"
        : [t1] "=&d" (t1), [x] "+d" (x)
        : [y] "d"  (y)
    );
    return x;
}

static inline int32_t fixmul32b(int32_t x, int32_t y)
{
    asm (
        "mac.l   %[x], %[y], %%acc0  \n" /* multiply */
        "movclr.l %%acc0, %[x]  \n"     /* get higher half */
        : [x] "+d" (x)
        : [y] "d"  (y)
    );
    return x;
}

#else

static inline int32_t fixmul32(int32_t x, int32_t y)
{
    int64_t temp;
    temp = x;
    temp *= y;

    temp >>= PRECISION;

    return (int32_t)temp;
}

static inline int32_t fixmul32b(int32_t x, int32_t y)
{
    int64_t temp;

    temp = x;
    temp *= y;

    temp >>= 31;        //16+31-16 = 31 bits

    return (int32_t)temp;
}

#endif

#ifdef __arm__
static inline
void CMUL(int32_t *x, int32_t *y,
          int32_t  a, int32_t  b,
          int32_t  t, int32_t  v)
{
    /* This version loses one bit of precision. Could be solved at the cost
     * of 2 extra cycles if it becomes an issue. */
    int x1, y1, l;
    asm(
        "smull    %[l], %[y1], %[b], %[t] \n"
        "smlal    %[l], %[y1], %[a], %[v] \n"
        "rsb      %[b], %[b], #0          \n"
        "smull    %[l], %[x1], %[a], %[t] \n"
        "smlal    %[l], %[x1], %[b], %[v] \n"
        : [l] "=&r" (l), [x1]"=&r" (x1), [y1]"=&r" (y1), [b] "+r" (b)
        : [a] "r" (a),   [t] "r" (t),    [v] "r" (v)
        : "cc"
    );
    *x = x1 << 1;
    *y = y1 << 1;
}
#elif defined CPU_COLDFIRE
static inline
void CMUL(int32_t *x, int32_t *y,
          int32_t  a, int32_t  b,
          int32_t  t, int32_t  v)
{
  asm volatile ("mac.l %[a], %[t], %%acc0;"
                "msac.l %[b], %[v], %%acc0;"
                "mac.l %[b], %[t], %%acc1;"
                "mac.l %[a], %[v], %%acc1;"
                "movclr.l %%acc0, %[a];"
                "move.l %[a], (%[x]);"
                "movclr.l %%acc1, %[a];"
                "move.l %[a], (%[y]);"
                : [a] "+&r" (a)
                : [x] "a" (x), [y] "a" (y),
                  [b] "r" (b), [t] "r" (t), [v] "r" (v)
                : "cc", "memory");
}
#else
static inline
void CMUL(int32_t *pre,
          int32_t *pim,
          int32_t are,
          int32_t aim,
          int32_t bre,
          int32_t bim)
{
    //int64_t x,y;
    int32_t _aref = are;
    int32_t _aimf = aim;
    int32_t _bref = bre;
    int32_t _bimf = bim;
    int32_t _r1 = fixmul32b(_bref, _aref);
    int32_t _r2 = fixmul32b(_bimf, _aimf);
    int32_t _r3 = fixmul32b(_bref, _aimf);
    int32_t _r4 = fixmul32b(_bimf, _aref);
    *pre = _r1 - _r2;
    *pim = _r3 + _r4;

}
#endif
