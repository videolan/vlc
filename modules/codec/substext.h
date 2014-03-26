#include <vlc_strings.h>

typedef struct
{
    bool b_set;
    unsigned int i_value;
} subpicture_updater_sys_option_t;

typedef struct segment_t segment_t;

typedef struct
{
    uint8_t i_fontsize;
    uint32_t i_color;   //ARGB
    uint8_t i_flags;
} segment_style_t;

struct segment_t
{
    char *psz_string;
    unsigned int i_size;
    segment_t *p_next;
    /* styles applied to that segment */
    segment_style_t styles;
};

struct subpicture_updater_sys_t {
    char *text;
    char *html;
    segment_t *p_htmlsegments;

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

static void SegmentFree( segment_t *p_segment )
{
    if ( p_segment )
    {
        free( p_segment->psz_string );
        free( p_segment );
    }
}

static void MakeHtmlNewLines( char **ppsz_src )
{
    unsigned int i_nlcount = 0;
    unsigned i_len = strlen( *ppsz_src );
    if ( i_len == 0 ) return;
    for ( unsigned i=0; i<i_len; i++ )
        if ( (*ppsz_src)[i] == '\n' )
            i_nlcount++;
    if ( !i_nlcount ) return;

    char *psz_dst = malloc( i_len + 1 + (i_nlcount * 4) );
    char *ptr = psz_dst;
    for ( unsigned i=0; i<i_len; i++ )
    {
        if ( (*ppsz_src)[i] == '\n' )
        {
            strcpy( ptr, "<br/>" );
            ptr += 5;
        } else {
            *ptr++ = (*ppsz_src)[i];
        }
    }
    *ptr = 0;
    free( *ppsz_src );
    *ppsz_src = psz_dst;
}

static void HtmlAppend( char **ppsz_dst, const char *psz_src,
                        const segment_style_t *p_styles, const float f_scale )
{
    if ( !ppsz_dst ) return;
    int i_return;
    char *psz_subtext = NULL;
    char *psz_text = NULL;
    char *psz_fontsize = NULL;
    char *psz_color = NULL;
    char *psz_encoded = convert_xml_special_chars( psz_src );
    if ( !psz_encoded ) return;

    MakeHtmlNewLines( &psz_encoded );

    if ( p_styles->i_color & 0xFF000000 ) //ARGB
    {
        i_return = asprintf( &psz_color, " color=\"#%6x\"",
                             p_styles->i_color & 0x00FFFFFF );
        if ( i_return < 0 ) psz_color = NULL;
    }

    if ( p_styles->i_fontsize > 0 && f_scale > 0 )
    {
        i_return = asprintf( &psz_fontsize, " size=\"%u\"",
                             (unsigned) (f_scale * p_styles->i_fontsize) );
        if ( i_return < 0 ) psz_fontsize = NULL;
    }

    i_return = asprintf( &psz_subtext, "%s%s%s%s%s%s%s",
                        ( p_styles->i_flags & STYLE_UNDERLINE ) ? "<u>" : "",
                        ( p_styles->i_flags & STYLE_BOLD ) ? "<b>" : "",
                        ( p_styles->i_flags & STYLE_ITALIC ) ? "<i>" : "",
                          psz_encoded,
                        ( p_styles->i_flags & STYLE_ITALIC ) ? "</i>" : "",
                        ( p_styles->i_flags & STYLE_BOLD ) ? "</b>" : "",
                        ( p_styles->i_flags & STYLE_UNDERLINE ) ? "</u>" : ""
                        );
    if ( i_return < 0 ) psz_subtext = NULL;

    if ( psz_color || psz_fontsize )
    {
        i_return = asprintf( &psz_text, "<font%s%s>%s</font>",
                            psz_color ? psz_color : "",
                            psz_fontsize ? psz_fontsize : "",
                            psz_subtext );
        if ( i_return < 0 ) psz_text = NULL;
        free( psz_subtext );
    }
    else
    {
        psz_text = psz_subtext;
    }

    free( psz_fontsize );
    free( psz_color );

    if ( *ppsz_dst )
    {
        char *psz_dst = *ppsz_dst;
        i_return = asprintf( ppsz_dst, "%s%s", psz_dst, psz_text );
        if ( i_return < 0 ) ppsz_dst = NULL;
        free( psz_dst );
        free( psz_text );
    }
    else
        *ppsz_dst = psz_text;
}

static char *SegmentsToHtml( segment_t *p_head, const float f_scale )
{
    char *psz_dst = NULL;
    char *psz_ret = NULL;
    while( p_head )
    {
        HtmlAppend( &psz_dst, p_head->psz_string, &p_head->styles, f_scale );
        p_head = p_head->p_next;
    }
    int i_ret = asprintf( &psz_ret, "<text>%s</text>", psz_dst );
    if ( i_ret < 0 ) psz_ret = NULL;
    free( psz_dst );
    return psz_ret;
}

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
    if ( sys->p_htmlsegments )
        r->psz_html = SegmentsToHtml( sys->p_htmlsegments,
                                      (float) fmt_dst->i_height / fmt_src->i_height );
    else if ( sys->html )
        r->psz_html = strdup(sys->html);
    else
        r->psz_html = NULL;
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
        r->p_style = text_style_New();
        if (!r->p_style) return;

        if (sys->i_font_height_abs_to_src)
            sys->i_font_height_percent = sys->i_font_height_abs_to_src * 100 /
                                         fmt_src->i_visible_height;

        if (sys->i_font_height_percent)
        {
            r->p_style->i_font_size = sys->i_font_height_percent *
                                      subpic->i_original_picture_height / 100;
            r->p_style->i_font_color = 0xffffff;
            r->p_style->i_font_alpha = 0xff;
        }

        if (sys->style_flags.b_set)
            r->p_style->i_style_flags = sys->style_flags.i_value;
        if (sys->font_color.b_set)
            r->p_style->i_font_color = sys->font_color.i_value;
        if (sys->background_color.b_set)
            r->p_style->i_background_color = sys->background_color.i_value;
        if (sys->i_alpha)
            r->p_style->i_font_alpha = sys->i_alpha;
        if (sys->i_drop_shadow)
            r->p_style->i_shadow_width = sys->i_drop_shadow;
        if (sys->i_drop_shadow_alpha)
            r->p_style->i_shadow_alpha = sys->i_drop_shadow_alpha;
    }
}
static void SubpictureTextDestroy(subpicture_t *subpic)
{
    subpicture_updater_sys_t *sys = subpic->updater.p_sys;

    free(sys->text);
    free(sys->html);
    while( sys->p_htmlsegments )
    {
        segment_t *p_segment = sys->p_htmlsegments;
        sys->p_htmlsegments = sys->p_htmlsegments->p_next;
        SegmentFree( p_segment );
    }
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
