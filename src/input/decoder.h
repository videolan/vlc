/*****************************************************************************
 * decoder.h: Input decoder functions
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

#ifndef LIBVLC_INPUT_DECODER_H
#define LIBVLC_INPUT_DECODER_H 1

#include <vlc_common.h>
#include <vlc_decoder.h>
#include <vlc_codec.h>
#include <vlc_mouse.h>

#include "input_internal.h"

struct vlc_clock_t;

struct vlc_input_decoder_callbacks {
    /* notifications */
    void (*on_vout_started)(vlc_input_decoder_t *decoder, vout_thread_t *vout,
                            enum vlc_vout_order vout_order,
                            void *userdata);
    void (*on_vout_stopped)(vlc_input_decoder_t *decoder, vout_thread_t *vout,
                            void *userdata);
    void (*on_output_paused)(vlc_input_decoder_t *decoder, bool paused,
                             vlc_tick_t pause_date,
                             void *userdata);

    void (*on_thumbnail_ready)(vlc_input_decoder_t *decoder, picture_t *pic,
                               void *userdata);

    void (*on_new_video_stats)(vlc_input_decoder_t *decoder, unsigned decoded,
                               unsigned lost, unsigned displayed, unsigned late,
                               void *userdata);
    void (*on_new_audio_stats)(vlc_input_decoder_t *decoder, unsigned decoded,
                               unsigned lost, unsigned played, void *userdata);
    void (*frame_next_status)(vlc_input_decoder_t *decoder, int status,
                              void *userdata);
    void (*frame_next_need_data)(vlc_input_decoder_t *decoder, bool need_data,
                                 void *userdata);
    void (*frame_previous_status)(vlc_input_decoder_t *decoder, int status,
                                  void *userdata);
    void (*frame_previous_seek)(vlc_input_decoder_t *decoder, vlc_tick_t pts,
                                unsigned frame_rate, unsigned frame_rate_base,
                                int steps, bool failed, void *userdata);
    /* requests */
    int (*get_attachments)(vlc_input_decoder_t *decoder,
                           input_attachment_t ***ppp_attachment,
                           void *userdata);
};

struct vlc_input_decoder_cfg
{
    const es_format_t *fmt;
    const char *str_id;
    struct vlc_clock_t *clock;
    input_resource_t *resource;
    sout_stream_t *sout;
    enum input_type input_type;
    bool hw_dec;
    unsigned cc_decoder;
    const struct vlc_input_decoder_callbacks *cbs;
    void *cbs_data;
};

vlc_input_decoder_t *
vlc_input_decoder_New( vlc_object_t *parent,
                       const struct vlc_input_decoder_cfg *cfg );

/**
 * This function changes the pause state.
 * The date parameter MUST hold the exact date at which the change has been
 * done for proper vout/aout pausing.
 */
void vlc_input_decoder_ChangePause( vlc_input_decoder_t *, bool b_paused, vlc_tick_t i_date );

/**
 * Changes the decoder rate.
 *
 * This function changes rate of the intended playback speed to nominal speed.
 * \param dec decoder
 * \param rate playback rate (default is 1)
 */
void vlc_input_decoder_ChangeRate( vlc_input_decoder_t *dec, float rate );

/**
 * This function makes the decoder start waiting for a valid data block from its fifo.
 */
void vlc_input_decoder_StartWait( vlc_input_decoder_t * );

/**
 * This function waits for the decoder to actually receive data.
 */
void vlc_input_decoder_Wait( vlc_input_decoder_t * );

/**
 * This function exits the waiting mode of the decoder.
 */
void vlc_input_decoder_StopWait( vlc_input_decoder_t * );

/**
 * This function returns true if the decoder fifo is empty and false otherwise.
 */
bool vlc_input_decoder_IsEmpty( vlc_input_decoder_t * );

/**
 * This function forces the display of the next picture
 */
void vlc_input_decoder_FrameNext( vlc_input_decoder_t *p_dec );

/**
 * This function forces the display of the previous picture
 */
void vlc_input_decoder_FramePrevious( vlc_input_decoder_t *p_dec );

/*
 * This function reset the decoder for normal playback after a previous or a
 * next frame call.
 */
void vlc_input_decoder_StopFrameNext( vlc_input_decoder_t *p_dec );

/**
 * This function Creates and adds the requested SubDec.
 *
 * The sub decoder returned by this function must be deleted with
 * vlc_input_decoder_Delete() before the parent is deleted.
 */
vlc_input_decoder_t *
vlc_input_decoder_CreateSubDec(vlc_input_decoder_t *dec,
                               const struct vlc_input_decoder_cfg *cfg);

struct vlc_subdec_desc
{
    es_format_t *fmt_array;
    size_t fmt_count;
};

void vlc_subdec_desc_Clean(struct vlc_subdec_desc *desc);


struct vlc_input_decoder_status
{
    struct {
        /* True if the ES format or meta data have changed since the last call.
         * */
        bool changed;
        /* If changed is true, a copy of the current es_format_t, MUST be freed
         * with es_format_Clean() */
        es_format_t fmt;
        /* If changed is true, a copy of the current description, can be NULL,
         * MUST be freed with vlc_meta_Delete.() */
        vlc_meta_t *meta;
    } format;

    struct vlc_subdec_desc subdec_desc;
};

/**
 * Get the last status of the decoder.
 */
void vlc_input_decoder_DecodeWithStatus(vlc_input_decoder_t *p_dec,
                                        vlc_frame_t *frame, bool do_pace,
                                        struct vlc_input_decoder_status *status);

/**
 * This function returns the current size in bytes of the decoder fifo
 */
size_t vlc_input_decoder_GetFifoSize( vlc_input_decoder_t *p_dec );

int vlc_input_decoder_GetVbiPage( vlc_input_decoder_t *, bool *opaque );
int vlc_input_decoder_SetVbiPage( vlc_input_decoder_t *, unsigned page );
int vlc_input_decoder_SetVbiOpaque( vlc_input_decoder_t *, bool opaque );

void vlc_input_decoder_SetVoutMouseEvent( vlc_input_decoder_t *, vlc_mouse_event, void * );
int  vlc_input_decoder_AddVoutOverlay( vlc_input_decoder_t *, subpicture_t *, size_t * );
int  vlc_input_decoder_DelVoutOverlay( vlc_input_decoder_t *, size_t );

#endif
