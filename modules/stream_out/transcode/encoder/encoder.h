/*****************************************************************************
 * encoder.h: transcoding encoders header
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

#define ENC_FRAMERATE (25 * 1000)
#define ENC_FRAMERATE_BASE 1000
#define FIRSTVALID(a,b,c) ( a ? a : ( b ? b : c ) )

typedef struct transcode_encoder_t transcode_encoder_t;

typedef struct
{
    vlc_fourcc_t i_codec; /* (0 if not transcode) */
    char         *psz_name;
    char         *psz_lang;
    config_chain_t *p_config_chain;
    union
    {
        struct
        {
            unsigned int    i_bitrate;
            float           f_scale;
            unsigned int    i_width, i_maxwidth;
            unsigned int    i_height, i_maxheight;
            bool            b_hurry_up;
            vlc_rational_t  fps;
            struct
            {
                unsigned int i_count;
                int          i_priority;
                uint32_t     pool_size;
            } threads;
        } video;
        struct
        {
            unsigned int    i_bitrate;
            uint32_t        i_sample_rate;
            uint32_t        i_channels;
        } audio;
        struct
        {
            unsigned int    i_width; /* render width */
            unsigned int    i_height;
        } spu;
    };
} transcode_encoder_config_t;

void transcode_encoder_config_init( transcode_encoder_config_t * );
void transcode_encoder_config_clean( transcode_encoder_config_t * );

const es_format_t *transcode_encoder_format_in( const transcode_encoder_t * );
const es_format_t *transcode_encoder_format_out( const transcode_encoder_t * );
void transcode_encoder_update_format_in( transcode_encoder_t *, const es_format_t * );
void transcode_encoder_update_format_out( transcode_encoder_t *, const es_format_t * );

block_t * transcode_encoder_encode( transcode_encoder_t *, void * );
block_t * transcode_encoder_get_output_async( transcode_encoder_t * );
void transcode_encoder_delete( transcode_encoder_t * );
transcode_encoder_t * transcode_encoder_new( encoder_t *, const es_format_t * );
void transcode_encoder_close( transcode_encoder_t * );

bool transcode_encoder_opened( const transcode_encoder_t * );
int transcode_encoder_open( transcode_encoder_t *, const transcode_encoder_config_t * );
int transcode_encoder_drain( transcode_encoder_t *, block_t ** );

int transcode_encoder_test( encoder_t *p_encoder,
                            const transcode_encoder_config_t *p_cfg,
                            const es_format_t *p_dec_fmtin,
                            vlc_fourcc_t i_codec_in,
                            es_format_t *p_enc_wanted_in );

void transcode_encoder_video_configure( vlc_object_t *p_obj,
                                        const video_format_t *p_dec_out,
                                        const transcode_encoder_config_t *p_cfg,
                                        const video_format_t *p_src,
                                        vlc_video_context *vctx_in,
                                        transcode_encoder_t *p_enc );

void transcode_video_framerate_apply( const video_format_t *p_src,
                                            video_format_t *p_dst );
void transcode_video_sar_apply( const video_format_t *p_src,
                                      video_format_t *p_dst );

int transcode_encoder_audio_configure( const transcode_encoder_config_t *p_cfg,
                                       const audio_format_t *p_dec_out,
                                       transcode_encoder_t *p_enc, bool );

