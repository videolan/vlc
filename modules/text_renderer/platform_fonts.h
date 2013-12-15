/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Bernie Purcell <bitmap@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Felix Paul Kühne <fkuehne@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_FONTCONFIG
char* FontConfig_Select( filter_t *p_filter, const char* family,
                          bool b_bold, bool b_italic, int i_size, int *i_idx );
void FontConfig_BuildCache( filter_t *p_filter );
#endif


#ifdef _WIN32
char* Win32_Select( filter_t *p_filter, const char* family,
                           bool b_bold, bool b_italic, int i_size, int *i_idx );

#endif /* _WIN32 */

#ifdef __APPLE__
#if !TARGET_OS_IPHONE
char* MacLegacy_Select( filter_t *p_filter, const char* psz_fontname,
                          bool b_bold, bool b_italic, int i_size, int *i_idx );
#endif
#endif

