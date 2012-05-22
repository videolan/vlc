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

#include <vlc_block.h>
#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_subpicture.h>

/**
 * \file
 * This file defines the structure and types used by decoders and encoders
 */

typedef struct decoder_owner_sys_t decoder_owner_sys_t;

/**
 * \defgroup decoder Decoder
 *
 * The structure describing a decoder
 *
 * @{
 */

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

    /* Some decoders only accept packetized data (ie. not truncated) */
    bool                b_need_packetized;

    /* Tell the decoder if it is allowed to drop frames */
    bool                b_pace_control;

    /* */
    picture_t *         ( * pf_decode_video )( decoder_t *, block_t ** );
    block_t *           ( * pf_decode_audio )( decoder_t *, block_t ** );
    subpicture_t *      ( * pf_decode_sub)   ( decoder_t *, block_t ** );
    block_t *           ( * pf_packetize )   ( decoder_t *, block_t ** );

    /* Closed Caption (CEA 608/708) extraction.
     * If set, it *may* be called after pf_decode_video/pf_packetize
     * returned data. It should return CC for the pictures returned by the
     * last pf_packetize/pf_decode_video call only,
     * pb_present will be used to known which cc channel are present (but
     * globaly, not necessary for the current packet */
    block_t *           ( * pf_get_cc )      ( decoder_t *, bool pb_present[4] );

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
     * XXX use decoder_NewPicture/decoder_DeletePicture
     * and decoder_LinkPicture/decoder_UnlinkPicture */
    picture_t      *(*pf_vout_buffer_new)( decoder_t * );
    void            (*pf_vout_buffer_del)( decoder_t *, picture_t * );
    void            (*pf_picture_link)   ( decoder_t *, picture_t * );
    void            (*pf_picture_unlink) ( decoder_t *, picture_t * );

    /**
     * Number of extra (ie in addition to the DPB) picture buffers
     * needed for decoding.
     */
    int             i_extra_picture_buffers;

    /* Audio output callbacks
     * XXX use decoder_NewAudioBuffer/decoder_DeleteAudioBuffer */
    block_t        *(*pf_aout_buffer_new)( decoder_t *, int );

    /* SPU output callbacks
     * XXX use decoder_NewSubpicture and decoder_DeleteSubpicture */
    subpicture_t   *(*pf_spu_buffer_new)( decoder_t *, const subpicture_updater_t * );
    void            (*pf_spu_buffer_del)( decoder_t *, subpicture_t * );

    /* Input attachments
     * XXX use decoder_GetInputAttachments */
    int             (*pf_get_attachments)( decoder_t *p_dec, input_attachment_t ***ppp_attachment, int *pi_attachment );

    /* Display date
     * XXX use decoder_GetDisplayDate */
    mtime_t         (*pf_get_display_date)( decoder_t *, mtime_t );

    /* Display rate
     * XXX use decoder_GetDisplayRate */
    int             (*pf_get_display_rate)( decoder_t * );

    /* Private structure for the owner of the decoder */
    decoder_owner_sys_t *p_owner;

    bool                b_error;
};

/**
 * @}
 */

/**
 * \defgroup encoder Encoder
 *
 * The structure describing a Encoder
 *
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
 */


/**
 * This function will return a new picture usable by a decoder as an output
 * buffer. You have to release it using decoder_DeletePicture or by returning
 * it to the caller as a pf_decode_video return value.
 */
VLC_API picture_t * decoder_NewPicture( decoder_t * ) VLC_USED;

/**
 * This function will release a picture create by decoder_NewPicture.
 */
VLC_API void decoder_DeletePicture( decoder_t *, picture_t *p_picture );

/**
 * This function will increase the picture reference count.
 * (picture_Hold is not usable.)
 */
VLC_API void decoder_LinkPicture( decoder_t *, picture_t * );

/**
 * This function will decrease the picture reference count.
 * (picture_Release is not usable.)
 */
VLC_API void decoder_UnlinkPicture( decoder_t *, picture_t * );

/**
 * This function will return a new audio buffer usable by a decoder as an
 * output buffer. You have to release it using decoder_DeleteAudioBuffer
 * or by returning it to the caller as a pf_decode_audio return value.
 */
VLC_API block_t * decoder_NewAudioBuffer( decoder_t *, int i_size ) VLC_USED;

/**
 * This function will return a new subpicture usable by a decoder as an output
 * buffer. You have to release it using decoder_DeleteSubpicture or by returning
 * it to the caller as a pf_decode_sub return value.
 */
VLC_API subpicture_t * decoder_NewSubpicture( decoder_t *, const subpicture_updater_t * ) VLC_USED;

/**
 * This function will release a subpicture created by decoder_NewSubicture.
 */
VLC_API void decoder_DeleteSubpicture( decoder_t *, subpicture_t *p_subpicture );

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
VLC_API mtime_t decoder_GetDisplayDate( decoder_t *, mtime_t ) VLC_USED;

/**
 * This function returns the current input rate.
 * You MUST use it *only* for gathering statistics about speed.
 */
VLC_API int decoder_GetDisplayRate( decoder_t * ) VLC_USED;

#endif /* _VLC_CODEC_H */
