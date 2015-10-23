/*****************************************************************************
 * quartztext.c : Put text on the video, using Mac OS X Quartz Engine
 *****************************************************************************
 * Copyright (C) 2007, 2009, 2012, 2015 VLC authors and VideoLAN
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
#define DEFAULT_MONOFONT       "Andale-Mono"
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

static int GetFontSize(filter_t *p_filter);
static int RenderYUVA(filter_t *p_filter, subpicture_region_t *p_region,
                       CFMutableAttributedStringRef p_attrString);

static void setFontAttibutes(char *psz_fontname, int i_font_size, uint32_t i_font_color,
                              bool b_bold, bool b_italic, bool b_underline, bool b_halfwidth,
                              int i_spacing,
                              CFRange p_range, CFMutableAttributedStringRef p_attrString);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* The preferred way to set font style information is for it to come from the
 * subtitle file, and for it to be rendered with RenderHtml instead of
 * RenderText. */
#define FONT_TEXT N_("Font")
#define FONT_LONGTEXT N_("Name for the font you want to use")
#define MONOSPACE_FONT_TEXT N_("Monospace Font")
#define FONTSIZER_TEXT N_("Relative font size")
#define FONTSIZER_LONGTEXT N_("This is the relative default size of the " \
    "fonts that will be rendered on the video. If absolute font size is set, "\
    "relative size will be overridden.")
#define COLOR_TEXT N_("Text default color")
#define COLOR_LONGTEXT N_("The color of the text that will be rendered on "\
    "the video. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white")
#define OUTLINE_TEXT N_("Add outline")
#define SHADOW_TEXT N_("Add shadow")

static const int pi_color_values[] = {
  0x00000000, 0x00808080, 0x00C0C0C0, 0x00FFFFFF, 0x00800000,
  0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00808000, 0x00008000, 0x00008080,
  0x0000FF00, 0x00800080, 0x00000080, 0x000000FF, 0x0000FFFF };

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
    add_string("quartztext-monofont", DEFAULT_MONOFONT, MONOSPACE_FONT_TEXT, FONT_LONGTEXT,
              false)
    add_integer("quartztext-rel-fontsize", DEFAULT_REL_FONT_SIZE, FONTSIZER_TEXT,
                 FONTSIZER_LONGTEXT, false)
        change_integer_list(pi_sizes, ppsz_sizes_text)
    add_integer("quartztext-color", 0x00FFFFFF, COLOR_TEXT,
                 COLOR_LONGTEXT, false)
        change_integer_list(pi_color_values, ppsz_color_descriptions)
    add_bool("quartztext-outline", false, OUTLINE_TEXT, NULL, false)
    add_bool("quartztext-shadow", true, SHADOW_TEXT, NULL, false)
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
    text_style_t  *p_default_style;

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

    p_sys->p_default_style = text_style_Create( STYLE_FULLY_SET );
    if(unlikely(!p_sys->p_default_style))
    {
        free(p_sys);
        return VLC_ENOMEM;
    }
    p_sys->p_default_style->psz_fontname = var_CreateGetString(p_this, "quartztext-font");;
    p_sys->p_default_style->psz_monofontname = var_CreateGetString(p_this, "quartztext-monofont");
    p_sys->p_default_style->i_font_size = GetFontSize(p_filter);

    p_sys->p_default_style->i_font_color = VLC_CLIP(var_CreateGetInteger(p_this, "quartztext-color") , 0, 0xFFFFFF);
    p_sys->p_default_style->i_features |= STYLE_HAS_FONT_COLOR;

    if( var_InheritBool(p_this, "quartztext-outline") )
    {
        p_sys->p_default_style->i_style_flags |= STYLE_OUTLINE;
        p_sys->p_default_style->i_features |= STYLE_HAS_FLAGS;
    }

    if( var_InheritBool(p_this, "quartztext-shadow") )
    {
        p_sys->p_default_style->i_style_flags |= STYLE_SHADOW;
        p_sys->p_default_style->i_features |= STYLE_HAS_FLAGS;
    }

    if (p_sys->p_default_style->i_font_size <= 0)
    {
        vlc_value_t val;
        msg_Warn(p_filter, "invalid fontsize, using 12");
        if (VLC_SUCCESS == var_Get(p_filter, "scale", &val))
            p_sys->p_default_style->i_font_size = 12 * val.i_int / 1000;
        else
            p_sys->p_default_style->i_font_size = 12;
    }

    p_filter->pf_render = RenderText;

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
        for (int k = 0; k < p_sys->i_fonts; k++)
            ATSFontDeactivate(p_sys->p_fonts[k], NULL, kATSOptionFlagsDefault);

        free(p_sys->p_fonts);
    }
#endif
    text_style_Delete( p_sys->p_default_style );
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

/* Renders a text subpicture region into another one.
 * It is used as pf_add_string callback in the vout method by this module */
static int RenderText(filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in,
                       const vlc_fourcc_t *p_chroma_list)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    char         *psz_render_string = NULL;
    VLC_UNUSED(p_chroma_list);

    // Sanity check
    if (!p_region_in || !p_region_out) {
        msg_Warn(p_filter, "No region");
        return VLC_EGENERIC;
    }

    /* Convert to segments to single raw text */
    /* FIXME: render split segment/style */
    size_t i_len = 0;
    for (const text_segment_t *p_text = p_region_in->p_text; p_text != NULL; p_text = p_text->p_next)
    {
        i_len += (p_text->psz_text) ? strlen(p_text->psz_text) : 0;
    }
    if(i_len == 0)
        return VLC_EGENERIC;

    char *psz = psz_render_string = malloc(i_len + 1);
    if(!psz_render_string)
        return VLC_EGENERIC;
    *psz = 0;

    for (const text_segment_t *p_text = p_region_in->p_text; p_text != NULL; p_text = p_text->p_next)
    {
        if(p_text->psz_text)
            strcat(psz, p_text->psz_text);
    }

    const int i_font_size = GetFontSize(p_filter);

    p_region_out->i_x = p_region_in->i_x;
    p_region_out->i_y = p_region_in->i_y;

    CFMutableAttributedStringRef p_attrString = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);

    if (p_attrString) {
        CFStringRef   p_cfString;
        int           len;

        p_cfString = CFStringCreateWithCString(NULL, psz_render_string, kCFStringEncodingUTF8);
        if (!p_cfString)
        {
            CFRelease(p_attrString);
            free(psz_render_string);
            return VLC_EGENERIC;
        }

        CFAttributedStringReplaceString(p_attrString, CFRangeMake(0, 0), p_cfString);
        CFRelease(p_cfString);
        len = CFAttributedStringGetLength(p_attrString);

        setFontAttibutes((p_sys->p_default_style->i_style_flags & STYLE_MONOSPACED) ? p_sys->p_default_style->psz_monofontname :
                                                                                      p_sys->p_default_style->psz_fontname,
                         i_font_size,
                         p_sys->p_default_style->i_font_color,
                         p_sys->p_default_style->i_style_flags & STYLE_BOLD,
                         p_sys->p_default_style->i_style_flags & STYLE_ITALIC,
                         p_sys->p_default_style->i_style_flags & STYLE_UNDERLINE,
                         p_sys->p_default_style->i_style_flags & STYLE_HALFWIDTH,
                         p_sys->p_default_style->i_spacing,
                         CFRangeMake(0, len), p_attrString);

        RenderYUVA(p_filter, p_region_out, p_attrString);
        CFRelease(p_attrString);
    }

    free(psz_render_string);

    return VLC_SUCCESS;
}

static void setFontAttibutes(char *psz_fontname, int i_font_size, uint32_t i_font_color,
        bool b_bold, bool b_italic, bool b_underline, bool b_halfwidth,
        int i_spacing,
        CFRange p_range, CFMutableAttributedStringRef p_attrString)
{
    CFStringRef p_cfString;
    CTFontRef   p_font;

    int i_font_width = b_halfwidth ? i_font_size / 2 : i_font_size;
    CGAffineTransform trans = CGAffineTransformMakeScale((float)i_font_width
                                                         / i_font_size, 1.0);

    // fallback on default
    if (!psz_fontname)
        psz_fontname = (char *)DEFAULT_FONT;

    p_cfString = CFStringCreateWithCString(kCFAllocatorDefault,
                                            psz_fontname,
                                            kCFStringEncodingUTF8);
    p_font     = CTFontCreateWithName(p_cfString,
                                       (float)i_font_size,
                                       &trans);
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

    // spacing
    if (i_spacing > 0)
    {
        CGFloat spacing = i_spacing;
        CFNumberRef spacingCFNum = CFNumberCreate(NULL,
                kCFNumberCGFloatType, &spacing);
        CFAttributedStringSetAttribute(p_attrString,
                                        p_range,
                                        kCTKernAttributeName,
                                        spacingCFNum);
        CFRelease(spacingCFNum);
    }
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
            if (&CGContextSetAllowsAntialiasing != NULL)
                CGContextSetAllowsAntialiasing(p_context, true);
        }
        *pp_memory = p_bitmap;
    }

    return p_context;
}

static offscreen_bitmap_t *Compose(filter_t *p_filter,
                                    subpicture_region_t *p_region,
                                    CFMutableAttributedStringRef p_attrString,
                                    unsigned i_width,
                                    unsigned i_height,
                                    unsigned *pi_textblock_height)
{
    filter_sys_t *p_sys   = p_filter->p_sys;
    offscreen_bitmap_t  *p_offScreen  = NULL;
    CGColorSpaceRef      p_colorSpace = NULL;
    CGContextRef         p_context = NULL;

    p_context = CreateOffScreenContext(i_width, i_height, &p_offScreen, &p_colorSpace);

    *pi_textblock_height = 0;
    if (p_context) {
        float horiz_flush;

        CGContextSetTextMatrix(p_context, CGAffineTransformIdentity);

        if (p_region->i_align & SUBPICTURE_ALIGN_RIGHT)
            horiz_flush = 1.0;
        else if ((p_region->i_align & SUBPICTURE_ALIGN_LEFT) == 0)
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
            if (p_sys->p_default_style->i_style_flags & STYLE_OUTLINE)
            {
                CGContextSetRGBStrokeColor(p_context, 0, 0, 0, 0.5);
                CGContextSetTextDrawingMode(p_context, kCGTextFillStroke);
            }

            // Shadow
            if (p_sys->p_default_style->i_style_flags & STYLE_SHADOW)
            {
                // TODO: Use CGContextSetShadowWithColor.
                // TODO: Use user defined parrameters (color, distance, etc.)
                CGContextSetShadow(p_context, CGSizeMake(3.0f, -3.0f), 2.0f);
            }

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
                    if (horiz_flush == 0.0)
                        penPosition.x = p_region->i_x;
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

    if (!p_attrString) {
        msg_Err(p_filter, "Invalid argument to RenderYUVA");
        return VLC_EGENERIC;
    }

    p_offScreen = Compose(p_filter, p_region, p_attrString,
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
    if (!p_region->p_picture) {
        free(p_offScreen->p_data);
        free(p_offScreen);
        return VLC_EGENERIC;
    }
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
