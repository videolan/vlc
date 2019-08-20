/*****************************************************************************
 * encoder_priv.h: transcoding encoders interface
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VideoLAN and VLC authors
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
 * along with this program; if not, If not, see https://www.gnu.org/licenses/
 *****************************************************************************/
#include <vlc_picture_fifo.h>

struct transcode_encoder_t
{
    encoder_t       *p_encoder;
    vlc_thread_t    thread;
    vlc_mutex_t     lock_out;
    bool            b_abort;
    picture_fifo_t *pp_pics;
    vlc_sem_t       picture_pool_has_room;
    vlc_cond_t      cond;

    /* output buffers */
    block_t         *p_buffers;
    bool b_threaded;
};

int transcode_encoder_audio_open( transcode_encoder_t *p_enc,
                                  const transcode_encoder_config_t *p_cfg );
int transcode_encoder_video_open( transcode_encoder_t *p_enc,
                                   const transcode_encoder_config_t *p_cfg );
int transcode_encoder_spu_open( transcode_encoder_t *p_enc,
                                const transcode_encoder_config_t *p_cfg );

void transcode_encoder_video_close( transcode_encoder_t *p_enc );

block_t * transcode_encoder_video_encode( transcode_encoder_t *p_enc, picture_t *p_pic );
block_t * transcode_encoder_audio_encode( transcode_encoder_t *p_enc, block_t *p_block );
block_t * transcode_encoder_spu_encode( transcode_encoder_t *p_enc, subpicture_t *p_spu );

int transcode_encoder_audio_drain( transcode_encoder_t *p_enc, block_t **out );
int transcode_encoder_video_drain( transcode_encoder_t *p_enc, block_t **out );

int transcode_encoder_video_test( encoder_t *p_encoder,
                                  const transcode_encoder_config_t *p_cfg,
                                  const es_format_t *p_dec_fmtin,
                                  vlc_fourcc_t i_codec_in,
                                  es_format_t *p_enc_wanted_in );

int transcode_encoder_audio_test( encoder_t *p_encoder,
                                  const transcode_encoder_config_t *p_cfg,
                                  const es_format_t *p_dec_out,
                                  vlc_fourcc_t i_codec_in,
                                  es_format_t *p_enc_wanted_in );
