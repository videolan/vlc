/*****************************************************************************
 * quartztext.c : Put text on the video, using Mac OS X Quartz Engine
 *****************************************************************************
 * Copyright (C) 2007, 2009, 2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Bernie Purcell <bitmap@videolan.org>
 *          Pierre d'Herbemont <pdherbemont # videolan dot>
 *          Felix Paul Kühne <fkuehne # videolan # org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_xml.h>
#include <vlc_input.h>
#include <vlc_filter.h>

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>

#else
// Fix ourselves ColorSync headers that gets included in ApplicationServices.
#define DisposeCMProfileIterateUPP(a) DisposeCMProfileIterateUPP(CMProfileIterateUPP userUPP __attribute__((unused)))
#define DisposeCMMIterateUPP(a) DisposeCMMIterateUPP(CMProfileIterateUPP userUPP __attribute__((unused)))
#define __MACHINEEXCEPTIONS__
#include <ApplicationServices/ApplicationServices.h>
#endif

#define DEFAULT_FONT           "Helvetica-Neue"
#define DEFAULT_FONT_COLOR     0xffffff
#define DEFAULT_REL_FONT_SIZE  16

#define VERTICAL_MARGIN 3
#define HORIZONTAL_MARGIN 10

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create (vlc_object_t *);
static void Destroy(vlc_object_t *);

static int LoadFontsFromAttachments(filter_t *p_filter);

static int RenderText(filter_t *, subpicture_region_t *,
                       subpicture_region_t *,
                       const vlc_fourcc_t *);
static int RenderHtml(filter_t *, subpicture_region_t *,
                       subpicture_region_t *,
                       const vlc_fourcc_t *);

static int GetFontSize(filter_t *p_filter);
static int RenderYUVA(filter_t *p_filter, subpicture_region_t *p_region,
                       CFMutableAttributedStringRef p_attrString);

static void setFontAttibutes(char *psz_fontname, int i_font_size, uint32_t i_font_color,
                              bool b_bold, bool b_italic, bool b_underline,
                              CFRange p_range, CFMutableAttributedStringRef p_attrString);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* The preferred way to set font style information is for it to come from the
 * subtitle file, and for it to be rendered with RenderHtml instead of
 * RenderText. */
#define FONT_TEXT N_("Font")
#define FONT_LONGTEXT N_("Name for the font you want to use")
#define FONTSIZER_TEXT N_("Relative font size")
#define FONTSIZER_LONGTEXT N_("This is the relative default size of the " \
    "fonts that will be rendered on the video. If absolute font size is set, "\
    "relative size will be overridden.")
#define COLOR_TEXT N_("Text default color")
#define COLOR_LONGTEXT N_("The color of the text that will be rendered on "\
    "the video. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white")

static const int pi_color_values[] = {
  0x00000000, 0x00808080, 0x00C0C0C0, 0x00FFFFFF, 0x00800000,
  0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00808000, 0x00008000, 0x00008080,
  0x0000FF00, 0x00800080, 0x00000080, 0x000000FF, 0x0000FFFF };

static const char *const ppsz_color_names[] = {
    "black", "gray", "silver", "white", "maroon",
    "red", "fuchsia", "yellow", "olive", "green",
    "teal", "lime", "purple", "navy", "blue", "aqua" };

static const char *const ppsz_color_descriptions[] = {
  N_("Black"), N_("Gray"), N_("Silver"), N_("White"), N_("Maroon"),
  N_("Red"), N_("Fuchsia"), N_("Yellow"), N_("Olive"), N_("Green"), N_("Teal"),
  N_("Lime"), N_("Purple"), N_("Navy"), N_("Blue"), N_("Aqua") };

static const int pi_sizes[] = { 20, 18, 16, 12, 6 };
static const char *const ppsz_sizes_text[] = {
    N_("Smaller"), N_("Small"), N_("Normal"), N_("Large"), N_("Larger") };

vlc_module_begin ()
    set_shortname(N_("Text renderer for Mac"))
    set_description(N_("CoreText font renderer"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_SUBPIC)

    add_string("quartztext-font", DEFAULT_FONT, FONT_TEXT, FONT_LONGTEXT,
              false)
    add_integer("quartztext-rel-fontsize", DEFAULT_REL_FONT_SIZE, FONTSIZER_TEXT,
                 FONTSIZER_LONGTEXT, false)
        change_integer_list(pi_sizes, ppsz_sizes_text)
    add_integer("quartztext-color", 0x00FFFFFF, COLOR_TEXT,
                 COLOR_LONGTEXT, false)
        change_integer_list(pi_color_values, ppsz_color_descriptions)
    set_capability("text renderer", 50)
    add_shortcut("text")
    set_callbacks(Create, Destroy)
vlc_module_end ()

typedef struct font_stack_t font_stack_t;
struct font_stack_t
{
    char          *psz_name;
    int            i_size;
    uint32_t       i_color;            // ARGB

    font_stack_t  *p_next;
};

typedef struct
{
    int         i_font_size;
    uint32_t    i_font_color;         /* ARGB */
    bool  b_italic;
    bool  b_bold;
    bool  b_underline;
    char       *psz_fontname;
} ft_style_t;

typedef struct offscreen_bitmap_t offscreen_bitmap_t;
struct offscreen_bitmap_t
{
    uint8_t       *p_data;
    int            i_bitsPerChannel;
    int            i_bitsPerPixel;
    int            i_bytesPerPixel;
    int            i_bytesPerRow;
};

/*****************************************************************************
 * filter_sys_t: quartztext local data
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the freetype specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    char          *psz_font_name;
    uint8_t        i_font_opacity;
    int            i_font_color;
    int            i_font_size;

#ifndef TARGET_OS_IPHONE
    ATSFontContainerRef    *p_fonts;
    int                     i_fonts;
#endif
};

/*****************************************************************************
 * Create: allocates osd-text video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
 *****************************************************************************/
static int Create(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    // Allocate structure
    p_filter->p_sys = p_sys = malloc(sizeof(filter_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;
    p_sys->psz_font_name  = var_CreateGetString(p_this, "quartztext-font");
    p_sys->i_font_opacity = 255;
    p_sys->i_font_color = VLC_CLIP(var_CreateGetInteger(p_this, "quartztext-color") , 0, 0xFFFFFF);
    p_sys->i_font_size = GetFontSize(p_filter);

    p_filter->pf_render_text = RenderText;
    p_filter->pf_render_html = RenderHtml;

#ifndef TARGET_OS_IPHONE
    p_sys->p_fonts = NULL;
    p_sys->i_fonts = 0;
#endif

    LoadFontsFromAttachments(p_filter);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Clone video thread output method
 *****************************************************************************
 * Clean up all data and library connections
 *****************************************************************************/
static void Destroy(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
#ifndef TARGET_OS_IPHONE
    if (p_sys->p_fonts) {
        for (int k = 0; k < p_sys->i_fonts; k++) {
            ATSFontDeactivate(p_sys->p_fonts[k], NULL, kATSOptionFlagsDefault);

        free(p_sys->p_fonts);
    }
#endif
    free(p_sys->psz_font_name);
    free(p_sys);
}

/*****************************************************************************
 * Make any TTF/OTF fonts present in the attachments of the media file
 * available to the Quartz engine for text rendering
 *****************************************************************************/
static int LoadFontsFromAttachments(filter_t *p_filter)
{
#ifdef TARGET_OS_IPHONE
    VLC_UNUSED(p_filter);
    return VLC_SUCCESS;
#else
    filter_sys_t         *p_sys = p_filter->p_sys;
    input_attachment_t  **pp_attachments;
    int                   i_attachments_cnt;

    if (filter_GetInputAttachments(p_filter, &pp_attachments, &i_attachments_cnt))
        return VLC_EGENERIC;

    p_sys->i_fonts = 0;
    p_sys->p_fonts = malloc(i_attachments_cnt * sizeof(ATSFontContainerRef));
    if (! p_sys->p_fonts)
        return VLC_ENOMEM;

    for (int k = 0; k < i_attachments_cnt; k++) {
        input_attachment_t *p_attach = pp_attachments[k];

        if ((!strcmp(p_attach->psz_mime, "application/x-truetype-font") || // TTF
              !strcmp(p_attach->psz_mime, "application/x-font-otf")) &&    // OTF
            p_attach->i_data > 0 && p_attach->p_data) {
            ATSFontContainerRef  container;

            if (noErr == ATSFontActivateFromMemory(p_attach->p_data,
                                                    p_attach->i_data,
                                                    kATSFontContextLocal,
                                                    kATSFontFormatUnspecified,
                                                    NULL,
                                                    kATSOptionFlagsDefault,
                                                    &container))
                p_sys->p_fonts[ p_sys->i_fonts++ ] = container;
        }
        vlc_input_attachment_Delete(p_attach);
    }
    free(pp_attachments);
    return VLC_SUCCESS;
#endif
}

static char *EliminateCRLF(char *psz_string)
{
    char *q;

    for (char * p = psz_string; p && *p; p++) {
        if ((*p == '\r') && (*(p+1) == '\n')) {
            for (q = p + 1; *q; q++)
                *(q - 1) = *q;

            *(q - 1) = '\0';
        }
    }
    return psz_string;
}

/* Renders a text subpicture region into another one.
 * It is used as pf_add_string callback in the vout method by this module */
static int RenderText(filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in,
                       const vlc_fourcc_t *p_chroma_list)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    char         *psz_string;
    int           i_font_alpha, i_font_size;
    uint32_t      i_font_color;
    bool          b_bold, b_uline, b_italic;
    vlc_value_t val;
    b_bold = b_uline = b_italic = FALSE;
    VLC_UNUSED(p_chroma_list);

    p_sys->i_font_size = GetFontSize(p_filter);

    // Sanity check
    if (!p_region_in || !p_region_out)
        return VLC_EGENERIC;

    psz_string = p_region_in->psz_text;
    if (!psz_string || !*psz_string)
        return VLC_EGENERIC;

    if (p_region_in->p_style) {
        i_font_color = VLC_CLIP(p_region_in->p_style->i_font_color, 0, 0xFFFFFF);
        i_font_alpha = VLC_CLIP(p_region_in->p_style->i_font_alpha, 0, 255);
        i_font_size  = VLC_CLIP(p_region_in->p_style->i_font_size, 0, 255);
        if (p_region_in->p_style->i_style_flags) {
            if (p_region_in->p_style->i_style_flags & STYLE_BOLD)
                b_bold = TRUE;
            if (p_region_in->p_style->i_style_flags & STYLE_ITALIC)
                b_italic = TRUE;
            if (p_region_in->p_style->i_style_flags & STYLE_UNDERLINE)
                b_uline = TRUE;
        }
    } else {
        i_font_color = p_sys->i_font_color;
        i_font_alpha = 255 - p_sys->i_font_opacity;
        i_font_size  = p_sys->i_font_size;
    }

    if (!i_font_alpha)
        i_font_alpha = 255 - p_sys->i_font_opacity;

    if (i_font_size <= 0) {
        msg_Warn(p_filter, "invalid fontsize, using 12");
        if (VLC_SUCCESS == var_Get(p_filter, "scale", &val))
            i_font_size = 12 * val.i_int / 1000;
        else
            i_font_size = 12;
    }

    p_region_out->i_x = p_region_in->i_x;
    p_region_out->i_y = p_region_in->i_y;

    CFMutableAttributedStringRef p_attrString = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);

    if (p_attrString) {
        CFStringRef   p_cfString;
        int           len;

        EliminateCRLF(psz_string);
        p_cfString = CFStringCreateWithCString(NULL, psz_string, kCFStringEncodingUTF8);
        CFAttributedStringReplaceString(p_attrString, CFRangeMake(0, 0), p_cfString);
        CFRelease(p_cfString);
        len = CFAttributedStringGetLength(p_attrString);

        setFontAttibutes(p_sys->psz_font_name, i_font_size, i_font_color, b_bold, b_italic, b_uline,
                                             CFRangeMake(0, len), p_attrString);

        RenderYUVA(p_filter, p_region_out, p_attrString);
        CFRelease(p_attrString);
    }

    return VLC_SUCCESS;
}


static int PushFont(font_stack_t **p_font, const char *psz_name, int i_size,
                     uint32_t i_color)
{
    font_stack_t *p_new;

    if (!p_font)
        return VLC_EGENERIC;

    p_new = malloc(sizeof(font_stack_t));
    if (! p_new)
        return VLC_ENOMEM;

    p_new->p_next = NULL;

    if (psz_name)
        p_new->psz_name = strdup(psz_name);
    else
        p_new->psz_name = NULL;

    p_new->i_size   = i_size;
    p_new->i_color  = i_color;

    if (!*p_font)
        *p_font = p_new;
    else {
        font_stack_t *p_last;

        for (p_last = *p_font; p_last->p_next; p_last = p_last->p_next)
        ;

        p_last->p_next = p_new;
    }
    return VLC_SUCCESS;
}

static int PopFont(font_stack_t **p_font)
{
    font_stack_t *p_last, *p_next_to_last;

    if (!p_font || !*p_font)
        return VLC_EGENERIC;

    p_next_to_last = NULL;
    for (p_last = *p_font; p_last->p_next; p_last = p_last->p_next)
        p_next_to_last = p_last;

    if (p_next_to_last)
        p_next_to_last->p_next = NULL;
    else
        *p_font = NULL;

    free(p_last->psz_name);
    free(p_last);

    return VLC_SUCCESS;
}

static int PeekFont(font_stack_t **p_font, char **psz_name, int *i_size,
                     uint32_t *i_color)
{
    font_stack_t *p_last;

    if (!p_font || !*p_font)
        return VLC_EGENERIC;

    for (p_last=*p_font;
         p_last->p_next;
         p_last=p_last->p_next)
    ;

    *psz_name = p_last->psz_name;
    *i_size   = p_last->i_size;
    *i_color  = p_last->i_color;

    return VLC_SUCCESS;
}

static int HandleFontAttributes(xml_reader_t *p_xml_reader,
                                  font_stack_t **p_fonts)
{
    int        rv;
    char      *psz_fontname = NULL;
    uint32_t   i_font_color = 0xffffff;
    int        i_font_alpha = 0;
    int        i_font_size  = 24;
    const char *attr, *value;

    /* Default all attributes to the top font in the stack -- in case not
     * all attributes are specified in the sub-font */
    if (VLC_SUCCESS == PeekFont(p_fonts,
                                &psz_fontname,
                                &i_font_size,
                                &i_font_color)) {
        psz_fontname = strdup(psz_fontname);
        i_font_size = i_font_size;
    }
    i_font_alpha = (i_font_color >> 24) & 0xff;
    i_font_color &= 0x00ffffff;

    while ((attr = xml_ReaderNextAttr(p_xml_reader, &value))) {
        if (!strcasecmp("face", attr)) {
            free(psz_fontname);
            psz_fontname = strdup(value);
        } else if (!strcasecmp("size", attr)) {
            if ((*value == '+') || (*value == '-')) {
                int i_value = atoi(value);

                if ((i_value >= -5) && (i_value <= 5))
                    i_font_size += (i_value * i_font_size) / 10;
                else if (i_value < -5)
                    i_font_size = - i_value;
                else if (i_value > 5)
                    i_font_size = i_value;
            }
            else
                i_font_size = atoi(value);
        } else if (!strcasecmp("color", attr)) {
            if (value[0] == '#') {
                i_font_color = strtol(value + 1, NULL, 16);
                i_font_color &= 0x00ffffff;
            } else {
                /* color detection fallback */
                unsigned int count = sizeof(ppsz_color_names);
                for (unsigned x = 0; x < count; x++) {
                    if (!strcmp(value, ppsz_color_names[x])) {
                        i_font_color = pi_color_values[x];
                        break;
                    }
                }
            }
        } else if (!strcasecmp("alpha", attr) && (value[0] == '#')) {
            i_font_alpha = strtol(value + 1, NULL, 16);
            i_font_alpha &= 0xff;
        }
    }
    rv = PushFont(p_fonts,
                  psz_fontname,
                  i_font_size,
                  (i_font_color & 0xffffff) | ((i_font_alpha & 0xff) << 24));

    free(psz_fontname);

    return rv;
}

static void setFontAttibutes(char *psz_fontname, int i_font_size, uint32_t i_font_color,
        bool b_bold, bool b_italic, bool b_underline,
        CFRange p_range, CFMutableAttributedStringRef p_attrString)
{
    CFStringRef p_cfString;
    CTFontRef   p_font;

    // fallback on default
    if (!psz_fontname)
        psz_fontname = (char *)DEFAULT_FONT;

    p_cfString = CFStringCreateWithCString(kCFAllocatorDefault,
                                            psz_fontname,
                                            kCFStringEncodingUTF8);
    p_font     = CTFontCreateWithName(p_cfString,
                                       (float)i_font_size,
                                       NULL);
    CFRelease(p_cfString);
    CFAttributedStringSetAttribute(p_attrString,
                                    p_range,
                                    kCTFontAttributeName,
                                    p_font);
    CFRelease(p_font);

    // Handle Underline
    SInt32 _uline;
    if (b_underline)
        _uline = kCTUnderlineStyleSingle;
    else
        _uline = kCTUnderlineStyleNone;

    CFNumberRef underline = CFNumberCreate(NULL, kCFNumberSInt32Type, &_uline);
    CFAttributedStringSetAttribute(p_attrString,
                                    p_range,
                                    kCTUnderlineStyleAttributeName,
                                    underline);
    CFRelease(underline);

    // Handle Bold
    float _weight;
    if (b_bold)
        _weight = 0.5;
    else
        _weight = 0.0;

    CFNumberRef weight = CFNumberCreate(NULL, kCFNumberFloatType, &_weight);
    CFAttributedStringSetAttribute(p_attrString,
                                    p_range,
                                    kCTFontWeightTrait,
                                    weight);
    CFRelease(weight);

    // Handle Italic
    float _slant;
    if (b_italic)
        _slant = 1.0;
    else
        _slant = 0.0;

    CFNumberRef slant = CFNumberCreate(NULL, kCFNumberFloatType, &_slant);
    CFAttributedStringSetAttribute(p_attrString,
                                    p_range,
                                    kCTFontSlantTrait,
                                    slant);
    CFRelease(slant);

    // fetch invalid colors
    if (i_font_color == 0xFFFFFFFF)
        i_font_color = 0x00FFFFFF;

    // Handle foreground color
    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    CGFloat components[] = { (float)((i_font_color & 0x00ff0000) >> 16) / 255.0,
                             (float)((i_font_color & 0x0000ff00) >>  8) / 255.0,
                             (float)((i_font_color & 0x000000ff)) / 255.0,
                             (float)(255-((i_font_color & 0xff000000) >> 24)) / 255.0 };
    CGColorRef fg_text = CGColorCreate(rgbColorSpace, components);
    CGColorSpaceRelease(rgbColorSpace);

    CFAttributedStringSetAttribute(p_attrString,
                                    p_range,
                                    kCTForegroundColorAttributeName,
                                    fg_text);
    CFRelease(fg_text);

}

static void GetAttrStrFromFontStack(font_stack_t **p_fonts,
        bool b_bold, bool b_italic, bool b_uline,
        CFRange p_range, CFMutableAttributedStringRef p_attrString)
{
    char       *psz_fontname = NULL;
    int         i_font_size  = 0;
    uint32_t    i_font_color = 0;

    if (VLC_SUCCESS == PeekFont(p_fonts, &psz_fontname, &i_font_size,
                                &i_font_color)) {
        setFontAttibutes(psz_fontname,
                         i_font_size,
                         i_font_color,
                         b_bold, b_italic, b_uline,
                         p_range,
                         p_attrString);
    }
}

static int ProcessNodes(filter_t *p_filter,
                         xml_reader_t *p_xml_reader,
                         text_style_t *p_font_style,
                         CFMutableAttributedStringRef p_attrString)
{
    int           rv             = VLC_SUCCESS;
    filter_sys_t *p_sys          = p_filter->p_sys;
    font_stack_t *p_fonts        = NULL;

    int type;
    const char *node;

    bool b_italic = false;
    bool b_bold   = false;
    bool b_uline  = false;

    if (p_font_style) {
        rv = PushFont(&p_fonts,
               p_font_style->psz_fontname,
               p_font_style->i_font_size,
               (p_font_style->i_font_color & 0xffffff) |
                   ((p_font_style->i_font_alpha & 0xff) << 24));

        if (p_font_style->i_style_flags & STYLE_BOLD)
            b_bold = true;
        if (p_font_style->i_style_flags & STYLE_ITALIC)
            b_italic = true;
        if (p_font_style->i_style_flags & STYLE_UNDERLINE)
            b_uline = true;
    } else {
        rv = PushFont(&p_fonts,
                       p_sys->psz_font_name,
                       p_sys->i_font_size,
                       p_sys->i_font_color);
    }
    if (rv != VLC_SUCCESS)
        return rv;

    while ((type = xml_ReaderNextNode(p_xml_reader, &node)) > 0) {
        switch (type) {
            case XML_READER_ENDELEM:
                if (!strcasecmp("font", node))
                    PopFont(&p_fonts);
                else if (!strcasecmp("b", node))
                    b_bold   = false;
                else if (!strcasecmp("i", node))
                    b_italic = false;
                else if (!strcasecmp("u", node))
                    b_uline  = false;

                break;
            case XML_READER_STARTELEM:
                if (!strcasecmp("font", node))
                    rv = HandleFontAttributes(p_xml_reader, &p_fonts);
                else if (!strcasecmp("b", node))
                    b_bold = true;
                else if (!strcasecmp("i", node))
                    b_italic = true;
                else if (!strcasecmp("u", node))
                    b_uline = true;
                else if (!strcasecmp("br", node)) {
                    CFMutableAttributedStringRef p_attrnode = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
                    CFAttributedStringReplaceString(p_attrnode, CFRangeMake(0, 0), CFSTR("\n"));

                    GetAttrStrFromFontStack(&p_fonts, b_bold, b_italic, b_uline,
                                             CFRangeMake(0, 1),
                                             p_attrnode);
                    CFAttributedStringReplaceAttributedString(p_attrString,
                                    CFRangeMake(CFAttributedStringGetLength(p_attrString), 0),
                                    p_attrnode);
                    CFRelease(p_attrnode);
                }
                break;
            case XML_READER_TEXT:
            {
                CFStringRef   p_cfString;
                int           len;

                // Turn any multiple-whitespaces into single spaces
                char *dup = strdup(node);
                if (!dup)
                    break;
                char *s = strpbrk(dup, "\t\r\n ");
                while(s)
                {
                    int i_whitespace = strspn(s, "\t\r\n ");

                    if (i_whitespace > 1)
                        memmove(&s[1],
                                 &s[i_whitespace],
                                 strlen(s) - i_whitespace + 1);
                    *s++ = ' ';

                    s = strpbrk(s, "\t\r\n ");
                }


                CFMutableAttributedStringRef p_attrnode = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
                p_cfString = CFStringCreateWithCString(NULL, dup, kCFStringEncodingUTF8);
                CFAttributedStringReplaceString(p_attrnode, CFRangeMake(0, 0), p_cfString);
                CFRelease(p_cfString);
                len = CFAttributedStringGetLength(p_attrnode);

                GetAttrStrFromFontStack(&p_fonts, b_bold, b_italic, b_uline,
                                         CFRangeMake(0, len),
                                         p_attrnode);

                CFAttributedStringReplaceAttributedString(p_attrString,
                                CFRangeMake(CFAttributedStringGetLength(p_attrString), 0),
                                p_attrnode);
                CFRelease(p_attrnode);

                free(dup);
                break;
            }
        }
    }

    while(VLC_SUCCESS == PopFont(&p_fonts));

    return rv;
}

static int RenderHtml(filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in,
                       const vlc_fourcc_t *p_chroma_list)
{
    int          rv = VLC_SUCCESS;
    stream_t     *p_sub = NULL;
    xml_t        *p_xml = NULL;
    xml_reader_t *p_xml_reader = NULL;
    VLC_UNUSED(p_chroma_list);

    if (!p_region_in || !p_region_in->psz_html)
        return VLC_EGENERIC;

    /* Reset the default fontsize in case screen metrics have changed */
    p_filter->p_sys->i_font_size = GetFontSize(p_filter);

    p_sub = stream_MemoryNew(VLC_OBJECT(p_filter),
                              (uint8_t *) p_region_in->psz_html,
                              strlen(p_region_in->psz_html),
                              true);
    if (p_sub) {
        p_xml = xml_Create(p_filter);
        if (p_xml) {
            bool b_karaoke = false;

            p_xml_reader = xml_ReaderCreate(p_xml, p_sub);
            if (p_xml_reader) {
                /* Look for Root Node */
                const char *name;
                if (xml_ReaderNextNode(p_xml_reader, &name)
                        == XML_READER_STARTELEM) {
                    if (!strcasecmp("karaoke", name)) {
                        /* We're going to have to render the text a number
                         * of times to show the progress marker on the text.
                         */
                        var_SetBool(p_filter, "text-rerender", true);
                        b_karaoke = true;
                    } else if (!strcasecmp("text", name))
                        b_karaoke = false;
                    else {
                        /* Only text and karaoke tags are supported */
                        msg_Dbg(p_filter, "Unsupported top-level tag "
                                           "<%s> ignored.", name);
                        rv = VLC_EGENERIC;
                    }
                } else {
                    msg_Err(p_filter, "Malformed HTML subtitle");
                    rv = VLC_EGENERIC;
                }

                if (rv != VLC_SUCCESS) {
                    xml_ReaderDelete(p_xml_reader);
                    p_xml_reader = NULL;
                }
            }

            if (p_xml_reader) {
                int         i_len;

                CFMutableAttributedStringRef p_attrString = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
                rv = ProcessNodes(p_filter, p_xml_reader,
                              p_region_in->p_style, p_attrString);

                i_len = CFAttributedStringGetLength(p_attrString);

                p_region_out->i_x = p_region_in->i_x;
                p_region_out->i_y = p_region_in->i_y;

                if ((rv == VLC_SUCCESS) && (i_len > 0))
                    RenderYUVA(p_filter, p_region_out, p_attrString);

                CFRelease(p_attrString);

                xml_ReaderDelete(p_xml_reader);
            }
            xml_Delete(p_xml);
        }
        stream_Delete(p_sub);
    }

    return rv;
}

static CGContextRef CreateOffScreenContext(int i_width, int i_height,
                         offscreen_bitmap_t **pp_memory, CGColorSpaceRef *pp_colorSpace)
{
    offscreen_bitmap_t *p_bitmap;
    CGContextRef        p_context = NULL;

    p_bitmap = (offscreen_bitmap_t *) malloc(sizeof(offscreen_bitmap_t));
    if (p_bitmap) {
        p_bitmap->i_bitsPerChannel = 8;
        p_bitmap->i_bitsPerPixel   = 4 * p_bitmap->i_bitsPerChannel; // A,R,G,B
        p_bitmap->i_bytesPerPixel  = p_bitmap->i_bitsPerPixel / 8;
        p_bitmap->i_bytesPerRow    = i_width * p_bitmap->i_bytesPerPixel;

        p_bitmap->p_data = calloc(i_height, p_bitmap->i_bytesPerRow);

        *pp_colorSpace = CGColorSpaceCreateDeviceRGB();

        if (p_bitmap->p_data && *pp_colorSpace)
            p_context = CGBitmapContextCreate(p_bitmap->p_data, i_width, i_height,
                                p_bitmap->i_bitsPerChannel, p_bitmap->i_bytesPerRow,
                                *pp_colorSpace, kCGImageAlphaPremultipliedFirst);

        if (p_context) {
            if (CGContextSetAllowsAntialiasing != NULL)
                CGContextSetAllowsAntialiasing(p_context, true);
        }
        *pp_memory = p_bitmap;
    }

    return p_context;
}

static offscreen_bitmap_t *Compose(int i_text_align,
                                    CFMutableAttributedStringRef p_attrString,
                                    unsigned i_width,
                                    unsigned i_height,
                                    unsigned *pi_textblock_height)
{
    offscreen_bitmap_t  *p_offScreen  = NULL;
    CGColorSpaceRef      p_colorSpace = NULL;
    CGContextRef         p_context = NULL;

    p_context = CreateOffScreenContext(i_width, i_height, &p_offScreen, &p_colorSpace);

    *pi_textblock_height = 0;
    if (p_context) {
        float horiz_flush;

        CGContextSetTextMatrix(p_context, CGAffineTransformIdentity);

        if (i_text_align == SUBPICTURE_ALIGN_RIGHT)
            horiz_flush = 1.0;
        else if (i_text_align != SUBPICTURE_ALIGN_LEFT)
            horiz_flush = 0.5;
        else
            horiz_flush = 0.0;

        // Create the framesetter with the attributed string.
        CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString(p_attrString);
        if (framesetter) {
            CTFrameRef frame;
            CGMutablePathRef p_path = CGPathCreateMutable();
            CGRect p_bounds = CGRectMake((float)HORIZONTAL_MARGIN,
                                          (float)VERTICAL_MARGIN,
                                          (float)(i_width  - HORIZONTAL_MARGIN*2),
                                          (float)(i_height - VERTICAL_MARGIN  *2));
            CGPathAddRect(p_path, NULL, p_bounds);

            // Create the frame and draw it into the graphics context
            frame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), p_path, NULL);

            CGPathRelease(p_path);

            // Set up black outlining of the text --
            CGContextSetRGBStrokeColor(p_context, 0, 0, 0, 0.5);
            CGContextSetTextDrawingMode(p_context, kCGTextFillStroke);

            if (frame != NULL) {
                CFArrayRef lines;
                CGPoint    penPosition;

                lines = CTFrameGetLines(frame);
                penPosition.y = i_height;
                for (int i=0; i<CFArrayGetCount(lines); i++) {
                    CGFloat  ascent, descent, leading;

                    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(lines, i);
                    CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

                    // Set the outlining for this line to be dependant on the size of the line -
                    // make it about 5% of the ascent, with a minimum at 1.0
                    float f_thickness = ascent * 0.05;
                    CGContextSetLineWidth(p_context, ((f_thickness > 1.0) ? 1.0 : f_thickness));

                    double penOffset = CTLineGetPenOffsetForFlush(line, horiz_flush, (i_width  - HORIZONTAL_MARGIN*2));
                    penPosition.x = HORIZONTAL_MARGIN + penOffset;
                    penPosition.y -= ascent;
                    CGContextSetTextPosition(p_context, penPosition.x, penPosition.y);
                    CTLineDraw(line, p_context);
                    penPosition.y -= descent + leading;

                }
                *pi_textblock_height = i_height - penPosition.y;

                CFRelease(frame);
            }
            CFRelease(framesetter);
        }
        CGContextFlush(p_context);
        CGContextRelease(p_context);
    }
    if (p_colorSpace) CGColorSpaceRelease(p_colorSpace);

    return p_offScreen;
}

static int GetFontSize(filter_t *p_filter)
{
    int i_size = 0;

    int i_ratio = var_CreateGetInteger( p_filter, "quartztext-rel-fontsize" );
    if( i_ratio > 0 )
        i_size = (int)p_filter->fmt_out.video.i_height / i_ratio;

    if( i_size <= 0 )
    {
        msg_Warn( p_filter, "invalid fontsize, using 12" );
        i_size = 12;
    }
    return i_size;
}

static int RenderYUVA(filter_t *p_filter, subpicture_region_t *p_region,
                       CFMutableAttributedStringRef p_attrString)
{
    offscreen_bitmap_t *p_offScreen = NULL;
    unsigned      i_textblock_height = 0;

    unsigned i_width = p_filter->fmt_out.video.i_visible_width;
    unsigned i_height = p_filter->fmt_out.video.i_visible_height;
    unsigned i_text_align = p_region->i_align & 0x3;

    if (!p_attrString) {
        msg_Err(p_filter, "Invalid argument to RenderYUVA");
        return VLC_EGENERIC;
    }

    p_offScreen = Compose(i_text_align, p_attrString,
                           i_width, i_height, &i_textblock_height);

    if (!p_offScreen) {
        msg_Err(p_filter, "No offscreen buffer");
        return VLC_EGENERIC;
    }

    uint8_t *p_dst_y,*p_dst_u,*p_dst_v,*p_dst_a;
    video_format_t fmt;
    int i_offset;
    unsigned i_pitch;
    uint8_t i_y, i_u, i_v; // YUV values, derived from incoming RGB

    // Create a new subpicture region
    memset(&fmt, 0, sizeof(video_format_t));
    fmt.i_chroma = VLC_CODEC_YUVA;
    fmt.i_width = fmt.i_visible_width = i_width;
    fmt.i_height = fmt.i_visible_height = __MIN(i_height, i_textblock_height + VERTICAL_MARGIN * 2);
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    p_region->p_picture = picture_NewFromFormat(&fmt);
    if (!p_region->p_picture)
        return VLC_EGENERIC;
    p_region->fmt = fmt;

    p_dst_y = p_region->p_picture->Y_PIXELS;
    p_dst_u = p_region->p_picture->U_PIXELS;
    p_dst_v = p_region->p_picture->V_PIXELS;
    p_dst_a = p_region->p_picture->A_PIXELS;
    i_pitch = p_region->p_picture->A_PITCH;

    i_offset = (i_height + VERTICAL_MARGIN < fmt.i_height) ? VERTICAL_MARGIN *i_pitch : 0 ;
    for (unsigned y = 0; y < fmt.i_height; y++) {
        for (unsigned x = 0; x < fmt.i_width; x++) {
            int i_alpha = p_offScreen->p_data[ y * p_offScreen->i_bytesPerRow + x * p_offScreen->i_bytesPerPixel     ];
            int i_red   = p_offScreen->p_data[ y * p_offScreen->i_bytesPerRow + x * p_offScreen->i_bytesPerPixel + 1 ];
            int i_green = p_offScreen->p_data[ y * p_offScreen->i_bytesPerRow + x * p_offScreen->i_bytesPerPixel + 2 ];
            int i_blue  = p_offScreen->p_data[ y * p_offScreen->i_bytesPerRow + x * p_offScreen->i_bytesPerPixel + 3 ];

            i_y = (uint8_t)__MIN(abs(2104 * i_red  + 4130 * i_green +
                              802 * i_blue + 4096 + 131072) >> 13, 235);
            i_u = (uint8_t)__MIN(abs(-1214 * i_red  + -2384 * i_green +
                             3598 * i_blue + 4096 + 1048576) >> 13, 240);
            i_v = (uint8_t)__MIN(abs(3598 * i_red + -3013 * i_green +
                              -585 * i_blue + 4096 + 1048576) >> 13, 240);

            p_dst_y[ i_offset + x ] = i_y;
            p_dst_u[ i_offset + x ] = i_u;
            p_dst_v[ i_offset + x ] = i_v;
            p_dst_a[ i_offset + x ] = i_alpha;
        }
        i_offset += i_pitch;
    }

    free(p_offScreen->p_data);
    free(p_offScreen);

    return VLC_SUCCESS;
}
