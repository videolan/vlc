/*****************************************************************************
 * motion3dnow.c : 3DNow! motion compensation module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: motion3dnow.c,v 1.5 2001/12/09 17:01:36 sam Exp $
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

#define MODULE_NAME motion3dnow
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

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
ADD_WINDOW( "Configuration for 3DNow! motion compensation module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_MOTION;
    p_module->psz_longname = "3DNow! motion compensation module";
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
    if( !TestCPU( CPU_CAPABILITY_3DNOW ) )
    {
        return( 0 );
    }

    if( TestMethod( MOTION_METHOD_VAR, "motion3dnow" )
         || TestMethod( MOTION_METHOD_VAR, "3dnow" ) )
    {
        return( 999 );
    }

    return( 250 );
}

/*****************************************************************************
 * Motion compensation in 3DNow (OK I know this does MMXEXT too and it's ugly)
 *****************************************************************************/

#define CPU_MMXEXT 0
#define CPU_3DNOW 1


//CPU_MMXEXT/CPU_3DNOW adaptation layer

#define pavg_r2r(src,dest)                                                  \
do {                                                                        \
    if (cpu == CPU_MMXEXT)                                                  \
        pavgb_r2r (src, dest);                                              \
    else                                                                    \
        pavgusb_r2r (src, dest);                                            \
} while (0)

#define pavg_m2r(src,dest)                                                  \
do {                                                                        \
    if (cpu == CPU_MMXEXT)                                                  \
        pavgb_m2r (src, dest);                                              \
    else                                                                    \
        pavgusb_m2r (src, dest);                                            \
} while (0)


//CPU_MMXEXT code


static __inline__ void MC_put1_8 (int height, yuv_data_t * dest, yuv_data_t * ref,
                              int stride)
{
    do {
        movq_m2r (*ref, mm0);
        movq_r2m (mm0, *dest);
        ref += stride;
        dest += stride;
    } while (--height);
}

static __inline__ void MC_put1_16 (int height, yuv_data_t * dest, yuv_data_t * ref,
                               int stride)
{
    do {
        movq_m2r (*ref, mm0);
        movq_m2r (*(ref+8), mm1);
        ref += stride;
        movq_r2m (mm0, *dest);
        movq_r2m (mm1, *(dest+8));
        dest += stride;
    } while (--height);
}

static __inline__ void MC_avg1_8 (int height, yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int cpu)
{
    do {
        movq_m2r (*ref, mm0);
        pavg_m2r (*dest, mm0);
        ref += stride;
        movq_r2m (mm0, *dest);
        dest += stride;
    } while (--height);
}

static __inline__ void MC_avg1_16 (int height, yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int cpu)
{
    do {
        movq_m2r (*ref, mm0);
        movq_m2r (*(ref+8), mm1);
        pavg_m2r (*dest, mm0);
        pavg_m2r (*(dest+8), mm1);
        movq_r2m (mm0, *dest);
        ref += stride;
        movq_r2m (mm1, *(dest+8));
        dest += stride;
    } while (--height);
}

static __inline__ void MC_put2_8 (int height, yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int offset, int cpu)
{
    do {
        movq_m2r (*ref, mm0);
        pavg_m2r (*(ref+offset), mm0);
        ref += stride;
        movq_r2m (mm0, *dest);
        dest += stride;
    } while (--height);
}

static __inline__ void MC_put2_16 (int height, yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int offset, int cpu)
{
    do {
        movq_m2r (*ref, mm0);
        movq_m2r (*(ref+8), mm1);
        pavg_m2r (*(ref+offset), mm0);
        pavg_m2r (*(ref+offset+8), mm1);
        movq_r2m (mm0, *dest);
        ref += stride;
        movq_r2m (mm1, *(dest+8));
        dest += stride;
    } while (--height);
}

static __inline__ void MC_avg2_8 (int height, yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int offset, int cpu)
{
    do {
        movq_m2r (*ref, mm0);
        pavg_m2r (*(ref+offset), mm0);
        pavg_m2r (*dest, mm0);
        ref += stride;
        movq_r2m (mm0, *dest);
        dest += stride;
    } while (--height);
}

static __inline__ void MC_avg2_16 (int height, yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int offset, int cpu)
{
    do {
        movq_m2r (*ref, mm0);
        movq_m2r (*(ref+8), mm1);
        pavg_m2r (*(ref+offset), mm0);
        pavg_m2r (*(ref+offset+8), mm1);
        pavg_m2r (*dest, mm0);
        pavg_m2r (*(dest+8), mm1);
        ref += stride;
        movq_r2m (mm0, *dest);
        movq_r2m (mm1, *(dest+8));
        dest += stride;
    } while (--height);
}

static mmx_t mask_one = {0x0101010101010101LL};

static __inline__ void MC_put4_8 (int height, yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int cpu)
{
    movq_m2r (*ref, mm0);
    movq_m2r (*(ref+1), mm1);
    movq_r2r (mm0, mm7);
    pxor_r2r (mm1, mm7);
    pavg_r2r (mm1, mm0);
    ref += stride;

    do {
        movq_m2r (*ref, mm2);
        movq_r2r (mm0, mm5);

        movq_m2r (*(ref+1), mm3);
        movq_r2r (mm2, mm6);

        pxor_r2r (mm3, mm6);
        pavg_r2r (mm3, mm2);

        por_r2r (mm6, mm7);
        pxor_r2r (mm2, mm5);

        pand_r2r (mm5, mm7);
        pavg_r2r (mm2, mm0);

        pand_m2r (mask_one, mm7);

        psubusb_r2r (mm7, mm0);

        ref += stride;
        movq_r2m (mm0, *dest);
        dest += stride;

        movq_r2r (mm6, mm7);        // unroll !
        movq_r2r (mm2, mm0);        // unroll !
    } while (--height);
}

static __inline__ void MC_put4_16 (int height, yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int cpu)
{
    do {
        movq_m2r (*ref, mm0);
        movq_m2r (*(ref+stride+1), mm1);
        movq_r2r (mm0, mm7);
        movq_m2r (*(ref+1), mm2);
        pxor_r2r (mm1, mm7);
        movq_m2r (*(ref+stride), mm3);
        movq_r2r (mm2, mm6);
        pxor_r2r (mm3, mm6);
        pavg_r2r (mm1, mm0);
        pavg_r2r (mm3, mm2);
        por_r2r (mm6, mm7);
        movq_r2r (mm0, mm6);
        pxor_r2r (mm2, mm6);
        pand_r2r (mm6, mm7);
        pand_m2r (mask_one, mm7);
        pavg_r2r (mm2, mm0);
        psubusb_r2r (mm7, mm0);
        movq_r2m (mm0, *dest);

        movq_m2r (*(ref+8), mm0);
        movq_m2r (*(ref+stride+9), mm1);
        movq_r2r (mm0, mm7);
        movq_m2r (*(ref+9), mm2);
        pxor_r2r (mm1, mm7);
        movq_m2r (*(ref+stride+8), mm3);
        movq_r2r (mm2, mm6);
        pxor_r2r (mm3, mm6);
        pavg_r2r (mm1, mm0);
        pavg_r2r (mm3, mm2);
        por_r2r (mm6, mm7);
        movq_r2r (mm0, mm6);
        pxor_r2r (mm2, mm6);
        pand_r2r (mm6, mm7);
        pand_m2r (mask_one, mm7);
        pavg_r2r (mm2, mm0);
        psubusb_r2r (mm7, mm0);
        ref += stride;
        movq_r2m (mm0, *(dest+8));
        dest += stride;
    } while (--height);
}

static __inline__ void MC_avg4_8 (int height, yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int cpu)
{
    do {
        movq_m2r (*ref, mm0);
        movq_m2r (*(ref+stride+1), mm1);
        movq_r2r (mm0, mm7);
        movq_m2r (*(ref+1), mm2);
        pxor_r2r (mm1, mm7);
        movq_m2r (*(ref+stride), mm3);
        movq_r2r (mm2, mm6);
        pxor_r2r (mm3, mm6);
        pavg_r2r (mm1, mm0);
        pavg_r2r (mm3, mm2);
        por_r2r (mm6, mm7);
        movq_r2r (mm0, mm6);
        pxor_r2r (mm2, mm6);
        pand_r2r (mm6, mm7);
        pand_m2r (mask_one, mm7);
        pavg_r2r (mm2, mm0);
        psubusb_r2r (mm7, mm0);
        movq_m2r (*dest, mm1);
        pavg_r2r (mm1, mm0);
        ref += stride;
        movq_r2m (mm0, *dest);
        dest += stride;
    } while (--height);
}

static __inline__ void MC_avg4_16 (int height, yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int cpu)
{
    do {
        movq_m2r (*ref, mm0);
        movq_m2r (*(ref+stride+1), mm1);
        movq_r2r (mm0, mm7);
        movq_m2r (*(ref+1), mm2);
        pxor_r2r (mm1, mm7);
        movq_m2r (*(ref+stride), mm3);
        movq_r2r (mm2, mm6);
        pxor_r2r (mm3, mm6);
        pavg_r2r (mm1, mm0);
        pavg_r2r (mm3, mm2);
        por_r2r (mm6, mm7);
        movq_r2r (mm0, mm6);
        pxor_r2r (mm2, mm6);
        pand_r2r (mm6, mm7);
        pand_m2r (mask_one, mm7);
        pavg_r2r (mm2, mm0);
        psubusb_r2r (mm7, mm0);
        movq_m2r (*dest, mm1);
        pavg_r2r (mm1, mm0);
        movq_r2m (mm0, *dest);

        movq_m2r (*(ref+8), mm0);
        movq_m2r (*(ref+stride+9), mm1);
        movq_r2r (mm0, mm7);
        movq_m2r (*(ref+9), mm2);
        pxor_r2r (mm1, mm7);
        movq_m2r (*(ref+stride+8), mm3);
        movq_r2r (mm2, mm6);
        pxor_r2r (mm3, mm6);
        pavg_r2r (mm1, mm0);
        pavg_r2r (mm3, mm2);
        por_r2r (mm6, mm7);
        movq_r2r (mm0, mm6);
        pxor_r2r (mm2, mm6);
        pand_r2r (mm6, mm7);
        pand_m2r (mask_one, mm7);
        pavg_r2r (mm2, mm0);
        psubusb_r2r (mm7, mm0);
        movq_m2r (*(dest+8), mm1);
        pavg_r2r (mm1, mm0);
        ref += stride;
        movq_r2m (mm0, *(dest+8));
        dest += stride;
    } while (--height);
}

static void MC_avg_16_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_avg1_16 (height, dest, ref, stride, CPU_MMXEXT);
}

static void MC_avg_8_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                             int stride, int height)
{
    MC_avg1_8 (height, dest, ref, stride, CPU_MMXEXT);
}

static void MC_put_16_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_put1_16 (height, dest, ref, stride);
}

static void MC_put_8_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                             int stride, int height)
{
    MC_put1_8 (height, dest, ref, stride);
}

static void MC_avg_x16_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_avg2_16 (height, dest, ref, stride, 1, CPU_MMXEXT);
}

static void MC_avg_x8_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_avg2_8 (height, dest, ref, stride, 1, CPU_MMXEXT);
}

static void MC_put_x16_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_put2_16 (height, dest, ref, stride, 1, CPU_MMXEXT);
}

static void MC_put_x8_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_put2_8 (height, dest, ref, stride, 1, CPU_MMXEXT);
}

static void MC_avg_y16_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_avg2_16 (height, dest, ref, stride, stride, CPU_MMXEXT);
}

static void MC_avg_y8_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_avg2_8 (height, dest, ref, stride, stride, CPU_MMXEXT);
}

static void MC_put_y16_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_put2_16 (height, dest, ref, stride, stride, CPU_MMXEXT);
}

static void MC_put_y8_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_put2_8 (height, dest, ref, stride, stride, CPU_MMXEXT);
}

static void MC_avg_xy16_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                                int stride, int height)
{
    MC_avg4_16 (height, dest, ref, stride, CPU_MMXEXT);
}

static void MC_avg_xy8_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_avg4_8 (height, dest, ref, stride, CPU_MMXEXT);
}

static void MC_put_xy16_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                                int stride, int height)
{
    MC_put4_16 (height, dest, ref, stride, CPU_MMXEXT);
}

static void MC_put_xy8_mmxext (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_put4_8 (height, dest, ref, stride, CPU_MMXEXT);
}


static void MC_avg_16_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_avg1_16 (height, dest, ref, stride, CPU_3DNOW);
}

static void MC_avg_8_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                             int stride, int height)
{
    MC_avg1_8 (height, dest, ref, stride, CPU_3DNOW);
}

static void MC_put_16_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_put1_16 (height, dest, ref, stride);
}

static void MC_put_8_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                             int stride, int height)
{
    MC_put1_8 (height, dest, ref, stride);
}

static void MC_avg_x16_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_avg2_16 (height, dest, ref, stride, 1, CPU_3DNOW);
}

static void MC_avg_x8_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_avg2_8 (height, dest, ref, stride, 1, CPU_3DNOW);
}

static void MC_put_x16_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_put2_16 (height, dest, ref, stride, 1, CPU_3DNOW);
}

static void MC_put_x8_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_put2_8 (height, dest, ref, stride, 1, CPU_3DNOW);
}

static void MC_avg_y16_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_avg2_16 (height, dest, ref, stride, stride, CPU_3DNOW);
}

static void MC_avg_y8_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_avg2_8 (height, dest, ref, stride, stride, CPU_3DNOW);
}

static void MC_put_y16_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_put2_16 (height, dest, ref, stride, stride, CPU_3DNOW);
}

static void MC_put_y8_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                              int stride, int height)
{
    MC_put2_8 (height, dest, ref, stride, stride, CPU_3DNOW);
}

static void MC_avg_xy16_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                                int stride, int height)
{
    MC_avg4_16 (height, dest, ref, stride, CPU_3DNOW);
}

static void MC_avg_xy8_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_avg4_8 (height, dest, ref, stride, CPU_3DNOW);
}

static void MC_put_xy16_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                                int stride, int height)
{
    MC_put4_16 (height, dest, ref, stride, CPU_3DNOW);
}

static void MC_put_xy8_3dnow (yuv_data_t * dest, yuv_data_t * ref,
                               int stride, int height)
{
    MC_put4_8 (height, dest, ref, stride, CPU_3DNOW);
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
                MC_put_16_3dnow, MC_put_x16_3dnow, MC_put_y16_3dnow, MC_put_xy16_3dnow
            },
            {
                /* Width == 8 */
                MC_put_8_3dnow,  MC_put_x8_3dnow,  MC_put_y8_3dnow, MC_put_xy8_3dnow
            }
        },
        {
            /* Averaging functions */
            {
                /* Width == 16 */
                MC_avg_16_3dnow, MC_avg_x16_3dnow, MC_avg_y16_3dnow, MC_avg_xy16_3dnow
            },
            {
                /* Width == 8 */
                MC_avg_8_3dnow,  MC_avg_x8_3dnow,  MC_avg_y8_3dnow,  MC_avg_xy8_3dnow
            }
        }
    };

    p_function_list->pf_probe = motion_Probe;

#define list p_function_list->functions.motion
    memcpy( list.ppppf_motion, ppppf_motion, sizeof( void * ) * 16 );
#undef list

    return;
}

