/*
 * WMA compatible decoder
 * Copyright (c) 2002 The FFmpeg Project.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include "wmafixed.h"
#include "mdct.h"

/*these are the sin and cos rotations used by the MDCT*/

/*accessed too infrequently to give much speedup in IRAM*/

int32_t *tcosarray[5], *tsinarray[5];
int32_t tcos0[1024], tcos1[512], tcos2[256], tcos3[128], tcos4[64];
int32_t tsin0[1024], tsin1[512], tsin2[256], tsin3[128], tsin4[64];

uint16_t revtab0[1024];

/**
 * init MDCT or IMDCT computation.
 */
int ff_mdct_init(MDCTContext *s, int nbits, int inverse)
{
    int n, n4, i;

    memset(s, 0, sizeof(*s));
    n = 1 << nbits;            /* nbits ranges from 12 to 8 inclusive */
    s->nbits = nbits;
    s->n = n;
    n4 = n >> 2;
    s->tcos = tcosarray[12-nbits];
    s->tsin = tsinarray[12-nbits];
    for(i=0;i<n4;i++)
    {
        int32_t ip = itofix32(i) + 0x2000;
        ip = ip >> nbits;

        /*I can't remember why this works, but it seems
          to agree for ~24 bits, maybe more!*/
        s->tsin[i] = - fsincos(ip<<16, &(s->tcos[i]));
        s->tcos[i] *=-1;
    }

    (&s->fft)->nbits = nbits-2;
    (&s->fft)->inverse = inverse;

    return 0;

}

/**
 * Compute inverse MDCT of size N = 2^nbits
 * @param output N samples
 * @param input N/2 samples
 * @param tmp N/2 samples
 */
void ff_imdct_calc(MDCTContext *s,
                   int32_t *output,
                   int32_t *input)
{
    int k, n8, n4, n2, n, j,scale;
    const int32_t *tcos = s->tcos;
    const int32_t *tsin = s->tsin;
    const int32_t *in1, *in2;
    FFTComplex *z1 = (FFTComplex *)output;
    FFTComplex *z2 = (FFTComplex *)input;
    int revtabshift = 12 - s->nbits;

    n = 1 << s->nbits;

    n2 = n >> 1;
    n4 = n >> 2;
    n8 = n >> 3;

    /* pre rotation */
    in1 = input;
    in2 = input + n2 - 1;

    for(k = 0; k < n4; k++)
    {
        j=revtab0[k<<revtabshift];
        CMUL(&z1[j].re, &z1[j].im, *in2, *in1, tcos[k], tsin[k]);
        in1 += 2;
        in2 -= 2;
    }

    scale = fft_calc_unscaled(&s->fft, z1);

    /* post rotation + reordering */
    for(k = 0; k < n4; k++)
    {
        CMUL(&z2[k].re, &z2[k].im, (z1[k].re), (z1[k].im), tcos[k], tsin[k]);
    }

    for(k = 0; k < n8; k++)
    {
        int32_t r1,r2,r3,r4,r1n,r2n,r3n;

        r1 = z2[n8 + k].im;
        r1n = r1 * -1;
        r2 = z2[n8-1-k].re;
        r2n = r2 * -1;
        r3 = z2[k+n8].re;
        r3n = r3 * -1;
        r4 = z2[n8-k-1].im;

        output[2*k] = r1n;
        output[n2-1-2*k] = r1;

        output[2*k+1] = r2;
        output[n2-1-2*k-1] = r2n;

        output[n2 + 2*k]= r3n;
        output[n-1- 2*k]= r3n;

        output[n2 + 2*k+1]= r4;
        output[n-2 - 2 * k] = r4;
    }
}

/* init MDCT */

int mdct_init_global(void)
{
    int i,j,m;

    /* although seemingly degenerate, these cannot actually be merged together without
       a substantial increase in error which is unjustified by the tiny memory savings*/

    tcosarray[0] = tcos0; tcosarray[1] = tcos1; tcosarray[2] = tcos2; tcosarray[3] = tcos3;tcosarray[4] = tcos4;
    tsinarray[0] = tsin0; tsinarray[1] = tsin1; tsinarray[2] = tsin2; tsinarray[3] = tsin3;tsinarray[4] = tsin4;

    /* init the MDCT bit reverse table here rather then in fft_init */

    for(i=0;i<1024;i++)           /*hard coded to a 2048 bit rotation*/
    {                             /*smaller sizes can reuse the largest*/
        m=0;
        for(j=0;j<10;j++)
        {
            m |= ((i >> j) & 1) << (10-j-1);
        }

       revtab0[i]=m;
    }

    fft_init_global();

    return 0;
}
