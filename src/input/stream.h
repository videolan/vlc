/*****************************************************************************
 * stream.h: Input stream functions
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
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
#include "input_internal.h"

stream_t *vlc_stream_CustomNew(vlc_object_t *parent,
                               void (*destroy)(stream_t *), size_t extra_size,
                               const char *type_name);
void *vlc_stream_Private(stream_t *stream);

/* */
void stream_CommonDelete( stream_t *s );

stream_t *vlc_stream_AttachmentNew(vlc_object_t *p_this,
                                   input_attachment_t *attachement);

/**
 * This function creates a raw stream_t from an URL.
 */
stream_t *stream_AccessNew(vlc_object_t *, input_thread_t *, es_out_t *, bool,
                           const char *);

/**
 * Probes stream filters automatically.
 *
 * This function automatically and repeatedly probes for applicable stream
 * filters to append downstream of an existing stream. Any such filter will
 * convert the stream into another stream, e.g. decompressing it or extracting
 * the list of contained files (playlist).
 *
 * This function transfers ownership of the supplied stream to the following
 * stream filter, of the first stream filter to the second stream filter, and
 * so on. Any attempt to access the original stream filter directly is
 * explicitly undefined.
 *
 * If, and only if, no filters were probed succesfully, a pointer to the
 * unchanged source stream will be returned. Otherwise, this returns a stream
 * filter. The return value is thus guaranteed to be non-NULL.
 *
 * @param source input stream around which to build a filter chain
 *
 * @return the last, most downstream stream object.
 *
 * @note The return value must be freed with vlc_stream_Delete() after use.
 * This will automatically free the whole chain and the underlying stream.
 */
stream_t *stream_FilterAutoNew( stream_t *source ) VLC_USED;

/**
 * Builds an explicit chain of stream filters.
 *
 * This function creates a chain of filters according to a supplied list.
 *
 * See also stream_FilterAutoNew(). Those two functions have identical
 * semantics; the only difference lies in how the list of probed filters is
 * determined (manually versus automatically).
 *
 * If the list is empty, or if probing each of the requested filters failed,
 * this function will return a pointer to the supplied source stream.
 *
 * @param source input stream around which to build a filter chain
 * @param list colon-separated list of stream filters (upstream first)
 *
 * @return The last stream (filter) in the chain.
 * The return value is always a valid (non-NULL) stream pointer.
 */
stream_t *stream_FilterChainNew( stream_t *source, const char *list ) VLC_USED;

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
