/*****************************************************************************
 * idct.c : C IDCT module
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: idct.c,v 1.22 2002/04/21 11:23:03 gbazin Exp $
 *
 * Author: Gaël Hendryckx <jimmy@via.ecp.fr>
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
#include <stdlib.h>
#include <string.h>

#include <videolan/vlc.h>

#include "idct.h"
#include "block_c.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("IDCT module") )
    ADD_CAPABILITY( IDCT, 50 )
    ADD_SHORTCUT( "c" )
    ADD_SHORTCUT( "idct" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    idct_getfunctions( &p_module->p_functions->idct );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/* Following functions are local */

/*****************************************************************************
 * NormScan : Unused in this IDCT
 *****************************************************************************/
static void NormScan( u8 ppi_scan[2][64] )
{
}

/*****************************************************************************
 * IDCT : IDCT function for normal matrices
 *****************************************************************************/
static __inline__ void IDCT( dctelem_t * p_block )
{
    s32 tmp0, tmp1, tmp2, tmp3;
    s32 tmp10, tmp11, tmp12, tmp13;
    s32 z1, z2, z3, z4, z5;
    s32 d0, d1, d2, d3, d4, d5, d6, d7;
    dctelem_t * dataptr;
    int rowctr;

    SHIFT_TEMPS

    /* Pass 1: process rows. */
    /* Note results are scaled up by sqrt(8) compared to a true IDCT; */
    /* furthermore, we scale the results by 2**PASS1_BITS. */

    dataptr = p_block;

    for (rowctr = DCTSIZE-1; rowctr >= 0; rowctr--)
    {
        /* Due to quantization, we will usually find that many of the input
         * coefficients are zero, especially the AC terms.  We can exploit this
         * by short-circuiting the IDCT calculation for any row in which all
         * the AC terms are zero.  In that case each output is equal to the
         * DC coefficient (with scale factor as needed).
         * With typical images and quantization tables, half or more of the
         * row DCT calculations can be simplified this way.
         */

        register int * idataptr = (int*)dataptr;
        d0 = dataptr[0];
        d1 = dataptr[1];
        if ( (d1 == 0) && ((idataptr[1] | idataptr[2] | idataptr[3]) == 0) )
        {
      /* AC terms all zero */
            if (d0)
            {
      /* Compute a 32 bit value to assign. */
                dctelem_t dcval = (dctelem_t) (d0 << PASS1_BITS);
                register int v = (dcval & 0xffff) | (dcval << 16);

                idataptr[0] = v;
                idataptr[1] = v;
                idataptr[2] = v;
                idataptr[3] = v;
            }

            dataptr += DCTSIZE; /* advance pointer to next row */
            continue;
        }
        d2 = dataptr[2];
        d3 = dataptr[3];
        d4 = dataptr[4];
        d5 = dataptr[5];
        d6 = dataptr[6];
        d7 = dataptr[7];

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */
        if (d6)
        {
            if (d4)
            {
                if (d2)
                {
                    if (d0)
                    {
            /* d0 != 0, d2 != 0, d4 != 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = (d0 + d4) << CONST_BITS;
                        tmp1 = (d0 - d4) << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp1 + tmp2;
                        tmp12 = tmp1 - tmp2;
                    }
                    else
                    {
                    /* d0 == 0, d2 != 0, d4 != 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = d4 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp2 - tmp0;
                        tmp12 = -(tmp0 + tmp2);
                        }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 != 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp0 = (d0 + d4) << CONST_BITS;
                        tmp1 = (d0 - d4) << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp1 + tmp2;
                        tmp12 = tmp1 - tmp2;
                        }
                    else
                    {
                    /* d0 == 0, d2 == 0, d4 != 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp0 = d4 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp2 - tmp0;
                        tmp12 = -(tmp0 + tmp2);
                        }
                }
            }
            else
            {
                if (d2)
                {
                    if (d0)
                    {
            /* d0 != 0, d2 != 0, d4 == 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = d0 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp0 + tmp2;
                        tmp12 = tmp0 - tmp2;
                    }
                    else
                    {
                    /* d0 == 0, d2 != 0, d4 == 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp10 = tmp3;
                        tmp13 = -tmp3;
                        tmp11 = tmp2;
                        tmp12 = -tmp2;
                            }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 == 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp0 = d0 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp0 + tmp2;
                        tmp12 = tmp0 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 == 0, d4 == 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp10 = tmp3;
                        tmp13 = -tmp3;
                        tmp11 = tmp2;
                        tmp12 = -tmp2;
                    }
                }
            }
        }
        else
        {
            if (d4)
            {
                if (d2)
                {
                    if (d0)
                    {
                    /* d0 != 0, d2 != 0, d4 != 0, d6 == 0 */
                        tmp2 = MULTIPLY(d2, FIX(0.541196100));
                        tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                        tmp0 = (d0 + d4) << CONST_BITS;
                        tmp1 = (d0 - d4) << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp1 + tmp2;
                        tmp12 = tmp1 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 != 0, d4 != 0, d6 == 0 */
                        tmp2 = MULTIPLY(d2, FIX(0.541196100));
                        tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                        tmp0 = d4 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp2 - tmp0;
                        tmp12 = -(tmp0 + tmp2);
                    }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 != 0, d6 == 0 */
                        tmp10 = tmp13 = (d0 + d4) << CONST_BITS;
                        tmp11 = tmp12 = (d0 - d4) << CONST_BITS;
                    }
                    else
                    {
            /* d0 == 0, d2 == 0, d4 != 0, d6 == 0 */
                        tmp10 = tmp13 = d4 << CONST_BITS;
                        tmp11 = tmp12 = -tmp10;
                    }
                }
            }
            else
            {
                if (d2)
                {
                    if (d0)
                    {
            /* d0 != 0, d2 != 0, d4 == 0, d6 == 0 */
                        tmp2 = MULTIPLY(d2, FIX(0.541196100));
                        tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                        tmp0 = d0 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp0 + tmp2;
                        tmp12 = tmp0 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 != 0, d4 == 0, d6 == 0 */
                        tmp2 = MULTIPLY(d2, FIX(0.541196100));
                        tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                        tmp10 = tmp3;
                        tmp13 = -tmp3;
                        tmp11 = tmp2;
                        tmp12 = -tmp2;
                    }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 == 0, d6 == 0 */
                        tmp10 = tmp13 = tmp11 = tmp12 = d0 << CONST_BITS;
                    }
                    else
                    {
            /* d0 == 0, d2 == 0, d4 == 0, d6 == 0 */
                        tmp10 = tmp13 = tmp11 = tmp12 = 0;
                    }
                }
            }
        }


    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

        if (d7)
            {
            if (d5)
            {
                if (d3)
                {
                    if (d1)
                    {
            /* d1 != 0, d3 != 0, d5 != 0, d7 != 0 */
                        z1 = d7 + d1;
                        z2 = d5 + d3;
                        z3 = d7 + d3;
                        z4 = d5 + d1;
                        z5 = MULTIPLY(z3 + z4, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, FIX(0.298631336));
                        tmp1 = MULTIPLY(d5, FIX(2.053119869));
                        tmp2 = MULTIPLY(d3, FIX(3.072711026));
                        tmp3 = MULTIPLY(d1, FIX(1.501321110));
                        z1 = MULTIPLY(z1, - FIX(0.899976223));
                        z2 = MULTIPLY(z2, - FIX(2.562915447));
                        z3 = MULTIPLY(z3, - FIX(1.961570560));
                        z4 = MULTIPLY(z4, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 += z1 + z3;
                        tmp1 += z2 + z4;
                        tmp2 += z2 + z3;
                        tmp3 += z1 + z4;
                    }
                    else
                    {
            /* d1 == 0, d3 != 0, d5 != 0, d7 != 0 */
                        z2 = d5 + d3;
                        z3 = d7 + d3;
                        z5 = MULTIPLY(z3 + d5, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, FIX(0.298631336));
                        tmp1 = MULTIPLY(d5, FIX(2.053119869));
                        tmp2 = MULTIPLY(d3, FIX(3.072711026));
                        z1 = MULTIPLY(d7, - FIX(0.899976223));
                        z2 = MULTIPLY(z2, - FIX(2.562915447));
                        z3 = MULTIPLY(z3, - FIX(1.961570560));
                        z4 = MULTIPLY(d5, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 += z1 + z3;
                        tmp1 += z2 + z4;
                        tmp2 += z2 + z3;
                        tmp3 = z1 + z4;
                        }
                    }
                else
                {
                    if (d1)
                    {
            /* d1 != 0, d3 == 0, d5 != 0, d7 != 0 */
                        z1 = d7 + d1;
                        z4 = d5 + d1;
                        z5 = MULTIPLY(d7 + z4, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, FIX(0.298631336));
                        tmp1 = MULTIPLY(d5, FIX(2.053119869));
                        tmp3 = MULTIPLY(d1, FIX(1.501321110));
                        z1 = MULTIPLY(z1, - FIX(0.899976223));
                        z2 = MULTIPLY(d5, - FIX(2.562915447));
                        z3 = MULTIPLY(d7, - FIX(1.961570560));
                        z4 = MULTIPLY(z4, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 += z1 + z3;
                        tmp1 += z2 + z4;
                        tmp2 = z2 + z3;
                        tmp3 += z1 + z4;
                    }
                    else
                    {
            /* d1 == 0, d3 == 0, d5 != 0, d7 != 0 */
                        z5 = MULTIPLY(d7 + d5, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, - FIX2(0.601344887));
                        tmp1 = MULTIPLY(d5, - FIX2(0.509795578));
                        z1 = MULTIPLY(d7, - FIX(0.899976223));
                        z3 = MULTIPLY(d7, - FIX(1.961570560));
                        z2 = MULTIPLY(d5, - FIX(2.562915447));
                        z4 = MULTIPLY(d5, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 += z3;
                        tmp1 += z4;
                        tmp2 = z2 + z3;
                        tmp3 = z1 + z4;
                    }
                }
            }
            else
            {
                if (d3)
                {
                    if (d1)
                    {
            /* d1 != 0, d3 != 0, d5 == 0, d7 != 0 */
                        z1 = d7 + d1;
                        z3 = d7 + d3;
                        z5 = MULTIPLY(z3 + d1, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, FIX(0.298631336));
                        tmp2 = MULTIPLY(d3, FIX(3.072711026));
                        tmp3 = MULTIPLY(d1, FIX(1.501321110));
                        z1 = MULTIPLY(z1, - FIX(0.899976223));
                        z2 = MULTIPLY(d3, - FIX(2.562915447));
                        z3 = MULTIPLY(z3, - FIX(1.961570560));
                        z4 = MULTIPLY(d1, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 += z1 + z3;
                        tmp1 = z2 + z4;
                        tmp2 += z2 + z3;
                        tmp3 += z1 + z4;
                    }
                    else
                    {
            /* d1 == 0, d3 != 0, d5 == 0, d7 != 0 */
                        z3 = d7 + d3;
                        z5 = MULTIPLY(z3, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, - FIX2(0.601344887));
                        tmp2 = MULTIPLY(d3, FIX(0.509795579));
                        z1 = MULTIPLY(d7, - FIX(0.899976223));
                        z2 = MULTIPLY(d3, - FIX(2.562915447));
                        z3 = MULTIPLY(z3, - FIX2(0.785694958));

                        tmp0 += z3;
                        tmp1 = z2 + z5;
                        tmp2 += z3;
                        tmp3 = z1 + z5;
                    }
                }
                else
                {
                    if (d1)
                    {
            /* d1 != 0, d3 == 0, d5 == 0, d7 != 0 */
                        z1 = d7 + d1;
                        z5 = MULTIPLY(z1, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, - FIX2(1.662939224));
                        tmp3 = MULTIPLY(d1, FIX2(1.111140466));
                        z1 = MULTIPLY(z1, FIX2(0.275899379));
                        z3 = MULTIPLY(d7, - FIX(1.961570560));
                        z4 = MULTIPLY(d1, - FIX(0.390180644));

                        tmp0 += z1;
                        tmp1 = z4 + z5;
                        tmp2 = z3 + z5;
                        tmp3 += z1;
                    }
                else
                    {
            /* d1 == 0, d3 == 0, d5 == 0, d7 != 0 */
                        tmp0 = MULTIPLY(d7, - FIX2(1.387039845));
                        tmp1 = MULTIPLY(d7, FIX(1.175875602));
                        tmp2 = MULTIPLY(d7, - FIX2(0.785694958));
                        tmp3 = MULTIPLY(d7, FIX2(0.275899379));
                    }
                }
            }
        }
        else
        {
            if (d5)
            {
                if (d3)
                {
                    if (d1)
                    {
            /* d1 != 0, d3 != 0, d5 != 0, d7 == 0 */
                        z2 = d5 + d3;
                        z4 = d5 + d1;
                        z5 = MULTIPLY(d3 + z4, FIX(1.175875602));

                        tmp1 = MULTIPLY(d5, FIX(2.053119869));
                        tmp2 = MULTIPLY(d3, FIX(3.072711026));
                        tmp3 = MULTIPLY(d1, FIX(1.501321110));
                        z1 = MULTIPLY(d1, - FIX(0.899976223));
                        z2 = MULTIPLY(z2, - FIX(2.562915447));
                        z3 = MULTIPLY(d3, - FIX(1.961570560));
                        z4 = MULTIPLY(z4, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 = z1 + z3;
                        tmp1 += z2 + z4;
                        tmp2 += z2 + z3;
                        tmp3 += z1 + z4;
                    }
                    else
                    {
            /* d1 == 0, d3 != 0, d5 != 0, d7 == 0 */
                        z2 = d5 + d3;
                        z5 = MULTIPLY(z2, FIX(1.175875602));

                        tmp1 = MULTIPLY(d5, FIX2(1.662939225));
                        tmp2 = MULTIPLY(d3, FIX2(1.111140466));
                        z2 = MULTIPLY(z2, - FIX2(1.387039845));
                        z3 = MULTIPLY(d3, - FIX(1.961570560));
                        z4 = MULTIPLY(d5, - FIX(0.390180644));

                        tmp0 = z3 + z5;
                        tmp1 += z2;
                        tmp2 += z2;
                        tmp3 = z4 + z5;
                    }
                }
                else
                {
                    if (d1)
                    {
            /* d1 != 0, d3 == 0, d5 != 0, d7 == 0 */
                        z4 = d5 + d1;
                        z5 = MULTIPLY(z4, FIX(1.175875602));

                        tmp1 = MULTIPLY(d5, - FIX2(0.509795578));
                        tmp3 = MULTIPLY(d1, FIX2(0.601344887));
                        z1 = MULTIPLY(d1, - FIX(0.899976223));
                        z2 = MULTIPLY(d5, - FIX(2.562915447));
                        z4 = MULTIPLY(z4, FIX2(0.785694958));

                        tmp0 = z1 + z5;
                        tmp1 += z4;
                        tmp2 = z2 + z5;
                        tmp3 += z4;
                    }
                    else
                    {
            /* d1 == 0, d3 == 0, d5 != 0, d7 == 0 */
                        tmp0 = MULTIPLY(d5, FIX(1.175875602));
                        tmp1 = MULTIPLY(d5, FIX2(0.275899380));
                        tmp2 = MULTIPLY(d5, - FIX2(1.387039845));
                        tmp3 = MULTIPLY(d5, FIX2(0.785694958));
                    }
                }
            }
            else
            {
                if (d3)
                {
                    if (d1)
                    {
            /* d1 != 0, d3 != 0, d5 == 0, d7 == 0 */
                        z5 = d3 + d1;

                        tmp2 = MULTIPLY(d3, - FIX(1.451774981));
                        tmp3 = MULTIPLY(d1, (FIX(0.211164243) - 1));
                        z1 = MULTIPLY(d1, FIX(1.061594337));
                        z2 = MULTIPLY(d3, - FIX(2.172734803));
                        z4 = MULTIPLY(z5, FIX(0.785694958));
                        z5 = MULTIPLY(z5, FIX(1.175875602));

                        tmp0 = z1 - z4;
                        tmp1 = z2 + z4;
                        tmp2 += z5;
                        tmp3 += z5;
                    }
                    else
                    {
            /* d1 == 0, d3 != 0, d5 == 0, d7 == 0 */
                        tmp0 = MULTIPLY(d3, - FIX2(0.785694958));
                        tmp1 = MULTIPLY(d3, - FIX2(1.387039845));
                        tmp2 = MULTIPLY(d3, - FIX2(0.275899379));
                        tmp3 = MULTIPLY(d3, FIX(1.175875602));
                    }
                }
                else
                {
                    if (d1)
                    {
            /* d1 != 0, d3 == 0, d5 == 0, d7 == 0 */
                        tmp0 = MULTIPLY(d1, FIX2(0.275899379));
                        tmp1 = MULTIPLY(d1, FIX2(0.785694958));
                        tmp2 = MULTIPLY(d1, FIX(1.175875602));
                        tmp3 = MULTIPLY(d1, FIX2(1.387039845));
                    }
                    else
                    {
            /* d1 == 0, d3 == 0, d5 == 0, d7 == 0 */
                        tmp0 = tmp1 = tmp2 = tmp3 = 0;
                    }
                }
            }
        }

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

        dataptr[0] = (dctelem_t) DESCALE(tmp10 + tmp3, CONST_BITS-PASS1_BITS);
        dataptr[7] = (dctelem_t) DESCALE(tmp10 - tmp3, CONST_BITS-PASS1_BITS);
        dataptr[1] = (dctelem_t) DESCALE(tmp11 + tmp2, CONST_BITS-PASS1_BITS);
        dataptr[6] = (dctelem_t) DESCALE(tmp11 - tmp2, CONST_BITS-PASS1_BITS);
        dataptr[2] = (dctelem_t) DESCALE(tmp12 + tmp1, CONST_BITS-PASS1_BITS);
        dataptr[5] = (dctelem_t) DESCALE(tmp12 - tmp1, CONST_BITS-PASS1_BITS);
        dataptr[3] = (dctelem_t) DESCALE(tmp13 + tmp0, CONST_BITS-PASS1_BITS);
        dataptr[4] = (dctelem_t) DESCALE(tmp13 - tmp0, CONST_BITS-PASS1_BITS);

        dataptr += DCTSIZE;              /* advance pointer to next row */
    }

  /* Pass 2: process columns. */
  /* Note that we must descale the results by a factor of 8 == 2**3, */
  /* and also undo the PASS1_BITS scaling. */

    dataptr = p_block;
    for (rowctr = DCTSIZE-1; rowctr >= 0; rowctr--)
    {
    /* Columns of zeroes can be exploited in the same way as we did with rows.
     * However, the row calculation has created many nonzero AC terms, so the
     * simplification applies less often (typically 5% to 10% of the time).
     * On machines with very fast multiplication, it's possible that the
     * test takes more time than it's worth.  In that case this section
     * may be commented out.
     */

        d0 = dataptr[DCTSIZE*0];
        d1 = dataptr[DCTSIZE*1];
        d2 = dataptr[DCTSIZE*2];
        d3 = dataptr[DCTSIZE*3];
        d4 = dataptr[DCTSIZE*4];
        d5 = dataptr[DCTSIZE*5];
        d6 = dataptr[DCTSIZE*6];
        d7 = dataptr[DCTSIZE*7];

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */
        if (d6)
        {
            if (d4)
            {
                if (d2)
                {
                    if (d0)
                    {
            /* d0 != 0, d2 != 0, d4 != 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = (d0 + d4) << CONST_BITS;
                        tmp1 = (d0 - d4) << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp1 + tmp2;
                        tmp12 = tmp1 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 != 0, d4 != 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = d4 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp2 - tmp0;
                        tmp12 = -(tmp0 + tmp2);
                    }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 != 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp0 = (d0 + d4) << CONST_BITS;
                        tmp1 = (d0 - d4) << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp1 + tmp2;
                        tmp12 = tmp1 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 == 0, d4 != 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, -FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp0 = d4 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp2 - tmp0;
                        tmp12 = -(tmp0 + tmp2);
                    }
                }
            }
            else
            {
                if (d2)
                {
                    if (d0)
                    {
            /* d0 != 0, d2 != 0, d4 == 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = d0 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp0 + tmp2;
                        tmp12 = tmp0 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 != 0, d4 == 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp10 = tmp3;
                        tmp13 = -tmp3;
                        tmp11 = tmp2;
                        tmp12 = -tmp2;
                    }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 == 0, d6 != 0 */
                    tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                    tmp3 = MULTIPLY(d6, FIX(0.541196100));

                    tmp0 = d0 << CONST_BITS;

                    tmp10 = tmp0 + tmp3;
                    tmp13 = tmp0 - tmp3;
                    tmp11 = tmp0 + tmp2;
                    tmp12 = tmp0 - tmp2;
                }
                else
                {
            /* d0 == 0, d2 == 0, d4 == 0, d6 != 0 */
                    tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                    tmp3 = MULTIPLY(d6, FIX(0.541196100));
                    tmp10 = tmp3;
                    tmp13 = -tmp3;
                    tmp11 = tmp2;
                    tmp12 = -tmp2;
                }
            }
        }
    }
    else
    {
        if (d4)
        {
            if (d2)
            {
                if (d0)
                {
            /* d0 != 0, d2 != 0, d4 != 0, d6 == 0 */
                    tmp2 = MULTIPLY(d2, FIX(0.541196100));
                    tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                    tmp0 = (d0 + d4) << CONST_BITS;
                    tmp1 = (d0 - d4) << CONST_BITS;

                    tmp10 = tmp0 + tmp3;
                    tmp13 = tmp0 - tmp3;
                    tmp11 = tmp1 + tmp2;
                    tmp12 = tmp1 - tmp2;
                }
                else
                {
            /* d0 == 0, d2 != 0, d4 != 0, d6 == 0 */
                    tmp2 = MULTIPLY(d2, FIX(0.541196100));
                    tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                    tmp0 = d4 << CONST_BITS;

                    tmp10 = tmp0 + tmp3;
                    tmp13 = tmp0 - tmp3;
                    tmp11 = tmp2 - tmp0;
                    tmp12 = -(tmp0 + tmp2);
                }
            }
            else
            {
                if (d0)
                {
            /* d0 != 0, d2 == 0, d4 != 0, d6 == 0 */
                    tmp10 = tmp13 = (d0 + d4) << CONST_BITS;
                    tmp11 = tmp12 = (d0 - d4) << CONST_BITS;
                }
                else
                {
            /* d0 == 0, d2 == 0, d4 != 0, d6 == 0 */
                    tmp10 = tmp13 = d4 << CONST_BITS;
                    tmp11 = tmp12 = -tmp10;
                }
            }
        }
        else
        {
        if (d2)
        {
            if (d0)
            {
            /* d0 != 0, d2 != 0, d4 == 0, d6 == 0 */
                    tmp2 = MULTIPLY(d2, FIX(0.541196100));
                    tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                    tmp0 = d0 << CONST_BITS;

                    tmp10 = tmp0 + tmp3;
                    tmp13 = tmp0 - tmp3;
                    tmp11 = tmp0 + tmp2;
                    tmp12 = tmp0 - tmp2;
            }
            else
            {
            /* d0 == 0, d2 != 0, d4 == 0, d6 == 0 */
                    tmp2 = MULTIPLY(d2, FIX(0.541196100));
                    tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                    tmp10 = tmp3;
                    tmp13 = -tmp3;
                    tmp11 = tmp2;
                    tmp12 = -tmp2;
            }
        }
        else
        {
            if (d0)
                {
            /* d0 != 0, d2 == 0, d4 == 0, d6 == 0 */
                    tmp10 = tmp13 = tmp11 = tmp12 = d0 << CONST_BITS;
                }
                else
                {
            /* d0 == 0, d2 == 0, d4 == 0, d6 == 0 */
                    tmp10 = tmp13 = tmp11 = tmp12 = 0;
                }
            }
        }
    }

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */
    if (d7)
    {
        if (d5)
        {
            if (d3)
            {
                if (d1)
                {
            /* d1 != 0, d3 != 0, d5 != 0, d7 != 0 */
                    z1 = d7 + d1;
                    z2 = d5 + d3;
                    z3 = d7 + d3;
                    z4 = d5 + d1;
                    z5 = MULTIPLY(z3 + z4, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, FIX(0.298631336));
                    tmp1 = MULTIPLY(d5, FIX(2.053119869));
                    tmp2 = MULTIPLY(d3, FIX(3.072711026));
                    tmp3 = MULTIPLY(d1, FIX(1.501321110));
                    z1 = MULTIPLY(z1, - FIX(0.899976223));
                    z2 = MULTIPLY(z2, - FIX(2.562915447));
                    z3 = MULTIPLY(z3, - FIX(1.961570560));
                    z4 = MULTIPLY(z4, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 += z1 + z3;
                    tmp1 += z2 + z4;
                    tmp2 += z2 + z3;
                    tmp3 += z1 + z4;
                }
                else
                {
            /* d1 == 0, d3 != 0, d5 != 0, d7 != 0 */
                    z2 = d5 + d3;
                    z3 = d7 + d3;
                    z5 = MULTIPLY(z3 + d5, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, FIX(0.298631336));
                    tmp1 = MULTIPLY(d5, FIX(2.053119869));
                    tmp2 = MULTIPLY(d3, FIX(3.072711026));
                    z1 = MULTIPLY(d7, - FIX(0.899976223));
                    z2 = MULTIPLY(z2, - FIX(2.562915447));
                    z3 = MULTIPLY(z3, - FIX(1.961570560));
                    z4 = MULTIPLY(d5, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 += z1 + z3;
                    tmp1 += z2 + z4;
                    tmp2 += z2 + z3;
                    tmp3 = z1 + z4;
                }
            }
            else
            {
                if (d1)
                {
            /* d1 != 0, d3 == 0, d5 != 0, d7 != 0 */
                    z1 = d7 + d1;
                    z4 = d5 + d1;
                    z5 = MULTIPLY(d7 + z4, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, FIX(0.298631336));
                    tmp1 = MULTIPLY(d5, FIX(2.053119869));
                    tmp3 = MULTIPLY(d1, FIX(1.501321110));
                    z1 = MULTIPLY(z1, - FIX(0.899976223));
                    z2 = MULTIPLY(d5, - FIX(2.562915447));
                    z3 = MULTIPLY(d7, - FIX(1.961570560));
                    z4 = MULTIPLY(z4, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 += z1 + z3;
                    tmp1 += z2 + z4;
                    tmp2 = z2 + z3;
                    tmp3 += z1 + z4;
                }
                else
                {
            /* d1 == 0, d3 == 0, d5 != 0, d7 != 0 */
                    z5 = MULTIPLY(d5 + d7, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, - FIX2(0.601344887));
                    tmp1 = MULTIPLY(d5, - FIX2(0.509795578));
                    z1 = MULTIPLY(d7, - FIX(0.899976223));
                    z3 = MULTIPLY(d7, - FIX(1.961570560));
                    z2 = MULTIPLY(d5, - FIX(2.562915447));
                    z4 = MULTIPLY(d5, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 += z3;
                    tmp1 += z4;
                    tmp2 = z2 + z3;
                    tmp3 = z1 + z4;
                }
            }
        }
        else
        {
            if (d3)
            {
                if (d1)
                {
            /* d1 != 0, d3 != 0, d5 == 0, d7 != 0 */
                    z1 = d7 + d1;
                    z3 = d7 + d3;
                    z5 = MULTIPLY(z3 + d1, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, FIX(0.298631336));
                    tmp2 = MULTIPLY(d3, FIX(3.072711026));
                    tmp3 = MULTIPLY(d1, FIX(1.501321110));
                    z1 = MULTIPLY(z1, - FIX(0.899976223));
                    z2 = MULTIPLY(d3, - FIX(2.562915447));
                    z3 = MULTIPLY(z3, - FIX(1.961570560));
                    z4 = MULTIPLY(d1, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 += z1 + z3;
                    tmp1 = z2 + z4;
                    tmp2 += z2 + z3;
                    tmp3 += z1 + z4;
                }
                else
                {
            /* d1 == 0, d3 != 0, d5 == 0, d7 != 0 */
                    z3 = d7 + d3;
                    z5 = MULTIPLY(z3, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, - FIX2(0.601344887));
                    z1 = MULTIPLY(d7, - FIX(0.899976223));
                    tmp2 = MULTIPLY(d3, FIX(0.509795579));
                    z2 = MULTIPLY(d3, - FIX(2.562915447));
                    z3 = MULTIPLY(z3, - FIX2(0.785694958));

                    tmp0 += z3;
                    tmp1 = z2 + z5;
                    tmp2 += z3;
                    tmp3 = z1 + z5;
                }
            }
            else
            {
                if (d1)
                {
            /* d1 != 0, d3 == 0, d5 == 0, d7 != 0 */
                    z1 = d7 + d1;
                    z5 = MULTIPLY(z1, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, - FIX2(1.662939224));
                    tmp3 = MULTIPLY(d1, FIX2(1.111140466));
                    z1 = MULTIPLY(z1, FIX2(0.275899379));
                    z3 = MULTIPLY(d7, - FIX(1.961570560));
                    z4 = MULTIPLY(d1, - FIX(0.390180644));

                    tmp0 += z1;
                    tmp1 = z4 + z5;
                    tmp2 = z3 + z5;
                    tmp3 += z1;
                }
                else
                {
            /* d1 == 0, d3 == 0, d5 == 0, d7 != 0 */
                    tmp0 = MULTIPLY(d7, - FIX2(1.387039845));
                    tmp1 = MULTIPLY(d7, FIX(1.175875602));
                    tmp2 = MULTIPLY(d7, - FIX2(0.785694958));
                    tmp3 = MULTIPLY(d7, FIX2(0.275899379));
                }
            }
        }
    }
    else
    {
        if (d5)
        {
            if (d3)
            {
                if (d1)
                {
            /* d1 != 0, d3 != 0, d5 != 0, d7 == 0 */
                    z2 = d5 + d3;
                    z4 = d5 + d1;
                    z5 = MULTIPLY(d3 + z4, FIX(1.175875602));

                    tmp1 = MULTIPLY(d5, FIX(2.053119869));
                    tmp2 = MULTIPLY(d3, FIX(3.072711026));
                    tmp3 = MULTIPLY(d1, FIX(1.501321110));
                    z1 = MULTIPLY(d1, - FIX(0.899976223));
                    z2 = MULTIPLY(z2, - FIX(2.562915447));
                    z3 = MULTIPLY(d3, - FIX(1.961570560));
                    z4 = MULTIPLY(z4, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 = z1 + z3;
                    tmp1 += z2 + z4;
                    tmp2 += z2 + z3;
                    tmp3 += z1 + z4;
                }
                else
                {
            /* d1 == 0, d3 != 0, d5 != 0, d7 == 0 */
                    z2 = d5 + d3;
                    z5 = MULTIPLY(z2, FIX(1.175875602));

                    tmp1 = MULTIPLY(d5, FIX2(1.662939225));
                    tmp2 = MULTIPLY(d3, FIX2(1.111140466));
                    z2 = MULTIPLY(z2, - FIX2(1.387039845));
                    z3 = MULTIPLY(d3, - FIX(1.961570560));
                    z4 = MULTIPLY(d5, - FIX(0.390180644));

                    tmp0 = z3 + z5;
                    tmp1 += z2;
                    tmp2 += z2;
                    tmp3 = z4 + z5;
                }
            }
            else
            {
                if (d1)
                {
            /* d1 != 0, d3 == 0, d5 != 0, d7 == 0 */
                    z4 = d5 + d1;
                    z5 = MULTIPLY(z4, FIX(1.175875602));

                    tmp1 = MULTIPLY(d5, - FIX2(0.509795578));
                    tmp3 = MULTIPLY(d1, FIX2(0.601344887));
                    z1 = MULTIPLY(d1, - FIX(0.899976223));
                    z2 = MULTIPLY(d5, - FIX(2.562915447));
                    z4 = MULTIPLY(z4, FIX2(0.785694958));

                    tmp0 = z1 + z5;
                    tmp1 += z4;
                    tmp2 = z2 + z5;
                    tmp3 += z4;
                }
                else
                {
            /* d1 == 0, d3 == 0, d5 != 0, d7 == 0 */
                    tmp0 = MULTIPLY(d5, FIX(1.175875602));
                    tmp1 = MULTIPLY(d5, FIX2(0.275899380));
                    tmp2 = MULTIPLY(d5, - FIX2(1.387039845));
                    tmp3 = MULTIPLY(d5, FIX2(0.785694958));
                }
            }
        }
        else
        {
            if (d3)
            {
                if (d1)
                {
            /* d1 != 0, d3 != 0, d5 == 0, d7 == 0 */
                    z5 = d3 + d1;

                    tmp2 = MULTIPLY(d3, - FIX(1.451774981));
                    tmp3 = MULTIPLY(d1, (FIX(0.211164243) - 1));
                    z1 = MULTIPLY(d1, FIX(1.061594337));
                    z2 = MULTIPLY(d3, - FIX(2.172734803));
                    z4 = MULTIPLY(z5, FIX(0.785694958));
                    z5 = MULTIPLY(z5, FIX(1.175875602));

                    tmp0 = z1 - z4;
                    tmp1 = z2 + z4;
                    tmp2 += z5;
                    tmp3 += z5;
                }
                else
                {
            /* d1 == 0, d3 != 0, d5 == 0, d7 == 0 */
                    tmp0 = MULTIPLY(d3, - FIX2(0.785694958));
                    tmp1 = MULTIPLY(d3, - FIX2(1.387039845));
                    tmp2 = MULTIPLY(d3, - FIX2(0.275899379));
                    tmp3 = MULTIPLY(d3, FIX(1.175875602));
                }
            }
            else
            {
                if (d1)
                {
            /* d1 != 0, d3 == 0, d5 == 0, d7 == 0 */
                    tmp0 = MULTIPLY(d1, FIX2(0.275899379));
                    tmp1 = MULTIPLY(d1, FIX2(0.785694958));
                    tmp2 = MULTIPLY(d1, FIX(1.175875602));
                    tmp3 = MULTIPLY(d1, FIX2(1.387039845));
                }
                else
                {
            /* d1 == 0, d3 == 0, d5 == 0, d7 == 0 */
                    tmp0 = tmp1 = tmp2 = tmp3 = 0;
                }
            }
        }
    }

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

    dataptr[DCTSIZE*0] = (dctelem_t) DESCALE(tmp10 + tmp3,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*7] = (dctelem_t) DESCALE(tmp10 - tmp3,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*1] = (dctelem_t) DESCALE(tmp11 + tmp2,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*6] = (dctelem_t) DESCALE(tmp11 - tmp2,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*2] = (dctelem_t) DESCALE(tmp12 + tmp1,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*5] = (dctelem_t) DESCALE(tmp12 - tmp1,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*3] = (dctelem_t) DESCALE(tmp13 + tmp0,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*4] = (dctelem_t) DESCALE(tmp13 - tmp0,
                       CONST_BITS+PASS1_BITS+3);

    dataptr++;             /* advance pointer to next column */
    }
}

static __inline__ void RestoreCPUState( )
{
    ;
}

#include "idct_sparse.h"
#include "idct_decl.h"
