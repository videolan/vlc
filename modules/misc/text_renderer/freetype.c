/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2011 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Bernie Purcell <bitmap@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>                        /* stream_MemoryNew */
#include <vlc_input.h>                         /* vlc_input_attachment_* */
#include <vlc_xml.h>                           /* xml_reader */
#include <vlc_strings.h>                       /* resolve_xml_special_chars */
#include <vlc_charset.h>                       /* ToCharset */
#include <vlc_dialog.h>                        /* FcCache dialog */
#include <vlc_filter.h>                                      /* filter_sys_t */
#include <vlc_text_style.h>                                   /* text_style_t*/
#include <vlc_memory.h>                                   /* realloc_or_free */

/* Default fonts */
#ifdef __APPLE__
# define DEFAULT_FONT_FILE "/Library/Fonts/Arial Black.ttf"
# define DEFAULT_FAMILY "Arial Black"
#elif defined( WIN32 )
# define DEFAULT_FONT_FILE "arial.ttf" /* Default path font found at run-time */
# define DEFAULT_FAMILY "Arial"
#elif defined( HAVE_MAEMO )
# define DEFAULT_FONT_FILE "/usr/share/fonts/nokia/nosnb.ttf"
# define DEFAULT_FAMILY "Nokia Sans Bold"
#else
# define DEFAULT_FONT_FILE "/usr/share/fonts/truetype/freefont/FreeSerifBold.ttf"
# define DEFAULT_FAMILY "Serif Bold"
#endif

/* Freetype */
#include <freetype/ftsynth.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#define FT_FLOOR(X)     ((X & -64) >> 6)
#define FT_CEIL(X)      (((X + 63) & -64) >> 6)
#ifndef FT_MulFix
 #define FT_MulFix(v, s) (((v)*(s))>>16)
#endif

/* RTL */
#if defined(HAVE_FRIBIDI)
# include <fribidi/fribidi.h>
#endif

/* Win32 GDI */
#ifdef WIN32
# include <windows.h>
# include <shlobj.h>
# define HAVE_STYLES
# undef HAVE_FONTCONFIG
#endif

/* FontConfig */
#ifdef HAVE_FONTCONFIG
# include <fontconfig/fontconfig.h>
# define HAVE_STYLES
#endif

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Create ( vlc_object_t * );
static void Destroy( vlc_object_t * );

#define FONT_TEXT N_("Font")

#define FAMILY_LONGTEXT N_("Font family for the font you want to use")
#define FONT_LONGTEXT N_("Font file for the font you want to use")

#define FONTSIZE_TEXT N_("Font size in pixels")
#define FONTSIZE_LONGTEXT N_("This is the default size of the fonts " \
    "that will be rendered on the video. " \
    "If set to something different than 0 this option will override the " \
    "relative font size." )
#define OPACITY_TEXT N_("Opacity")
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

static const int pi_sizes[] = { 20, 18, 16, 12, 6 };
static const char *const ppsz_sizes_text[] = {
    N_("Smaller"), N_("Small"), N_("Normal"), N_("Large"), N_("Larger") };
#define YUVP_TEXT N_("Use YUVP renderer")
#define YUVP_LONGTEXT N_("This renders the font using \"paletized YUV\". " \
  "This option is only needed if you want to encode into DVB subtitles" )
#define EFFECT_TEXT N_("Font Effect")
#define EFFECT_LONGTEXT N_("It is possible to apply effects to the rendered " \
"text to improve its readability." )

enum { EFFECT_BACKGROUND  = 1,
       EFFECT_OUTLINE     = 2,
       EFFECT_OUTLINE_FAT = 3,
};
static int const pi_effects[] = { EFFECT_BACKGROUND, EFFECT_OUTLINE, EFFECT_OUTLINE_FAT };
static const char *const ppsz_effects_text[] = {
    N_("Background"),N_("Outline"), N_("Fat Outline") };

static const int pi_color_values[] = {
  0x00000000, 0x00808080, 0x00C0C0C0, 0x00FFFFFF, 0x00800000,
  0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00808000, 0x00008000, 0x00008080,
  0x0000FF00, 0x00800080, 0x00000080, 0x000000FF, 0x0000FFFF };

static const char *const ppsz_color_descriptions[] = {
  N_("Black"), N_("Gray"), N_("Silver"), N_("White"), N_("Maroon"),
  N_("Red"), N_("Fuchsia"), N_("Yellow"), N_("Olive"), N_("Green"), N_("Teal"),
  N_("Lime"), N_("Purple"), N_("Navy"), N_("Blue"), N_("Aqua") };

vlc_module_begin ()
    set_shortname( N_("Text renderer"))
    set_description( N_("Freetype2 font renderer") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SUBPIC )

#ifdef HAVE_STYLES
    add_font( "freetype-font", DEFAULT_FAMILY, FONT_TEXT, FAMILY_LONGTEXT, false )
#else
    add_loadfile( "freetype-font", DEFAULT_FONT_FILE, FONT_TEXT, FONT_LONGTEXT, false )
#endif

    add_integer( "freetype-fontsize", 0, FONTSIZE_TEXT,
                 FONTSIZE_LONGTEXT, true )
        change_safe()

    /* opacity valid on 0..255, with default 255 = fully opaque */
    add_integer_with_range( "freetype-opacity", 255, 0, 255,
        OPACITY_TEXT, OPACITY_LONGTEXT, false )
        change_safe()

    /* hook to the color values list, with default 0x00ffffff = white */
    add_integer( "freetype-color", 0x00FFFFFF, COLOR_TEXT,
                 COLOR_LONGTEXT, false )
        change_integer_list( pi_color_values, ppsz_color_descriptions )
        change_safe()

    add_integer( "freetype-rel-fontsize", 16, FONTSIZER_TEXT,
                 FONTSIZER_LONGTEXT, false )
        change_integer_list( pi_sizes, ppsz_sizes_text )
        change_safe()

    add_integer( "freetype-effect", 2, EFFECT_TEXT,
                 EFFECT_LONGTEXT, false )
        change_integer_list( pi_effects, ppsz_effects_text )
        change_safe()

    add_bool( "freetype-yuvp", false, YUVP_TEXT,
              YUVP_LONGTEXT, true )
    set_capability( "text renderer", 100 )
    add_shortcut( "text" )
    set_callbacks( Create, Destroy )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct line_desc_t line_desc_t;
struct line_desc_t
{
    /** NULL-terminated list of glyphs making the string */
    FT_BitmapGlyph *pp_glyphs;
    /** list of relative positions for the glyphs */
    FT_Vector      *p_glyph_pos;
    /** list of RGB information for styled text */
    uint32_t       *p_fg_rgb;
    uint32_t       *p_bg_rgb;
    uint8_t        *p_fg_bg_ratio; /* 0x00=100% FG --> 0x7F=100% BG */
    /** underline information -- only supplied if text should be underlined */
    int            *pi_underline_offset;
    uint16_t       *pi_underline_thickness;

    int             i_width;
    int             i_height;

    int             i_alpha;

    line_desc_t    *p_next;
};

typedef struct font_stack_t font_stack_t;
struct font_stack_t
{
    char          *psz_name;
    int            i_size;
    uint32_t       i_color;            /* ARGB */
    uint32_t       i_karaoke_bg_color; /* ARGB */

    font_stack_t  *p_next;
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
    bool           i_use_kerning;
    uint8_t        i_font_opacity;
    int            i_font_color;
    int            i_font_size;
    int            i_effect;

    int            i_default_font_size;
    int            i_display_height;
    char*          psz_fontfamily;
#ifdef HAVE_STYLES
    xml_reader_t  *p_xml;
#ifdef WIN32
    char*          psz_win_fonts_path;
#endif
#endif

    input_attachment_t **pp_font_attachments;
    int                  i_font_attachments;
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
        int i_ratio = var_GetInteger( p_filter, "freetype-rel-fontsize" );
        if( i_ratio > 0 )
        {
            i_size = (int)p_filter->fmt_out.video.i_height / i_ratio;
            p_filter->p_sys->i_display_height = p_filter->fmt_out.video.i_height;
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

    p_sys->i_font_size = i_size;

    if( FT_Set_Pixel_Sizes( p_sys->p_face, 0, i_size ) )
    {
        msg_Err( p_filter, "couldn't set font size to %d", i_size );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

#ifdef HAVE_STYLES
#ifdef HAVE_FONTCONFIG
static void FontConfig_BuildCache( filter_t *p_filter )
{
    /* */
    msg_Dbg( p_filter, "Building font databases.");
    mtime_t t1, t2;
    t1 = mdate();

#ifdef WIN32
    dialog_progress_bar_t *p_dialog = NULL;
    FcConfig *fcConfig = FcInitLoadConfig();

    p_dialog = dialog_ProgressCreate( p_filter,
            _("Building font cache"),
            _("Please wait while your font cache is rebuilt.\n"
                "This should take less than a few minutes."), NULL );

/*    if( p_dialog )
        dialog_ProgressSet( p_dialog, NULL, 0.5 ); */

    FcConfigBuildFonts( fcConfig );
    if( p_dialog )
    {
//        dialog_ProgressSet( p_dialog, NULL, 1.0 );
        dialog_ProgressDestroy( p_dialog );
        p_dialog = NULL;
    }
#endif
    t2 = mdate();
    msg_Dbg( p_filter, "Took %ld microseconds", (long)((t2 - t1)) );
}

/***
 * \brief Selects a font matching family, bold, italic provided
 ***/
static char* FontConfig_Select( FcConfig* config, const char* family,
                          bool b_bold, bool b_italic, int i_size, int *i_idx )
{
    FcResult result = FcResultMatch;
    FcPattern *pat, *p_pat;
    FcChar8* val_s;
    FcBool val_b;

    /* Create a pattern and fills it */
    pat = FcPatternCreate();
    if (!pat) return NULL;

    /* */
    FcPatternAddString( pat, FC_FAMILY, (const FcChar8*)family );
    FcPatternAddBool( pat, FC_OUTLINE, FcTrue );
    FcPatternAddInteger( pat, FC_SLANT, b_italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN );
    FcPatternAddInteger( pat, FC_WEIGHT, b_bold ? FC_WEIGHT_EXTRABOLD : FC_WEIGHT_NORMAL );
    if( i_size != -1 )
    {
        char *psz_fontsize;
        if( asprintf( &psz_fontsize, "%d", i_size ) != -1 )
        {
            FcPatternAddString( pat, FC_SIZE, (const FcChar8 *)psz_fontsize );
            free( psz_fontsize );
        }
    }

    /* */
    FcDefaultSubstitute( pat );
    if( !FcConfigSubstitute( config, pat, FcMatchPattern ) )
    {
        FcPatternDestroy( pat );
        return NULL;
    }

    /* Find the best font for the pattern, destroy the pattern */
    p_pat = FcFontMatch( config, pat, &result );
    FcPatternDestroy( pat );
    if( !p_pat || result == FcResultNoMatch ) return NULL;

    /* Check the new pattern */
    if( ( FcResultMatch != FcPatternGetBool( p_pat, FC_OUTLINE, 0, &val_b ) )
        || ( val_b != FcTrue ) )
    {
        FcPatternDestroy( p_pat );
        return NULL;
    }
    if( FcResultMatch != FcPatternGetInteger( p_pat, FC_INDEX, 0, i_idx ) )
    {
        *i_idx = 0;
    }

    if( FcResultMatch != FcPatternGetString( p_pat, FC_FAMILY, 0, &val_s ) )
    {
        FcPatternDestroy( p_pat );
        return NULL;
    }

    /* if( strcasecmp((const char*)val_s, family ) != 0 )
        msg_Warn( p_filter, "fontconfig: selected font family is not"
                            "the requested one: '%s' != '%s'\n",
                            (const char*)val_s, family );   */

    if( FcResultMatch != FcPatternGetString( p_pat, FC_FILE, 0, &val_s ) )
    {
        FcPatternDestroy( p_pat );
        return NULL;
    }

    FcPatternDestroy( p_pat );
    return strdup( (const char*)val_s );
}
#endif

#ifdef WIN32
#define UNICODE
#define FONT_DIR_NT "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts"

static int GetFileFontByName( const char *font_name, char **psz_filename )
{
    HKEY hKey;
    wchar_t vbuffer[MAX_PATH];
    wchar_t dbuffer[256];

    if( RegOpenKeyEx(HKEY_LOCAL_MACHINE, FONT_DIR_NT, 0, KEY_READ, &hKey) != ERROR_SUCCESS )
        return 1;

    for( int index = 0;; index++ )
    {
        DWORD vbuflen = MAX_PATH - 1;
        DWORD dbuflen = 255;

        if( RegEnumValueW( hKey, index, vbuffer, &vbuflen,
                           NULL, NULL, (LPBYTE)dbuffer, &dbuflen) != ERROR_SUCCESS )
            return 2;

        char *psz_value = FromWide( vbuffer );

        char *s = strchr( psz_value,'(' );
        if( s != NULL && s != psz_value ) s[-1] = '\0';

        /* Manage concatenated font names */
        if( strchr( psz_value, '&') ) {
            if( strcasestr( psz_value, font_name ) != NULL )
                break;
        }
        else {
            if( strcasecmp( psz_value, font_name ) == 0 )
                break;
        }
    }

    *psz_filename = FromWide( dbuffer );
    return 0;
}


static int CALLBACK EnumFontCallback(const ENUMLOGFONTEX *lpelfe, const NEWTEXTMETRICEX *metric,
                                     DWORD type, LPARAM lParam)
{
    VLC_UNUSED( metric );
    if( (type & RASTER_FONTTYPE) ) return 1;
    // if( lpelfe->elfScript ) FIXME

    return GetFileFontByName( (const char *)lpelfe->elfFullName, (char **)lParam );
}

static char* Win32_Select( filter_t *p_filter, const char* family,
                           bool b_bold, bool b_italic, int i_size, int *i_idx )
{
    VLC_UNUSED( i_size );
    // msg_Dbg( p_filter, "Here in Win32_Select, asking for %s", family );

    /* */
    LOGFONT lf;
    lf.lfCharSet = DEFAULT_CHARSET;
    if( b_italic )
        lf.lfItalic = true;
    if( b_bold )
        lf.lfWeight = FW_BOLD;
    strncpy( (LPSTR)&lf.lfFaceName, family, 32);

    /* */
    char *psz_filename = NULL;
    HDC hDC = GetDC( NULL );
    EnumFontFamiliesEx(hDC, &lf, (FONTENUMPROC)&EnumFontCallback, (LPARAM)&psz_filename, 0);
    ReleaseDC(NULL, hDC);

    if( psz_filename == NULL )
        return NULL;

    /* FIXME: increase i_idx, when concatenated strings  */
    i_idx = 0;

    /* */
    char *psz_tmp;
    if( asprintf( &psz_tmp, "%s\\%s", p_filter->p_sys->psz_win_fonts_path, psz_filename ) == -1 )
        return NULL;
    return psz_tmp;
}
#endif

#endif


/*****************************************************************************
 * RenderYUVP: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
static int RenderYUVP( filter_t *p_filter, subpicture_region_t *p_region,
                       line_desc_t *p_line, int i_width, int i_height )
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
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_CODEC_YUVP;
    fmt.i_width = fmt.i_visible_width = i_width + 4;
    fmt.i_height = fmt.i_visible_height = i_height + 4;
    if( p_region->fmt.i_visible_width > 0 )
        fmt.i_visible_width = p_region->fmt.i_visible_width;
    if( p_region->fmt.i_visible_height > 0 )
        fmt.i_visible_height = p_region->fmt.i_visible_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    assert( !p_region->p_picture );
    p_region->p_picture = picture_NewFromFormat( &fmt );
    if( !p_region->p_picture )
        return VLC_EGENERIC;
    fmt.p_palette = p_region->fmt.p_palette ? p_region->fmt.p_palette : malloc(sizeof(*fmt.p_palette));
    p_region->fmt = fmt;

    /* Calculate text color components
     * Only use the first color */
    YUVFromRGB( p_line->p_fg_rgb[ 0 ], &i_y, &i_u, &i_v );

    /* Build palette */
    fmt.p_palette->i_entries = 16;
    for( i = 0; i < 8; i++ )
    {
        fmt.p_palette->palette[i][0] = 0;
        fmt.p_palette->palette[i][1] = 0x80;
        fmt.p_palette->palette[i][2] = 0x80;
        fmt.p_palette->palette[i][3] = pi_gamma[i];
        fmt.p_palette->palette[i][3] =
            (int)fmt.p_palette->palette[i][3] * (255 - p_line->i_alpha) / 255;
    }
    for( i = 8; i < fmt.p_palette->i_entries; i++ )
    {
        fmt.p_palette->palette[i][0] = i * 16 * i_y / 256;
        fmt.p_palette->palette[i][1] = i_u;
        fmt.p_palette->palette[i][2] = i_v;
        fmt.p_palette->palette[i][3] = pi_gamma[i];
        fmt.p_palette->palette[i][3] =
            (int)fmt.p_palette->palette[i][3] * (255 - p_line->i_alpha) / 255;
    }

    p_dst = p_region->p_picture->Y_PIXELS;
    i_pitch = p_region->p_picture->Y_PITCH;

    /* Initialize the region pixels */
    memset( p_dst, 0, i_pitch * p_region->fmt.i_height );

    for( ; p_line != NULL; p_line = p_line->p_next )
    {
        int i_glyph_tmax = 0;
        int i_bitmap_offset, i_offset, i_align_offset = 0;
        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];
            i_glyph_tmax = __MAX( i_glyph_tmax, p_glyph->top );
        }

        if( p_line->i_width < i_width )
        {
            if( (p_region->i_align & 0x3) == SUBPICTURE_ALIGN_RIGHT )
            {
                i_align_offset = i_width - p_line->i_width;
            }
            else if( (p_region->i_align & 0x3) != SUBPICTURE_ALIGN_LEFT )
            {
                i_align_offset = ( i_width - p_line->i_width ) / 2;
            }
        }

        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];

            i_offset = ( p_line->p_glyph_pos[ i ].y +
                i_glyph_tmax - p_glyph->top + 2 ) *
                i_pitch + p_line->p_glyph_pos[ i ].x + p_glyph->left + 2 +
                i_align_offset;

            for( y = 0, i_bitmap_offset = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++, i_bitmap_offset++ )
                {
                    if( p_glyph->bitmap.buffer[i_bitmap_offset] )
                        p_dst[i_offset+x] =
                         ((int)p_glyph->bitmap.buffer[i_bitmap_offset] + 8)/16;
                }
                i_offset += i_pitch;
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

static void UnderlineGlyphYUVA( int i_line_thickness, int i_line_offset, bool b_ul_next_char,
                                FT_BitmapGlyph  p_this_glyph, FT_Vector *p_this_glyph_pos,
                                FT_BitmapGlyph  p_next_glyph, FT_Vector *p_next_glyph_pos,
                                int i_glyph_tmax, int i_align_offset,
                                uint8_t i_y, uint8_t i_u, uint8_t i_v,
                                subpicture_region_t *p_region)
{
    int i_pitch;
    uint8_t *p_dst_y,*p_dst_u,*p_dst_v,*p_dst_a;

    p_dst_y = p_region->p_picture->Y_PIXELS;
    p_dst_u = p_region->p_picture->U_PIXELS;
    p_dst_v = p_region->p_picture->V_PIXELS;
    p_dst_a = p_region->p_picture->A_PIXELS;
    i_pitch = p_region->p_picture->A_PITCH;

    int i_offset = ( p_this_glyph_pos->y + i_glyph_tmax + i_line_offset + 3 ) * i_pitch +
                     p_this_glyph_pos->x + p_this_glyph->left + 3 + i_align_offset;

    for( int y = 0; y < i_line_thickness; y++ )
    {
        int i_extra = p_this_glyph->bitmap.width;

        if( b_ul_next_char )
        {
            i_extra = (p_next_glyph_pos->x + p_next_glyph->left) -
                      (p_this_glyph_pos->x + p_this_glyph->left);
        }
        for( int x = 0; x < i_extra; x++ )
        {
            bool b_ok = true;

            /* break the underline around the tails of any glyphs which cross it */
            /* Strikethrough doesn't get broken */
            for( int z = x - i_line_thickness;
                 z < x + i_line_thickness && b_ok && (i_line_offset >= 0);
                 z++ )
            {
                if( p_next_glyph && ( z >= i_extra ) )
                {
                    int i_row = i_line_offset + p_next_glyph->top + y;

                    if( ( p_next_glyph->bitmap.rows > i_row ) &&
                        p_next_glyph->bitmap.buffer[p_next_glyph->bitmap.width * i_row + z-i_extra] )
                    {
                        b_ok = false;
                    }
                }
                else if ((z > 0 ) && (z < p_this_glyph->bitmap.width))
                {
                    int i_row = i_line_offset + p_this_glyph->top + y;

                    if( ( p_this_glyph->bitmap.rows > i_row ) &&
                        p_this_glyph->bitmap.buffer[p_this_glyph->bitmap.width * i_row + z] )
                    {
                        b_ok = false;
                    }
                }
            }

            if( b_ok )
            {
                p_dst_y[i_offset+x] = (i_y * 255) >> 8;
                p_dst_u[i_offset+x] = i_u;
                p_dst_v[i_offset+x] = i_v;
                p_dst_a[i_offset+x] = 255;
            }
        }
        i_offset += i_pitch;
    }
}

static void DrawBlack( line_desc_t *p_line, int i_width, subpicture_region_t *p_region, int xoffset, int yoffset )
{
    uint8_t *p_dst = p_region->p_picture->A_PIXELS;
    int i_pitch = p_region->p_picture->A_PITCH;
    int y;

    for( ; p_line != NULL; p_line = p_line->p_next )
    {
        int i_glyph_tmax=0, i = 0;
        int i_bitmap_offset, i_offset, i_align_offset = 0;
        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];
            i_glyph_tmax = __MAX( i_glyph_tmax, p_glyph->top );
        }

        if( p_line->i_width < i_width )
        {
            if( (p_region->i_align & 0x3) == SUBPICTURE_ALIGN_RIGHT )
            {
                i_align_offset = i_width - p_line->i_width;
            }
            else if( (p_region->i_align & 0x3) != SUBPICTURE_ALIGN_LEFT )
            {
                i_align_offset = ( i_width - p_line->i_width ) / 2;
            }
        }

        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];

            i_offset = ( p_line->p_glyph_pos[ i ].y +
                i_glyph_tmax - p_glyph->top + 3 + yoffset ) *
                i_pitch + p_line->p_glyph_pos[ i ].x + p_glyph->left + 3 +
                i_align_offset +xoffset;

            for( y = 0, i_bitmap_offset = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( int x = 0; x < p_glyph->bitmap.width; x++, i_bitmap_offset++ )
                {
                    if( p_glyph->bitmap.buffer[i_bitmap_offset] )
                        if( p_dst[i_offset+x] <
                            ((int)p_glyph->bitmap.buffer[i_bitmap_offset]) )
                            p_dst[i_offset+x] =
                                ((int)p_glyph->bitmap.buffer[i_bitmap_offset]);
                }
                i_offset += i_pitch;
            }
        }
    }
}

/*****************************************************************************
 * RenderYUVA: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
static int RenderYUVA( filter_t *p_filter, subpicture_region_t *p_region,
                   line_desc_t *p_line, int i_width, int i_height )
{
    uint8_t *p_dst_y,*p_dst_u,*p_dst_v,*p_dst_a;
    video_format_t fmt;
    int i, y, i_pitch, i_alpha;

    if( i_width == 0 || i_height == 0 )
        return VLC_SUCCESS;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_CODEC_YUVA;
    fmt.i_width = fmt.i_visible_width = i_width + 6;
    fmt.i_height = fmt.i_visible_height = i_height + 6;
    if( p_region->fmt.i_visible_width > 0 )
        fmt.i_visible_width = p_region->fmt.i_visible_width;
    if( p_region->fmt.i_visible_height > 0 )
        fmt.i_visible_height = p_region->fmt.i_visible_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    p_region->p_picture = picture_NewFromFormat( &fmt );
    if( !p_region->p_picture )
        return VLC_EGENERIC;
    p_region->fmt = fmt;

    /* Save the alpha value */
    i_alpha = p_line->i_alpha;

    p_dst_y = p_region->p_picture->Y_PIXELS;
    p_dst_u = p_region->p_picture->U_PIXELS;
    p_dst_v = p_region->p_picture->V_PIXELS;
    p_dst_a = p_region->p_picture->A_PIXELS;
    i_pitch = p_region->p_picture->A_PITCH;

    /* Initialize the region pixels */
    memset( p_dst_y, 0x00, i_pitch * p_region->fmt.i_height );
    memset( p_dst_u, 0x80, i_pitch * p_region->fmt.i_height );
    memset( p_dst_v, 0x80, i_pitch * p_region->fmt.i_height );

    if( p_filter->p_sys->i_effect != EFFECT_BACKGROUND )
        memset( p_dst_a, 0x00, i_pitch * p_region->fmt.i_height );
    else
        memset( p_dst_a, 0x80, i_pitch * p_region->fmt.i_height );

    if( p_filter->p_sys->i_effect == EFFECT_OUTLINE ||
        p_filter->p_sys->i_effect == EFFECT_OUTLINE_FAT )
    {
        DrawBlack( p_line, i_width, p_region,  0,  0);
        DrawBlack( p_line, i_width, p_region, -1,  0);
        DrawBlack( p_line, i_width, p_region,  0, -1);
        DrawBlack( p_line, i_width, p_region,  1,  0);
        DrawBlack( p_line, i_width, p_region,  0,  1);
    }

    if( p_filter->p_sys->i_effect == EFFECT_OUTLINE_FAT )
    {
        DrawBlack( p_line, i_width, p_region, -1, -1);
        DrawBlack( p_line, i_width, p_region, -1,  1);
        DrawBlack( p_line, i_width, p_region,  1, -1);
        DrawBlack( p_line, i_width, p_region,  1,  1);

        DrawBlack( p_line, i_width, p_region, -2,  0);
        DrawBlack( p_line, i_width, p_region,  0, -2);
        DrawBlack( p_line, i_width, p_region,  2,  0);
        DrawBlack( p_line, i_width, p_region,  0,  2);

        DrawBlack( p_line, i_width, p_region, -2, -2);
        DrawBlack( p_line, i_width, p_region, -2,  2);
        DrawBlack( p_line, i_width, p_region,  2, -2);
        DrawBlack( p_line, i_width, p_region,  2,  2);

        DrawBlack( p_line, i_width, p_region, -3,  0);
        DrawBlack( p_line, i_width, p_region,  0, -3);
        DrawBlack( p_line, i_width, p_region,  3,  0);
        DrawBlack( p_line, i_width, p_region,  0,  3);
    }

    for( ; p_line != NULL; p_line = p_line->p_next )
    {
        int i_glyph_tmax = 0;
        int i_bitmap_offset, i_offset, i_align_offset = 0;
        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];
            i_glyph_tmax = __MAX( i_glyph_tmax, p_glyph->top );
        }

        if( p_line->i_width < i_width )
        {
            if( (p_region->i_align & 0x3) == SUBPICTURE_ALIGN_RIGHT )
            {
                i_align_offset = i_width - p_line->i_width;
            }
            else if( (p_region->i_align & 0x3) != SUBPICTURE_ALIGN_LEFT )
            {
                i_align_offset = ( i_width - p_line->i_width ) / 2;
            }
        }

        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];

            i_offset = ( p_line->p_glyph_pos[ i ].y +
                i_glyph_tmax - p_glyph->top + 3 ) *
                i_pitch + p_line->p_glyph_pos[ i ].x + p_glyph->left + 3 +
                i_align_offset;

            /* Every glyph can (and in fact must) have its own color */
            uint8_t i_y, i_u, i_v;
            YUVFromRGB( p_line->p_fg_rgb[ i ], &i_y, &i_u, &i_v );

            for( y = 0, i_bitmap_offset = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( int x = 0; x < p_glyph->bitmap.width; x++, i_bitmap_offset++ )
                {
                    uint8_t i_y_local = i_y;
                    uint8_t i_u_local = i_u;
                    uint8_t i_v_local = i_v;

                    if( p_line->p_fg_bg_ratio != 0x00 )
                    {
                        int i_split = p_glyph->bitmap.width *
                                      p_line->p_fg_bg_ratio[ i ] / 0x7f;

                        if( x > i_split )
                        {
                            YUVFromRGB( p_line->p_bg_rgb[ i ],
                                        &i_y_local, &i_u_local, &i_v_local );
                        }
                    }

                    if( p_glyph->bitmap.buffer[i_bitmap_offset] )
                    {
                        p_dst_y[i_offset+x] = ((p_dst_y[i_offset+x] *(255-(int)p_glyph->bitmap.buffer[i_bitmap_offset])) +
                                              i_y * ((int)p_glyph->bitmap.buffer[i_bitmap_offset])) >> 8;

                        p_dst_u[i_offset+x] = i_u;
                        p_dst_v[i_offset+x] = i_v;

                        if( p_filter->p_sys->i_effect == EFFECT_BACKGROUND )
                            p_dst_a[i_offset+x] = 0xff;
                    }
                }
                i_offset += i_pitch;
            }

            if( p_line->pi_underline_thickness[ i ] )
            {
                UnderlineGlyphYUVA( p_line->pi_underline_thickness[ i ],
                                    p_line->pi_underline_offset[ i ],
                                   (p_line->pp_glyphs[i+1] && (p_line->pi_underline_thickness[ i + 1] > 0)),
                                    p_line->pp_glyphs[i], &(p_line->p_glyph_pos[i]),
                                    p_line->pp_glyphs[i+1], &(p_line->p_glyph_pos[i+1]),
                                    i_glyph_tmax, i_align_offset,
                                    i_y, i_u, i_v,
                                    p_region);
            }
        }
    }

    /* Apply the alpha setting */
    for( i = 0; i < (int)fmt.i_height * i_pitch; i++ )
        p_dst_a[i] = p_dst_a[i] * (255 - i_alpha) / 255;

    return VLC_SUCCESS;
}

static text_style_t *CreateStyle( char *psz_fontname, int i_font_size,
                                  uint32_t i_font_color, uint32_t i_karaoke_bg_color,
                                  int i_style_flags )
{
    text_style_t *p_style = text_style_New();
    if( !p_style )
        return NULL;

    p_style->psz_fontname = strdup( psz_fontname );
    p_style->i_font_size  = i_font_size;
    p_style->i_font_color = (i_font_color & 0x00ffffff) >>  0;
    p_style->i_font_alpha = (i_font_color & 0xff000000) >> 24;
    p_style->i_karaoke_background_color = (i_karaoke_bg_color & 0x00ffffff) >>  0;
    p_style->i_karaoke_background_alpha = (i_karaoke_bg_color & 0xff000000) >> 24;
    p_style->i_style_flags |= i_style_flags;
    return p_style;
}

static bool StyleEquals( text_style_t *s1, text_style_t *s2 )
{
    if( !s1 || !s2 )
        return false;
    if( s1 == s2 )
        return true;

    return s1->i_font_size   == s2->i_font_size &&
           s1->i_font_color  == s2->i_font_color &&
           s1->i_font_alpha  == s2->i_font_alpha &&
           s1->i_style_flags == s2->i_style_flags &&
           !strcmp( s1->psz_fontname, s2->psz_fontname );
}

static void IconvText( filter_t *p_filter, const char *psz_string,
                       size_t *i_string_length, uint32_t *psz_unicode )
{
    *i_string_length = 0;
    if( psz_unicode == NULL )
        return;

    size_t i_length;
    uint32_t *psz_tmp =
#if defined(WORDS_BIGENDIAN)
            ToCharset( "UCS-4BE", psz_string, &i_length );
#else
            ToCharset( "UCS-4LE", psz_string, &i_length );
#endif
    if( !psz_tmp )
    {
        msg_Warn( p_filter, "failed to convert string to unicode (%m)" );
        return;
    }
    memcpy( psz_unicode, psz_tmp, i_length );
    *i_string_length = i_length / 4;

    free( psz_tmp );
}

static int PushFont( font_stack_t **p_font, const char *psz_name, int i_size,
                     uint32_t i_color, uint32_t i_karaoke_bg_color )
{
    if( !p_font )
        return VLC_EGENERIC;

    font_stack_t *p_new = malloc( sizeof(*p_new) );
    if( !p_new )
        return VLC_ENOMEM;

    p_new->p_next = NULL;

    if( psz_name )
        p_new->psz_name = strdup( psz_name );
    else
        p_new->psz_name = NULL;

    p_new->i_size              = i_size;
    p_new->i_color             = i_color;
    p_new->i_karaoke_bg_color  = i_karaoke_bg_color;

    if( !*p_font )
    {
        *p_font = p_new;
    }
    else
    {
        font_stack_t *p_last;

        for( p_last = *p_font;
             p_last->p_next;
             p_last = p_last->p_next )
        ;

        p_last->p_next = p_new;
    }
    return VLC_SUCCESS;
}

static int PopFont( font_stack_t **p_font )
{
    font_stack_t *p_last, *p_next_to_last;

    if( !p_font || !*p_font )
        return VLC_EGENERIC;

    p_next_to_last = NULL;
    for( p_last = *p_font;
         p_last->p_next;
         p_last = p_last->p_next )
    {
        p_next_to_last = p_last;
    }

    if( p_next_to_last )
        p_next_to_last->p_next = NULL;
    else
        *p_font = NULL;

    free( p_last->psz_name );
    free( p_last );

    return VLC_SUCCESS;
}

static int PeekFont( font_stack_t **p_font, char **psz_name, int *i_size,
                     uint32_t *i_color, uint32_t *i_karaoke_bg_color )
{
    font_stack_t *p_last;

    if( !p_font || !*p_font )
        return VLC_EGENERIC;

    for( p_last=*p_font;
         p_last->p_next;
         p_last=p_last->p_next )
    ;

    *psz_name            = p_last->psz_name;
    *i_size              = p_last->i_size;
    *i_color             = p_last->i_color;
    *i_karaoke_bg_color  = p_last->i_karaoke_bg_color;

    return VLC_SUCCESS;
}

static const struct {
    const char *psz_name;
    uint32_t   i_value;
} p_html_colors[] = {
    /* Official html colors */
    { "Aqua",    0x00FFFF },
    { "Black",   0x000000 },
    { "Blue",    0x0000FF },
    { "Fuchsia", 0xFF00FF },
    { "Gray",    0x808080 },
    { "Green",   0x008000 },
    { "Lime",    0x00FF00 },
    { "Maroon",  0x800000 },
    { "Navy",    0x000080 },
    { "Olive",   0x808000 },
    { "Purple",  0x800080 },
    { "Red",     0xFF0000 },
    { "Silver",  0xC0C0C0 },
    { "Teal",    0x008080 },
    { "White",   0xFFFFFF },
    { "Yellow",  0xFFFF00 },

    /* Common ones */
    { "AliceBlue", 0xF0F8FF },
    { "AntiqueWhite", 0xFAEBD7 },
    { "Aqua", 0x00FFFF },
    { "Aquamarine", 0x7FFFD4 },
    { "Azure", 0xF0FFFF },
    { "Beige", 0xF5F5DC },
    { "Bisque", 0xFFE4C4 },
    { "Black", 0x000000 },
    { "BlanchedAlmond", 0xFFEBCD },
    { "Blue", 0x0000FF },
    { "BlueViolet", 0x8A2BE2 },
    { "Brown", 0xA52A2A },
    { "BurlyWood", 0xDEB887 },
    { "CadetBlue", 0x5F9EA0 },
    { "Chartreuse", 0x7FFF00 },
    { "Chocolate", 0xD2691E },
    { "Coral", 0xFF7F50 },
    { "CornflowerBlue", 0x6495ED },
    { "Cornsilk", 0xFFF8DC },
    { "Crimson", 0xDC143C },
    { "Cyan", 0x00FFFF },
    { "DarkBlue", 0x00008B },
    { "DarkCyan", 0x008B8B },
    { "DarkGoldenRod", 0xB8860B },
    { "DarkGray", 0xA9A9A9 },
    { "DarkGrey", 0xA9A9A9 },
    { "DarkGreen", 0x006400 },
    { "DarkKhaki", 0xBDB76B },
    { "DarkMagenta", 0x8B008B },
    { "DarkOliveGreen", 0x556B2F },
    { "Darkorange", 0xFF8C00 },
    { "DarkOrchid", 0x9932CC },
    { "DarkRed", 0x8B0000 },
    { "DarkSalmon", 0xE9967A },
    { "DarkSeaGreen", 0x8FBC8F },
    { "DarkSlateBlue", 0x483D8B },
    { "DarkSlateGray", 0x2F4F4F },
    { "DarkSlateGrey", 0x2F4F4F },
    { "DarkTurquoise", 0x00CED1 },
    { "DarkViolet", 0x9400D3 },
    { "DeepPink", 0xFF1493 },
    { "DeepSkyBlue", 0x00BFFF },
    { "DimGray", 0x696969 },
    { "DimGrey", 0x696969 },
    { "DodgerBlue", 0x1E90FF },
    { "FireBrick", 0xB22222 },
    { "FloralWhite", 0xFFFAF0 },
    { "ForestGreen", 0x228B22 },
    { "Fuchsia", 0xFF00FF },
    { "Gainsboro", 0xDCDCDC },
    { "GhostWhite", 0xF8F8FF },
    { "Gold", 0xFFD700 },
    { "GoldenRod", 0xDAA520 },
    { "Gray", 0x808080 },
    { "Grey", 0x808080 },
    { "Green", 0x008000 },
    { "GreenYellow", 0xADFF2F },
    { "HoneyDew", 0xF0FFF0 },
    { "HotPink", 0xFF69B4 },
    { "IndianRed", 0xCD5C5C },
    { "Indigo", 0x4B0082 },
    { "Ivory", 0xFFFFF0 },
    { "Khaki", 0xF0E68C },
    { "Lavender", 0xE6E6FA },
    { "LavenderBlush", 0xFFF0F5 },
    { "LawnGreen", 0x7CFC00 },
    { "LemonChiffon", 0xFFFACD },
    { "LightBlue", 0xADD8E6 },
    { "LightCoral", 0xF08080 },
    { "LightCyan", 0xE0FFFF },
    { "LightGoldenRodYellow", 0xFAFAD2 },
    { "LightGray", 0xD3D3D3 },
    { "LightGrey", 0xD3D3D3 },
    { "LightGreen", 0x90EE90 },
    { "LightPink", 0xFFB6C1 },
    { "LightSalmon", 0xFFA07A },
    { "LightSeaGreen", 0x20B2AA },
    { "LightSkyBlue", 0x87CEFA },
    { "LightSlateGray", 0x778899 },
    { "LightSlateGrey", 0x778899 },
    { "LightSteelBlue", 0xB0C4DE },
    { "LightYellow", 0xFFFFE0 },
    { "Lime", 0x00FF00 },
    { "LimeGreen", 0x32CD32 },
    { "Linen", 0xFAF0E6 },
    { "Magenta", 0xFF00FF },
    { "Maroon", 0x800000 },
    { "MediumAquaMarine", 0x66CDAA },
    { "MediumBlue", 0x0000CD },
    { "MediumOrchid", 0xBA55D3 },
    { "MediumPurple", 0x9370D8 },
    { "MediumSeaGreen", 0x3CB371 },
    { "MediumSlateBlue", 0x7B68EE },
    { "MediumSpringGreen", 0x00FA9A },
    { "MediumTurquoise", 0x48D1CC },
    { "MediumVioletRed", 0xC71585 },
    { "MidnightBlue", 0x191970 },
    { "MintCream", 0xF5FFFA },
    { "MistyRose", 0xFFE4E1 },
    { "Moccasin", 0xFFE4B5 },
    { "NavajoWhite", 0xFFDEAD },
    { "Navy", 0x000080 },
    { "OldLace", 0xFDF5E6 },
    { "Olive", 0x808000 },
    { "OliveDrab", 0x6B8E23 },
    { "Orange", 0xFFA500 },
    { "OrangeRed", 0xFF4500 },
    { "Orchid", 0xDA70D6 },
    { "PaleGoldenRod", 0xEEE8AA },
    { "PaleGreen", 0x98FB98 },
    { "PaleTurquoise", 0xAFEEEE },
    { "PaleVioletRed", 0xD87093 },
    { "PapayaWhip", 0xFFEFD5 },
    { "PeachPuff", 0xFFDAB9 },
    { "Peru", 0xCD853F },
    { "Pink", 0xFFC0CB },
    { "Plum", 0xDDA0DD },
    { "PowderBlue", 0xB0E0E6 },
    { "Purple", 0x800080 },
    { "Red", 0xFF0000 },
    { "RosyBrown", 0xBC8F8F },
    { "RoyalBlue", 0x4169E1 },
    { "SaddleBrown", 0x8B4513 },
    { "Salmon", 0xFA8072 },
    { "SandyBrown", 0xF4A460 },
    { "SeaGreen", 0x2E8B57 },
    { "SeaShell", 0xFFF5EE },
    { "Sienna", 0xA0522D },
    { "Silver", 0xC0C0C0 },
    { "SkyBlue", 0x87CEEB },
    { "SlateBlue", 0x6A5ACD },
    { "SlateGray", 0x708090 },
    { "SlateGrey", 0x708090 },
    { "Snow", 0xFFFAFA },
    { "SpringGreen", 0x00FF7F },
    { "SteelBlue", 0x4682B4 },
    { "Tan", 0xD2B48C },
    { "Teal", 0x008080 },
    { "Thistle", 0xD8BFD8 },
    { "Tomato", 0xFF6347 },
    { "Turquoise", 0x40E0D0 },
    { "Violet", 0xEE82EE },
    { "Wheat", 0xF5DEB3 },
    { "White", 0xFFFFFF },
    { "WhiteSmoke", 0xF5F5F5 },
    { "Yellow", 0xFFFF00 },
    { "YellowGreen", 0x9ACD32 },

    { NULL, 0 }
};

static int HandleFontAttributes( xml_reader_t *p_xml_reader,
                                 font_stack_t **p_fonts )
{
    int        rv;
    char      *psz_fontname = NULL;
    uint32_t   i_font_color = 0xffffff;
    int        i_font_alpha = 0;
    uint32_t   i_karaoke_bg_color = 0x00ffffff;
    int        i_font_size  = 24;

    /* Default all attributes to the top font in the stack -- in case not
     * all attributes are specified in the sub-font
     */
    if( VLC_SUCCESS == PeekFont( p_fonts,
                                 &psz_fontname,
                                 &i_font_size,
                                 &i_font_color,
                                 &i_karaoke_bg_color ))
    {
        psz_fontname = strdup( psz_fontname );
        i_font_size = i_font_size;
    }
    i_font_alpha = (i_font_color >> 24) & 0xff;
    i_font_color &= 0x00ffffff;

    const char *name, *value;
    while( (name = xml_ReaderNextAttr( p_xml_reader, &value )) != NULL )
    {
        if( !strcasecmp( "face", name ) )
        {
            free( psz_fontname );
            psz_fontname = strdup( value );
        }
        else if( !strcasecmp( "size", name ) )
        {
            if( ( *value == '+' ) || ( *value == '-' ) )
            {
                int i_value = atoi( value );

                if( ( i_value >= -5 ) && ( i_value <= 5 ) )
                    i_font_size += ( i_value * i_font_size ) / 10;
                else if( i_value < -5 )
                    i_font_size = - i_value;
                else if( i_value > 5 )
                    i_font_size = i_value;
            }
            else
                i_font_size = atoi( value );
        }
        else if( !strcasecmp( "color", name ) )
        {
            if( value[0] == '#' )
            {
                i_font_color = strtol( value + 1, NULL, 16 );
                i_font_color &= 0x00ffffff;
            }
            else
            {
                for( int i = 0; p_html_colors[i].psz_name != NULL; i++ )
                {
                    if( !strncasecmp( value, p_html_colors[i].psz_name, strlen(p_html_colors[i].psz_name) ) )
                    {
                        i_font_color = p_html_colors[i].i_value;
                        break;
                    }
                }
            }
        }
        else if( !strcasecmp( "alpha", name ) && ( value[0] == '#' ) )
        {
            i_font_alpha = strtol( value + 1, NULL, 16 );
            i_font_alpha &= 0xff;
        }
    }
    rv = PushFont( p_fonts,
                   psz_fontname,
                   i_font_size,
                   (i_font_color & 0xffffff) | ((i_font_alpha & 0xff) << 24),
                   i_karaoke_bg_color );

    free( psz_fontname );

    return rv;
}

static void SetKaraokeLen( uint32_t i_runs, uint32_t *pi_run_lengths,
                           uint32_t i_k_runs, uint32_t *pi_k_run_lengths )
{
    /* Karaoke tags _PRECEDE_ the text they specify a duration
     * for, therefore we are working out the length for the
     * previous tag, and first time through we have nothing
     */
    if( pi_k_run_lengths )
    {
        int i_chars = 0;
        uint32_t i;

        /* Work out how many characters are presently in the string
         */
        for( i = 0; i < i_runs; i++ )
            i_chars += pi_run_lengths[ i ];

        /* Subtract away those we've already allocated to other
         * karaoke tags
         */
        for( i = 0; i < i_k_runs; i++ )
            i_chars -= pi_k_run_lengths[ i ];

        pi_k_run_lengths[ i_k_runs - 1 ] = i_chars;
    }
}

static void SetupKaraoke( xml_reader_t *p_xml_reader, uint32_t *pi_k_runs,
                          uint32_t **ppi_k_run_lengths,
                          uint32_t **ppi_k_durations )
{
    const char *name, *value;

    while( (name = xml_ReaderNextAttr( p_xml_reader, &value )) != NULL )
    {
        if( !strcasecmp( "t", name ) )
        {
            if( ppi_k_durations && ppi_k_run_lengths )
            {
                (*pi_k_runs)++;

                if( *ppi_k_durations )
                {
                    *ppi_k_durations = realloc_or_free( *ppi_k_durations,
                                                        *pi_k_runs * sizeof(**ppi_k_durations) );
                }
                else if( *pi_k_runs == 1 )
                {
                    *ppi_k_durations = malloc( *pi_k_runs * sizeof(**ppi_k_durations) );
                }

                if( *ppi_k_run_lengths )
                {
                    *ppi_k_run_lengths = realloc_or_free( *ppi_k_run_lengths,
                                                          *pi_k_runs * sizeof(**ppi_k_run_lengths) );
                }
                else if( *pi_k_runs == 1 )
                {
                    *ppi_k_run_lengths = malloc( *pi_k_runs * sizeof(**ppi_k_run_lengths) );
                }
                if( *ppi_k_durations )
                    (*ppi_k_durations)[ *pi_k_runs - 1 ] = atoi( value );

                if( *ppi_k_run_lengths )
                    (*ppi_k_run_lengths)[ *pi_k_runs - 1 ] = 0;
            }
        }
    }
}

/* Turn any multiple-whitespaces into single spaces */
static void HandleWhiteSpace( char *psz_node )
{
    char *s = strpbrk( psz_node, "\t\r\n " );
    while( s )
    {
        int i_whitespace = strspn( s, "\t\r\n " );

        if( i_whitespace > 1 )
            memmove( &s[1],
                     &s[i_whitespace],
                     strlen( s ) - i_whitespace + 1 );
        *s++ = ' ';

        s = strpbrk( s, "\t\r\n " );
    }
}


static text_style_t *GetStyleFromFontStack( filter_sys_t *p_sys,
                                            font_stack_t **p_fonts,
                                            int i_style_flags )
{
    char       *psz_fontname = NULL;
    uint32_t    i_font_color = p_sys->i_font_color & 0x00ffffff;
    uint32_t    i_karaoke_bg_color = i_font_color;
    int         i_font_size  = p_sys->i_font_size;

    if( PeekFont( p_fonts, &psz_fontname, &i_font_size,
                  &i_font_color, &i_karaoke_bg_color ) )
        return NULL;

    return CreateStyle( psz_fontname, i_font_size, i_font_color,
                        i_karaoke_bg_color,
                        i_style_flags );
}

static int RenderTag( filter_t *p_filter, FT_Face p_face,
                      int i_font_color,
                      int i_style_flags,
                      int i_karaoke_bgcolor,
                      line_desc_t *p_line, uint32_t *psz_unicode,
                      int *pi_pen_x, int i_pen_y, int *pi_start,
                      FT_Vector *p_result )
{
    FT_BBox      line;
    int          i_yMin, i_yMax;
    int          i;
    bool   b_first_on_line = true;

    int          i_previous = 0;
    int          i_pen_x_start = *pi_pen_x;

    uint32_t *psz_unicode_start = psz_unicode;

    line.xMin = line.xMax = line.yMin = line.yMax = 0;

    /* Account for part of line already in position */
    for( i = 0; i<*pi_start; i++ )
    {
        FT_BBox glyph_size;

        FT_Glyph_Get_CBox( (FT_Glyph) p_line->pp_glyphs[ i ],
                            ft_glyph_bbox_pixels, &glyph_size );

        line.xMax = p_line->p_glyph_pos[ i ].x + glyph_size.xMax -
            glyph_size.xMin + p_line->pp_glyphs[ i ]->left;
        line.yMax = __MAX( line.yMax, glyph_size.yMax );
        line.yMin = __MIN( line.yMin, glyph_size.yMin );
    }
    i_yMin = line.yMin;
    i_yMax = line.yMax;

    if( line.xMax > 0 )
        b_first_on_line = false;

    while( *psz_unicode && ( *psz_unicode != '\n' ) )
    {
        FT_BBox glyph_size;
        FT_Glyph tmp_glyph;
        int i_error;

        int i_glyph_index = FT_Get_Char_Index( p_face, *psz_unicode++ );
        if( FT_HAS_KERNING( p_face ) && i_glyph_index
            && i_previous )
        {
            FT_Vector delta;
            FT_Get_Kerning( p_face, i_previous, i_glyph_index,
                            ft_kerning_default, &delta );
            *pi_pen_x += delta.x >> 6;
        }
        p_line->p_glyph_pos[ i ].x = *pi_pen_x;
        p_line->p_glyph_pos[ i ].y = i_pen_y;

        i_error = FT_Load_Glyph( p_face, i_glyph_index, FT_LOAD_NO_BITMAP | FT_LOAD_DEFAULT );
        if( i_error )
        {
            i_error = FT_Load_Glyph( p_face, i_glyph_index, FT_LOAD_DEFAULT );
            if( i_error )
            {
                msg_Err( p_filter,
                       "unable to render text FT_Load_Glyph returned %d", i_error );
                p_line->pp_glyphs[ i ] = NULL;
                return VLC_EGENERIC;
            }
        }

        /* Do synthetic styling now that Freetype supports it;
         * ie. if the font we have loaded is NOT already in the
         * style that the tags want, then switch it on; if they
         * are then don't. */
        if ((i_style_flags & STYLE_BOLD) && !(p_face->style_flags & FT_STYLE_FLAG_BOLD))
            FT_GlyphSlot_Embolden( p_face->glyph );
        if ((i_style_flags & STYLE_ITALIC) && !(p_face->style_flags & FT_STYLE_FLAG_ITALIC))
            FT_GlyphSlot_Oblique( p_face->glyph );

        i_error = FT_Get_Glyph( p_face->glyph, &tmp_glyph );
        if( i_error )
        {
            msg_Err( p_filter,
                    "unable to render text FT_Get_Glyph returned %d", i_error );
            p_line->pp_glyphs[ i ] = NULL;
            return VLC_EGENERIC;
        }
        FT_Glyph_Get_CBox( tmp_glyph, ft_glyph_bbox_pixels, &glyph_size );
        i_error = FT_Glyph_To_Bitmap( &tmp_glyph, FT_RENDER_MODE_NORMAL, 0, 1);
        if( i_error )
        {
            FT_Done_Glyph( tmp_glyph );
            continue;
        }
        if( i_style_flags & (STYLE_UNDERLINE | STYLE_STRIKEOUT) )
        {
            float aOffset = FT_FLOOR(FT_MulFix(p_face->underline_position,
                                               p_face->size->metrics.y_scale));
            float aSize = FT_CEIL(FT_MulFix(p_face->underline_thickness,
                                            p_face->size->metrics.y_scale));

            p_line->pi_underline_offset[ i ]  =
                                       ( aOffset < 0 ) ? -aOffset : aOffset;
            p_line->pi_underline_thickness[ i ] =
                                       ( aSize < 0 ) ? -aSize   : aSize;
            if (i_style_flags & STYLE_STRIKEOUT)
            {
                /* Move the baseline to make it strikethrough instead of
                 * underline. That means that strikethrough takes precedence
                 */
                float aDescent = FT_FLOOR(FT_MulFix(p_face->descender*2,
                                                    p_face->size->metrics.y_scale));

                p_line->pi_underline_offset[ i ]  -=
                                       ( aDescent < 0 ) ? -aDescent : aDescent;
            }
        }

        p_line->pp_glyphs[ i ] = (FT_BitmapGlyph)tmp_glyph;
        p_line->p_fg_rgb[ i ] = i_font_color;
        p_line->p_bg_rgb[ i ] = i_karaoke_bgcolor;
        p_line->p_fg_bg_ratio[ i ] = 0x00;

        line.xMax = p_line->p_glyph_pos[i].x + glyph_size.xMax -
                    glyph_size.xMin + ((FT_BitmapGlyph)tmp_glyph)->left;
        if( line.xMax > (int)p_filter->fmt_out.video.i_visible_width - 20 )
        {
            for( ; i >= *pi_start; i-- )
                FT_Done_Glyph( (FT_Glyph)p_line->pp_glyphs[ i ] );
            i = *pi_start;

            while( psz_unicode > psz_unicode_start && *psz_unicode != ' ' )
            {
                psz_unicode--;
            }
            if( psz_unicode == psz_unicode_start )
            {
                if( b_first_on_line )
                {
                    msg_Warn( p_filter, "unbreakable string" );
                    p_line->pp_glyphs[ i ] = NULL;
                    return VLC_EGENERIC;
                }
                *pi_pen_x = i_pen_x_start;

                p_line->i_width = line.xMax;
                p_line->i_height = __MAX( p_line->i_height,
                                          p_face->size->metrics.height >> 6 );
                p_line->pp_glyphs[ i ] = NULL;

                p_result->x = __MAX( p_result->x, line.xMax );
                p_result->y = __MAX( p_result->y, __MAX( p_line->i_height,
                                                         i_yMax - i_yMin ) );
                return VLC_SUCCESS;
            }
            else
            {
                *psz_unicode = '\n';
            }
            psz_unicode = psz_unicode_start;
            *pi_pen_x = i_pen_x_start;
            i_previous = 0;

            line.yMax = i_yMax;
            line.yMin = i_yMin;

            continue;
        }
        line.yMax = __MAX( line.yMax, glyph_size.yMax );
        line.yMin = __MIN( line.yMin, glyph_size.yMin );

        i_previous = i_glyph_index;
        *pi_pen_x += p_face->glyph->advance.x >> 6;
        i++;
    }
    p_line->i_width = line.xMax;
    p_line->i_height = __MAX( p_line->i_height,
                              p_face->size->metrics.height >> 6 );
    p_line->pp_glyphs[ i ] = NULL;

    p_result->x = __MAX( p_result->x, line.xMax );
    p_result->y = __MAX( p_result->y, __MAX( p_line->i_height,
                         line.yMax - line.yMin ) );

    *pi_start = i;

    /* Get rid of any text processed - if necessary repositioning
     * at the start of a new line of text
     */
    if( !*psz_unicode )
    {
        *psz_unicode_start = '\0';
    }
    else if( psz_unicode > psz_unicode_start )
    {
        for( i=0; psz_unicode[ i ]; i++ )
            psz_unicode_start[ i ] = psz_unicode[ i ];
        psz_unicode_start[ i ] = '\0';
    }

    return VLC_SUCCESS;
}

static void SetupLine( filter_t *p_filter, const char *psz_text_in,
                       uint32_t **ppsz_text_out, uint32_t *pi_runs,
                       uint32_t **ppi_run_lengths, text_style_t ***ppp_styles,
                       text_style_t *p_style )
{
    size_t i_string_length;

    IconvText( p_filter, psz_text_in, &i_string_length, *ppsz_text_out );
    *ppsz_text_out += i_string_length;

    if( ppp_styles && ppi_run_lengths )
    {
        (*pi_runs)++;

        /* XXX this logic looks somewhat broken */

        if( *ppp_styles )
        {
            *ppp_styles = realloc_or_free( *ppp_styles,
                                           *pi_runs * sizeof(**ppp_styles) );
        }
        else if( *pi_runs == 1 )
        {
            *ppp_styles = malloc( *pi_runs * sizeof(**ppp_styles) );
        }

        /* We have just malloc'ed this memory successfully -
         * *pi_runs HAS to be within the memory area of *ppp_styles */
        if( *ppp_styles )
        {
            (*ppp_styles)[ *pi_runs - 1 ] = p_style;
            p_style = NULL;
        }

        /* XXX more iffy logic */

        if( *ppi_run_lengths )
        {
            *ppi_run_lengths = realloc_or_free( *ppi_run_lengths,
                                                *pi_runs * sizeof(**ppi_run_lengths) );
        }
        else if( *pi_runs == 1 )
        {
            *ppi_run_lengths = malloc( *pi_runs * sizeof(**ppi_run_lengths) );
        }

        /* same remarks here */
        if( *ppi_run_lengths )
        {
            (*ppi_run_lengths)[ *pi_runs - 1 ] = i_string_length;
        }
    }
    /* If we couldn't use the p_style argument due to memory allocation
     * problems above, release it here.
     */
    text_style_Delete( p_style );
}

static int CheckForEmbeddedFont( filter_sys_t *p_sys, FT_Face *pp_face, text_style_t *p_style )
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
                if( !strcasecmp( p_face->family_name, p_style->psz_fontname ) &&
                    (p_style->i_style_flags & (STYLE_BOLD | STYLE_BOLD)) == i_style_received )
                {
                    *pp_face = p_face;
                    return VLC_SUCCESS;
                }
                FT_Done_Face( p_face );
            }
            i_font_idx++;
        }
    }
    return VLC_EGENERIC;
}

static int ProcessNodes( filter_t *p_filter,
                         xml_reader_t *p_xml_reader,
                         text_style_t *p_font_style,
                         uint32_t *psz_text,
                         int *pi_len,

                         uint32_t *pi_runs,
                         uint32_t **ppi_run_lengths,
                         text_style_t * **ppp_styles,

                         bool b_karaoke,
                         uint32_t *pi_k_runs,
                         uint32_t **ppi_k_run_lengths,
                         uint32_t **ppi_k_durations )
{
    int           rv             = VLC_SUCCESS;
    filter_sys_t *p_sys          = p_filter->p_sys;
    uint32_t        *psz_text_orig  = psz_text;
    font_stack_t *p_fonts        = NULL;

    int i_style_flags = 0;

    if( p_font_style )
    {
        rv = PushFont( &p_fonts,
               p_font_style->psz_fontname,
               p_font_style->i_font_size,
               (p_font_style->i_font_color & 0xffffff) |
                   ((p_font_style->i_font_alpha & 0xff) << 24),
               (p_font_style->i_karaoke_background_color & 0xffffff) |
                   ((p_font_style->i_karaoke_background_alpha & 0xff) << 24));

        i_style_flags = p_font_style->i_style_flags & (STYLE_BOLD |
                                                       STYLE_ITALIC |
                                                       STYLE_UNDERLINE |
                                                       STYLE_STRIKEOUT);
    }
#ifdef HAVE_STYLES
    else
    {
        rv = PushFont( &p_fonts,
                       p_sys->psz_fontfamily,
                       p_sys->i_font_size,
                       (p_sys->i_font_color & 0xffffff) |
                          (((255-p_sys->i_font_opacity) & 0xff) << 24),
                       0x00ffffff );
    }
#endif

    if( rv != VLC_SUCCESS )
        return rv;

    const char *node;
    int type;

    while ( (type = xml_ReaderNextNode( p_xml_reader, &node )) > 0 )
    {
        switch ( type )
        {
            case XML_READER_ENDELEM:
                if( !strcasecmp( "font", node ) )
                    PopFont( &p_fonts );
                else if( !strcasecmp( "b", node ) )
                    i_style_flags &= ~STYLE_BOLD;
               else if( !strcasecmp( "i", node ) )
                    i_style_flags &= ~STYLE_ITALIC;
                else if( !strcasecmp( "u", node ) )
                    i_style_flags &= ~STYLE_UNDERLINE;
                else if( !strcasecmp( "s", node ) )
                    i_style_flags &= ~STYLE_STRIKEOUT;
                break;

            case XML_READER_STARTELEM:
                if( !strcasecmp( "font", node ) )
                    rv = HandleFontAttributes( p_xml_reader, &p_fonts );
                else if( !strcasecmp( "b", node ) )
                    i_style_flags |= STYLE_BOLD;
                else if( !strcasecmp( "i", node ) )
                    i_style_flags |= STYLE_ITALIC;
                else if( !strcasecmp( "u", node ) )
                    i_style_flags |= STYLE_UNDERLINE;
                else if( !strcasecmp( "s", node ) )
                    i_style_flags |= STYLE_STRIKEOUT;
                else if( !strcasecmp( "br", node ) )
                {
                    SetupLine( p_filter, "\n", &psz_text,
                               pi_runs, ppi_run_lengths, ppp_styles,
                               GetStyleFromFontStack( p_sys,
                                                      &p_fonts,
                                                      i_style_flags ) );
                }
                else if( !strcasecmp( "k", node ) )
                {
                    /* Only valid in karaoke */
                    if( b_karaoke )
                    {
                        if( *pi_k_runs > 0 )
                            SetKaraokeLen( *pi_runs, *ppi_run_lengths,
                                           *pi_k_runs, *ppi_k_run_lengths );
                        SetupKaraoke( p_xml_reader, pi_k_runs,
                                      ppi_k_run_lengths, ppi_k_durations );
                    }
                }
                break;

            case XML_READER_TEXT:
            {
                char *psz_node = strdup( node );
                if( unlikely(!psz_node) )
                    break;

                HandleWhiteSpace( psz_node );
                resolve_xml_special_chars( psz_node );

                SetupLine( p_filter, psz_node, &psz_text,
                           pi_runs, ppi_run_lengths, ppp_styles,
                           GetStyleFromFontStack( p_sys,
                                                  &p_fonts,
                                                  i_style_flags ) );
                free( psz_node );
                break;
            }
        }
        if( rv != VLC_SUCCESS )
        {
            psz_text = psz_text_orig;
            break;
        }
    }
    if( b_karaoke )
    {
        SetKaraokeLen( *pi_runs, *ppi_run_lengths,
                       *pi_k_runs, *ppi_k_run_lengths );
    }

    *pi_len = psz_text - psz_text_orig;

    while( VLC_SUCCESS == PopFont( &p_fonts ) );

    return rv;
}

static void FreeLine( line_desc_t *p_line )
{
    for( int i = 0; p_line->pp_glyphs && p_line->pp_glyphs[i] != NULL; i++ )
        FT_Done_Glyph( (FT_Glyph)p_line->pp_glyphs[i] );

    free( p_line->pp_glyphs );
    free( p_line->p_glyph_pos );
    free( p_line->p_fg_rgb );
    free( p_line->p_bg_rgb );
    free( p_line->p_fg_bg_ratio );
    free( p_line->pi_underline_offset );
    free( p_line->pi_underline_thickness );
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

    p_line->i_width = 0;
    p_line->i_height = 0;
    p_line->i_alpha = 0xff;

    p_line->p_next = NULL;

    p_line->pp_glyphs              = calloc( i_count + 1, sizeof(*p_line->pp_glyphs) );
    p_line->p_glyph_pos            = calloc( i_count + 1, sizeof(*p_line->p_glyph_pos) );
    p_line->p_fg_rgb               = calloc( i_count + 1, sizeof(*p_line->p_fg_rgb) );
    p_line->p_bg_rgb               = calloc( i_count + 1, sizeof(*p_line->p_bg_rgb) );
    p_line->p_fg_bg_ratio          = calloc( i_count + 1, sizeof(*p_line->p_fg_bg_ratio) );
    p_line->pi_underline_offset    = calloc( i_count + 1, sizeof(*p_line->pi_underline_offset) );
    p_line->pi_underline_thickness = calloc( i_count + 1, sizeof(*p_line->pi_underline_thickness) );

    if( !p_line->pp_glyphs || !p_line->p_glyph_pos ||
        !p_line->p_fg_rgb || !p_line->p_bg_rgb || !p_line->p_fg_bg_ratio ||
        !p_line->pi_underline_offset || !p_line->pi_underline_thickness )
    {
        FreeLine( p_line );
        return NULL;
    }
    p_line->pp_glyphs[0] = NULL;
    return p_line;
}


static int ProcessLines( filter_t *p_filter,
                         uint32_t *psz_text,
                         int i_len,

                         uint32_t i_runs,
                         uint32_t *pi_run_lengths,
                         text_style_t **pp_styles,
                         line_desc_t **pp_lines,

                         FT_Vector *p_result,

                         bool b_karaoke,
                         uint32_t i_k_runs,
                         uint32_t *pi_k_run_lengths,
                         uint32_t *pi_k_durations )
{
    filter_sys_t   *p_sys = p_filter->p_sys;
    text_style_t   **pp_char_styles;
    int            *p_new_positions = NULL;
    int8_t         *p_levels = NULL;
    uint8_t        *pi_karaoke_bar = NULL;
    uint32_t        i, j, k;
    int             i_prev;

    /* Assign each character in the text string its style explicitly, so that
     * after the characters have been shuffled around by Fribidi, we can re-apply
     * the styles, and to simplify the calculation of runs within a line.
     */
    pp_char_styles = malloc( i_len * sizeof(*pp_char_styles));
    if( !pp_char_styles )
        return VLC_ENOMEM;

    if( b_karaoke )
    {
        pi_karaoke_bar = malloc( i_len * sizeof(*pi_karaoke_bar));
        /* If we can't allocate sufficient memory for karaoke, continue anyway -
         * we just won't be able to display the progress bar; at least we'll
         * get the text.
         */
    }

    i = 0;
    for( j = 0; j < i_runs; j++ )
        for( k = 0; k < pi_run_lengths[ j ]; k++ )
            pp_char_styles[ i++ ] = pp_styles[ j ];

#if defined(HAVE_FRIBIDI)
    {
        text_style_t **pp_char_styles_new;
        int         *p_old_positions;
        uint32_t    *p_fribidi_string;
        int start_pos, pos = 0;

        pp_char_styles_new = malloc( i_len * sizeof(*pp_char_styles_new));

        p_fribidi_string   = malloc( (i_len + 1) * sizeof(*p_fribidi_string) );
        p_old_positions    = malloc( (i_len + 1) * sizeof(*p_old_positions) );
        p_new_positions    = malloc( (i_len + 1) * sizeof(*p_new_positions) );
        p_levels           = malloc( (i_len + 1) * sizeof(*p_levels) );

        if( ! pp_char_styles_new ||
            ! p_fribidi_string ||
            ! p_old_positions ||
            ! p_new_positions ||
            ! p_levels )
        {
            free( p_levels );
            free( p_old_positions );
            free( p_new_positions );
            free( p_fribidi_string );
            free( pp_char_styles_new );
            free( pi_karaoke_bar );

            free( pp_char_styles );
            return VLC_ENOMEM;
        }

        /* Do bidi conversion line-by-line */
        while(pos < i_len)
        {
            while(pos < i_len) {
                if (psz_text[pos] != '\n')
                    break;
                p_fribidi_string[pos] = psz_text[pos];
                pp_char_styles_new[pos] = pp_char_styles[pos];
                p_new_positions[pos] = pos;
                p_levels[pos] = 0;
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
                        p_levels + start_pos );
                for( j = (uint32_t) start_pos; j < (uint32_t) pos; j++ )
                {
                    pp_char_styles_new[ j ] = pp_char_styles[ start_pos +
                                                p_old_positions[ j - start_pos ] ];
                    p_new_positions[ j ] += start_pos;
                }
            }
        }
        free( p_old_positions );
        free( pp_char_styles );
        pp_char_styles = pp_char_styles_new;
        psz_text = p_fribidi_string;
        p_fribidi_string[ i_len ] = 0;
    }
#endif
    /* Work out the karaoke */
    if( pi_karaoke_bar )
    {
        int64_t i_last_duration = 0;
        int64_t i_duration = 0;
        int64_t i_start_pos = 0;
        int64_t i_elapsed  = var_GetTime( p_filter, "spu-elapsed" ) / 1000;

        for( k = 0; k< i_k_runs; k++ )
        {
             double fraction = 0.0;

             i_duration += pi_k_durations[ k ];

             if( i_duration < i_elapsed )
             {
                 /* Completely finished this run-length -
                  * let it render normally */

                 fraction = 1.0;
             }
             else if( i_elapsed < i_last_duration )
             {
                 /* Haven't got up to this segment yet -
                  * render it completely in karaoke BG mode */

                 fraction = 0.0;
             }
             else
             {
                 /* Partway through this run */

                 fraction = (double)(i_elapsed - i_last_duration) /
                            (double)pi_k_durations[ k ];
             }
             for( i = 0; i < pi_k_run_lengths[ k ]; i++ )
             {
                 double shade = pi_k_run_lengths[ k ] * fraction;

                 if( p_new_positions )
                     j = p_new_positions[ i_start_pos + i ];
                 else
                     j = i_start_pos + i;

                 if( i < (uint32_t)shade )
                     pi_karaoke_bar[ j ] = 0xff;
                 else if( (double)i > shade )
                     pi_karaoke_bar[ j ] = 0x00;
                 else
                 {
                     shade -= (int)shade;
                     pi_karaoke_bar[ j ] = ((int)(shade * 128.0) & 0x7f) |
                                   ((p_levels ? (p_levels[ j ] % 2) : 0 ) << 7);
                 }
             }

             i_last_duration = i_duration;
             i_start_pos += pi_k_run_lengths[ k ];
        }
    }
    free( p_levels );
    free( p_new_positions );

    FT_Vector tmp_result;

    line_desc_t *p_line = NULL;
    line_desc_t *p_prev = NULL;

    int i_pen_x = 0;
    int i_pen_y = 0;
    int i_posn  = 0;

    p_result->x = p_result->y = 0;
    tmp_result.x = tmp_result.y = 0;

    i_prev = 0;
    for( k = 0; k <= (uint32_t) i_len; k++ )
    {
        if( ( k == (uint32_t) i_len ) ||
          ( ( k > 0 ) &&
            !StyleEquals( pp_char_styles[ k ], pp_char_styles[ k - 1] ) ) )
        {
            text_style_t *p_style = pp_char_styles[ k - 1 ];

            /* End of the current style run */
            FT_Face p_face = NULL;
            int      i_idx = 0;

            /* Look for a match amongst our attachments first */
            CheckForEmbeddedFont( p_sys, &p_face, p_style );

            if( ! p_face )
            {
                char *psz_fontfile;

#ifdef HAVE_FONTCONFIG
                psz_fontfile = FontConfig_Select( NULL,
                                                  p_style->psz_fontname,
                                                  (p_style->i_style_flags & STYLE_BOLD) != 0,
                                                  (p_style->i_style_flags & STYLE_ITALIC) != 0,
                                                  -1,
                                                  &i_idx );
#elif defined( WIN32 )
                psz_fontfile = Win32_Select( p_filter,
                                            p_style->psz_fontname,
                                            (p_style->i_style_flags & STYLE_BOLD) != 0,
                                            (p_style->i_style_flags & STYLE_ITALIC) != 0,
                                            -1,
                                            &i_idx );
#else
                psz_fontfile = NULL;
#endif
                if( psz_fontfile && ! *psz_fontfile )
                {
                    msg_Warn( p_filter,
                              "We were not able to find a matching font: \"%s\" (%s %s),"
                              " so using default font",
                              p_style->psz_fontname,
                              (p_style->i_style_flags & STYLE_BOLD)   ? "Bold" : "",
                              (p_style->i_style_flags & STYLE_ITALIC) ? "Italic" : "" );
                    free( psz_fontfile );
                    psz_fontfile = NULL;
                }

                if( psz_fontfile )
                {
                    if( FT_New_Face( p_sys->p_library,
                                psz_fontfile, i_idx, &p_face ) )
                    {
                        free( psz_fontfile );
                        free( pp_char_styles );
#if defined(HAVE_FRIBIDI)
                        free( psz_text );
#endif
                        free( pi_karaoke_bar );
                        return VLC_EGENERIC;
                    }
                    free( psz_fontfile );
                }
            }
            if( p_face &&
                FT_Select_Charmap( p_face, ft_encoding_unicode ) )
            {
                /* We've loaded a font face which is unhelpful for actually
                 * rendering text - fallback to the default one.
                 */
                 FT_Done_Face( p_face );
                 p_face = NULL;
            }

            if( FT_Select_Charmap( p_face ? p_face : p_sys->p_face,
                        ft_encoding_unicode ) ||
                FT_Set_Pixel_Sizes( p_face ? p_face : p_sys->p_face, 0,
                    p_style->i_font_size ) )
            {
                if( p_face ) FT_Done_Face( p_face );
                free( pp_char_styles );
#if defined(HAVE_FRIBIDI)
                free( psz_text );
#endif
                free( pi_karaoke_bar );
                return VLC_EGENERIC;
            }
            p_sys->i_use_kerning =
                        FT_HAS_KERNING( ( p_face  ? p_face : p_sys->p_face ) );


            uint32_t *psz_unicode = malloc( (k - i_prev + 1) * sizeof(*psz_unicode) );
            if( !psz_unicode )
            {
                if( p_face ) FT_Done_Face( p_face );
                free( pp_char_styles );
                free( psz_unicode );
#if defined(HAVE_FRIBIDI)
                free( psz_text );
#endif
                free( pi_karaoke_bar );
                return VLC_ENOMEM;
            }
            memcpy( psz_unicode, psz_text + i_prev,
                                        sizeof( uint32_t ) * ( k - i_prev ) );
            psz_unicode[ k - i_prev ] = 0;
            while( *psz_unicode )
            {
                if( !p_line )
                {
                    if( !(p_line = NewLine( i_len - i_prev)) )
                    {
                        if( p_face ) FT_Done_Face( p_face );
                        free( pp_char_styles );
                        free( psz_unicode );
#if defined(HAVE_FRIBIDI)
                        free( psz_text );
#endif
                        free( pi_karaoke_bar );
                        return VLC_ENOMEM;
                    }
                    p_line->i_alpha = p_style->i_font_alpha & 0xff;
                    i_pen_x = 0;
                    i_pen_y += tmp_result.y;
                    tmp_result.x = 0;
                    tmp_result.y = 0;
                    i_posn = 0;
                    if( p_prev ) p_prev->p_next = p_line;
                    else *pp_lines = p_line;
                }

                if( RenderTag( p_filter, p_face ? p_face : p_sys->p_face,
                               p_style->i_font_color,
                               p_style->i_style_flags,
                               p_style->i_karaoke_background_color,
                               p_line, psz_unicode, &i_pen_x, i_pen_y, &i_posn,
                               &tmp_result ) != VLC_SUCCESS )
                {
                    if( p_face ) FT_Done_Face( p_face );
                    free( pp_char_styles );
                    free( psz_unicode );
#if defined(HAVE_FRIBIDI)
                    free( psz_text );
#endif
                    free( pi_karaoke_bar );
                    return VLC_EGENERIC;
                }

                if( *psz_unicode )
                {
                    p_result->x = __MAX( p_result->x, tmp_result.x );
                    p_result->y += tmp_result.y;

                    p_prev = p_line;
                    p_line = NULL;

                    if( *psz_unicode == '\n')
                    {
                        uint32_t *c_ptr;

                        for( c_ptr = psz_unicode; *c_ptr; c_ptr++ )
                        {
                            *c_ptr = *(c_ptr+1);
                        }
                    }
                }
            }
            free( psz_unicode );
            if( p_face ) FT_Done_Face( p_face );
            i_prev = k;
        }
    }
    free( pp_char_styles );
#if defined(HAVE_FRIBIDI)
    free( psz_text );
#endif
    if( p_line )
    {
        p_result->x = __MAX( p_result->x, tmp_result.x );
        p_result->y += tmp_result.y;
    }

    if( pi_karaoke_bar )
    {
        int i = 0;
        for( p_line = *pp_lines; p_line; p_line=p_line->p_next )
        {
            for( k = 0; p_line->pp_glyphs[ k ]; k++, i++ )
            {
                if( (pi_karaoke_bar[ i ] & 0x7f) == 0x7f)
                {
                    /* do nothing */
                }
                else if( (pi_karaoke_bar[ i ] & 0x7f) == 0x00)
                {
                    /* 100% BG colour will render faster if we
                     * instead make it 100% FG colour, so leave
                     * the ratio alone and copy the value across
                     */
                    p_line->p_fg_rgb[ k ] = p_line->p_bg_rgb[ k ];
                }
                else
                {
                    if( pi_karaoke_bar[ i ] & 0x80 )
                    {
                        /* Swap Left and Right sides over for Right aligned
                         * language text (eg. Arabic, Hebrew)
                         */
                        uint32_t i_tmp = p_line->p_fg_rgb[ k ];

                        p_line->p_fg_rgb[ k ] = p_line->p_bg_rgb[ k ];
                        p_line->p_bg_rgb[ k ] = i_tmp;
                    }
                    p_line->p_fg_bg_ratio[ k ] = (pi_karaoke_bar[ i ] & 0x7f);
                }
            }
            /* Jump over the '\n' at the line-end */
            i++;
        }
        free( pi_karaoke_bar );
    }

    return VLC_SUCCESS;
}

/**
 * This function renders a text subpicture region into another one.
 * It also calculates the size needed for this string, and renders the
 * needed glyphs into memory. It is used as pf_add_string callback in
 * the vout method by this module
 */
static int RenderCommon( filter_t *p_filter, subpicture_region_t *p_region_out,
                         subpicture_region_t *p_region_in, bool b_html )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_region_in )
        return VLC_EGENERIC;
    if( b_html && !p_region_in->psz_html )
        return VLC_EGENERIC;
    if( !b_html && !p_region_in->psz_text )
        return VLC_EGENERIC;

    uint32_t *psz_text = calloc( strlen( b_html ? p_region_in->psz_html
                                                : p_region_in->psz_text ),
                                 sizeof( *psz_text ) );
    if( !psz_text )
        return VLC_EGENERIC;


    /* Reset the default fontsize in case screen metrics have changed */
    p_filter->p_sys->i_font_size = GetFontSize( p_filter );

    /* */
    int rv = VLC_SUCCESS;
    int i_text_length = 0;
    FT_Vector result = {0, 0};
    line_desc_t *p_lines = NULL;

#ifdef HAVE_STYLES
    if( b_html )
    {
        stream_t *p_sub = stream_MemoryNew( VLC_OBJECT(p_filter),
                                            (uint8_t *) p_region_in->psz_html,
                                            strlen( p_region_in->psz_html ),
                                            true );
        if( unlikely(p_sub == NULL) )
            return VLC_SUCCESS;

        xml_reader_t *p_xml_reader = p_filter->p_sys->p_xml;
        if( !p_xml_reader )
            p_xml_reader = xml_ReaderCreate( p_filter, p_sub );
        else
            p_xml_reader = xml_ReaderReset( p_xml_reader, p_sub );
        p_filter->p_sys->p_xml = p_xml_reader;

        if( !p_xml_reader )
            rv = VLC_EGENERIC;

        bool b_karaoke = false;
        if( !rv )
        {
            /* Look for Root Node */
            const char *node;

            if( xml_ReaderNextNode( p_xml_reader, &node ) == XML_READER_STARTELEM )
            {
                if( !strcasecmp( "karaoke", node ) )
                {
                    /* We're going to have to render the text a number
                     * of times to show the progress marker on the text.
                     */
                    var_SetBool( p_filter, "text-rerender", true );
                    b_karaoke = true;
                }
                else if( !strcasecmp( "text", node ) )
                {
                    b_karaoke = false;
                }
                else
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
            uint32_t    i_runs           = 0;
            uint32_t    i_k_runs         = 0;
            uint32_t   *pi_run_lengths   = NULL;
            uint32_t   *pi_k_run_lengths = NULL;
            uint32_t   *pi_k_durations   = NULL;
            text_style_t **pp_styles     = NULL;

            rv = ProcessNodes( p_filter, p_xml_reader,
                               p_region_in->p_style, psz_text, &i_text_length,
                               &i_runs, &pi_run_lengths, &pp_styles,
                               b_karaoke, &i_k_runs, &pi_k_run_lengths,
                               &pi_k_durations );

            if( !rv && i_text_length > 0 )
            {
                rv = ProcessLines( p_filter, psz_text, i_text_length, i_runs,
                                   pi_run_lengths, pp_styles, &p_lines,
                                   &result, b_karaoke, i_k_runs,
                                   pi_k_run_lengths, pi_k_durations );
            }

            for( uint32_t k = 0; k < i_runs; k++ )
                 text_style_Delete( pp_styles[k] );
            free( pp_styles );
            free( pi_run_lengths );

        }

        if( p_xml_reader )
            p_filter->p_sys->p_xml = xml_ReaderReset( p_xml_reader, NULL );

        stream_Delete( p_sub );
    }
    else
#endif
    {

        size_t i_iconv_length;
        IconvText( p_filter, p_region_in->psz_text, &i_iconv_length, psz_text );
        i_text_length = i_iconv_length;

        text_style_t *p_style;
        if( p_region_in->p_style )
            p_style = CreateStyle( p_region_in->p_style->psz_fontname,
                                   p_region_in->p_style->i_font_size,
                                   (p_region_in->p_style->i_font_color & 0xffffff) |
                                   ((p_region_in->p_style->i_font_alpha & 0xff) << 24),
                                   0x00ffffff,
                                   p_region_in->p_style->i_style_flags & (STYLE_BOLD |
                                                                          STYLE_ITALIC |
                                                                          STYLE_UNDERLINE |
                                                                          STYLE_STRIKEOUT) );
        else
            p_style = CreateStyle( p_sys->psz_fontfamily,
                                   p_sys->i_font_size,
                                   (p_sys->i_font_color & 0xffffff) |
                                   (((255-p_sys->i_font_opacity) & 0xff) << 24),
                                   0x00ffffff, 0);
        uint32_t i_run_length = i_text_length;

        rv = ProcessLines( p_filter, psz_text, i_text_length,
                           1, &i_run_length, &p_style,
                           &p_lines, &result,
                           false, 0, NULL, NULL );
        text_style_Delete( p_style );
    }
    free( psz_text );

    p_region_out->i_x = p_region_in->i_x;
    p_region_out->i_y = p_region_in->i_y;

    /* Don't attempt to render text that couldn't be layed out
     * properly. */
    if( !rv && i_text_length > 0 )
    {
        if( var_InheritBool( p_filter, "freetype-yuvp" ) )
            RenderYUVP( p_filter, p_region_out, p_lines,
                        result.x, result.y );
        else
            RenderYUVA( p_filter, p_region_out, p_lines,
                        result.x, result.y );
    }

    FreeLines( p_lines );
    return rv;
}

static int RenderText( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in )
{
    return RenderCommon( p_filter, p_region_out, p_region_in, false );
}

#ifdef HAVE_STYLES

static int RenderHtml( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in )
{
    return RenderCommon( p_filter, p_region_out, p_region_in, true );
}

#endif

/*****************************************************************************
 * Create: allocates osd-text video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t      *p_filter = (filter_t *)p_this;
    filter_sys_t  *p_sys;
    char          *psz_fontfile   = NULL;
    char          *psz_fontfamily = NULL;
    int            i_error = 0, fontindex = 0;

    /* Allocate structure */
    p_filter->p_sys = p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->psz_fontfamily   = NULL;
#ifdef HAVE_STYLES
    p_sys->p_xml            = NULL;
#endif
    p_sys->p_face           = 0;
    p_sys->p_library        = 0;
    p_sys->i_font_size      = 0;
    p_sys->i_display_height = 0;

    var_Create( p_filter, "freetype-rel-fontsize",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    psz_fontfamily = var_InheritString( p_filter, "freetype-font" );
    p_sys->i_default_font_size = var_InheritInteger( p_filter, "freetype-fontsize" );
    p_sys->i_effect = var_InheritInteger( p_filter, "freetype-effect" );
    p_sys->i_font_opacity = var_InheritInteger( p_filter,"freetype-opacity" );
    p_sys->i_font_opacity = __MAX( __MIN( p_sys->i_font_opacity, 255 ), 0 );
    p_sys->i_font_color = var_InheritInteger( p_filter, "freetype-color" );
    p_sys->i_font_color = __MAX( __MIN( p_sys->i_font_color , 0xFFFFFF ), 0 );

#ifdef WIN32
    /* Get Windows Font folder */
    wchar_t wdir[MAX_PATH];
    if( S_OK != SHGetFolderPathW( NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, wdir ) )
    {
        GetWindowsDirectoryW( wdir, MAX_PATH );
        wcscat( wdir, L"\\fonts" );
    }
    p_sys->psz_win_fonts_path = FromWide( wdir );
#endif

    /* Set default psz_fontfamily */
    if( !psz_fontfamily || !*psz_fontfamily )
    {
        free( psz_fontfamily );
#ifdef HAVE_STYLES
        psz_fontfamily = strdup( DEFAULT_FAMILY );
#else
# ifdef WIN32
        if( asprintf( &psz_fontfamily, "%s"DEFAULT_FONT_FILE, p_sys->psz_win_fonts_path ) == -1 )
            goto error;
# else
        psz_fontfamily = strdup( DEFAULT_FONT_FILE );
# endif
        msg_Err( p_filter,"User specified an empty fontfile, using %s", psz_fontfamily );
#endif
    }

    /* Set the current font file */
    p_sys->psz_fontfamily = psz_fontfamily;
#ifdef HAVE_STYLES
#ifdef HAVE_FONTCONFIG
    FontConfig_BuildCache( p_filter );

    /* */
    psz_fontfile = FontConfig_Select( NULL, psz_fontfamily, false, false,
                                      p_sys->i_default_font_size, &fontindex );
#elif defined(WIN32)
    psz_fontfile = Win32_Select( p_filter, psz_fontfamily, false, false,
                                 p_sys->i_default_font_size, &fontindex );

#endif
    msg_Dbg( p_filter, "Using %s as font from file %s", psz_fontfamily, psz_fontfile );

    /* If nothing is found, use the default family */
    if( !psz_fontfile )
        psz_fontfile = strdup( psz_fontfamily );

#else /* !HAVE_STYLES */
    /* Use the default file */
    psz_fontfile = psz_fontfamily;
#endif

    /* */
    i_error = FT_Init_FreeType( &p_sys->p_library );
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
#ifdef HAVE_STYLES
    free( psz_fontfile );
#endif

    i_error = FT_Select_Charmap( p_sys->p_face, ft_encoding_unicode );
    if( i_error )
    {
        msg_Err( p_filter, "font has no unicode translation table" );
        goto error;
    }

    p_sys->i_use_kerning = FT_HAS_KERNING( p_sys->p_face );

    if( SetFontSize( p_filter, 0 ) != VLC_SUCCESS ) goto error;


    p_sys->pp_font_attachments = NULL;
    p_sys->i_font_attachments = 0;

    p_filter->pf_render_text = RenderText;
#ifdef HAVE_STYLES
    p_filter->pf_render_html = RenderHtml;
#else
    p_filter->pf_render_html = NULL;
#endif

    LoadFontsFromAttachments( p_filter );

    return VLC_SUCCESS;

error:
    if( p_sys->p_face ) FT_Done_Face( p_sys->p_face );
    if( p_sys->p_library ) FT_Done_FreeType( p_sys->p_library );
#ifdef HAVE_STYLES
    free( psz_fontfile );
#endif
    free( psz_fontfamily );
    free( p_sys );
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

    if( p_sys->pp_font_attachments )
    {
        for( int k = 0; k < p_sys->i_font_attachments; k++ )
            vlc_input_attachment_Delete( p_sys->pp_font_attachments[k] );

        free( p_sys->pp_font_attachments );
    }

#ifdef HAVE_STYLES
    if( p_sys->p_xml ) xml_ReaderDelete( p_sys->p_xml );
#endif
    free( p_sys->psz_fontfamily );

    /* FcFini asserts calling the subfunction FcCacheFini()
     * even if no other library functions have been made since FcInit(),
     * so don't call it. */

    FT_Done_Face( p_sys->p_face );
    FT_Done_FreeType( p_sys->p_library );
    free( p_sys );
}


