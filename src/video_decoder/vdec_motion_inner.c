/*****************************************************************************
 * vdec_motion.c : motion compensation routines
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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
#include "defs.h"

#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "intf_msg.h"

#include "input.h"
#include "decoder_fifo.h"
#include "video.h"
#include "video_output.h"

#include "vdec_idct.h"
#include "video_decoder.h"
#include "vdec_motion.h"

#include "vpar_blocks.h"
#include "vpar_headers.h"
#include "vpar_synchro.h"
#include "video_parser.h"
#include "video_fifo.h"

#define __MotionComponent_x_y_copy(width,height)			\
void MotionComponent_x_y_copy_##width##_##height(yuv_data_t * p_src,	\
						 yuv_data_t * p_dest,	\
						 int i_stride)		\
{									\
    int i_x, i_y;      							\
									\
    for( i_y = 0; i_y < height; i_y ++ )				\
    {									\
	for( i_x = 0; i_x < width; i_x++ )     				\
	{								\
	    p_dest[i_x] = p_src[i_x];					\
	}								\
	p_dest += i_stride;						\
	p_src += i_stride;						\
    }									\
}

#define __MotionComponent_X_y_copy(width,height)			\
void MotionComponent_X_y_copy_##width##_##height(yuv_data_t * p_src,	\
						 yuv_data_t * p_dest,	\
						 int i_stride)		\
{									\
    int i_x, i_y;      							\
									\
    for( i_y = 0; i_y < height; i_y ++ )				\
    {									\
	for( i_x = 0; i_x < width; i_x++ )     				\
	{								\
	    p_dest[i_x] = (unsigned int)(p_src[i_x]			\
					 + p_src[i_x + 1]		\
					 + 1) >> 1;			\
	}								\
	p_dest += i_stride;						\
	p_src += i_stride;						\
    }									\
}

#define __MotionComponent_x_Y_copy(width,height)			\
void MotionComponent_x_Y_copy_##width##_##height(yuv_data_t * p_src,	\
						 yuv_data_t * p_dest,	\
						 int i_stride,		\
						 int i_step)		\
{									\
    int i_x, i_y;      							\
									\
    for( i_y = 0; i_y < height; i_y ++ )				\
    {									\
	for( i_x = 0; i_x < width; i_x++ )     				\
	{								\
	    p_dest[i_x] = (unsigned int)(p_src[i_x]			\
					 + p_src[i_x + i_step]		\
					 + 1) >> 1;			\
	}								\
	p_dest += i_stride;						\
	p_src += i_stride;						\
    }									\
}

#define __MotionComponent_X_Y_copy(width,height)			\
void MotionComponent_X_Y_copy_##width##_##height(yuv_data_t * p_src,	\
						 yuv_data_t * p_dest,	\
						 int i_stride,		\
						 int i_step)		\
{									\
    int i_x, i_y;      							\
									\
    for( i_y = 0; i_y < height; i_y ++ )				\
    {									\
	for( i_x = 0; i_x < width; i_x++ )     				\
	{								\
	    p_dest[i_x] = (unsigned int)(p_src[i_x]			\
					 + p_src[i_x + 1]		\
					 + p_src[i_x + i_step]		\
					 + p_src[i_x + i_step + 1]	\
					 + 2) >> 2;			\
	}								\
	p_dest += i_stride;						\
	p_src += i_stride;						\
    }									\
}

#define __MotionComponent_x_y_avg(width,height)				\
void MotionComponent_x_y_avg_##width##_##height(yuv_data_t * p_src,	\
						yuv_data_t * p_dest,	\
						int i_stride)		\
{									\
    int i_x, i_y;      							\
    unsigned int i_dummy;						\
									\
    for( i_y = 0; i_y < height; i_y ++ )				\
    {									\
	for( i_x = 0; i_x < width; i_x++ )     				\
	{								\
	    i_dummy = p_dest[i_x] + p_src[i_x];				\
	    p_dest[i_x] = (i_dummy + 1) >> 1;  				\
	}								\
	p_dest += i_stride;						\
	p_src += i_stride;						\
    }									\
}

#define __MotionComponent_X_y_avg(width,height)				\
void MotionComponent_X_y_avg_##width##_##height(yuv_data_t * p_src,	\
						yuv_data_t * p_dest,	\
						int i_stride)		\
{									\
    int i_x, i_y;      							\
    unsigned int i_dummy;						\
									\
    for( i_y = 0; i_y < height; i_y ++ )				\
    {									\
	for( i_x = 0; i_x < width; i_x++ )     				\
	{								\
	    i_dummy = p_dest[i_x] + ((unsigned int)(p_src[i_x]		\
						    + p_src[i_x + 1]	\
						    + 1) >> 1);		\
	    p_dest[i_x] = (i_dummy + 1) >> 1;  				\
	}								\
	p_dest += i_stride;						\
	p_src += i_stride;						\
    }									\
}

#define __MotionComponent_x_Y_avg(width,height)				\
void MotionComponent_x_Y_avg_##width##_##height(yuv_data_t * p_src,	\
						yuv_data_t * p_dest,	\
						int i_stride,		\
						int i_step)		\
{									\
    int i_x, i_y;      							\
    unsigned int i_dummy;						\
									\
    for( i_y = 0; i_y < height; i_y ++ )				\
    {									\
	for( i_x = 0; i_x < width; i_x++ )     				\
	{								\
	    i_dummy =							\
		p_dest[i_x] + ((unsigned int)(p_src[i_x]		\
					      + p_src[i_x + i_step]	\
					      + 1) >> 1);		\
	    p_dest[i_x] = (i_dummy + 1) >> 1;  				\
	}								\
	p_dest += i_stride;						\
	p_src += i_stride;						\
    }									\
}

#define __MotionComponent_X_Y_avg(width,height)				\
void MotionComponent_X_Y_avg_##width##_##height(yuv_data_t * p_src,	\
						yuv_data_t * p_dest,	\
						int i_stride,		\
						int i_step)		\
{									\
    int i_x, i_y;      							\
    unsigned int i_dummy;						\
									\
    for( i_y = 0; i_y < height; i_y ++ )				\
    {									\
	for( i_x = 0; i_x < width; i_x++ )     				\
	{								\
	    i_dummy =							\
		p_dest[i_x] + ((unsigned int)(p_src[i_x]		\
					      + p_src[i_x + 1]		\
					      + p_src[i_x + i_step]	\
					      + p_src[i_x + i_step + 1]	\
					      + 2) >> 2);		\
	    p_dest[i_x] = (i_dummy + 1) >> 1;  				\
	}								\
	p_dest += i_stride;						\
	p_src += i_stride;						\
    }									\
}

#define __MotionComponents(width,height)	\
__MotionComponent_x_y_copy(width,height)	\
__MotionComponent_X_y_copy(width,height)	\
__MotionComponent_x_Y_copy(width,height)	\
__MotionComponent_X_Y_copy(width,height)	\
__MotionComponent_x_y_avg(width,height)		\
__MotionComponent_X_y_avg(width,height)		\
__MotionComponent_x_Y_avg(width,height)		\
__MotionComponent_X_Y_avg(width,height)

__MotionComponents (16,16)	/* 444, 422, 420 */
__MotionComponents (16,8)	/* 444, 422, 420 */
__MotionComponents (8,8)	/* 422, 420 */
__MotionComponents (8,4)	/* 420 */
#if 0
__MotionComponents (8,16)	/* 422 */
#endif
