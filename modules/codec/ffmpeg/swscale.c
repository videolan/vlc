/*****************************************************************************
 * swscale.c: scaling and chroma conversion using libswscale
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include "swscale.h"

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static const int pi_mode_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
const char *const ppsz_mode_descriptions[] =
{ N_("Fast bilinear"), N_("Bilinear"), N_("Bicubic (good quality)"),
  N_("Experimental"), N_("Nearest neighbour (bad quality)"),
  N_("Area"), N_("Luma bicubic / chroma bilinear"), N_("Gauss"),
  N_("SincR"), N_("Lanczos"), N_("Bicubic spline") };

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("Video scaling filter") );
    set_capability( "video filter2", 1000 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );
    set_callbacks( OpenScaler, CloseScaler );
    add_integer( "swscale-mode", 0, NULL, SCALEMODE_TEXT, SCALEMODE_LONGTEXT, true );
        change_integer_list( pi_mode_values, ppsz_mode_descriptions, 0 );
vlc_module_end();
