/*****************************************************************************
 * idctmmx.c : MMX IDCT module
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: idctmmx.c,v 1.9 2001/04/15 04:19:57 sam Exp $
 *
 * Authors: Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *          Michel Lespinasse <walken@zoy.org>
 *          Peter Gubanov <peter@elecard.net.ru>
 *          (from the LiViD project)
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#define MODULE_NAME idctmmx

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"                                              /* TestCPU() */

#include "video.h"
#include "video_output.h"

#include "video_decoder.h"

#include "modules.h"
#include "modules_inner.h"

#include "idct.h"

#include "attributes.h"
#include "mmx.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list );
static int  idct_Probe      ( probedata_t *p_data );
static void vdec_NormScan   ( u8 ppi_scan[2][64] );


/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for MMX IDCT module" )
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
MODULE_INIT
{
    p_module->psz_name = MODULE_STRING;
    p_module->psz_longname = "MMX IDCT module";
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
MODULE_ACTIVATE
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
MODULE_DEACTIVATE
{
    free( p_module->p_functions );

    return( 0 );
}

/* Following functions are local */

/*****************************************************************************
 * Functions exported as capabilities.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = idct_Probe;
    p_function_list->functions.idct.pf_init = _M( vdec_InitIDCT );
    p_function_list->functions.idct.pf_sparse_idct = _M( vdec_SparseIDCT );
    p_function_list->functions.idct.pf_idct = _M( vdec_IDCT );
    p_function_list->functions.idct.pf_norm_scan = vdec_NormScan;
}

/*****************************************************************************
 * idct_Probe: return a preference score
 *****************************************************************************/
static int idct_Probe( probedata_t *p_data )
{
    if( TestCPU( CPU_CAPABILITY_MMX ) )
    {
        if( TestMethod( IDCT_METHOD_VAR, "idctmmx" ) )
        {
            return( 999 );
        }
        else
        {
            return( 150 );
        }
    }
    else
    {
        return( 0 );
    }
}

/*****************************************************************************
 * vdec_NormScan : This IDCT uses reordered coeffs, so we patch the scan table
 *****************************************************************************/
static void vdec_NormScan( u8 ppi_scan[2][64] )
{
    int     i, j;

    for( i = 0; i < 64; i++ )
    {
        j = ppi_scan[0][i];
        ppi_scan[0][i] = (j & 0x38) | ((j & 6) >> 1) | ((j & 1) << 2);

        j = ppi_scan[1][i];
        ppi_scan[1][i] = (j & 0x38) | ((j & 6) >> 1) | ((j & 1) << 2);
    }
}

/*****************************************************************************
 * vdec_IDCT :
 *****************************************************************************/
#define ROW_SHIFT 11
#define COL_SHIFT 6

#define round(bias) ((int)(((bias)+0.5) * (1<<ROW_SHIFT)))
#define rounder(bias) {round (bias), round (bias)}

#define table(c1,c2,c3,c4,c5,c6,c7) {  c4,  c2,  c4,  c6,   \
                                       c4,  c6, -c4, -c2,   \
                                       c1,  c3,  c3, -c7,   \
                                       c5,  c7, -c1, -c5,   \
                                       c4, -c6,  c4, -c2,   \
                                      -c4,  c2,  c4, -c6,   \
                                       c5, -c1,  c7, -c5,   \
                                       c7,  c3,  c3, -c1 }

static __inline__ void RowHead( dctelem_t * row, int offset, dctelem_t * table )
{
    movq_m2r (*(row+offset), mm2);      // mm2 = x6 x4 x2 x0

    movq_m2r (*(row+offset+4), mm5);    // mm5 = x7 x5 x3 x1
    movq_r2r (mm2, mm0);                // mm0 = x6 x4 x2 x0

    movq_m2r (*table, mm3);             // mm3 = C6 C4 C2 C4
    movq_r2r (mm5, mm6);                // mm6 = x7 x5 x3 x1

    punpckldq_r2r (mm0, mm0);           // mm0 = x2 x0 x2 x0

    movq_m2r (*(table+4), mm4);         // mm4 = -C2 -C4 C6 C4
    pmaddwd_r2r (mm0, mm3);             // mm3 = C4*x0+C6*x2 C4*x0+C2*x2

    movq_m2r (*(table+8), mm1);         // mm1 = -C7 C3 C3 C1
    punpckhdq_r2r (mm2, mm2);           // mm2 = x6 x4 x6 x4
}

static __inline__ void Row( dctelem_t * table, s32 * rounder )
{
    pmaddwd_r2r (mm2, mm4);             // mm4 = -C4*x4-C2*x6 C4*x4+C6*x6
    punpckldq_r2r (mm5, mm5);           // mm5 = x3 x1 x3 x1

    pmaddwd_m2r (*(table+16), mm0);     // mm0 = C4*x0-C2*x2 C4*x0-C6*x2
    punpckhdq_r2r (mm6, mm6);           // mm6 = x7 x5 x7 x5

    movq_m2r (*(table+12), mm7);        // mm7 = -C5 -C1 C7 C5
    pmaddwd_r2r (mm5, mm1);             // mm1 = C3*x1-C7*x3 C1*x1+C3*x3

    paddd_m2r (*rounder, mm3);          // mm3 += rounder
    pmaddwd_r2r (mm6, mm7);             // mm7 = -C1*x5-C5*x7 C5*x5+C7*x7

    pmaddwd_m2r (*(table+20), mm2);     // mm2 = C4*x4-C6*x6 -C4*x4+C2*x6
    paddd_r2r (mm4, mm3);               // mm3 = a1 a0 + rounder

    pmaddwd_m2r (*(table+24), mm5);     // mm5 = C7*x1-C5*x3 C5*x1-C1*x3
    movq_r2r (mm3, mm4);                // mm4 = a1 a0 + rounder

    pmaddwd_m2r (*(table+28), mm6);     // mm6 = C3*x5-C1*x7 C7*x5+C3*x7
    paddd_r2r (mm7, mm1);               // mm1 = b1 b0

    paddd_m2r (*rounder, mm0);          // mm0 += rounder
    psubd_r2r (mm1, mm3);               // mm3 = a1-b1 a0-b0 + rounder

    psrad_i2r (ROW_SHIFT, mm3);         // mm3 = y6 y7
    paddd_r2r (mm4, mm1);               // mm1 = a1+b1 a0+b0 + rounder

    paddd_r2r (mm2, mm0);               // mm0 = a3 a2 + rounder
    psrad_i2r (ROW_SHIFT, mm1);         // mm1 = y1 y0

    paddd_r2r (mm6, mm5);               // mm5 = b3 b2
    movq_r2r (mm0, mm7);                // mm7 = a3 a2 + rounder

    paddd_r2r (mm5, mm0);               // mm0 = a3+b3 a2+b2 + rounder
    psubd_r2r (mm5, mm7);               // mm7 = a3-b3 a2-b2 + rounder
}

static __inline__ void RowTail( dctelem_t * row, int store )
{
    psrad_i2r (ROW_SHIFT, mm0);         // mm0 = y3 y2

    psrad_i2r (ROW_SHIFT, mm7);         // mm7 = y4 y5

    packssdw_r2r (mm0, mm1);            // mm1 = y3 y2 y1 y0

    packssdw_r2r (mm3, mm7);            // mm7 = y6 y7 y4 y5

    movq_r2m (mm1, *(row+store));       // save y3 y2 y1 y0
    movq_r2r (mm7, mm4);                // mm4 = y6 y7 y4 y5

    pslld_i2r (16, mm7);                // mm7 = y7 0 y5 0

    psrld_i2r (16, mm4);                // mm4 = 0 y6 0 y4

    por_r2r (mm4, mm7);                 // mm7 = y7 y6 y5 y4

    // slot

    movq_r2m (mm7, *(row+store+4));     // save y7 y6 y5 y4
}

static __inline__ void RowMid( dctelem_t * row, int store,
                               int offset, dctelem_t * table )
{
    movq_m2r (*(row+offset), mm2);      // mm2 = x6 x4 x2 x0
    psrad_i2r (ROW_SHIFT, mm0);         // mm0 = y3 y2

    movq_m2r (*(row+offset+4), mm5);    // mm5 = x7 x5 x3 x1
    psrad_i2r (ROW_SHIFT, mm7);         // mm7 = y4 y5

    packssdw_r2r (mm0, mm1);            // mm1 = y3 y2 y1 y0
    movq_r2r (mm5, mm6);                // mm6 = x7 x5 x3 x1

    packssdw_r2r (mm3, mm7);            // mm7 = y6 y7 y4 y5
    movq_r2r (mm2, mm0);                // mm0 = x6 x4 x2 x0

    movq_r2m (mm1, *(row+store));       // save y3 y2 y1 y0
    movq_r2r (mm7, mm1);                // mm1 = y6 y7 y4 y5

    punpckldq_r2r (mm0, mm0);           // mm0 = x2 x0 x2 x0
    psrld_i2r (16, mm7);                // mm7 = 0 y6 0 y4

    movq_m2r (*table, mm3);             // mm3 = C6 C4 C2 C4
    pslld_i2r (16, mm1);                // mm1 = y7 0 y5 0

    movq_m2r (*(table+4), mm4);         // mm4 = -C2 -C4 C6 C4
    por_r2r (mm1, mm7);                 // mm7 = y7 y6 y5 y4

    movq_m2r (*(table+8), mm1);         // mm1 = -C7 C3 C3 C1
    punpckhdq_r2r (mm2, mm2);           // mm2 = x6 x4 x6 x4

    movq_r2m (mm7, *(row+store+4));     // save y7 y6 y5 y4
    pmaddwd_r2r (mm0, mm3);             // mm3 = C4*x0+C6*x2 C4*x0+C2*x2
}

static __inline__ void Col( dctelem_t * col, int offset )
{
#define T1 13036
#define T2 27146
#define T3 43790
#define C4 23170

    static short _T1[] ATTR_ALIGN(8) = {T1,T1,T1,T1};
    static short _T2[] ATTR_ALIGN(8) = {T2,T2,T2,T2};
    static short _T3[] ATTR_ALIGN(8) = {T3,T3,T3,T3};
    static short _C4[] ATTR_ALIGN(8) = {C4,C4,C4,C4};
    static mmx_t scratch0, scratch1;

    /* column code adapted from peter gubanov */
    /* http://www.elecard.com/peter/idct.shtml */

    movq_m2r (*_T1, mm0);               // mm0 = T1

    movq_m2r (*(col+offset+1*8), mm1);  // mm1 = x1
    movq_r2r (mm0, mm2);                // mm2 = T1

    movq_m2r (*(col+offset+7*8), mm4);  // mm4 = x7
    pmulhw_r2r (mm1, mm0);              // mm0 = T1*x1

    movq_m2r (*_T3, mm5);               // mm5 = T3
    pmulhw_r2r (mm4, mm2);              // mm2 = T1*x7

    movq_m2r (*(col+offset+5*8), mm6);  // mm6 = x5
    movq_r2r (mm5, mm7);                // mm7 = T3-1

    movq_m2r (*(col+offset+3*8), mm3);  // mm3 = x3
    psubsw_r2r (mm4, mm0);              // mm0 = v17

    movq_m2r (*_T2, mm4);               // mm4 = T2
    pmulhw_r2r (mm3, mm5);              // mm5 = (T3-1)*x3

    paddsw_r2r (mm2, mm1);              // mm1 = u17
    pmulhw_r2r (mm6, mm7);              // mm7 = (T3-1)*x5

    //slot

    movq_r2r (mm4, mm2);                // mm2 = T2
    paddsw_r2r (mm3, mm5);              // mm5 = T3*x3

    pmulhw_m2r (*(col+offset+2*8), mm4);// mm4 = T2*x2
    paddsw_r2r (mm6, mm7);              // mm7 = T3*x5

    psubsw_r2r (mm6, mm5);              // mm5 = v35
    paddsw_r2r (mm3, mm7);              // mm7 = u35

    movq_m2r (*(col+offset+6*8), mm3);  // mm3 = x6
    movq_r2r (mm0, mm6);                // mm6 = v17

    pmulhw_r2r (mm3, mm2);              // mm2 = T2*x6
    psubsw_r2r (mm5, mm0);              // mm0 = b3

    psubsw_r2r (mm3, mm4);              // mm4 = v26
    paddsw_r2r (mm6, mm5);              // mm5 = v12

    movq_r2m (mm0, scratch0);           // save b3
    movq_r2r (mm1, mm6);                // mm6 = u17

    paddsw_m2r (*(col+offset+2*8), mm2);// mm2 = u26
    paddsw_r2r (mm7, mm6);              // mm6 = b0

    psubsw_r2r (mm7, mm1);              // mm1 = u12
    movq_r2r (mm1, mm7);                // mm7 = u12

    movq_m2r (*(col+offset+0*8), mm3);  // mm3 = x0
    paddsw_r2r (mm5, mm1);              // mm1 = u12+v12

    movq_m2r (*_C4, mm0);               // mm0 = C4/2
    psubsw_r2r (mm5, mm7);              // mm7 = u12-v12

    movq_r2m (mm6, scratch1);           // save b0
    pmulhw_r2r (mm0, mm1);              // mm1 = b1/2

    movq_r2r (mm4, mm6);                // mm6 = v26
    pmulhw_r2r (mm0, mm7);              // mm7 = b2/2

    movq_m2r (*(col+offset+4*8), mm5);  // mm5 = x4
    movq_r2r (mm3, mm0);                // mm0 = x0

    psubsw_r2r (mm5, mm3);              // mm3 = v04
    paddsw_r2r (mm5, mm0);              // mm0 = u04

    paddsw_r2r (mm3, mm4);              // mm4 = a1
    movq_r2r (mm0, mm5);                // mm5 = u04

    psubsw_r2r (mm6, mm3);              // mm3 = a2
    paddsw_r2r (mm2, mm5);              // mm5 = a0

    paddsw_r2r (mm1, mm1);              // mm1 = b1
    psubsw_r2r (mm2, mm0);              // mm0 = a3

    paddsw_r2r (mm7, mm7);              // mm7 = b2
    movq_r2r (mm3, mm2);                // mm2 = a2

    movq_r2r (mm4, mm6);                // mm6 = a1
    paddsw_r2r (mm7, mm3);              // mm3 = a2+b2

    psraw_i2r (COL_SHIFT, mm3);         // mm3 = y2
    paddsw_r2r (mm1, mm4);              // mm4 = a1+b1

    psraw_i2r (COL_SHIFT, mm4);         // mm4 = y1
    psubsw_r2r (mm1, mm6);              // mm6 = a1-b1

    movq_m2r (scratch1, mm1);           // mm1 = b0
    psubsw_r2r (mm7, mm2);              // mm2 = a2-b2

    psraw_i2r (COL_SHIFT, mm6);         // mm6 = y6
    movq_r2r (mm5, mm7);                // mm7 = a0

    movq_r2m (mm4, *(col+offset+1*8));  // save y1
    psraw_i2r (COL_SHIFT, mm2);         // mm2 = y5

    movq_r2m (mm3, *(col+offset+2*8));  // save y2
    paddsw_r2r (mm1, mm5);              // mm5 = a0+b0

    movq_m2r (scratch0, mm4);           // mm4 = b3
    psubsw_r2r (mm1, mm7);              // mm7 = a0-b0

    psraw_i2r (COL_SHIFT, mm5);         // mm5 = y0
    movq_r2r (mm0, mm3);                // mm3 = a3

    movq_r2m (mm2, *(col+offset+5*8));  // save y5
    psubsw_r2r (mm4, mm3);              // mm3 = a3-b3

    psraw_i2r (COL_SHIFT, mm7);         // mm7 = y7
    paddsw_r2r (mm0, mm4);              // mm4 = a3+b3

    movq_r2m (mm5, *(col+offset+0*8));  // save y0
    psraw_i2r (COL_SHIFT, mm3);         // mm3 = y4

    movq_r2m (mm6, *(col+offset+6*8));  // save y6
    psraw_i2r (COL_SHIFT, mm4);         // mm4 = y3

    movq_r2m (mm7, *(col+offset+7*8));  // save y7

    movq_r2m (mm3, *(col+offset+4*8));  // save y4

    movq_r2m (mm4, *(col+offset+3*8));  // save y3
}


static s32 rounder0[] ATTR_ALIGN(8) =
    rounder ((1 << (COL_SHIFT - 1)) - 0.5);
static s32 rounder4[] ATTR_ALIGN(8) = rounder (0);
static s32 rounder1[] ATTR_ALIGN(8) =
    rounder (1.25683487303);    // C1*(C1/C4+C1+C7)/2
static s32 rounder7[] ATTR_ALIGN(8) =
    rounder (-0.25);            // C1*(C7/C4+C7-C1)/2
static s32 rounder2[] ATTR_ALIGN(8) =
    rounder (0.60355339059);    // C2 * (C6+C2)/2
static s32 rounder6[] ATTR_ALIGN(8) =
    rounder (-0.25);            // C2 * (C6-C2)/2
static s32 rounder3[] ATTR_ALIGN(8) =
    rounder (0.087788325588);   // C3*(-C3/C4+C3+C5)/2
static s32 rounder5[] ATTR_ALIGN(8) =
    rounder (-0.441341716183);  // C3*(-C5/C4+C5-C3)/2

void _M( vdec_IDCT )( vdec_thread_t * p_vdec, dctelem_t * p_block,
                      int i_idontcare )
{
    static dctelem_t table04[] ATTR_ALIGN(16) =
        table (22725, 21407, 19266, 16384, 12873,  8867, 4520);
    static dctelem_t table17[] ATTR_ALIGN(16) =
        table (31521, 29692, 26722, 22725, 17855, 12299, 6270);
    static dctelem_t table26[] ATTR_ALIGN(16) =
        table (29692, 27969, 25172, 21407, 16819, 11585, 5906);
    static dctelem_t table35[] ATTR_ALIGN(16) =
        table (26722, 25172, 22654, 19266, 15137, 10426, 5315);

    RowHead( p_block, 0*8, table04 );
    Row( table04, rounder0 );
    RowMid( p_block, 0*8, 4*8, table04 );
    Row( table04, rounder4 );
    RowMid( p_block, 4*8, 1*8, table17 );
    Row( table17, rounder1 );
    RowMid( p_block, 1*8, 7*8, table17 );
    Row( table17, rounder7 );
    RowMid( p_block, 7*8, 2*8, table26 );
    Row( table26, rounder2 );
    RowMid( p_block, 2*8, 6*8, table26 );
    Row( table26, rounder6 );
    RowMid( p_block, 6*8, 3*8, table35 );
    Row( table35, rounder3 );
    RowMid( p_block, 3*8, 5*8, table35 );
    Row( table35, rounder5 );
    RowTail( p_block, 5*8);

    Col( p_block, 0 );
    Col( p_block, 4 );
}

