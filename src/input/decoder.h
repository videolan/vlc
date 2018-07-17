/*****************************************************************************
 * decoder.h: Input decoder functions
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

#ifndef LIBVLC_INPUT_DECODER_H
#define LIBVLC_INPUT_DECODER_H 1

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_mouse.h>

decoder_t *input_DecoderNew( input_thread_t *, es_format_t *, input_clock_t *,
                             sout_instance_t * ) VLC_USED;

/**
 * This function changes the pause state.
 * The date parameter MUST hold the exact date at which the change has been
 * done for proper vout/aout pausing.
 */
void input_DecoderChangePause( decoder_t *, bool b_paused, vlc_tick_t i_date );

/**
 * Changes the decoder rate.
 *
 * This function changes rate of the intended playback speed to nominal speed.
 * \param dec decoder
 * \param rate playback rate (default is 1)
 */
void input_DecoderChangeRate( decoder_t *dec, float rate );

/**
 * This function changes the delay.
 */
void input_DecoderChangeDelay( decoder_t *, vlc_tick_t i_delay );

/**
 * This function makes the decoder start waiting for a valid data block from its fifo.
 */
void input_DecoderStartWait( decoder_t * );

/**
 * This function waits for the decoder to actually receive data.
 */
void input_DecoderWait( decoder_t * );

/**
 * This function exits the waiting mode of the decoder.
 */
void input_DecoderStopWait( decoder_t * );

/**
 * This function returns true if the decoder fifo is empty and false otherwise.
 */
bool input_DecoderIsEmpty( decoder_t * );

/**
 * This function activates the request closed caption channel.
 */
int input_DecoderSetCcState( decoder_t *, vlc_fourcc_t, int i_channel, bool b_decode );

/**
 * This function returns an error if the requested channel does not exist and
 * set pb_decode to the channel status(active or not) otherwise.
 */
int input_DecoderGetCcState( decoder_t *, vlc_fourcc_t, int i_channel, bool *pb_decode );

/**
 * This function get cc channels descriptions
 */
void input_DecoderGetCcDesc( decoder_t *, decoder_cc_desc_t * );

/**
 * This function force the display of the next picture and fills the stream
 * time consumed.
 */
void input_DecoderFrameNext( decoder_t *p_dec, vlc_tick_t *pi_duration );

/**
 * This function will return true if the ES format or meta data have changed since
 * the last call. In which case, it will do a copy of the current es_format_t if p_fmt
 * is not NULL and will do a copy of the current description if pp_meta is non NULL.
 * The es_format_t MUST be freed by es_format_Clean and *pp_meta MUST be freed by
 * vlc_meta_Delete.
 * Otherwise it will return false and will not initialize p_fmt and *pp_meta.
 */
bool input_DecoderHasFormatChanged( decoder_t *p_dec, es_format_t *p_fmt, vlc_meta_t **pp_meta );

/**
 * This function returns the current size in bytes of the decoder fifo
 */
size_t input_DecoderGetFifoSize( decoder_t *p_dec );

/**
 * This function returns the objects associated to a decoder
 *
 * They must be released using vlc_object_release().
 */
void input_DecoderGetObjects( decoder_t *, vout_thread_t **, audio_output_t ** );

void input_DecoderSetVoutMouseEvent( decoder_t *, vlc_mouse_event, void * );

int  input_DecoderAddVoutOverlay( decoder_t *, subpicture_t *, int * );
int  input_DecoderFlushVoutOverlay( decoder_t *, int );

#endif
