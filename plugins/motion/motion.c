/*****************************************************************************
 * motion.c : C motion compensation module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: motion.c,v 1.13 2001/12/30 07:09:55 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <videolan/vlc.h>

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void motion_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "motion compensation module" )
    ADD_CAPABILITY( MOTION, 50 )
    ADD_SHORTCUT( "c" )
    ADD_SHORTCUT( "motion" )
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
    return( 50 );
}

/*****************************************************************************
 * Simple motion compensation in C
 *****************************************************************************/

#define avg2(a,b) ((a+b+1)>>1)
#define avg4(a,b,c,d) ((a+b+c+d+2)>>2)

#define predict_(i) (ref[i])
#define predict_x(i) (avg2 (ref[i], ref[i+1]))
#define predict_y(i) (avg2 (ref[i], (ref+stride)[i]))
#define predict_xy(i) (avg4 (ref[i], ref[i+1], (ref+stride)[i], (ref+stride)[i+1]))

#define put(predictor,i) dest[i] = predictor (i)
#define avg(predictor,i) dest[i] = avg2 (predictor (i), dest[i])

// mc function template

#define MC_FUNC(op,xy)                                                                                \
static void MC_##op##_##xy##16_c (yuv_data_t * dest, yuv_data_t * ref,      \
                                 int stride, int height)                    \
{                                                                           \
    do {                                                                    \
        op (predict_##xy, 0);                                               \
        op (predict_##xy, 1);                                               \
        op (predict_##xy, 2);                                               \
        op (predict_##xy, 3);                                               \
        op (predict_##xy, 4);                                               \
        op (predict_##xy, 5);                                               \
        op (predict_##xy, 6);                                               \
        op (predict_##xy, 7);                                               \
        op (predict_##xy, 8);                                               \
        op (predict_##xy, 9);                                               \
        op (predict_##xy, 10);                                              \
        op (predict_##xy, 11);                                              \
        op (predict_##xy, 12);                                              \
        op (predict_##xy, 13);                                              \
        op (predict_##xy, 14);                                              \
        op (predict_##xy, 15);                                              \
        ref += stride;                                                      \
        dest += stride;                                                     \
    } while (--height);                                                     \
}                                                                           \
static void MC_##op##_##xy##8_c (yuv_data_t * dest, yuv_data_t * ref,       \
                                int stride, int height)                     \
{                                                                           \
    do {                                                                    \
        op (predict_##xy, 0);                                               \
        op (predict_##xy, 1);                                               \
        op (predict_##xy, 2);                                               \
        op (predict_##xy, 3);                                               \
        op (predict_##xy, 4);                                               \
        op (predict_##xy, 5);                                               \
        op (predict_##xy, 6);                                               \
        op (predict_##xy, 7);                                               \
        ref += stride;                                                      \
        dest += stride;                                                     \
    } while (--height);                                                     \
}

// definitions of the actual mc functions

MC_FUNC (put,)
MC_FUNC (avg,)
MC_FUNC (put,x)
MC_FUNC (avg,x)
MC_FUNC (put,y)
MC_FUNC (avg,y)
MC_FUNC (put,xy)
MC_FUNC (avg,xy)

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
                MC_put_16_c, MC_put_x16_c, MC_put_y16_c, MC_put_xy16_c
            },
            {
                /* Width == 8 */
                MC_put_8_c,  MC_put_x8_c,  MC_put_y8_c, MC_put_xy8_c
            }
        },
        {
            /* Averaging functions */
            {
                /* Width == 16 */
                MC_avg_16_c, MC_avg_x16_c, MC_avg_y16_c, MC_avg_xy16_c
            },
            {
                /* Width == 8 */
                MC_avg_8_c,  MC_avg_x8_c,  MC_avg_y8_c,  MC_avg_xy8_c
            }
        }
    };

    p_function_list->pf_probe = motion_Probe;

#define list p_function_list->functions.motion
    memcpy( list.ppppf_motion, ppppf_motion, sizeof( void * ) * 16 );
#undef list

    return;
}
