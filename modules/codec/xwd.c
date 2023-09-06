/*****************************************************************************
 * xwd.c: X Window system raster image dump pseudo-decoder
 *****************************************************************************
 * Copyright (C) 2012 Rémi Denis-Courmont
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

#include <assert.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <X11/XWDFile.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

static int Decode (decoder_t *dec, block_t *block)
{
    picture_t *pic = NULL;

    if (block == NULL) /* No Drain */
        return VLCDEC_SUCCESS;

    if (block->i_pts == VLC_TICK_INVALID)
        goto drop; /* undated block, should never happen */
    if (block->i_buffer < sz_XWDheader)
        goto drop;

    /* Skip XWD header */
    const XWDFileHeader *hdr = (const void *)block->p_buffer;
    uint32_t hdrlen = ntohl(hdr->header_size);
    if (hdrlen < sz_XWDheader
     || ntohl(hdr->file_version) < XWD_FILE_VERSION
     || ntohl(hdr->pixmap_format) != 2 /* ZPixmap */)
        goto drop;

    hdrlen += ntohl(hdr->ncolors) * sz_XWDColor;
    if (hdrlen > block->i_buffer)
        goto drop;
    block->p_buffer += hdrlen;
    block->i_buffer -= hdrlen;

    /* Parse XWD header */
    vlc_fourcc_t chroma = 0;
    switch (ntohl(hdr->pixmap_depth))
    {
        case 8:
            if (ntohl(hdr->bits_per_pixel) == 8)
                chroma = VLC_CODEC_RGB233;
            break;
        case 15:
            if (ntohl(hdr->bits_per_pixel) == 16)
                chroma = ntohl(hdr->byte_order) == 0 /* LSBFirst */ ?
                    VLC_CODEC_RGB555LE : VLC_CODEC_RGB555BE;
            break;
        case 16:
            if (ntohl(hdr->bits_per_pixel) == 16)
                chroma = ntohl(hdr->byte_order) == 0 /* LSBFirst */ ?
                    VLC_CODEC_RGB565LE : VLC_CODEC_RGB565BE;
            break;
        case 24:
            switch (ntohl(hdr->bits_per_pixel))
            {
                case 32: chroma = VLC_CODEC_XRGB; break;
                case 24: chroma = VLC_CODEC_RGB24; break;
            }
            break;
        case 32:
            if (ntohl(hdr->bits_per_pixel) == 32)
                chroma = VLC_CODEC_ARGB;
            break;
    }
    /* TODO: check image endianness, set RGB mask */
    if (!chroma)
        goto drop;

    video_format_Setup(&dec->fmt_out.video, chroma,
                       ntohl(hdr->pixmap_width), ntohl(hdr->pixmap_height),
                       ntohl(hdr->pixmap_width), ntohl(hdr->pixmap_height),
                       dec->fmt_in->video.i_sar_num,
                       dec->fmt_in->video.i_sar_den);

    if (decoder_UpdateVideoFormat(dec))
        goto drop;
    pic = decoder_NewPicture(dec);
    if (pic == NULL)
        goto drop;

    plane_t plane;
    plane.p_pixels        = block->p_buffer;
    plane.i_pitch         = plane.i_visible_pitch = ntohl(hdr->bytes_per_line);
    plane.i_lines         = dec->fmt_out.video.i_height;
    plane.i_visible_lines = dec->fmt_out.video.i_visible_height;
    plane_CopyPixels(pic->p, &plane);
    pic->date = block->i_pts;
    pic->b_progressive = true;

drop:
    block_Release(block);
    decoder_QueueVideo(dec, pic);
    return VLCDEC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;

    if (dec->fmt_in->i_codec != VLC_CODEC_XWD)
        return VLC_EGENERIC;

    dec->pf_decode = Decode;
    es_format_Copy(&dec->fmt_out, dec->fmt_in);
    dec->fmt_out.i_codec = VLC_CODEC_XRGB;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_description(N_("XWD image decoder"))
    set_capability("video decoder", 50)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_callback(Open)
vlc_module_end()
