/*****************************************************************************
 * idctclassic.c : Classic IDCT module
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: idctclassic.c,v 1.5 2001/01/17 18:17:30 massiot Exp $
 *
 * Authors: Gaël Hendryckx <jimmy@via.ecp.fr>
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

#define MODULE_NAME idctclassic

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "video.h"
#include "video_output.h"

#include "video_decoder.h"

#include "modules.h"
#include "modules_inner.h"

#include "idct.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list );
static int  idct_Probe      ( probedata_t *p_data );
static void vdec_NormScan   ( u8 ppi_scan[2][64] );


/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for classic IDCT module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_END

/*****************************************************************************
 * InitModule: get the module structure and configuration.
 *****************************************************************************
 * We have to fill psz_name, psz_longname and psz_version. These variables
 * will be strdup()ed later by the main application because the module can
 * be unloaded later to save memory, and we want to be able to access this
 * data even after the module has been unloaded.
 *****************************************************************************/
int InitModule( module_t * p_module )
{
    p_module->psz_name = MODULE_STRING;
    p_module->psz_longname = "classic C IDCT module";
    p_module->psz_version = VERSION;

    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_IDCT;

    return( 0 );
}

/*****************************************************************************
 * ActivateModule: set the module to an usable state.
 *****************************************************************************
 * This function fills the capability functions and the configuration
 * structure. Once ActivateModule() has been called, the i_usage can
 * be set to 0 and calls to NeedModule() be made to increment it. To unload
 * the module, one has to wait until i_usage == 0 and call DeactivateModule().
 *****************************************************************************/
int ActivateModule( module_t * p_module )
{
    p_module->p_functions = malloc( sizeof( module_functions_t ) );
    if( p_module->p_functions == NULL )
    {
        return( -1 );
    }

    idct_getfunctions( &p_module->p_functions->idct );

    p_module->p_config = p_config;

    return( 0 );
}

/*****************************************************************************
 * DeactivateModule: make sure the module can be unloaded.
 *****************************************************************************
 * This function must only be called when i_usage == 0. If it successfully
 * returns, i_usage can be set to -1 and the module unloaded. Be careful to
 * lock usage_lock during the whole process.
 *****************************************************************************/
int DeactivateModule( module_t * p_module )
{
    free( p_module->p_functions );

    return( 0 );
}

/* Following functions are local */

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = idct_Probe;
    p_function_list->functions.idct.pf_init = vdec_InitIDCT;
    p_function_list->functions.idct.pf_sparse_idct = vdec_SparseIDCT;
    p_function_list->functions.idct.pf_idct = vdec_IDCT;
    p_function_list->functions.idct.pf_norm_scan = vdec_NormScan;
}

/*****************************************************************************
 * idct_Probe: returns a preference score
 *****************************************************************************/
static int idct_Probe( probedata_t *p_data )
{
    if( TestMethod( IDCT_METHOD_VAR, "idctclassic" ) )
    {
        return( 999 );
    }

    /* This plugin always works */
    return( 100 );
}

/*****************************************************************************
 * vdec_NormScan : Unused in this IDCT
 *****************************************************************************/
static void vdec_NormScan( u8 ppi_scan[2][64] )
{
}

/*****************************************************************************
 * vdec_IDCT : IDCT function for normal matrices
 *****************************************************************************/
void vdec_IDCT( vdec_thread_t * p_vdec, dctelem_t * p_block,
                int i_idontcare )
{
    /* dct classique: pour tester la meilleure entre la classique et la */
    /* no classique */
    s32 tmp0, tmp1, tmp2, tmp3;
    s32 tmp10, tmp11, tmp12, tmp13;
    s32 z1, z2, z3, z4, z5;
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

        if ((dataptr[1] | dataptr[2] | dataptr[3] | dataptr[4] |
                dataptr[5] | dataptr[6] | dataptr[7]) == 0)
        {
      /* AC terms all zero */
            dctelem_t dcval = (dctelem_t) (dataptr[0] << PASS1_BITS);

            dataptr[0] = dcval;
            dataptr[1] = dcval;
            dataptr[2] = dcval;
            dataptr[3] = dcval;
            dataptr[4] = dcval;
            dataptr[5] = dcval;
            dataptr[6] = dcval;
            dataptr[7] = dcval;

            dataptr += DCTSIZE; /* advance pointer to next row */
            continue;
        }

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */

        z2 = (s32) dataptr[2];
        z3 = (s32) dataptr[6];

        z1 = MULTIPLY(z2 + z3, FIX(0.541196100));
        tmp2 = z1 + MULTIPLY(z3, - FIX(1.847759065));
        tmp3 = z1 + MULTIPLY(z2, FIX(0.765366865));

        tmp0 = ((s32) dataptr[0] + (s32) dataptr[4]) << CONST_BITS;
        tmp1 = ((s32) dataptr[0] - (s32) dataptr[4]) << CONST_BITS;

        tmp10 = tmp0 + tmp3;
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

        tmp0 = (s32) dataptr[7];
        tmp1 = (s32) dataptr[5];
        tmp2 = (s32) dataptr[3];
        tmp3 = (s32) dataptr[1];

        z1 = tmp0 + tmp3;
        z2 = tmp1 + tmp2;
        z3 = tmp0 + tmp2;
        z4 = tmp1 + tmp3;
        z5 = MULTIPLY(z3 + z4, FIX(1.175875602)); /* sqrt(2) * c3 */

        tmp0 = MULTIPLY(tmp0, FIX(0.298631336)); /* sqrt(2) * (-c1+c3+c5-c7) */
        tmp1 = MULTIPLY(tmp1, FIX(2.053119869)); /* sqrt(2) * ( c1+c3-c5+c7) */
        tmp2 = MULTIPLY(tmp2, FIX(3.072711026)); /* sqrt(2) * ( c1+c3+c5-c7) */
        tmp3 = MULTIPLY(tmp3, FIX(1.501321110)); /* sqrt(2) * ( c1+c3-c5-c7) */
        z1 = MULTIPLY(z1, - FIX(0.899976223)); /* sqrt(2) * (c7-c3) */
        z2 = MULTIPLY(z2, - FIX(2.562915447)); /* sqrt(2) * (-c1-c3) */
        z3 = MULTIPLY(z3, - FIX(1.961570560)); /* sqrt(2) * (-c3-c5) */
        z4 = MULTIPLY(z4, - FIX(0.390180644)); /* sqrt(2) * (c5-c3) */

        z3 += z5;
        z4 += z5;

        tmp0 += z1 + z3;
        tmp1 += z2 + z4;
        tmp2 += z2 + z3;
        tmp3 += z1 + z4;

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

        dataptr[0] = (dctelem_t) DESCALE(tmp10 + tmp3, CONST_BITS-PASS1_BITS);
        dataptr[7] = (dctelem_t) DESCALE(tmp10 - tmp3, CONST_BITS-PASS1_BITS);
        dataptr[1] = (dctelem_t) DESCALE(tmp11 + tmp2, CONST_BITS-PASS1_BITS);
        dataptr[6] = (dctelem_t) DESCALE(tmp11 - tmp2, CONST_BITS-PASS1_BITS);
        dataptr[2] = (dctelem_t) DESCALE(tmp12 + tmp1, CONST_BITS-PASS1_BITS);
        dataptr[5] = (dctelem_t) DESCALE(tmp12 - tmp1, CONST_BITS-PASS1_BITS);
        dataptr[3] = (dctelem_t) DESCALE(tmp13 + tmp0, CONST_BITS-PASS1_BITS);
        dataptr[4] = (dctelem_t) DESCALE(tmp13 - tmp0, CONST_BITS-PASS1_BITS);

        dataptr += DCTSIZE;             /* advance pointer to next row */
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

#ifndef NO_ZERO_COLUMN_TEST /*ajoute un test mais evite des calculs */
        if ((dataptr[DCTSIZE*1] | dataptr[DCTSIZE*2] | dataptr[DCTSIZE*3] |
            dataptr[DCTSIZE*4] | dataptr[DCTSIZE*5] | dataptr[DCTSIZE*6] |
            dataptr[DCTSIZE*7]) == 0)
        {
      /* AC terms all zero */
            dctelem_t dcval = (dctelem_t) DESCALE((s32) dataptr[0], PASS1_BITS+3);

            dataptr[DCTSIZE*0] = dcval;
            dataptr[DCTSIZE*1] = dcval;
            dataptr[DCTSIZE*2] = dcval;
            dataptr[DCTSIZE*3] = dcval;
            dataptr[DCTSIZE*4] = dcval;
            dataptr[DCTSIZE*5] = dcval;
            dataptr[DCTSIZE*6] = dcval;
            dataptr[DCTSIZE*7] = dcval;

            dataptr++;          /* advance pointer to next column */
            continue;
        }
#endif

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */

        z2 = (s32) dataptr[DCTSIZE*2];
        z3 = (s32) dataptr[DCTSIZE*6];

        z1 = MULTIPLY(z2 + z3, FIX(0.541196100));
        tmp2 = z1 + MULTIPLY(z3, - FIX(1.847759065));
        tmp3 = z1 + MULTIPLY(z2, FIX(0.765366865));

        tmp0 = ((s32) dataptr[DCTSIZE*0] + (s32) dataptr[DCTSIZE*4]) << CONST_BITS;
        tmp1 = ((s32) dataptr[DCTSIZE*0] - (s32) dataptr[DCTSIZE*4]) << CONST_BITS;

        tmp10 = tmp0 + tmp3;
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

        tmp0 = (s32) dataptr[DCTSIZE*7];
        tmp1 = (s32) dataptr[DCTSIZE*5];
        tmp2 = (s32) dataptr[DCTSIZE*3];
        tmp3 = (s32) dataptr[DCTSIZE*1];

        z1 = tmp0 + tmp3;
        z2 = tmp1 + tmp2;
        z3 = tmp0 + tmp2;
        z4 = tmp1 + tmp3;
        z5 = MULTIPLY(z3 + z4, FIX(1.175875602)); /* sqrt(2) * c3 */

        tmp0 = MULTIPLY(tmp0, FIX(0.298631336)); /* sqrt(2) * (-c1+c3+c5-c7) */
        tmp1 = MULTIPLY(tmp1, FIX(2.053119869)); /* sqrt(2) * ( c1+c3-c5+c7) */
        tmp2 = MULTIPLY(tmp2, FIX(3.072711026)); /* sqrt(2) * ( c1+c3+c5-c7) */
        tmp3 = MULTIPLY(tmp3, FIX(1.501321110)); /* sqrt(2) * ( c1+c3-c5-c7) */
        z1 = MULTIPLY(z1, - FIX(0.899976223)); /* sqrt(2) * (c7-c3) */
        z2 = MULTIPLY(z2, - FIX(2.562915447)); /* sqrt(2) * (-c1-c3) */
        z3 = MULTIPLY(z3, - FIX(1.961570560)); /* sqrt(2) * (-c3-c5) */
        z4 = MULTIPLY(z4, - FIX(0.390180644)); /* sqrt(2) * (c5-c3) */

        z3 += z5;
        z4 += z5;

        tmp0 += z1 + z3;
        tmp1 += z2 + z4;
        tmp2 += z2 + z3;
        tmp3 += z1 + z4;

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

        dataptr++;                      /* advance pointer to next column */
    }
}

