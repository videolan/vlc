/*****************************************************************************
 * postprocessing_common.h
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: postprocessing_common.h,v 1.2 2002/08/08 22:28:22 sam Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#if 0
#define PP_USE_3DNOW /* Nothing done yet */
#define PP_USE_MMX   /* when only MMX is supported */
#define PP_USE_MMXEXT  /* when MMXEXT is also supported, imply MMX */
#endif


/* thresholds for deblocking, I've taken value given by ISO  */
#define PP_THR1 2ULL /* threshold for deblocking */

#define PP_2xTHR1 ( 2 * PP_THR1 )/* internal usage */

#define PP_THR2 6ULL



/* Some usefull macros */
#define PP_MAX( a, b ) ( a > b ? (a) : (b) )
#define PP_MIN( a, b ) ( a < b ? (a) : (b) )
#define PP_ABS( x ) ( ( x < 0 ) ? (-(x)) : (x) )
#define PP_SGN( x ) ( ( x < 0 ) ? -1 : 1 )
#define PP_MIN3( a, b, c ) ( PP_MIN( (a), PP_MIN( (b), (c) ) ) )
#define PP_CLIP( x, a, b ) ( PP_MAX( (a), PP_MIN( (x), (b) ) ) )

void E_( pp_deblock_V )();
void E_( pp_deblock_H )();

void E_( pp_dering_Y )();
void E_( pp_dering_C )();
