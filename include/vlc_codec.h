/*****************************************************************************
 * vlc_codec.h: codec related structures
 *****************************************************************************
 * Copyright (C) 1999-2003 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#ifndef _VLC_CODEC_H
#define _VLC_CODEC_H 1

#include "ninput.h"

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

struct decoder_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t *          p_module;
    decoder_sys_t *     p_sys;

    picture_t *         ( * pf_decode_video )( decoder_t *, block_t ** );
    aout_buffer_t *     ( * pf_decode_audio )( decoder_t *, block_t ** );
    void                ( * pf_decode_sub)   ( decoder_t *, block_t ** );
    block_t *           ( * pf_packetize )   ( decoder_t *, block_t ** );

    /* Some decoders only accept packetized data (ie. not truncated) */
    vlc_bool_t          b_need_packetized;

    /* Input format ie from demuxer (XXX: a lot of field could be invalid) */
    es_format_t         fmt_in;

    /* Output format of decoder/packetizer */
    es_format_t         fmt_out;

    /*
     * Buffers allocation
     */

    /* Audio output callbacks */
    aout_buffer_t * ( * pf_aout_buffer_new) ( decoder_t *, int );
    void            ( * pf_aout_buffer_del) ( decoder_t *, aout_buffer_t * );

    /* Video output callbacks */
    picture_t     * ( * pf_vout_buffer_new) ( decoder_t * );
    void            ( * pf_vout_buffer_del) ( decoder_t *, picture_t * );
    void            ( * pf_picture_link)    ( decoder_t *, picture_t * );
    void            ( * pf_picture_unlink)  ( decoder_t *, picture_t * );


    /* Private structure for the owner of the decoder */
    decoder_owner_sys_t *p_owner;
};

/**
 * @}
 */

/**
 * \defgroup decoder Encoder
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

    block_t *           ( * pf_header )( encoder_t * );
    block_t *           ( * pf_encode_video )( encoder_t *, picture_t * );
    block_t *           ( * pf_encode_audio )( encoder_t *, aout_buffer_t * );

    /* Properties of the input data fed to the encoder */
    es_format_t         fmt_in;

    /* Properties of the output of the encoder */
    es_format_t         fmt_out;

    /* Number of threads to use during encoding */
    int                 i_threads;

    /* Encoder config */
    sout_cfg_t *p_cfg;
};

/**
 * @}
 */

#endif /* _VLC_CODEC_H */
