struct subpicture_updater_sys_t {
    char *text;
    char *html;

    int  align;
    int  x;
    int  y;
    int  i_font_height_percent;

    bool is_fixed;
    int  fixed_width;
    int  fixed_height;
    bool renderbg;
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

    r->psz_text = sys->text ? strdup(sys->text) : NULL;
    r->psz_html = sys->html ? strdup(sys->html) : NULL;
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

    if (sys->i_font_height_percent != 0)
    {
        r->p_style = text_style_New();
        if (r->p_style)
        {
	    r->p_style->i_font_size = sys->i_font_height_percent *
	      subpic->i_original_picture_height / 100;
            r->p_style->i_font_color = 0xffffff;
            r->p_style->i_font_alpha = 0xff;
	}
    }
}
static void SubpictureTextDestroy(subpicture_t *subpic)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;

    free(sys->text);
    free(sys->html);
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
