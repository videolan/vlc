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

#include "vlc_subpicture.h"

typedef struct vlc_input_decoder_t vlc_input_decoder_t;

/**
 * This defines an opaque input resource handler.
 */
typedef struct input_resource_t input_resource_t;

struct vlc_video_output_callbacks;

/* */
struct vlc_clock_t;
VLC_API vlc_input_decoder_t *
vlc_input_decoder_Create( vlc_object_t *, const es_format_t *,
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

VLC_API void vlc_input_decoder_Decode( vlc_input_decoder_t *, block_t *, bool b_do_pace );
VLC_API void vlc_input_decoder_Drain( vlc_input_decoder_t * );
VLC_API void vlc_input_decoder_Flush( vlc_input_decoder_t * );
VLC_API int  vlc_input_decoder_SetSpuHighlight( vlc_input_decoder_t *, const vlc_spu_highlight_t * );
VLC_API void vlc_input_decoder_ChangeDelay( vlc_input_decoder_t *, vlc_tick_t i_delay );

/**
 * It creates an empty input resource handler.
 *
 * The given object MUST stay alive as long as the input_resource_t is
 * not deleted.
 */
VLC_API input_resource_t * input_resource_New(vlc_object_t *,
        const struct vlc_video_output_callbacks *cbs,
        void *owner) VLC_USED;

/**
 * It releases an input resource.
 */
VLC_API void input_resource_Release( input_resource_t * );

/** @} */
#endif
