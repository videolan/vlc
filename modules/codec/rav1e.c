/*****************************************************************************
 * rav1e.c : rav1e encoder (AV1) module
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * Authors: Kartik Ohri <kartikohri13@gmail.com>
 *          Thomas Guillem <thomas@gllm.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <rav1e/rav1e.h>

#define SOUT_CFG_PREFIX "sout-rav1e-"

typedef struct
{
    struct RaContext *ra_context;
    date_t date;
    bool date_set;
} encoder_sys_t;

static block_t *Encode(encoder_t *enc, picture_t *p_pict)
{
    encoder_sys_t *sys = enc->p_sys;
    RaContext *ctx = sys->ra_context;
    block_t *p_out = NULL;

    RaFrame *frame;
    if (p_pict != NULL)
    {
        if (!sys->date_set && p_pict->date != VLC_TICK_INVALID)
        {
            date_Set(&sys->date, p_pict->date);
            sys->date_set = true;
        }

        frame = rav1e_frame_new(ctx);
        if (frame == NULL) {
            msg_Err(enc, "Unable to create new frame\n");
            goto error;
        }

        for (int idx = 0; idx < p_pict->i_planes; idx++)
            rav1e_frame_fill_plane(frame, idx,
                                   p_pict->p[idx].p_pixels,
                                   p_pict->p[idx].i_pitch * p_pict->p[idx].i_visible_lines,
                                   p_pict->p[idx].i_pitch,
                                   p_pict->p[idx].i_pixel_pitch);
    }
    else
        frame = NULL; /* Drain with a NULL frame */

    int ret = rav1e_send_frame(ctx, frame);
    rav1e_frame_unref(frame);
    if (ret != 0)
    {
        msg_Err(enc, "rav1e_send_frame failed: %d: %s", ret,
                rav1e_status_to_str(ret));
        goto error;
    }

    bool again;
    do
    {
        RaPacket *pkt = NULL;
        ret = rav1e_receive_packet(ctx, &pkt);

        switch (ret)
        {
            case RA_ENCODER_STATUS_SUCCESS:
            {
                block_t *p_block = block_Alloc(pkt->len);
                if (unlikely(p_block == NULL))
                {
                    block_ChainRelease(p_out);
                    p_out = NULL;
                    break;
                }

                memcpy(p_block->p_buffer, pkt->data, pkt->len);
                p_block->i_dts = p_block->i_pts = date_Get(&sys->date);

                if (pkt->frame_type == RA_FRAME_TYPE_KEY)
                    p_block->i_flags |= BLOCK_FLAG_TYPE_I;
                block_ChainAppend(&p_out, p_block);
                rav1e_packet_unref(pkt);
            }
            /* fall-through */
            case RA_ENCODER_STATUS_ENCODED:
                again = true;
                break;
            case RA_ENCODER_STATUS_LIMIT_REACHED:
            case RA_ENCODER_STATUS_NEED_MORE_DATA:
                again = false;
                break;
            default:
                msg_Err(enc, "rav1e_receive_packet() failed: %d: %s", ret,
                        rav1e_status_to_str(ret));
                goto error;
        }
    } while (again);

    return p_out;

error:
    free(p_out);
    return NULL;
}

static int OpenEncoder(vlc_object_t *this)
{
    encoder_t *enc = (encoder_t *) this;
    encoder_sys_t *sys;

    if (enc->fmt_out.i_codec != VLC_CODEC_AV1)
        return VLC_EGENERIC;

    static const char *const ppsz_rav1e_options[] = {
        "bitdepth", "tile-rows", "tile-columns", NULL
    };

    config_ChainParse(enc, SOUT_CFG_PREFIX, ppsz_rav1e_options, enc->p_cfg);

    sys = malloc(sizeof(*sys));
    if (sys == NULL)
        return VLC_ENOMEM;

    enc->p_sys = sys;

    struct RaConfig *ra_config = rav1e_config_default();
    if (ra_config == NULL)
    {
        msg_Err(enc, "Unable to initialize configuration\n");
        free(sys);
        return VLC_EGENERIC;
    }

    int ret;

    ret = rav1e_config_parse_int(ra_config, "height", enc->fmt_in.video.i_visible_height);
    if (ret < 0)
    {
        msg_Err(enc, "Unable to set height\n");
        goto error;
    }

    ret = rav1e_config_parse_int(ra_config, "width", enc->fmt_in.video.i_visible_width);
    if (ret < 0) {
        msg_Err(enc, "Unable to set width\n");
        goto error;
    }

    RaRational *timebase = malloc(sizeof(RaRational));
    if (timebase == NULL)
    {
        msg_Err(enc, "%s", "Unable to set width\n");
        goto error;
    }

    timebase->num = enc->fmt_in.video.i_frame_rate_base;
    timebase->den = enc->fmt_in.video.i_frame_rate;
    rav1e_config_set_time_base(ra_config, *timebase);

    int tile_rows = var_InheritInteger(enc, SOUT_CFG_PREFIX "tile-rows");
    int tile_columns = var_InheritInteger(enc, SOUT_CFG_PREFIX "tile-columns");
    tile_rows = 1 << tile_rows;
    tile_columns = 1 << tile_columns;

    ret = rav1e_config_parse_int(ra_config, "tile_rows", tile_rows);
    if (ret < 0)
    {
        msg_Err(enc, "Unable to set tile rows\n");
        goto error;
    }

    ret = rav1e_config_parse_int(ra_config, "tile_cols", tile_columns);
    if (ret < 0)
    {
        msg_Err(enc, "Unable to set tile columns\n");
        goto error;
    }

    int bitdepth = var_InheritInteger(enc, SOUT_CFG_PREFIX "bitdepth");
    int profile = var_InheritInteger(enc, SOUT_CFG_PREFIX "profile");

    RaChromaSampling chroma_sampling;
    switch (profile)
    {
        case 2:
            chroma_sampling = RA_CHROMA_SAMPLING_CS422;
            enc->fmt_in.i_codec = bitdepth == 8 ? VLC_CODEC_I422 : VLC_CODEC_I422_10L;
            break;
        case 1:
            chroma_sampling = RA_CHROMA_SAMPLING_CS444;
            enc->fmt_in.i_codec = bitdepth == 8 ? VLC_CODEC_I444 : VLC_CODEC_I444_10L;
            break;
        default:
        case 0:
            chroma_sampling = RA_CHROMA_SAMPLING_CS420;
            enc->fmt_in.i_codec = bitdepth == 8 ? VLC_CODEC_I420 : VLC_CODEC_I420_10L;
            break;
    }

    RaChromaSamplePosition sample_pos;
    switch (enc->fmt_in.video.chroma_location)
    {
        case CHROMA_LOCATION_LEFT:
            sample_pos = RA_CHROMA_SAMPLE_POSITION_VERTICAL;
            break;
        case CHROMA_LOCATION_TOP_LEFT:
            sample_pos = RA_CHROMA_SAMPLE_POSITION_COLOCATED;
            break;
        default:
            sample_pos = RA_CHROMA_SAMPLE_POSITION_UNKNOWN;
            break;
    }

    RaPixelRange pixel_range;
    switch (enc->fmt_in.video.color_range)
    {
        case COLOR_RANGE_FULL:
            pixel_range = RA_PIXEL_RANGE_FULL;
            break;
        case COLOR_RANGE_LIMITED:
        default:
            pixel_range = RA_PIXEL_RANGE_LIMITED;
            break;
    }
    ret = rav1e_config_set_pixel_format(ra_config, bitdepth, chroma_sampling,
                                        sample_pos, pixel_range);
    if (ret < 0)
    {
        msg_Err(enc, "Unable to set pixel format\n");
        goto error;
    }


    sys->ra_context = rav1e_context_new(ra_config);
    if (!sys->ra_context)
    {
        msg_Err(enc, "Unable to allocate a new context\n");
        goto error;
    }
    rav1e_config_unref(ra_config);

    date_Init(&sys->date, enc->fmt_out.video.i_frame_rate,
              enc->fmt_out.video.i_frame_rate_base);
    sys->date_set = false;

    enc->pf_encode_video = Encode;
    return VLC_SUCCESS;

error:
    rav1e_config_unref(ra_config);
    free(sys);
    return VLC_EGENERIC;
}

static void CloseEncoder(vlc_object_t* this)
{
    encoder_t *enc = (encoder_t *) this;
    encoder_sys_t *sys = enc->p_sys;
    rav1e_context_unref(sys->ra_context);
    free(sys);
}

static const int bitdepth_values_list[] = {8, 10};
static const char *bitdepth_values_name_list[] = {N_("8 bpp"), N_("10 bpp")};

vlc_module_begin()
    set_shortname("rav1e")
    set_description(N_("rav1e video encoder"))
    set_capability("encoder", 101)
    set_callbacks(OpenEncoder, CloseEncoder)
    add_integer(SOUT_CFG_PREFIX "profile", 0, "Profile", NULL, true)
        change_integer_range(0, 3)
    add_integer(SOUT_CFG_PREFIX "bitdepth", 8, "Bit Depth", NULL, true)
        change_integer_list(bitdepth_values_list, bitdepth_values_name_list)
    add_integer(SOUT_CFG_PREFIX "tile-rows", 0, "Tile Rows (in log2 units)", NULL, true)
        change_integer_range(0, 6)
    add_integer(SOUT_CFG_PREFIX "tile-columns", 0, "Tile Columns (in log2 units)", NULL, true)
        change_integer_range(0, 6)
vlc_module_end()
