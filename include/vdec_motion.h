/*****************************************************************************
 * vdec_motion.h : types for the motion compensation algorithm
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/
 
/*****************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "threads.h"
 *  "video_parser.h"
 *  "undec_picture.h"
 *****************************************************************************/

/*****************************************************************************
 * Function pointers
 *****************************************************************************/
struct macroblock_s;
struct vpar_thread_s;
struct motion_arg_s;

typedef void (*f_motion_t)( struct macroblock_s* );

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vdec_MotionFieldField420( struct macroblock_s * p_mb );
void vdec_MotionField16x8420( struct macroblock_s * p_mb );
void vdec_MotionFieldDMV420( struct macroblock_s * p_mb );
void vdec_MotionFrameFrame420( struct macroblock_s * p_mb );
void vdec_MotionFrameField420( struct macroblock_s * p_mb );
void vdec_MotionFrameDMV420( struct macroblock_s * p_mb );
void vdec_MotionFieldField422( struct macroblock_s * p_mb );
void vdec_MotionField16x8422( struct macroblock_s * p_mb );
void vdec_MotionFieldDMV422( struct macroblock_s * p_mb );
void vdec_MotionFrameFrame422( struct macroblock_s * p_mb );
void vdec_MotionFrameField422( struct macroblock_s * p_mb );
void vdec_MotionFrameDMV422( struct macroblock_s * p_mb );
void vdec_MotionFieldField444( struct macroblock_s * p_mb );
void vdec_MotionField16x8444( struct macroblock_s * p_mb );
void vdec_MotionFieldDMV444( struct macroblock_s * p_mb );
void vdec_MotionFrameFrame444( struct macroblock_s * p_mb );
void vdec_MotionFrameField444( struct macroblock_s * p_mb );
void vdec_MotionFrameDMV444( struct macroblock_s * p_mb );
