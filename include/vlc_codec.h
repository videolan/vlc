/*****************************************************************************
 * vlc_codec.h: Definition of the decoder and encoder structures
 *****************************************************************************
 * Copyright (C) 1999-2003 VLC authors and VideoLAN
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
#include <vlc_window.h>
#include <vlc_picture.h>
#include <vlc_subpicture.h>

/**
 * \defgroup decoder Decoder
 * \ingroup input
 * Audio, video and text decoders
 * @{
 *
 * \file
 * Decoder and encoder modules interface
 */

typedef struct decoder_cc_desc_t decoder_cc_desc_t;

struct decoder_owner_callbacks
{
    union
    {
        struct
        {
            vlc_decoder_device * (*get_device)( decoder_t * );
            int         (*format_update)( decoder_t *, vlc_video_context * );

            /* cf. decoder_NewPicture, can be called from any decoder thread */
            picture_t*  (*buffer_new)( decoder_t * );
            /* cf.decoder_QueueVideo */
            void        (*queue)( decoder_t *, picture_t * );
            /* cf.decoder_QueueCC */
            void        (*queue_cc)( decoder_t *, vlc_frame_t *,
                                     const decoder_cc_desc_t * );

            /* Display date
             * cf. decoder_GetDisplayDate */
            vlc_tick_t  (*get_display_date)( decoder_t *, vlc_tick_t, vlc_tick_t );
            /* Display rate
             * cf. decoder_GetDisplayRate */
            float       (*get_display_rate)( decoder_t * );
        } video;
        struct
        {
            int     (*format_update)( decoder_t * );

            /* cf.decoder_QueueAudio */
            void    (*queue)( decoder_t *, vlc_frame_t * );
        } audio;
        struct
        {
            /* cf. decoder_NewSubpicture */
            subpicture_t*   (*buffer_new)( decoder_t *,
                                           const subpicture_updater_t * );

            /* cf.decoder_QueueSub */
            void            (*queue)( decoder_t *, subpicture_t *);
        } spu;
    };

    /* Input attachments
     * cf. decoder_GetInputAttachments */
    int (*get_attachments)( decoder_t *p_dec,
                            input_attachment_t ***ppp_attachment,
                            int *pi_attachment );
};

/*
 * BIG FAT WARNING : the code relies in the first 4 members of filter_t
 * and decoder_t to be the same, so if you have anything to add, do it
 * at the end of the structure.
 */
struct decoder_t
{
    struct vlc_object_t obj;

    /* Module properties */
    module_t *          p_module;
    void               *p_sys;

    /* Input format ie from demuxer (XXX: a lot of fields could be invalid),
       cannot be NULL */
    const es_format_t   *fmt_in;

    /* Output format of decoder/packetizer */
    es_format_t         fmt_out;

    /* Tell the decoder if it is allowed to drop frames */
    bool                b_frame_drop_allowed;

    /**
     * Number of extra (ie in addition to the DPB) picture buffers
     * needed for decoding.
     */
    int                 i_extra_picture_buffers;

    union
    {
#       define VLCDEC_SUCCESS   VLC_SUCCESS
#       define VLCDEC_ECRITICAL VLC_EGENERIC
#       define VLCDEC_RELOAD    (-100)
        /* This function is called to decode one packetized block.
         *
         * The module implementation will own the input block (p_block) and should
         * process and release it. Depending of the decoder type, the module should
         * send output frames/blocks via decoder_QueueVideo(), decoder_QueueAudio()
         * or decoder_QueueSub().
         *
         * If frame is NULL, the decoder asks the module to drain itself. The
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
         *  modify the frame in argument (The same frame will be feed to the
         *  next decoder module).
         */
        int             ( * pf_decode )   ( decoder_t *, vlc_frame_t *frame );

        /* This function is called in a loop with the same pp_block argument until
         * it returns NULL. This allows a module implementation to return more than
         * one output blocks for one input block.
         *
         * pp_block or *pp_block can be NULL.
         *
         * If pp_block and *pp_block are not NULL, the module implementation will
         * own the input block (*pp_block) and should process and release it. The
         * module can also process a part of the block. In that case, it should
         * modify (*ppframe)->p_buffer/i_buffer accordingly and return a valid
         * output block. The module can also set *ppframe to NULL when the input
         * block is consumed.
         *
         * If ppframe is not NULL but *ppframe is NULL, a previous call of the pf
         * function has set the *ppframe to NULL. Here, the module can return new
         * output block for the same, already processed, input block (the
         * pf_packetize function will be called as long as the module return an
         * output block).
         *
         * When the pf function returns NULL, the next call to this function will
         * have a new a valid ppframe (if the packetizer is not drained).
         *
         * If ppframe is NULL, the packetizer asks the module to drain itself. In
         * that case, the module has to return all output frames available (the
         * pf_packetize function will be called as long as the module return an
         * output block).
         */
        vlc_frame_t *   ( * pf_packetize )( decoder_t *, vlc_frame_t  **ppframe );
    };

    /* */
    void                ( * pf_flush ) ( decoder_t * );

    /* Closed Caption (CEA 608/708) extraction.
     * If set, it *may* be called after pf_packetize returned data. It should
     * return CC for the pictures returned by the last pf_packetize call only,
     * channel bitmaps will be used to known which cc channel are present (but
     * globaly, not necessary for the current packet. Video decoders should use
     * the decoder_QueueCc() function to pass closed captions. */
    vlc_frame_t *       ( * pf_get_cc )      ( decoder_t *, decoder_cc_desc_t * );

    /* Meta data at codec level
     *  The decoder owner set it back to NULL once it has retrieved what it needs.
     *  The decoder owner is responsible of its release except when you overwrite it.
     */
    vlc_meta_t          *p_description;

    /* Private structure for the owner of the decoder */
    const struct decoder_owner_callbacks *cbs;
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

struct encoder_owner_callbacks
{
    struct
    {
        vlc_decoder_device *(*get_device)( encoder_t * );
    } video;
};

/**
 * Creates/Updates the output decoder device.
 *
 * \note
 * This function is not reentrant.
 *
 * @return the held decoder device, NULL if none should be used
 */
VLC_API vlc_decoder_device *vlc_encoder_GetDecoderDevice( encoder_t * );


/**
 * \defgroup encoder Encoder
 * \ingroup sout
 * Audio, video and text encoders
 * @{
 */

struct vlc_encoder_operations
{
    void (*close)(encoder_t *);

    union {
        block_t * (*encode_video)(encoder_t *, picture_t *);
        block_t * (*encode_audio)(encoder_t *, block_t *);
        block_t * (*encode_sub)(encoder_t *, subpicture_t *);
    };
};

struct encoder_t
{
    struct vlc_object_t obj;

    /* Module properties */
    module_t *          p_module;
    void               *p_sys;

    /* Properties of the input data fed to the encoder */
    es_format_t         fmt_in;
    vlc_video_context   *vctx_in; /* for video */

    /* Properties of the output of the encoder */
    es_format_t         fmt_out;

    /* Common encoder options */
    int i_threads;               /* Number of threads to use during encoding */
    int i_iframes;               /* One I frame per i_iframes */
    int i_bframes;               /* One B frame per i_bframes */
    int i_tolerance;             /* Bitrate tolerance */

    /* Encoder config */
    config_chain_t *p_cfg;

    /* Private structure for the owner of the encoder */
    const struct vlc_encoder_operations *ops;
    const struct encoder_owner_callbacks *cbs;
};

VLC_API void vlc_encoder_Destroy(encoder_t *encoder);

static inline block_t *
vlc_encoder_EncodeVideo(encoder_t *encoder, picture_t *pic)
{
    assert(encoder->fmt_in.i_cat == VIDEO_ES);
    return encoder->ops->encode_video(encoder, pic);
}

static inline block_t *
vlc_encoder_EncodeAudio(encoder_t *encoder, block_t *audio)
{
    assert(encoder->fmt_in.i_cat == AUDIO_ES);
    return encoder->ops->encode_audio(encoder, audio);
}

static inline block_t *
vlc_encoder_EncodeSub(encoder_t *encoder, subpicture_t *sub)
{
    assert(encoder->fmt_in.i_cat == SPU_ES);
    return encoder->ops->encode_sub(encoder, sub);
}

/**
 * @}
 *
 * \ingroup decoder
 * @{
 */

/**
 * Creates/Updates the output decoder device.
 *
 * This function notifies the video output pipeline of a new video output
 * format (fmt_out.video). If there was no decoder device so far or a new
 * decoder device is required, a new decoder device will be set up.
 * decoder_UpdateVideoOutput() can then be used.
 *
 * If the format is unchanged, this function has no effects and returns zero.
 *
 * \param dec the decoder object
 *
 * \note
 * This function is not reentrant.
 *
 * @return the received of the held decoder device, NULL not to get one
 */
static inline vlc_decoder_device * decoder_GetDecoderDevice( decoder_t *dec )
{
    vlc_assert( dec->fmt_in->i_cat == VIDEO_ES && dec->cbs != NULL );
    if ( unlikely(dec->fmt_in->i_cat != VIDEO_ES || dec->cbs == NULL ) )
        return NULL;

    vlc_assert(dec->cbs->video.get_device != NULL);
    return dec->cbs->video.get_device( dec );
}

/**
 * Creates/Updates the rest of the video output pipeline.
 *
 * After a call to decoder_GetDecoderDevice() this function notifies the
 * video output pipeline of a new video output format (fmt_out.video). If there
 * was no video output from the decoder so far, a new decoder video output will
 * be set up. decoder_NewPicture() can then be used to allocate picture buffers.
 *
 * If the format is unchanged, this function has no effects and returns zero.
 *
 * \note
 * This function is not reentrant.
 *
 * @return 0 if the video output was set up successfully, -1 otherwise.
 */
VLC_API int decoder_UpdateVideoOutput( decoder_t *dec, vlc_video_context *vctx_out );

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
VLC_API int decoder_UpdateVideoFormat( decoder_t *dec );

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
VLC_API picture_t *decoder_NewPicture( decoder_t *dec );

/**
 * Initialize a decoder structure before creating the decoder.
 *
 * To be used by decoder owners.
 * By default frame drop is not allowed.
 *
 * @param dec the decoder to be initialized
 * @param fmt_in the es_format_t where the decoder owner stores the input ES format
 * @param fmt the input es_format_t used to initialize the decoder
 */
VLC_API void decoder_Init( decoder_t *dec, es_format_t *fmt_in, const es_format_t *fmt );

/**
 * Destroy a decoder and reset the structure.
 *
 * To be used by decoder owners.
 */
VLC_API void decoder_Destroy( decoder_t *p_dec );

/**
 * Unload a decoder module and reset the input/output formats.
 *
 * To be used by decoder owners.
 */
VLC_API void decoder_Clean( decoder_t *p_dec );

/**
 * This function queues a single picture to the video output.
 *
 * \note
 * The caller doesn't own the picture anymore after this call (even in case of
 * error).
 * FIXME: input_DecoderFrameNext won't work if a module use this function.
 */
static inline void decoder_QueueVideo( decoder_t *dec, picture_t *p_pic )
{
    vlc_assert( dec->fmt_in->i_cat == VIDEO_ES && dec->cbs != NULL );
    vlc_assert( !picture_HasChainedPics( p_pic ) );
    vlc_assert( dec->cbs->video.queue != NULL );
    dec->cbs->video.queue( dec, p_pic );
}

/**
 * This function queues the Closed Captions
 *
 * \param dec the decoder object
 * \param p_cc the closed-caption to queue
 * \param p_desc decoder_cc_desc_t description structure
 */
static inline void decoder_QueueCc( decoder_t *dec, vlc_frame_t *p_cc,
                                   const decoder_cc_desc_t *p_desc )
{
    vlc_assert( dec->fmt_in->i_cat == VIDEO_ES && dec->cbs != NULL );

    if( dec->cbs->video.queue_cc == NULL )
        block_Release( p_cc );
    else
        dec->cbs->video.queue_cc( dec, p_cc, p_desc );
}

/**
 * This function queues a single audio block to the audio output.
 *
 * \note
 * The caller doesn't own the audio block anymore after this call (even in case
 * of error).
 */
static inline void decoder_QueueAudio( decoder_t *dec, vlc_frame_t *p_aout_buf )
{
    vlc_assert( dec->fmt_in->i_cat == AUDIO_ES && dec->cbs != NULL );
    vlc_assert( p_aout_buf->p_next == NULL );
    vlc_assert( dec->cbs->audio.queue != NULL );
    dec->cbs->audio.queue( dec, p_aout_buf );
}

/**
 * This function queues a single subtitle to the video output.
 *
 * \note
 * The caller doesn't own the subtitle anymore after this call (even in case of
 * error).
 */
static inline void decoder_QueueSub( decoder_t *dec, subpicture_t *p_spu )
{
    vlc_assert( dec->fmt_in->i_cat == SPU_ES && dec->cbs != NULL );
    vlc_assert( p_spu->p_next == NULL );
    vlc_assert( dec->cbs->spu.queue != NULL );
    dec->cbs->spu.queue( dec, p_spu );
}

/**
 * This function notifies the audio output pipeline of a new audio output
 * format (fmt_out.audio). If there is currently no audio output or if the
 * audio output format has changed, a new audio output will be set up.
 * @return 0 if the audio output is working, -1 if not. */
VLC_USED
static inline int decoder_UpdateAudioFormat( decoder_t *dec )
{
    vlc_assert( dec->fmt_in->i_cat == AUDIO_ES && dec->cbs != NULL );

    if( dec->fmt_in->i_cat == AUDIO_ES && dec->cbs->audio.format_update != NULL )
        return dec->cbs->audio.format_update( dec );
    else
        return -1;
}

/**
 * This function will return a new audio buffer usable by a decoder as an
 * output buffer. It must be released with block_Release() or returned it to
 * the caller as a decoder_QueueAudio parameter.
 */
VLC_API vlc_frame_t * decoder_NewAudioBuffer( decoder_t *, int i_nb_samples ) VLC_USED;

/**
 * This function will return a new subpicture usable by a decoder as an output
 * buffer. You have to release it using subpicture_Delete() or by returning
 * it to the caller as a decoder_QueueSub parameter.
 */
VLC_USED
static inline subpicture_t *decoder_NewSubpicture( decoder_t *dec,
                                                   const subpicture_updater_t *p_dyn )
{
    vlc_assert( dec->fmt_in->i_cat == SPU_ES && dec->cbs != NULL );

    subpicture_t *p_subpicture = dec->cbs->spu.buffer_new( dec, p_dyn );
    if( !p_subpicture )
        msg_Warn( dec, "can't get output subpicture" );
    return p_subpicture;
}

/**
 * This function gives all input attachments at once.
 *
 * You MUST release the returned values
 */
static inline int decoder_GetInputAttachments( decoder_t *dec,
                                               input_attachment_t ***ppp_attachment,
                                               int *pi_attachment )
{
    vlc_assert( dec->cbs != NULL );

    if( !dec->cbs->get_attachments )
        return VLC_EGENERIC;

    return dec->cbs->get_attachments( dec, ppp_attachment, pi_attachment );
}

/**
 * This function converts a decoder timestamp into a display date comparable
 * to vlc_tick_now().
 * You MUST use it *only* for gathering statistics about speed.
 */
VLC_USED
static inline vlc_tick_t decoder_GetDisplayDate( decoder_t *dec,
                                                 vlc_tick_t system_now,
                                                 vlc_tick_t i_ts )
{
    vlc_assert( dec->fmt_in->i_cat == VIDEO_ES && dec->cbs != NULL );

    if( !dec->cbs->video.get_display_date )
        return VLC_TICK_INVALID;

    return dec->cbs->video.get_display_date( dec, system_now, i_ts );
}

/**
 * This function returns the current input rate.
 * You MUST use it *only* for gathering statistics about speed.
 */
VLC_USED
static inline float decoder_GetDisplayRate( decoder_t *dec )
{
    vlc_assert( dec->fmt_in->i_cat == VIDEO_ES && dec->cbs != NULL );

    if( !dec->cbs->video.get_display_rate )
        return 1.f;

    return dec->cbs->video.get_display_rate( dec );
}

/** @} */

/**
 * \defgroup decoder_device Decoder hardware device
 * \ingroup input
 * @{
 */

/** Decoder device type */
enum vlc_decoder_device_type
{
    VLC_DECODER_DEVICE_VAAPI,
    VLC_DECODER_DEVICE_VDPAU,
    VLC_DECODER_DEVICE_DXVA2,
    VLC_DECODER_DEVICE_D3D11VA,
    VLC_DECODER_DEVICE_VIDEOTOOLBOX,
    VLC_DECODER_DEVICE_AWINDOW,
    VLC_DECODER_DEVICE_NVDEC,
    VLC_DECODER_DEVICE_MMAL,
    VLC_DECODER_DEVICE_GSTDECODE,
};

struct vlc_decoder_device_operations
{
    void (*close)(struct vlc_decoder_device *);
};

/**
 * Decoder context struct
 */
typedef struct vlc_decoder_device
{
    struct vlc_object_t obj;

    const struct vlc_decoder_device_operations *ops;

    /** Private context that could be used by the "decoder device" module
     * implementation */
    void *sys;

    /** Must be set from the "decoder device" module open entry point */
    enum vlc_decoder_device_type type;

    /**
     * Could be set from the "decoder device" module open entry point and will
     * be used by hardware decoder modules.
     *
     * The type of pointer will depend of the type:
     * VAAPI: VADisplay
     * VDPAU: vdp_t *
     * DXVA2: d3d9_decoder_device_t*
     * D3D11VA: d3d11_decoder_device_t*
     * VIDEOTOOLBOX: NULL
     * AWindow: android AWindowHandler*
     * NVDEC: decoder_device_nvdec_t*
     * MMAL: MMAL_PORT_T*
     */
    void *opaque;
} vlc_decoder_device;

/**
 * "decoder device" module open entry point
 *
 * @param device the "decoder device" structure to initialize
 * @param window pointer to a window to help device initialization (can be NULL)
 **/
typedef int (*vlc_decoder_device_Open)(vlc_decoder_device *device,
                                        vlc_window_t *window);

#define set_callback_dec_device(activate, priority) \
    { \
        vlc_decoder_device_Open open__ = activate; \
        (void) open__; \
        set_callback(activate) \
    } \
    set_capability( "decoder device", priority )


/**
 * Create a decoder device from a window
 *
 * This function will be hidden in the future. It is now used by opengl vout
 * module as a transition.
 */
VLC_API vlc_decoder_device *
vlc_decoder_device_Create(vlc_object_t *, vlc_window_t *window) VLC_USED;

/**
 * Hold a decoder device
 */
VLC_API vlc_decoder_device *
vlc_decoder_device_Hold(vlc_decoder_device *device);

/**
 * Release a decoder device
 */
VLC_API void
vlc_decoder_device_Release(vlc_decoder_device *device);

/** @} */
#endif /* _VLC_CODEC_H */
