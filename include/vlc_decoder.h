/*****************************************************************************
 * vlc_decoder.h: decoder API
 *****************************************************************************
 * Copyright (C) 1999-2015 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC_DECODER_H
#define VLC_DECODER_H 1

/**
 * \ingroup decoder
 * @{
 */

#include <vlc_tick.h>

typedef struct vlc_input_decoder_t vlc_input_decoder_t;
typedef struct vlc_spu_highlight_t vlc_spu_highlight_t;

/**
 * This defines an opaque input resource handler.
 */
typedef struct input_resource_t input_resource_t;

/* */
struct vlc_clock_t;
struct vlc_frame_t;


/**
 * Spawn a decoder thread outside of the input thread.
 */
VLC_API vlc_input_decoder_t *
vlc_input_decoder_Create( vlc_object_t *, const es_format_t *, const char *es_id,
                          struct vlc_clock_t *, input_resource_t * ) VLC_USED;

/**
 * Delete an existing vlc_input_decoder_t instance.
 *
 * Close the decoder implementation and delete the vlc_input_decoder_t
 * instance.
 * The instance must have been drained using vlc_input_decoder_Drain() or
 * flushed using vlc_input_decoder_Flush() after any previous call to
 * vlc_input_decoder_Decode() before calling the destructor.
 *
 * @param decoder The vlc_input_decoder_t to delete, created from
 *        vlc_input_decoder_Create().
 */
VLC_API void vlc_input_decoder_Delete( vlc_input_decoder_t * decoder);

/**
 * Put a vlc_frame_t in the decoder's fifo.
 * Thread-safe w.r.t. the decoder. May be a cancellation point.
 *
 * @param p_dec the decoder object
 * @param frame the data frame
 * @param do_pace whether we wait for some decoding to happen or not
 */
VLC_API void vlc_input_decoder_Decode( vlc_input_decoder_t *p_dec, struct vlc_frame_t *frame, bool do_pace );

/**
 * Signals that there are no further frames to decode, and requests that the
 * decoder drain all pending buffers. This is used to ensure that all
 * intermediate buffers empty and no samples get lost at the end of the stream.
 *
 * @note The function does not actually wait for draining. It just signals that
 * draining should be performed once the decoder has emptied FIFO.
 */
VLC_API void vlc_input_decoder_Drain( vlc_input_decoder_t * );

/**
 * Returns the drained state
 *
 * @warning This function need to be polled (every few ms) to know when the
 * decoder is drained
 * @return true if drained (after a call to vlc_input_decoder_Drain())
 */
 VLC_API bool vlc_input_decoder_IsDrained( vlc_input_decoder_t * );

/**
 * Requests that the decoder immediately discard all pending buffers.
 * This is useful when seeking or when deselecting a stream.
 */
VLC_API void vlc_input_decoder_Flush( vlc_input_decoder_t * );
VLC_API int  vlc_input_decoder_SetSpuHighlight( vlc_input_decoder_t *, const vlc_spu_highlight_t * );
VLC_API void vlc_input_decoder_ChangeDelay( vlc_input_decoder_t *, vlc_tick_t i_delay );

/**
 * It creates an empty input resource handler.
 *
 * The given object MUST stay alive as long as the input_resource_t is
 * not deleted.
 */
VLC_API input_resource_t * input_resource_New( vlc_object_t * ) VLC_USED;

/**
 * It releases an input resource.
 */
VLC_API void input_resource_Release( input_resource_t * );

/** @} */
#endif
