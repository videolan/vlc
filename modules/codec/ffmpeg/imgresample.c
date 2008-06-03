/*****************************************************************************
 * ffmpeg.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
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
#include <vlc_codec.h>

/* ffmpeg header */
#define HAVE_MMX 1
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#elif defined(HAVE_FFMPEG_AVCODEC_H)
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#if LIBAVCODEC_BUILD < 5000
#   error You must have a libavcodec >= 5000 (get CVS)
#endif

#include "imgresample.h"

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static const int pi_mode_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_mode_descriptions[] =
{ N_("Fast bilinear"), N_("Bilinear"), N_("Bicubic (good quality)"),
  N_("Experimental"), N_("Nearest neighbour (bad quality)"),
  N_("Area"), N_("Luma bicubic / chroma bilinear"), N_("Gauss"),
  N_("SincR"), N_("Lanczos"), N_("Bicubic spline") };

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_capability( "video filter2", 50 );
    set_callbacks( OpenFilter, CloseFilter );
    set_description( N_("FFmpeg video filter") );

    /* crop/padd submodule */
    add_submodule();
    set_capability( "crop padd", 10 );
    set_callbacks( OpenCropPadd, CloseFilter );
    set_description( N_("FFmpeg crop padd filter") );

    /* chroma conversion submodule */
    add_submodule();
    set_capability( "chroma", 50 );
    set_callbacks( OpenChroma, CloseChroma );
    set_description( N_("FFmpeg chroma conversion") );
vlc_module_end();
