#include <vlc_strings.h>
#include <vlc_text_style.h>

typedef struct
{
    bool b_set;
    unsigned int i_value;
} subpicture_updater_sys_option_t;

struct subpicture_updater_sys_t {
    text_segment_t *p_segments;

    int  align;
    int  x;
    int  y;
    int  i_font_height_percent;
    int  i_font_height_abs_to_src;

    bool is_fixed;
    int  fixed_width;
    int  fixed_height;
    bool renderbg;

    /* styling */
    subpicture_updater_sys_option_t style_flags;
    subpicture_updater_sys_option_t font_color;
    subpicture_updater_sys_option_t background_color;
    int16_t i_alpha;
    int16_t i_drop_shadow;
    int16_t i_drop_shadow_alpha;
};

static int SubpictureTextValidate(subpicture_t *subpic,
                                  bool has_src_changed, const video_format_t *fmt_src,
                                  bool has_dst_changed, const video_format_t *fmt_dst,
                                  mtime_t ts)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;
    VLC_UNUSED(fmt_src); VLC_UNUSED(fmt_dst); VLC_UNUSED(ts);

    if (!has_src_changed && !has_dst_changed)
        return VLC_SUCCESS;
    if (!sys->is_fixed && subpic->b_absolute && subpic->p_region &&
        subpic->i_original_picture_width > 0 &&
        subpic->i_original_picture_height > 0) {

        sys->is_fixed     = true;
        sys->x            = subpic->p_region->i_x;
        sys->y            = subpic->p_region->i_y;
        sys->fixed_width  = subpic->i_original_picture_width;
        sys->fixed_height = subpic->i_original_picture_height;
    }
    return VLC_EGENERIC;
}

static void SubpictureTextUpdate(subpicture_t *subpic,
                                 const video_format_t *fmt_src,
                                 const video_format_t *fmt_dst,
                                 mtime_t ts)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;
    VLC_UNUSED(fmt_src); VLC_UNUSED(ts);

    if (fmt_dst->i_sar_num <= 0 || fmt_dst->i_sar_den <= 0)
        return;

    subpic->i_original_picture_width  = fmt_dst->i_width * fmt_dst->i_sar_num / fmt_dst->i_sar_den;
    subpic->i_original_picture_height = fmt_dst->i_height;

    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_TEXT);
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    subpicture_region_t *r = subpic->p_region = subpicture_region_New(&fmt);
    if (!r)
        return;

    r->p_text = sys->p_segments;
    r->i_align  = sys->align;
    r->b_renderbg = sys->renderbg;
    if (!sys->is_fixed) {
        const float margin_ratio = 0.04;
        const int   margin_h     = margin_ratio * fmt_dst->i_visible_width;
        const int   margin_v     = margin_ratio * fmt_dst->i_visible_height;

        r->i_x = 0;
        if (r->i_align & SUBPICTURE_ALIGN_LEFT)
            r->i_x += margin_h + fmt_dst->i_x_offset;
        else if (r->i_align & SUBPICTURE_ALIGN_RIGHT)
            r->i_x += margin_h + fmt_dst->i_width - (fmt_dst->i_visible_width + fmt_dst->i_x_offset);

        r->i_y = 0;
        if (r->i_align & SUBPICTURE_ALIGN_TOP )
            r->i_y += margin_v + fmt_dst->i_y_offset;
        else if (r->i_align & SUBPICTURE_ALIGN_BOTTOM )
            r->i_y += margin_v + fmt_dst->i_height - (fmt_dst->i_visible_height + fmt_dst->i_y_offset);
    } else {
        /* FIXME it doesn't adapt on crop settings changes */
        r->i_x = sys->x * fmt_dst->i_width  / sys->fixed_width;
        r->i_y = sys->y * fmt_dst->i_height / sys->fixed_height;
    }

    if (sys->i_font_height_percent || sys->i_alpha ||
        sys->style_flags.b_set ||
        sys->font_color.b_set ||
        sys->background_color.b_set )
    {
        //FIXME: Is this used for something else than tx3g?
        for ( text_segment_t* p_segment = sys->p_segments; p_segment; p_segment = p_segment->p_next )
        {
            text_style_t* p_style = p_segment->style;

            if (sys->i_font_height_abs_to_src)
                sys->i_font_height_percent = sys->i_font_height_abs_to_src * 100 /
                                             fmt_src->i_visible_height;

            if (sys->i_font_height_percent)
            {
                p_style->i_font_size = sys->i_font_height_percent *
                                       subpic->i_original_picture_height / 100;
                p_style->i_font_color = 0xffffff;
                p_style->i_font_alpha = 0xff;
            }

            if (sys->style_flags.b_set)
                p_style->i_style_flags = sys->style_flags.i_value;
            if (sys->font_color.b_set)
                p_style->i_font_color = sys->font_color.i_value;
            if (sys->background_color.b_set)
                p_style->i_background_color = sys->background_color.i_value;
            if (sys->i_alpha)
                p_style->i_font_alpha = sys->i_alpha;
            if (sys->i_drop_shadow)
                p_style->i_shadow_width = sys->i_drop_shadow;
            if (sys->i_drop_shadow_alpha)
                p_style->i_shadow_alpha = sys->i_drop_shadow_alpha;
        }
    }
}
static void SubpictureTextDestroy(subpicture_t *subpic)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;

    text_segment_ChainDelete( sys->p_segments );
    free(sys);
}

static inline subpicture_t *decoder_NewSubpictureText(decoder_t *decoder)
{
    subpicture_updater_sys_t *sys = calloc(1, sizeof(*sys));
    subpicture_updater_t updater = {
        .pf_validate = SubpictureTextValidate,
        .pf_update   = SubpictureTextUpdate,
        .pf_destroy  = SubpictureTextDestroy,
        .p_sys       = sys,
    };
    subpicture_t *subpic = decoder_NewSubpicture(decoder, &updater);
    if (!subpic)
        free(sys);
    return subpic;
}
