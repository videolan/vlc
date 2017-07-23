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
void stream_CommonDelete( stream_t *s );

/**
 * This function creates a raw stream_t from an URL.
 */
stream_t *stream_AccessNew(vlc_object_t *, input_thread_t *, bool, const char *);

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
 * You must release the returned value using vlc_stream_Delete unless it is
 * used as a source to another filter.
 */
stream_t *stream_FilterChainNew( stream_t *p_source, const char *psz_chain );

/**
 * Attach \ref stream_extractor%s according to specified data
 *
 * This function will parse the passed data, and try to attach a \ref
 * stream_extractor for each specified entity as per the fragment specification
 * associated with a \ref mrl,
 *
 * \warning The data in `*stream` can be modified even if this function only
 *          locates some of the entities specified in `psz_data`. It is up to
 *          the caller to free the resource referred to by `*stream`, no matter
 *          what this function returns.
 *
 * \warning Please see \ref vlc_stream_extractor_Attach for a function that
 *          will not modify the passed stream upon failure. \ref
 *          stream_extractor_AttachParsed shall only be used when the caller
 *          only cares about the stream on successful attachment of **all**
 *          stream-extractors referred to by `psz_data`, something which is not
 *          guaranteed.
 *
 * \param[out] source a pointer-to-pointer to stream where the attached
 *             stream-extractor will be applied. `*stream` will refer
 *             to the last successful attachment.
 * \param[out] out_extra `*out_extra` will point to any additional data
 *             in `psz_data` that does not specify an entity (if any).
 * \return VLC_SUCCESS on success, an error-code on failure
 **/
int stream_extractor_AttachParsed( stream_t** stream, const char* psz_data,
                                   char const** out_extra );

char *get_path(const char *location);

#endif
