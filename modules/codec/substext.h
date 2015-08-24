#include <vlc_strings.h>
#include <vlc_text_style.h>

struct subpicture_updater_sys_t {
    text_segment_t *p_segments;

    int  align;
    int  x;
    int  y;

    bool is_fixed;
    int  fixed_width;
    int  fixed_height;
    bool noregionbg;
    bool gridmode;

    /* styling */
    text_style_t *p_default_style; /* decoder (full or partial) defaults */
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
    r->b_noregionbg = sys->noregionbg;
    r->b_gridmode = sys->gridmode;
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

    /* Add missing default style, if any, to all segments */
    for ( text_segment_t* p_segment = sys->p_segments; p_segment; p_segment = p_segment->p_next )
    {
        /* Add decoder defaults */
        if( p_segment->style )
            text_style_Merge( p_segment->style, sys->p_default_style, false );
        else
            p_segment->style = text_style_Duplicate( sys->p_default_style );
        /* Update all segments font sizes in pixels, *** metric used by renderers *** */
        /* We only do this when a fixed font size isn't set */
        if( p_segment->style->f_font_relsize && !p_segment->style->i_font_size )
        {
            p_segment->style->i_font_size = p_segment->style->f_font_relsize *
                                            subpic->i_original_picture_height / 100;
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
    sys->p_default_style = text_style_Create( STYLE_NO_DEFAULTS );
    if(unlikely(!sys->p_default_style))
    {
        free(sys);
        return NULL;
    }
    subpicture_t *subpic = decoder_NewSubpicture(decoder, &updater);
    if (!subpic)
    {
        text_style_Delete(sys->p_default_style);
        free(sys);
    }
    return subpic;
}
