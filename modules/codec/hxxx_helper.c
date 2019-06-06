/*****************************************************************************
 * hxxx_helper.c: AnnexB / avcC helper for dumb decoders
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#include <stdint.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_bits.h>

#include "hxxx_helper.h"
#include "../packetizer/hxxx_nal.h"
#include "../packetizer/h264_slice.h"

void
hxxx_helper_init(struct hxxx_helper *hh, vlc_object_t *p_obj,
                 vlc_fourcc_t i_codec, bool b_need_xvcC)
{
    assert(i_codec == VLC_CODEC_H264 || i_codec == VLC_CODEC_HEVC);

    memset(hh, 0, sizeof(struct hxxx_helper));
    hh->p_obj = p_obj;
    hh->i_codec = i_codec;
    hh->b_need_xvcC = b_need_xvcC;
}

#define RELEASE_NALS(list, max, release) \
    for (size_t i = 0; i <= max; ++i) \
    { \
        hnal = &list[i]; \
        if (hnal->b) \
        { \
            block_Release(hnal->b); \
            release; \
        } \
    }

static void
helper_clear_sei(struct hxxx_helper *hh)
{
    if (hh->i_codec != VLC_CODEC_HEVC)
        return;

    for (uint8_t i=0; i<hh->hevc.i_sei_count; i++)
    {
        if(hh->hevc.sei_list[i].b)
        {
            block_Release(hh->hevc.sei_list[i].b);
            hh->hevc.sei_list[i].b = NULL;
        }
    }
    hh->hevc.i_sei_count = 0;
}

void
hxxx_helper_clean(struct hxxx_helper *hh)
{
    struct hxxx_helper_nal *hnal;
    switch (hh->i_codec)
    {
        case VLC_CODEC_H264:
            RELEASE_NALS(hh->h264.sps_list, H264_SPS_ID_MAX,
                         h264_release_sps(hnal->h264_sps));
            RELEASE_NALS(hh->h264.pps_list, H264_PPS_ID_MAX,
                         h264_release_pps(hnal->h264_pps));
            memset(&hh->h264, 0, sizeof(hh->h264));
            break;
        case VLC_CODEC_HEVC:
            RELEASE_NALS(hh->hevc.vps_list, HEVC_VPS_ID_MAX,
                         hevc_rbsp_release_vps(hnal->hevc_vps));
            RELEASE_NALS(hh->hevc.sps_list, HEVC_SPS_ID_MAX,
                         hevc_rbsp_release_sps(hnal->hevc_sps));
            RELEASE_NALS(hh->hevc.pps_list, HEVC_PPS_ID_MAX,
                         hevc_rbsp_release_pps(hnal->hevc_pps));
            helper_clear_sei(hh);
            memset(&hh->hevc, 0, sizeof(hh->hevc));
            break;
        default:
            vlc_assert_unreachable();
    }
}

#define HELPER_FOREACH_NAL(it, p_nal_list, i_nal_count, i_nal_max) \
    for (size_t ii = 0, i_nal_found = 0; ii < i_nal_max && i_nal_count > i_nal_found; ++ii) \
        if (p_nal_list[ii].b != NULL && (it = &p_nal_list[ii]) && ++i_nal_found)

static int
helper_dup_buf(struct hxxx_helper_nal *p_nal,
               const uint8_t *p_nal_buf, size_t i_nal_buf)
{
    if (!p_nal->b)
    {
        p_nal->b = block_Alloc(i_nal_buf);
        if (!p_nal->b)
            return VLC_ENOMEM;
    }
    else if (p_nal->b != NULL && i_nal_buf > p_nal->b->i_buffer)
    {
        block_t *b = block_TryRealloc(p_nal->b, 0, i_nal_buf);
        if (b == NULL)
            return VLC_ENOMEM;
        p_nal->b = b;
    }
    memcpy(p_nal->b->p_buffer, p_nal_buf, i_nal_buf);
    p_nal->b->i_buffer = i_nal_buf;
    return VLC_SUCCESS;
}

static inline const struct hxxx_helper_nal *
helper_search_nal(const struct hxxx_helper_nal *p_nal_list, size_t i_nal_count,
                  size_t i_nal_max, const void *p_nal_buf, size_t i_nal_buf)
{
    size_t i_nal_nb = 0;
    for (size_t i = 0; i < i_nal_max && i_nal_count > i_nal_nb; ++i)
    {
        const struct hxxx_helper_nal *p_nal = &p_nal_list[i];
        if (p_nal->b == NULL)
            continue;
        i_nal_nb++;
        const int i_diff = i_nal_buf - p_nal->b->i_buffer;
        if (i_diff == 0 && memcmp(p_nal_buf, p_nal->b->p_buffer, i_nal_buf) == 0)
            return p_nal;
    }
    return NULL;
}

static inline bool
helper_nal_length_valid(struct hxxx_helper *hh)
{
    return hh->i_nal_length_size == 1 || hh->i_nal_length_size == 2
        || hh->i_nal_length_size == 4;
}

static void
helper_load_sei(struct hxxx_helper *hh, const uint8_t *p_nal, size_t i_nal)
{
    if(hh->i_codec != VLC_CODEC_HEVC)
        return;

    if(hh->hevc.i_sei_count == HXXX_HELPER_SEI_COUNT)
        return;

    struct hxxx_helper_nal *hnal = &hh->hevc.sei_list[hh->hevc.i_sei_count];
    if (helper_dup_buf(hnal, p_nal, i_nal))
        return;
    hh->hevc.i_sei_count++;
}

#define LOAD_xPS(list, count, id, max, xpstype, xpsdecode, xpsrelease) \
    if (helper_search_nal(list, count, max+1, p_nal, i_nal) != NULL)\
        continue;\
    xpstype *p_xps = xpsdecode(p_nal, i_nal, true);\
    if (!p_xps)\
        return VLC_EGENERIC;\
\
    struct hxxx_helper_nal *hnal = &list[id];\
    if (helper_dup_buf(hnal, p_nal, i_nal))\
    {\
        xpsrelease(p_xps);\
        return VLC_EGENERIC;\
    }\
    if (hnal->xps)\
        xpsrelease(hnal->xps);\
    else\
        count++;\
\
    hnal->xps = p_xps;\
    *p_config_changed = true

static int
h264_helper_parse_nal(struct hxxx_helper *hh, const uint8_t *p_buf, size_t i_buf,
                      uint8_t i_nal_length_size, bool *p_config_changed)
{
    const uint8_t *p_nal;
    size_t i_nal;
    hxxx_iterator_ctx_t it;
    hxxx_iterator_init(&it, p_buf, i_buf, i_nal_length_size);
    *p_config_changed = false;

    while ((i_nal_length_size) ? hxxx_iterate_next(&it, &p_nal, &i_nal)
                               : hxxx_annexb_iterate_next(&it, &p_nal, &i_nal))
    {
        if (i_nal < 2)
            continue;

        const enum h264_nal_unit_type_e i_nal_type = p_nal[0] & 0x1F;

        if (i_nal_type == H264_NAL_SPS)
        {
            LOAD_xPS(hh->h264.sps_list, hh->h264.i_sps_count,
                     p_xps->i_id, H264_SPS_ID_MAX,
                     h264_sequence_parameter_set_t,
                     h264_decode_sps,
                     h264_release_sps);
            hh->h264.i_current_sps = ((h264_sequence_parameter_set_t*)p_xps)->i_id;
            msg_Dbg(hh->p_obj, "new SPS parsed: %u", hh->h264.i_current_sps);
        }
        else if (i_nal_type == H264_NAL_PPS)
        {
            LOAD_xPS(hh->h264.pps_list, hh->h264.i_pps_count,
                     p_xps->i_id, H264_PPS_ID_MAX,
                     h264_picture_parameter_set_t,
                     h264_decode_pps,
                     h264_release_pps);
            msg_Dbg(hh->p_obj, "new PPS parsed: %u", ((h264_picture_parameter_set_t*)p_xps)->i_id);
        }
        else if (i_nal_type <= H264_NAL_SLICE_IDR
              && i_nal_type != H264_NAL_UNKNOWN)
        {
            if (hh->h264.i_sps_count > 1)
            {
                /* There is more than one SPS. Get the PPS id of the current
                 * SLICE in order to get the current SPS id */

                /* Get the PPS id from the slice: inspirated from
                 * h264_decode_slice() */
                bs_t s;
                bs_init(&s, p_nal, i_nal);
                bs_skip(&s, 8);
                bs_read_ue(&s);
                bs_read_ue(&s);
                unsigned i_pps_id = bs_read_ue(&s);
                if (i_pps_id > H264_PPS_ID_MAX)
                    return VLC_EGENERIC;

                struct hxxx_helper_nal *hpps = &hh->h264.pps_list[i_pps_id];
                if (hpps->b == NULL)
                    return VLC_EGENERIC;

                struct hxxx_helper_nal *hsps =
                    &hh->h264.sps_list[hpps->h264_pps->i_sps_id];
                if (hsps->b == NULL)
                    return VLC_EGENERIC;

                assert(hpps->h264_pps->i_sps_id == hsps->h264_sps->i_id);
                if (hsps->h264_sps->i_id != hh->h264.i_current_sps)
                {
                    hh->h264.i_current_sps = hsps->h264_sps->i_id;
                    *p_config_changed = true;
                }
            }
            break; /* No need to parse further NAL */
        }
    }
    return VLC_SUCCESS;
}

static void
helper_check_sei_au(struct hxxx_helper *hh, uint8_t i_nal_type)
{
    if ((i_nal_type <= HEVC_NAL_IRAP_VCL23 &&
         hh->hevc.i_previous_nal_type != HEVC_NAL_PREF_SEI) ||
        (i_nal_type == HEVC_NAL_PREF_SEI &&
         hh->hevc.i_previous_nal_type != HEVC_NAL_PREF_SEI))
        helper_clear_sei(hh);
    hh->hevc.i_previous_nal_type = i_nal_type;
}

static int
hevc_helper_parse_nal(struct hxxx_helper *hh, const uint8_t *p_buf, size_t i_buf,
                      uint8_t i_nal_length_size, bool *p_config_changed)
{
    const uint8_t *p_nal;
    size_t i_nal;
    hxxx_iterator_ctx_t it;
    hxxx_iterator_init(&it, p_buf, i_buf, i_nal_length_size);
    *p_config_changed = false;

    while ((i_nal_length_size) ? hxxx_iterate_next(&it, &p_nal, &i_nal)
                               : hxxx_annexb_iterate_next(&it, &p_nal, &i_nal))
    {
        if (i_nal < 2 || hevc_getNALLayer(p_nal) > 0)
            continue;

        const uint8_t i_nal_type = hevc_getNALType(p_nal);

        /* we need to clear sei not belonging to this access unit */
        helper_check_sei_au(hh, i_nal_type);

        if (i_nal_type == HEVC_NAL_VPS)
        {
            uint8_t i_id;
            if( !hevc_get_xps_id(p_nal, i_nal, &i_id) )
                return VLC_EGENERIC;
            LOAD_xPS(hh->hevc.vps_list, hh->hevc.i_vps_count,
                     i_id, HEVC_VPS_ID_MAX,
                     hevc_video_parameter_set_t,
                     hevc_decode_vps,
                     hevc_rbsp_release_vps);
            msg_Dbg(hh->p_obj, "new VPS parsed: %u", i_id);
        }
        else if (i_nal_type == HEVC_NAL_SPS)
        {
            uint8_t i_id;
            if( !hevc_get_xps_id(p_nal, i_nal, &i_id) )
                return VLC_EGENERIC;
            LOAD_xPS(hh->hevc.sps_list, hh->hevc.i_sps_count,
                     i_id, HEVC_SPS_ID_MAX,
                     hevc_sequence_parameter_set_t,
                     hevc_decode_sps,
                     hevc_rbsp_release_sps);
            msg_Dbg(hh->p_obj, "new SPS parsed: %u", i_id);
        }
        else if (i_nal_type == HEVC_NAL_PPS)
        {
            uint8_t i_id;
            if( !hevc_get_xps_id(p_nal, i_nal, &i_id) )
                return VLC_EGENERIC;
            LOAD_xPS(hh->hevc.pps_list, hh->hevc.i_pps_count,
                     i_id, HEVC_PPS_ID_MAX,
                     hevc_picture_parameter_set_t,
                     hevc_decode_pps,
                     hevc_rbsp_release_pps);
            msg_Dbg(hh->p_obj, "new PPS parsed: %u", i_id);
        }
        else if (i_nal_type <= HEVC_NAL_IRAP_VCL23)
        {
            if (hh->hevc.i_sps_count > 1 || hh->hevc.i_vps_count > 1)
            {
                /* Get the PPS id from the slice: inspirated from
                 * h264_decode_slice() */
                bs_t s;
                bs_init(&s, p_nal, i_nal);
                bs_skip(&s, 2);
                unsigned i_id = bs_read_ue(&s);
                if (i_id > HEVC_PPS_ID_MAX)
                    return VLC_EGENERIC;

                struct hxxx_helper_nal *xps = &hh->hevc.pps_list[i_id];
                if (xps->b == NULL)
                    return VLC_EGENERIC;

                const uint8_t i_spsid = hevc_get_pps_sps_id(xps->hevc_pps);
                xps = &hh->hevc.sps_list[i_spsid];
                if (xps->b == NULL)
                    return VLC_EGENERIC;

                i_id = hevc_get_sps_vps_id(xps->hevc_sps);
                xps = &hh->hevc.vps_list[i_id];

                if (i_spsid != hh->hevc.i_current_sps ||
                    i_id != hh->hevc.i_current_vps)
                {
                    hh->hevc.i_current_sps = i_spsid;
                    hh->hevc.i_current_vps = i_id;
                    *p_config_changed = true;
                }
            }
            break; /* No need to parse further NAL */
        }
        else if(i_nal_type == HEVC_NAL_PREF_SEI||
                i_nal_type == HEVC_NAL_SUFF_SEI)
        {
            helper_load_sei(hh, p_nal, i_nal);
        }
    }
    return VLC_SUCCESS;
}

static int
helper_process_avcC_h264(struct hxxx_helper *hh, const uint8_t *p_buf,
                         size_t i_buf)
{
    if (i_buf < H264_MIN_AVCC_SIZE)
        return VLC_EGENERIC;

    p_buf += 5; i_buf -= 5;

    for (unsigned int j = 0; j < 2 && i_buf > 0; j++)
    {
        /* First time is SPS, Second is PPS */
        const unsigned int i_num_nal = p_buf[0] & (j == 0 ? 0x1f : 0xff);
        p_buf++; i_buf--;

        for (unsigned int i = 0; i < i_num_nal && i_buf >= 2; i++)
        {
            uint16_t i_nal_size = (p_buf[0] << 8) | p_buf[1];
            if (i_nal_size > i_buf - 2)
                return VLC_EGENERIC;
            bool b_unused;
            int i_ret = h264_helper_parse_nal(hh, p_buf, i_nal_size + 2, 2,
                                              &b_unused);
            if (i_ret != VLC_SUCCESS)
                return i_ret;
            p_buf += i_nal_size + 2;
            i_buf -= i_nal_size + 2;
        }
    }

    return VLC_SUCCESS;
}

static bool
hxxx_extra_isannexb(const void *p_extra, size_t i_extra)
{
    return i_extra == 0
        || (i_extra > 4 && memcmp(p_extra, annexb_startcode4, 4) == 0);
}

static int
h264_helper_set_extra(struct hxxx_helper *hh, const void *p_extra,
                      size_t i_extra)
{
    if (h264_isavcC(p_extra, i_extra))
    {
        hh->i_nal_length_size = (((uint8_t*)p_extra)[4] & 0x03) + 1;
        if (!helper_nal_length_valid(hh))
            return VLC_EGENERIC;
        hh->b_is_xvcC = true;

        /* XXX h264_AVC_to_AnnexB() works only with a i_nal_length_size of 4.
         * If nal_length_size is smaller than 4, fallback to SW decoding. I
         * don't know if it's worth the effort to fix h264_AVC_to_AnnexB() for
         * a smaller nal_length_size. Indeed, this case will happen only with
         * very small resolutions, where hardware decoders are not that useful.
         * -Thomas */
        if (!hh->b_need_xvcC && hh->i_nal_length_size != 4)
        {
            msg_Dbg(hh->p_obj, "nal_length_size is too small");
            return VLC_EGENERIC;
        }

        return helper_process_avcC_h264(hh, p_extra, i_extra);
    }
    else if (hxxx_extra_isannexb(p_extra, i_extra))
    {
        hh->i_nal_length_size = 4;
        bool unused;
        return i_extra == 0 ? VLC_SUCCESS :
               h264_helper_parse_nal(hh, p_extra, i_extra, 0, &unused);
    }
    else
        return VLC_EGENERIC;
}

static int
helper_process_hvcC_hevc(struct hxxx_helper *hh, const uint8_t *p_buf,
                         size_t i_buf)
{
    if (i_buf < HEVC_MIN_HVCC_SIZE)
        return VLC_EGENERIC;

    const uint8_t i_num_array = p_buf[22];
    p_buf += 23; i_buf -= 23;

    for( uint8_t i = 0; i < i_num_array; i++ )
    {
        if(i_buf < 3)
            return VLC_EGENERIC;

        const uint16_t i_num_nalu = p_buf[1] << 8 | p_buf[2];
        p_buf += 3; i_buf -= 3;

        for( uint16_t j = 0; j < i_num_nalu; j++ )
        {
            if(i_buf < 2)
                return VLC_EGENERIC;

            const uint16_t i_nalu_length = p_buf[0] << 8 | p_buf[1];
            if(i_buf < (size_t)i_nalu_length + 2)
                return VLC_EGENERIC;

            bool foo;
            hevc_helper_parse_nal( hh, &p_buf[0],
                                   i_nalu_length + 2, 2, &foo );

            p_buf += i_nalu_length + 2;
            i_buf -= i_nalu_length + 2;
        }
    }

    return VLC_SUCCESS;
}

static int
hevc_helper_set_extra(struct hxxx_helper *hh, const void *p_extra,
                      size_t i_extra)
{
    if (hevc_ishvcC(p_extra, i_extra))
    {
        hh->i_nal_length_size = hevc_getNALLengthSize(p_extra);
        if (!helper_nal_length_valid(hh))
            return VLC_EGENERIC;
        hh->b_is_xvcC = true;

        return helper_process_hvcC_hevc( hh, p_extra, i_extra );
    }
    else if (hxxx_extra_isannexb(p_extra, i_extra))
    {
        hh->i_nal_length_size = 4;
        bool unused;
        return i_extra == 0 ? VLC_SUCCESS :
               hevc_helper_parse_nal(hh, p_extra, i_extra, 0, &unused);
    }
    else
        return VLC_EGENERIC;
}

static inline block_t *
helper_process_block_hxxx_annexb(struct hxxx_helper *hh,
                                 int(*parser)(struct hxxx_helper *,
                                              const uint8_t*, size_t,uint8_t,bool*),
                                 block_t *p_block, bool *p_config_changed)
{
    if (p_config_changed != NULL)
    {
        int i_ret = parser(hh, p_block->p_buffer, p_block->i_buffer,
                           0, p_config_changed);
        if (i_ret != VLC_SUCCESS)
        {
            block_Release(p_block);
            return NULL;
        }
    }
    return p_block;
}

static block_t *
helper_process_block_h264_annexb(struct hxxx_helper *hh, block_t *p_block,
                                 bool *p_config_changed)
{
    if (p_config_changed != NULL)
        return helper_process_block_hxxx_annexb(hh, h264_helper_parse_nal,
                                                p_block,p_config_changed);
    return p_block;
}

static block_t *
helper_process_block_hevc_annexb(struct hxxx_helper *hh, block_t *p_block,
                                 bool *p_config_changed)
{
    if (p_config_changed != NULL)
        return helper_process_block_hxxx_annexb(hh, hevc_helper_parse_nal,
                                                p_block,p_config_changed);
    return p_block;
}

static block_t *
helper_process_block_xvcc2annexb(struct hxxx_helper *hh, block_t *p_block,
                                 bool *p_config_changed)
{
    assert(helper_nal_length_valid(hh));
    h264_AVC_to_AnnexB(p_block->p_buffer, p_block->i_buffer,
                       hh->i_nal_length_size);
    return helper_process_block_h264_annexb(hh, p_block, p_config_changed);
}

static block_t *
helper_process_block_h264_annexb2avcc(struct hxxx_helper *hh, block_t *p_block,
                                      bool *p_config_changed)
{
    p_block = helper_process_block_h264_annexb(hh, p_block, p_config_changed);
    return p_block ? hxxx_AnnexB_to_xVC(p_block, hh->i_nal_length_size) : NULL;
}

static block_t *
helper_process_block_hevc_annexb2hvcc(struct hxxx_helper *hh, block_t *p_block,
                                      bool *p_config_changed)
{
    p_block = helper_process_block_hevc_annexb(hh, p_block, p_config_changed);
    return p_block ? hxxx_AnnexB_to_xVC(p_block, hh->i_nal_length_size) : NULL;
}

static block_t *
helper_process_block_h264_avcc(struct hxxx_helper *hh, block_t *p_block,
                               bool *p_config_changed)
{
    if (p_config_changed != NULL)
    {
        int i_ret = h264_helper_parse_nal(hh, p_block->p_buffer,
                                          p_block->i_buffer,
                                          hh->i_nal_length_size,
                                          p_config_changed);
        if (i_ret != VLC_SUCCESS)
        {
            block_Release(p_block);
            return NULL;
        }
    }
    return p_block;
}

static block_t *
helper_process_block_hevc_hvcc(struct hxxx_helper *hh, block_t *p_block,
                               bool *p_config_changed)
{
    if (p_config_changed != NULL)
    {
        int i_ret = hevc_helper_parse_nal(hh, p_block->p_buffer,
                                          p_block->i_buffer,
                                          hh->i_nal_length_size,
                                          p_config_changed);
        if (i_ret != VLC_SUCCESS)
        {
            block_Release(p_block);
            return NULL;
        }
    }
    return p_block;
}

int
hxxx_helper_set_extra(struct hxxx_helper *hh, const void *p_extra,
                      size_t i_extra)
{
    int i_ret;
    switch (hh->i_codec)
    {
        case VLC_CODEC_H264:
            i_ret = h264_helper_set_extra(hh, p_extra, i_extra);
            break;
        case VLC_CODEC_HEVC:
            i_ret = hevc_helper_set_extra(hh, p_extra, i_extra);
            break;
        default:
            vlc_assert_unreachable();
    }
    if (i_ret != VLC_SUCCESS)
        return i_ret;

    switch (hh->i_codec)
    {
        case VLC_CODEC_H264:
            if (hh->b_is_xvcC)
            {
                if (hh->b_need_xvcC)
                    hh->pf_process_block = helper_process_block_h264_avcc;
                else
                    hh->pf_process_block = helper_process_block_xvcc2annexb;
            }
            else /* AnnexB */
            {
                if (hh->b_need_xvcC)
                    hh->pf_process_block = helper_process_block_h264_annexb2avcc;
                else
                    hh->pf_process_block = helper_process_block_h264_annexb;
            }
            break;
        case VLC_CODEC_HEVC:
            if (hh->b_is_xvcC)
            {
                if (hh->b_need_xvcC)
                    hh->pf_process_block = helper_process_block_hevc_hvcc;
                else
                    hh->pf_process_block = helper_process_block_xvcc2annexb;
            }
            else /* AnnexB */
            {
                if (hh->b_need_xvcC)
                    hh->pf_process_block = helper_process_block_hevc_annexb2hvcc;
                else
                    hh->pf_process_block = helper_process_block_hevc_annexb;
            }
            break;
        default:
            vlc_assert_unreachable();
    }
    return VLC_SUCCESS;;
}

static block_t *
hxxx_helper_get_annexb_config( const struct hxxx_helper_nal *pp_nal_lists[],
                               const size_t p_nal_counts[],
                               const size_t p_nal_maxs[],
                               size_t i_lists_size )
{
    static const uint8_t annexb_startcode[] = { 0x00, 0x00, 0x00, 0x01 };

    block_t *p_block_list = NULL, *p_current;
    for (size_t i = 0; i < i_lists_size; ++i)
    {
        size_t i_nals_size = 0;
        const struct hxxx_helper_nal *p_nal;
        HELPER_FOREACH_NAL(p_nal, pp_nal_lists[i], p_nal_counts[i], p_nal_maxs[i])
        {
            i_nals_size += p_nal->b->i_buffer + sizeof annexb_startcode;
        }

        block_t *p_block = block_Alloc(i_nals_size);
        if (p_block == NULL)
        {
            if (p_block_list != NULL)
                block_Release(p_block_list);
            return NULL;
        }

        p_block->i_buffer = 0;
        HELPER_FOREACH_NAL(p_nal, pp_nal_lists[i], p_nal_counts[i], p_nal_maxs[i])
        {
            memcpy(&p_block->p_buffer[p_block->i_buffer], annexb_startcode,
                   sizeof annexb_startcode);
            p_block->i_buffer += sizeof annexb_startcode;
            memcpy(&p_block->p_buffer[p_block->i_buffer], p_nal->b->p_buffer,
                   p_nal->b->i_buffer);
            p_block->i_buffer += p_nal->b->i_buffer;
        }
        if (p_block_list == NULL)
            p_current = p_block_list = p_block;
        else
        {
            p_current->p_next = p_block;
            p_current = p_block;
        }
    }

    return p_block_list;
}

block_t *
h264_helper_get_annexb_config(const struct hxxx_helper *hh)
{
    if (hh->h264.i_sps_count == 0 || hh->h264.i_pps_count == 0)
        return NULL;

    const struct hxxx_helper_nal *pp_nal_lists[] = {
        hh->h264.sps_list, hh->h264.pps_list };
    const size_t p_nal_counts[] = { hh->h264.i_sps_count, hh->h264.i_pps_count };
    const size_t p_nal_maxs[] = { H264_SPS_ID_MAX+1, H264_PPS_ID_MAX+1 };

    return hxxx_helper_get_annexb_config( pp_nal_lists, p_nal_counts, p_nal_maxs, 2 );
}

block_t *
hevc_helper_get_annexb_config(const struct hxxx_helper *hh)
{
    if (hh->hevc.i_vps_count == 0 || hh->hevc.i_sps_count == 0 ||
        hh->hevc.i_pps_count == 0 )
        return NULL;

    const struct hxxx_helper_nal *pp_nal_lists[] = {
        hh->hevc.vps_list, hh->hevc.sps_list, hh->hevc.pps_list };
    const size_t p_nal_counts[] = { hh->hevc.i_vps_count, hh->hevc.i_sps_count,
                                    hh->hevc.i_pps_count };
    const size_t p_nal_maxs[] = { HEVC_VPS_ID_MAX+1, HEVC_SPS_ID_MAX+1, HEVC_PPS_ID_MAX+1 };

    return hxxx_helper_get_annexb_config( pp_nal_lists, p_nal_counts, p_nal_maxs, 3 );
}

block_t *
h264_helper_get_avcc_config(const struct hxxx_helper *hh)
{
    const struct hxxx_helper_nal *p_nal;
    const uint8_t *pp_sps_bufs[hh->h264.i_sps_count];
    size_t p_sps_sizes[hh->h264.i_sps_count];
    HELPER_FOREACH_NAL(p_nal, hh->h264.sps_list, hh->h264.i_sps_count,
                       H264_SPS_ID_MAX+1)
    {
        pp_sps_bufs[i_nal_found - 1] = p_nal->b->p_buffer;
        p_sps_sizes[i_nal_found - 1] = p_nal->b->i_buffer;
    }

    const uint8_t *pp_pps_bufs[hh->h264.i_pps_count];
    size_t p_pps_sizes[hh->h264.i_pps_count];
    HELPER_FOREACH_NAL(p_nal, hh->h264.pps_list, hh->h264.i_pps_count,
                       H264_PPS_ID_MAX+1)
    {
        pp_pps_bufs[i_nal_found - 1] = p_nal->b->p_buffer;
        p_pps_sizes[i_nal_found - 1] = p_nal->b->i_buffer;
    }
    return h264_NAL_to_avcC(4, pp_sps_bufs, p_sps_sizes, hh->h264.i_sps_count,
                            pp_pps_bufs, p_pps_sizes, hh->h264.i_pps_count);
}

block_t *
hevc_helper_get_hvcc_config(const struct hxxx_helper *hh)
{
    struct hevc_dcr_params params = {};
    const struct hxxx_helper_nal *p_nal;

    HELPER_FOREACH_NAL(p_nal, hh->hevc.vps_list, hh->hevc.i_vps_count,
                       HEVC_VPS_ID_MAX+1)
    {
        params.p_vps[params.i_vps_count] = p_nal->b->p_buffer;
        params.rgi_vps[params.i_vps_count++] = p_nal->b->i_buffer;
    }

    HELPER_FOREACH_NAL(p_nal, hh->hevc.sps_list, hh->hevc.i_sps_count,
                       HEVC_SPS_ID_MAX+1)
    {
        params.p_sps[params.i_sps_count] = p_nal->b->p_buffer;
        params.rgi_sps[params.i_sps_count++] = p_nal->b->i_buffer;
    }

    HELPER_FOREACH_NAL(p_nal, hh->hevc.pps_list, hh->hevc.i_pps_count,
                       HEVC_PPS_ID_MAX+1)
    {
        params.p_pps[params.i_pps_count] = p_nal->b->p_buffer;
        params.rgi_pps[params.i_pps_count++] = p_nal->b->i_buffer;
    }

    HELPER_FOREACH_NAL(p_nal, hh->hevc.sei_list, hh->hevc.i_sei_count,
                       HEVC_DCR_SEI_COUNT)
    {
        if (hevc_getNALType(p_nal->b->p_buffer) == HEVC_NAL_PREF_SEI)
        {
            params.p_seipref[params.i_seipref_count] = p_nal->b->p_buffer;
            params.rgi_seipref[params.i_seipref_count++] = p_nal->b->i_buffer;
        }
        else
        {
            params.p_seisuff[params.i_seisuff_count] = p_nal->b->p_buffer;
            params.rgi_seisuff[params.i_seisuff_count++] = p_nal->b->i_buffer;
        }
    }

    size_t i_dcr;
    uint8_t *p_dcr = hevc_create_dcr(&params, 4, true, &i_dcr);
    if(p_dcr == NULL)
        return NULL;

    return block_heap_Alloc(p_dcr, i_dcr);
}

static const struct hxxx_helper_nal *
h264_helper_get_current_sps(const struct hxxx_helper *hh)
{
    if (hh->h264.i_sps_count == 0)
        return NULL;

    const struct hxxx_helper_nal *hsps =
        &hh->h264.sps_list[hh->h264.i_current_sps];
    assert(hsps->b != NULL);
    return hsps;
}

int
hxxx_helper_get_current_picture_size(const struct hxxx_helper *hh,
                                     unsigned *p_w, unsigned *p_h,
                                     unsigned *p_vw, unsigned *p_vh)
{
    if(hh->i_codec == VLC_CODEC_H264)
    {
        const struct hxxx_helper_nal *hsps = h264_helper_get_current_sps(hh);
        if (hsps && h264_get_picture_size(hsps->h264_sps, p_w, p_h, p_vw, p_vh))
               return VLC_SUCCESS;
    }
    else if(hh->i_codec == VLC_CODEC_HEVC)
    {
        const struct hxxx_helper_nal *hsps = &hh->hevc.sps_list[hh->hevc.i_current_sps];
        if(hsps && hsps->hevc_sps && hevc_get_picture_size(hsps->hevc_sps, p_w, p_h, p_vw, p_vh))
            return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

int
hxxx_helper_get_current_sar(const struct hxxx_helper *hh, int *p_num, int *p_den)
{
    if(hh->i_codec == VLC_CODEC_H264)
    {
        const struct hxxx_helper_nal *hsps = h264_helper_get_current_sps(hh);
        if (hsps)
        {
            *p_num = hsps->h264_sps->vui.i_sar_num;
            *p_den = hsps->h264_sps->vui.i_sar_den;
            return VLC_SUCCESS;
        }
    }
    else if(hh->i_codec == VLC_CODEC_HEVC)
    {
        const struct hxxx_helper_nal *hsps = &hh->hevc.sps_list[hh->hevc.i_current_sps];
        unsigned num, den;
        if(hsps && hsps->hevc_sps && hevc_get_aspect_ratio(hsps->hevc_sps, &num, &den))
        {
            *p_num = num;
            *p_den = den;
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

int
h264_helper_get_current_dpb_values(const struct hxxx_helper *hh,
                                   uint8_t *p_depth, unsigned *p_delay)
{
    const struct hxxx_helper_nal *hsps = h264_helper_get_current_sps(hh);
    if (hsps == NULL)
        return VLC_EGENERIC;
    return h264_get_dpb_values(hsps->h264_sps, p_depth, p_delay) ?
           VLC_SUCCESS : VLC_EGENERIC;
}

int
hxxx_helper_get_current_profile_level(const struct hxxx_helper *hh,
                                      uint8_t *p_profile, uint8_t *p_level)
{
    if(hh->i_codec == VLC_CODEC_H264)
    {
        const struct hxxx_helper_nal *hsps = h264_helper_get_current_sps(hh);
        if (hsps)
        {
            *p_profile = hsps->h264_sps->i_profile;
            *p_level = hsps->h264_sps->i_level;
            return VLC_SUCCESS;
        }
    }
    else if(hh->i_codec == VLC_CODEC_HEVC)
    {
        const struct hxxx_helper_nal *hsps = &hh->hevc.sps_list[hh->hevc.i_current_sps];
        if (hsps && hsps->hevc_sps &&
            hevc_get_sps_profile_tier_level(hsps->hevc_sps, p_profile, p_level))
            return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

int
hxxx_helper_get_chroma_chroma(const struct hxxx_helper *hh, uint8_t *pi_chroma_format,
                              uint8_t *pi_depth_luma, uint8_t *pi_depth_chroma)
{
    switch (hh->i_codec)
    {
        case VLC_CODEC_H264:
        {
            const struct hxxx_helper_nal *hsps = h264_helper_get_current_sps(hh);
            if (hsps == NULL)
                return VLC_EGENERIC;
            return h264_get_chroma_luma(hsps->h264_sps, pi_chroma_format, pi_depth_luma,
                                        pi_depth_chroma)
                == true ? VLC_SUCCESS : VLC_EGENERIC;
        }
        case VLC_CODEC_HEVC:
        {
            const struct hxxx_helper_nal *hsps = &hh->hevc.sps_list[hh->hevc.i_current_sps];
            if (hsps == NULL || hsps->hevc_sps == NULL)
                return VLC_EGENERIC;

            return hevc_get_chroma_luma(hsps->hevc_sps, pi_chroma_format, pi_depth_luma,
                                        pi_depth_chroma)
                == true ? VLC_SUCCESS : VLC_EGENERIC;
        }
        default:
            vlc_assert_unreachable();
    }
}


int
hxxx_helper_get_colorimetry(const struct hxxx_helper *hh,
                            video_color_primaries_t *p_primaries,
                            video_transfer_func_t *p_transfer,
                            video_color_space_t *p_colorspace,
                            video_color_range_t *p_full_range)
{
    switch (hh->i_codec)
    {
        case VLC_CODEC_H264:
        {
            const struct hxxx_helper_nal *hsps = h264_helper_get_current_sps(hh);
            if (hsps == NULL)
                return VLC_EGENERIC;
            return h264_get_colorimetry(hsps->h264_sps, p_primaries, p_transfer,
                                        p_colorspace, p_full_range)
                == true ? VLC_SUCCESS : VLC_EGENERIC;
        }
        case VLC_CODEC_HEVC:
        {
            const struct hxxx_helper_nal *hsps = &hh->hevc.sps_list[hh->hevc.i_current_sps];
            if (hsps == NULL || hsps->hevc_sps == NULL)
                return VLC_EGENERIC;

            return hevc_get_colorimetry(hsps->hevc_sps, p_primaries, p_transfer,
                                        p_colorspace, p_full_range)
                == true ? VLC_SUCCESS : VLC_EGENERIC;
        }
        default:
            vlc_assert_unreachable();
    }
}
