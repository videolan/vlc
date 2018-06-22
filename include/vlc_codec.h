/*****************************************************************************
 * vlc_codec.h: Definition of the decoder and encoder structures
 *****************************************************************************
 * Copyright (C) 1999-2003 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

#ifndef VLC_CODEC_H
#define VLC_CODEC_H 1

#include <assert.h>

#include <vlc_block.h>
#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_subpicture.h>

/**
 * \defgroup codec Codec
 * Decoders and encoders
 * @{
 * \file
 * Decoder and encoder modules interface
 *
 * \defgroup decoder Decoder
 * Audio, video and text decoders
 * @{
 */

typedef struct decoder_owner_sys_t decoder_owner_sys_t;

typedef struct decoder_cc_desc_t decoder_cc_desc_t;

/*
 * BIG FAT WARNING : the code relies in the first 4 members of filter_t
 * and decoder_t to be the same, so if you have anything to add, do it
 * at the end of the structure.
 */
struct decoder_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t *          p_module;
    decoder_sys_t *     p_sys;

    /* Input format ie from demuxer (XXX: a lot of field could be invalid) */
    es_format_t         fmt_in;

    /* Output format of decoder/packetizer */
    es_format_t         fmt_out;

    /* Tell the decoder if it is allowed to drop frames */
    bool                b_frame_drop_allowed;

#   define VLCDEC_SUCCESS   VLC_SUCCESS
#   define VLCDEC_ECRITICAL VLC_EGENERIC
#   define VLCDEC_RELOAD    (-100)
    /* This function is called to decode one packetized block.
     *
     * The module implementation will own the input block (p_block) and should
     * process and release it. Depending of the decoder type, the module should
     * send output frames/blocks via decoder_QueueVideo(), decoder_QueueAudio()
     * or decoder_QueueSub().
     *
     * If p_block is NULL, the decoder asks the module to drain itself. The
     * module should return all available output frames/block via the queue
     * functions.
     *
     * Return values can be:
     *  VLCDEC_SUCCESS: pf_decode will be called again
     *  VLCDEC_ECRITICAL: in case of critical error, pf_decode won't be called
     *  again.
     *  VLCDEC_RELOAD: Request that the decoder should be reloaded. The current
     *  module will be unloaded. Reloading a module may cause a loss of frames.
     *  When returning this status, the implementation shouldn't release or
     *  modify the p_block in argument (The same p_block will be feed to the
     *  next decoder module).
     */
    int                 ( * pf_decode )   ( decoder_t *, block_t *p_block );

    /* This function is called in a loop with the same pp_block argument until
     * it returns NULL. This allows a module implementation to return more than
     * one output blocks for one input block.
     *
     * pp_block or *pp_block can be NULL.
     *
     * If pp_block and *pp_block are not NULL, the module implementation will
     * own the input block (*pp_block) and should process and release it. The
     * module can also process a part of the block. In that case, it should
     * modify (*pp_block)->p_buffer/i_buffer accordingly and return a valid
     * output block. The module can also set *pp_block to NULL when the input
     * block is consumed.
     *
     * If pp_block is not NULL but *pp_block is NULL, a previous call of the pf
     * function has set the *pp_block to NULL. Here, the module can return new
     * output block for the same, already processed, input block (the
     * pf_packetize function will be called as long as the module return an
     * output block).
     *
     * When the pf function returns NULL, the next call to this function will
     * have a new a valid pp_block (if the packetizer is not drained).
     *
     * If pp_block is NULL, the packetizer asks the module to drain itself. In
     * that case, the module has to return all output frames available (the
     * pf_packetize function will be called as long as the module return an
     * output block).
     */
    block_t *           ( * pf_packetize )( decoder_t *, block_t **pp_block );
    /* */
    void                ( * pf_flush ) ( decoder_t * );

    /* Closed Caption (CEA 608/708) extraction.
     * If set, it *may* be called after pf_packetize returned data. It should
     * return CC for the pictures returned by the last pf_packetize call only,
     * channel bitmaps will be used to known which cc channel are present (but
     * globaly, not necessary for the current packet. Video decoders should use
     * the decoder_QueueCc() function to pass closed captions. */
    block_t *           ( * pf_get_cc )      ( decoder_t *, decoder_cc_desc_t * );

    /* Meta data at codec level
     *  The decoder owner set it back to NULL once it has retreived what it needs.
     *  The decoder owner is responsible of its release except when you overwrite it.
     */
    vlc_meta_t          *p_description;

    /*
     * Owner fields
     * XXX You MUST not use them directly.
     */

    /* Video output callbacks
     * XXX use decoder_NewPicture */
    int             (*pf_vout_format_update)( decoder_t * );
    picture_t      *(*pf_vout_buffer_new)( decoder_t * );

    /**
     * Number of extra (ie in addition to the DPB) picture buffers
     * needed for decoding.
     */
    int             i_extra_picture_buffers;

    /* Audio output callbacks */
    int             (*pf_aout_format_update)( decoder_t * );

    /* SPU output callbacks
     * XXX use decoder_NewSubpicture */
    subpicture_t   *(*pf_spu_buffer_new)( decoder_t *, const subpicture_updater_t * );

    /* Input attachments
     * XXX use decoder_GetInputAttachments */
    int             (*pf_get_attachments)( decoder_t *p_dec, input_attachment_t ***ppp_attachment, int *pi_attachment );

    /* Display date
     * XXX use decoder_GetDisplayDate */
    vlc_tick_t      (*pf_get_display_date)( decoder_t *, vlc_tick_t );

    /* Display rate
     * XXX use decoder_GetDisplayRate */
    int             (*pf_get_display_rate)( decoder_t * );

    /* XXX use decoder_QueueVideo or decoder_QueueVideoWithCc */
    int             (*pf_queue_video)( decoder_t *, picture_t * );
    /* XXX use decoder_QueueAudio */
    int             (*pf_queue_audio)( decoder_t *, block_t * );
    /* XXX use decoder_QueueCC */
    int             (*pf_queue_cc)( decoder_t *, block_t *, const decoder_cc_desc_t * );
    /* XXX use decoder_QueueSub */
    int             (*pf_queue_sub)( decoder_t *, subpicture_t *);
    void             *p_queue_ctx;

    /* Private structure for the owner of the decoder */
    decoder_owner_sys_t *p_owner;
};

/* struct for packetizer get_cc polling/decoder queue_cc
 * until we have a proper metadata way */
struct decoder_cc_desc_t
{
    uint8_t i_608_channels;  /* 608 channels bitmap */
    uint64_t i_708_channels; /* 708 */
    int i_reorder_depth;     /* reorder depth, -1 for no reorder, 0 for old P/B flag based */
};

/**
 * @}
 */

/**
 * \defgroup encoder Encoder
 * Audio, video and text encoders
 * @{
 */

struct encoder_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t *          p_module;
    encoder_sys_t *     p_sys;

    /* Properties of the input data fed to the encoder */
    es_format_t         fmt_in;

    /* Properties of the output of the encoder */
    es_format_t         fmt_out;

    block_t *           ( * pf_encode_video )( encoder_t *, picture_t * );
    block_t *           ( * pf_encode_audio )( encoder_t *, block_t * );
    block_t *           ( * pf_encode_sub )( encoder_t *, subpicture_t * );

    /* Common encoder options */
    int i_threads;               /* Number of threads to use during encoding */
    int i_iframes;               /* One I frame per i_iframes */
    int i_bframes;               /* One B frame per i_bframes */
    int i_tolerance;             /* Bitrate tolerance */

    /* Encoder config */
    config_chain_t *p_cfg;
};

/**
 * @}
 *
 * \ingroup decoder
 * @{
 */

/**
 * Updates the video output format.
 *
 * This function notifies the video output pipeline of a new video output
 * format (fmt_out.video). If there was no video output from the decoder so far
 * or if the video output format has changed, a new video output will be set
 * up. decoder_NewPicture() can then be used to allocate picture buffers.
 *
 * If the format is unchanged, this function has no effects and returns zero.
 *
 * \note
 * This function is not reentrant.
 *
 * @return 0 if the video output was set up successfully, -1 otherwise.
 */
VLC_USED
static inline int decoder_UpdateVideoFormat( decoder_t *dec )
{
    assert( dec->fmt_in.i_cat == VIDEO_ES );
    if( dec->fmt_in.i_cat == VIDEO_ES && dec->pf_vout_format_update != NULL )
        return dec->pf_vout_format_update( dec );
    else
        return -1;
}

/**
 * Allocates an output picture buffer.
 *
 * This function pulls an output picture buffer for the decoder from the
 * buffer pool of the video output. The picture must be released with
 * picture_Release() when it is no longer referenced by the decoder.
 *
 * \note
 * This function is reentrant. However, decoder_UpdateVideoFormat() cannot be
 * used concurrently; the caller is responsible for serialization.
 *
 * \warning
 * The behaviour is undefined if decoder_UpdateVideoFormat() was not called or
 * if the last call returned an error.
 *
 * \return a picture buffer on success, NULL on error
 */
VLC_USED
static inline picture_t *decoder_NewPicture( decoder_t *dec )
{
    return dec->pf_vout_buffer_new( dec );
}

/**
 * Abort any calls of decoder_NewPicture
 *
 * If b_abort is true, all pending and futures calls of decoder_NewPicture
 * will be aborted. This function can be used by asynchronous video decoders
 * to unblock a thread that is waiting for a picture.
 */
VLC_API void decoder_AbortPictures( decoder_t *dec, bool b_abort );

/**
 * This function queues a single picture to the video output.
 *
 * \note
 * The caller doesn't own the picture anymore after this call (even in case of
 * error).
 * FIXME: input_DecoderFrameNext won't work if a module use this function.
 *
 * \return 0 if the picture is queued, -1 on error
 */
static inline int decoder_QueueVideo( decoder_t *dec, picture_t *p_pic )
{
    assert( p_pic->p_next == NULL );
    assert( dec->pf_queue_video != NULL );
    return dec->pf_queue_video( dec, p_pic );
}

/**
 * This function queues the Closed Captions
 *
 * \param dec the decoder object
 * \param p_cc the closed-caption to queue
 * \param p_desc decoder_cc_desc_t description structure
 * \return 0 if queued, -1 on error
 */
static inline int decoder_QueueCc( decoder_t *dec, block_t *p_cc,
                                   const decoder_cc_desc_t *p_desc )
{
    if( dec->pf_queue_cc == NULL )
    {
        block_Release( p_cc );
        return -1;
    }
    return dec->pf_queue_cc( dec, p_cc, p_desc );
}

/**
 * This function queues a single audio block to the audio output.
 *
 * \note
 * The caller doesn't own the audio block anymore after this call (even in case
 * of error).
 *
 * \return 0 if the block is queued, -1 on error
 */
static inline int decoder_QueueAudio( decoder_t *dec, block_t *p_aout_buf )
{
    assert( p_aout_buf->p_next == NULL );
    assert( dec->pf_queue_audio != NULL );
    return dec->pf_queue_audio( dec, p_aout_buf );
}

/**
 * This function queues a single subtitle to the video output.
 *
 * \note
 * The caller doesn't own the subtitle anymore after this call (even in case of
 * error).
 *
 * \return 0 if the subtitle is queued, -1 on error
 */
static inline int decoder_QueueSub( decoder_t *dec, subpicture_t *p_spu )
{
    assert( p_spu->p_next == NULL );
    assert( dec->pf_queue_sub != NULL );
    return dec->pf_queue_sub( dec, p_spu );
}

/**
 * This function notifies the audio output pipeline of a new audio output
 * format (fmt_out.audio). If there is currently no audio output or if the
 * audio output format has changed, a new audio output will be set up.
 * @return 0 if the audio output is working, -1 if not. */
VLC_USED
static inline int decoder_UpdateAudioFormat( decoder_t *dec )
{
    assert(dec->fmt_in.i_cat == AUDIO_ES);
    if( dec->fmt_in.i_cat == AUDIO_ES && dec->pf_aout_format_update != NULL )
        return dec->pf_aout_format_update( dec );
    else
        return -1;
}

/**
 * This function will return a new audio buffer usable by a decoder as an
 * output buffer. It must be released with block_Release() or returned it to
 * the caller as a decoder_QueueAudio parameter.
 */
VLC_API block_t * decoder_NewAudioBuffer( decoder_t *, int i_nb_samples ) VLC_USED;

/**
 * This function will return a new subpicture usable by a decoder as an output
 * buffer. You have to release it using subpicture_Delete() or by returning
 * it to the caller as a decoder_QueueSub parameter.
 */
VLC_API subpicture_t * decoder_NewSubpicture( decoder_t *, const subpicture_updater_t * ) VLC_USED;

/**
 * This function gives all input attachments at once.
 *
 * You MUST release the returned values
 */
VLC_API int decoder_GetInputAttachments( decoder_t *, input_attachment_t ***ppp_attachment, int *pi_attachment );

/**
 * This function converts a decoder timestamp into a display date comparable
 * to mdate().
 * You MUST use it *only* for gathering statistics about speed.
 */
VLC_API vlc_tick_t decoder_GetDisplayDate( decoder_t *, vlc_tick_t ) VLC_USED;

/**
 * This function returns the current input rate.
 * You MUST use it *only* for gathering statistics about speed.
 */
VLC_API int decoder_GetDisplayRate( decoder_t * ) VLC_USED;

/** @} */
/** @} */
#endif /* _VLC_CODEC_H */
