/*****************************************************************************
 * motionmmx.c : MMX motion compensation module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: motionmmx.c,v 1.12 2001/11/28 15:08:05 massiot Exp $
 *
 * Authors: Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *          Michel Lespinasse <walken@zoy.org>
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

#define MODULE_NAME motionmmx
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "mmx.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void motion_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for MMX motion compensation module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_MOTION;
    p_module->psz_longname = "MMX motion compensation module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    motion_getfunctions( &p_module->p_functions->motion );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * motion_Probe: tests probe the CPU and return a score
 *****************************************************************************/
static int motion_Probe( probedata_t *p_data )
{
    if( !TestCPU( CPU_CAPABILITY_MMX ) )
    {
        return( 0 );
    }

    if( TestMethod( MOTION_METHOD_VAR, "motionmmx" )
         || TestMethod( MOTION_METHOD_VAR, "mmx" ) )
    {
        return( 999 );
    }

    return( 150 );
}

/*****************************************************************************
 * Motion compensation in MMX
 *****************************************************************************/

// some rounding constants
mmx_t round1 = {0x0001000100010001LL};
mmx_t round4 = {0x0002000200020002LL};

/*
 * This code should probably be compiled with loop unrolling
 * (ie, -funroll-loops in gcc)becuase some of the loops
 * use a small static number of iterations. This was written
 * with the assumption the compiler knows best about when
 * unrolling will help
 */

static __inline__ void mmx_zero_reg ()
{
    // load 0 into mm0
    pxor_r2r (mm0, mm0);
}

static __inline__ void mmx_average_2_U8 (yuv_data_t * dest,
                                     yuv_data_t * src1, yuv_data_t * src2)
{
    //
    // *dest = (*src1 + *src2 + 1)/ 2;
    //

    movq_m2r (*src1, mm1);        // load 8 src1 bytes
    movq_r2r (mm1, mm2);        // copy 8 src1 bytes

    movq_m2r (*src2, mm3);        // load 8 src2 bytes
    movq_r2r (mm3, mm4);        // copy 8 src2 bytes

    punpcklbw_r2r (mm0, mm1);        // unpack low src1 bytes
    punpckhbw_r2r (mm0, mm2);        // unpack high src1 bytes

    punpcklbw_r2r (mm0, mm3);        // unpack low src2 bytes
    punpckhbw_r2r (mm0, mm4);        // unpack high src2 bytes

    paddw_r2r (mm3, mm1);        // add lows to mm1
    paddw_m2r (round1, mm1);
    psraw_i2r (1, mm1);                // /2

    paddw_r2r (mm4, mm2);        // add highs to mm2
    paddw_m2r (round1, mm2);
    psraw_i2r (1, mm2);                // /2

    packuswb_r2r (mm2, mm1);        // pack (w/ saturation)
    movq_r2m (mm1, *dest);        // store result in dest
}

static __inline__ void mmx_interp_average_2_U8 (yuv_data_t * dest,
                                            yuv_data_t * src1, yuv_data_t * src2)
{
    //
    // *dest = (*dest + (*src1 + *src2 + 1)/ 2 + 1)/ 2;
    //

    movq_m2r (*dest, mm1);        // load 8 dest bytes
    movq_r2r (mm1, mm2);        // copy 8 dest bytes

    movq_m2r (*src1, mm3);        // load 8 src1 bytes
    movq_r2r (mm3, mm4);        // copy 8 src1 bytes

    movq_m2r (*src2, mm5);        // load 8 src2 bytes
    movq_r2r (mm5, mm6);        // copy 8 src2 bytes

    punpcklbw_r2r (mm0, mm1);        // unpack low dest bytes
    punpckhbw_r2r (mm0, mm2);        // unpack high dest bytes

    punpcklbw_r2r (mm0, mm3);        // unpack low src1 bytes
    punpckhbw_r2r (mm0, mm4);        // unpack high src1 bytes

    punpcklbw_r2r (mm0, mm5);        // unpack low src2 bytes
    punpckhbw_r2r (mm0, mm6);        // unpack high src2 bytes

    paddw_r2r (mm5, mm3);        // add lows
    paddw_m2r (round1, mm3);
    psraw_i2r (1, mm3);                // /2

    paddw_r2r (mm6, mm4);        // add highs
    paddw_m2r (round1, mm4);
    psraw_i2r (1, mm4);                // /2

    paddw_r2r (mm3, mm1);        // add lows
    paddw_m2r (round1, mm1);
    psraw_i2r (1, mm1);                // /2

    paddw_r2r (mm4, mm2);        // add highs
    paddw_m2r (round1, mm2);
    psraw_i2r (1, mm2);                // /2

    packuswb_r2r (mm2, mm1);        // pack (w/ saturation)
    movq_r2m (mm1, *dest);        // store result in dest
}

static __inline__ void mmx_average_4_U8 (yuv_data_t * dest,
                                     yuv_data_t * src1, yuv_data_t * src2,
                                     yuv_data_t * src3, yuv_data_t * src4)
{
    //
    // *dest = (*src1 + *src2 + *src3 + *src4 + 2)/ 4;
    //

    movq_m2r (*src1, mm1);        // load 8 src1 bytes
    movq_r2r (mm1, mm2);        // copy 8 src1 bytes

    punpcklbw_r2r (mm0, mm1);        // unpack low src1 bytes
    punpckhbw_r2r (mm0, mm2);        // unpack high src1 bytes

    movq_m2r (*src2, mm3);        // load 8 src2 bytes
    movq_r2r (mm3, mm4);        // copy 8 src2 bytes

    punpcklbw_r2r (mm0, mm3);        // unpack low src2 bytes
    punpckhbw_r2r (mm0, mm4);        // unpack high src2 bytes

    paddw_r2r (mm3, mm1);        // add lows
    paddw_r2r (mm4, mm2);        // add highs

    // now have partials in mm1 and mm2

    movq_m2r (*src3, mm3);        // load 8 src3 bytes
    movq_r2r (mm3, mm4);        // copy 8 src3 bytes

    punpcklbw_r2r (mm0, mm3);        // unpack low src3 bytes
    punpckhbw_r2r (mm0, mm4);        // unpack high src3 bytes

    paddw_r2r (mm3, mm1);        // add lows
    paddw_r2r (mm4, mm2);        // add highs

    movq_m2r (*src4, mm5);        // load 8 src4 bytes
    movq_r2r (mm5, mm6);        // copy 8 src4 bytes

    punpcklbw_r2r (mm0, mm5);        // unpack low src4 bytes
    punpckhbw_r2r (mm0, mm6);        // unpack high src4 bytes

    paddw_r2r (mm5, mm1);        // add lows
    paddw_r2r (mm6, mm2);        // add highs

    // now have subtotal in mm1 and mm2

    paddw_m2r (round4, mm1);
    psraw_i2r (2, mm1);                // /4
    paddw_m2r (round4, mm2);
    psraw_i2r (2, mm2);                // /4

    packuswb_r2r (mm2, mm1);        // pack (w/ saturation)
    movq_r2m (mm1, *dest);        // store result in dest
}

static __inline__ void mmx_interp_average_4_U8 (yuv_data_t * dest,
                                            yuv_data_t * src1, yuv_data_t * src2,
                                            yuv_data_t * src3, yuv_data_t * src4)
{
    //
    // *dest = (*dest + (*src1 + *src2 + *src3 + *src4 + 2)/ 4 + 1)/ 2;
    //

    movq_m2r (*src1, mm1);        // load 8 src1 bytes
    movq_r2r (mm1, mm2);        // copy 8 src1 bytes

    punpcklbw_r2r (mm0, mm1);        // unpack low src1 bytes
    punpckhbw_r2r (mm0, mm2);        // unpack high src1 bytes

    movq_m2r (*src2, mm3);        // load 8 src2 bytes
    movq_r2r (mm3, mm4);        // copy 8 src2 bytes

    punpcklbw_r2r (mm0, mm3);        // unpack low src2 bytes
    punpckhbw_r2r (mm0, mm4);        // unpack high src2 bytes

    paddw_r2r (mm3, mm1);        // add lows
    paddw_r2r (mm4, mm2);        // add highs

    // now have partials in mm1 and mm2

    movq_m2r (*src3, mm3);        // load 8 src3 bytes
    movq_r2r (mm3, mm4);        // copy 8 src3 bytes

    punpcklbw_r2r (mm0, mm3);        // unpack low src3 bytes
    punpckhbw_r2r (mm0, mm4);        // unpack high src3 bytes

    paddw_r2r (mm3, mm1);        // add lows
    paddw_r2r (mm4, mm2);        // add highs

    movq_m2r (*src4, mm5);        // load 8 src4 bytes
    movq_r2r (mm5, mm6);        // copy 8 src4 bytes

    punpcklbw_r2r (mm0, mm5);        // unpack low src4 bytes
    punpckhbw_r2r (mm0, mm6);        // unpack high src4 bytes

    paddw_r2r (mm5, mm1);        // add lows
    paddw_r2r (mm6, mm2);        // add highs

    paddw_m2r (round4, mm1);
    psraw_i2r (2, mm1);                // /4
    paddw_m2r (round4, mm2);
    psraw_i2r (2, mm2);                // /4

    // now have subtotal/4 in mm1 and mm2

    movq_m2r (*dest, mm3);        // load 8 dest bytes
    movq_r2r (mm3, mm4);        // copy 8 dest bytes

    punpcklbw_r2r (mm0, mm3);        // unpack low dest bytes
    punpckhbw_r2r (mm0, mm4);        // unpack high dest bytes

    paddw_r2r (mm3, mm1);        // add lows
    paddw_r2r (mm4, mm2);        // add highs

    paddw_m2r (round1, mm1);
    psraw_i2r (1, mm1);                // /2
    paddw_m2r (round1, mm2);
    psraw_i2r (1, mm2);                // /2

    // now have end value in mm1 and mm2

    packuswb_r2r (mm2, mm1);        // pack (w/ saturation)
    movq_r2m (mm1,*dest);        // store result in dest
}

//-----------------------------------------------------------------------

static __inline__ void MC_avg_mmx (int width, int height,
                               yuv_data_t * dest, yuv_data_t * ref, int stride)
{
    mmx_zero_reg ();

    do {
        mmx_average_2_U8 (dest, dest, ref);

        if (width == 16)
            mmx_average_2_U8 (dest+8, dest+8, ref+8);

        dest += stride;
        ref += stride;
    } while (--height);
}

static void MC_avg_16_mmx (yuv_data_t * dest, yuv_data_t * ref,
                           int stride, int height)
{
    MC_avg_mmx (16, height, dest, ref, stride);
}

static void MC_avg_8_mmx (yuv_data_t * dest, yuv_data_t * ref,
                          int stride, int height)
{
    MC_avg_mmx (8, height, dest, ref, stride);
}

//-----------------------------------------------------------------------

static __inline__ void MC_put_mmx (int width, int height,
                               yuv_data_t * dest, yuv_data_t * ref, int stride)
{
    mmx_zero_reg ();

    do {
        movq_m2r (* ref, mm1);        // load 8 ref bytes
        movq_r2m (mm1,* dest);        // store 8 bytes at curr

        if (width == 16)
            {
                movq_m2r (* (ref+8), mm1);        // load 8 ref bytes
                movq_r2m (mm1,* (dest+8));        // store 8 bytes at curr
            }

        dest += stride;
        ref += stride;
    } while (--height);
}

static void MC_put_16_mmx (yuv_data_t * dest, yuv_data_t * ref,
                           int stride, int height)
{
    MC_put_mmx (16, height, dest, ref, stride);
}

static void MC_put_8_mmx (yuv_data_t * dest, yuv_data_t * ref,
                          int stride, int height)
{
    MC_put_mmx (8, height, dest, ref, stride);
}

//-----------------------------------------------------------------------

// Half pixel interpolation in the x direction
static __inline__ void MC_avg_x_mmx (int width, int height,
                                 yuv_data_t * dest, yuv_data_t * ref, int stride)
{
    mmx_zero_reg ();

    do {
        mmx_interp_average_2_U8 (dest, ref, ref+1);

        if (width == 16)
            mmx_interp_average_2_U8 (dest+8, ref+8, ref+9);

        dest += stride;
        ref += stride;
    } while (--height);
}

static void MC_avg_x16_mmx (yuv_data_t * dest, yuv_data_t * ref,
                            int stride, int height)
{
    MC_avg_x_mmx (16, height, dest, ref, stride);
}

static void MC_avg_x8_mmx (yuv_data_t * dest, yuv_data_t * ref,
                           int stride, int height)
{
    MC_avg_x_mmx (8, height, dest, ref, stride);
}

//-----------------------------------------------------------------------

static __inline__ void MC_put_x_mmx (int width, int height,
                                 yuv_data_t * dest, yuv_data_t * ref, int stride)
{
    mmx_zero_reg ();

    do {
        mmx_average_2_U8 (dest, ref, ref+1);

        if (width == 16)
            mmx_average_2_U8 (dest+8, ref+8, ref+9);

        dest += stride;
        ref += stride;
    } while (--height);
}

static void MC_put_x16_mmx (yuv_data_t * dest, yuv_data_t * ref,
                            int stride, int height)
{
    MC_put_x_mmx (16, height, dest, ref, stride);
}

static void MC_put_x8_mmx (yuv_data_t * dest, yuv_data_t * ref,
                           int stride, int height)
{
    MC_put_x_mmx (8, height, dest, ref, stride);
}

//-----------------------------------------------------------------------

static __inline__ void MC_avg_xy_mmx (int width, int height,
                                  yuv_data_t * dest, yuv_data_t * ref, int stride)
{
    yuv_data_t * ref_next = ref+stride;

    mmx_zero_reg ();

    do {
        mmx_interp_average_4_U8 (dest, ref, ref+1, ref_next, ref_next+1);

        if (width == 16)
            mmx_interp_average_4_U8 (dest+8, ref+8, ref+9,
                                     ref_next+8, ref_next+9);

        dest += stride;
        ref += stride;
        ref_next += stride;
    } while (--height);
}

static void MC_avg_xy16_mmx (yuv_data_t * dest, yuv_data_t * ref,
                             int stride, int height)
{
    MC_avg_xy_mmx (16, height, dest, ref, stride);
}

static void MC_avg_xy8_mmx (yuv_data_t * dest, yuv_data_t * ref,
                            int stride, int height)
{
    MC_avg_xy_mmx (8, height, dest, ref, stride);
}

//-----------------------------------------------------------------------

static __inline__ void MC_put_xy_mmx (int width, int height,
                                  yuv_data_t * dest, yuv_data_t * ref, int stride)
{
    yuv_data_t * ref_next = ref+stride;

    mmx_zero_reg ();

    do {
        mmx_average_4_U8 (dest, ref, ref+1, ref_next, ref_next+1);

        if (width == 16)
            mmx_average_4_U8 (dest+8, ref+8, ref+9, ref_next+8, ref_next+9);

        dest += stride;
        ref += stride;
        ref_next += stride;
    } while (--height);
}

static void MC_put_xy16_mmx (yuv_data_t * dest, yuv_data_t * ref,
                             int stride, int height)
{
    MC_put_xy_mmx (16, height, dest, ref, stride);
}

static void MC_put_xy8_mmx (yuv_data_t * dest, yuv_data_t * ref,
                            int stride, int height)
{
    MC_put_xy_mmx (8, height, dest, ref, stride);
}

//-----------------------------------------------------------------------

static __inline__ void MC_avg_y_mmx (int width, int height,
                                 yuv_data_t * dest, yuv_data_t * ref, int stride)
{
    yuv_data_t * ref_next = ref+stride;

    mmx_zero_reg ();

    do {
        mmx_interp_average_2_U8 (dest, ref, ref_next);

        if (width == 16)
            mmx_interp_average_2_U8 (dest+8, ref+8, ref_next+8);

        dest += stride;
        ref += stride;
        ref_next += stride;
    } while (--height);
}

static void MC_avg_y16_mmx (yuv_data_t * dest, yuv_data_t * ref,
                            int stride, int height)
{
    MC_avg_y_mmx (16, height, dest, ref, stride);
}

static void MC_avg_y8_mmx (yuv_data_t * dest, yuv_data_t * ref,
                           int stride, int height)
{
    MC_avg_y_mmx (8, height, dest, ref, stride);
}

//-----------------------------------------------------------------------

static __inline__ void MC_put_y_mmx (int width, int height,
                                 yuv_data_t * dest, yuv_data_t * ref, int stride)
{
    yuv_data_t * ref_next = ref+stride;

    mmx_zero_reg ();

    do {
        mmx_average_2_U8 (dest, ref, ref_next);

        if (width == 16)
            mmx_average_2_U8 (dest+8, ref+8, ref_next+8);

        dest += stride;
        ref += stride;
        ref_next += stride;
    } while (--height);
}

static void MC_put_y16_mmx (yuv_data_t * dest, yuv_data_t * ref,
                            int stride, int height)
{
    MC_put_y_mmx (16, height, dest, ref, stride);
}

static void MC_put_y8_mmx (yuv_data_t * dest, yuv_data_t * ref,
                           int stride, int height)
{
    MC_put_y_mmx (8, height, dest, ref, stride);
}


/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void motion_getfunctions( function_list_t * p_function_list )
{
    static void (* ppppf_motion[2][2][4])( yuv_data_t *, yuv_data_t *,
                                           int, int ) =
    {
        {
            /* Copying functions */
            {
                /* Width == 16 */
                MC_put_16_mmx, MC_put_x16_mmx, MC_put_y16_mmx, MC_put_xy16_mmx
            },
            {
                /* Width == 8 */
                MC_put_8_mmx,  MC_put_x8_mmx,  MC_put_y8_mmx, MC_put_xy8_mmx
            }
        },
        {
            /* Averaging functions */
            {
                /* Width == 16 */
                MC_avg_16_mmx, MC_avg_x16_mmx, MC_avg_y16_mmx, MC_avg_xy16_mmx
            },
            {
                /* Width == 8 */
                MC_avg_8_mmx,  MC_avg_x8_mmx,  MC_avg_y8_mmx,  MC_avg_xy8_mmx
            }
        }
    };

    p_function_list->pf_probe = motion_Probe;

#define list p_function_list->functions.motion
    memcpy( list.ppppf_motion, ppppf_motion, sizeof( void * ) * 16 );
#undef list

    return;
}

