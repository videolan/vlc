/*****************************************************************************
 * motionaltivec.c : Altivec motion compensation module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: motionaltivec.c,v 1.1 2001/09/05 16:07:49 massiot Exp $
 *
 * Authors: Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *          Paul Mackerras <paulus@linuxcare.com.au>
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

#define MODULE_NAME motionaltivec
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "tests.h"

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
ADD_WINDOW( "Configuration for Altivec motion compensation module" )
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
    if( !TestCPU( CPU_CAPABILITY_ALTIVEC ) )
    {
        return( 0 );
    }

    if( TestMethod( MOTION_METHOD_VAR, "motionaltivec" )
         || TestMethod( MOTION_METHOD_VAR, "altivec" ) )
    {
        return( 999 );
    }

    return( 150 );
}

/*****************************************************************************
 * Motion compensation in Altivec
 *****************************************************************************/

#define COPY_8(d, s)	(*(long long *)(d) = *(long long *)(s))
#define COPY_16(d, s)	(((long long *)(d))[0] = ((long long *)(s))[0], \
			 ((long long *)(d))[1] = ((long long *)(s))[1])

void
MC_put_16_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift, refw0, refw1, d;

	do {
		rshift = vec_lvsl(0, ref);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		d = vec_perm(refw0, refw1, rshift);
		if ((unsigned long)dest & 15) {
			/* unaligned store, yuck */
			vector unsigned char x = d;
			COPY_16(dest, &x);
		} else
			vec_st(d, 0, dest);
		ref += stride;
		dest += stride;
	} while (--height);
}

void
MC_avg_16_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift, refw0, refw1;
	vector unsigned char r, d;

	do {
		rshift = vec_lvsl(0, ref);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		r = vec_perm(refw0, refw1, rshift);
		if ((unsigned long)dest & 15) {
			/* unaligned load/store, yuck */
			vector unsigned char dw0, dw1, dshift, mask;

			dshift = vec_lvsr(0, dest);
			dw0 = vec_ld(0, dest);
			dw1 = vec_ld(16, dest);
			d = vec_perm(r, r, dshift);
			mask = vec_perm((vector unsigned char)(0),
					(vector unsigned char)(255), dshift);
			dw0 = vec_sel(dw0, vec_avg(dw0, d), mask);
			dw1 = vec_sel(vec_avg(dw1, d), dw1, mask);
			vec_st(dw0, 0, dest);
			vec_st(dw1, 16, dest);
		} else {
			d = vec_ld(0, dest);
			d = vec_avg(d, r);
			vec_st(d, 0, dest);
		}
		ref += stride;
		dest += stride;
	} while (--height);
}

void
MC_put_x16_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift0, rshift1, refw0, refw1;
	vector unsigned char t0, t1, d, one;

	one = (vector unsigned char)(1);
	do {
		rshift0 = vec_lvsl(0, ref);
		rshift1 = vec_add(rshift0, one);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		t0 = vec_perm(refw0, refw1, rshift0);
		t1 = vec_perm(refw0, refw1, rshift1);
		d = vec_avg(t0, t1);
		if ((unsigned long)dest & 15) {
			/* unaligned store, yuck */
			vector unsigned char x = d;
			COPY_16(dest, &x);
		} else
			vec_st(d, 0, dest);
		ref += stride;
		dest += stride;
	} while (--height);
}

void
MC_avg_x16_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift0, rshift1, refw0, refw1;
	vector unsigned char t0, t1, r, d, one;

	one = (vector unsigned char)(1);
	do {
		rshift0 = vec_lvsl(0, ref);
		rshift1 = vec_add(rshift0, one);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		t0 = vec_perm(refw0, refw1, rshift0);
		t1 = vec_perm(refw0, refw1, rshift1);
		r = vec_avg(t0, t1);
		if ((unsigned long)dest & 15) {
			/* unaligned load/store, yuck */
			vector unsigned char dw0, dw1, dshift, mask;

			dshift = vec_lvsr(0, dest);
			dw0 = vec_ld(0, dest);
			dw1 = vec_ld(16, dest);
			d = vec_perm(r, r, dshift);
			mask = vec_perm((vector unsigned char)(0),
					(vector unsigned char)(255), dshift);
			dw0 = vec_sel(dw0, vec_avg(dw0, d), mask);
			dw1 = vec_sel(vec_avg(dw1, d), dw1, mask);
			vec_st(dw0, 0, dest);
			vec_st(dw1, 16, dest);
		} else {
			d = vec_ld(0, dest);
			d = vec_avg(d, r);
			vec_st(d, 0, dest);
		}
		ref += stride;
		dest += stride;
	} while (--height);
}

void
MC_put_y16_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift, refw0, refw1;
	vector unsigned char r0, r1, d;

	rshift = vec_lvsl(0, ref);
	refw0 = vec_ld(0, ref);
	refw1 = vec_ld(16, ref);
	r0 = vec_perm(refw0, refw1, rshift);
	do {
		ref += stride;
		rshift = vec_lvsl(0, ref);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		r1 = vec_perm(refw0, refw1, rshift);
		d = vec_avg(r0, r1);
		r0 = r1;
		if ((unsigned long)dest & 15) {
			/* unaligned store, yuck */
			vector unsigned char x = d;
			COPY_16(dest, &x);
		} else
			vec_st(d, 0, dest);
		dest += stride;
	} while (--height);
}

void
MC_avg_y16_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift, refw0, refw1;
	vector unsigned char r0, r1, r, d;

	rshift = vec_lvsl(0, ref);
	refw0 = vec_ld(0, ref);
	refw1 = vec_ld(16, ref);
	r0 = vec_perm(refw0, refw1, rshift);
	do {
		ref += stride;
		rshift = vec_lvsl(0, ref);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		r1 = vec_perm(refw0, refw1, rshift);
		r = vec_avg(r0, r1);
		r0 = r1;
		if ((unsigned long)dest & 15) {
			/* unaligned load/store, yuck */
			vector unsigned char dw0, dw1, dshift, mask;

			dshift = vec_lvsr(0, dest);
			dw0 = vec_ld(0, dest);
			dw1 = vec_ld(16, dest);
			d = vec_perm(r, r, dshift);
			mask = vec_perm((vector unsigned char)(0),
					(vector unsigned char)(255), dshift);
			dw0 = vec_sel(dw0, vec_avg(dw0, d), mask);
			dw1 = vec_sel(vec_avg(dw1, d), dw1, mask);
			vec_st(dw0, 0, dest);
			vec_st(dw1, 16, dest);
		} else {
			d = vec_ld(0, dest);
			d = vec_avg(d, r);
			vec_st(d, 0, dest);
		}
		dest += stride;
	} while (--height);
}

void
MC_put_xy16_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift0, rshift1, refw0, refw1;
	vector unsigned char t0, t1, r0, r1, d, one;

	rshift0 = vec_lvsl(0, ref);
	one = (vector unsigned char)(1);
	rshift1 = vec_add(rshift0, one);
	refw0 = vec_ld(0, ref);
	refw1 = vec_ld(16, ref);
	t0 = vec_perm(refw0, refw1, rshift0);
	t1 = vec_perm(refw0, refw1, rshift1);
	r0 = vec_avg(t0, t1);
	do {
		ref += stride;
		rshift0 = vec_lvsl(0, ref);
		rshift1 = vec_add(rshift0, one);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		t0 = vec_perm(refw0, refw1, rshift0);
		t1 = vec_perm(refw0, refw1, rshift1);
		r1 = vec_avg(t0, t1);
		d = vec_avg(r0, r1);
		r0 = r1;
		if ((unsigned long)dest & 15) {
			/* unaligned store, yuck */
			vector unsigned char x = d;
			COPY_16(dest, &x);
		} else
			vec_st(d, 0, dest);
		dest += stride;
	} while (--height);
}

void
MC_avg_xy16_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift0, rshift1, refw0, refw1;
	vector unsigned char t0, t1, r0, r1, r, d, one;

	rshift0 = vec_lvsl(0, ref);
	one = (vector unsigned char)(1);
	rshift1 = vec_add(rshift0, one);
	refw0 = vec_ld(0, ref);
	refw1 = vec_ld(16, ref);
	t0 = vec_perm(refw0, refw1, rshift0);
	t1 = vec_perm(refw0, refw1, rshift1);
	r0 = vec_avg(t0, t1);
	do {
		ref += stride;
		rshift0 = vec_lvsl(0, ref);
		rshift1 = vec_add(rshift0, one);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		t0 = vec_perm(refw0, refw1, rshift0);
		t1 = vec_perm(refw0, refw1, rshift1);
		r1 = vec_avg(t0, t1);
		r = vec_avg(r0, r1);
		r0 = r1;
		if ((unsigned long)dest & 15) {
			/* unaligned load/store, yuck */
			vector unsigned char dw0, dw1, dshift, mask;

			dshift = vec_lvsr(0, dest);
			dw0 = vec_ld(0, dest);
			dw1 = vec_ld(16, dest);
			d = vec_perm(r, r, dshift);
			mask = vec_perm((vector unsigned char)(0),
					(vector unsigned char)(255), dshift);
			dw0 = vec_sel(dw0, vec_avg(dw0, d), mask);
			dw1 = vec_sel(vec_avg(dw1, d), dw1, mask);
			vec_st(dw0, 0, dest);
			vec_st(dw1, 16, dest);
		} else {
			d = vec_ld(0, dest);
			d = vec_avg(d, r);
			vec_st(d, 0, dest);
		}
		dest += stride;
	} while (--height);
}

void
MC_put_8_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift, refw0, refw1, d;

	do {
		rshift = vec_lvsl(0, ref);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		d = vec_perm(refw0, refw1, rshift);
		COPY_8(dest, &d);
		ref += stride;
		dest += stride;
	} while (--height);
}

void
MC_avg_8_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift, refw0, refw1;
	vector unsigned char r, d;

	do {
		rshift = vec_lvsl(0, ref);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		r = vec_perm(refw0, refw1, rshift);
		COPY_8(&d, dest);
		d = vec_avg(d, r);
		COPY_8(dest, &d);
		ref += stride;
		dest += stride;
	} while (--height);
}

void
MC_put_x8_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift0, rshift1, refw0, refw1;
	vector unsigned char t0, t1, d, one;

	one = (vector unsigned char)(1);
	do {
		rshift0 = vec_lvsl(0, ref);
		rshift1 = vec_add(rshift0, one);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		t0 = vec_perm(refw0, refw1, rshift0);
		t1 = vec_perm(refw0, refw1, rshift1);
		d = vec_avg(t0, t1);
		COPY_8(dest, &d);
		ref += stride;
		dest += stride;
	} while (--height);
}

void
MC_avg_x8_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift0, rshift1, refw0, refw1;
	vector unsigned char t0, t1, r, d, one;

	one = (vector unsigned char)(1);
	do {
		rshift0 = vec_lvsl(0, ref);
		rshift1 = vec_add(rshift0, one);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		t0 = vec_perm(refw0, refw1, rshift0);
		t1 = vec_perm(refw0, refw1, rshift1);
		r = vec_avg(t0, t1);
		COPY_8(&d, dest);
		d = vec_avg(d, r);
		COPY_8(dest, &d);
		ref += stride;
		dest += stride;
	} while (--height);
}

void
MC_put_y8_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift, refw0, refw1;
	vector unsigned char r0, r1, d;

	rshift = vec_lvsl(0, ref);
	refw0 = vec_ld(0, ref);
	refw1 = vec_ld(16, ref);
	r0 = vec_perm(refw0, refw1, rshift);
	do {
		ref += stride;
		rshift = vec_lvsl(0, ref);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		r1 = vec_perm(refw0, refw1, rshift);
		d = vec_avg(r0, r1);
		r0 = r1;
		COPY_8(dest, &d);
		dest += stride;
	} while (--height);
}

void
MC_avg_y8_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift, refw0, refw1;
	vector unsigned char r0, r1, r, d;

	rshift = vec_lvsl(0, ref);
	refw0 = vec_ld(0, ref);
	refw1 = vec_ld(16, ref);
	r0 = vec_perm(refw0, refw1, rshift);
	do {
		ref += stride;
		rshift = vec_lvsl(0, ref);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		r1 = vec_perm(refw0, refw1, rshift);
		r = vec_avg(r0, r1);
		r0 = r1;
		COPY_8(&d, dest);
		d = vec_avg(d, r);
		COPY_8(dest, &d);
		dest += stride;
	} while (--height);
}

void
MC_put_xy8_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift0, rshift1, refw0, refw1;
	vector unsigned char t0, t1, r0, r1, d, one;

	rshift0 = vec_lvsl(0, ref);
	one = (vector unsigned char)(1);
	rshift1 = vec_add(rshift0, one);
	refw0 = vec_ld(0, ref);
	refw1 = vec_ld(16, ref);
	t0 = vec_perm(refw0, refw1, rshift0);
	t1 = vec_perm(refw0, refw1, rshift1);
	r0 = vec_avg(t0, t1);
	do {
		ref += stride;
		rshift0 = vec_lvsl(0, ref);
		rshift1 = vec_add(rshift0, one);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		t0 = vec_perm(refw0, refw1, rshift0);
		t1 = vec_perm(refw0, refw1, rshift1);
		r1 = vec_avg(t0, t1);
		d = vec_avg(r0, r1);
		r0 = r1;
		COPY_8(dest, &d);
		dest += stride;
	} while (--height);
}

void
MC_avg_xy8_altivec(uint8_t * dest, uint8_t * ref, int stride, int height)
{
	vector unsigned char rshift0, rshift1, refw0, refw1;
	vector unsigned char t0, t1, r0, r1, r, d, one;

	rshift0 = vec_lvsl(0, ref);
	one = (vector unsigned char)(1);
	rshift1 = vec_add(rshift0, one);
	refw0 = vec_ld(0, ref);
	refw1 = vec_ld(16, ref);
	t0 = vec_perm(refw0, refw1, rshift0);
	t1 = vec_perm(refw0, refw1, rshift1);
	r0 = vec_avg(t0, t1);
	do {
		ref += stride;
		rshift0 = vec_lvsl(0, ref);
		rshift1 = vec_add(rshift0, one);
		refw0 = vec_ld(0, ref);
		refw1 = vec_ld(16, ref);
		t0 = vec_perm(refw0, refw1, rshift0);
		t1 = vec_perm(refw0, refw1, rshift1);
		r1 = vec_avg(t0, t1);
		r = vec_avg(r0, r1);
		r0 = r1;
		COPY_8(&d, dest);
		d = vec_avg(d, r);
		COPY_8(dest, &d);
		dest += stride;
	} while (--height);
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
                MC_put_16_altivec, MC_put_x16_altivec, MC_put_y16_altivec, MC_put_xy16_altivec
            },
            {
                /* Width == 8 */
                MC_put_8_altivec,  MC_put_x8_altivec,  MC_put_y8_altivec, MC_put_xy8_altivec
            }
        },
        {
            /* Averaging functions */
            {
                /* Width == 16 */
                MC_avg_16_altivec, MC_avg_x16_altivec, MC_avg_y16_altivec, MC_avg_xy16_altivec
            },
            {
                /* Width == 8 */
                MC_avg_8_altivec,  MC_avg_x8_altivec,  MC_avg_y8_altivec,  MC_avg_xy8_altivec
            }
        }
    };

    p_function_list->pf_probe = motion_Probe;

#define list p_function_list->functions.motion
    memcpy( list.ppppf_motion, ppppf_motion, sizeof( void * ) * 16 );
#undef list

    return;
}
