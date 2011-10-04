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
#   if defined(HAVE_LIBAVFORMAT_AVIO_H)
#      include <libavformat/avio.h>
#   endif
#elif defined(HAVE_FFMPEG_AVFORMAT_H)
#   include <ffmpeg/avformat.h>
#endif
int  OpenAvio (vlc_object_t *);
void CloseAvio(vlc_object_t *);
int  OutOpenAvio (vlc_object_t *);
void OutCloseAvio(vlc_object_t *);

#define AVIO_MODULE \
    set_shortname(N_("FFmpeg"))             \
    set_description(N_("FFmpeg access") )   \
    set_category(CAT_INPUT)                 \
    set_subcategory(SUBCAT_INPUT_ACCESS)    \
    set_capability("access", -1)            \
    add_shortcut("avio", "rtmp")            \
    set_callbacks(OpenAvio, CloseAvio) \
    add_submodule () \
        set_shortname( "libavformat" ) \
        set_description( N_("libavformat access output") ) \
        set_capability( "sout access", -1 ) \
        set_category( CAT_SOUT ) \
        set_subcategory( SUBCAT_SOUT_ACO ) \
        add_shortcut( "avio", "rtmp" ) \
        set_callbacks( OutOpenAvio, OutCloseAvio)
