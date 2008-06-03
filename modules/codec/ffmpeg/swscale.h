/*****************************************************************************
 * swscale.h: scaling and chroma conversion using libswscale
 *****************************************************************************
 * Copyright (C) 2001-2008 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

int  OpenScaler( vlc_object_t * );
void CloseScaler( vlc_object_t * );

#define SCALEMODE_TEXT N_("Scaling mode")
#define SCALEMODE_LONGTEXT N_("Scaling mode to use.")

#define MUX_TEXT N_("Ffmpeg mux")
#define MUX_LONGTEXT N_("Force use of ffmpeg muxer.")
