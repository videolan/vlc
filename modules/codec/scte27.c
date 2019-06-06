/*****************************************************************************
 * scte27.c : SCTE-27 subtitles decoder
 *****************************************************************************
 * Copyright (C) Laurent Aimar
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_bits.h>

#include <assert.h>

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_description(N_("SCTE-27 decoder"))
    set_shortname(N_("SCTE-27"))
    set_capability( "spu decoder", 51)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_SCODEC)
    set_callbacks(Open, Close)
vlc_module_end ()

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
typedef struct
{
    int     segment_id;
    int     segment_size;
    uint8_t *segment_buffer;
    vlc_tick_t segment_date;
} decoder_sys_t;

typedef struct {
    uint8_t y, u, v;
    uint8_t alpha;
} scte27_color_t;

static const scte27_color_t scte27_color_transparent = {
    .y     = 0x00,
    .u     = 0x80,
    .v     = 0x80,
    .alpha = 0x00,
};

static scte27_color_t bs_read_color(bs_t *bs)
{
    scte27_color_t color;

    /* XXX it's unclear if a value of 0 in Y/U/V means a transparent pixel */
    color.y     = bs_read(bs, 5) << 3;
    color.alpha = bs_read1(bs) ? 0xff : 0x80;
    color.v     = bs_read(bs, 5) << 3;
    color.u     = bs_read(bs, 5) << 3;

    return color;
}

static inline void SetYUVPPixel(picture_t *picture, int x, int y, int value)
{
    picture->p->p_pixels[y * picture->p->i_pitch + x] = value;
}

static subpicture_region_t *DecodeSimpleBitmap(decoder_t *dec,
                                               const uint8_t *data, int size)
{
    VLC_UNUSED(dec);
    /* Parse the bitmap and its properties */
    bs_t bs;
    bs_init(&bs, data, size);

    bs_skip(&bs, 5);
    int is_framed = bs_read(&bs, 1);
    int outline_style = bs_read(&bs, 2);
    scte27_color_t character_color = bs_read_color(&bs);
    int top_h = bs_read(&bs, 12);
    int top_v = bs_read(&bs, 12);
    int bottom_h = bs_read(&bs, 12);
    int bottom_v = bs_read(&bs, 12);
    if (top_h >= bottom_h || top_v >= bottom_v)
        return NULL;
    int frame_top_h = top_h;
    int frame_top_v = top_v;
    int frame_bottom_h = bottom_h;
    int frame_bottom_v = bottom_v;
    scte27_color_t frame_color = scte27_color_transparent;
    if (is_framed) {
        frame_top_h = bs_read(&bs, 12);
        frame_top_v = bs_read(&bs, 12);
        frame_bottom_h = bs_read(&bs, 12);
        frame_bottom_v = bs_read(&bs, 12);
        frame_color = bs_read_color(&bs);
        if (frame_top_h > top_h ||
            frame_top_v > top_v ||
            frame_bottom_h < bottom_h ||
            frame_bottom_v < bottom_v)
            return NULL;
    }
    int outline_thickness = 0;
    scte27_color_t outline_color = scte27_color_transparent;
    int shadow_right = 0;
    int shadow_bottom = 0;
    scte27_color_t shadow_color = scte27_color_transparent;
    if (outline_style == 1) {
        bs_skip(&bs, 4);
        outline_thickness = bs_read(&bs, 4);
        outline_color = bs_read_color(&bs);
    } else if (outline_style == 2) {
        shadow_right = bs_read(&bs, 4);
        shadow_bottom = bs_read(&bs, 4);
        shadow_color = bs_read_color(&bs);
    } else if (outline_style == 3) {
        bs_skip(&bs, 24);
    }
    bs_skip(&bs, 16); // bitmap_compressed_length
    int bitmap_h = bottom_h - top_h;
    int bitmap_v = bottom_v - top_v;
    int bitmap_size = bitmap_h * bitmap_v;
    bool *bitmap = vlc_alloc(bitmap_size, sizeof(*bitmap));
    if (!bitmap)
        return NULL;
    for (int position = 0; position < bitmap_size;) {
        if (bs_eof(&bs)) {
            for (; position < bitmap_size; position++)
                bitmap[position] = false;
            break;
        }

        int run_on_length = 0;
        int run_off_length = 0;
        if (!bs_read1(&bs)) {
            if (!bs_read1(&bs)) {
                if (!bs_read1(&bs)) {
                    if (bs_read(&bs, 2) == 1) {
                        int next = __MIN((position / bitmap_h + 1) * bitmap_h,
                                         bitmap_size);
                        for (; position < next; position++)
                            bitmap[position] = false;
                    }
                } else {
                    run_on_length = 4;
                }
            } else {
                run_off_length = 6;
            }
        } else {
            run_on_length = 3;
            run_off_length = 5;
        }

        if (run_on_length > 0) {
            int run = bs_read(&bs, run_on_length);
            if (!run)
                run = 1 << run_on_length;
            for (; position < bitmap_size && run > 0; position++, run--)
                bitmap[position] = true;
        }
        if (run_off_length > 0) {
            int run = bs_read(&bs, run_off_length);
            if (!run)
                run = 1 << run_off_length;
            for (; position < bitmap_size && run > 0; position++, run--)
                bitmap[position] = false;
        }
    }

    /* Render the bitmap into a subpicture_region_t */

    /* Reserve the place for the style
     * FIXME It's unclear if it is needed or if the bitmap should already include
     * the needed margin (I think the samples I have do both). */
    int margin_h = 0;
    int margin_v = 0;
    if (outline_style == 1) {
        margin_h =
        margin_v = outline_thickness;
    } else if (outline_style == 2) {
        margin_h = shadow_right;
        margin_v = shadow_bottom;
    }
    frame_top_h -= margin_h;
    frame_top_v -= margin_v;
    frame_bottom_h += margin_h;
    frame_bottom_v += margin_v;

    const int frame_h = frame_bottom_h - frame_top_h;
    const int frame_v = frame_bottom_v - frame_top_v;
    const int bitmap_oh = top_h - frame_top_h;
    const int bitmap_ov = top_v - frame_top_v;

    enum {
        COLOR_FRAME,
        COLOR_CHARACTER,
        COLOR_OUTLINE,
        COLOR_SHADOW,
    };
    video_palette_t palette = {
        .i_entries = 4,
        .palette = {
            [COLOR_FRAME] = {
                frame_color.y,
                frame_color.u,
                frame_color.v,
                frame_color.alpha
            },
            [COLOR_CHARACTER] = {
                character_color.y,
                character_color.u,
                character_color.v,
                character_color.alpha
            },
            [COLOR_OUTLINE] = {
                outline_color.y,
                outline_color.u,
                outline_color.v,
                outline_color.alpha
            },
            [COLOR_SHADOW] = {
                shadow_color.y,
                shadow_color.u,
                shadow_color.v,
                shadow_color.alpha
            },
        },
    };
    video_format_t fmt = {
        .i_chroma = VLC_CODEC_YUVP,
        .i_width = frame_h,
        .i_visible_width = frame_h,
        .i_height = frame_v,
        .i_visible_height = frame_v,
        .i_sar_num = 0, /* Use video AR */
        .i_sar_den = 1,
        .p_palette = &palette,
    };
    subpicture_region_t *r = subpicture_region_New(&fmt);
    if (!r) {
        free(bitmap);
        return NULL;
    }
    r->i_x = frame_top_h;
    r->i_y = frame_top_v;

    /* Fill up with frame (background) color */
    for (int y = 0; y < frame_v; y++)
        memset(&r->p_picture->p->p_pixels[y * r->p_picture->p->i_pitch],
               COLOR_FRAME,
               frame_h);

    /* Draw the outline/shadow if requested */
    if (outline_style == 1) {
        /* Draw an outline
         * XXX simple but slow and of low quality (no anti-aliasing) */
        bool circle[16][16];
        for (int dy = 0; dy <= 15; dy++) {
            for (int dx = 0; dx <= 15; dx++)
                circle[dy][dx] = (dx > 0 || dy > 0) &&
                                 dx * dx + dy * dy <= outline_thickness * outline_thickness;
        }
        for (int by = 0; by < bitmap_v; by++) {
            for (int bx = 0; bx < bitmap_h; bx++) {
                if (!bitmap[by * bitmap_h + bx])
                    continue;
                for (int dy = 0; dy <= outline_thickness; dy++) {
                    for (int dx = 0; dx <= outline_thickness; dx++) {
                        if (circle[dy][dx]) {
                            SetYUVPPixel(r->p_picture,
                                         bx + bitmap_oh + dx, by + bitmap_ov + dy, COLOR_OUTLINE);
                            SetYUVPPixel(r->p_picture,
                                         bx + bitmap_oh - dx, by + bitmap_ov + dy, COLOR_OUTLINE);
                            SetYUVPPixel(r->p_picture,
                                         bx + bitmap_oh + dx, by + bitmap_ov - dy, COLOR_OUTLINE);
                            SetYUVPPixel(r->p_picture,
                                         bx + bitmap_oh - dx, by + bitmap_ov - dy, COLOR_OUTLINE);
                        }
                    }
                }
            }
        }
    } else if (outline_style == 2) {
        /* Draw a shadow by drawing the character shifted by shaddow right/bottom */
        for (int by = 0; by < bitmap_v; by++) {
            for (int bx = 0; bx < bitmap_h; bx++) {
                if (bitmap[by * bitmap_h + bx])
                    SetYUVPPixel(r->p_picture,
                                 bx + bitmap_oh + shadow_right,
                                 by + bitmap_ov + shadow_bottom,
                                 COLOR_SHADOW);
            }
        }
    }

    /* Draw the character */
    for (int by = 0; by < bitmap_v; by++) {
        for (int bx = 0; bx < bitmap_h; bx++) {
            if (bitmap[by * bitmap_h + bx])
                SetYUVPPixel(r->p_picture,
                             bx + bitmap_oh, by + bitmap_ov, COLOR_CHARACTER);
        }
    }
    free(bitmap);
    return r;
}

static subpicture_t *DecodeSubtitleMessage(decoder_t *dec,
                                           const uint8_t *data, int size,
                                           vlc_tick_t date)
{
    if (size < 12)
        goto error;

    /* Parse the header */
    bool pre_clear_display = data[3] & 0x80;
    int  display_standard = data[3] & 0x1f;
    int subtitle_type = data[8] >> 4;
    int display_duration = ((data[8] & 0x07) << 8) | data[9];
    int block_length = GetWBE(&data[10]);

    size -= 12;
    data += 12;

    if (block_length > size)
        goto error;

    if (subtitle_type == 1) {
        subpicture_region_t *region = DecodeSimpleBitmap(dec, data, block_length);
        if (!region)
            goto error;
        subpicture_t *sub = decoder_NewSubpicture(dec, NULL);
        if (!sub) {
            subpicture_region_Delete(region);
            return NULL;
        }
        vlc_tick_t frame_duration;
        switch (display_standard) {
        case 0:
            sub->i_original_picture_width  = 720;
            sub->i_original_picture_height = 480;
            frame_duration = VLC_TICK_FROM_US(33367);
            break;
        case 1:
            sub->i_original_picture_width  = 720;
            sub->i_original_picture_height = 576;
            frame_duration = VLC_TICK_FROM_MS(40);
            break;
        case 2:
            sub->i_original_picture_width  = 1280;
            sub->i_original_picture_height =  720;
            frame_duration = VLC_TICK_FROM_US(16683);
            break;
        case 3:
            sub->i_original_picture_width  = 1920;
            sub->i_original_picture_height = 1080;
            frame_duration = VLC_TICK_FROM_US(16683);
            break;
        default:
            msg_Warn(dec, "Unknown display standard");
            sub->i_original_picture_width  = 0;
            sub->i_original_picture_height = 0;
            frame_duration = VLC_TICK_FROM_MS(40);
            break;
        }
        sub->b_absolute = true;
        if (!pre_clear_display)
            msg_Warn(dec, "SCTE-27 subtitles without pre_clear_display flag are not well supported");
        sub->b_ephemer = true;
        sub->i_start = date;
        sub->i_stop = date + display_duration * frame_duration;
        sub->p_region = region;

        return sub;
    } else {
        /* Reserved */
        return NULL;
    }

error:
    msg_Err(dec, "corrupted subtitle_message");
    return NULL;
}

static int Decode(decoder_t *dec, block_t *b)
{
    decoder_sys_t *sys = dec->p_sys;

    if (b == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    if (b->i_flags & (BLOCK_FLAG_CORRUPTED))
        goto exit;

    while (b->i_buffer > 3) {
        const int table_id =  b->p_buffer[0];
        if (table_id != 0xc6) {
            //if (table_id != 0xff)
            //    msg_Err(dec, "Invalid SCTE-27 table id (0x%x)", table_id);
            break;
        }
        const int section_length = ((b->p_buffer[1] & 0xf) << 8) | b->p_buffer[2];
        if (section_length <= 1 + 4 || b->i_buffer < 3 + (unsigned)section_length) {
            msg_Err(dec, "Invalid SCTE-27 section length");
            break;
        }
        const int protocol_version = b->p_buffer[3] & 0x3f;
        if (protocol_version != 0) {
            msg_Err(dec, "Unsupported SCTE-27 protocol version (%d)", protocol_version);
            break;
        }
        const bool segmentation_overlay = b->p_buffer[3] & 0x40;

        subpicture_t *sub = NULL;
        if (segmentation_overlay) {
            if (section_length < 1 + 5 + 4)
                break;
            int id = GetWBE(&b->p_buffer[4]);
            int last = (b->p_buffer[6] << 4) | (b->p_buffer[7] >> 4);
            int index = ((b->p_buffer[7] & 0x0f) << 8) | b->p_buffer[8];
            if (index > last)
                break;
            if (index == 0) {
                sys->segment_id = id;
                sys->segment_size = 0;
                sys->segment_date = b->i_pts != VLC_TICK_INVALID ? b->i_pts : b->i_dts;
            } else {
                if (sys->segment_id != id || sys->segment_size <= 0) {
                    sys->segment_id = -1;
                    break;
                }
            }

            int segment_size = section_length - 1 - 5 - 4;

            sys->segment_buffer = xrealloc(sys->segment_buffer,
                                           sys->segment_size + segment_size);
            memcpy(&sys->segment_buffer[sys->segment_size],
                   &b->p_buffer[9], segment_size);
            sys->segment_size += segment_size;

            if (index == last) {
                sub = DecodeSubtitleMessage(dec,
                                            sys->segment_buffer,
                                            sys->segment_size,
                                            sys->segment_date);
                sys->segment_size = 0;
            }
        } else {
            sub = DecodeSubtitleMessage(dec,
                                        &b->p_buffer[4],
                                        section_length - 1 - 4,
                                        b->i_pts != VLC_TICK_INVALID ? b->i_pts : b->i_dts);
        }
        if (sub != NULL)
            decoder_QueueSub(dec, sub);

        b->i_buffer -= 3 + section_length;
        b->p_buffer += 3 + section_length;
        break;
    }

exit:
    block_Release(b);
    return VLCDEC_SUCCESS;
}

static int Open(vlc_object_t *object)
{
    decoder_t *dec = (decoder_t *)object;

    if (dec->fmt_in.i_codec != VLC_CODEC_SCTE_27)
        return VLC_EGENERIC;

    decoder_sys_t *sys = dec->p_sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    sys->segment_id = -1;
    sys->segment_size = 0;
    sys->segment_buffer = NULL;

    dec->pf_decode = Decode;
    dec->fmt_out.i_codec = VLC_CODEC_YUVP;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    decoder_t *dec = (decoder_t *)object;
    decoder_sys_t *sys = dec->p_sys;

    free(sys->segment_buffer);
    free(sys);
}

