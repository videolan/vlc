/*****************************************************************************
 * avcodec.h: decoder and encoder using libavcodec
 *****************************************************************************
 * Copyright (C) 2001-2013 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef AVCOMMON_COMPAT_H
#define AVCOMMON_COMPAT_H 1

#define AVPROVIDER(lib) ((lib##_VERSION_MICRO < 100) ? "libav" : "ffmpeg")

#include <libavcodec/avcodec.h>

/* LIBAVCODEC_VERSION_CHECK checks for the right version of FFmpeg
 * a is the major version
 * b is the minor version
 * c is the micro version
 */
#define LIBAVCODEC_VERSION_CHECK( a, b, c ) \
    (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( a, b, c ))

#ifndef FF_API_AVIO_WRITE_NONCONST // removed in ffmpeg 7
# define FF_API_AVIO_WRITE_NONCONST (LIBAVFORMAT_VERSION_MAJOR < 61)
#endif

#ifndef FF_API_V408_CODECID // removed in ffmpeg 9
# define FF_API_V408_CODECID (LIBAVCODEC_VERSION_MAJOR < 63)
#endif

# include <libavutil/avutil.h>

/* LIBAVUTIL_VERSION_CHECK checks for the right version of FFmpeg
 * a is the major version
 * b is the minor version
 * c is the micro version
 */
#define LIBAVUTIL_VERSION_CHECK( a, b, c ) \
    (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( a, b, c ))

#ifdef HAVE_LIBAVFORMAT_AVFORMAT_H
# include <libavformat/avformat.h>

#define LIBAVFORMAT_VERSION_CHECK( a, b, c ) \
    (LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT( a, b, c ))

#endif

#if LIBAVCODEC_VERSION_CHECK(60,26,100)
# define AVPROFILE(prof) (AV_PROFILE_##prof)
#else
# define AVPROFILE(prof) (FF_PROFILE_##prof)
#endif

#endif
