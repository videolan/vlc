/*****************************************************************************
 * vlc_codec.h: codec related structures
 *****************************************************************************
 * Copyright (C) 1999-2003 VideoLAN
 * $Id: vlc_codec.h,v 1.2 2003/10/27 01:04:38 gbazin Exp $
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

/**
 * \file
 * This file defines the structure and types used by decoders and encoders
 */

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
    int                 ( * pf_init )  ( decoder_t * );
    int                 ( * pf_decode )( decoder_t *, block_t * );
    int                 ( * pf_end )   ( decoder_t * );

    /* Input properties */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */

    /* Tmp field for old decoder api */
    int                 ( * pf_run ) ( decoder_fifo_t * );
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
    union {
        audio_sample_format_t audio;
        video_frame_format_t  video;
    } format;

    /* Properties of the output of the encoder */
    vlc_fourcc_t i_fourcc;
    int          i_bitrate;

    int          i_extra_data;
    uint8_t      *p_extra_data;

    /* FIXME: move these to the ffmpeg encoder */
    int i_frame_rate;
    int i_frame_rate_base;
    int i_key_int;
    int i_b_frames;
    int i_vtolerance;
    int i_qmin;
    int i_qmax;
    int i_hq;

};

/**
 * @}
 */

#endif /* _VLC_CODEC_H */
