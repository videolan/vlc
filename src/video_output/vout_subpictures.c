/*****************************************************************************
 * vout_subpictures.c : subpicture management functions
 *****************************************************************************
 * Copyright (C) 2000-2019 VLC authors, VideoLAN and Videolabs SAS
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <limits.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_vout.h>
#include <vlc_filter.h>
#include <vlc_spu.h>
#include <vlc_vector.h>

#include "../libvlc.h"
#include "vout_internal.h"
#include "../misc/subpicture.h"
#include "../input/input_internal.h"
#include "../clock/clock.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Hold of subpicture with original ts */
typedef struct {
    /* Shared with prerendering thread */
    subpicture_t *subpic; /* picture to be rendered */
    /* */
    vlc_tick_t orgstart; /* original picture timestamp, for conversion updates */
    vlc_tick_t orgstop;
    vlc_tick_t start; /* current entry rendering time, */
    vlc_tick_t stop;  /* set to subpicture at rendering time */
    bool is_late;
    enum vlc_vout_order channel_order;
} spu_render_entry_t;

typedef struct VLC_VECTOR(spu_render_entry_t) spu_render_vector;

struct spu_channel {
    spu_render_vector entries;
    size_t id;
    enum vlc_vout_order order;
    vlc_clock_t *clock;
    vlc_tick_t delay;
    float rate;
};

typedef struct VLC_VECTOR(struct spu_channel) spu_channel_vector;
typedef struct VLC_VECTOR(subpicture_t *) spu_prerender_vector;
#define SPU_CHROMALIST_COUNT 8

struct spu_private_t {
    vlc_mutex_t  lock;            /* lock to protect all followings fields */
    input_thread_t *input;

    spu_channel_vector channels;

    int channel;             /**< number of subpicture channels registered */
    filter_t *text;                              /**< text renderer module */
    vlc_mutex_t textlock;
    filter_t *scale_yuvp;                     /**< scaling module for YUVP */
    filter_t *scale;                    /**< scaling module (all but YUVP) */
    bool force_crop;                     /**< force cropping of subpicture */
    struct {
        int x;
        int y;
        int width;
        int height;
    } crop;                                                  /**< cropping */

    int margin;                        /**< force position of a subpicture */
    /**
     * Move the secondary subtites vertically.
     * Note: Primary sub margin is applied to all sub tracks and is absolute.
     * Secondary sub margin is not absolute to enable overlap detection.
     */
    int secondary_margin;
    int secondary_alignment;       /**< Force alignment for secondary subs */
    video_palette_t palette;              /**< force palette of subpicture */

    /* Subpiture filters */
    char           *source_chain_current;
    char           *source_chain_update;
    filter_chain_t *source_chain;
    char           *filter_chain_current;
    char           *filter_chain_update;
    vlc_mutex_t    filter_chain_lock;
    filter_chain_t *filter_chain;
    /**/
    struct
    {
        vlc_thread_t    thread;
        vlc_mutex_t     lock;
        vlc_cond_t      cond;
        vlc_cond_t      output_cond;
        spu_prerender_vector vector;
        subpicture_t   *p_processed;
        video_format_t  fmtsrc;
        video_format_t  fmtdst;
        vlc_fourcc_t    chroma_list[SPU_CHROMALIST_COUNT+1];
        bool            live;
    } prerender;

    /* */
    vlc_tick_t          last_sort_date;
    vout_thread_t       *vout;
};

static void spu_PrerenderSync(spu_private_t *, const subpicture_t *);
static void spu_PrerenderCancel(spu_private_t *, const subpicture_t *);

static void spu_channel_Init(struct spu_channel *channel, size_t id,
                             enum vlc_vout_order order, vlc_clock_t *clock)
{
    channel->id = id;
    channel->clock = clock;
    channel->delay = 0;
    channel->rate = 1.f;
    channel->order = order;

    vlc_vector_init(&channel->entries);
}

static int spu_channel_Push(struct spu_channel *channel, subpicture_t *subpic,
                            vlc_tick_t orgstart, vlc_tick_t orgstop)
{
    const spu_render_entry_t entry = {
        .subpic = subpic,
        .orgstart = orgstart,
        .orgstop = orgstop,
        .start = subpic->i_start,
        .stop = subpic->i_stop,
    };
    return vlc_vector_push(&channel->entries, entry) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void spu_channel_DeleteAt(struct spu_channel *channel, size_t index)
{
    assert(index < channel->entries.size);
    assert(channel->entries.data[index].subpic);

    subpicture_Delete(channel->entries.data[index].subpic);
    vlc_vector_remove(&channel->entries, index);
}

static void spu_channel_Clean(spu_private_t *sys, struct spu_channel *channel)
{
    for (size_t i = 0; i < channel->entries.size; i++)
    {
        assert(channel->entries.data[i].subpic);
        spu_PrerenderCancel(sys, channel->entries.data[i].subpic);
        subpicture_Delete(channel->entries.data[i].subpic);
    }
    vlc_vector_destroy(&channel->entries);
}

static struct spu_channel *spu_GetChannel(spu_t *spu, size_t channel_id)
{
    spu_private_t *sys = spu->p;

    for (size_t i = 0; i < sys->channels.size; ++i)
        if (sys->channels.data[i].id == channel_id)
            return &sys->channels.data[i];

    vlc_assert_unreachable();
}

static ssize_t spu_GetFreeChannelId(spu_t *spu, enum vlc_vout_order *order)
{
    spu_private_t *sys = spu->p;

    if (unlikely(sys->channels.size > SSIZE_MAX))
        return VOUT_SPU_CHANNEL_INVALID;

    size_t id;
    if (order)
        *order = VLC_VOUT_ORDER_PRIMARY;
    for (id = VOUT_SPU_CHANNEL_OSD_COUNT; id < sys->channels.size + 1; ++id)
    {
        bool used = false;
        for (size_t i = VOUT_SPU_CHANNEL_OSD_COUNT; i < sys->channels.size; ++i)
        {
            if (sys->channels.data[i].id == id)
            {
                used = true;
                if (order)
                    *order = VLC_VOUT_ORDER_SECONDARY;
                break;
            }
        }
        if (!used)
            return id;
    }
    return VOUT_SPU_CHANNEL_INVALID;
}

static void FilterRelease(filter_t *filter)
{
    if (filter->p_module)
        module_unneed(filter, filter->p_module);
    vlc_object_delete(filter);
}

static int spu_get_attachments(filter_t *filter,
                               input_attachment_t ***attachment_ptr,
                               int *attachment_count)
{
    spu_t *spu = filter->owner.sys;

    if (spu->p->input)
    {
        int count = input_GetAttachments(spu->p->input, attachment_ptr);
        if (count <= 0)
            return VLC_EGENERIC;
        *attachment_count = count;
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static filter_t *SpuRenderCreateAndLoadText(spu_t *spu)
{
    filter_t *text = vlc_custom_create(spu, sizeof(*text), "spu text");
    if (!text)
        return NULL;

    es_format_Init(&text->fmt_in, VIDEO_ES, 0);

    es_format_Init(&text->fmt_out, VIDEO_ES, 0);
    text->fmt_out.video.i_width          =
    text->fmt_out.video.i_visible_width  = 32;
    text->fmt_out.video.i_height         =
    text->fmt_out.video.i_visible_height = 32;

    text->owner = (const struct filter_owner_t) {
        .pf_get_attachments = spu_get_attachments,
        .sys = spu
    };

    text->p_module = module_need_var(text, "text renderer", "text-renderer");
    if (!text->p_module)
    {
        vlc_object_delete(text);
        return NULL;
    }

    return text;
}

static filter_t *SpuRenderCreateAndLoadScale(vlc_object_t *object,
                                             vlc_fourcc_t src_chroma,
                                             vlc_fourcc_t dst_chroma,
                                             bool require_resize)
{
    filter_t *scale = vlc_custom_create(object, sizeof(*scale), "scale");
    if (!scale)
        return NULL;

    es_format_Init(&scale->fmt_in, VIDEO_ES, 0);
    scale->fmt_in.video.i_chroma = src_chroma;
    scale->fmt_in.video.i_width =
    scale->fmt_in.video.i_visible_width =
    scale->fmt_in.video.i_height =
    scale->fmt_in.video.i_visible_height = 32;

    es_format_Init(&scale->fmt_out, VIDEO_ES, 0);
    scale->fmt_out.video.i_chroma = dst_chroma;
    scale->fmt_out.video.i_width =
    scale->fmt_out.video.i_visible_width =
    scale->fmt_out.video.i_height =
    scale->fmt_out.video.i_visible_height = require_resize ? 16 : 32;

    scale->p_module = module_need(scale, "video converter", NULL, false);
    if (!scale->p_module)
    {
        vlc_object_delete(scale);
        return NULL;
    }

    return scale;
}

static int SpuRenderText(spu_t *spu,
                          subpicture_region_t *region,
                          int i_original_width,
                          int i_original_height,
                          const vlc_fourcc_t *chroma_list)
{
    spu_private_t *sys = spu->p;
    assert(region->fmt.i_chroma == VLC_CODEC_TEXT);

    vlc_mutex_lock(&sys->textlock);
    filter_t *text = sys->text;
    if(!text)
    {
        vlc_mutex_unlock(&sys->textlock);
        return VLC_EGENERIC;
    }

    // assume rendered text is in sRGB if nothing is set
    if (region->fmt.transfer == TRANSFER_FUNC_UNDEF)
        region->fmt.transfer = TRANSFER_FUNC_SRGB;
    if (region->fmt.primaries == COLOR_PRIMARIES_UNDEF)
        region->fmt.primaries = COLOR_PRIMARIES_SRGB;
    if (region->fmt.space == COLOR_SPACE_UNDEF)
        region->fmt.space = COLOR_SPACE_SRGB;
    if (region->fmt.color_range == COLOR_RANGE_UNDEF)
        region->fmt.color_range = COLOR_RANGE_FULL;

    /* FIXME aspect ratio ? */
    text->fmt_out.video.i_width =
    text->fmt_out.video.i_visible_width  = i_original_width;

    text->fmt_out.video.i_height =
    text->fmt_out.video.i_visible_height = i_original_height;

    int i_ret = text->pf_render(text, region, region, chroma_list);

    vlc_mutex_unlock(&sys->textlock);
    return i_ret;
}

/**
 * A few scale functions helpers.
 */

#define SCALE_UNIT (10000)
typedef struct {
    int w;
    int h;
} spu_scale_t;

static spu_scale_t spu_scale_create(int w, int h)
{
    spu_scale_t s = { .w = w, .h = h };
    if (s.w <= 0)
        s.w = SCALE_UNIT;
    if (s.h <= 0)
        s.h = SCALE_UNIT;
    return s;
}
static spu_scale_t spu_scale_unit(void)
{
    return spu_scale_create(SCALE_UNIT, SCALE_UNIT);
}
static spu_scale_t spu_scale_createq(int64_t wn, int64_t wd, int64_t hn, int64_t hd)
{
    return spu_scale_create(wn * SCALE_UNIT / wd,
                            hn * SCALE_UNIT / hd);
}
static int spu_scale_w(int v, const spu_scale_t s)
{
    return v * s.w / SCALE_UNIT;
}
static int spu_scale_h(int v, const spu_scale_t s)
{
    return v * s.h / SCALE_UNIT;
}
static int spu_invscale_w(int v, const spu_scale_t s)
{
    return v * SCALE_UNIT / s.w;
}
static int spu_invscale_h(int v, const spu_scale_t s)
{
    return v * SCALE_UNIT / s.h;
}

/**
 * A few area functions helpers
 */
typedef struct {
    int x;
    int y;
    int width;
    int height;

    spu_scale_t scale;
} spu_area_t;

static spu_area_t spu_area_create(int x, int y, int w, int h, spu_scale_t s)
{
    spu_area_t a = { .x = x, .y = y, .width = w, .height = h, .scale = s };
    return a;
}
static spu_area_t spu_area_scaled(spu_area_t a)
{
    if (a.scale.w == SCALE_UNIT && a.scale.h == SCALE_UNIT)
        return a;

    a.x      = spu_scale_w(a.x,      a.scale);
    a.y      = spu_scale_h(a.y,      a.scale);
    a.width  = spu_scale_w(a.width,  a.scale);
    a.height = spu_scale_h(a.height, a.scale);

    a.scale = spu_scale_unit();
    return a;
}
static spu_area_t spu_area_unscaled(spu_area_t a, spu_scale_t s)
{
    if (a.scale.w == s.w && a.scale.h == s.h)
        return a;

    a = spu_area_scaled(a);

    a.x      = spu_invscale_w(a.x,      s);
    a.y      = spu_invscale_h(a.y,      s);
    a.width  = spu_invscale_w(a.width,  s);
    a.height = spu_invscale_h(a.height, s);

    a.scale = s;
    return a;
}
static bool spu_area_overlap(spu_area_t a, spu_area_t b)
{
    a = spu_area_scaled(a);
    b = spu_area_scaled(b);

    return __MAX(a.x, b.x) < __MIN(a.x + a.width , b.x + b.width ) &&
           __MAX(a.y, b.y) < __MIN(a.y + a.height, b.y + b.height);
}

/**
 * Avoid area overlapping
 */
static void SpuAreaFixOverlap(spu_area_t *dst,
                              const spu_area_t *sub_array, size_t sub_count, int align)
{
    spu_area_t a = spu_area_scaled(*dst);
    bool is_moved = false;
    bool is_ok;

    /* Check for overlap
     * XXX It is not fast O(n^2) but we should not have a lot of region */
    do {
        is_ok = true;
        for (size_t i = 0; i < sub_count; i++) {
            spu_area_t sub = spu_area_scaled(sub_array[i]);

            if (!spu_area_overlap(a, sub))
                continue;

            if (align & SUBPICTURE_ALIGN_TOP) {
                /* We go down */
                int i_y = sub.y + sub.height;
                a.y = i_y;
                is_moved = true;
            } else if (align & SUBPICTURE_ALIGN_BOTTOM) {
                /* We go up */
                int i_y = sub.y - a.height;
                a.y = i_y;
                is_moved = true;
            } else {
                /* TODO what to do in this case? */
                //fprintf(stderr, "Overlap with unsupported alignment\n");
                break;
            }

            is_ok = false;
            break;
        }
    } while (!is_ok);

    if (is_moved)
        *dst = spu_area_unscaled(a, dst->scale);
}


static void SpuAreaFitInside(spu_area_t *area, const spu_area_t *boundary)
{
    spu_area_t a = spu_area_scaled(*area);

    const int i_error_x = (a.x + a.width) - boundary->width;
    if (i_error_x > 0)
        a.x -= i_error_x;
    if (a.x < 0)
        a.x = 0;

    const int i_error_y = (a.y + a.height) - boundary->height;
    if (i_error_y > 0)
        a.y -= i_error_y;
    if (a.y < 0)
        a.y = 0;

    *area = spu_area_unscaled(a, area->scale);
}

/**
 * Place a region
 */
static void SpuRegionPlace(int *x, int *y,
                           const subpicture_t *subpic,
                           const subpicture_region_t *region,
                           int i_align)
{
    assert(region->i_x != INT_MAX && region->i_y != INT_MAX);
    if (subpic->b_absolute) {
        *x = region->i_x;
        *y = region->i_y;
    } else {
        if (i_align & SUBPICTURE_ALIGN_TOP)
            *y = region->i_y;
        else if (i_align & SUBPICTURE_ALIGN_BOTTOM)
            *y = subpic->i_original_picture_height - region->fmt.i_visible_height - region->i_y;
        else
            *y = subpic->i_original_picture_height / 2 - region->fmt.i_visible_height / 2;

        if (i_align & SUBPICTURE_ALIGN_LEFT)
            *x = region->i_x;
        else if (i_align & SUBPICTURE_ALIGN_RIGHT)
            *x = subpic->i_original_picture_width - region->fmt.i_visible_width - region->i_x;
        else
            *x = subpic->i_original_picture_width / 2 - region->fmt.i_visible_width / 2;
    }
}

/**
 * This function compares two 64 bits integers.
 * It can be used by qsort.
 */
static int IntegerCmp(int64_t i0, int64_t i1)
{
    return i0 < i1 ? -1 : i0 > i1 ? 1 : 0;
}

static int SSizeCmp(ssize_t i0, ssize_t i1)
{
    return i0 < i1 ? -1 : i0 > i1 ? 1 : 0;
}
/**
 * This function compares 2 subpictures using the following properties
 * (ordered by priority)
 * 1. absolute positionning
 * 2. start time (display time)
 * 3. creation order (per channel)
 *
 * It can be used by qsort.
 *
 * XXX spu_RenderSubpictures depends heavily on this order.
 */
static int SpuRenderCmp(const void *s0, const void *s1)
{
    const spu_render_entry_t *render_entry0 = s0;
    const spu_render_entry_t *render_entry1 = s1;
    subpicture_t *subpic0 = render_entry0->subpic;
    subpicture_t *subpic1 = render_entry1->subpic;
    int r;

    r = IntegerCmp(!subpic0->b_absolute, !subpic1->b_absolute);
    if (!r)
        r = IntegerCmp(subpic0->i_start, subpic1->i_start);
    if (!r)
        r = SSizeCmp(subpic0->i_channel, subpic1->i_channel);
    if (!r)
        r = IntegerCmp(subpic0->i_order, subpic1->i_order);
    return r;
}

static size_t spu_channel_UpdateDates(struct spu_channel *channel,
                                       vlc_tick_t system_now)
{
    /* Put every spu start and stop ts into the same array to convert them in
     * one shot */
    if (channel->entries.size == 0)
        return 0;
    vlc_tick_t *date_array = vlc_alloc(channel->entries.size,
                                       2 * sizeof(vlc_tick_t));
    if (!date_array)
        return 0;

    for (size_t index = 0; index < channel->entries.size; index++)
    {
        spu_render_entry_t *current = &channel->entries.data[index];
        assert(current);

        date_array[index * 2] = current->orgstart;
        date_array[index * 2 + 1] = current->orgstop;
    }

    /* Convert all spu ts */
    if (channel->clock)
        vlc_clock_ConvertArrayToSystem(channel->clock, system_now, date_array,
                                       channel->entries.size * 2, channel->rate);

    /* Put back the converted ts into the output spu_render_entry_t struct */
    for (size_t index = 0; index < channel->entries.size; index++)
    {
        spu_render_entry_t *render_entry = &channel->entries.data[index];

        render_entry->start = date_array[index * 2];
        render_entry->stop = date_array[index * 2 + 1];
    }

    free(date_array);
    return channel->entries.size;
}

static bool
spu_render_entry_IsSelected(spu_render_entry_t *render_entry, size_t channel_id,
                            vlc_tick_t system_now,
                            vlc_tick_t render_subtitle_date, bool ignore_osd)
{
    subpicture_t *subpic = render_entry->subpic;
    assert(subpic);

    assert(subpic->i_channel >= 0 && (size_t) subpic->i_channel == channel_id);
    (void) channel_id;

    if (ignore_osd && !subpic->b_subtitle)
        return false;

    const vlc_tick_t render_date =
        subpic->b_subtitle ? render_subtitle_date : system_now;

    if (render_date && render_date < render_entry->start)
        return false; /* Too early, come back next monday */
    return true;
}

/*****************************************************************************
 * spu_SelectSubpictures: find the subpictures to display
 *****************************************************************************
 * This function parses all subpictures and decides which ones need to be
 * displayed. If no picture has been selected, display_date will depend on
 * the subpicture.
 * We also check for ephemer DVD subpictures (subpictures that have
 * to be removed if a newer one is available), which makes it a lot
 * more difficult to guess if a subpicture has to be rendered or not.
 *****************************************************************************/
static spu_render_entry_t *
spu_SelectSubpictures(spu_t *spu, vlc_tick_t system_now,
                      vlc_tick_t render_subtitle_date,
                      bool ignore_osd, size_t *subpicture_count)
{
    spu_private_t *sys = spu->p;

    assert(sys->channels.size >= VOUT_SPU_CHANNEL_OSD_COUNT);

    /* */
    *subpicture_count = 0;
    size_t total_size = 0;
    for (size_t i = 0; i < sys->channels.size; ++i)
        total_size += sys->channels.data[i].entries.size;
    if (total_size == 0)
        return NULL;

    spu_render_entry_t *subpicture_array =
        vlc_alloc(total_size, sizeof(spu_render_entry_t));
    if (!subpicture_array)
        return NULL;

    /* Fill up the subpicture_array arrays with relevant pictures */
    for (size_t i = 0; i < sys->channels.size; i++)
    {
        struct spu_channel *channel = &sys->channels.data[i];

        vlc_tick_t   start_date = render_subtitle_date;
        vlc_tick_t   ephemer_subtitle_date = 0;
        vlc_tick_t   ephemer_osd_date = 0;
        int64_t      selected_max_order = INT64_MIN;

        if (spu_channel_UpdateDates(channel, system_now) == 0)
            continue;

        /* Select available pictures */
        for (size_t index = 0; index < channel->entries.size; index++) {
            spu_render_entry_t *render_entry = &channel->entries.data[index];
            subpicture_t *current = render_entry->subpic;

            if (!spu_render_entry_IsSelected(render_entry, channel->id,
                                             system_now, render_subtitle_date,
                                             ignore_osd))
                continue;

            const vlc_tick_t render_date = current->b_subtitle ? render_subtitle_date : system_now;

            vlc_tick_t *date_ptr  = current->b_subtitle ? &ephemer_subtitle_date  : &ephemer_osd_date;
            if (current->i_start >= *date_ptr) {
                *date_ptr = render_entry->start;
                if (current->i_order > selected_max_order)
                    selected_max_order = current->i_order;
            }

            /* An ephemer with stop time can be ephemer,
               but a pic without stop time must be ephemer */
            if(current->i_stop < current->i_start)
                current->b_ephemer = true;

            /* If the spu is ephemer, the stop time is invalid, but it has been converted to
               system time and used in comparisons below */
            const bool is_stop_valid = !current->b_ephemer || render_entry->orgstop > render_entry->orgstart;
            render_entry->is_late = is_stop_valid && current->i_stop <= render_date;

            /* start_date will be used for correct automatic overlap support
             * in case picture that should not be displayed anymore (display_time)
             * overlap with a picture to be displayed (render_entry->start)  */
            if (current->b_subtitle && !render_entry->is_late && !current->b_ephemer)
                start_date = render_entry->start;
        }

        /* Only forced old picture display at the transition */
        if (start_date < sys->last_sort_date)
            start_date = sys->last_sort_date;
        if (start_date <= 0)
            start_date = INT64_MAX;

        /* Select pictures to be displayed */
        for (size_t index = 0; index < channel->entries.size; ) {
            spu_render_entry_t *render_entry = &channel->entries.data[index];
            subpicture_t *current = render_entry->subpic;
            bool is_late = render_entry->is_late;

            if (!spu_render_entry_IsSelected(render_entry, channel->id,
                                             system_now, render_subtitle_date,
                                             ignore_osd))
            {
                index++;
                continue;
            }

            const vlc_tick_t stop_date = current->b_subtitle ? __MAX(start_date, sys->last_sort_date) : system_now;
            const vlc_tick_t ephemer_date  = current->b_subtitle ? ephemer_subtitle_date  : ephemer_osd_date;

            /* Destroy late and obsolete ephemer subpictures */
            bool is_rejeted = is_late && render_entry->stop  <= stop_date;
            if (current->b_ephemer) {
                if (render_entry->start < ephemer_date)
                    is_rejeted = true;
                else if (render_entry->start == ephemer_date &&
                         current->i_order < selected_max_order)
                    is_rejeted = true;
            }

            if (is_rejeted)
            {
                spu_PrerenderCancel(sys, current);
                subpicture_Delete(current);
                vlc_vector_remove(&channel->entries, index);
            }
            else
            {
                render_entry->channel_order = channel->order;
                subpicture_array[(*subpicture_count)++] = *render_entry;
                index++;
            }
        }
    }

    sys->last_sort_date = render_subtitle_date;
    return subpicture_array;
}



/**
 * It will transform the provided region into another region suitable for rendering.
 */
static void SpuRenderRegion(spu_t *spu,
                            subpicture_region_t **dst_ptr, spu_area_t *dst_area,
                            const spu_render_entry_t *entry, subpicture_region_t *region,
                            const spu_scale_t scale_size,
                            const vlc_fourcc_t *chroma_list,
                            const video_format_t *fmt,
                            int i_original_width, int i_original_height,
                            const spu_area_t *subtitle_area, size_t subtitle_area_count,
                            vlc_tick_t render_date)
{
    subpicture_t *subpic = entry->subpic;
    spu_private_t *sys = spu->p;

    int x_offset;
    int y_offset;

    video_format_t region_fmt;
    picture_t *region_picture;

    /* Invalidate area by default */
    *dst_area = spu_area_create(0,0, 0,0, scale_size);
    *dst_ptr  = NULL;

    /* Render text region */
    if (region->fmt.i_chroma == VLC_CODEC_TEXT)
    {
        if(SpuRenderText(spu, region,
                      i_original_width, i_original_height,
                      chroma_list) != VLC_SUCCESS)
            return;
        assert(region->fmt.i_chroma != VLC_CODEC_TEXT);
    }

    video_format_AdjustColorSpace(&region->fmt);

    /* Force palette if requested
     * FIXME b_force_palette and force_crop are applied to all subpictures using palette
     * instead of only the right one (being the dvd spu).
     */
    const bool using_palette = region->fmt.i_chroma == VLC_CODEC_YUVP;
    const bool force_palette = using_palette && sys->palette.i_entries > 0;
    const bool crop_requested = (force_palette && sys->force_crop) ||
                                region->i_max_width || region->i_max_height;
    bool changed_palette     = false;

    /* Compute the margin which is expressed in destination pixel unit
     * The margin is applied only to subtitle and when no forced crop is
     * requested (dvd menu).
     * Note: Margin will also be applied to secondary subtitles if they exist
     * to ensure that overlap does not occur. */
    int y_margin = 0;
    if (!crop_requested && subpic->b_subtitle)
        y_margin = spu_invscale_h(sys->margin, scale_size);

    /* Place the picture
     * We compute the position in the rendered size */

    int i_align = region->i_align;
    if (entry->channel_order == VLC_VOUT_ORDER_SECONDARY)
        i_align = sys->secondary_alignment >= 0 ? sys->secondary_alignment : i_align;

    SpuRegionPlace(&x_offset, &y_offset,
                   subpic, region, i_align);

    if (entry->channel_order == VLC_VOUT_ORDER_SECONDARY)
    {
        int secondary_margin =
            spu_invscale_h(sys->secondary_margin, scale_size);
        if (!subpic->b_absolute)
        {
            /* Move the secondary subtitles by the secondary margin before
             * overlap detection. This way, overlaps will be resolved if they
             * still exist.  */
            y_offset -= secondary_margin;
        }
        else
        {
            /* Use an absolute margin for secondary subpictures that have
             * already been placed but have been moved by the user */
            y_margin += secondary_margin;
        }
    }

    /* Save this position for subtitle overlap support
     * it is really important that there are given without scale_size applied */
    *dst_area = spu_area_create(x_offset, y_offset,
                                region->fmt.i_visible_width,
                                region->fmt.i_visible_height,
                                scale_size);

    /* Handle overlapping subtitles when possible */
    if (subpic->b_subtitle && !subpic->b_absolute)
        SpuAreaFixOverlap(dst_area, subtitle_area, subtitle_area_count,
                          i_align);

    /* we copy the area: for the subtitle overlap support we want
     * to only save the area without margin applied */
    spu_area_t restrained = *dst_area;

    /* apply margin to subtitles and correct if they go over the picture edge */
    if (subpic->b_subtitle)
        restrained.y -= y_margin;

    spu_area_t display = spu_area_create(0, 0, fmt->i_visible_width,
                                         fmt->i_visible_height,
                                         spu_scale_unit());
    SpuAreaFitInside(&restrained, &display);

    /* Fix the position for the current scale_size */
    x_offset = spu_scale_w(restrained.x, restrained.scale);
    y_offset = spu_scale_h(restrained.y, restrained.scale);

    /* */
    if (force_palette) {
        video_palette_t *old_palette = region->fmt.p_palette;
        video_palette_t new_palette;
        bool b_opaque = false;
        bool b_old_opaque = false;

        /* We suppose DVD palette here */
        new_palette.i_entries = 4;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
                new_palette.palette[i][j] = sys->palette.palette[i][j];
            b_opaque |= (new_palette.palette[i][3] > 0x00);
        }

        if (old_palette->i_entries == new_palette.i_entries) {
            for (int i = 0; i < old_palette->i_entries; i++)
            {
                for (int j = 0; j < 4; j++)
                    changed_palette |= old_palette->palette[i][j] != new_palette.palette[i][j];
                b_old_opaque |= (old_palette->palette[i][3] > 0x00);
            }
        } else {
            changed_palette = true;
            b_old_opaque = true;
        }

        /* Reject or patch fully transparent broken palette used for dvd menus */
        if( !b_opaque )
        {
            if( !b_old_opaque )
            {
                /* replace with new one and fixed alpha */
                old_palette->palette[1][3] = 0x80;
                old_palette->palette[2][3] = 0x80;
                old_palette->palette[3][3] = 0x80;
            }
            /* keep old visible palette */
            else changed_palette = false;
        }

        if( changed_palette )
            *old_palette = new_palette;
    }

    /* */
    region_fmt = region->fmt;
    region_picture = region->p_picture;

    bool convert_chroma = true;
    for (int i = 0; chroma_list[i] && convert_chroma; i++) {
        if (region_fmt.i_chroma == chroma_list[i])
            convert_chroma = false;
    }

    /* Scale from rendered size to destination size */
    if (scale_size.w != SCALE_UNIT || scale_size.h != SCALE_UNIT || convert_chroma)
    {
        const unsigned dst_width  = spu_scale_w(region->fmt.i_visible_width,  scale_size);
        const unsigned dst_height = spu_scale_h(region->fmt.i_visible_height, scale_size);

        /* Destroy the cache if unusable */
        if (region->p_private) {
            subpicture_region_private_t *private = region->p_private;
            bool is_changed = false;

            /* Check resize changes */
            if (dst_width  != private->fmt.i_visible_width ||
                dst_height != private->fmt.i_visible_height)
                is_changed = true;

            /* Check forced palette changes */
            if (changed_palette)
                is_changed = true;

            if (convert_chroma && private->fmt.i_chroma != chroma_list[0])
                is_changed = true;

            if (is_changed) {
                subpicture_region_private_Delete(private);
                region->p_private = NULL;
            }
        }

        /* Scale if needed into cache */
        if (!region->p_private && dst_width > 0 && dst_height > 0) {
            filter_t *scale = sys->scale;

            picture_t *picture = region->p_picture;
            picture_Hold(picture);

            /* Convert YUVP to YUVA/RGBA first for better scaling quality */
            if (using_palette) {
                filter_t *scale_yuvp = sys->scale_yuvp;

                scale_yuvp->fmt_in.video = region->fmt;

                scale_yuvp->fmt_out.video = region->fmt;
                scale_yuvp->fmt_out.video.i_chroma = chroma_list[0];

                picture = scale_yuvp->pf_video_filter(scale_yuvp, picture);
                if (!picture) {
                    /* Well we will try conversion+scaling */
                    msg_Warn(spu, "%4.4s to %4.4s conversion failed",
                             (const char*)&scale_yuvp->fmt_in.video.i_chroma,
                             (const char*)&scale_yuvp->fmt_out.video.i_chroma);
                }
            }

            /* Conversion(except from YUVP)/Scaling */
            if (picture &&
                (picture->format.i_visible_width  != dst_width ||
                 picture->format.i_visible_height != dst_height ||
                 (convert_chroma && !using_palette)))
            {
                scale->fmt_in.video  = picture->format;
                scale->fmt_out.video = picture->format;
                if (using_palette)
                    scale->fmt_in.video.i_chroma = chroma_list[0];
                if (convert_chroma)
                    scale->fmt_out.i_codec        =
                    scale->fmt_out.video.i_chroma = chroma_list[0];

                scale->fmt_out.video.i_width  = dst_width;
                scale->fmt_out.video.i_height = dst_height;

                scale->fmt_out.video.i_visible_width =
                    spu_scale_w(region->fmt.i_visible_width, scale_size);
                scale->fmt_out.video.i_visible_height =
                    spu_scale_h(region->fmt.i_visible_height, scale_size);

                picture = scale->pf_video_filter(scale, picture);
                if (!picture)
                    msg_Err(spu, "scaling failed");
            }

            /* */
            if (picture) {
                region->p_private = subpicture_region_private_New(&picture->format);
                if (region->p_private) {
                    region->p_private->p_picture = picture;
                    if (!region->p_private->p_picture) {
                        subpicture_region_private_Delete(region->p_private);
                        region->p_private = NULL;
                    }
                } else {
                    picture_Release(picture);
                }
            }
        }

        /* And use the scaled picture */
        if (region->p_private) {
            region_fmt     = region->p_private->fmt;
            region_picture = region->p_private->p_picture;
        }
    }

    /* Force cropping if requested */
    if (crop_requested) {
        int crop_x, crop_y, crop_width, crop_height;
        if(sys->force_crop){
            crop_x     = spu_scale_w(sys->crop.x, scale_size);
            crop_y     = spu_scale_h(sys->crop.y, scale_size);
            crop_width = spu_scale_w(sys->crop.width,  scale_size);
            crop_height= spu_scale_h(sys->crop.height, scale_size);
        }
        else
        {
            crop_x = x_offset;
            crop_y = y_offset;
            crop_width = region_fmt.i_visible_width;
            crop_height = region_fmt.i_visible_height;
        }

        if(region->i_max_width && spu_scale_w(region->i_max_width, scale_size) < crop_width)
            crop_width = spu_scale_w(region->i_max_width, scale_size);

        if(region->i_max_height && spu_scale_h(region->i_max_height, scale_size) < crop_height)
            crop_height = spu_scale_h(region->i_max_height, scale_size);

        /* Find the intersection */
        if (crop_x + crop_width <= x_offset ||
            x_offset + (int)region_fmt.i_visible_width  < crop_x ||
            crop_y + crop_height <= y_offset ||
            y_offset + (int)region_fmt.i_visible_height < crop_y) {
            /* No intersection */
            region_fmt.i_visible_width  =
            region_fmt.i_visible_height = 0;
        } else {
            int x, y, x_end, y_end;
            x = __MAX(crop_x, x_offset);
            y = __MAX(crop_y, y_offset);
            x_end = __MIN(crop_x + crop_width,
                          x_offset + (int)region_fmt.i_visible_width);
            y_end = __MIN(crop_y + crop_height,
                          y_offset + (int)region_fmt.i_visible_height);

            region_fmt.i_x_offset       = x - x_offset;
            region_fmt.i_y_offset       = y - y_offset;
            region_fmt.i_visible_width  = x_end - x;
            region_fmt.i_visible_height = y_end - y;

            x_offset = __MAX(x, 0);
            y_offset = __MAX(y, 0);
        }
    }

    subpicture_region_t *dst = *dst_ptr = subpicture_region_NewInternal(&region_fmt);
    if (dst) {
        dst->i_x       = x_offset;
        dst->i_y       = y_offset;
        dst->i_align   = 0;
        assert(!dst->p_picture);
        dst->p_picture = picture_Hold(region_picture);
        int fade_alpha = 255;
        if (subpic->b_fade) {
            vlc_tick_t fade_start = subpic->i_start + 3 * (subpic->i_stop - subpic->i_start) / 4;

            if (fade_start <= render_date && fade_start < subpic->i_stop)
                fade_alpha = 255 * (subpic->i_stop - render_date) /
                                   (subpic->i_stop - fade_start);
        }
        dst->i_alpha   = fade_alpha * subpic->i_alpha * region->i_alpha / 65025;
    }
}

/**
 * This function renders all sub picture units in the list.
 */
static subpicture_t *SpuRenderSubpictures(spu_t *spu,
                                          size_t i_subpicture,
                                          const spu_render_entry_t *p_entries,
                                          const vlc_fourcc_t *chroma_list,
                                          const video_format_t *fmt_dst,
                                          const video_format_t *fmt_src,
                                          vlc_tick_t system_now,
                                          vlc_tick_t render_subtitle_date,
                                          bool external_scale)
{
    /* Count the number of regions and subtitle regions */
    unsigned int subtitle_region_count = 0;
    unsigned int region_count          = 0;
    for (unsigned i = 0; i < i_subpicture; i++) {
        const subpicture_t *subpic = p_entries[i].subpic;

        unsigned count = 0;
        for (subpicture_region_t *r = subpic->p_region; r != NULL; r = r->p_next)
            count++;

        if (subpic->b_subtitle)
            subtitle_region_count += count;
        region_count += count;
    }
    if (region_count <= 0)
        return NULL;

    /* Create the output subpicture */
    subpicture_t *output = subpicture_New(NULL);
    if (!output)
        return NULL;
    output->i_order = p_entries[i_subpicture - 1].subpic->i_order;
    output->i_original_picture_width  = fmt_dst->i_visible_width;
    output->i_original_picture_height = fmt_dst->i_visible_height;
    subpicture_region_t **output_last_ptr = &output->p_region;

    /* Allocate area array for subtitle overlap */
    spu_area_t subtitle_area_buffer[100];
    spu_area_t *subtitle_area;
    size_t subtitle_area_count = 0;

    subtitle_area = subtitle_area_buffer;
    if (subtitle_region_count > ARRAY_SIZE(subtitle_area_buffer))
        subtitle_area = calloc(subtitle_region_count, sizeof(*subtitle_area));

    /* Process all subpictures and regions (in the right order) */
    for (size_t index = 0; index < i_subpicture; index++) {
        const spu_render_entry_t *entry = &p_entries[index];
        subpicture_t *subpic = entry->subpic;
        subpicture_region_t *region;

        if (!subpic->p_region)
            continue;

        if (subpic->i_original_picture_width  <= 0 ||
            subpic->i_original_picture_height <= 0) {
            if (subpic->i_original_picture_width  > 0 ||
                subpic->i_original_picture_height > 0)
                msg_Err(spu, "original picture size %dx%d is unsupported",
                         subpic->i_original_picture_width,
                         subpic->i_original_picture_height);
            else
                msg_Warn(spu, "original picture size is undefined");

            subpic->i_original_picture_width  = fmt_src->i_visible_width;
            subpic->i_original_picture_height = fmt_src->i_visible_height;
        }

        const int i_original_width = subpic->i_original_picture_width;
        const int i_original_height = subpic->i_original_picture_height;

        /* Render all regions
         * We always transform non absolute subtitle into absolute one on the
         * first rendering to allow good subtitle overlap support.
         */
        for (region = subpic->p_region; region != NULL; region = region->p_next) {
            spu_area_t area;

            /* Compute region scale AR */
            video_format_t region_fmt = region->fmt;
            if (region_fmt.i_sar_num <= 0 || region_fmt.i_sar_den <= 0) {

                const uint64_t i_sar_num = (uint64_t)fmt_dst->i_visible_width  *
                                           fmt_dst->i_sar_num * subpic->i_original_picture_height;
                const uint64_t i_sar_den = (uint64_t)fmt_dst->i_visible_height *
                                           fmt_dst->i_sar_den * subpic->i_original_picture_width;

                vlc_ureduce(&region_fmt.i_sar_num, &region_fmt.i_sar_den,
                            i_sar_num, i_sar_den, 65536);
            }

            /* Compute scaling from original size to destination size
             * FIXME The current scaling ensure that the heights match, the width being
             * cropped.
             */
            spu_scale_t scale = spu_scale_createq((int64_t)fmt_dst->i_visible_height                 * fmt_dst->i_sar_den * region_fmt.i_sar_num,
                                                  (int64_t)subpic->i_original_picture_height * fmt_dst->i_sar_num * region_fmt.i_sar_den,
                                                  fmt_dst->i_visible_height,
                                                  subpic->i_original_picture_height);

            /* Check scale validity */
            if (scale.w <= 0 || scale.h <= 0)
                continue;

            const bool do_external_scale = external_scale && region->fmt.i_chroma != VLC_CODEC_TEXT;
            spu_scale_t virtual_scale = external_scale ? (spu_scale_t){ SCALE_UNIT, SCALE_UNIT } : scale;

            /* */
            SpuRenderRegion(spu, output_last_ptr, &area,
                            entry, region, virtual_scale,
                            chroma_list, fmt_dst,
                            i_original_width, i_original_height,
                            subtitle_area, subtitle_area_count,
                            subpic->b_subtitle ? render_subtitle_date : system_now);
            if (*output_last_ptr)
            {
                if (do_external_scale)
                {
                    if (scale.h != SCALE_UNIT)
                    {
                        (*output_last_ptr)->zoom_h.num = scale.h;
                        (*output_last_ptr)->zoom_h.den = SCALE_UNIT;
                    }
                    if (scale.w != SCALE_UNIT)
                    {
                        (*output_last_ptr)->zoom_v.num = scale.w;
                        (*output_last_ptr)->zoom_v.den = SCALE_UNIT;
                    }
                }

                output_last_ptr = &(*output_last_ptr)->p_next;
            }

            if (subpic->b_subtitle) {
                area = spu_area_unscaled(area, scale);
                if (!subpic->b_absolute && area.width > 0 && area.height > 0) {
                    region->i_x = area.x;
                    region->i_y = area.y;
                }
                if (subtitle_area)
                    subtitle_area[subtitle_area_count++] = area;
            }
        }
        if (subpic->b_subtitle && subpic->p_region)
            subpic->b_absolute = true;
    }

    /* */
    if (subtitle_area != subtitle_area_buffer)
        free(subtitle_area);

    return output;
}

/*****************************************************************************
 * Object variables callbacks
 *****************************************************************************/

/*****************************************************************************
 * UpdateSPU: update subpicture settings
 *****************************************************************************/
static void UpdateSPU(spu_t *spu, const vlc_spu_highlight_t *hl)
{
    spu_private_t *sys = spu->p;

    vlc_mutex_assert(&sys->lock);

    sys->palette.i_entries = 0;
    sys->force_crop = false;

    if (hl == NULL)
        return;

    sys->force_crop = true;
    sys->crop.x      = hl->x_start;
    sys->crop.y      = hl->y_start;
    sys->crop.width  = hl->x_end - sys->crop.x;
    sys->crop.height = hl->y_end - sys->crop.y;

    if (hl->palette.i_entries == 4) /* XXX: Only DVD palette for now */
        memcpy(&sys->palette, &hl->palette, sizeof(sys->palette));

    msg_Dbg(spu, "crop: %i,%i,%i,%i, palette forced: %i",
            sys->crop.x, sys->crop.y,
            sys->crop.width, sys->crop.height,
            sys->palette.i_entries);
}

/*****************************************************************************
 * Buffers allocation callbacks for the filters
 *****************************************************************************/

static subpicture_t *sub_new_buffer(filter_t *filter)
{
    ssize_t channel = *(ssize_t *)filter->owner.sys;

    subpicture_t *subpicture = subpicture_New(NULL);
    if (subpicture)
        subpicture->i_channel = channel;
    return subpicture;
}

static const struct filter_subpicture_callbacks sub_cbs = {
    sub_new_buffer,
};

static int SubSourceInit(filter_t *filter, void *data)
{
    spu_t *spu = data;
    ssize_t *channel = malloc(sizeof (ssize_t));
    if (unlikely(channel == NULL))
        return VLC_ENOMEM;

    *channel = spu_RegisterChannel(spu);
    filter->owner.sys = channel;
    filter->owner.sub = &sub_cbs;
    return VLC_SUCCESS;
}

static int SubSourceClean(filter_t *filter, void *data)
{
    spu_t *spu = data;
    ssize_t *channel = filter->owner.sys;

    spu_ClearChannel(spu, *channel);
    free(channel);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Proxy callbacks
 *****************************************************************************/

static int RestartSubFilterCallback(vlc_object_t *obj, char const *psz_var,
                                    vlc_value_t oldval, vlc_value_t newval,
                                    void *p_data)
{ VLC_UNUSED(obj); VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(newval);
    vout_ControlChangeSubFilters((vout_thread_t *)p_data, NULL);
    return VLC_SUCCESS;
}

static int SubFilterAddProxyCallbacks(filter_t *filter, void *opaque)
{
    filter_AddProxyCallbacks((vlc_object_t *)opaque, filter,
                             RestartSubFilterCallback);
    return VLC_SUCCESS;
}

static int SubFilterDelProxyCallbacks(filter_t *filter, void *opaque)
{
    filter_DelProxyCallbacks((vlc_object_t *)opaque, filter,
                             RestartSubFilterCallback);
    return VLC_SUCCESS;
}

static int RestartSubSourceCallback(vlc_object_t *obj, char const *psz_var,
                                    vlc_value_t oldval, vlc_value_t newval,
                                    void *p_data)
{ VLC_UNUSED(obj); VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(newval);
    vout_ControlChangeSubSources((vout_thread_t *)p_data, NULL);
    return VLC_SUCCESS;
}

static int SubSourceAddProxyCallbacks(filter_t *filter, void *opaque)
{
    filter_AddProxyCallbacks((vlc_object_t *)opaque, filter,
                             RestartSubSourceCallback);
    return VLC_SUCCESS;
}

static int SubSourceDelProxyCallbacks(filter_t *filter, void *opaque)
{
    filter_DelProxyCallbacks((vlc_object_t *)opaque, filter,
                             RestartSubSourceCallback);
    return VLC_SUCCESS;
}

static void spu_PrerenderWake(spu_private_t *sys,
                              const video_format_t *fmt_dst,
                              const video_format_t *fmt_src,
                              const vlc_fourcc_t *chroma_list)
{
    vlc_mutex_lock(&sys->prerender.lock);
    if(!video_format_IsSimilar(fmt_dst, &sys->prerender.fmtdst))
    {
        video_format_Clean(&sys->prerender.fmtdst);
        video_format_Copy(&sys->prerender.fmtdst, fmt_dst);
    }
    if(!video_format_IsSimilar(fmt_src, &sys->prerender.fmtsrc))
    {
        video_format_Clean(&sys->prerender.fmtsrc);
        video_format_Copy(&sys->prerender.fmtsrc, fmt_src);
    }

    for(size_t i=0; i<SPU_CHROMALIST_COUNT; i++)
    {
        sys->prerender.chroma_list[i] = chroma_list[i];
        if(!chroma_list[i])
            break;
    }

    vlc_cond_signal(&sys->prerender.cond);
    vlc_mutex_unlock(&sys->prerender.lock);
}

static void spu_PrerenderEnqueue(spu_private_t *sys, subpicture_t *p_subpic)
{
    vlc_mutex_lock(&sys->prerender.lock);
    vlc_vector_push(&sys->prerender.vector, p_subpic);
    vlc_cond_signal(&sys->prerender.cond);
    vlc_mutex_unlock(&sys->prerender.lock);
}

static void spu_PrerenderCancel(spu_private_t *sys, const subpicture_t *p_subpic)
{
    vlc_mutex_lock(&sys->prerender.lock);
    ssize_t i_idx;
    vlc_vector_index_of(&sys->prerender.vector, p_subpic, &i_idx);
    if(i_idx >= 0)
        vlc_vector_remove(&sys->prerender.vector, i_idx);
    else while(sys->prerender.p_processed == p_subpic)
        vlc_cond_wait(&sys->prerender.output_cond, &sys->prerender.lock);
    vlc_mutex_unlock(&sys->prerender.lock);
}

static void spu_PrerenderPause(spu_private_t *sys)
{
    vlc_mutex_lock(&sys->prerender.lock);
    while(sys->prerender.p_processed)
        vlc_cond_wait(&sys->prerender.output_cond, &sys->prerender.lock);
    sys->prerender.chroma_list[0] = 0;
    vlc_mutex_unlock(&sys->prerender.lock);
}

static void spu_PrerenderSync(spu_private_t *sys, const subpicture_t *p_subpic)
{
    vlc_mutex_lock(&sys->prerender.lock);
    ssize_t i_idx;
    vlc_vector_index_of(&sys->prerender.vector, p_subpic, &i_idx);
    while(i_idx >= 0 || sys->prerender.p_processed == p_subpic)
    {
        vlc_cond_wait(&sys->prerender.output_cond, &sys->prerender.lock);
        vlc_vector_index_of(&sys->prerender.vector, p_subpic, &i_idx);
    }
    vlc_mutex_unlock(&sys->prerender.lock);
}

static void spu_PrerenderText(spu_t *spu, subpicture_t *p_subpic,
                              video_format_t *fmtsrc, video_format_t *fmtdst,
                              vlc_fourcc_t *chroma_list)
{
    if (p_subpic->i_original_picture_width  <= 0 ||
        p_subpic->i_original_picture_height <= 0) {
        if (p_subpic->i_original_picture_width  > 0 ||
            p_subpic->i_original_picture_height > 0)
            msg_Err(spu, "original picture size %dx%d is unsupported",
                     p_subpic->i_original_picture_width,
                     p_subpic->i_original_picture_height);
        else
            msg_Warn(spu, "original picture size is undefined");

        p_subpic->i_original_picture_width  = fmtsrc->i_visible_width;
        p_subpic->i_original_picture_height = fmtsrc->i_visible_height;
    }


    subpicture_Update(p_subpic, fmtsrc, fmtdst,
                      p_subpic->b_subtitle ? p_subpic->i_start : vlc_tick_now());

    const int i_original_picture_width = p_subpic->i_original_picture_width;
    const int i_original_picture_height = p_subpic->i_original_picture_height;

    subpicture_region_t *region;
    for (region = p_subpic->p_region; region != NULL; region = region->p_next)
    {
        if(region->fmt.i_chroma != VLC_CODEC_TEXT)
            continue;
        SpuRenderText(spu, region,
                      i_original_picture_width, i_original_picture_height,
                      chroma_list);
    }
}

static void * spu_PrerenderThread(void *priv)
{
    spu_t *spu = priv;
    spu_private_t *sys = spu->p;
    vlc_fourcc_t chroma_list[SPU_CHROMALIST_COUNT+1];

    chroma_list[SPU_CHROMALIST_COUNT] = 0;

    vlc_mutex_lock(&sys->prerender.lock);
    while (sys->prerender.live)
    {
        video_format_t fmtsrc;
        video_format_t fmtdst;

        if (!sys->prerender.vector.size || !sys->prerender.chroma_list[0])
        {
            vlc_cond_wait(&sys->prerender.cond, &sys->prerender.lock);
            continue;
        }

        size_t i_idx = 0;
        sys->prerender.p_processed = sys->prerender.vector.data[0];
        for(size_t i=1; i<sys->prerender.vector.size; i++)
        {
             if(sys->prerender.p_processed->i_start > sys->prerender.vector.data[i]->i_start)
             {
                 sys->prerender.p_processed = sys->prerender.vector.data[i];
                 i_idx = i;
             }
        }
        vlc_vector_remove(&sys->prerender.vector, i_idx);
        memcpy(chroma_list, sys->prerender.chroma_list, SPU_CHROMALIST_COUNT);
        video_format_Copy(&fmtdst, &sys->prerender.fmtdst);
        video_format_Copy(&fmtsrc, &sys->prerender.fmtsrc);

        vlc_mutex_unlock(&sys->prerender.lock);

        spu_PrerenderText(spu, sys->prerender.p_processed,
                          &fmtsrc, &fmtdst, chroma_list);

        video_format_Clean(&fmtdst);
        video_format_Clean(&fmtsrc);

        vlc_mutex_lock(&sys->prerender.lock);
        sys->prerender.p_processed = NULL;
        vlc_cond_signal(&sys->prerender.output_cond);
    }

    vlc_mutex_unlock(&sys->prerender.lock);
    return NULL;
}

/*****************************************************************************
 * Public API
 *****************************************************************************/

static void spu_Cleanup(spu_t *spu)
{
    spu_private_t *sys = spu->p;

    if (sys->text)
        FilterRelease(sys->text);

    if (sys->scale_yuvp)
        FilterRelease(sys->scale_yuvp);

    if (sys->scale)
        FilterRelease(sys->scale);

    filter_chain_ForEach(sys->source_chain, SubSourceClean, spu);
    if (sys->vout)
        filter_chain_ForEach(sys->source_chain,
                             SubSourceDelProxyCallbacks, sys->vout);
    filter_chain_Delete(sys->source_chain);
    free(sys->source_chain_current);
    if (sys->vout)
        filter_chain_ForEach(sys->filter_chain,
                             SubFilterDelProxyCallbacks, sys->vout);
    filter_chain_Delete(sys->filter_chain);
    free(sys->filter_chain_current);
    free(sys->source_chain_update);
    free(sys->filter_chain_update);

    /* Destroy all remaining subpictures */
    for (size_t i = 0; i < sys->channels.size; ++i)
        spu_channel_Clean(sys, &sys->channels.data[i]);

    vlc_vector_destroy(&sys->channels);

    vlc_vector_clear(&sys->prerender.vector);
    video_format_Clean(&sys->prerender.fmtdst);
    video_format_Clean(&sys->prerender.fmtsrc);
}

/**
 * Destroy the subpicture unit
 *
 * \param p_this the parent object which destroys the subpicture unit
 */
void spu_Destroy(spu_t *spu)
{
    spu_private_t *sys = spu->p;
    /* stop prerendering */
    vlc_mutex_lock(&sys->prerender.lock);
    sys->prerender.live = false;
    vlc_cond_signal(&sys->prerender.cond);
    vlc_mutex_unlock(&sys->prerender.lock);
    vlc_join(sys->prerender.thread, NULL);
    /* delete filters and free resources */
    spu_Cleanup(spu);
    vlc_object_delete(spu);
}

#undef spu_Create
/**
 * Creates the subpicture unit
 *
 * \param p_this the parent object which creates the subpicture unit
 */
spu_t *spu_Create(vlc_object_t *object, vout_thread_t *vout)
{
    spu_t *spu = vlc_custom_create(object,
                                   sizeof(spu_t) + sizeof(spu_private_t),
                                   "subpicture");
    if (!spu)
        return NULL;

    /* Initialize spu fields */
    spu_private_t *sys = spu->p = (spu_private_t*)&spu[1];

    vlc_vector_init(&sys->channels);
    if (!vlc_vector_reserve(&sys->channels, VOUT_SPU_CHANNEL_OSD_COUNT))
    {
        vlc_object_delete(spu);
        return NULL;
    }
    for (size_t i = 0; i < VOUT_SPU_CHANNEL_OSD_COUNT; ++i)
    {
        struct spu_channel channel;
        spu_channel_Init(&channel, i, VLC_VOUT_ORDER_PRIMARY, NULL);
        vlc_vector_push(&sys->channels, channel);
    }

    /* Initialize private fields */
    vlc_mutex_init(&sys->lock);

    sys->margin = var_InheritInteger(spu, "sub-margin");
    sys->secondary_margin = var_InheritInteger(spu, "secondary-sub-margin");

    sys->secondary_alignment = var_InheritInteger(spu,
                                                  "secondary-sub-alignment");

    sys->source_chain_update = NULL;
    sys->filter_chain_update = NULL;
    vlc_mutex_init(&sys->filter_chain_lock);
    sys->source_chain = filter_chain_NewSPU(spu, "sub source");
    sys->filter_chain = filter_chain_NewSPU(spu, "sub filter");

    vlc_mutex_init(&sys->prerender.lock);
    vlc_cond_init(&sys->prerender.cond);
    vlc_cond_init(&sys->prerender.output_cond);
    vlc_vector_init(&sys->prerender.vector);
    video_format_Init(&sys->prerender.fmtdst, 0);
    video_format_Init(&sys->prerender.fmtsrc, 0);
    sys->prerender.p_processed = NULL;
    sys->prerender.chroma_list[0] = 0;
    sys->prerender.chroma_list[SPU_CHROMALIST_COUNT] = 0;
    sys->prerender.live = true;

    /* Load text and scale module */
    sys->text = SpuRenderCreateAndLoadText(spu);
    vlc_mutex_init(&sys->textlock);

    /* XXX spu->p_scale is used for all conversion/scaling except yuvp to
     * yuva/rgba */
    sys->scale = SpuRenderCreateAndLoadScale(VLC_OBJECT(spu),
                                             VLC_CODEC_YUVA, VLC_CODEC_RGBA, true);

    /* This one is used for YUVP to YUVA/RGBA without scaling
     * FIXME rename it */
    sys->scale_yuvp = SpuRenderCreateAndLoadScale(VLC_OBJECT(spu),
                                                  VLC_CODEC_YUVP, VLC_CODEC_YUVA, false);


    if (!sys->source_chain || !sys->filter_chain || !sys->text || !sys->scale
     || !sys->scale_yuvp)
    {
        sys->vout = NULL;
        spu_Cleanup(spu);
        vlc_object_delete(spu);
        return NULL;
    }
    /* */
    sys->last_sort_date = -1;
    sys->vout = vout;

    if(vlc_clone(&sys->prerender.thread, spu_PrerenderThread, spu, VLC_THREAD_PRIORITY_VIDEO))
    {
        spu_Cleanup(spu);
        vlc_object_delete(spu);
        spu = NULL;
    }

    return spu;
}

/**
 * Attach the SPU to an input
 */
void spu_Attach(spu_t *spu, input_thread_t *input)
{
    vlc_mutex_lock(&spu->p->lock);
    if (spu->p->input != input) {
        UpdateSPU(spu, NULL);

        spu->p->input = input;

        vlc_mutex_lock(&spu->p->textlock);
        if (spu->p->text)
            FilterRelease(spu->p->text);
        spu->p->text = SpuRenderCreateAndLoadText(spu);
        vlc_mutex_unlock(&spu->p->textlock);
    }
    vlc_mutex_unlock(&spu->p->lock);
}

/**
 * Detach the SPU from its attached input
 */
void spu_Detach(spu_t *spu)
{
    vlc_mutex_lock(&spu->p->lock);
    spu_PrerenderPause(spu->p);
    spu->p->input = NULL;
    vlc_mutex_unlock(&spu->p->lock);
}

void spu_SetClockDelay(spu_t *spu, size_t channel_id, vlc_tick_t delay)
{
    spu_private_t *sys = spu->p;

    vlc_mutex_lock(&sys->lock);
    struct spu_channel *channel = spu_GetChannel(spu, channel_id);
    assert(channel->clock);
    vlc_clock_SetDelay(channel->clock, delay);
    channel->delay = delay;
    vlc_mutex_unlock(&sys->lock);
}

void spu_SetClockRate(spu_t *spu, size_t channel_id, float rate)
{
    spu_private_t *sys = spu->p;

    vlc_mutex_lock(&sys->lock);
    struct spu_channel *channel = spu_GetChannel(spu, channel_id);
    assert(channel->clock);
    channel->rate = rate;
    vlc_mutex_unlock(&sys->lock);
}

/**
 * Display a subpicture
 *
 * Remove the reservation flag of a subpicture, which will cause it to be
 * ready for display.
 * \param spu the subpicture unit object
 * \param subpic the subpicture to display
 */
void spu_PutSubpicture(spu_t *spu, subpicture_t *subpic)
{
    spu_private_t *sys = spu->p;

    /* Update sub-filter chain */
    vlc_mutex_lock(&sys->lock);
    char *chain_update = sys->filter_chain_update;
    sys->filter_chain_update = NULL;
    vlc_mutex_unlock(&sys->lock);

    bool is_left_empty = false;

    vlc_mutex_lock(&sys->filter_chain_lock);
    if (chain_update) {
        if (*chain_update) {
            if (sys->vout)
                filter_chain_ForEach(sys->filter_chain,
                                     SubFilterDelProxyCallbacks,
                                     sys->vout);
            filter_chain_Clear(sys->filter_chain);

            filter_chain_AppendFromString(spu->p->filter_chain, chain_update);
            if (sys->vout)
                filter_chain_ForEach(sys->filter_chain,
                                     SubFilterAddProxyCallbacks,
                                     sys->vout);
        }
        else
            filter_chain_Clear(sys->filter_chain);

        /* "sub-source"  was formerly "sub-filter", so now the "sub-filter"
        configuration may contain sub-filters or sub-sources configurations.
        if the filters chain was left empty it may indicate that it's a sub-source configuration */
        is_left_empty = filter_chain_IsEmpty(spu->p->filter_chain);
    }
    vlc_mutex_unlock(&sys->filter_chain_lock);

    if (is_left_empty) {
        /* try to use the configuration as a sub-source configuration,
           but only if there is no 'source_chain_update' value and
           if only if 'chain_update' has a value */
        if (chain_update && *chain_update) {
            vlc_mutex_lock(&sys->lock);
            if (!sys->source_chain_update || !*sys->source_chain_update) {
                free(sys->source_chain_update);
                sys->source_chain_update = chain_update;
                sys->source_chain_current = strdup(chain_update);
                chain_update = NULL;
            }
            vlc_mutex_unlock(&sys->lock);
        }
    }

    free(chain_update);

    /* Run filter chain on the new subpicture */
    vlc_mutex_lock(&sys->filter_chain_lock);
    subpic = filter_chain_SubFilter(spu->p->filter_chain, subpic);
    vlc_mutex_unlock(&sys->filter_chain_lock);
    if (!subpic || subpic->i_channel < 0)
    {
        if (subpic)
            subpicture_Delete(subpic);
        return;
    }

    /* SPU_DEFAULT_CHANNEL always reset itself */
    if (subpic->i_channel == VOUT_SPU_CHANNEL_OSD)
        spu_ClearChannel(spu, VOUT_SPU_CHANNEL_OSD);

    /* p_private is for spu only and cannot be non NULL here */
    for (subpicture_region_t *r = subpic->p_region; r != NULL; r = r->p_next)
        assert(r->p_private == NULL);

    /* */
    vlc_mutex_lock(&sys->lock);
    struct spu_channel *channel = spu_GetChannel(spu, subpic->i_channel);


    /* Convert all spu ts */
    vlc_tick_t orgstart = subpic->i_start;
    vlc_tick_t orgstop = subpic->i_stop;
    if (channel->clock)
    {
        vlc_tick_t system_now = vlc_tick_now();
        vlc_tick_t times[2] = { orgstart, orgstop };
        vlc_clock_ConvertArrayToSystem(channel->clock, system_now,
                                       times, 2, channel->rate);
        subpic->i_start = times[0];
        subpic->i_stop = times[1];
    }

    if (spu_channel_Push(channel, subpic, orgstart, orgstop))
    {
        vlc_mutex_unlock(&sys->lock);
        msg_Err(spu, "subpicture heap full");
        subpicture_Delete(subpic);
        return;
    }
    spu_PrerenderEnqueue(sys, subpic);
    vlc_mutex_unlock(&sys->lock);
}

subpicture_t *spu_Render(spu_t *spu,
                         const vlc_fourcc_t *chroma_list,
                         const video_format_t *fmt_dst,
                         const video_format_t *fmt_src,
                         vlc_tick_t system_now,
                         vlc_tick_t render_subtitle_date,
                         bool ignore_osd,
                         bool external_scale)
{
    spu_private_t *sys = spu->p;

    /* Update sub-source chain */
    vlc_mutex_lock(&sys->lock);
    char *chain_update = sys->source_chain_update;
    sys->source_chain_update = NULL;
    vlc_mutex_unlock(&sys->lock);

    if (chain_update) {
        filter_chain_ForEach(sys->source_chain, SubSourceClean, spu);
            if (sys->vout)
                filter_chain_ForEach(sys->source_chain,
                                     SubSourceDelProxyCallbacks,
                                     sys->vout);
        filter_chain_Clear(sys->source_chain);

        filter_chain_AppendFromString(spu->p->source_chain, chain_update);
        if (sys->vout)
            filter_chain_ForEach(sys->source_chain,
                                 SubSourceAddProxyCallbacks, sys->vout);
        filter_chain_ForEach(sys->source_chain, SubSourceInit, spu);

        free(chain_update);
    }
    /* Run subpicture sources */
    filter_chain_SubSource(sys->source_chain, spu, system_now);

    static const vlc_fourcc_t chroma_list_default_yuv[] = {
        VLC_CODEC_YUVA,
        VLC_CODEC_RGBA,
        VLC_CODEC_ARGB,
        VLC_CODEC_BGRA,
        VLC_CODEC_YUVP,
        0,
    };
    static const vlc_fourcc_t chroma_list_default_rgb[] = {
        VLC_CODEC_RGBA,
        VLC_CODEC_ARGB,
        VLC_CODEC_BGRA,
        VLC_CODEC_YUVA,
        VLC_CODEC_YUVP,
        0,
    };

    if (!chroma_list || *chroma_list == 0)
        chroma_list = vlc_fourcc_IsYUV(fmt_dst->i_chroma) ? chroma_list_default_yuv
                                                          : chroma_list_default_rgb;

    /* wake up prerenderer, we have some video size and chroma */
    spu_PrerenderWake(sys, fmt_dst, fmt_src, chroma_list);

    vlc_mutex_lock(&sys->lock);

    size_t subpicture_count;

    /* Get an array of subpictures to render */
    spu_render_entry_t *subpicture_array =
        spu_SelectSubpictures(spu, system_now, render_subtitle_date,
                             ignore_osd, &subpicture_count);
    if (!subpicture_array)
    {
        vlc_mutex_unlock(&sys->lock);
        return NULL;
    }

    /* Updates the subpictures */
    for (size_t i = 0; i < subpicture_count; i++) {
        spu_render_entry_t *entry = &subpicture_array[i];
        subpicture_t *subpic = entry->subpic;

        spu_PrerenderSync(sys, entry->subpic);

        /* Update time to clock */
        entry->subpic->i_start = entry->start;
        entry->subpic->i_stop = entry->stop;

        if (!subpic->updater.pf_validate)
            continue;

        subpicture_Update(subpic,
                          fmt_src, fmt_dst,
                          subpic->b_subtitle ? render_subtitle_date : system_now);
    }

    /* Now order the subpicture array
     * XXX The order is *really* important for overlap subtitles positionning */
    qsort(subpicture_array, subpicture_count, sizeof(*subpicture_array), SpuRenderCmp);

    /* Render the subpictures */
    subpicture_t *render = SpuRenderSubpictures(spu,
                                                subpicture_count, subpicture_array,
                                                chroma_list,
                                                fmt_dst,
                                                fmt_src,
                                                system_now,
                                                render_subtitle_date,
                                                external_scale);
    free(subpicture_array);
    vlc_mutex_unlock(&sys->lock);

    return render;
}

ssize_t spu_RegisterChannelInternal(spu_t *spu, vlc_clock_t *clock,
                                    enum vlc_vout_order *order)
{
    spu_private_t *sys = spu->p;

    vlc_mutex_lock(&sys->lock);

    ssize_t channel_id = spu_GetFreeChannelId(spu, order);

    if (channel_id != VOUT_SPU_CHANNEL_INVALID)
    {
        struct spu_channel channel;
        spu_channel_Init(&channel, channel_id,
                         order ? *order : VLC_VOUT_ORDER_PRIMARY, clock);
        if (vlc_vector_push(&sys->channels, channel))
        {
            vlc_mutex_unlock(&sys->lock);
            return channel_id;
        }
    }

    vlc_mutex_unlock(&sys->lock);

    return VOUT_SPU_CHANNEL_INVALID;
}

ssize_t spu_RegisterChannel(spu_t *spu)
{
    /* Public call, order is always primary (used for OSD or dvd/bluray spus) */
    return spu_RegisterChannelInternal(spu, NULL, NULL);
}

static void spu_channel_Clear(spu_private_t *sys,
                              struct spu_channel *channel)
{
    for (size_t i = 0; i < channel->entries.size; i++)
    {
        spu_PrerenderCancel(sys, channel->entries.data[i].subpic);
        spu_channel_DeleteAt(channel, i);
    }
}

void spu_ClearChannel(spu_t *spu, size_t channel_id)
{
    spu_private_t *sys = spu->p;
    vlc_mutex_lock(&sys->lock);
    struct spu_channel *channel = spu_GetChannel(spu, channel_id);
    spu_channel_Clear(sys, channel);
    if (channel->clock)
    {
        vlc_clock_Reset(channel->clock);
        vlc_clock_SetDelay(channel->clock, channel->delay);
    }
    vlc_mutex_unlock(&sys->lock);
}

void spu_UnregisterChannel(spu_t *spu, size_t channel_id)
{
    spu_private_t *sys = spu->p;

    vlc_mutex_lock(&sys->lock);
    struct spu_channel *channel = spu_GetChannel(spu, channel_id);
    spu_channel_Clean(sys, channel);
    vlc_vector_remove(&sys->channels, channel_id);
    vlc_mutex_unlock(&sys->lock);
}

void spu_ChangeSources(spu_t *spu, const char *filters)
{
    spu_private_t *sys = spu->p;

    vlc_mutex_lock(&sys->lock);

    free(sys->source_chain_update);
    if (filters)
    {
        sys->source_chain_update = strdup(filters);
        free(sys->source_chain_current);
        sys->source_chain_current = strdup(filters);
    }
    else if (sys->source_chain_current)
        sys->source_chain_update = strdup(sys->source_chain_current);

    vlc_mutex_unlock(&sys->lock);
}

void spu_ChangeFilters(spu_t *spu, const char *filters)
{
    spu_private_t *sys = spu->p;

    vlc_mutex_lock(&sys->lock);

    free(sys->filter_chain_update);
    if (filters)
    {
        sys->filter_chain_update = strdup(filters);
        free(sys->filter_chain_current);
        sys->filter_chain_current = strdup(filters);
    }
    else if (sys->filter_chain_current)
        sys->filter_chain_update = strdup(sys->filter_chain_current);

    vlc_mutex_unlock(&sys->lock);
}

void spu_ChangeChannelOrderMargin(spu_t *spu, enum vlc_vout_order order,
                                  int margin)
{
    spu_private_t *sys = spu->p;

    vlc_mutex_lock(&sys->lock);
    switch (order)
    {
        case VLC_VOUT_ORDER_PRIMARY:
            sys->margin = margin;
            break;
        case VLC_VOUT_ORDER_SECONDARY:
            sys->secondary_margin = margin;
            break;
        default:
            vlc_assert_unreachable();
    }
    vlc_mutex_unlock(&sys->lock);
}

void spu_SetHighlight(spu_t *spu, const vlc_spu_highlight_t *hl)
{
    vlc_mutex_lock(&spu->p->lock);
    UpdateSPU(spu, hl);
    vlc_mutex_unlock(&spu->p->lock);
}
