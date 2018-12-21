/*****************************************************************************
 * extradata.c: Muxing extradata builder/gatherer
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VLC authors and VideoLAN
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
#include <vlc_codec.h>

#include "extradata.h"
#include "../packetizer/av1_obu.h"
#include "../packetizer/a52.h"

struct mux_extradata_builder_cb
{
    int  (*pf_init)(mux_extradata_builder_t *);
    void (*pf_feed)(mux_extradata_builder_t *, const uint8_t *, size_t);
    void (*pf_deinit)(mux_extradata_builder_t *);
};

struct mux_extradata_builder_t
{
    struct mux_extradata_builder_cb cb;
    void *priv;
    uint8_t *p_extra;
    size_t i_extra;
    vlc_fourcc_t fcc;
};

static void ac3_extradata_builder_Feed(mux_extradata_builder_t *m,
                                       const uint8_t *p_data, size_t i_data)
{
    if(m->i_extra || i_data < VLC_A52_MIN_HEADER_SIZE ||
       p_data[0] != 0x0B || p_data[1] != 0x77)
        return;

    struct vlc_a52_bitstream_info bsi;
    if(vlc_a52_ParseAc3BitstreamInfo(&bsi, &p_data[4], /* start code + CRC */
                                     VLC_A52_MIN_HEADER_SIZE - 4 ) != VLC_SUCCESS)
        return;

    m->p_extra = malloc(3);
    if(!m->p_extra)
        return;
    m->i_extra = 3;

    bs_t s;
    bs_write_init(&s, m->p_extra, m->i_extra);
    bs_write(&s, 2, bsi.i_fscod);
    bs_write(&s, 5, bsi.i_bsid);
    bs_write(&s, 3, bsi.i_bsmod);
    bs_write(&s, 3, bsi.i_acmod);
    bs_write(&s, 1, bsi.i_lfeon);
    bs_write(&s, 5, bsi.i_frmsizcod >> 1); // bit_rate_code
    bs_write(&s, 5, 0); // reserved
}

const struct mux_extradata_builder_cb ac3_cb =
{
    NULL,
    ac3_extradata_builder_Feed,
    NULL,
};

static void av1_extradata_builder_Feed(mux_extradata_builder_t *m,
                                       const uint8_t *p_data, size_t i_data)
{
    if(m->i_extra)
        return;

    AV1_OBU_iterator_ctx_t ctx;
    AV1_OBU_iterator_init(&ctx, p_data, i_data);
    const uint8_t *p_obu; size_t i_obu;
    while(AV1_OBU_iterate_next(&ctx, &p_obu, &i_obu))
    {
        enum av1_obu_type_e OBUtype = AV1_OBUGetType(p_obu);
        if(OBUtype != AV1_OBU_SEQUENCE_HEADER)
            continue;
        av1_OBU_sequence_header_t *p_sh = AV1_OBU_parse_sequence_header(p_obu, i_obu);
        if(p_sh)
        {
            m->i_extra = AV1_create_DecoderConfigurationRecord(&m->p_extra, p_sh,
                                                               1, (const uint8_t **)&p_obu, &i_obu);
            AV1_release_sequence_header(p_sh);
        }
        break;
    }
}

const struct mux_extradata_builder_cb av1_cb =
{
    NULL,
    av1_extradata_builder_Feed,
    NULL,
};

void mux_extradata_builder_Delete(mux_extradata_builder_t *m)
{
    if(m->cb.pf_deinit)
        m->cb.pf_deinit(m);
    free(m->p_extra);
    free(m);
}

mux_extradata_builder_t * mux_extradata_builder_New(vlc_fourcc_t fcc)
{
    const struct mux_extradata_builder_cb *cb;
    switch(fcc)
    {
        case VLC_CODEC_AV1:
            cb = &av1_cb;
            break;
        case VLC_CODEC_A52:
            cb = &ac3_cb;
            break;
        default:
            return NULL;
    }

    mux_extradata_builder_t *m = calloc(1, sizeof(*m));
    if(m)
    {
        m->fcc = fcc;
        m->cb = *cb;
        if(m->cb.pf_init && m->cb.pf_init(m) != 0)
        {
            free(m);
            m = NULL;
        }
    }
    return m;
}

size_t mux_extradata_builder_Get(mux_extradata_builder_t *m, const uint8_t **a)
{
    *a = m->p_extra;
    return m->i_extra;
}

void mux_extradata_builder_Feed(mux_extradata_builder_t *m,
                                const uint8_t *p_data, size_t i_data)
{
    m->cb.pf_feed(m, p_data, i_data);
}
