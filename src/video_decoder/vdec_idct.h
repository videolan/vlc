/*****************************************************************************
 * vdec_idct.h : types for the inverse discrete cosine transform
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vdec_idct.h,v 1.3 2001/01/13 12:57:47 sam Exp $
 *
 * Authors: Gaël Hendryckx <jimmy@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

struct vdec_thread_s;

typedef void ( *idct_init_t )   ( struct vdec_thread_s * );
typedef void ( *f_idct_t )      ( struct vdec_thread_s *, dctelem_t*, int );

