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

#ifndef AV_CODEC_FLAG_OUTPUT_CORRUPT
# define AV_CODEC_FLAG_OUTPUT_CORRUPT CODEC_FLAG_OUTPUT_CORRUPT
#endif
#ifndef AV_CODEC_FLAG_GRAY
# define AV_CODEC_FLAG_GRAY CODEC_FLAG_GRAY
#endif
#ifndef AV_CODEC_FLAG_DR1
# define AV_CODEC_FLAG_DR1 CODEC_FLAG_DR1
#endif
#ifndef AV_CODEC_FLAG_DELAY
# define AV_CODEC_FLAG_DELAY CODEC_FLAG_DELAY
#endif
#ifndef AV_CODEC_FLAG2_FAST
# define AV_CODEC_FLAG2_FAST CODEC_FLAG2_FAST
#endif
#ifndef FF_INPUT_BUFFER_PADDING_SIZE
# define FF_INPUT_BUFFER_PADDING_SIZE AV_INPUT_BUFFER_PADDING_SIZE
#endif
#ifndef AV_CODEC_FLAG_INTERLACED_DCT
# define AV_CODEC_FLAG_INTERLACED_DCT CODEC_FLAG_INTERLACED_DCT
#endif
#ifndef AV_CODEC_FLAG_INTERLACED_ME
# define AV_CODEC_FLAG_INTERLACED_ME CODEC_FLAG_INTERLACED_ME
#endif
#ifndef AV_CODEC_FLAG_GLOBAL_HEADER
# define AV_CODEC_FLAG_GLOBAL_HEADER CODEC_FLAG_GLOBAL_HEADER
#endif
#ifndef AV_CODEC_FLAG_LOW_DELAY
# define AV_CODEC_FLAG_LOW_DELAY CODEC_FLAG_LOW_DELAY
#endif
#ifndef AV_CODEC_CAP_SMALL_LAST_FRAME
# define AV_CODEC_CAP_SMALL_LAST_FRAME CODEC_CAP_SMALL_LAST_FRAME
#endif
#ifndef  FF_MAX_B_FRAMES
# define  FF_MAX_B_FRAMES 16 // FIXME: remove this
#endif
#ifndef FF_API_AVIO_WRITE_NONCONST // removed in ffmpeg 7
# define FF_API_AVIO_WRITE_NONCONST (LIBAVFORMAT_VERSION_MAJOR < 61)
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
