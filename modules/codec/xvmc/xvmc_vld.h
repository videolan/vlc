/* $Id$
 * Copyright (c) 2004 The Unichrome project. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTIES OR REPRESENTATIONS; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 */

#ifndef _XVMC_VLD_H
#define _XVMC_VLD_H

#include "accel_xvmc.h"
//#include "xvmc.h"

extern void mpeg2_xxmc_slice( mpeg2dec_t *mpeg2dec, picture_t *picture,
                              int code, uint8_t *buffer, int size );
extern void mpeg2_xxmc_choose_coding( decoder_t *p_dec,
                mpeg2_decoder_t * const decoder, picture_t *picture,
                double aspect_ratio, int flags );
extern void mpeg2_xxmc_vld_frame_complete( mpeg2dec_t *mpeg2dec,
                picture_t *picture, int code );

#endif
