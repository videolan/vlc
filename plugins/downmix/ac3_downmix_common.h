/*****************************************************************************
 * ac3_downmix_common.h: ac3 downmix functions headers
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: ac3_downmix_common.h,v 1.1 2001/05/15 16:19:42 sam Exp $
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

void _M( downmix_3f_2r_to_2ch )     ( float *, dm_par_t * );
void _M( downmix_2f_2r_to_2ch )     ( float *, dm_par_t * );
void _M( downmix_3f_1r_to_2ch )     ( float *, dm_par_t * );
void _M( downmix_2f_1r_to_2ch )     ( float *, dm_par_t * );
void _M( downmix_3f_0r_to_2ch )     ( float *, dm_par_t * );
void _M( stream_sample_2ch_to_s16 ) ( s16 *, float *, float * );
void _M( stream_sample_1ch_to_s16 ) ( s16 *, float * );

