/*****************************************************************************
 * vdec_motion.h : types for the motion compensation algorithm
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vdec_motion.h,v 1.14 2001/01/18 05:13:22 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Marc Dressler <polux@via.ecp.fr>
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

