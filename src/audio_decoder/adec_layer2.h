/*****************************************************************************
 * adec_layer2.h: MPEG Layer II audio decoder
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: adec_layer2.h,v 1.3 2001/03/21 13:42:34 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
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

int adec_layer2_mono   ( adec_thread_t * p_adec, s16 * buffer );
int adec_layer2_stereo ( adec_thread_t * p_adec, s16 * buffer );

