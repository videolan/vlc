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
#include <arpa/inet.h>
#include <X11/XWDFile.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

static int Open(vlc_object_t *);

vlc_module_begin()
    set_description(N_("XWD image decoder"))
    set_capability("decoder", 50)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_callbacks(Open, NULL)
vlc_module_end()

static picture_t *Decode(decoder_t *, block_t **);

static int Open(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;

    if (dec->fmt_in.i_codec != VLC_CODEC_XWD)
        return VLC_EGENERIC;

    dec->pf_decode_video = Decode;
    es_format_Copy(&dec->fmt_out, &dec->fmt_in);
    dec->fmt_out.i_codec = VLC_CODEC_RGB32;
    dec->fmt_out.i_cat = VIDEO_ES;
    return VLC_SUCCESS;
}

static picture_t *Decode (decoder_t *dec, block_t **pp)
{
    picture_t *pic = NULL;

    if (pp == NULL)
        return NULL;

    block_t *block = *pp;
    if (block == NULL)
        return NULL;
    *pp = NULL;

    if (block->i_pts <= VLC_TS_INVALID)
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
                chroma = VLC_CODEC_RGB8;
            break;
        case 15:
            if (ntohl(hdr->bits_per_pixel) == 16)
                chroma = VLC_CODEC_RGB15;
            break;
        case 16:
            if (ntohl(hdr->bits_per_pixel) == 16)
                chroma = VLC_CODEC_RGB16;
            break;
        case 24:
            switch (ntohl(hdr->bits_per_pixel))
            {
                case 32: chroma = VLC_CODEC_RGB32; break;
                case 24: chroma = VLC_CODEC_RGB24; break;
            }
            break;
        case 32:
            if (ntohl(hdr->bits_per_pixel) == 32)
                chroma = VLC_CODEC_RGBA;
            break;
    }
    /* TODO: check image endianess, set RGB mask */
    if (!chroma)
        goto drop;

    video_format_Setup(&dec->fmt_out.video, chroma,
                       ntohl(hdr->pixmap_width), ntohl(hdr->pixmap_height),
                       dec->fmt_in.video.i_sar_num,
                       dec->fmt_in.video.i_sar_den);

    const size_t copy = dec->fmt_out.video.i_width
                        * (dec->fmt_out.video.i_bits_per_pixel / 8);
    const uint32_t pitch = ntohl(hdr->bytes_per_line);

    /* Build picture */
    if (pitch < copy
     || (block->i_buffer / pitch) < dec->fmt_out.video.i_height)
        goto drop;

    pic = decoder_NewPicture(dec);
    if (pic == NULL)
        goto drop;

    const uint8_t *in = block->p_buffer;
    uint8_t *out = pic->p->p_pixels;
    for (unsigned i = 0; i < dec->fmt_out.video.i_height; i++)
    {
        memcpy(out, in, copy);
        in += pitch;
        out += pic->p->i_pitch;
    }
    pic->date = block->i_pts;
    pic->b_progressive = true;

drop:
    block_Release(block);
    return pic;
}
