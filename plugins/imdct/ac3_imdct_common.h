/*****************************************************************************
 * ac3_imdct_common.h: common ac3 DCT headers
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_imdct_common.h,v 1.1 2001/05/15 16:19:42 sam Exp $
 *
 * Authors: Renaud Dartus <reno@videolan.org>
 *          Aaron Holtzman <aholtzma@engr.uvic.ca>
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

void _M( imdct_init )       ( imdct_t * p_imdct );
void _M( imdct_do_256 )     ( imdct_t * p_imdct, float data[], float delay[] );
void _M( imdct_do_256_nol ) ( imdct_t * p_imdct, float data[], float delay[] );
void _M( imdct_do_512  )    ( imdct_t * p_imdct, float data[], float delay[] );
void _M( imdct_do_512_nol ) ( imdct_t * p_imdct, float data[], float delay[] );

