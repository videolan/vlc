/*****************************************************************************
 * stream.h: Input stream functions
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef LIBVLC_INPUT_STREAM_H
#define LIBVLC_INPUT_STREAM_H 1

#include <vlc_common.h>
#include <vlc_stream.h>
#include <vlc_charset.h>

struct stream_text_t
{
    /* UTF-16 and UTF-32 file reading */
    vlc_iconv_t     conv;
    int             i_char_width;
    bool            b_little_endian;
};

/* */
stream_t *stream_CommonNew( vlc_object_t * );
void stream_CommonDelete( stream_t * );

/**
 * This function creates a stream_t from a provided access_t.
 *
 * An optional NULL terminated list of file may be provided. The content
 * of these extra files will be concatenated after to the main access.
 *
 * XXX ppsz_list is treated as const (I failed to avoid a warning when
 * using const keywords for pointer of pointers)
 */
stream_t *stream_AccessNew( access_t *p_access, char **ppsz_list );

/**
 * This function creates a new stream_t filter.
 *
 * You must release it using stream_Delete unless it is used as a
 * source to another filter.
 */
stream_t *stream_FilterNew( stream_t *p_source,
                            const char *psz_stream_filter );

/**
 * This function creates a chain of filters:
 * - first, automatic probed stream filters are inserted.
 * - then, optional user filters (configured by psz_chain) are inserted.
 * - finaly, an optional record filter is inserted if b_record is true.
 *
 * You must release the returned value using stream_Delete unless it is used as a
 * source to another filter.
 */
stream_t *stream_FilterChainNew( stream_t *p_source,
                                 const char *psz_chain,
                                 bool b_record );
#endif
