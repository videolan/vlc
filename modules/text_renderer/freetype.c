/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Bernie Purcell <bitmap@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Felix Paul Kühne <fkuehne@videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>                        /* stream_MemoryNew */
#include <vlc_input.h>                         /* vlc_input_attachment_* */
#include <vlc_xml.h>                           /* xml_reader */
#include <vlc_dialog.h>                        /* FcCache dialog */
#include <vlc_filter.h>                                      /* filter_sys_t */
#include <vlc_text_style.h>                                   /* text_style_t*/

/* Freetype */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H
#include FT_SYNTHESIS_H

#define FT_FLOOR(X)     ((X & -64) >> 6)
#define FT_CEIL(X)      (((X + 63) & -64) >> 6)
#ifndef FT_MulFix
 #define FT_MulFix(v, s) (((v)*(s))>>16)
#endif

/* RTL */
#if defined(HAVE_FRIBIDI)
# include <fribidi/fribidi.h>
#endif

/* apple stuff */
#ifdef __APPLE__
# include <TargetConditionals.h>
# undef HAVE_FONTCONFIG
# define HAVE_GET_FONT_BY_FAMILY_NAME
#endif

/* Win32 */
#ifdef _WIN32
# undef HAVE_FONTCONFIG
# if !VLC_WINSTORE_APP
#  define HAVE_GET_FONT_BY_FAMILY_NAME
# endif
#endif

/* FontConfig */
#ifdef HAVE_FONTCONFIG
# define HAVE_GET_FONT_BY_FAMILY_NAME
#endif

#include <assert.h>

#include "text_renderer.h"
#include "platform_fonts.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Create ( vlc_object_t * );
static void Destroy( vlc_object_t * );

#define FONT_TEXT N_("Font")
#define MONOSPACE_FONT_TEXT N_("Monospace Font")

#define FAMILY_LONGTEXT N_("Font family for the font you want to use")
#define FONT_LONGTEXT N_("Font file for the font you want to use")

#define FONTSIZE_TEXT N_("Font size in pixels")
#define FONTSIZE_LONGTEXT N_("This is the default size of the fonts " \
    "that will be rendered on the video. " \
    "If set to something different than 0 this option will override the " \
    "relative font size." )
#define OPACITY_TEXT N_("Text opacity")
#define OPACITY_LONGTEXT N_("The opacity (inverse of transparency) of the " \
    "text that will be rendered on the video. 0 = transparent, " \
    "255 = totally opaque. " )
#define COLOR_TEXT N_("Text default color")
#define COLOR_LONGTEXT N_("The color of the text that will be rendered on "\
    "the video. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white" )
#define FONTSIZER_TEXT N_("Relative font size")
#define FONTSIZER_LONGTEXT N_("This is the relative default size of the " \
    "fonts that will be rendered on the video. If absolute font size is set, "\
    "relative size will be overridden." )
#define BOLD_TEXT N_("Force bold")

#define BG_OPACITY_TEXT N_("Background opacity")
#define BG_COLOR_TEXT N_("Background color")

#define OUTLINE_OPACITY_TEXT N_("Outline opacity")
#define OUTLINE_COLOR_TEXT N_("Outline color")
#define OUTLINE_THICKNESS_TEXT N_("Outline thickness")

#define SHADOW_OPACITY_TEXT N_("Shadow opacity")
#define SHADOW_COLOR_TEXT N_("Shadow color")
#define SHADOW_ANGLE_TEXT N_("Shadow angle")
#define SHADOW_DISTANCE_TEXT N_("Shadow distance")


static const int pi_sizes[] = { 20, 18, 16, 12, 6 };
static const char *const ppsz_sizes_text[] = {
    N_("Smaller"), N_("Small"), N_("Normal"), N_("Large"), N_("Larger") };
#define YUVP_TEXT N_("Use YUVP renderer")
#define YUVP_LONGTEXT N_("This renders the font using \"paletized YUV\". " \
  "This option is only needed if you want to encode into DVB subtitles" )

static const int pi_color_values[] = {
  0x00000000, 0x00808080, 0x00C0C0C0, 0x00FFFFFF, 0x00800000,
  0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00808000, 0x00008000, 0x00008080,
  0x0000FF00, 0x00800080, 0x00000080, 0x000000FF, 0x0000FFFF };

static const char *const ppsz_color_descriptions[] = {
  N_("Black"), N_("Gray"), N_("Silver"), N_("White"), N_("Maroon"),
  N_("Red"), N_("Fuchsia"), N_("Yellow"), N_("Olive"), N_("Green"), N_("Teal"),
  N_("Lime"), N_("Purple"), N_("Navy"), N_("Blue"), N_("Aqua") };

static const int pi_outline_thickness[] = {
    0, 2, 4, 6,
};
static const char *const ppsz_outline_thickness[] = {
    N_("None"), N_("Thin"), N_("Normal"), N_("Thick"),
};

vlc_module_begin ()
    set_shortname( N_("Text renderer"))
    set_description( N_("Freetype2 font renderer") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SUBPIC )

#ifdef HAVE_GET_FONT_BY_FAMILY_NAME
    add_font( "freetype-font", DEFAULT_FAMILY, FONT_TEXT, FAMILY_LONGTEXT, false )
    add_font( "freetype-monofont", DEFAULT_MONOSPACE_FAMILY, MONOSPACE_FONT_TEXT, FAMILY_LONGTEXT, false )
#else
    add_loadfile( "freetype-font", DEFAULT_FONT_FILE, FONT_TEXT, FONT_LONGTEXT, false )
    add_loadfile( "freetype-monofont", DEFAULT_MONOSPACE_FONT_FILE, MONOSPACE_FONT_TEXT, FONT_LONGTEXT, false )
#endif

    add_integer( "freetype-fontsize", 0, FONTSIZE_TEXT,
                 FONTSIZE_LONGTEXT, true )
        change_integer_range( 0, 4096)
        change_safe()

    add_integer( "freetype-rel-fontsize", 16, FONTSIZER_TEXT,
                 FONTSIZER_LONGTEXT, false )
        change_integer_list( pi_sizes, ppsz_sizes_text )
        change_safe()

    /* opacity valid on 0..255, with default 255 = fully opaque */
    add_integer_with_range( "freetype-opacity", 255, 0, 255,
        OPACITY_TEXT, OPACITY_LONGTEXT, false )
        change_safe()

    /* hook to the color values list, with default 0x00ffffff = white */
    add_rgb( "freetype-color", 0x00FFFFFF, COLOR_TEXT,
                 COLOR_LONGTEXT, false )
        change_integer_list( pi_color_values, ppsz_color_descriptions )
        change_safe()

    add_bool( "freetype-bold", false, BOLD_TEXT, NULL, false )
        change_safe()

    add_integer_with_range( "freetype-background-opacity", 0, 0, 255,
                            BG_OPACITY_TEXT, NULL, false )
        change_safe()
    add_rgb( "freetype-background-color", 0x00000000, BG_COLOR_TEXT,
             NULL, false )
        change_integer_list( pi_color_values, ppsz_color_descriptions )
        change_safe()

    add_integer_with_range( "freetype-outline-opacity", 255, 0, 255,
                            OUTLINE_OPACITY_TEXT, NULL, false )
        change_safe()
    add_rgb( "freetype-outline-color", 0x00000000, OUTLINE_COLOR_TEXT,
             NULL, false )
        change_integer_list( pi_color_values, ppsz_color_descriptions )
        change_safe()
    add_integer_with_range( "freetype-outline-thickness", 4, 0, 50, OUTLINE_THICKNESS_TEXT,
             NULL, false )
        change_integer_list( pi_outline_thickness, ppsz_outline_thickness )
        change_safe()

    add_integer_with_range( "freetype-shadow-opacity", 128, 0, 255,
                            SHADOW_OPACITY_TEXT, NULL, false )
        change_safe()
    add_rgb( "freetype-shadow-color", 0x00000000, SHADOW_COLOR_TEXT,
             NULL, false )
        change_integer_list( pi_color_values, ppsz_color_descriptions )
        change_safe()
    add_float_with_range( "freetype-shadow-angle", -45, -360, 360,
                          SHADOW_ANGLE_TEXT, NULL, false )
        change_safe()
    add_float_with_range( "freetype-shadow-distance", 0.06, 0.0, 1.0,
                          SHADOW_DISTANCE_TEXT, NULL, false )
        change_safe()

    add_obsolete_integer( "freetype-effect" );

    add_bool( "freetype-yuvp", false, YUVP_TEXT,
              YUVP_LONGTEXT, true )
    set_capability( "text renderer", 100 )
    add_shortcut( "text" )
    set_callbacks( Create, Destroy )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    FT_BitmapGlyph p_glyph;
    FT_BitmapGlyph p_outline;
    FT_BitmapGlyph p_shadow;
    uint32_t       i_color;             /* ARGB color */
    int            i_line_offset;       /* underline/strikethrough offset */
    int            i_line_thickness;    /* underline/strikethrough thickness */
} line_character_t;

typedef struct line_desc_t line_desc_t;
struct line_desc_t
{
    line_desc_t      *p_next;

    int              i_width;
    int              i_height;
    int              i_base_line;
    int              i_character_count;
    line_character_t *p_character;
};

/*****************************************************************************
 * filter_sys_t: freetype local data
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the freetype specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    FT_Library     p_library;   /* handle to library     */
    FT_Face        p_face;      /* handle to face object */
    FT_Stroker     p_stroker;   /* handle to path stroker object */

    xml_reader_t  *p_xml;       /* vlc xml parser */

    text_style_t   style;       /* Current Style */

    /* More styles... */
    float          f_shadow_vector_x;
    float          f_shadow_vector_y;
    int            i_default_font_size;

    /* Attachments */
    input_attachment_t **pp_font_attachments;
    int                  i_font_attachments;

    char * (*pf_select) (filter_t *, const char* family,
                               bool bold, bool italic, int size,
                               int *index);

};

/* */
static void YUVFromRGB( uint32_t i_argb,
                    uint8_t *pi_y, uint8_t *pi_u, uint8_t *pi_v )
{
    int i_red   = ( i_argb & 0x00ff0000 ) >> 16;
    int i_green = ( i_argb & 0x0000ff00 ) >>  8;
    int i_blue  = ( i_argb & 0x000000ff );

    *pi_y = (uint8_t)__MIN(abs( 2104 * i_red  + 4130 * i_green +
                      802 * i_blue + 4096 + 131072 ) >> 13, 235);
    *pi_u = (uint8_t)__MIN(abs( -1214 * i_red  + -2384 * i_green +
                     3598 * i_blue + 4096 + 1048576) >> 13, 240);
    *pi_v = (uint8_t)__MIN(abs( 3598 * i_red + -3013 * i_green +
                      -585 * i_blue + 4096 + 1048576) >> 13, 240);
}
static void RGBFromRGB( uint32_t i_argb,
                        uint8_t *pi_r, uint8_t *pi_g, uint8_t *pi_b )
{
    *pi_r = ( i_argb & 0x00ff0000 ) >> 16;
    *pi_g = ( i_argb & 0x0000ff00 ) >>  8;
    *pi_b = ( i_argb & 0x000000ff );
}

/*****************************************************************************
 * Make any TTF/OTF fonts present in the attachments of the media file
 * and store them for later use by the FreeType Engine
 *****************************************************************************/
static int LoadFontsFromAttachments( filter_t *p_filter )
{
    filter_sys_t         *p_sys = p_filter->p_sys;
    input_attachment_t  **pp_attachments;
    int                   i_attachments_cnt;

    if( filter_GetInputAttachments( p_filter, &pp_attachments, &i_attachments_cnt ) )
        return VLC_EGENERIC;

    p_sys->i_font_attachments = 0;
    p_sys->pp_font_attachments = malloc( i_attachments_cnt * sizeof(*p_sys->pp_font_attachments));
    if( !p_sys->pp_font_attachments )
        return VLC_ENOMEM;

    for( int k = 0; k < i_attachments_cnt; k++ )
    {
        input_attachment_t *p_attach = pp_attachments[k];

        if( ( !strcmp( p_attach->psz_mime, "application/x-truetype-font" ) || // TTF
              !strcmp( p_attach->psz_mime, "application/x-font-otf" ) ) &&    // OTF
            p_attach->i_data > 0 && p_attach->p_data )
        {
            p_sys->pp_font_attachments[ p_sys->i_font_attachments++ ] = p_attach;
        }
        else
        {
            vlc_input_attachment_Delete( p_attach );
        }
    }
    free( pp_attachments );

    return VLC_SUCCESS;
}

static int GetFontSize( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    int           i_size = 0;

    if( p_sys->i_default_font_size )
    {
        i_size = p_sys->i_default_font_size;
    }
    else
    {
        int i_ratio = var_InheritInteger( p_filter, "freetype-rel-fontsize" );
        if( i_ratio > 0 )
        {
            i_size = (int)p_filter->fmt_out.video.i_height / i_ratio;
        }
    }
    if( i_size <= 0 )
    {
        msg_Warn( p_filter, "invalid fontsize, using 12" );
        i_size = 12;
    }
    return i_size;
}

static int SetFontSize( filter_t *p_filter, int i_size )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !i_size )
    {
        i_size = GetFontSize( p_filter );

        msg_Dbg( p_filter, "using fontsize: %i", i_size );
    }

    p_sys->style.i_font_size = i_size;

    if( FT_Set_Pixel_Sizes( p_sys->p_face, 0, i_size ) )
    {
        msg_Err( p_filter, "couldn't set font size to %d", i_size );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * RenderYUVP: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
static int RenderYUVP( filter_t *p_filter, subpicture_region_t *p_region,
                       line_desc_t *p_line,
                       FT_BBox *p_bbox )
{
    VLC_UNUSED(p_filter);
    static const uint8_t pi_gamma[16] =
        {0x00, 0x52, 0x84, 0x96, 0xb8, 0xca, 0xdc, 0xee, 0xff,
          0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    uint8_t *p_dst;
    video_format_t fmt;
    int i, x, y, i_pitch;
    uint8_t i_y, i_u, i_v; /* YUV values, derived from incoming RGB */

    /* Create a new subpicture region */
    video_format_Init( &fmt, VLC_CODEC_YUVP );
    fmt.i_width          =
    fmt.i_visible_width  = p_bbox->xMax - p_bbox->xMin + 4;
    fmt.i_height         =
    fmt.i_visible_height = p_bbox->yMax - p_bbox->yMin + 4;

    assert( !p_region->p_picture );
    p_region->p_picture = picture_NewFromFormat( &fmt );
    if( !p_region->p_picture )
        return VLC_EGENERIC;
    fmt.p_palette = p_region->fmt.p_palette ? p_region->fmt.p_palette : malloc(sizeof(*fmt.p_palette));
    p_region->fmt = fmt;

    /* Calculate text color components
     * Only use the first color */
    int i_alpha = (p_line->p_character[0].i_color >> 24) & 0xff;
    YUVFromRGB( p_line->p_character[0].i_color, &i_y, &i_u, &i_v );

    /* Build palette */
    fmt.p_palette->i_entries = 16;
    for( i = 0; i < 8; i++ )
    {
        fmt.p_palette->palette[i][0] = 0;
        fmt.p_palette->palette[i][1] = 0x80;
        fmt.p_palette->palette[i][2] = 0x80;
        fmt.p_palette->palette[i][3] = pi_gamma[i];
        fmt.p_palette->palette[i][3] =
            (int)fmt.p_palette->palette[i][3] * i_alpha / 255;
    }
    for( i = 8; i < fmt.p_palette->i_entries; i++ )
    {
        fmt.p_palette->palette[i][0] = i * 16 * i_y / 256;
        fmt.p_palette->palette[i][1] = i_u;
        fmt.p_palette->palette[i][2] = i_v;
        fmt.p_palette->palette[i][3] = pi_gamma[i];
        fmt.p_palette->palette[i][3] =
            (int)fmt.p_palette->palette[i][3] * i_alpha / 255;
    }

    p_dst = p_region->p_picture->Y_PIXELS;
    i_pitch = p_region->p_picture->Y_PITCH;

    /* Initialize the region pixels */
    memset( p_dst, 0, i_pitch * p_region->fmt.i_height );

    for( ; p_line != NULL; p_line = p_line->p_next )
    {
        int i_align_left = 0;
        if( p_line->i_width < (int)fmt.i_visible_width )
        {
            if( (p_region->i_align & 0x3) == SUBPICTURE_ALIGN_RIGHT )
                i_align_left = ( fmt.i_visible_width - p_line->i_width );
            else if( (p_region->i_align & 0x3) != SUBPICTURE_ALIGN_LEFT )
                i_align_left = ( fmt.i_visible_width - p_line->i_width ) / 2;
        }
        int i_align_top = 0;

        for( i = 0; i < p_line->i_character_count; i++ )
        {
            const line_character_t *ch = &p_line->p_character[i];
            FT_BitmapGlyph p_glyph = ch->p_glyph;

            int i_glyph_y = i_align_top  - p_glyph->top  + p_bbox->yMax + p_line->i_base_line;
            int i_glyph_x = i_align_left + p_glyph->left - p_bbox->xMin;

            for( y = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++ )
                {
                    if( p_glyph->bitmap.buffer[y * p_glyph->bitmap.width + x] )
                        p_dst[(i_glyph_y + y) * i_pitch + (i_glyph_x + x)] =
                            (p_glyph->bitmap.buffer[y * p_glyph->bitmap.width + x] + 8)/16;
                }
            }
        }
    }

    /* Outlining (find something better than nearest neighbour filtering ?) */
    if( 1 )
    {
        uint8_t *p_dst = p_region->p_picture->Y_PIXELS;
        uint8_t *p_top = p_dst; /* Use 1st line as a cache */
        uint8_t left, current;

        for( y = 1; y < (int)fmt.i_height - 1; y++ )
        {
            if( y > 1 ) memcpy( p_top, p_dst, fmt.i_width );
            p_dst += p_region->p_picture->Y_PITCH;
            left = 0;

            for( x = 1; x < (int)fmt.i_width - 1; x++ )
            {
                current = p_dst[x];
                p_dst[x] = ( 8 * (int)p_dst[x] + left + p_dst[x+1] + p_top[x -1]+ p_top[x] + p_top[x+1] +
                             p_dst[x -1 + p_region->p_picture->Y_PITCH ] + p_dst[x + p_region->p_picture->Y_PITCH] + p_dst[x + 1 + p_region->p_picture->Y_PITCH]) / 16;
                left = current;
            }
        }
        memset( p_top, 0, fmt.i_width );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * RenderYUVA: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
static void FillYUVAPicture( picture_t *p_picture,
                             int i_a, int i_y, int i_u, int i_v )
{
    memset( p_picture->p[0].p_pixels, i_y,
            p_picture->p[0].i_pitch * p_picture->p[0].i_lines );
    memset( p_picture->p[1].p_pixels, i_u,
            p_picture->p[1].i_pitch * p_picture->p[1].i_lines );
    memset( p_picture->p[2].p_pixels, i_v,
            p_picture->p[2].i_pitch * p_picture->p[2].i_lines );
    memset( p_picture->p[3].p_pixels, i_a,
            p_picture->p[3].i_pitch * p_picture->p[3].i_lines );
}

static inline void BlendYUVAPixel( picture_t *p_picture,
                                   int i_picture_x, int i_picture_y,
                                   int i_a, int i_y, int i_u, int i_v,
                                   int i_alpha )
{
    int i_an = i_a * i_alpha / 255;

    uint8_t *p_y = &p_picture->p[0].p_pixels[i_picture_y * p_picture->p[0].i_pitch + i_picture_x];
    uint8_t *p_u = &p_picture->p[1].p_pixels[i_picture_y * p_picture->p[1].i_pitch + i_picture_x];
    uint8_t *p_v = &p_picture->p[2].p_pixels[i_picture_y * p_picture->p[2].i_pitch + i_picture_x];
    uint8_t *p_a = &p_picture->p[3].p_pixels[i_picture_y * p_picture->p[3].i_pitch + i_picture_x];

    int i_ao = *p_a;
    if( i_ao == 0 )
    {
        *p_y = i_y;
        *p_u = i_u;
        *p_v = i_v;
        *p_a = i_an;
    }
    else
    {
        *p_a = 255 - (255 - *p_a) * (255 - i_an) / 255;
        if( *p_a != 0 )
        {
            *p_y = ( *p_y * i_ao * (255 - i_an) / 255 + i_y * i_an ) / *p_a;
            *p_u = ( *p_u * i_ao * (255 - i_an) / 255 + i_u * i_an ) / *p_a;
            *p_v = ( *p_v * i_ao * (255 - i_an) / 255 + i_v * i_an ) / *p_a;
        }
    }
}

static void FillRGBAPicture( picture_t *p_picture,
                             int i_a, int i_r, int i_g, int i_b )
{
    for( int dy = 0; dy < p_picture->p[0].i_visible_lines; dy++ )
    {
        for( int dx = 0; dx < p_picture->p[0].i_visible_pitch; dx += 4 )
        {
            uint8_t *p_rgba = &p_picture->p->p_pixels[dy * p_picture->p->i_pitch + dx];
            p_rgba[0] = i_r;
            p_rgba[1] = i_g;
            p_rgba[2] = i_b;
            p_rgba[3] = i_a;
        }
    }
}

static inline void BlendRGBAPixel( picture_t *p_picture,
                                   int i_picture_x, int i_picture_y,
                                   int i_a, int i_r, int i_g, int i_b,
                                   int i_alpha )
{
    int i_an = i_a * i_alpha / 255;

    uint8_t *p_rgba = &p_picture->p->p_pixels[i_picture_y * p_picture->p->i_pitch + 4 * i_picture_x];

    int i_ao = p_rgba[3];
    if( i_ao == 0 )
    {
        p_rgba[0] = i_r;
        p_rgba[1] = i_g;
        p_rgba[2] = i_b;
        p_rgba[3] = i_an;
    }
    else
    {
        p_rgba[3] = 255 - (255 - p_rgba[3]) * (255 - i_an) / 255;
        if( p_rgba[3] != 0 )
        {
            p_rgba[0] = ( p_rgba[0] * i_ao * (255 - i_an) / 255 + i_r * i_an ) / p_rgba[3];
            p_rgba[1] = ( p_rgba[1] * i_ao * (255 - i_an) / 255 + i_g * i_an ) / p_rgba[3];
            p_rgba[2] = ( p_rgba[2] * i_ao * (255 - i_an) / 255 + i_b * i_an ) / p_rgba[3];
        }
    }
}

static void FillARGBPicture(picture_t *pic, int a, int r, int g, int b)
{
    if (a == 0)
        r = g = b = 0;
    if (a == r && a == b && a == g)
    {   /* fast path */
        memset(pic->p->p_pixels, a, pic->p->i_visible_lines * pic->p->i_pitch);
        return;
    }

    uint_fast32_t pixel = VLC_FOURCC(a, r, g, b);
    uint8_t *line = pic->p->p_pixels;

    for (unsigned lines = pic->p->i_visible_lines; lines > 0; lines--)
    {
        uint32_t *pixels = (uint32_t *)line;
        for (unsigned cols = pic->p->i_visible_pitch; cols > 0; cols -= 4)
            *(pixels++) = pixel;
        line += pic->p->i_pitch;
    }
}

static inline void BlendARGBPixel(picture_t *pic, int pic_x, int pic_y,
                                  int a, int r, int g, int b, int alpha)
{
    uint8_t *rgba = &pic->p->p_pixels[pic_y * pic->p->i_pitch + 4 * pic_x];
    int an = a * alpha / 255;
    int ao = rgba[3];

    if (ao == 0)
    {
        rgba[0] = an;
        rgba[1] = r;
        rgba[2] = g;
        rgba[3] = b;
    }
    else
    {
        rgba[0] = 255 - (255 - rgba[0]) * (255 - an) / 255;
        if (rgba[0] != 0)
        {
            rgba[1] = (rgba[1] * ao * (255 - an) / 255 + r * an ) / rgba[0];
            rgba[2] = (rgba[2] * ao * (255 - an) / 255 + g * an ) / rgba[0];
            rgba[3] = (rgba[3] * ao * (255 - an) / 255 + b * an ) / rgba[0];
        }
    }
}

static inline void BlendAXYZGlyph( picture_t *p_picture,
                                   int i_picture_x, int i_picture_y,
                                   int i_a, int i_x, int i_y, int i_z,
                                   FT_BitmapGlyph p_glyph,
                                   void (*BlendPixel)(picture_t *, int, int, int, int, int, int, int) )

{
    for( int dy = 0; dy < p_glyph->bitmap.rows; dy++ )
    {
        for( int dx = 0; dx < p_glyph->bitmap.width; dx++ )
            BlendPixel( p_picture, i_picture_x + dx, i_picture_y + dy,
                        i_a, i_x, i_y, i_z,
                        p_glyph->bitmap.buffer[dy * p_glyph->bitmap.width + dx] );
    }
}

static inline void BlendAXYZLine( picture_t *p_picture,
                                  int i_picture_x, int i_picture_y,
                                  int i_a, int i_x, int i_y, int i_z,
                                  const line_character_t *p_current,
                                  const line_character_t *p_next,
                                  void (*BlendPixel)(picture_t *, int, int, int, int, int, int, int) )
{
    int i_line_width = p_current->p_glyph->bitmap.width;
    if( p_next )
        i_line_width = p_next->p_glyph->left - p_current->p_glyph->left;

    for( int dx = 0; dx < i_line_width; dx++ )
    {
        for( int dy = 0; dy < p_current->i_line_thickness; dy++ )
            BlendPixel( p_picture,
                        i_picture_x + dx,
                        i_picture_y + p_current->i_line_offset + dy,
                        i_a, i_x, i_y, i_z, 0xff );
    }
}

static inline void RenderBackground( subpicture_region_t *p_region,
                                     line_desc_t *p_line_head,
                                     FT_BBox *p_bbox,
                                     int i_margin,
                                     picture_t *p_picture,
                                     int i_text_width,
                                     void (*ExtractComponents)( uint32_t, uint8_t *, uint8_t *, uint8_t * ),
                                     void (*BlendPixel)(picture_t *, int, int, int, int, int, int, int) )
{
    for( line_desc_t *p_line = p_line_head; p_line != NULL; p_line = p_line->p_next )
    {
        int i_align_left = i_margin;
        int i_align_top = i_margin;
        int line_start = 0;
        int line_end = 0;
        unsigned line_top = 0;
        int line_bottom = 0;
        int max_height = 0;

        if( p_line->i_width < i_text_width )
        {
            /* Left offset to take into account alignment */
            if( (p_region->i_align & 0x3) == SUBPICTURE_ALIGN_RIGHT )
                i_align_left += ( i_text_width - p_line->i_width );
            else if( (p_region->i_align & 0x10) == SUBPICTURE_ALIGN_LEAVETEXT)
                i_align_left = i_margin; /* Keep it the way it is */
            else if( (p_region->i_align & 0x3) != SUBPICTURE_ALIGN_LEFT )
                i_align_left += ( i_text_width - p_line->i_width ) / 2;
        }

        /* Find the tallest character in the line */
        for( int i = 0; i < p_line->i_character_count; i++ ) {
            const line_character_t *ch = &p_line->p_character[i];
            FT_BitmapGlyph p_glyph = ch->p_outline ? ch->p_outline : ch->p_glyph;
            if (p_glyph->top > max_height)
                max_height = p_glyph->top;
        }

        /* Compute the background for the line (identify leading/trailing space) */
        for( int i = 0; i < p_line->i_character_count; i++ ) {
            const line_character_t *ch = &p_line->p_character[i];
            FT_BitmapGlyph p_glyph = ch->p_outline ? ch->p_outline : ch->p_glyph;
            if (p_glyph && p_glyph->bitmap.rows > 0) {
                // Found a non-whitespace character
                line_start = i_align_left + p_glyph->left - p_bbox->xMin;
                break;
            }
        }

        /* Fudge factor to make sure caption background edges are left aligned
           despite variable font width */
        if (line_start < 12)
            line_start = 0;

        /* Find right boundary for bounding box for background */
        for( int i = p_line->i_character_count; i > 0; i-- ) {
            const line_character_t *ch = &p_line->p_character[i - 1];
            FT_BitmapGlyph p_glyph = ch->p_shadow ? ch->p_shadow : ch->p_glyph;
            if (p_glyph && p_glyph->bitmap.rows > 0) {
                // Found a non-whitespace character
                line_end = i_align_left + p_glyph->left - p_bbox->xMin + p_glyph->bitmap.width;
                break;
            }
        }

        /* Setup color for the background */
        uint8_t i_x, i_y, i_z;
        ExtractComponents( 0x000000, &i_x, &i_y, &i_z );

        /* Compute the upper boundary for the background */
        if ((i_align_top + p_line->i_base_line - max_height) < 0)
            line_top = i_align_top + p_line->i_base_line;
        else
            line_top = i_align_top + p_line->i_base_line - max_height;

        /* Compute lower boundary for the background */
        line_bottom =  __MIN(line_top + p_line->i_height, p_region->fmt.i_visible_height);

        /* Render the actual background */
        for( int dy = line_top; dy < line_bottom; dy++ )
        {
            for( int dx = line_start; dx < line_end; dx++ )
                BlendPixel( p_picture, dx, dy, 0xff, i_x, i_y, i_z, 0xff );
        }
    }
}

static inline int RenderAXYZ( filter_t *p_filter,
                              subpicture_region_t *p_region,
                              line_desc_t *p_line_head,
                              FT_BBox *p_bbox,
                              int i_margin,
                              vlc_fourcc_t i_chroma,
                              void (*ExtractComponents)( uint32_t, uint8_t *, uint8_t *, uint8_t * ),
                              void (*FillPicture)( picture_t *p_picture, int, int, int, int ),
                              void (*BlendPixel)(picture_t *, int, int, int, int, int, int, int) )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Create a new subpicture region */
    const int i_text_width  = p_bbox->xMax - p_bbox->xMin;
    const int i_text_height = p_bbox->yMax - p_bbox->yMin;
    video_format_t fmt;
    video_format_Init( &fmt, i_chroma );
    fmt.i_width          =
    fmt.i_visible_width  = i_text_width  + 2 * i_margin;
    fmt.i_height         =
    fmt.i_visible_height = i_text_height + 2 * i_margin;

    picture_t *p_picture = p_region->p_picture = picture_NewFromFormat( &fmt );
    if( !p_region->p_picture )
        return VLC_EGENERIC;
    p_region->fmt = fmt;

    /* Initialize the picture background */
    uint8_t i_a = var_InheritInteger( p_filter, "freetype-background-opacity" );
    i_a = VLC_CLIP( i_a, 0, 255 );
    uint8_t i_x, i_y, i_z;

    if (p_region->b_renderbg) {
        /* Render the background just under the text */
        FillPicture( p_picture, 0x00, 0x00, 0x00, 0x00 );
        RenderBackground(p_region, p_line_head, p_bbox, i_margin, p_picture, i_text_width,
                         ExtractComponents, BlendPixel);
    } else {
        /* Render background under entire subpicture block */
        int i_background_color = var_InheritInteger( p_filter, "freetype-background-color" );
        i_background_color = VLC_CLIP( i_background_color, 0, 0xFFFFFF );
        ExtractComponents( i_background_color, &i_x, &i_y, &i_z );
        FillPicture( p_picture, i_a, i_x, i_y, i_z );
    }

    /* Render shadow then outline and then normal glyphs */
    for( int g = 0; g < 3; g++ )
    {
        /* Render all lines */
        for( line_desc_t *p_line = p_line_head; p_line != NULL; p_line = p_line->p_next )
        {
            int i_align_left = i_margin;
            if( p_line->i_width < i_text_width )
            {
                /* Left offset to take into account alignment */
                if( (p_region->i_align & 0x3) == SUBPICTURE_ALIGN_RIGHT )
                    i_align_left += ( i_text_width - p_line->i_width );
                else if( (p_region->i_align & 0x10) == SUBPICTURE_ALIGN_LEAVETEXT)
                    i_align_left = i_margin; /* Keep it the way it is */
                else if( (p_region->i_align & 0x3) != SUBPICTURE_ALIGN_LEFT )
                    i_align_left += ( i_text_width - p_line->i_width ) / 2;
            }
            int i_align_top = i_margin;

            /* Render all glyphs and underline/strikethrough */
            for( int i = 0; i < p_line->i_character_count; i++ )
            {
                const line_character_t *ch = &p_line->p_character[i];
                FT_BitmapGlyph p_glyph = g == 0 ? ch->p_shadow : g == 1 ? ch->p_outline : ch->p_glyph;
                if( !p_glyph )
                    continue;

                i_a = (ch->i_color >> 24) & 0xff;
                uint32_t i_color;
                switch (g) {
                case 0:
                    i_a     = i_a * p_sys->style.i_shadow_alpha / 255;
                    i_color = p_sys->style.i_shadow_color;
                    break;
                case 1:
                    i_a     = i_a * p_sys->style.i_outline_alpha / 255;
                    i_color = p_sys->style.i_outline_color;
                    break;
                default:
                    i_color = ch->i_color;
                    break;
                }
                ExtractComponents( i_color, &i_x, &i_y, &i_z );

                int i_glyph_y = i_align_top  - p_glyph->top  + p_bbox->yMax + p_line->i_base_line;
                int i_glyph_x = i_align_left + p_glyph->left - p_bbox->xMin;

                BlendAXYZGlyph( p_picture,
                                i_glyph_x, i_glyph_y,
                                i_a, i_x, i_y, i_z,
                                p_glyph,
                                BlendPixel );

                /* underline/strikethrough are only rendered for the normal glyph */
                if( g == 2 && ch->i_line_thickness > 0 )
                    BlendAXYZLine( p_picture,
                                   i_glyph_x, i_glyph_y + p_glyph->top,
                                   i_a, i_x, i_y, i_z,
                                   &ch[0],
                                   i + 1 < p_line->i_character_count ? &ch[1] : NULL,
                                   BlendPixel );
            }
        }
    }

    return VLC_SUCCESS;
}



static void FreeLine( line_desc_t *p_line )
{
    for( int i = 0; i < p_line->i_character_count; i++ )
    {
        line_character_t *ch = &p_line->p_character[i];
        FT_Done_Glyph( (FT_Glyph)ch->p_glyph );
        if( ch->p_outline )
            FT_Done_Glyph( (FT_Glyph)ch->p_outline );
        if( ch->p_shadow )
            FT_Done_Glyph( (FT_Glyph)ch->p_shadow );
    }

    free( p_line->p_character );
    free( p_line );
}

static void FreeLines( line_desc_t *p_lines )
{
    for( line_desc_t *p_line = p_lines; p_line != NULL; )
    {
        line_desc_t *p_next = p_line->p_next;
        FreeLine( p_line );
        p_line = p_next;
    }
}

static line_desc_t *NewLine( int i_count )
{
    line_desc_t *p_line = malloc( sizeof(*p_line) );

    if( !p_line )
        return NULL;

    p_line->p_next = NULL;
    p_line->i_width = 0;
    p_line->i_base_line = 0;
    p_line->i_character_count = 0;

    p_line->p_character = calloc( i_count, sizeof(*p_line->p_character) );
    if( !p_line->p_character )
    {
        free( p_line );
        return NULL;
    }
    return p_line;
}

static FT_Face LoadEmbeddedFace( filter_sys_t *p_sys, const text_style_t *p_style )
{
    for( int k = 0; k < p_sys->i_font_attachments; k++ )
    {
        input_attachment_t *p_attach   = p_sys->pp_font_attachments[k];
        int                 i_font_idx = 0;
        FT_Face             p_face = NULL;

        while( 0 == FT_New_Memory_Face( p_sys->p_library,
                                        p_attach->p_data,
                                        p_attach->i_data,
                                        i_font_idx,
                                        &p_face ))
        {
            if( p_face )
            {
                int i_style_received = ((p_face->style_flags & FT_STYLE_FLAG_BOLD)    ? STYLE_BOLD   : 0) |
                                       ((p_face->style_flags & FT_STYLE_FLAG_ITALIC ) ? STYLE_ITALIC : 0);
                if( p_face->family_name != NULL
                 && !strcasecmp( p_face->family_name, p_style->psz_fontname )
                 && (p_style->i_style_flags & (STYLE_BOLD | STYLE_ITALIC))
                                                          == i_style_received )
                    return p_face;

                FT_Done_Face( p_face );
            }
            i_font_idx++;
        }
    }
    return NULL;
}

static FT_Face LoadFace( filter_t *p_filter,
                         const text_style_t *p_style )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Look for a match amongst our attachments first */
    FT_Face p_face = LoadEmbeddedFace( p_sys, p_style );

    /* Load system wide font otheriwse */
    if( !p_face )
    {
        int  i_idx = 0;
        char *psz_fontfile = NULL;
        if( p_sys->pf_select )
            psz_fontfile = p_sys->pf_select( p_filter,
                                             p_style->psz_fontname,
                                             (p_style->i_style_flags & STYLE_BOLD) != 0,
                                             (p_style->i_style_flags & STYLE_ITALIC) != 0,
                                             -1,
                                             &i_idx );
        else
            psz_fontfile = NULL;

        if( !psz_fontfile )
            return NULL;

        if( *psz_fontfile == '\0' )
        {
            msg_Warn( p_filter,
                      "We were not able to find a matching font: \"%s\" (%s %s),"
                      " so using default font",
                      p_style->psz_fontname,
                      (p_style->i_style_flags & STYLE_BOLD)   ? "Bold" : "",
                      (p_style->i_style_flags & STYLE_ITALIC) ? "Italic" : "" );
            p_face = NULL;
        }
        else
        {
            if( FT_New_Face( p_sys->p_library, psz_fontfile, i_idx, &p_face ) )
                p_face = NULL;
        }
        free( psz_fontfile );
    }
    if( !p_face )
        return NULL;

    if( FT_Select_Charmap( p_face, ft_encoding_unicode ) )
    {
        /* We've loaded a font face which is unhelpful for actually
         * rendering text - fallback to the default one.
         */
        FT_Done_Face( p_face );
        return NULL;
    }
    return p_face;
}

static int GetGlyph( filter_t *p_filter,
                     FT_Glyph *pp_glyph,   FT_BBox *p_glyph_bbox,
                     FT_Glyph *pp_outline, FT_BBox *p_outline_bbox,
                     FT_Glyph *pp_shadow,  FT_BBox *p_shadow_bbox,

                     FT_Face  p_face,
                     int i_glyph_index,
                     int i_style_flags,
                     FT_Vector *p_pen,
                     FT_Vector *p_pen_shadow )
{
    if( FT_Load_Glyph( p_face, i_glyph_index, FT_LOAD_NO_BITMAP | FT_LOAD_DEFAULT ) &&
        FT_Load_Glyph( p_face, i_glyph_index, FT_LOAD_DEFAULT ) )
    {
        msg_Err( p_filter, "unable to render text FT_Load_Glyph failed" );
        return VLC_EGENERIC;
    }

    /* Do synthetic styling now that Freetype supports it;
     * ie. if the font we have loaded is NOT already in the
     * style that the tags want, then switch it on; if they
     * are then don't. */
    if ((i_style_flags & STYLE_BOLD) && !(p_face->style_flags & FT_STYLE_FLAG_BOLD))
        FT_GlyphSlot_Embolden( p_face->glyph );
    if ((i_style_flags & STYLE_ITALIC) && !(p_face->style_flags & FT_STYLE_FLAG_ITALIC))
        FT_GlyphSlot_Oblique( p_face->glyph );

    FT_Glyph glyph;
    if( FT_Get_Glyph( p_face->glyph, &glyph ) )
    {
        msg_Err( p_filter, "unable to render text FT_Get_Glyph failed" );
        return VLC_EGENERIC;
    }

    FT_Glyph outline = NULL;
    if( p_filter->p_sys->p_stroker )
    {
        outline = glyph;
        if( FT_Glyph_StrokeBorder( &outline, p_filter->p_sys->p_stroker, 0, 0 ) )
            outline = NULL;
    }

    FT_Glyph shadow = NULL;
    if( p_filter->p_sys->style.i_shadow_alpha > 0 )
    {
        shadow = outline ? outline : glyph;
        if( FT_Glyph_To_Bitmap( &shadow, FT_RENDER_MODE_NORMAL, p_pen_shadow, 0  ) )
        {
            shadow = NULL;
        }
        else
        {
            FT_Glyph_Get_CBox( shadow, ft_glyph_bbox_pixels, p_shadow_bbox );
        }
    }
    *pp_shadow = shadow;

    if( FT_Glyph_To_Bitmap( &glyph, FT_RENDER_MODE_NORMAL, p_pen, 1) )
    {
        FT_Done_Glyph( glyph );
        if( outline )
            FT_Done_Glyph( outline );
        if( shadow )
            FT_Done_Glyph( shadow );
        return VLC_EGENERIC;
    }
    FT_Glyph_Get_CBox( glyph, ft_glyph_bbox_pixels, p_glyph_bbox );
    *pp_glyph = glyph;

    if( outline )
    {
        FT_Glyph_To_Bitmap( &outline, FT_RENDER_MODE_NORMAL, p_pen, 1 );
        FT_Glyph_Get_CBox( outline, ft_glyph_bbox_pixels, p_outline_bbox );
    }
    *pp_outline = outline;

    return VLC_SUCCESS;
}

static void FixGlyph( FT_Glyph glyph, FT_BBox *p_bbox, FT_Face face, const FT_Vector *p_pen )
{
    FT_BitmapGlyph glyph_bmp = (FT_BitmapGlyph)glyph;
    if( p_bbox->xMin >= p_bbox->xMax )
    {
        p_bbox->xMin = FT_CEIL(p_pen->x);
        p_bbox->xMax = FT_CEIL(p_pen->x + face->glyph->advance.x);
        glyph_bmp->left = p_bbox->xMin;
    }
    if( p_bbox->yMin >= p_bbox->yMax )
    {
        p_bbox->yMax = FT_CEIL(p_pen->y);
        p_bbox->yMin = FT_CEIL(p_pen->y + face->glyph->advance.y);
        glyph_bmp->top  = p_bbox->yMax;
    }
}

static void BBoxEnlarge( FT_BBox *p_max, const FT_BBox *p )
{
    p_max->xMin = __MIN(p_max->xMin, p->xMin);
    p_max->yMin = __MIN(p_max->yMin, p->yMin);
    p_max->xMax = __MAX(p_max->xMax, p->xMax);
    p_max->yMax = __MAX(p_max->yMax, p->yMax);
}

static int ProcessLines( filter_t *p_filter,
                         line_desc_t **pp_lines,
                         FT_BBox     *p_bbox,
                         int         *pi_max_face_height,

                         uni_char_t *psz_text,
                         text_style_t **pp_styles,
                         uint32_t *pi_k_dates,
                         int i_len )
{
    filter_sys_t   *p_sys = p_filter->p_sys;
    uni_char_t     *p_fribidi_string = NULL;
    text_style_t   **pp_fribidi_styles = NULL;
    int            *p_new_positions = NULL;

#if defined(HAVE_FRIBIDI)
    {
        int    *p_old_positions;
        int start_pos, pos = 0;

        pp_fribidi_styles = calloc( i_len, sizeof(*pp_fribidi_styles) );

        p_fribidi_string  = malloc( (i_len + 1) * sizeof(*p_fribidi_string) );
        p_old_positions   = malloc( (i_len + 1) * sizeof(*p_old_positions) );
        p_new_positions   = malloc( (i_len + 1) * sizeof(*p_new_positions) );

        if( ! pp_fribidi_styles ||
            ! p_fribidi_string ||
            ! p_old_positions ||
            ! p_new_positions )
        {
            free( p_old_positions );
            free( p_new_positions );
            free( p_fribidi_string );
            free( pp_fribidi_styles );
            return VLC_ENOMEM;
        }

        /* Do bidi conversion line-by-line */
        while(pos < i_len)
        {
            while(pos < i_len) {
                if (psz_text[pos] != '\n')
                    break;
                p_fribidi_string[pos] = psz_text[pos];
                pp_fribidi_styles[pos] = pp_styles[pos];
                p_new_positions[pos] = pos;
                ++pos;
            }
            start_pos = pos;
            while(pos < i_len) {
                if (psz_text[pos] == '\n')
                    break;
                ++pos;
            }
            if (pos > start_pos)
            {
#if (FRIBIDI_MINOR_VERSION < 19) && (FRIBIDI_MAJOR_VERSION == 0)
                FriBidiCharType base_dir = FRIBIDI_TYPE_LTR;
#else
                FriBidiParType base_dir = FRIBIDI_PAR_LTR;
#endif
                fribidi_log2vis((FriBidiChar*)psz_text + start_pos,
                        pos - start_pos, &base_dir,
                        (FriBidiChar*)p_fribidi_string + start_pos,
                        p_new_positions + start_pos,
                        p_old_positions,
                        NULL );
                for( int j = start_pos; j < pos; j++ )
                {
                    pp_fribidi_styles[ j ] = pp_styles[ start_pos + p_old_positions[j - start_pos] ];
                    p_new_positions[ j ] += start_pos;
                }
            }
        }
        p_fribidi_string[ i_len ] = 0;
        free( p_old_positions );

        pp_styles = pp_fribidi_styles;
        psz_text = p_fribidi_string;
    }
#endif
    /* Work out the karaoke */
    uint8_t *pi_karaoke_bar = NULL;
    if( pi_k_dates )
    {
        pi_karaoke_bar = malloc( i_len * sizeof(*pi_karaoke_bar));
        if( pi_karaoke_bar )
        {
            int64_t i_elapsed  = var_GetTime( p_filter, "spu-elapsed" ) / 1000;
            for( int i = 0; i < i_len; i++ )
            {
                unsigned i_bar = p_new_positions ? p_new_positions[i] : i;
                pi_karaoke_bar[i_bar] = pi_k_dates[i] >= i_elapsed;
            }
        }
    }
    free( p_new_positions );

    *pi_max_face_height = 0;
    *pp_lines = NULL;
    line_desc_t **pp_line_next = pp_lines;

    FT_BBox bbox = {
        .xMin = INT_MAX,
        .yMin = INT_MAX,
        .xMax = INT_MIN,
        .yMax = INT_MIN,
    };
    int i_face_height_previous = 0;
    int i_base_line = 0;
    const text_style_t *p_previous_style = NULL;
    FT_Face p_face = NULL;
    for( int i_start = 0; i_start < i_len; )
    {
        /* Compute the length of the current text line */
        int i_length = 0;
        while( i_start + i_length < i_len && psz_text[i_start + i_length] != '\n' )
            i_length++;

        /* Render the text line (or the begining if too long) into 0 or 1 glyph line */
        line_desc_t *p_line = i_length > 0 ? NewLine( i_length ) : NULL;
        int i_index = i_start;
        FT_Vector pen = {
            .x = 0,
            .y = 0,
        };
        int i_face_height = 0;
        FT_BBox line_bbox = {
            .xMin = INT_MAX,
            .yMin = INT_MAX,
            .xMax = INT_MIN,
            .yMax = INT_MIN,
        };
        int i_ul_offset = 0;
        int i_ul_thickness = 0;
        typedef struct {
            int       i_index;
            FT_Vector pen;
            FT_BBox   line_bbox;
            int i_face_height;
            int i_ul_offset;
            int i_ul_thickness;
        } break_point_t;
        break_point_t break_point;
        break_point_t break_point_fallback;

#define SAVE_BP(dst) do { \
        dst.i_index = i_index; \
        dst.pen = pen; \
        dst.line_bbox = line_bbox; \
        dst.i_face_height = i_face_height; \
        dst.i_ul_offset = i_ul_offset; \
        dst.i_ul_thickness = i_ul_thickness; \
    } while(0)

        SAVE_BP( break_point );
        SAVE_BP( break_point_fallback );

        while( i_index < i_start + i_length )
        {
            /* Split by common FT_Face + Size */
            const text_style_t *p_current_style = pp_styles[i_index];
            int i_part_length = 0;
            while( i_index + i_part_length < i_start + i_length )
            {
                const text_style_t *p_style = pp_styles[i_index + i_part_length];
                if( !FaceStyleEquals( p_style, p_current_style ) ||
                    p_style->i_font_size != p_current_style->i_font_size )
                    break;
                i_part_length++;
            }

            /* (Re)load/reconfigure the face if needed */
            if( !FaceStyleEquals( p_current_style, p_previous_style ) )
            {
                if( p_face )
                    FT_Done_Face( p_face );
                p_previous_style = NULL;

                p_face = LoadFace( p_filter, p_current_style );
            }
            FT_Face p_current_face = p_face ? p_face : p_sys->p_face;
            if( !p_previous_style || p_previous_style->i_font_size != p_current_style->i_font_size )
            {
                if( FT_Set_Pixel_Sizes( p_current_face, 0, p_current_style->i_font_size ) )
                    msg_Err( p_filter, "Failed to set font size to %d", p_current_style->i_font_size );
                if( p_sys->p_stroker )
                {
                    double f_outline_thickness = var_InheritInteger( p_filter, "freetype-outline-thickness" ) / 100.0;
                    f_outline_thickness = VLC_CLIP( f_outline_thickness, 0.0, 0.5 );
                    int i_radius = (p_current_style->i_font_size << 6) * f_outline_thickness;
                    FT_Stroker_Set( p_sys->p_stroker,
                                    i_radius,
                                    FT_STROKER_LINECAP_ROUND,
                                    FT_STROKER_LINEJOIN_ROUND, 0 );
                }
            }
            p_previous_style = p_current_style;

            i_face_height = __MAX(i_face_height, FT_CEIL(FT_MulFix(p_current_face->height,
                                                                   p_current_face->size->metrics.y_scale)));

            /* Render the part */
            bool b_break_line = false;
            int i_glyph_last = 0;
            while( i_part_length > 0 )
            {
                const text_style_t *p_glyph_style = pp_styles[i_index];
                uni_char_t character = psz_text[i_index];
                int i_glyph_index = FT_Get_Char_Index( p_current_face, character );

                /* Get kerning vector */
                FT_Vector kerning = { .x = 0, .y = 0 };
                if( FT_HAS_KERNING( p_current_face ) && i_glyph_last != 0 && i_glyph_index != 0 )
                    FT_Get_Kerning( p_current_face, i_glyph_last, i_glyph_index, ft_kerning_default, &kerning );

                /* Get the glyph bitmap and its bounding box and all the associated properties */
                FT_Vector pen_new = {
                    .x = pen.x + kerning.x,
                    .y = pen.y + kerning.y,
                };
                FT_Vector pen_shadow_new = {
                    .x = pen_new.x + p_sys->f_shadow_vector_x * (p_current_style->i_font_size << 6),
                    .y = pen_new.y + p_sys->f_shadow_vector_y * (p_current_style->i_font_size << 6),
                };
                FT_Glyph glyph;
                FT_BBox  glyph_bbox;
                FT_Glyph outline;
                FT_BBox  outline_bbox;
                FT_Glyph shadow;
                FT_BBox  shadow_bbox;

                if( GetGlyph( p_filter,
                              &glyph, &glyph_bbox,
                              &outline, &outline_bbox,
                              &shadow, &shadow_bbox,
                              p_current_face, i_glyph_index, p_glyph_style->i_style_flags,
                              &pen_new, &pen_shadow_new ) )
                    goto next;

                FixGlyph( glyph, &glyph_bbox, p_current_face, &pen_new );
                if( outline )
                    FixGlyph( outline, &outline_bbox, p_current_face, &pen_new );
                if( shadow )
                    FixGlyph( shadow, &shadow_bbox, p_current_face, &pen_shadow_new );

                /* FIXME and what about outline */

                bool     b_karaoke = pi_karaoke_bar && pi_karaoke_bar[i_index] != 0;
                uint32_t i_color = b_karaoke ? (p_glyph_style->i_karaoke_background_color |
                                                (p_glyph_style->i_karaoke_background_alpha << 24))
                                             : (p_glyph_style->i_font_color |
                                                (p_glyph_style->i_font_alpha << 24));
                int i_line_offset    = 0;
                int i_line_thickness = 0;
                if( p_glyph_style->i_style_flags & (STYLE_UNDERLINE | STYLE_STRIKEOUT) )
                {
                    i_line_offset = abs( FT_FLOOR(FT_MulFix(p_current_face->underline_position,
                                                            p_current_face->size->metrics.y_scale)) );

                    i_line_thickness = abs( FT_CEIL(FT_MulFix(p_current_face->underline_thickness,
                                                              p_current_face->size->metrics.y_scale)) );

                    if( p_glyph_style->i_style_flags & STYLE_STRIKEOUT )
                    {
                        /* Move the baseline to make it strikethrough instead of
                         * underline. That means that strikethrough takes precedence
                         */
                        i_line_offset -= abs( FT_FLOOR(FT_MulFix(p_current_face->descender*2,
                                                                 p_current_face->size->metrics.y_scale)) );
                    }
                    else if( i_line_thickness > 0 )
                    {
                        glyph_bbox.yMin = __MIN( glyph_bbox.yMin, - i_line_offset - i_line_thickness );

                        /* The real underline thickness and position are
                         * updated once the whole line has been parsed */
                        i_ul_offset = __MAX( i_ul_offset, i_line_offset );
                        i_ul_thickness = __MAX( i_ul_thickness, i_line_thickness );
                        i_line_thickness = -1;
                    }
                }
                FT_BBox line_bbox_new = line_bbox;
                BBoxEnlarge( &line_bbox_new, &glyph_bbox );
                if( outline )
                    BBoxEnlarge( &line_bbox_new, &outline_bbox );
                if( shadow )
                    BBoxEnlarge( &line_bbox_new, &shadow_bbox );

                b_break_line = i_index > i_start &&
                               line_bbox_new.xMax - line_bbox_new.xMin >= (int)p_filter->fmt_out.video.i_visible_width;
                if( b_break_line )
                {
                    FT_Done_Glyph( glyph );
                    if( outline )
                        FT_Done_Glyph( outline );
                    if( shadow )
                        FT_Done_Glyph( shadow );

                    break_point_t *p_bp = NULL;
                    if( break_point.i_index > i_start )
                        p_bp = &break_point;
                    else if( break_point_fallback.i_index > i_start )
                        p_bp = &break_point_fallback;

                    if( p_bp )
                    {
                        msg_Dbg( p_filter, "Breaking line");
                        for( int i = p_bp->i_index; i < i_index; i++ )
                        {
                            line_character_t *ch = &p_line->p_character[i - i_start];
                            FT_Done_Glyph( (FT_Glyph)ch->p_glyph );
                            if( ch->p_outline )
                                FT_Done_Glyph( (FT_Glyph)ch->p_outline );
                            if( ch->p_shadow )
                                FT_Done_Glyph( (FT_Glyph)ch->p_shadow );
                        }
                        p_line->i_character_count = p_bp->i_index - i_start;

                        i_index = p_bp->i_index;
                        pen = p_bp->pen;
                        line_bbox = p_bp->line_bbox;
                        i_face_height = p_bp->i_face_height;
                        i_ul_offset = p_bp->i_ul_offset;
                        i_ul_thickness = p_bp->i_ul_thickness;
                    }
                    else
                    {
                        msg_Err( p_filter, "Breaking unbreakable line");
                    }
                    break;
                }

                assert( p_line->i_character_count == i_index - i_start);
                p_line->p_character[p_line->i_character_count++] = (line_character_t){
                    .p_glyph = (FT_BitmapGlyph)glyph,
                    .p_outline = (FT_BitmapGlyph)outline,
                    .p_shadow = (FT_BitmapGlyph)shadow,
                    .i_color = i_color,
                    .i_line_offset = i_line_offset,
                    .i_line_thickness = i_line_thickness,
                };

                pen.x = pen_new.x + p_current_face->glyph->advance.x;
                pen.y = pen_new.y + p_current_face->glyph->advance.y;
                line_bbox = line_bbox_new;
            next:
                i_glyph_last = i_glyph_index;
                i_part_length--;
                i_index++;

                if( character == ' ' || character == '\t' )
                    SAVE_BP( break_point );
                else if( character == 160 )
                    SAVE_BP( break_point_fallback );
            }
            if( b_break_line )
                break;
        }
#undef SAVE_BP
        /* Update our baseline */
        if( i_face_height_previous > 0 )
            i_base_line += __MAX(i_face_height, i_face_height_previous);
        if( i_face_height > 0 )
            i_face_height_previous = i_face_height;

        /* Update the line bbox with the actual base line */
        if (line_bbox.yMax > line_bbox.yMin) {
            line_bbox.yMin -= i_base_line;
            line_bbox.yMax -= i_base_line;
        }
        BBoxEnlarge( &bbox, &line_bbox );

        /* Terminate and append the line */
        if( p_line )
        {
            p_line->i_width  = __MAX(line_bbox.xMax - line_bbox.xMin, 0);
            p_line->i_base_line = i_base_line;
            p_line->i_height = __MAX(i_face_height, i_face_height_previous);
            if( i_ul_thickness > 0 )
            {
                for( int i = 0; i < p_line->i_character_count; i++ )
                {
                    line_character_t *ch = &p_line->p_character[i];
                    if( ch->i_line_thickness < 0 )
                    {
                        ch->i_line_offset    = i_ul_offset;
                        ch->i_line_thickness = i_ul_thickness;
                    }
                }
            }

            *pp_line_next = p_line;
            pp_line_next = &p_line->p_next;
        }

        *pi_max_face_height = __MAX( *pi_max_face_height, i_face_height );

        /* Skip what we have rendered and the line delimitor if present */
        i_start = i_index;
        if( i_start < i_len && psz_text[i_start] == '\n' )
            i_start++;

        if( bbox.yMax - bbox.yMin >= (int)p_filter->fmt_out.video.i_visible_height )
        {
            msg_Err( p_filter, "Truncated too high subtitle" );
            break;
        }
    }
    if( p_face )
        FT_Done_Face( p_face );

    free( pp_fribidi_styles );
    free( p_fribidi_string );
    free( pi_karaoke_bar );

    *p_bbox = bbox;
    return VLC_SUCCESS;
}

static xml_reader_t *GetXMLReader( filter_t *p_filter, stream_t *p_sub )
{
    xml_reader_t *p_xml_reader = p_filter->p_sys->p_xml;
    if( !p_xml_reader )
        p_xml_reader = xml_ReaderCreate( p_filter, p_sub );
    else
        p_xml_reader = xml_ReaderReset( p_xml_reader, p_sub );
    p_filter->p_sys->p_xml = p_xml_reader;

    return p_xml_reader;
}

/**
 * This function renders a text subpicture region into another one.
 * It also calculates the size needed for this string, and renders the
 * needed glyphs into memory. It is used as pf_add_string callback in
 * the vout method by this module
 */
static int RenderCommon( filter_t *p_filter, subpicture_region_t *p_region_out,
                         subpicture_region_t *p_region_in, bool b_html,
                         const vlc_fourcc_t *p_chroma_list )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_region_in )
        return VLC_EGENERIC;
    if( b_html && !p_region_in->psz_html )
        return VLC_EGENERIC;
    if( !b_html && !p_region_in->psz_text )
        return VLC_EGENERIC;

    const size_t i_text_max = strlen( b_html ? p_region_in->psz_html
                                             : p_region_in->psz_text );

    uni_char_t *psz_text = calloc( i_text_max, sizeof( *psz_text ) );
    text_style_t **pp_styles = calloc( i_text_max, sizeof( *pp_styles ) );
    if( !psz_text || !pp_styles )
    {
        free( psz_text );
        free( pp_styles );
        return VLC_EGENERIC;
    }

    /* Reset the default fontsize in case screen metrics have changed */
    p_filter->p_sys->style.i_font_size = GetFontSize( p_filter );

    /* */
    int rv = VLC_SUCCESS;
    int i_text_length = 0;
    FT_BBox bbox;
    int i_max_face_height;
    line_desc_t *p_lines = NULL;

    uint32_t *pi_k_durations   = NULL;

    if( b_html )
    {
        stream_t *p_sub = stream_MemoryNew( VLC_OBJECT(p_filter),
                                            (uint8_t *) p_region_in->psz_html,
                                            strlen( p_region_in->psz_html ),
                                            true );
        if( unlikely(p_sub == NULL) )
        {
            free( psz_text );
            free( pp_styles );
            return VLC_SUCCESS;
        }

        xml_reader_t *p_xml_reader = GetXMLReader( p_filter, p_sub );

        if( !p_xml_reader )
            rv = VLC_EGENERIC;

        if( !rv )
        {
            /* Look for Root Node */
            const char *node;

            if( xml_ReaderNextNode( p_xml_reader, &node ) == XML_READER_STARTELEM )
            {
                if( strcasecmp( "karaoke", node ) == 0 )
                {
                    pi_k_durations = calloc( i_text_max, sizeof( *pi_k_durations ) );
                }
                else if( strcasecmp( "text", node ) != 0 )
                {
                    /* Only text and karaoke tags are supported */
                    msg_Dbg( p_filter, "Unsupported top-level tag <%s> ignored.",
                             node );
                    rv = VLC_EGENERIC;
                }
            }
            else
            {
                msg_Err( p_filter, "Malformed HTML subtitle" );
                rv = VLC_EGENERIC;
            }
        }
        if( !rv )
        {
            rv = ProcessNodes( p_filter,
                               psz_text, pp_styles, pi_k_durations, &i_text_length,
                               p_xml_reader, p_region_in->p_style, &p_filter->p_sys->style );
        }

        if( p_xml_reader )
            p_filter->p_sys->p_xml = xml_ReaderReset( p_xml_reader, NULL );

        stream_Delete( p_sub );
    }
    else
    {
        text_style_t *p_style;
        if( p_region_in->p_style )
            p_style = CreateStyle( p_region_in->p_style->psz_fontname ? p_region_in->p_style->psz_fontname
                                                                      : p_sys->style.psz_fontname,
                                   p_region_in->p_style->i_font_size > 0 ? p_region_in->p_style->i_font_size
                                                                         : p_sys->style.i_font_size,
                                   (p_region_in->p_style->i_font_color & 0xffffff) |
                                   ((p_region_in->p_style->i_font_alpha & 0xff) << 24),
                                   0x00ffffff,
                                   p_region_in->p_style->i_style_flags & (STYLE_BOLD |
                                                                          STYLE_ITALIC |
                                                                          STYLE_UNDERLINE |
                                                                          STYLE_STRIKEOUT) );
        else
        {
            uint32_t i_font_color = var_InheritInteger( p_filter, "freetype-color" );
            i_font_color = VLC_CLIP( i_font_color, 0, 0xFFFFFF );
            p_style = CreateStyle( p_sys->style.psz_fontname,
                                   p_sys->style.i_font_size,
                                   (i_font_color & 0xffffff) |
                                   ((p_sys->style.i_font_alpha & 0xff) << 24),
                                   0x00ffffff, 0);
        }
        if( p_sys->style.i_style_flags & STYLE_BOLD )
            p_style->i_style_flags |= STYLE_BOLD;

        i_text_length = SetupText( p_filter,
                                   psz_text,
                                   pp_styles,
                                   NULL,
                                   p_region_in->psz_text, p_style, 0 );
    }

    if( !rv && i_text_length > 0 )
    {
        rv = ProcessLines( p_filter,
                           &p_lines, &bbox, &i_max_face_height,
                           psz_text, pp_styles, pi_k_durations, i_text_length );
    }

    p_region_out->i_x = p_region_in->i_x;
    p_region_out->i_y = p_region_in->i_y;

    /* Don't attempt to render text that couldn't be layed out
     * properly. */
    if( !rv && i_text_length > 0 && bbox.xMin < bbox.xMax && bbox.yMin < bbox.yMax )
    {
        const vlc_fourcc_t p_chroma_list_yuvp[] = { VLC_CODEC_YUVP, 0 };
        const vlc_fourcc_t p_chroma_list_rgba[] = { VLC_CODEC_RGBA, 0 };

        if( var_InheritBool( p_filter, "freetype-yuvp" ) )
            p_chroma_list = p_chroma_list_yuvp;
        else if( !p_chroma_list || *p_chroma_list == 0 )
            p_chroma_list = p_chroma_list_rgba;

        uint8_t i_background_opacity = var_InheritInteger( p_filter, "freetype-background-opacity" );
        i_background_opacity = VLC_CLIP( i_background_opacity, 0, 255 );
        const int i_margin = i_background_opacity > 0 ? i_max_face_height / 4 : 0;
        for( const vlc_fourcc_t *p_chroma = p_chroma_list; *p_chroma != 0; p_chroma++ )
        {
            rv = VLC_EGENERIC;
            if( *p_chroma == VLC_CODEC_YUVP )
                rv = RenderYUVP( p_filter, p_region_out, p_lines, &bbox );
            else if( *p_chroma == VLC_CODEC_YUVA )
                rv = RenderAXYZ( p_filter, p_region_out, p_lines, &bbox, i_margin,
                                 VLC_CODEC_YUVA,
                                 YUVFromRGB,
                                 FillYUVAPicture,
                                 BlendYUVAPixel );
            else if( *p_chroma == VLC_CODEC_RGBA )
                rv = RenderAXYZ( p_filter, p_region_out, p_lines, &bbox, i_margin,
                                 VLC_CODEC_RGBA,
                                 RGBFromRGB,
                                 FillRGBAPicture,
                                 BlendRGBAPixel );
            else if( *p_chroma == VLC_CODEC_ARGB )
                rv = RenderAXYZ( p_filter, p_region_out, p_lines, &bbox,
                                 i_margin, *p_chroma, RGBFromRGB,
                                 FillARGBPicture, BlendARGBPixel );

            if( !rv )
                break;
        }

        /* With karaoke, we're going to have to render the text a number
         * of times to show the progress marker on the text.
         */
        if( pi_k_durations )
            var_SetBool( p_filter, "text-rerender", true );
    }

    FreeLines( p_lines );

    free( psz_text );
    for( int i = 0; i < i_text_length; i++ )
    {
        if( pp_styles[i] && ( i + 1 == i_text_length || pp_styles[i] != pp_styles[i + 1] ) )
            text_style_Delete( pp_styles[i] );
    }
    free( pp_styles );
    free( pi_k_durations );

    return rv;
}

static int RenderText( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in,
                       const vlc_fourcc_t *p_chroma_list )
{
    return RenderCommon( p_filter, p_region_out, p_region_in, false, p_chroma_list );
}

static int RenderHtml( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in,
                       const vlc_fourcc_t *p_chroma_list )
{
    return RenderCommon( p_filter, p_region_out, p_region_in, true, p_chroma_list );
}

/*****************************************************************************
 * Create: allocates osd-text video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
 *****************************************************************************/
static int Init_FT( vlc_object_t *p_this,
                    const char *psz_fontfile,
                    const int fontindex,
                    const float f_outline_thickness)
{
    filter_t      *p_filter = (filter_t *)p_this;
    filter_sys_t  *p_sys = p_filter->p_sys;

    /* */
    int i_error = FT_Init_FreeType( &p_sys->p_library );
    if( i_error )
    {
        msg_Err( p_filter, "couldn't initialize freetype" );
        goto error;
    }

    i_error = FT_New_Face( p_sys->p_library, psz_fontfile ? psz_fontfile : "",
                           fontindex, &p_sys->p_face );

    if( i_error == FT_Err_Unknown_File_Format )
    {
        msg_Err( p_filter, "file %s have unknown format",
                 psz_fontfile ? psz_fontfile : "(null)" );
        goto error;
    }
    else if( i_error )
    {
        msg_Err( p_filter, "failed to load font file %s",
                 psz_fontfile ? psz_fontfile : "(null)" );
        goto error;
    }

    i_error = FT_Select_Charmap( p_sys->p_face, ft_encoding_unicode );
    if( i_error )
    {
        msg_Err( p_filter, "font has no unicode translation table" );
        goto error;
    }

    if( SetFontSize( p_filter, 0 ) != VLC_SUCCESS ) goto error;

    p_sys->p_stroker = NULL;
    if( f_outline_thickness > 0.001 )
    {
        i_error = FT_Stroker_New( p_sys->p_library, &p_sys->p_stroker );
        if( i_error )
            msg_Err( p_filter, "Failed to create stroker for outlining" );
    }

    return VLC_SUCCESS;

error:
    if( p_sys->p_face ) FT_Done_Face( p_sys->p_face );
    if( p_sys->p_library ) FT_Done_FreeType( p_sys->p_library );

    return VLC_EGENERIC;
}


static int Create( vlc_object_t *p_this )
{
    filter_t      *p_filter = (filter_t *)p_this;
    filter_sys_t  *p_sys;
    char          *psz_fontfile   = NULL;
    char          *psz_fontname = NULL;
    char          *psz_monofontfile   = NULL;
    char          *psz_monofontfamily = NULL;
    int            fontindex = 0, monofontindex = 0;

    /* Allocate structure */
    p_filter->p_sys = p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->style.psz_fontname   = NULL;
    p_sys->p_xml            = NULL;
    p_sys->p_face           = 0;
    p_sys->p_library        = 0;
    p_sys->style.i_font_size      = 0;
    p_sys->style.i_style_flags = 0;

    /*
     * The following variables should not be cached, as they might be changed on-the-fly:
     * freetype-rel-fontsize, freetype-background-opacity, freetype-background-color,
     * freetype-outline-thickness, freetype-color
     *
     */

    psz_fontname = var_InheritString( p_filter, "freetype-font" );
    psz_monofontfamily = var_InheritString( p_filter, "freetype-monofont" );
    p_sys->i_default_font_size = var_InheritInteger( p_filter, "freetype-fontsize" );
    p_sys->style.i_font_alpha = var_InheritInteger( p_filter,"freetype-opacity" );
    p_sys->style.i_font_alpha = VLC_CLIP( p_sys->style.i_font_alpha, 0, 255 );
    if( var_InheritBool( p_filter, "freetype-bold" ) )
        p_sys->style.i_style_flags |= STYLE_BOLD;

    double f_outline_thickness = var_InheritInteger( p_filter, "freetype-outline-thickness" ) / 100.0;
    f_outline_thickness = VLC_CLIP( f_outline_thickness, 0.0, 0.5 );
    p_sys->style.i_outline_alpha = var_InheritInteger( p_filter, "freetype-outline-opacity" );
    p_sys->style.i_outline_alpha = VLC_CLIP( p_sys->style.i_outline_alpha, 0, 255 );
    p_sys->style.i_outline_color = var_InheritInteger( p_filter, "freetype-outline-color" );
    p_sys->style.i_outline_color = VLC_CLIP( p_sys->style.i_outline_color, 0, 0xFFFFFF );

    p_sys->style.i_shadow_alpha = var_InheritInteger( p_filter, "freetype-shadow-opacity" );
    p_sys->style.i_shadow_alpha = VLC_CLIP( p_sys->style.i_shadow_alpha, 0, 255 );
    p_sys->style.i_shadow_color = var_InheritInteger( p_filter, "freetype-shadow-color" );
    p_sys->style.i_shadow_color = VLC_CLIP( p_sys->style.i_shadow_color, 0, 0xFFFFFF );
    float f_shadow_angle = var_InheritFloat( p_filter, "freetype-shadow-angle" );
    float f_shadow_distance = var_InheritFloat( p_filter, "freetype-shadow-distance" );
    f_shadow_distance = VLC_CLIP( f_shadow_distance, 0, 1 );
    p_sys->f_shadow_vector_x = f_shadow_distance * cos(2 * M_PI * f_shadow_angle / 360);
    p_sys->f_shadow_vector_y = f_shadow_distance * sin(2 * M_PI * f_shadow_angle / 360);

    /* Set default psz_fontname */
    if( !psz_fontname || !*psz_fontname )
    {
        free( psz_fontname );
#ifdef HAVE_GET_FONT_BY_FAMILY_NAME
        psz_fontname = strdup( DEFAULT_FAMILY );
#else
        psz_fontname = File_Select( DEFAULT_FONT_FILE );
#endif
    }

    /* set default psz_monofontname */
    if( !psz_monofontfamily || !*psz_monofontfamily )
    {
        free( psz_monofontfamily );
#ifdef HAVE_GET_FONT_BY_FAMILY_NAME
        psz_monofontfamily = strdup( DEFAULT_MONOSPACE_FAMILY );
#else
        psz_monofontfamily = File_Select( DEFAULT_MONOSPACE_FONT_FILE );
#endif
    }

    /* Set the current font file */
    p_sys->style.psz_fontname = psz_fontname;
    p_sys->style.psz_monofontname = psz_monofontfamily;

#ifdef HAVE_FONTCONFIG
    p_sys->pf_select = FontConfig_Select;
    FontConfig_BuildCache( p_filter );
#elif defined( __APPLE__ )
#if !TARGET_OS_IPHONE
    p_sys->pf_select = MacLegacy_Select;
#endif
#elif defined( _WIN32 ) && defined( HAVE_GET_FONT_BY_FAMILY_NAME )
    p_sys->pf_select = Win32_Select;
#else
    p_sys->pf_select = Dummy_Select;
#endif

    /* */
    psz_fontfile = p_sys->pf_select( p_filter, psz_fontname, false, false,
                                      p_sys->i_default_font_size, &fontindex );
    psz_monofontfile = p_sys->pf_select( p_filter, psz_monofontfamily, false,
                                          false, p_sys->i_default_font_size,
                                          &monofontindex );
    msg_Dbg( p_filter, "Using %s as font from file %s", psz_fontname, psz_fontfile );
    msg_Dbg( p_filter, "Using %s as mono-font from file %s", psz_monofontfamily, psz_monofontfile );

    /* If nothing is found, use the default family */
    if( !psz_fontfile )
        psz_fontfile = File_Select( psz_fontname );
    if( !psz_monofontfile )
        psz_monofontfile = File_Select( psz_monofontfamily );

    if( Init_FT( p_this, psz_fontfile, fontindex, f_outline_thickness ) != VLC_SUCCESS )
        goto error;

    p_sys->pp_font_attachments = NULL;
    p_sys->i_font_attachments = 0;

    p_filter->pf_render_text = RenderText;
    p_filter->pf_render_html = RenderHtml;

    LoadFontsFromAttachments( p_filter );

    free( psz_fontfile );
    free( psz_monofontfile );

    return VLC_SUCCESS;

error:
    free( psz_fontfile );
    free( psz_monofontfile );
    free( psz_fontname );
    free( psz_monofontfamily );
    free( p_sys );
    return VLC_EGENERIC;
}


static void Destroy_FT( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->p_stroker )
        FT_Stroker_Done( p_sys->p_stroker );
    FT_Done_Face( p_sys->p_face );
    FT_Done_FreeType( p_sys->p_library );
}

/*****************************************************************************
 * Destroy: destroy Clone video thread output method
 *****************************************************************************
 * Clean up all data and library connections
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->pp_font_attachments )
    {
        for( int k = 0; k < p_sys->i_font_attachments; k++ )
            vlc_input_attachment_Delete( p_sys->pp_font_attachments[k] );

        free( p_sys->pp_font_attachments );
    }

    if( p_sys->p_xml ) xml_ReaderDelete( p_sys->p_xml );
    free( p_sys->style.psz_fontname );
    free( p_sys->style.psz_monofontname );

    Destroy_FT( p_this );
    free( p_sys );
}
