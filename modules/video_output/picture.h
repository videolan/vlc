/*****************************************************************************
 * picture.h:
 *****************************************************************************
 * Copyright (C) 2004-2005 VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
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

#ifndef _PICTURE_VOUT_H
#define _PICTURE_VOUT_H 1

#define PICTURE_VOUT_E_AVAILABLE 0
#define PICTURE_VOUT_E_OCCUPIED 1
typedef struct picture_vout_e_t
{
    picture_t *p_picture;
    int i_status;
    char *psz_id;
} picture_vout_e_t;

typedef struct picture_vout_t
{
    int i_picture_num;
    picture_vout_e_t *p_pic;
} picture_vout_t;

#undef IMAGE_2PASSES

#endif
