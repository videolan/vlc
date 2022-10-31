/*****************************************************************************
 * rtp-rawvid.c: RTP raw video decoder
 *****************************************************************************
 * Copyright (C) 2022 Rémi Denis-Courmont
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
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#define PLANES_3(bits) \
    uint##bits##_t *restrict p0 = planes[0]; \
    uint##bits##_t *restrict p1 = planes[1]; \
    uint##bits##_t *restrict p2 = planes[2]; \
    (void)0

#define PLANES_4(bits) \
    uint##bits##_t *restrict p0 = planes[0]; \
    uint##bits##_t *restrict p1 = planes[1]; \
    uint##bits##_t *restrict p2 = planes[2]; \
    uint##bits##_t *restrict p3 = planes[3]; \
    (void)0

/* Read one 8-bit sample from one octet */
#define READ_8(a) \
    uint_fast8_t a; \
\
    do { \
        a = *(in++); \
        len--; \
    } while (0)

/* Read four 10-bit samples from five octets. */
#define READ_10(a, b, c, d) \
    uint_fast16_t a, b, c, d; \
\
    do { \
        uint_fast8_t b0_ = in[0]; \
        uint_fast8_t b1_ = in[1]; \
        uint_fast8_t b2_ = in[2]; \
        uint_fast8_t b3_ = in[3]; \
        uint_fast8_t b4_ = in[4]; \
\
        a =  (b0_ << 2) | (b1_ >> 6); \
        b = ((b1_ << 4) | (b2_ >> 4)) & 0x3ff; \
        c = ((b2_ << 6) | (b3_ >> 2)) & 0x3ff; \
        d = ((b3_ << 8) | (b4_ >> 0)) & 0x3ff; \
        in += 5; \
        len -= 5; \
    } while (0)

/* Read two 12-bit samples from three octets. */
#define READ_12(a, b) \
    uint_fast16_t a, b; \
\
    do { \
        uint_fast8_t b0_ = in[0]; \
        uint_fast8_t b1_ = in[1]; \
        uint_fast8_t b2_ = in[2]; \
\
        a =  (b0_ << 4) | (b1_ >> 4); \
        b = ((b1_ << 4) | (b2_ >> 0)) & 0xfff; \
        in += 3; \
        len -= 3; \
    } while (0)


/* Read one 16-bit sample from two octets */
#define READ_16(a) \
    uint_fast16_t a; \
\
    do { \
        a = GetWBE(in); \
        in += 2; \
        len -= 2; \
    } while (0)

#define WRITE_RGB(n) \
    do { \
        *(p0++) = g##n; \
        *(p1++) = b##n; \
        *(p2++) = r##n; \
    } while (0)

#define WRITE_RGBA(n) \
    do { \
        *(p0++) = g##n; \
        *(p1++) = b##n; \
        *(p2++) = r##n; \
        *(p3++) = a##n; \
    } while (0)

#define WRITE_YUV444(n) \
    do { \
        *(p0++) = y##n; \
        *(p1++) = u##n; \
        *(p2++) = v##n; \
    } while (0)

#define WRITE_YUV422(n) \
    do { \
        *(p0++) = y0##n; \
        *(p0++) = y1##n; \
        *(p1++) = u##n; \
        *(p2++) = v##n; \
    } while (0)

#define WRITE_YUV420(n) \
    do { \
        *(p0++) = y00##n; \
        *(p0++) = y01##n; \
        *(p3++) = y10##n; \
        *(p3++) = y11##n; \
        *(p1++) = u##n; \
        *(p2++) = v##n; \
    } while (0)

#define WRITE_YUV411(n) \
    do { \
        *(p0++) = y0##n; \
        *(p0++) = y1##n; \
        *(p0++) = y2##n; \
        *(p0++) = y3##n; \
        *(p1++) = u##n; \
        *(p2++) = v##n; \
    } while (0)

static void decode_rgb_8(void *restrict *restrict planes,
                         const unsigned char *restrict in, size_t len)
{
    PLANES_3(8);

    while (len > 0) {
        READ_8(r0);
        READ_8(g0);
        READ_8(b0);
        WRITE_RGB(0);
    }
}

static void decode_rgb_10(void *restrict *restrict planes,
                          const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_10(r0, g0, b0, r1);
        READ_10(g1, b1, r2, g2);
        READ_10(b2, r3, b3, g3);
        WRITE_RGB(0);
        WRITE_RGB(1);
        WRITE_RGB(2);
        WRITE_RGB(3);
    }
}

static void decode_rgb_12(void *restrict *restrict planes,
                          const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_12(r0, g0);
        READ_12(b0, r1);
        READ_12(g1, b1);
        WRITE_RGB(0);
        WRITE_RGB(1);
    }
}

static void decode_rgb_16(void *restrict *restrict planes,
                          const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_16(r0);
        READ_16(g0);
        READ_16(b0);
        WRITE_RGB(0);
    }
}

static void decode_rgba_8(void *restrict *restrict planes,
                          const unsigned char *restrict in, size_t len)
{
    PLANES_4(8);

    while (len > 0) {
        READ_8(r0);
        READ_8(g0);
        READ_8(b0);
        READ_8(a0);
        WRITE_RGBA(0);
    }
}

static void decode_rgba_10(void *restrict *restrict planes,
                           const unsigned char *restrict in, size_t len)
{
    PLANES_4(16);

    while (len > 0) {
        READ_10(r0, g0, b0, a0);
        WRITE_RGBA(0);
    }
}

static void decode_rgba_12(void *restrict *restrict planes,
                           const unsigned char *restrict in, size_t len)
{
    PLANES_4(16);

    while (len > 0) {
        READ_12(r0, g0);
        READ_12(b0, a0);
        WRITE_RGBA(0);
    }
}

static void decode_rgba_16(void *restrict *restrict planes,
                           const unsigned char *restrict in, size_t len)
{
    PLANES_4(16);

    while (len > 0) {
        READ_16(r0);
        READ_16(g0);
        READ_16(b0);
        READ_16(a0);
        WRITE_RGBA(0);
    }
}

static void decode_bgr_8(void *restrict *restrict planes,
                         const unsigned char *restrict in, size_t len)
{
    PLANES_3(8);

    while (len > 0) {
        READ_8(b0);
        READ_8(g0);
        READ_8(r0);
        WRITE_RGB(0);
    }
}

static void decode_bgr_10(void *restrict *restrict planes,
                          const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_10(b0, g0, r0, b1);
        READ_10(g1, r1, b2, g2);
        READ_10(r2, b3, g3, r3);
        WRITE_RGB(0);
        WRITE_RGB(1);
        WRITE_RGB(2);
        WRITE_RGB(3);
    }
}

static void decode_bgr_12(void *restrict *restrict planes,
                          const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_12(b0, g0);
        READ_12(r0, b1);
        READ_12(g1, r1);
        WRITE_RGB(0);
        WRITE_RGB(1);
    }
}

static void decode_bgr_16(void *restrict *restrict planes,
                          const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_16(b0);
        READ_16(g0);
        READ_16(r0);
        WRITE_RGB(0);
    }
}

static void decode_bgra_8(void *restrict *restrict planes,
                          const unsigned char *restrict in, size_t len)
{
    PLANES_4(8);

    while (len > 0) {
        READ_8(b0);
        READ_8(g0);
        READ_8(r0);
        READ_8(a0);
        WRITE_RGBA(0);
    }
}

static void decode_bgra_10(void *restrict *restrict planes,
                           const unsigned char *restrict in, size_t len)
{
    PLANES_4(16);

    while (len > 0) {
        READ_10(b0, g0, r0, a0);
        WRITE_RGBA(0);
    }
}

static void decode_bgra_12(void *restrict *restrict planes,
                           const unsigned char *restrict in, size_t len)
{
    PLANES_4(16);

    while (len > 0) {
        READ_12(b0, g0);
        READ_12(r0, a0);
        WRITE_RGBA(0);
    }
}

static void decode_bgra_16(void *restrict *restrict planes,
                           const unsigned char *restrict in, size_t len)
{
    PLANES_4(16);

    while (len > 0) {
        READ_16(b0);
        READ_16(g0);
        READ_16(r0);
        READ_16(a0);
        WRITE_RGBA(0);
    }
}

static void decode_yuv444_8(void *restrict *restrict planes,
                            const unsigned char *restrict in, size_t len)
{
    PLANES_3(8);

    while (len > 0) {
        READ_8(u0);
        READ_8(y0);
        READ_8(v0);
        WRITE_YUV444(0);
    }
}

static void decode_yuv444_10(void *restrict *restrict planes,
                             const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_10(u0, y0, v0, u1);
        READ_10(y1, v1, u2, y2);
        READ_10(v2, u3, y3, v3);
        WRITE_YUV444(0);
        WRITE_YUV444(1);
        WRITE_YUV444(2);
        WRITE_YUV444(3);
    }
}

static void decode_yuv444_12(void *restrict *restrict planes,
                             const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_12(u0, y0);
        READ_12(v0, u1);
        READ_12(y1, v1);
        WRITE_YUV444(0);
        WRITE_YUV444(1);
    }
}

static void decode_yuv444_16(void *restrict *restrict planes,
                             const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_16(u0);
        READ_16(y0);
        READ_16(v0);
        WRITE_YUV444(0);
    }
}

static void decode_yuv422_8(void *restrict *restrict planes,
                            const unsigned char *restrict in, size_t len)
{
    PLANES_3(8);

    while (len > 0) {
        READ_8(u0);
        READ_8(y00);
        READ_8(v0);
        READ_8(y10);
        WRITE_YUV422(0);
    }
}

static void decode_yuv422_10(void *restrict *restrict planes,
                             const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_10(u0, y00, v0, y10);
        WRITE_YUV422(0);
    }
}

static void decode_yuv422_12(void *restrict *restrict planes,
                             const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_12(u0, y00);
        READ_12(v0, y10);
        WRITE_YUV422(0);
    }
}

static void decode_yuv422_16(void *restrict *restrict planes,
                             const unsigned char *restrict in, size_t len)
{
    PLANES_3(16);

    while (len > 0) {
        READ_16(u0);
        READ_16(y00);
        READ_16(v0);
        READ_16(y10);
        WRITE_YUV422(0);
    }
}

static void decode_yuv420_8(void *restrict *restrict planes,
                            const unsigned char *restrict in, size_t len)
{
    PLANES_4(8);

    while (len > 0) {
        READ_8(y000);
        READ_8(y010);
        READ_8(y100);
        READ_8(y110);
        READ_8(u0);
        READ_8(v0);
        WRITE_YUV420(0);
    }
}

static void decode_yuv420_10(void *restrict *restrict planes,
                             const unsigned char *restrict in, size_t len)
{
    PLANES_4(16);

    while (len > 0) {
        READ_10(y000, y010, y100, y110);
        READ_10(u0, v0, y001, y011);
        READ_10(y101, y111, u1, v1);
        WRITE_YUV420(0);
        WRITE_YUV420(1);
    }
}

static void decode_yuv420_12(void *restrict *restrict planes,
                             const unsigned char *restrict in, size_t len)
{
    PLANES_4(16);

    while (len > 0) {
        READ_12(y000, y010);
        READ_12(y100, y110);
        READ_12(u0, v0);
        WRITE_YUV420(0);
    }
}

static void decode_yuv420_16(void *restrict *restrict planes,
                             const unsigned char *restrict in, size_t len)
{
    PLANES_4(16);

    while (len > 0) {
        READ_16(y000);
        READ_16(y010);
        READ_16(y100);
        READ_16(y110);
        READ_16(u0);
        READ_16(v0);
        WRITE_YUV420(0);
    }
}

static void decode_yuv411_8(void *restrict *restrict planes,
                            const unsigned char *restrict in, size_t len)
{
    PLANES_3(8);

    while (len > 0) {
        READ_8(u0);
        READ_8(y00);
        READ_8(y10);
        READ_8(v0);
        READ_8(y20);
        READ_8(y30);
        WRITE_YUV411(0);
    }
}

typedef void (*vlc_rtp_video_raw_cb)(void *restrict *,
                                     const unsigned char *, size_t);

struct vlc_rtp_video_raw_dec {
    unsigned int pgroup;
    bool half_height_uv;
    vlc_rtp_video_raw_cb decode_line;
    picture_t *pic;
};

static int Decode(decoder_t *dec, vlc_frame_t *block)
{
    struct vlc_rtp_video_raw_dec *sys = dec->p_sys;
    picture_t *pic = sys->pic;
    bool continuation;

    if (block == NULL) {
        /* Draining */
        if (pic != NULL) /* Send incomplete picture */
            decoder_QueueVideo(dec, pic);
        sys->pic = NULL;
        return VLCDEC_SUCCESS;
    }

    if ((block->i_flags & VLC_FRAME_FLAG_DISCONTINUITY)
     && pic != NULL && pic->date != block->i_pts) {
        /* Ideally, the EOS is set on the last block for a picture.
         * This manual check is necessary to deal with packet loss. */
        decoder_QueueVideo(dec, pic);
        pic = sys->pic = NULL;
    }

    if (pic == NULL) {
        pic = decoder_NewPicture(dec);

        if (pic == NULL) {
            block_Release(block);
            return VLCDEC_SUCCESS;
        }

        pic->date = block->i_pts;
        pic->b_progressive = true; /* TODO: interlacing */
        sys->pic = pic;
    }

    const unsigned char *in = block->p_buffer;
    size_t inlen = block->i_buffer;
    const unsigned int width = dec->fmt_out.video.i_width;
    const unsigned int height = dec->fmt_out.video.i_height;

    do {
        if (unlikely(inlen < 6)) {
corrupt:    msg_Err(dec, "corrupt packet, %zu bytes remaining", inlen);
            break;
        }

        uint_fast16_t length = GetWBE(in);
        uint_fast16_t lineno = GetWBE(in + 2);
        uint_fast16_t offset = GetWBE(in + 4);

        lineno &= 0x7fff; /* TODO: interlacing */
        continuation = (offset & 0x8000) != 0;
        offset &= 0x7fff;

        in += 6;
        inlen -= 6;

        div_t d = div(length, sys->pgroup);

        if (inlen < length /* input buffer underflow */
         || d.rem != 0 /* length must be a multiple of pgroup size */
         || offset + (unsigned)d.quot >= width /* output scanline overflow */
         || lineno >= height /* output picture overflow */)
            goto corrupt;

        void *restrict planes[4];

        if (sys->half_height_uv) {
            /* For I420, treat the odd Y lines as the 4th plane */
            if (unlikely(lineno & 1))
                goto corrupt; /* line number must always be even */

            assert(pic->i_planes <= 3);
            planes[0] = pic->p[0].p_pixels + lineno * pic->p[0].i_pitch
                                           + offset * pic->p[0].i_pixel_pitch;
            planes[3] = ((unsigned char *)planes[0]) + pic->p[0].i_pitch;
            lineno /= 2;

            for (int i = 1; i < pic->i_planes; i++) {
                plane_t *p = &pic->p[i];

                planes[i] = p->p_pixels + lineno * p->i_pitch
                                        + offset * p->i_pixel_pitch;
            }
        } else {
            for (int i = 0; i < pic->i_planes; i++) {
                plane_t *p = &pic->p[i];

                planes[i] = p->p_pixels + lineno * p->i_pitch
                                        + offset * p->i_pixel_pitch;
            }
        }

        sys->decode_line(planes, in, length);
    } while (continuation);

    if (block->i_flags & VLC_FRAME_FLAG_END_OF_SEQUENCE) {
        decoder_QueueVideo(dec, pic);
        sys->pic = NULL;
    }

    block_Release(block);
    return VLCDEC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;
    struct vlc_rtp_video_raw_dec *sys = dec->p_sys;

    if (sys->pic != NULL)
        picture_Release(sys->pic);
}

struct vlc_rtp_video_raw_format {
    vlc_fourcc_t fourcc;
    vlc_rtp_video_raw_cb line_cb;
};

/**
 * RTP video/raw sampling
 *
 * This defines the list of all supported (per component) bit depths.
 */
struct vlc_rtp_video_raw_sampling {
    struct vlc_rtp_video_raw_format depth8;
    struct vlc_rtp_video_raw_format depth10;
    struct vlc_rtp_video_raw_format depth12;
    struct vlc_rtp_video_raw_format depth16;
};

/**
 * RTP video/raw samplings
 *
 * This type defines the list of all support RTP video/raw samplings.
 * \note This is purposedly a structure rather than an array so that CPU
 * optimisations can readily cherry-pick which samplings to optimise.
 */
struct vlc_rtp_video_raw_samplings {
    struct vlc_rtp_video_raw_sampling rgb;
    struct vlc_rtp_video_raw_sampling rgba;
    struct vlc_rtp_video_raw_sampling bgr;
    struct vlc_rtp_video_raw_sampling bgra;
    struct vlc_rtp_video_raw_sampling yuv444;
    struct vlc_rtp_video_raw_sampling yuv422;
    struct vlc_rtp_video_raw_sampling yuv420;
    struct vlc_rtp_video_raw_sampling yuv411;
};

#ifdef WORDS_BIGENDIAN
#define NE(x) x##B
#else
#define NE(x) x##L
#endif

static const struct vlc_rtp_video_raw_samplings samplings = {
    .rgb = {
        {    VLC_CODEC_GBR_PLANAR,     decode_rgb_8,  },
        { NE(VLC_CODEC_GBR_PLANAR_10), decode_rgb_10, },
        { NE(VLC_CODEC_GBR_PLANAR_12), decode_rgb_12, },
        { NE(VLC_CODEC_GBR_PLANAR_16), decode_rgb_16, },
    },
    .rgba = {
        {    VLC_CODEC_GBR_PLANAR,     decode_rgba_8,  },
        { NE(VLC_CODEC_GBR_PLANAR_10), decode_rgba_10, },
        { NE(VLC_CODEC_GBR_PLANAR_12), decode_rgba_12, },
        { NE(VLC_CODEC_GBR_PLANAR_16), decode_rgba_16, },
    },
    .bgr = {
        {    VLC_CODEC_GBR_PLANAR,     decode_bgr_8,  },
        { NE(VLC_CODEC_GBR_PLANAR_10), decode_bgr_10, },
        { NE(VLC_CODEC_GBR_PLANAR_12), decode_bgr_12, },
        { NE(VLC_CODEC_GBR_PLANAR_16), decode_bgr_16, },
    },
    .bgra = {
        {    VLC_CODEC_GBR_PLANAR,     decode_bgra_8,  },
        { NE(VLC_CODEC_GBR_PLANAR_10), decode_bgra_10, },
        { NE(VLC_CODEC_GBR_PLANAR_12), decode_bgra_12, },
        { NE(VLC_CODEC_GBR_PLANAR_16), decode_bgra_16, },
    },
    .yuv444 = {
        {    VLC_CODEC_I444,           decode_yuv444_8,  },
        { NE(VLC_CODEC_I444_10),       decode_yuv444_10, },
        { NE(VLC_CODEC_I444_12),       decode_yuv444_12, },
        { NE(VLC_CODEC_I444_16),       decode_yuv444_16, },
    },
    .yuv422 = {
        {    VLC_CODEC_I422,           decode_yuv422_8,  },
        { NE(VLC_CODEC_I422_10),       decode_yuv422_10, },
        { NE(VLC_CODEC_I422_12),       decode_yuv422_12, },
        { NE(VLC_CODEC_I422_16),       decode_yuv422_16, },
    },
    .yuv420 = {
        {    VLC_CODEC_I420,           decode_yuv420_8,  },
        { NE(VLC_CODEC_I420_10),       decode_yuv420_10, },
        { NE(VLC_CODEC_I420_12),       decode_yuv420_12, },
        { NE(VLC_CODEC_I420_16),       decode_yuv420_16, },
    },
    .yuv411 = {
        {    VLC_CODEC_I411,           decode_yuv411_8,  },
        /* High-definition 4:1:1 not supported */
        {  0, NULL }, { 0, NULL }, { 0, NULL }
    },
};

static int Open(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;
    const char *sname = dec->fmt_in->p_extra;

    if (dec->fmt_in->i_codec != VLC_CODEC_RTP_VIDEO_RAW)
        return VLC_ENOTSUP;
    if (dec->fmt_in->i_extra <= 0 || sname[dec->fmt_in->i_extra - 1] != '\0')
        return VLC_EINVAL;

    /* Sampling is supplied as extra data, bit depth as level */
    unsigned int depth = dec->fmt_in->i_level;
    const struct vlc_rtp_video_raw_sampling *sampling;
    unsigned int spmp; /* samples per macropixel */
    bool half_height_uv = false;

    if (strcmp(sname, "RGB") == 0) {
        sampling = &samplings.rgb;
        spmp = 3;
    } else if (strcmp(sname, "RGBA") == 0) {
        sampling = &samplings.rgba;
        spmp = 4;
    } else if (strcmp(sname, "BGR") == 0) {
        sampling = &samplings.bgr;
        spmp = 3;
    } else if (strcmp(sname, "BGRA") == 0) {
        sampling = &samplings.bgra;
        spmp = 4;
    } else if (strcmp(sname, "YCbCr-4:4:4") == 0) {
        sampling = &samplings.yuv444;
        spmp = 3;
    } else if (strcmp(sname, "YCbCr-4:2:2") == 0) {
        sampling = &samplings.yuv422;
        spmp = 4;
    } else if (strcmp(sname, "YCbCr-4:2:0") == 0) {
        sampling = &samplings.yuv420;
        spmp = 6;
        half_height_uv = true;
    } else if (strcmp(sname, "YCbCr-4:1:1") == 0) {
        sampling = &samplings.yuv411;
        spmp = 6;
    } else {
        msg_Err(obj, "unknown RTP video sampling %s", sname);
        return VLC_ENOTSUP;
    }

    const struct vlc_rtp_video_raw_format *format;

    switch (depth) {
        case 8:
            format = &sampling->depth8;
            break;
        case 10:
            format = &sampling->depth10;
            break;
        case 12:
            format = &sampling->depth12;
            break;
        case 16:
            format = &sampling->depth16;
            break;
        default:
            msg_Err(obj, "unsupported RTP video bit depth %u", depth);
            return VLC_ENOTSUP;
    }

    if (format->fourcc == 0) {
        msg_Err(obj, "unimplemented RTP video format %u-bit %s",
                depth, sname);
        return VLC_ENOTSUP;
    }

    struct vlc_rtp_video_raw_dec *sys = vlc_obj_malloc(obj, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    es_format_Copy(&dec->fmt_out, dec->fmt_in);
    dec->fmt_out.i_codec = format->fourcc;
    dec->fmt_out.video.i_chroma = format->fourcc;

    int ret = decoder_UpdateVideoFormat(dec);
    if (ret != VLC_SUCCESS)
        return ret;

    unsigned int bpmp = depth * spmp;

    /* pixel group size equals LCM(depth * spmp, 8) bits */
    sys->pgroup = bpmp >> ((bpmp % 8) ? vlc_ctz(bpmp) : 3);
    sys->half_height_uv = half_height_uv;
    sys->decode_line = format->line_cb;
    sys->pic = NULL;
    dec->p_sys = sys;
    dec->pf_decode = Decode;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_description(N_("RTP raw video decoder"))
    set_capability("video decoder", 50)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_callbacks(Open, Close)
vlc_module_end()
