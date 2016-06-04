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

/* */
stream_t *stream_CommonNew( vlc_object_t *, void (*destroy)(stream_t *) );
void stream_CommonDelete( stream_t *s );

/**
 * This function creates a stream_t with an access_t back-end.
 */
stream_t *stream_AccessNew(vlc_object_t *, input_thread_t *, bool, const char *);

/**
 * This function creates a new stream_t filter.
 *
 * You must release it using stream_Delete unless it is used as a
 * source to another filter.
 */
stream_t *stream_FilterNew( stream_t *p_source,
                            const char *psz_stream_filter );

/**
 * Automatically wraps a stream with any applicable stream filter.
 * @return the (outermost/downstream) stream filter; if no filters were added,
 * then the function return the source parameter.
 * @note The function never returns NULL.
 */
stream_t *stream_FilterAutoNew( stream_t *source ) VLC_USED;

/**
 * This function creates a chain of filters according to the colon-separated
 * list.
 *
 * You must release the returned value using stream_Delete unless it is used as a
 * source to another filter.
 */
stream_t *stream_FilterChainNew( stream_t *p_source, const char *psz_chain );

char *get_path(const char *location);

#endif
