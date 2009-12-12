/*****************************************************************************
 * avio.h: access using libavformat library
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/* ffmpeg header */
#if defined(HAVE_LIBAVFORMAT_AVFORMAT_H)
#   include <libavformat/avformat.h>
#elif defined(HAVE_FFMPEG_AVFORMAT_H)
#   include <ffmpeg/avformat.h>
#endif
int  OpenAvio (vlc_object_t *);
void CloseAvio(vlc_object_t *);

#define AVIO_MODULE \
    set_shortname(N_("Avio"))               \
    set_description(N_("FFmpeg access") )   \
    set_category(CAT_INPUT)                 \
    set_subcategory(SUBCAT_INPUT_ACCESS)    \
    set_capability("access", -1)            \
    add_shortcut("avio")                    \
    add_shortcut("rtmp")                    \
    set_callbacks(OpenAvio, CloseAvio)

