/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2015 VLC authors and VideoLAN
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Bernie Purcell <bitmap@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Felix Paul Kühne <fkuehne@videolan.org>
 *          Salah-Eddin Shaban <salshaaban@gmail.com>
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
#include <vlc_input.h>                             /* vlc_input_attachment_* */
#include <vlc_filter.h>                                      /* filter_sys_t */
#include <vlc_subpicture.h>
#include <vlc_text_style.h>                                   /* text_style_t*/
#include <vlc_charset.h>

#include <assert.h>

#include "platform_fonts.h"
#include "freetype.h"
#include "text_layout.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Create ( vlc_object_t * );
static void Destroy( vlc_object_t * );

#define FONT_TEXT N_("Font")
#define MONOSPACE_FONT_TEXT N_("Monospace Font")

#define FAMILY_LONGTEXT N_("Font family for the font you want to use")
#define FONT_LONGTEXT N_("Font file for the font you want to use")

#define OPACITY_TEXT N_("Text opacity")
#define OPACITY_LONGTEXT N_("The opacity (inverse of transparency) of the " \
    "text that will be rendered on the video. 0 = transparent, " \
    "255 = totally opaque." )
#define COLOR_TEXT N_("Text default color")
#define COLOR_LONGTEXT N_("The color of the text that will be rendered on "\
    "the video. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white" )

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

#define TEXT_DIRECTION_TEXT N_("Text direction")
#define TEXT_DIRECTION_LONGTEXT N_("Paragraph base direction for the Unicode bi-directional algorithm.")


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

#ifdef HAVE_FRIBIDI
static const int pi_text_direction[] = {
    0, 1, 2,
};
static const char *const ppsz_text_direction[] = {
    N_("Left to right"), N_("Right to left"), N_("Auto"),
};
#endif

vlc_module_begin ()
    set_shortname( N_("Text renderer"))
    set_description( N_("Freetype2 font renderer") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SUBPIC )

#ifdef HAVE_GET_FONT_BY_FAMILY_NAME
    add_font("freetype-font", DEFAULT_FAMILY, FONT_TEXT, FAMILY_LONGTEXT)
    add_font("freetype-monofont", DEFAULT_MONOSPACE_FAMILY,
             MONOSPACE_FONT_TEXT, FAMILY_LONGTEXT)
#else
    add_loadfile("freetype-font", DEFAULT_FONT_FILE, FONT_TEXT, FONT_LONGTEXT)
    add_loadfile("freetype-monofont", DEFAULT_MONOSPACE_FONT_FILE,
                 MONOSPACE_FONT_TEXT, FONT_LONGTEXT)
#endif

    /* opacity valid on 0..255, with default 255 = fully opaque */
    add_integer_with_range( "freetype-opacity", 255, 0, 255,
        OPACITY_TEXT, OPACITY_LONGTEXT, false )
        change_safe()

    /* hook to the color values list, with default 0x00ffffff = white */
    add_rgb("freetype-color", 0x00FFFFFF, COLOR_TEXT, COLOR_LONGTEXT)
        change_integer_list( pi_color_values, ppsz_color_descriptions )
        change_integer_range( 0x000000, 0xFFFFFF )
        change_safe()

    add_bool( "freetype-bold", false, BOLD_TEXT, NULL, false )
        change_safe()

    add_integer_with_range( "freetype-background-opacity", 0, 0, 255,
                            BG_OPACITY_TEXT, NULL, false )
        change_safe()
    add_rgb("freetype-background-color", 0x00000000, BG_COLOR_TEXT, NULL)
        change_integer_list( pi_color_values, ppsz_color_descriptions )
        change_integer_range( 0x000000, 0xFFFFFF )
        change_safe()

    add_integer_with_range( "freetype-outline-opacity", 255, 0, 255,
                            OUTLINE_OPACITY_TEXT, NULL, false )
        change_safe()
    add_rgb("freetype-outline-color", 0x00000000, OUTLINE_COLOR_TEXT, NULL)
        change_integer_list( pi_color_values, ppsz_color_descriptions )
        change_integer_range( 0x000000, 0xFFFFFF )
        change_safe()
    add_integer_with_range( "freetype-outline-thickness", 4, 0, 50, OUTLINE_THICKNESS_TEXT,
             NULL, false )
        change_integer_list( pi_outline_thickness, ppsz_outline_thickness )
        change_safe()

    add_integer_with_range( "freetype-shadow-opacity", 128, 0, 255,
                            SHADOW_OPACITY_TEXT, NULL, false )
        change_safe()
    add_rgb("freetype-shadow-color", 0x00000000, SHADOW_COLOR_TEXT, NULL)
        change_integer_list( pi_color_values, ppsz_color_descriptions )
        change_integer_range( 0x000000, 0xFFFFFF )
        change_safe()
    add_float_with_range( "freetype-shadow-angle", -45, -360, 360,
                          SHADOW_ANGLE_TEXT, NULL, false )
        change_safe()
    add_float_with_range( "freetype-shadow-distance", 0.06, 0.0, 1.0,
                          SHADOW_DISTANCE_TEXT, NULL, false )
        change_safe()

    add_obsolete_integer( "freetype-fontsize" );
    add_obsolete_integer( "freetype-rel-fontsize" );
    add_obsolete_integer( "freetype-effect" );

    add_bool( "freetype-yuvp", false, YUVP_TEXT,
              YUVP_LONGTEXT, true )

#ifdef HAVE_FRIBIDI
    add_integer_with_range( "freetype-text-direction", 0, 0, 2, TEXT_DIRECTION_TEXT,
                            TEXT_DIRECTION_LONGTEXT, false )
        change_integer_list( pi_text_direction, ppsz_text_direction )
        change_safe()
#endif

    set_capability( "text renderer", 100 )
    add_shortcut( "text" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

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

static FT_Vector GetAlignedOffset( const line_desc_t *p_line,
                                   const FT_BBox *p_textbbox,
                                   int i_align )
{
    FT_Vector offsets = { 0, 0 };
    const int i_text_width = p_textbbox->xMax - p_textbbox->xMin;
    if ( p_line->i_width < i_text_width &&
        (i_align & SUBPICTURE_ALIGN_LEFT) == 0 )
    {
        /* Left offset to take into account alignment */
        if( i_align & SUBPICTURE_ALIGN_RIGHT )
            offsets.x = ( i_text_width - p_line->i_width );
        else /* center */
            offsets.x = ( i_text_width - p_line->i_width ) / 2;
    }
    else
    {
        offsets.x = p_textbbox->xMin - p_line->bbox.xMin;
    }
    return offsets;
}

static bool IsSupportedAttachment( const char *psz_mime )
{
    static const char * fontMimeTypes[] =
    {
        "application/x-truetype-font", // TTF
        "application/x-font-otf",  // OTF
        "application/font-sfnt",
        "font/ttf",
        "font/otf",
        "font/sfnt",
    };

    for( size_t i=0; i<ARRAY_SIZE(fontMimeTypes); ++i )
    {
        if( !strcmp( psz_mime, fontMimeTypes[i] ) )
            return true;
    }

    return false;
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
    FT_Face               p_face = NULL;

    if( filter_GetInputAttachments( p_filter, &pp_attachments, &i_attachments_cnt ) )
        return VLC_EGENERIC;

    p_sys->i_font_attachments = 0;
    p_sys->pp_font_attachments = vlc_alloc( i_attachments_cnt, sizeof(*p_sys->pp_font_attachments));
    if( !p_sys->pp_font_attachments )
    {
        for( int i = 0; i < i_attachments_cnt; ++i )
            vlc_input_attachment_Delete( pp_attachments[ i ] );
        free( pp_attachments );
        return VLC_ENOMEM;
    }

    int k = 0;
    for( ; k < i_attachments_cnt; k++ )
    {
        input_attachment_t *p_attach = pp_attachments[k];

        if( p_attach->i_data > 0 && p_attach->p_data &&
            IsSupportedAttachment( p_attach->psz_mime ) )
        {
            p_sys->pp_font_attachments[ p_sys->i_font_attachments++ ] = p_attach;

            int i_font_idx = 0;

            while( 0 == FT_New_Memory_Face( p_sys->p_library,
                                            p_attach->p_data,
                                            p_attach->i_data,
                                            i_font_idx,
                                            &p_face ))
            {

                bool b_bold = p_face->style_flags & FT_STYLE_FLAG_BOLD;
                bool b_italic = p_face->style_flags & FT_STYLE_FLAG_ITALIC;

                vlc_family_t *p_family = DeclareNewFamily( p_sys->fs, p_face->family_name );
                if( unlikely( !p_family ) )
                    goto error;

                char *psz_fontfile;
                if( asprintf( &psz_fontfile, ":/%d",
                              p_sys->i_font_attachments - 1 ) < 0
                 || !NewFont( psz_fontfile, i_font_idx, b_bold, b_italic, p_family ) )
                    goto error;

                FT_Done_Face( p_face );
                p_face = NULL;

                /* Add font attachment to the "attachments" fallback list */
                DeclareFamilyAsAttachMenFallback( p_sys->fs, p_family );

                i_font_idx++;
            }
        }
        else
        {
            vlc_input_attachment_Delete( p_attach );
        }
    }

    free( pp_attachments );

    return VLC_SUCCESS;

error:
    if( p_face )
        FT_Done_Face( p_face );

    for( int i = k + 1; i < i_attachments_cnt; ++i )
        vlc_input_attachment_Delete( pp_attachments[ i ] );

    free( pp_attachments );
    return VLC_ENOMEM;
}

/*****************************************************************************
 * RenderYUVP: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
static int RenderYUVP( filter_t *p_filter, subpicture_region_t *p_region,
                       line_desc_t *p_line,
                       FT_BBox *p_regionbbox, FT_BBox *p_paddedbbox,
                       FT_BBox *p_bbox )
{
    VLC_UNUSED(p_filter);
    VLC_UNUSED(p_paddedbbox);
    static const uint8_t pi_gamma[16] =
        {0x00, 0x52, 0x84, 0x96, 0xb8, 0xca, 0xdc, 0xee, 0xff,
          0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    uint8_t *p_dst;
    video_format_t fmt;
    int i, i_pitch;
    unsigned int x, y;
    uint8_t i_y, i_u, i_v; /* YUV values, derived from incoming RGB */

    /* Create a new subpicture region */
    video_format_Init( &fmt, VLC_CODEC_YUVP );
    fmt.i_width          =
    fmt.i_visible_width  = p_regionbbox->xMax - p_regionbbox->xMin + 4;
    fmt.i_height         =
    fmt.i_visible_height = p_regionbbox->yMax - p_regionbbox->yMin + 4;
    const unsigned regionnum = p_region->fmt.i_sar_num;
    const unsigned regionden = p_region->fmt.i_sar_den;
    fmt.i_sar_num = fmt.i_sar_den = 1;
    fmt.transfer  = p_region->fmt.transfer;
    fmt.primaries = p_region->fmt.primaries;
    fmt.space     = p_region->fmt.space;
    fmt.mastering = p_region->fmt.mastering;

    assert( !p_region->p_picture );
    p_region->p_picture = picture_NewFromFormat( &fmt );
    if( !p_region->p_picture )
        return VLC_EGENERIC;
    fmt.p_palette = p_region->fmt.p_palette ? p_region->fmt.p_palette : malloc(sizeof(*fmt.p_palette));
    p_region->fmt = fmt;
    fmt.i_sar_num = regionnum;
    fmt.i_sar_den = regionden;

    /* Calculate text color components
     * Only use the first color */
    const int i_alpha = p_line->p_character[0].p_style->i_font_alpha;
    YUVFromRGB( p_line->p_character[0].p_style->i_font_color, &i_y, &i_u, &i_v );

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
        FT_Vector offset = GetAlignedOffset( p_line, p_bbox, p_region->i_align );

        for( i = 0; i < p_line->i_character_count; i++ )
        {
            const line_character_t *ch = &p_line->p_character[i];
            FT_BitmapGlyph p_glyph = ch->p_glyph;

            int i_glyph_y = offset.y + p_regionbbox->yMax - p_glyph->top + p_line->i_base_line;
            int i_glyph_x = offset.x + p_glyph->left - p_regionbbox->xMin;

            for( y = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++ )
                {
                    if( p_glyph->bitmap.buffer[y * p_glyph->bitmap.pitch + x] )
                        p_dst[(i_glyph_y + y) * i_pitch + (i_glyph_x + x)] =
                            (p_glyph->bitmap.buffer[y * p_glyph->bitmap.pitch + x] + 8)/16;
                }
            }
        }
    }

    /* Outlining (find something better than nearest neighbour filtering ?) */
    if( 1 )
    {
        p_dst = p_region->p_picture->Y_PIXELS;
        uint8_t *p_top = p_dst; /* Use 1st line as a cache */
        uint8_t left, current;

        for( y = 1; y < fmt.i_height - 1; y++ )
        {
            if( y > 1 ) memcpy( p_top, p_dst, fmt.i_width );
            p_dst += p_region->p_picture->Y_PITCH;
            left = 0;

            for( x = 1; x < fmt.i_width - 1; x++ )
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
    if( i_alpha == 0 )
        return;

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
        int i_ani = 255 - i_an;
        int i_aoni = i_ao * i_ani / 255;
        *p_a = 255 - (255 - *p_a) * i_ani / 255;
        if( *p_a != 0 )
        {
            *p_y = ( *p_y * i_aoni + i_y * i_an ) / *p_a;
            *p_u = ( *p_u * i_aoni + i_u * i_an ) / *p_a;
            *p_v = ( *p_v * i_aoni + i_v * i_an ) / *p_a;
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
    if( i_alpha == 0 )
        return;

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
        int i_ani = 255 - i_an;
        p_rgba[3] = 255 - (255 - p_rgba[3]) * i_ani / 255;
        if( p_rgba[3] != 0 )
        {
            int i_aoni = i_ao * i_ani / 255;
            p_rgba[0] = ( p_rgba[0] * i_aoni + i_r * i_an ) / p_rgba[3];
            p_rgba[1] = ( p_rgba[1] * i_aoni + i_g * i_an ) / p_rgba[3];
            p_rgba[2] = ( p_rgba[2] * i_aoni + i_b * i_an ) / p_rgba[3];
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
    if (alpha == 0)
        return;

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
        int ani = 255 - an;
        rgba[0] = 255 - (255 - rgba[0]) * ani / 255;
        if (rgba[0] != 0)
        {
            int aoni = ao * ani / 255;
            rgba[1] = (rgba[1] * aoni + r * an ) / rgba[0];
            rgba[2] = (rgba[2] * aoni + g * an ) / rgba[0];
            rgba[3] = (rgba[3] * aoni + b * an ) / rgba[0];
        }
    }
}

static inline void BlendAXYZGlyph( picture_t *p_picture,
                                   int i_picture_x, int i_picture_y,
                                   int i_a, int i_x, int i_y, int i_z,
                                   FT_BitmapGlyph p_glyph,
                                   void (*BlendPixel)(picture_t *, int, int, int, int, int, int, int) )

{
    for( unsigned int dy = 0; dy < p_glyph->bitmap.rows; dy++ )
    {
        for( unsigned int dx = 0; dx < p_glyph->bitmap.width; dx++ )
            BlendPixel( p_picture, i_picture_x + dx, i_picture_y + dy,
                        i_a, i_x, i_y, i_z,
                        p_glyph->bitmap.buffer[dy * p_glyph->bitmap.pitch + dx] );
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
                                     FT_BBox *p_regionbbox,
                                     FT_BBox *p_paddedbbox,
                                     FT_BBox *p_textbbox,
                                     picture_t *p_picture,
                                     void (*ExtractComponents)( uint32_t, uint8_t *, uint8_t *, uint8_t * ),
                                     void (*BlendPixel)(picture_t *, int, int, int, int, int, int, int) )
{
    for( const line_desc_t *p_line = p_line_head; p_line != NULL; p_line = p_line->p_next )
    {
        FT_Vector offset = GetAlignedOffset( p_line, p_textbbox, p_region->i_text_align );

        FT_BBox linebgbox = p_line->bbox;
        linebgbox.xMin += offset.x;
        linebgbox.xMax += offset.x;
        linebgbox.yMax += offset.y;
        linebgbox.yMin += offset.y;

        if( p_line->i_first_visible_char_index < 0 )
            continue; /* only spaces */

        /* add padding */
        linebgbox.yMax += (p_paddedbbox->yMax - p_textbbox->yMax);
        linebgbox.yMin -= (p_textbbox->yMin - p_paddedbbox->yMin);
        linebgbox.xMin -= (p_textbbox->xMin - p_paddedbbox->xMin);
        linebgbox.xMax += (p_paddedbbox->xMax - p_textbbox->xMax);

        /* Compute lower boundary for the background
           continue down to next line top */
        if( p_line->p_next && p_line->p_next->i_first_visible_char_index >= 0 )
            linebgbox.yMin = __MIN(linebgbox.yMin, p_line->bbox.yMin - (p_line->bbox.yMin - p_line->p_next->bbox.yMax));

        /* Setup color for the background */
        const text_style_t *p_prev_style = p_line->p_character[p_line->i_first_visible_char_index].p_style;

        FT_BBox segmentbgbox = linebgbox;
        int i_char_index = p_line->i_first_visible_char_index;
        /* Compute the background for the line (identify leading/trailing space) */
        if( i_char_index > 0 )
        {
            segmentbgbox.xMin = offset.x +
                                p_line->p_character[p_line->i_first_visible_char_index].bbox.xMin -
                                /* padding offset */ (p_textbbox->xMin - p_paddedbbox->xMin);
        }

        while( i_char_index <= p_line->i_last_visible_char_index )
        {
            /* find last char having the same style */
            int i_seg_end = i_char_index;
            while( i_seg_end + 1 <= p_line->i_last_visible_char_index &&
                   p_prev_style == p_line->p_character[i_seg_end + 1].p_style )
            {
                i_seg_end++;
            }

            /* Find right boundary for bounding box for background */
            segmentbgbox.xMax = offset.x + p_line->p_character[i_seg_end].bbox.xMax;
            if( i_seg_end == p_line->i_last_visible_char_index ) /* add padding on last */
                segmentbgbox.xMax += (p_paddedbbox->xMax - p_textbbox->xMax);

            const line_character_t *p_char = &p_line->p_character[i_char_index];
            if( p_char->p_style->i_style_flags & STYLE_BACKGROUND )
            {
                uint8_t i_x, i_y, i_z;
                ExtractComponents( p_char->p_style->i_background_color,
                                   &i_x, &i_y, &i_z );
                const uint8_t i_alpha = p_char->p_style->i_background_alpha;

                /* Render the actual background */
                if( i_alpha != STYLE_ALPHA_TRANSPARENT )
                {
                    /* rebase and clip to SCREEN coordinates */
                    FT_BBox absbox =
                    {
                        .xMin = __MAX(0, segmentbgbox.xMin - p_regionbbox->xMin),
                        .xMax = VLC_CLIP(segmentbgbox.xMax - p_regionbbox->xMin,
                                         0, p_region->fmt.i_visible_width),
                        .yMin = VLC_CLIP(p_regionbbox->yMax - segmentbgbox.yMin,
                                         0, p_region->fmt.i_visible_height),
                        .yMax = __MAX(0, p_regionbbox->yMax - segmentbgbox.yMax),
                    };

                    for( int dy = absbox.yMax; dy < absbox.yMin; dy++ )
                    {
                        for( int dx = absbox.xMin; dx < absbox.xMax; dx++ )
                            BlendPixel( p_picture, dx, dy, i_alpha, i_x, i_y, i_z, 0xff );
                    }
                }
            }

            segmentbgbox.xMin = segmentbgbox.xMax;
            i_char_index = i_seg_end + 1;
            p_prev_style = p_line->p_character->p_style;
        }

    }
}

static void RenderCharAXYZ( filter_t *p_filter,
                           picture_t *p_picture,
                           const line_desc_t *p_line,
                           int i_offset_x,
                           int i_offset_y,
                           int g,
                           void (*ExtractComponents)( uint32_t, uint8_t *, uint8_t *, uint8_t * ),
                           void (*BlendPixel)(picture_t *, int, int, int, int, int, int, int) )
{
    VLC_UNUSED(p_filter);
    /* Render all glyphs and underline/strikethrough */
    for( int i = p_line->i_first_visible_char_index; i <= p_line->i_last_visible_char_index; i++ )
    {
        const line_character_t *ch = &p_line->p_character[i];
        const FT_BitmapGlyph p_glyph = g == 0 ? ch->p_shadow : g == 1 ? ch->p_outline : ch->p_glyph;
        if( !p_glyph )
            continue;

        uint8_t i_a = ch->p_style->i_font_alpha;

        uint32_t i_color;
        switch (g) {/* Apply font alpha ratio to shadow/outline alpha */
        case 0:
            i_a     = i_a * ch->p_style->i_shadow_alpha / 255;
            i_color = ch->p_style->i_shadow_color;
            break;
        case 1:
            i_a     = i_a * ch->p_style->i_outline_alpha / 255;
            i_color = ch->p_style->i_outline_color;
            break;
        default:
            i_color = ch->p_style->i_font_color;
            break;
        }

        if(ch->p_ruby && ch->p_ruby->p_laid)
        {
            RenderCharAXYZ( p_filter,
                            p_picture,
                            ch->p_ruby->p_laid,
                            i_offset_x, i_offset_y,
                            2,
                            ExtractComponents,
                            BlendPixel );
        }

        /* Don't render if invisible or not wanted */
        if( i_a == STYLE_ALPHA_TRANSPARENT ||
           (g == 0 && 0 == (ch->p_style->i_style_flags & STYLE_SHADOW) ) ||
           (g == 1 && 0 == (ch->p_style->i_style_flags & STYLE_OUTLINE) )
          )
            continue;

        uint8_t i_x, i_y, i_z;
        ExtractComponents( i_color, &i_x, &i_y, &i_z );

        int i_glyph_y = i_offset_y - p_glyph->top;
        int i_glyph_x = i_offset_x + p_glyph->left;

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

static inline int RenderAXYZ( filter_t *p_filter,
                              subpicture_region_t *p_region,
                              line_desc_t *p_line_head,
                              FT_BBox *p_regionbbox,
                              FT_BBox *p_paddedtextbbox,
                              FT_BBox *p_textbbox,
                              vlc_fourcc_t i_chroma,
                              const video_format_t *fmt_out,
                              void (*ExtractComponents)( uint32_t, uint8_t *, uint8_t *, uint8_t * ),
                              void (*FillPicture)( picture_t *p_picture, int, int, int, int ),
                              void (*BlendPixel)(picture_t *, int, int, int, int, int, int, int) )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Create a new subpicture region */
    video_format_t fmt;
    video_format_Init( &fmt, i_chroma );
    fmt.i_width          =
    fmt.i_visible_width  = p_regionbbox->xMax - p_regionbbox->xMin;
    fmt.i_height         =
    fmt.i_visible_height = p_regionbbox->yMax - p_regionbbox->yMin;
    const unsigned regionnum = p_region->fmt.i_sar_num;
    const unsigned regionden = p_region->fmt.i_sar_den;
    fmt.i_sar_num = fmt.i_sar_den = 1;
    fmt.transfer  = fmt_out->transfer;
    fmt.primaries = fmt_out->primaries;
    fmt.space     = fmt_out->space;
    fmt.mastering = fmt_out->mastering;

    picture_t *p_picture = p_region->p_picture = picture_NewFromFormat( &fmt );
    if( !p_region->p_picture )
        return VLC_EGENERIC;

    p_region->fmt = fmt;
    p_region->fmt.i_sar_num = regionnum;
    p_region->fmt.i_sar_den = regionden;

    /* Initialize the picture background */
    const text_style_t *p_style = p_sys->p_default_style;
    uint8_t i_x, i_y, i_z;

    if (p_region->b_noregionbg) {
        /* Render the background just under the text */
        FillPicture( p_picture, STYLE_ALPHA_TRANSPARENT, 0x00, 0x00, 0x00 );
    } else {
        /* Render background under entire subpicture block */
        ExtractComponents( p_style->i_background_color, &i_x, &i_y, &i_z );
        FillPicture( p_picture, p_style->i_background_alpha, i_x, i_y, i_z );
    }

    /* Render text's background (from decoder) if any */
    RenderBackground(p_region, p_line_head,
                     p_regionbbox, p_paddedtextbbox, p_textbbox,
                     p_picture, ExtractComponents, BlendPixel);

    /* Render shadow then outline and then normal glyphs */
    for( int g = 0; g < 3; g++ )
    {
        /* Render all lines */
        for( line_desc_t *p_line = p_line_head; p_line != NULL; p_line = p_line->p_next )
        {
            FT_Vector offset = GetAlignedOffset( p_line, p_textbbox, p_region->i_text_align );

            int i_glyph_offset_y = offset.y + p_regionbbox->yMax + p_line->i_base_line;
            int i_glyph_offset_x = offset.x - p_regionbbox->xMin;

            RenderCharAXYZ( p_filter, p_picture, p_line,
                            i_glyph_offset_x, i_glyph_offset_y, g,
                            ExtractComponents, BlendPixel );
        }
    }

    return VLC_SUCCESS;
}

static void UpdateDefaultLiveStyles( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    text_style_t *p_style = p_sys->p_default_style;

    p_style->i_font_color = var_InheritInteger( p_filter, "freetype-color" );

    p_style->i_background_alpha = var_InheritInteger( p_filter, "freetype-background-opacity" );
    p_style->i_background_color = var_InheritInteger( p_filter, "freetype-background-color" );
}

static void FillDefaultStyles( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

#ifdef HAVE_GET_FONT_BY_FAMILY_NAME
    p_sys->p_default_style->psz_fontname = var_InheritString( p_filter, "freetype-font" );
#endif
    /* Set default psz_fontname */
    if( !p_sys->p_default_style->psz_fontname || !*p_sys->p_default_style->psz_fontname )
    {
        free( p_sys->p_default_style->psz_fontname );
        p_sys->p_default_style->psz_fontname = strdup( DEFAULT_FAMILY );
    }

#ifdef HAVE_GET_FONT_BY_FAMILY_NAME
    p_sys->p_default_style->psz_monofontname = var_InheritString( p_filter, "freetype-monofont" );
#endif
    /* set default psz_monofontname */
    if( !p_sys->p_default_style->psz_monofontname || !*p_sys->p_default_style->psz_monofontname )
    {
        free( p_sys->p_default_style->psz_monofontname );
        p_sys->p_default_style->psz_monofontname = strdup( DEFAULT_MONOSPACE_FAMILY );
    }

    UpdateDefaultLiveStyles( p_filter );

    p_sys->p_default_style->i_font_alpha = var_InheritInteger( p_filter, "freetype-opacity" );

    p_sys->p_default_style->i_outline_alpha = var_InheritInteger( p_filter, "freetype-outline-opacity" );
    p_sys->p_default_style->i_outline_color = var_InheritInteger( p_filter, "freetype-outline-color" );

    p_sys->p_default_style->i_shadow_alpha = var_InheritInteger( p_filter, "freetype-shadow-opacity" );
    p_sys->p_default_style->i_shadow_color = var_InheritInteger( p_filter, "freetype-shadow-color" );

    p_sys->p_default_style->i_font_size = 0;
    p_sys->p_default_style->i_style_flags |= STYLE_SHADOW;
    p_sys->p_default_style->i_features |= STYLE_HAS_FLAGS;

    if( var_InheritBool( p_filter, "freetype-bold" ) )
    {
        p_sys->p_forced_style->i_style_flags |= STYLE_BOLD;
        p_sys->p_forced_style->i_features |= STYLE_HAS_FLAGS;
    }

    /* Apply forced styles to defaults, if any */
    text_style_Merge( p_sys->p_default_style, p_sys->p_forced_style, true );
}

static void FreeRubyBlockArray( ruby_block_t **pp_array, size_t i_array )
{
    ruby_block_t *p_lyt = NULL;
    for( size_t i = 0; i< i_array; i++ )
    {
        if( p_lyt != pp_array[i] )
        {
            p_lyt = pp_array[i];
            if( p_lyt )
            {
                free( p_lyt->p_uchars );
                text_style_Delete( p_lyt->p_style );
                if( p_lyt->p_laid )
                    FreeLines( p_lyt->p_laid );
                free( p_lyt );
            }
        }
    }
    free( pp_array );
}

static void FreeStylesArray( text_style_t **pp_styles, size_t i_styles )
{
    text_style_t *p_style = NULL;
    for( size_t i = 0; i< i_styles; i++ )
    {
        if( p_style != pp_styles[i] )
        {
            p_style = pp_styles[i];
            text_style_Delete( p_style );
        }
    }
    free( pp_styles );
}

static size_t AddTextAndStyles( filter_sys_t *p_sys,
                                const char *psz_text, const char *psz_rt,
                                const text_style_t *p_style,
                                layout_text_block_t *p_text_block )
{
    /* Convert chars to unicode */
    size_t i_bytes;
    uni_char_t *p_ucs4 = ToCharset( FREETYPE_TO_UCS, psz_text, &i_bytes );
    if( !p_ucs4 )
        return 0;

    const size_t i_newchars = i_bytes / 4;
    if( SIZE_MAX / 4 < p_text_block->i_count + i_newchars )
    {
        free( p_ucs4 );
        return 0;
    }
    size_t i_realloc = (p_text_block->i_count + i_newchars) * 4;
    void *p_realloc = realloc( p_text_block->p_uchars, i_realloc );
    if( unlikely(!p_realloc) )
        return 0;
    p_text_block->p_uchars = p_realloc;

    /* We want one per segment shared text_style_t* per unicode character */
    if( SIZE_MAX / sizeof(text_style_t *) < p_text_block->i_count + i_newchars )
        return 0;
    i_realloc = (p_text_block->i_count + i_newchars) * sizeof(text_style_t *);
    p_realloc = realloc( p_text_block->pp_styles, i_realloc );
    if ( unlikely(!p_realloc) )
        return 0;
    p_text_block->pp_styles = p_realloc;

    /* Same for ruby text */
    if( SIZE_MAX / sizeof(text_segment_ruby_t *) < p_text_block->i_count + i_newchars )
        return 0;
    i_realloc = (p_text_block->i_count + i_newchars) * sizeof(text_segment_ruby_t *);
    p_realloc = realloc( p_text_block->pp_ruby, i_realloc );
    if ( unlikely(!p_realloc) )
        return 0;
    p_text_block->pp_ruby = p_realloc;

    /* Copy data */
    memcpy( &p_text_block->p_uchars[p_text_block->i_count], p_ucs4, i_newchars * 4 );
    free( p_ucs4 );

    text_style_t *p_mgstyle = text_style_Duplicate( p_sys->p_default_style );
    if ( p_mgstyle == NULL )
        return 0;

    if( p_style )
        /* Replace defaults with segment values */
        text_style_Merge( p_mgstyle, p_style, true );

    /* Overwrite any default or value with forced ones */
    text_style_Merge( p_mgstyle, p_sys->p_forced_style, true );

    /* map it to each char of that segment */
    for ( size_t i = 0; i < i_newchars; ++i )
        p_text_block->pp_styles[p_text_block->i_count + i] = p_mgstyle;

    ruby_block_t *p_rubyblock = NULL;
    if( psz_rt )
    {
        p_ucs4 = ToCharset( FREETYPE_TO_UCS, psz_rt, &i_bytes );
        if( !p_ucs4 )
            return 0;
        p_rubyblock = malloc(sizeof(ruby_block_t));
        if( p_rubyblock )
        {
            p_rubyblock->p_style = text_style_Duplicate( p_mgstyle );
            if( !p_rubyblock->p_style )
            {
                free( p_ucs4 );
                free( p_rubyblock );
                return 0;
            }
            p_rubyblock->p_style->i_font_size *= 0.4;
            p_rubyblock->p_style->f_font_relsize *= 0.4;
            p_rubyblock->p_uchars = p_ucs4;
            p_rubyblock->i_count = i_bytes / 4;
            p_rubyblock->p_laid = NULL;
        }
        else free( p_ucs4 );
    }
    for ( size_t i = 0; i < i_newchars; ++i )
        p_text_block->pp_ruby[p_text_block->i_count + i] = p_rubyblock;

    /* now safe to update total nb */
    p_text_block->i_count += i_newchars;

    return i_newchars;
}

static size_t SegmentsToTextAndStyles( filter_t *p_filter, const text_segment_t *p_segment,
                                       layout_text_block_t *p_text_block )
{
    size_t i_nb_char = 0;

    for( const text_segment_t *s = p_segment; s != NULL; s = s->p_next )
    {
        if( !s->psz_text || !s->psz_text[0] )
            continue;

        if( s->p_ruby )
        {
            for( const text_segment_ruby_t *p_ruby = s->p_ruby;
                                            p_ruby; p_ruby = p_ruby->p_next )
            {
                i_nb_char += AddTextAndStyles( p_filter->p_sys,
                                               p_ruby->psz_base, p_ruby->psz_rt,
                                               s->style, p_text_block );
            }
        }
        else
        {
            i_nb_char += AddTextAndStyles( p_filter->p_sys,
                                           s->psz_text, NULL,
                                           s->style, p_text_block );
        }
    }

    return i_nb_char;
}

/**
 * This function renders a text subpicture region into another one.
 * It also calculates the size needed for this string, and renders the
 * needed glyphs into memory. It is used as pf_add_string callback in
 * the vout method by this module
 */
static int Render( filter_t *p_filter, subpicture_region_t *p_region_out,
                         subpicture_region_t *p_region_in,
                         const vlc_fourcc_t *p_chroma_list )
{
    if( !p_region_in )
        return VLC_EGENERIC;

    filter_sys_t *p_sys = p_filter->p_sys;
    bool b_grid = p_region_in->b_gridmode;
    p_sys->i_scale = ( b_grid ) ? 100 : var_InheritInteger( p_filter, "sub-text-scale");

    UpdateDefaultLiveStyles( p_filter );

    /*
     * Update the default face to reflect changes in video size or text scaling
     */
    p_sys->p_face = SelectAndLoadFace( p_filter, p_sys->p_default_style, ' ' );
    if( !p_sys->p_face )
    {
        msg_Err( p_filter, "Render(): Error loading default face" );
        return VLC_EGENERIC;
    }

    layout_text_block_t text_block = { 0 };
    text_block.b_balanced = p_region_in->b_balanced_text;
    text_block.b_grid = p_region_in->b_gridmode;
    text_block.i_count = SegmentsToTextAndStyles( p_filter, p_region_in->p_text,
                                                  &text_block );
    if( text_block.i_count == 0 )
    {
        free( text_block.pp_styles );
        free( text_block.p_uchars );
        return VLC_EGENERIC;
    }

    /* */
    int rv = VLC_SUCCESS;
    FT_BBox bbox;
    int i_max_face_height;

    unsigned i_max_width = p_filter->fmt_out.video.i_visible_width;
    if( p_region_in->i_max_width > 0 && (unsigned) p_region_in->i_max_width < i_max_width )
        i_max_width = p_region_in->i_max_width;
    else if( p_region_in->i_x > 0 && (unsigned)p_region_in->i_x < i_max_width )
        i_max_width -= p_region_in->i_x;

    unsigned i_max_height = p_filter->fmt_out.video.i_visible_height;
    if( p_region_in->i_max_height > 0 && (unsigned) p_region_in->i_max_height < i_max_height )
        i_max_height = p_region_in->i_max_height;
    else if( p_region_in->i_y > 0 && (unsigned)p_region_in->i_y < i_max_height )
        i_max_height -= p_region_in->i_y;

    text_block.i_max_width = i_max_width;
    text_block.i_max_height = i_max_height;
    rv = LayoutTextBlock( p_filter, &text_block, &text_block.p_laid, &bbox, &i_max_face_height );

    /* Don't attempt to render text that couldn't be layed out
     * properly. */
    if( !rv && text_block.i_count > 0 && bbox.xMin < bbox.xMax && bbox.yMin < bbox.yMax )
    {
        const vlc_fourcc_t p_chroma_list_yuvp[] = { VLC_CODEC_YUVP, 0 };
        const vlc_fourcc_t p_chroma_list_rgba[] = { VLC_CODEC_RGBA, 0 };

        uint8_t i_background_opacity = var_InheritInteger( p_filter, "freetype-background-opacity" );
        i_background_opacity = VLC_CLIP( i_background_opacity, 0, 255 );
        int i_margin = (i_background_opacity > 0 && !p_region_in->b_gridmode) ? i_max_face_height / 4 : 0;

        if( (unsigned)i_margin * 2 >= i_max_width || (unsigned)i_margin * 2 >= i_max_height )
            i_margin = 0;

        if( var_InheritBool( p_filter, "freetype-yuvp" ) )
            p_chroma_list = p_chroma_list_yuvp;
        else if( !p_chroma_list || *p_chroma_list == 0 )
            p_chroma_list = p_chroma_list_rgba;

        FT_BBox paddedbbox = bbox;
        paddedbbox.xMin -= i_margin;
        paddedbbox.xMax += i_margin;
        paddedbbox.yMin -= i_margin;
        paddedbbox.yMax += i_margin;

        FT_BBox regionbbox = paddedbbox;

        /* _______regionbbox_______________
         * |                               |
         * |                               |
         * |                               |
         * |     _bbox(<paddedbbox)___     |
         * |    |         rightaligned|    |
         * |    |            textlines|    |
         * |    |_____________________|    |
         * |_______________________________|
         *
         * we need at least 3 bounding boxes.
         * regionbbox containing the whole, including region background pixels
         * paddedbox an enlarged text box when for drawing text background
         * bbox the lines bounding box for all glyphs
         * For simple unstyled subs, bbox == paddedbox == regionbbox
         */

        unsigned outertext_w = (regionbbox.xMax - regionbbox.xMin);
        if( outertext_w < (unsigned) p_region_in->i_max_width )
        {
            if( p_region_in->i_text_align & SUBPICTURE_ALIGN_RIGHT )
                regionbbox.xMin -= (p_region_in->i_max_width - outertext_w);
            else if( p_region_in->i_text_align & SUBPICTURE_ALIGN_LEFT )
                regionbbox.xMax += (p_region_in->i_max_width - outertext_w);
            else
            {
                regionbbox.xMin -= (p_region_in->i_max_width - outertext_w) / 2;
                regionbbox.xMax += (p_region_in->i_max_width - outertext_w + 1) / 2;
            }
        }

        unsigned outertext_h = (regionbbox.yMax - regionbbox.yMin);
        if( outertext_h < (unsigned) p_region_in->i_max_height )
        {
            if( p_region_in->i_text_align & SUBPICTURE_ALIGN_TOP )
                regionbbox.yMin -= (p_region_in->i_max_height - outertext_h);
            else if( p_region_in->i_text_align & SUBPICTURE_ALIGN_BOTTOM )
                regionbbox.yMax += (p_region_in->i_max_height - outertext_h);
            else
            {
                regionbbox.yMin -= (p_region_in->i_max_height - outertext_h + 1) / 2;
                regionbbox.yMax += (p_region_in->i_max_height - outertext_h) / 2;
            }
        }

//        unsigned bboxcolor = 0xFF000000;
        /* TODO 4.0. No region self BG color for VLC 3.0 API*/

        /* Avoid useless pixels:
         *        reshrink/trim Region Box to padded text one,
         *        but update offsets to keep position and have same rendering */
//        if( (bboxcolor & 0xFF) == 0 )
        {
            p_region_out->i_x = (paddedbbox.xMin - regionbbox.xMin) + p_region_in->i_x;
            p_region_out->i_y = (regionbbox.yMax - paddedbbox.yMax) + p_region_in->i_y;
            regionbbox = paddedbbox;
        }
//        else /* case where the bounding box is larger and visible */
//        {
//            p_region_out->i_x = p_region_in->i_x;
//            p_region_out->i_y = p_region_in->i_y;
//        }

        for( const vlc_fourcc_t *p_chroma = p_chroma_list; *p_chroma != 0; p_chroma++ )
        {
            rv = VLC_EGENERIC;
            if( *p_chroma == VLC_CODEC_YUVP )
                rv = RenderYUVP( p_filter, p_region_out, text_block.p_laid,
                                 &regionbbox, &paddedbbox, &bbox );
            else if( *p_chroma == VLC_CODEC_YUVA )
                rv = RenderAXYZ( p_filter, p_region_out, text_block.p_laid,
                                 &regionbbox, &paddedbbox, &bbox,
                                 VLC_CODEC_YUVA,
                                 &p_region_out->fmt,
                                 YUVFromRGB,
                                 FillYUVAPicture,
                                 BlendYUVAPixel );
            else if( *p_chroma == VLC_CODEC_RGBA
                  || *p_chroma == VLC_CODEC_BGRA )
                rv = RenderAXYZ( p_filter, p_region_out, text_block.p_laid,
                                 &regionbbox, &paddedbbox, &bbox,
                                 *p_chroma,
                                 &p_region_out->fmt,
                                 RGBFromRGB,
                                 FillRGBAPicture,
                                 BlendRGBAPixel );
            else if( *p_chroma == VLC_CODEC_ARGB )
                rv = RenderAXYZ( p_filter, p_region_out, text_block.p_laid,
                                 &regionbbox, &paddedbbox, &bbox,
                                 VLC_CODEC_ARGB,
                                 &p_region_out->fmt,
                                 RGBFromRGB,
                                 FillARGBPicture,
                                 BlendARGBPixel );

            if( !rv )
                break;
        }
    }

    FreeLines( text_block.p_laid );

    free( text_block.p_uchars );
    FreeStylesArray( text_block.pp_styles, text_block.i_count );
    if( text_block.pp_ruby )
        FreeRubyBlockArray( text_block.pp_ruby, text_block.i_count );

    return rv;
}

static void FreeFace( void *p_face, void *p_obj )
{
    VLC_UNUSED( p_obj );

    FT_Done_Face( ( FT_Face ) p_face );
}

/*****************************************************************************
 * Create: allocates osd-text video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t      *p_filter         = ( filter_t * ) p_this;
    filter_sys_t  *p_sys            = NULL;

    /* Allocate structure */
    p_filter->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* Init Freetype and its stroker */
    if( FT_Init_FreeType( &p_sys->p_library ) )
    {
        msg_Err( p_filter, "Failed to initialize FreeType" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( FT_Stroker_New( p_sys->p_library, &p_sys->p_stroker ) )
    {
        msg_Err( p_filter, "Failed to create stroker for outlining" );
        p_sys->p_stroker = NULL;
    }

    /* Dictionnaries for fonts */
    vlc_dictionary_init( &p_sys->face_map, 50 );

    p_sys->i_scale = 100;

    /* default style to apply to uncomplete segmeents styles */
    p_sys->p_default_style = text_style_Create( STYLE_FULLY_SET );
    if(unlikely(!p_sys->p_default_style))
        goto error;

    /* empty style for style overriding cases */
    p_sys->p_forced_style = text_style_Create( STYLE_NO_DEFAULTS );
    if(unlikely(!p_sys->p_forced_style))
        goto error;

#ifndef HAVE_GET_FONT_BY_FAMILY_NAME
    p_sys->psz_fontfile = var_InheritString( p_filter, "freetype-font" );
    p_sys->psz_monofontfile = var_InheritString( p_filter, "freetype-monofont" );
#endif

    /* fills default and forced style */
    FillDefaultStyles( p_filter );

    /*
     * The following variables should not be cached, as they might be changed on-the-fly:
     * freetype-rel-fontsize, freetype-background-opacity, freetype-background-color,
     * freetype-outline-thickness, freetype-color
     *
     */

    float f_shadow_angle       = var_InheritFloat( p_filter, "freetype-shadow-angle" );
    float f_shadow_distance    = var_InheritFloat( p_filter, "freetype-shadow-distance" );
    f_shadow_distance          = VLC_CLIP( f_shadow_distance, 0, 1 );
    p_sys->f_shadow_vector_x   = f_shadow_distance * cosf((float)(2. * M_PI) * f_shadow_angle / 360);
    p_sys->f_shadow_vector_y   = f_shadow_distance * sinf((float)(2. * M_PI) * f_shadow_angle / 360);

    p_sys->fs = FontSelectNew( p_filter  );
    if( !p_sys->fs )
        goto error;

    if( LoadFontsFromAttachments( p_filter ) == VLC_ENOMEM )
        goto error;

    p_sys->p_face = SelectAndLoadFace( p_filter, p_sys->p_default_style, ' ' );
    if( !p_sys->p_face )
    {
        msg_Err( p_filter, "Error loading default face %s",
                 p_sys->p_default_style->psz_fontname );
        goto error;
    }

    p_filter->pf_render = Render;

    return VLC_SUCCESS;

error:
    Destroy( VLC_OBJECT(p_filter) );
    return VLC_EGENERIC;
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

#ifdef DEBUG_PLATFORM_FONTS
    DumpFamilies( p_sys->fs );
#endif

    free( p_sys->psz_fontfile );
    free( p_sys->psz_monofontfile );

    /* Text styles */
    text_style_Delete( p_sys->p_default_style );
    text_style_Delete( p_sys->p_forced_style );

    /* Fonts dicts */
    vlc_dictionary_clear( &p_sys->face_map, FreeFace, p_filter );

    /* Attachments */
    if( p_sys->pp_font_attachments )
    {
        for( int k = 0; k < p_sys->i_font_attachments; k++ )
            vlc_input_attachment_Delete( p_sys->pp_font_attachments[k] );

        free( p_sys->pp_font_attachments );
    }

    FontSelectDelete( p_sys->fs );

    /* Freetype */
    if( p_sys->p_stroker )
        FT_Stroker_Done( p_sys->p_stroker );

    FT_Done_FreeType( p_sys->p_library );

    free( p_sys );
}

