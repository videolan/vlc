/*****************************************************************************
 * image.c: Image demuxer
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_image.h>
#include "mxpeg_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define ID_TEXT N_("ES ID")
#define ID_LONGTEXT N_( \
    "Set the ID of the elementary stream")

#define GROUP_TEXT N_("Group")
#define GROUP_LONGTEXT N_(\
    "Set the group of the elementary stream")

#define DECODE_TEXT N_("Decode")
#define DECODE_LONGTEXT N_( \
    "Decode at the demuxer stage")

#define CHROMA_TEXT N_("Forced chroma")
#define CHROMA_LONGTEXT N_( \
    "If non empty and image-decode is true, the image will be " \
    "converted to the specified chroma.")

#define DURATION_TEXT N_("Duration in seconds")
#define DURATION_LONGTEXT N_( \
    "Duration in seconds before simulating an end of file. " \
    "A negative value means an unlimited play time.")

#define FPS_TEXT N_("Frame rate")
#define FPS_LONGTEXT N_( \
    "Frame rate of the elementary stream produced.")

#define RT_TEXT N_("Real-time")
#define RT_LONGTEXT N_( \
    "Use real-time mode suitable for being used as a master input and " \
    "real-time input slaves.")

vlc_module_begin()
    set_description(N_("Image demuxer"))
    set_shortname(N_("Image"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_DEMUX)
    add_integer("image-id", -1, ID_TEXT, ID_LONGTEXT, true)
        change_safe()
    add_integer("image-group", 0, GROUP_TEXT, GROUP_LONGTEXT, true)
        change_safe()
    add_bool("image-decode", true, DECODE_TEXT, DECODE_LONGTEXT, true)
        change_safe()
    add_string("image-chroma", "", CHROMA_TEXT, CHROMA_LONGTEXT, true)
        change_safe()
    add_float("image-duration", 10, DURATION_TEXT, DURATION_LONGTEXT, false)
        change_safe()
    add_string("image-fps", "10/1", FPS_TEXT, FPS_LONGTEXT, true)
        change_safe()
    add_bool("image-realtime", false, RT_TEXT, RT_LONGTEXT, true)
        change_safe()
    set_capability("demux", 10)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    block_t     *data;
    es_out_id_t *es;
    vlc_tick_t  duration;
    bool        is_realtime;
    vlc_tick_t  pts_offset;
    vlc_tick_t  pts_next;
    date_t        pts;
} demux_sys_t;

static block_t *Load(demux_t *demux)
{
    const unsigned max_size = 4096 * 4096 * 8;
    uint64_t size;

    if (vlc_stream_GetSize(demux->s, &size) == VLC_SUCCESS) {
        if (size > max_size) {
            msg_Err(demux, "image too large (%"PRIu64" > %u), rejected",
                    size, max_size);
            return NULL;
        }
    } else
        size = max_size;

    block_t *block = block_Alloc(size);
    if (block == NULL)
        return NULL;

    ssize_t val = vlc_stream_Read(demux->s, block->p_buffer, size);
    if (val < 0) {
        block_Release(block);
        return NULL;
    }

    block->i_buffer = val;
    return block;
}

static block_t *Decode(demux_t *demux,
                       es_format_t *fmt, vlc_fourcc_t chroma, block_t *data)
{
    image_handler_t *handler = image_HandlerCreate(demux);
    if (!handler) {
        block_Release(data);
        return NULL;
    }

    video_format_t decoded;
    video_format_Init(&decoded, chroma);

    picture_t *image = image_Read(handler, data, fmt, &decoded);
    image_HandlerDelete(handler);

    if (!image)
        return NULL;

    es_format_Clean(fmt);
    es_format_InitFromVideo(fmt, &decoded);
    video_format_Clean(&decoded);

    size_t size = 0;
    for (int i = 0; i < image->i_planes; i++)
        size += image->p[i].i_pitch * image->p[i].i_lines;

    data = block_Alloc(size);
    if (!data) {
        picture_Release(image);
        return NULL;
    }

    size_t offset = 0;
    for (int i = 0; i < image->i_planes; i++) {
        const plane_t *src = &image->p[i];
        for (int y = 0; y < src->i_visible_lines; y++) {
            memcpy(&data->p_buffer[offset],
                   &src->p_pixels[y * src->i_pitch],
                   src->i_visible_pitch);
            offset += src->i_visible_pitch;
        }
    }

    picture_Release(image);
    return data;
}

static int Demux(demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;

    if (!sys->data)
        return VLC_DEMUXER_EOF;

    vlc_tick_t deadline;
    const vlc_tick_t pts_first = sys->pts_offset + date_Get(&sys->pts);
    if (sys->pts_next != VLC_TICK_INVALID) {
        deadline = sys->pts_next;
    } else if (sys->is_realtime) {
        deadline = vlc_tick_now();
        const vlc_tick_t max_wait = VLC_TICK_FROM_MS(20);
        if (deadline + max_wait < pts_first) {
            es_out_SetPCR(demux->out, deadline);
            /* That's ugly, but not yet easily fixable */
            vlc_tick_wait(deadline + max_wait);
            return VLC_DEMUXER_SUCCESS;
        }
    } else {
        deadline = 1 + pts_first;
    }

    for (;;) {
        const vlc_tick_t pts = sys->pts_offset + date_Get(&sys->pts);
        if (sys->duration >= 0 && pts >= VLC_TICK_0 + sys->pts_offset + sys->duration)
            return VLC_DEMUXER_EOF;

        if (pts >= deadline)
            return VLC_DEMUXER_SUCCESS;

        block_t *data = block_Duplicate(sys->data);
        if (!data)
            return VLC_DEMUXER_EGENERIC;

        data->i_dts =
        data->i_pts = VLC_TICK_0 + pts;
        es_out_SetPCR(demux->out, data->i_pts);
        if(sys->es)
            es_out_Send(demux->out, sys->es, data);
        else
            block_Release(data);

        date_Increment(&sys->pts, 1);
    }
}

static int Control(demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query) {
    case DEMUX_CAN_SEEK:
        *va_arg(args, bool *) = sys->duration >= 0 && !sys->is_realtime;
        return VLC_SUCCESS;
    case DEMUX_GET_POSITION: {
        double *position = va_arg(args, double *);
        if (sys->duration > 0)
            *position = date_Get(&sys->pts) / (double)sys->duration;
        else
            *position = 0;
        return VLC_SUCCESS;
    }
    case DEMUX_SET_POSITION: {
        if (sys->duration < 0 || sys->is_realtime)
            return VLC_EGENERIC;
        double position = va_arg(args, double);
        date_Set(&sys->pts, position * sys->duration);
        return VLC_SUCCESS;
    }
    case DEMUX_GET_TIME: {
        *va_arg(args, vlc_tick_t *) = sys->pts_offset + date_Get(&sys->pts);
        return VLC_SUCCESS;
    }
    case DEMUX_SET_TIME: {
        if (sys->duration < 0 || sys->is_realtime)
            return VLC_EGENERIC;
        vlc_tick_t time = va_arg(args, vlc_tick_t);
        date_Set(&sys->pts, VLC_CLIP(time - sys->pts_offset, VLC_TICK_0, sys->duration));
        return VLC_SUCCESS;
    }
    case DEMUX_SET_NEXT_DEMUX_TIME: {
        vlc_tick_t pts_next = VLC_TICK_0 + va_arg(args, vlc_tick_t);
        if (sys->pts_next == VLC_TICK_INVALID)
            sys->pts_offset = pts_next - VLC_TICK_0;
        sys->pts_next = pts_next;
        return VLC_SUCCESS;
    }
    case DEMUX_GET_LENGTH: {
        *va_arg(args, vlc_tick_t *) = __MAX(sys->duration, 0);
        return VLC_SUCCESS;
    }
    case DEMUX_GET_FPS: {
        double *fps = va_arg(args, double *);
        *fps = (double)sys->pts.i_divider_num / sys->pts.i_divider_den;
        return VLC_SUCCESS;
    }
    case DEMUX_GET_META:
    case DEMUX_HAS_UNSUPPORTED_META:
    case DEMUX_GET_ATTACHMENTS:
        return VLC_EGENERIC;

    case DEMUX_CAN_PAUSE:
    case DEMUX_SET_PAUSE_STATE:
    case DEMUX_CAN_CONTROL_PACE:
    case DEMUX_GET_PTS_DELAY:
        return demux_vaControlHelper( demux->s, 0, -1, 0, 1, query, args );

    default:
        return VLC_EGENERIC;

    }
}

static bool IsBmp(stream_t *s)
{
    const uint8_t *header;
    if (vlc_stream_Peek(s, &header, 18) < 18)
        return false;
    if (memcmp(header, "BM", 2) &&
        memcmp(header, "BA", 2) &&
        memcmp(header, "CI", 2) &&
        memcmp(header, "CP", 2) &&
        memcmp(header, "IC", 2) &&
        memcmp(header, "PT", 2))
        return false;
    uint32_t file_size   = GetDWLE(&header[2]);
    uint32_t data_offset = GetDWLE(&header[10]);
    uint32_t header_size = GetDWLE(&header[14]);
    if (file_size != 14 && file_size != 14 + header_size &&
        file_size <= data_offset)
        return false;
    if (data_offset < header_size + 14)
        return false;
    if (header_size != 12 && header_size < 40)
        return false;
    return true;
}

static bool IsPcx(stream_t *s)
{
    const uint8_t *header;
    if (vlc_stream_Peek(s, &header, 66) < 66)
        return false;
    if (header[0] != 0x0A ||                        /* marker */
        (header[1] != 0x00 && header[1] != 0x02 &&
         header[1] != 0x03 && header[1] != 0x05) || /* version */
        (header[2] != 0 && header[2] != 1) ||       /* encoding */
        (header[3] != 1 && header[3] != 2 &&
         header[3] != 4 && header[3] != 8) ||       /* bits per pixel per plane */
        header[64] != 0 ||                          /* reserved */
        header[65] == 0 || header[65] > 4)          /* plane count */
        return false;
    if (GetWLE(&header[4]) > GetWLE(&header[8]) ||  /* xmin vs xmax */
        GetWLE(&header[6]) > GetWLE(&header[10]))   /* ymin vs ymax */
        return false;
    return true;
}

static bool IsLbm(stream_t *s)
{
    const uint8_t *header;
    if (vlc_stream_Peek(s, &header, 12) < 12)
        return false;
    if (memcmp(&header[0], "FORM", 4) ||
        GetDWBE(&header[4]) <= 4 ||
        (memcmp(&header[8], "ILBM", 4) && memcmp(&header[8], "PBM ", 4)))
        return false;
    return true;
}
static bool IsPnmBlank(uint8_t v)
{
    return v == ' ' || v == '\t' || v == '\r' || v == '\n';
}
static bool IsPnm(stream_t *s)
{
    const uint8_t *header;
    int size = vlc_stream_Peek(s, &header, 256);
    if (size < 3)
        return false;
    if (header[0] != 'P' ||
        header[1] < '1' || header[1] > '6' ||
        !IsPnmBlank(header[2]))
        return false;

    int number_count = 0;
    for (int i = 3, parsing_number = 0; i < size && number_count < 2; i++) {
        if (IsPnmBlank(header[i])) {
            if (parsing_number) {
                parsing_number = 0;
                number_count++;
            }
        } else {
            if (header[i] < '0' || header[i] > '9')
                break;
            parsing_number = 1;
        }
    }
    if (number_count < 2)
        return false;
    return true;
}

static uint8_t FindJpegMarker(int *position, const uint8_t *data, int size)
{
    for (int i = *position; i + 1 < size; i++) {
        if (data[i + 0] != 0xff || data[i + 1] == 0x00)
            return 0xff;
        if (data[i + 1] != 0xff) {
            *position = i + 2;
            return data[i + 1];
        }
    }
    return 0xff;
}
static bool IsJfif(stream_t *s)
{
    const uint8_t *header;
    int size = vlc_stream_Peek(s, &header, 256);
    int position = 0;

    if (FindJpegMarker(&position, header, size) != 0xd8)
        return false;
    if (FindJpegMarker(&position, header, size) != 0xe0)
        return false;
    position += 2;  /* Skip size */
    if (position + 5 > size)
        return false;
    if (memcmp(&header[position], "JFIF\0", 5))
        return false;
    return true;
}

static bool IsWebP(stream_t *s)
{
    const uint8_t *header;
    if (vlc_stream_Peek(s, &header, 20) < 20) /* WebP header size */
        return false;
    if (memcmp(&header[0], "RIFF", 4))
        return false;
    /* TODO: support other chunk types */
    if (memcmp(&header[8], "WEBPVP8 ", 8))
        return false;
    /* skip headers */
    return vlc_stream_Seek(s, 20) == 0;
}

static bool IsSpiff(stream_t *s)
{
    const uint8_t *header;
    if (vlc_stream_Peek(s, &header, 36) < 36) /* SPIFF header size */
        return false;
    if (header[0] != 0xff || header[1] != 0xd8 ||
        header[2] != 0xff || header[3] != 0xe8)
        return false;
    if (memcmp(&header[6], "SPIFF\0", 6))
        return false;
    return true;
}

static bool IsExif(stream_t *s)
{
    const uint8_t *header;
    ssize_t size = vlc_stream_Peek(s, &header, 256);
    if (size == -1)
        return false;
    int position = 0;

    if (FindJpegMarker(&position, header, size) != 0xd8)
        return false;
    if (FindJpegMarker(&position, header, size) != 0xe1)
        return false;
    position += 2;  /* Skip size */
    if (position + 5 > size)
        return false;
    if (memcmp(&header[position], "Exif\0", 5))
        return false;
    return true;
}

static bool FindSVGmarker(int *position, const uint8_t *data, const int size, const char *marker)
{
    for( int i = *position; i < size; i++)
    {
        if (memcmp(&data[i], marker, strlen(marker)) == 0)
        {
            *position = i;
            return true;
        }
    }
    return false;
}

static bool IsSVG(stream_t *s)
{
    if (s->psz_url == NULL)
        return false;

    char *ext = strstr(s->psz_url, ".svg");
    if (!ext) return false;

    const uint8_t *header;
    ssize_t size = vlc_stream_Peek(s, &header, 4096);
    if (size == -1)
        return false;
    int position = 0;

    const char xml[] = "<?xml version=\"";
    if (!FindSVGmarker(&position, header, size, xml))
        return false;
    if (position != 0)
        return false;

    const char endxml[] = ">\0";
    if (!FindSVGmarker(&position, header, size, endxml))
        return false;
    if (position <= 15)
        return false;

    const char svg[] = "<svg";
    if (!FindSVGmarker(&position, header, size, svg))
        return false;
    if (position < 19)
        return false;

    /* SVG Scalable Vector Graphics image */

    /* NOTE: some SVG images have the mimetype set in a meta data section
     * and some do not */
    return true;
}

static bool IsTarga(stream_t *s)
{
    /* The header is not enough to ensure proper detection, we need
     * to have a look at the footer. But doing so can be slow. So
     * try to avoid it when possible */
    const uint8_t *header;
    if (vlc_stream_Peek(s, &header, 18) < 18)   /* Targa fixed header */
        return false;
    if (header[1] > 1)                      /* Color Map Type */
        return false;
    if ((header[1] != 0 || header[3 + 4] != 0) &&
        header[3 + 4] != 8  &&
        header[3 + 4] != 15 && header[3 + 4] != 16 &&
        header[3 + 4] != 24 && header[3 + 4] != 32)
        return false;
    if ((header[2] > 3 && header[2] < 9) || header[2] > 11) /* Image Type */
        return false;
    if (GetWLE(&header[8 + 4]) <= 0 ||      /* Width */
        GetWLE(&header[8 + 6]) <= 0)        /* Height */
        return false;
    if (header[8 + 8] != 8  &&
        header[8 + 8] != 15 && header[8 + 8] != 16 &&
        header[8 + 8] != 24 && header[8 + 8] != 32)
        return false;
    if (header[8 + 9] & 0xc0)               /* Reserved bits */
        return false;

    const int64_t size = stream_Size(s);
    if (size <= 18 + 26)
        return false;
    bool can_seek;
    if (vlc_stream_Control(s, STREAM_CAN_SEEK, &can_seek) || !can_seek)
        return false;

    const int64_t position = vlc_stream_Tell(s);
    if (vlc_stream_Seek(s, size - 26))
        return false;

    const uint8_t *footer;
    if (vlc_stream_Peek(s, &footer, 26) < 26
     || memcmp(&footer[8], "TRUEVISION-XFILE.\x00", 18))
        return false;

    return vlc_stream_Seek(s, position) == 0;
}

typedef struct {
    vlc_fourcc_t  codec;
    size_t        marker_size;
    const uint8_t marker[14];
    bool          (*detect)(stream_t *s);
} image_format_t;

#define VLC_CODEC_XCF VLC_FOURCC('X', 'C', 'F', ' ')
#define VLC_CODEC_LBM VLC_FOURCC('L', 'B', 'M', ' ')
static const image_format_t formats[] = {
    { .codec = VLC_CODEC_XCF,
      .marker_size = 9 + 4 + 1,
      .marker = { 'g', 'i', 'm', 'p', ' ', 'x', 'c', 'f', ' ',
                  'f', 'i', 'l', 'e', '\0' }
    },
    { .codec = VLC_CODEC_XCF,
      .marker_size = 9 + 4 + 1,
      .marker = { 'g', 'i', 'm', 'p', ' ', 'x', 'c', 'f', ' ',
                  'v', '0', '0', '1', '\0' }
    },
    { .codec = VLC_CODEC_XCF,
      .marker_size = 9 + 4 + 1,
      .marker = { 'g', 'i', 'm', 'p', ' ', 'x', 'c', 'f', ' ',
                  'v', '0', '0', '2', '\0' }
    },
    { .codec = VLC_CODEC_PNG,
      .marker_size = 8,
      .marker = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A }
    },
    { .codec = VLC_CODEC_GIF,
      .marker_size = 6,
      .marker = { 'G', 'I', 'F', '8', '7', 'a' }
    },
    { .codec = VLC_CODEC_GIF,
      .marker_size = 6,
      .marker = { 'G', 'I', 'F', '8', '9', 'a' }
    },
    /* XXX TIFF detection may be a bit weak */
    { .codec = VLC_CODEC_TIFF,
      .marker_size = 4,
      .marker = { 'I', 'I', 0x2a, 0x00 },
    },
    { .codec = VLC_CODEC_TIFF,
      .marker_size = 4,
      .marker = { 'M', 'M', 0x00, 0x2a },
    },
    { .codec = VLC_CODEC_BMP,
      .detect = IsBmp,
    },
    { .codec = VLC_CODEC_PCX,
      .detect = IsPcx,
    },
    { .codec = VLC_CODEC_LBM,
      .detect = IsLbm,
    },
    { .codec = VLC_CODEC_PNM,
      .detect = IsPnm,
    },
    { .codec = VLC_CODEC_MXPEG,
      .detect = IsMxpeg,
    },
    { .codec = VLC_CODEC_JPEG,
      .detect = IsJfif,
    },
    { .codec = VLC_CODEC_JPEG,
      .detect = IsSpiff,
    },
    { .codec = VLC_CODEC_JPEG,
      .detect = IsExif,
    },
    { .codec = VLC_CODEC_WEBP,
      .detect = IsWebP,
    },
    { .codec = VLC_CODEC_BPG,
      .marker_size = 4,
      .marker = { 'B', 'P', 'G', 0xFB },
    },
    { .codec = VLC_CODEC_SVG,
      .detect = IsSVG,
    },
    { .codec = VLC_CODEC_TARGA,
      .detect = IsTarga,
    },
};

static vlc_fourcc_t Detect(stream_t *s)
{
    const uint8_t *peek;
    size_t peek_size = 0;

    for (size_t i = 0; i < ARRAY_SIZE(formats); i++) {
        const image_format_t *img = &formats[i];

        if (img->detect != NULL) {
            if (img->detect(s))
                return img->codec;

            if (vlc_stream_Seek(s, 0))
               return 0;

            /* Seeking invalidates the current peek buffer */
            peek_size = 0;
            continue;
        }

        if (peek_size < img->marker_size) {
            ssize_t val = vlc_stream_Peek(s, &peek, img->marker_size);
            if (val < 0)
                continue;
            peek_size = val;
        }

        assert(img->marker_size > 0); /* ensure peek is a valid pointer */

        if (peek_size >= img->marker_size
         && memcmp(peek, img->marker, img->marker_size) == 0)
            return img->codec;
    }
    return 0;
}

static int Open(vlc_object_t *object)
{
    demux_t *demux = (demux_t*)object;

    /* Detect the image type */
    vlc_fourcc_t codec = Detect(demux->s);
    if (codec == 0)
        return VLC_EGENERIC;

    msg_Dbg(demux, "Detected image: %s",
            vlc_fourcc_GetDescription(VIDEO_ES, codec));

    if (codec == VLC_CODEC_MXPEG)
        return VLC_EGENERIC; //let avformat demux this file

    /* Load and if selected decode */
    es_format_t fmt;
    es_format_Init(&fmt, VIDEO_ES, codec);
    fmt.video.i_chroma = fmt.i_codec;

    block_t *data = Load(demux);
    if (data && var_InheritBool(demux, "image-decode")) {
        char *string = var_InheritString(demux, "image-chroma");
        vlc_fourcc_t chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, string);
        free(string);

        data = Decode(demux, &fmt, chroma, data);
    }
    fmt.i_id    = var_InheritInteger(demux, "image-id");
    fmt.i_group = var_InheritInteger(demux, "image-group");
    if (var_InheritURational(demux,
                             &fmt.video.i_frame_rate,
                             &fmt.video.i_frame_rate_base,
                             "image-fps") ||
        fmt.video.i_frame_rate <= 0 || fmt.video.i_frame_rate_base <= 0) {
        msg_Err(demux, "Invalid frame rate, using 10/1 instead");
        fmt.video.i_frame_rate      = 10;
        fmt.video.i_frame_rate_base = 1;
    }

    /* If loadind failed, we still continue to avoid mis-detection
     * by other demuxers. */
    if (!data)
        msg_Err(demux, "Failed to load the image");

    /* */
    demux_sys_t *sys = malloc(sizeof(*sys));
    if (!sys) {
        if (data)
            block_Release(data);
        es_format_Clean(&fmt);
        return VLC_ENOMEM;
    }

    sys->data        = data;
    sys->es          = es_out_Add(demux->out, &fmt);
    sys->duration    = vlc_tick_from_sec( var_InheritFloat(demux, "image-duration") );
    sys->is_realtime = var_InheritBool(demux, "image-realtime");
    sys->pts_offset  = sys->is_realtime ? vlc_tick_now() : 0;
    sys->pts_next    = VLC_TICK_INVALID;
    date_Init(&sys->pts, fmt.video.i_frame_rate, fmt.video.i_frame_rate_base);
    date_Set(&sys->pts, VLC_TICK_0);

    es_format_Clean(&fmt);

    demux->pf_demux   = Demux;
    demux->pf_control = Control;
    demux->p_sys      = sys;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    demux_t     *demux = (demux_t*)object;
    demux_sys_t *sys   = demux->p_sys;

    if (sys->data)
        block_Release(sys->data);
    free(sys);
}

