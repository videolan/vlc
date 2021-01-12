/*****************************************************************************
 * repack.c: Codec specific formatting for AnnexB multiplexers
 *****************************************************************************
 * Copyright (C) 2021 VideoLabs, VLC authors and VideoLAN
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
#include <vlc_block.h>

#include "repack.h"
#include "../../packetizer/hevc_nal.h"
#include "../../packetizer/h264_nal.h"
#include "../../packetizer/hxxx_nal.h"

#include <assert.h>

static void AnnexBInject(block_t **pp_pes,
                   const uint8_t *p_extra, size_t i_extra,
                   const uint8_t *p_au, size_t i_au)
{
    if(!i_extra && !i_au)
        return;

    *pp_pes = block_Realloc(*pp_pes,
                            i_extra + i_au,
                            (*pp_pes)->i_buffer);
    if(!*pp_pes)
        return;
    if(i_au)
        memcpy(&(*pp_pes)->p_buffer[0], p_au, i_au);
    if(i_extra)
        memcpy(&(*pp_pes)->p_buffer[i_au], p_extra, i_extra);
}


static void PES_RepackHEVC(block_t **pp_pes,
                                    const uint8_t *p_extra, size_t i_extra)
{
    size_t i_au = 6;
    size_t i_aucurrent = 0;
    const uint8_t audata[] = { 0x00, 0x00, 0x00, 0x01, 0x46, 0x01 };
    hxxx_iterator_ctx_t ctx;
    hxxx_iterator_init(&ctx, (*pp_pes)->p_buffer, (*pp_pes)->i_buffer, 0);
    const uint8_t *p_nal; size_t i_nal;
    while(hxxx_annexb_iterate_next(&ctx, &p_nal, &i_nal))
    {
        if(i_nal < 2)
            return;
        uint8_t type = hevc_getNALType(p_nal);
        if(type < HEVC_NAL_VPS)
            break;
        switch(type)
        {
            case HEVC_NAL_AUD:
                i_au = 0;
                i_aucurrent = 2 + (p_nal - (*pp_pes)->p_buffer);
                break;
            case HEVC_NAL_VPS:
            case HEVC_NAL_PPS:
            case HEVC_NAL_SPS:
                i_extra = 0;
                break;
            default:
                break;
        }
    }

    if(i_extra && i_aucurrent) /* strip existing AU for now */
    {
        (*pp_pes)->p_buffer += i_aucurrent;
        (*pp_pes)->i_buffer -= i_aucurrent;
        i_au = 6;
    }

    AnnexBInject(pp_pes, p_extra, i_extra, audata, i_au);
}

static void PES_RepackH264(block_t **pp_pes,
                                    const uint8_t *p_extra, size_t i_extra)
{
    size_t i_au = 6;
    size_t i_aucurrent = 0;
    const uint8_t audata[] = { 0x00, 0x00, 0x00, 0x01, 0x09, 0xf0 };
    hxxx_iterator_ctx_t ctx;
    hxxx_iterator_init(&ctx, (*pp_pes)->p_buffer, (*pp_pes)->i_buffer, 0);
    const uint8_t *p_nal; size_t i_nal;
    while(hxxx_annexb_iterate_next(&ctx, &p_nal, &i_nal))
    {
        if(i_nal < 2)
            return;
        uint8_t type = p_nal[0]&0x1f;
        if(type < H264_NAL_SEI)
            break;
        switch(type)
        {
            case H264_NAL_AU_DELIMITER:
                i_au = 0;
                i_aucurrent = 2 + (p_nal - (*pp_pes)->p_buffer);
                break;
            case H264_NAL_SPS:
            case H264_NAL_PPS:
                i_extra = 0;
                break;
            default:
                break;
        }
    }

    if(i_extra && i_aucurrent) /* strip existing AU for now */
    {
        (*pp_pes)->p_buffer += i_aucurrent;
        (*pp_pes)->i_buffer -= i_aucurrent;
        i_au = 6;
    }

    AnnexBInject(pp_pes, p_extra, i_extra, audata, i_au);
}

static void PES_RepackMP4V(block_t **pp_pes,
                           const uint8_t *p_extra, size_t i_extra)
{
    hxxx_iterator_ctx_t ctx;
    hxxx_iterator_init(&ctx, (*pp_pes)->p_buffer, (*pp_pes)->i_buffer, 0);
    const uint8_t *p_nal; size_t i_nal;
    while(hxxx_annexb_iterate_next(&ctx, &p_nal, &i_nal))
    {
        if(i_nal < 2)
            return;
        if(p_nal[0] >= 0x30) /* > VOLS */
            break;
        if(p_nal[0] >= 0x20 && p_nal[0] == p_extra[3]) /* same VOL */
            i_extra = 0;
    }

    AnnexBInject(pp_pes, p_extra, i_extra, NULL, 0);
}

block_t * PES_Repack(vlc_fourcc_t i_codec,
                     const uint8_t *p_extra, size_t i_extra,
                     block_t **pp_pes)
{
    /* safety check for annexb extra */
    if(i_extra < 4 ||
       (memcmp(p_extra, annexb_startcode4, 4) &&
        memcmp(&p_extra[1], annexb_startcode3, 3)))
        i_extra = 0;

    switch(i_codec)
    {
        case VLC_CODEC_HEVC:
            PES_RepackHEVC(pp_pes, p_extra, i_extra);
            break;
        case VLC_CODEC_H264:
            PES_RepackH264(pp_pes, p_extra, i_extra);
            break;
        case VLC_CODEC_MP4V:
            PES_RepackMP4V(pp_pes, p_extra, i_extra);
            break;
        default:
            break;
    }
    return *pp_pes;
}
