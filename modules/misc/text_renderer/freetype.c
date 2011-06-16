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

#define BG_OPACITY_TEXT N_("Background opacity")
#define BG_COLOR_TEXT N_("Background color")

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

    add_integer_with_range( "freetype-background-opacity", 0, 0, 255,
                            BG_OPACITY_TEXT, "", false )
        change_safe()
    add_integer( "freetype-background-color", 0x00000000, BG_COLOR_TEXT,
                 "", false )
        change_integer_list( pi_color_values, ppsz_color_descriptions )
        change_safe()

    add_integer( "freetype-rel-fontsize", 16, FONTSIZER_TEXT,
                 FONTSIZER_LONGTEXT, false )
        change_integer_list( pi_sizes, ppsz_sizes_text )
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
    FT_Vector      pos;                 /* Relative position */
    uint32_t       i_color;             /* ARGB color */
    int            i_line_offset;       /* underline/strikethrough offset */
    uint16_t       i_line_thickness;    /* underline/strikethrough thickness */
} line_character_t;

typedef struct line_desc_t line_desc_t;
struct line_desc_t
{
    line_desc_t    *p_next;

    int              i_width;

    int              i_character_count;
    line_character_t *p_character;
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
    uint8_t        i_font_opacity;
    int            i_font_color;
    int            i_font_size;

    uint8_t        i_background_opacity;
    int            i_background_color;

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
    int i_alpha = 0xff - ((p_line->p_character[0].i_color >> 24) & 0xff);
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
        int i_glyph_tmax = 0;
        int i_bitmap_offset, i_offset, i_align_offset = 0;
        for( i = 0; i < p_line->i_character_count; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->p_character[i].p_glyph;
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

        for( i = 0; i < p_line->i_character_count; i++ )
        {
            const line_character_t *ch = &p_line->p_character[i];
            FT_BitmapGlyph p_glyph = ch->p_glyph;

            i_offset = ( ch->pos.y +
                i_glyph_tmax - p_glyph->top + 2 ) *
                i_pitch + ch->pos.x + p_glyph->left + 2 +
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

/*****************************************************************************
 * RenderYUVA: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
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

static inline void BlendYUVAGlyph( picture_t *p_picture,
                                   int i_picture_x, int i_picture_y,
                                   int i_a, int i_y, int i_u, int i_v,
                                   FT_BitmapGlyph p_glyph )
{
    for( int dy = 0; dy < p_glyph->bitmap.rows; dy++ )
    {
        for( int dx = 0; dx < p_glyph->bitmap.width; dx++ )
            BlendYUVAPixel( p_picture, i_picture_x + dx, i_picture_y + dy,
                            i_a, i_y, i_u, i_v,
                            p_glyph->bitmap.buffer[dy * p_glyph->bitmap.width + dx] );
    }
}

static inline void BlendYUVALine( picture_t *p_picture,
                                  int i_picture_x, int i_picture_y,
                                  int i_a, int i_y, int i_u, int i_v,
                                  const line_character_t *p_current,
                                  const line_character_t *p_next )
{
    int i_line_width = p_current->p_glyph->bitmap.width;
    if( p_next && p_next->i_line_thickness > 0 )
        i_line_width = (p_next->pos.x    + p_next->p_glyph->left) -
                       (p_current->pos.x + p_current->p_glyph->left);

    for( int dx = 0; dx < i_line_width; dx++ )
    {
        /* break the underline around the tails of any glyphs which cross it
           Strikethrough doesn't get broken */
        bool b_ok = true;
        for( int z = dx - p_current->i_line_thickness;
             z < dx + p_current->i_line_thickness && b_ok && p_current->i_line_offset >= 0;
             z++ )
        {
            FT_BitmapGlyph p_glyph_check = NULL;
            int i_column;
            if( p_next && z >= i_line_width )
            {
                i_column      = z - i_line_width;
                p_glyph_check = p_next->p_glyph;
            }
            else if( z >= 0 && z < p_current->p_glyph->bitmap.width )
            {
                i_column      = z;
                p_glyph_check = p_current->p_glyph;
            }
            if( p_glyph_check )
            {
                const FT_Bitmap *p_bitmap = &p_glyph_check->bitmap;
                for( int dy = 0; dy < p_current->i_line_thickness && b_ok; dy++ )
                {
                    int i_row = p_current->i_line_offset + p_glyph_check->top + dy;
                    b_ok = i_row >= p_bitmap->rows ||
                           p_bitmap->buffer[p_bitmap->width * i_row + i_column] == 0;
                }
            }
        }

        for( int dy = 0; dy < p_current->i_line_thickness && b_ok; dy++ )
            BlendYUVAPixel( p_picture,
                            i_picture_x + dx,
                            i_picture_y + p_current->i_line_offset + dy,
                            i_a, i_y, i_u, i_v, 0xff );
    }
}

static int RenderYUVA( filter_t *p_filter,
                       subpicture_region_t *p_region,
                       line_desc_t *p_line_head,
                       int i_width, int i_height )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Create a new subpicture region */
    video_format_t fmt;
    video_format_Init( &fmt, VLC_CODEC_YUVA );
    fmt.i_width          =
    fmt.i_visible_width  = i_width;
    fmt.i_height         =
    fmt.i_visible_height = i_height;

    picture_t *p_picture = p_region->p_picture = picture_NewFromFormat( &fmt );
    if( !p_region->p_picture )
        return VLC_EGENERIC;
    p_region->fmt = fmt;

    /* Initialize the picture background */
    uint8_t i_a = p_sys->i_background_opacity;
    uint8_t i_y, i_u, i_v;
    YUVFromRGB( p_sys->i_background_color, &i_y, &i_u, &i_v );

    memset( p_picture->p[0].p_pixels, i_y,
            p_picture->p[0].i_pitch * p_picture->p[0].i_lines );
    memset( p_picture->p[1].p_pixels, i_u,
            p_picture->p[1].i_pitch * p_picture->p[1].i_lines );
    memset( p_picture->p[2].p_pixels, i_v,
            p_picture->p[2].i_pitch * p_picture->p[2].i_lines );
    memset( p_picture->p[3].p_pixels, i_a,
            p_picture->p[3].i_pitch * p_picture->p[3].i_lines );

    /* Render all lines */
    for( line_desc_t *p_line = p_line_head; p_line != NULL; p_line = p_line->p_next )
    {
        /* Left offset to take into account alignment */
        int i_align_left = 0;
        if( p_line->i_width < i_width )
        {
            if( (p_region->i_align & 0x3) == SUBPICTURE_ALIGN_RIGHT )
                i_align_left = i_width - p_line->i_width;
            else if( (p_region->i_align & 0x3) != SUBPICTURE_ALIGN_LEFT )
                i_align_left = ( i_width - p_line->i_width ) / 2;
        }

        /* Compute the top alignment
         * FIXME seems bad (it seems that the glyphs are aligned too high) */
        int i_align_top = 0;
        for( int i = 0; i < p_line->i_character_count; i++ )
            i_align_top = __MAX( i_align_top, p_line->p_character[i].p_glyph->top );

        /* Render all glyphs and underline/strikethrough */
        for( int i = 0; i < p_line->i_character_count; i++ )
        {
            const line_character_t *ch = &p_line->p_character[i];
            FT_BitmapGlyph p_glyph = ch->p_glyph;

            i_a = 0xff - ((ch->i_color >> 24) & 0xff);
            YUVFromRGB( ch->i_color, &i_y, &i_u, &i_v );

            int i_picture_y = ch->pos.y + i_align_top;
            int i_picture_x = ch->pos.x + i_align_left + p_glyph->left;

            BlendYUVAGlyph( p_picture, i_picture_x, i_picture_y - p_glyph->top,
                            i_a, i_y, i_u, i_v,
                            p_glyph );

            if( ch->i_line_thickness > 0 )
                BlendYUVALine( p_picture, i_picture_x, i_picture_y,
                               i_a, i_y, i_u, i_v,
                               &ch[0],
                               i + 1 < p_line->i_character_count ? &ch[1] : NULL );
        }
    }

    return VLC_SUCCESS;
}

static text_style_t *CreateStyle( char *psz_fontname, int i_font_size,
                                  uint32_t i_font_color, uint32_t i_karaoke_bg_color,
                                  int i_style_flags )
{
    text_style_t *p_style = text_style_New();
    if( !p_style )
        return NULL;

    p_style->psz_fontname = psz_fontname ? strdup( psz_fontname ) : NULL;
    p_style->i_font_size  = i_font_size;
    p_style->i_font_color = (i_font_color & 0x00ffffff) >>  0;
    p_style->i_font_alpha = (i_font_color & 0xff000000) >> 24;
    p_style->i_karaoke_background_color = (i_karaoke_bg_color & 0x00ffffff) >>  0;
    p_style->i_karaoke_background_alpha = (i_karaoke_bg_color & 0xff000000) >> 24;
    p_style->i_style_flags |= i_style_flags;
    return p_style;
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

static unsigned SetupText( filter_t *p_filter,
                           uint32_t *psz_text_out,
                           text_style_t **pp_styles,
                           uint32_t *pi_k_dates,

                           const char *psz_text_in,
                           text_style_t *p_style,
                           uint32_t i_k_date )
{
    size_t i_string_length;

    size_t i_string_bytes;
#if defined(WORDS_BIGENDIAN)
    uint32_t *psz_tmp = ToCharset( "UCS-4BE", psz_text_in, &i_string_bytes );
#else
    uint32_t *psz_tmp = ToCharset( "UCS-4LE", psz_text_in, &i_string_bytes );
#endif
    if( psz_tmp )
    {
        memcpy( psz_text_out, psz_tmp, i_string_bytes );
        i_string_length = i_string_bytes / 4;
        free( psz_tmp );
    }
    else
    {
        msg_Warn( p_filter, "failed to convert string to unicode (%m)" );
        i_string_length = 0;
    }

    if( i_string_length > 0 )
    {
        for( unsigned i = 0; i < i_string_length; i++ )
            pp_styles[i] = p_style;
    }
    else
    {
        text_style_Delete( p_style );
    }
    if( i_string_length > 0 && pi_k_dates )
    {
        for( unsigned i = 0; i < i_string_length; i++ )
            pi_k_dates[i] = i_k_date;
    }
    return i_string_length;
}

static int ProcessNodes( filter_t *p_filter,
                         uint32_t *psz_text,
                         text_style_t **pp_styles,
                         uint32_t *pi_k_dates,
                         int *pi_len,
                         xml_reader_t *p_xml_reader,
                         text_style_t *p_font_style )
{
    int           rv      = VLC_SUCCESS;
    filter_sys_t *p_sys   = p_filter->p_sys;
    int i_text_length     = 0;
    font_stack_t *p_fonts = NULL;
    uint32_t i_k_date     = 0;

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
                    HandleFontAttributes( p_xml_reader, &p_fonts );
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
                    i_text_length += SetupText( p_filter,
                                                &psz_text[i_text_length],
                                                &pp_styles[i_text_length],
                                                pi_k_dates ? &pi_k_dates[i_text_length] : NULL,
                                                "\n",
                                                GetStyleFromFontStack( p_sys,
                                                                       &p_fonts,
                                                                       i_style_flags ),
                                                i_k_date );
                }
                else if( !strcasecmp( "k", node ) )
                {
                    /* Karaoke tags */
                    const char *name, *value;
                    while( (name = xml_ReaderNextAttr( p_xml_reader, &value )) != NULL )
                    {
                        if( !strcasecmp( "t", name ) && value )
                            i_k_date += atoi( value );
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

                i_text_length += SetupText( p_filter,
                                            &psz_text[i_text_length],
                                            &pp_styles[i_text_length],
                                            pi_k_dates ? &pi_k_dates[i_text_length] : NULL,
                                            psz_node,
                                            GetStyleFromFontStack( p_sys,
                                                                   &p_fonts,
                                                                   i_style_flags ),
                                            i_k_date );
                free( psz_node );
                break;
            }
        }
    }

    *pi_len = i_text_length;

    while( VLC_SUCCESS == PopFont( &p_fonts ) );

    return VLC_SUCCESS;
}

static void FreeLine( line_desc_t *p_line )
{
    for( int i = 0; i < p_line->i_character_count; i++ )
        FT_Done_Glyph( (FT_Glyph)p_line->p_character[i].p_glyph );

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
                if( !strcasecmp( p_face->family_name, p_style->psz_fontname ) &&
                    (p_style->i_style_flags & (STYLE_BOLD | STYLE_BOLD)) == i_style_received )
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

static bool FaceStyleEquals( const text_style_t *p_style1,
                             const text_style_t *p_style2 )
{
    if( !p_style1 || !p_style2 )
        return false;
    if( p_style1 == p_style2 )
        return true;

    const int i_style_mask = STYLE_BOLD | STYLE_ITALIC;
    return (p_style1->i_style_flags & i_style_mask) == (p_style2->i_style_flags & i_style_mask) &&
           !strcmp( p_style1->psz_fontname, p_style2->psz_fontname );
}

static int GetGlyph( filter_t *p_filter,
                     FT_Glyph *pp_glyph,
                     FT_BBox  *p_bbox,

                     FT_Face  p_face,
                     int i_glyph_index,
                     int i_style_flags )
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

    FT_BBox bbox;
    FT_Glyph_Get_CBox( glyph, ft_glyph_bbox_pixels, &bbox );

    if( FT_Glyph_To_Bitmap( &glyph, FT_RENDER_MODE_NORMAL, 0, 1) )
    {
        FT_Done_Glyph( glyph );
        return VLC_EGENERIC;
    }

    *pp_glyph = glyph;
    *p_bbox   = bbox;
    return VLC_SUCCESS;
}

static int ProcessLines( filter_t *p_filter,
                         line_desc_t **pp_lines,
                         FT_Vector   *p_size,

                         uint32_t *psz_text,
                         text_style_t **pp_styles,
                         uint32_t *pi_k_dates,
                         int i_len )
{
    filter_sys_t   *p_sys = p_filter->p_sys;
    uint32_t       *p_fribidi_string = NULL;
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

    *pp_lines = NULL;
    line_desc_t **pp_line_next = pp_lines;

    FT_BBox bbox = {
        .xMin = 0,
        .yMin = 0,
        .xMax = 0,
        .yMax = 0,
    };
    FT_Vector pen = { .x = 0, .y = 0 };
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
        pen.x = 0;
        int i_face_height = 0;
        FT_BBox line_bbox = {
            .xMin = 0,
            .yMin = 0,
            .xMax = 0,
            .yMax = 0,
        };
        typedef struct {
            int       i_index;
            FT_Vector pen;
            FT_BBox   line_bbox;
            int i_face_height;
        } break_point_t;
        break_point_t break_point;
        break_point_t break_point_fallback;

#define SAVE_BP(dst) do { \
        dst.i_index = i_index; \
        dst.pen = pen; \
        dst.line_bbox = line_bbox; \
        dst.i_face_height = i_face_height; \
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
            }
            p_previous_style = p_current_style;

            i_face_height = __MAX(i_face_height, FT_CEIL(p_current_face->size->metrics.height));

            /* Render the part */
            bool b_break_line = false;
            int i_glyph_last = 0;
            while( i_part_length > 0 )
            {
                const text_style_t *p_glyph_style = pp_styles[i_index];
                uint32_t character = psz_text[i_index];
                int i_glyph_index = FT_Get_Char_Index( p_current_face, character );

                /* Get kerning vector */
                FT_Vector kerning = { .x = 0, .y = 0 };
                if( FT_HAS_KERNING( p_current_face ) && i_glyph_last != 0 && i_glyph_index != 0 )
                    FT_Get_Kerning( p_current_face, i_glyph_last, i_glyph_index, ft_kerning_default, &kerning );

                /* Get the glyph bitmap and its bounding box and all the associated properties */
                FT_Glyph glyph;
                FT_BBox  glyph_bbox;
                if( GetGlyph( p_filter, &glyph, &glyph_bbox,
                              p_current_face, i_glyph_index, p_glyph_style->i_style_flags ) )
                    goto next;

                FT_Vector glyph_pos = {
                    .x = pen.x + FT_CEIL(kerning.x),
                    .y = pen.y
                };
                bool     b_karaoke = pi_karaoke_bar && pi_karaoke_bar[i_index] != 0;
                uint32_t i_color = b_karaoke ? (p_glyph_style->i_karaoke_background_color |
                                                (p_glyph_style->i_karaoke_background_alpha << 24))
                                             : (p_glyph_style->i_font_color |
                                                (p_glyph_style->i_font_alpha << 24));
                int i_ul_offset    = 0;
                int i_ul_thickness = 0;
                if( p_glyph_style->i_style_flags & (STYLE_UNDERLINE | STYLE_STRIKEOUT) )
                {
                    i_ul_offset = abs( FT_FLOOR(FT_MulFix(p_current_face->underline_position,
                                                          p_current_face->size->metrics.y_scale)) );

                    i_ul_thickness = abs( FT_CEIL(FT_MulFix(p_current_face->underline_thickness,
                                                            p_current_face->size->metrics.y_scale)) );

                    if( p_glyph_style->i_style_flags & STYLE_STRIKEOUT )
                    {
                        /* Move the baseline to make it strikethrough instead of
                         * underline. That means that strikethrough takes precedence
                         */
                        i_ul_offset -= abs( FT_FLOOR(FT_MulFix(p_current_face->descender*2,
                                                               p_current_face->size->metrics.y_scale)) );
                    }
                }
                FT_BitmapGlyph glyph_bmp = (FT_BitmapGlyph)glyph;
                FT_BBox line_bbox_new = {
                    .xMin = 0,
                    .xMax = __MAX( line_bbox.xMax,
                                   glyph_pos.x + glyph_bbox.xMax - glyph_bbox.xMin + glyph_bmp->left ),
                    .yMin = 0,
                    .yMax = __MAX( line_bbox.yMax,
                                   glyph_pos.y + glyph_bbox.yMax - glyph_bbox.yMin + glyph_bmp->top ),
                };

                b_break_line = i_index > i_start &&
                               line_bbox_new.xMax >= p_filter->fmt_out.video.i_visible_width;
                if( b_break_line )
                {
                    FT_Done_Glyph( glyph );

                    break_point_t *p_bp = NULL;
                    if( break_point.i_index > i_start )
                        p_bp = &break_point;
                    else if( break_point_fallback.i_index > i_start )
                        p_bp = &break_point_fallback;

                    if( p_bp )
                    {
                        msg_Dbg( p_filter, "Breaking line");
                        for( int i = p_bp->i_index; i < i_index; i++ )
                            FT_Done_Glyph( (FT_Glyph)p_line->p_character[i - i_start].p_glyph );
                        p_line->i_character_count = p_bp->i_index - i_start;

                        i_index = p_bp->i_index;
                        pen = p_bp->pen;
                        line_bbox = p_bp->line_bbox;
                        i_face_height = p_bp->i_face_height;
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
                    .pos     = glyph_pos,
                    .i_color = i_color,
                    .i_line_offset = i_ul_offset,
                    .i_line_thickness = i_ul_thickness,
                };

                pen.x += FT_CEIL(kerning.x) + FT_CEIL(p_current_face->glyph->advance.x);
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
        bbox.xMax = __MAX(bbox.xMax, line_bbox.xMax);
        bbox.yMax = __MAX(bbox.yMax, line_bbox.yMax);

        pen.y += i_face_height;

        /* Terminate and append the line */
        if( p_line )
        {
            p_line->i_width  = line_bbox.xMax - line_bbox.xMin;
            *pp_line_next = p_line;
            pp_line_next = &p_line->p_next;
        }

        /* Skip what we have rendered and the line delimitor if present */
        i_start = i_index;
        if( i_start < i_len && psz_text[i_start] == '\n' )
            i_start++;

        if( bbox.yMax >= p_filter->fmt_out.video.i_visible_height )
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

    p_size->x = bbox.xMax - bbox.xMin;
    p_size->y = bbox.yMax - bbox.yMin;
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

    const size_t i_text_max = strlen( b_html ? p_region_in->psz_html
                                             : p_region_in->psz_text );

    uint32_t *psz_text = calloc( i_text_max, sizeof( *psz_text ) );
    text_style_t **pp_styles = calloc( i_text_max, sizeof( *pp_styles ) );
    if( !psz_text || !pp_styles )
    {
        free( psz_text );
        free( pp_styles );
        return VLC_EGENERIC;
    }

    /* Reset the default fontsize in case screen metrics have changed */
    p_filter->p_sys->i_font_size = GetFontSize( p_filter );

    /* */
    int rv = VLC_SUCCESS;
    int i_text_length = 0;
    FT_Vector result = {0, 0};
    line_desc_t *p_lines = NULL;

    uint32_t *pi_k_durations   = NULL;

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
                               p_xml_reader, p_region_in->p_style );
        }

        if( p_xml_reader )
            p_filter->p_sys->p_xml = xml_ReaderReset( p_xml_reader, NULL );

        stream_Delete( p_sub );
    }
    else
#endif
    {
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

        i_text_length = SetupText( p_filter,
                                   psz_text,
                                   pp_styles,
                                   NULL,
                                   p_region_in->psz_text, p_style, 0 );
    }

    if( !rv && i_text_length > 0 )
    {
        rv = ProcessLines( p_filter,
                           &p_lines, &result,
                           psz_text, pp_styles, pi_k_durations, i_text_length );
    }

    p_region_out->i_x = p_region_in->i_x;
    p_region_out->i_y = p_region_in->i_y;

    /* Don't attempt to render text that couldn't be layed out
     * properly. */
    if( !rv && i_text_length > 0 && result.x > 0 && result.y > 0)
    {
        if( var_InheritBool( p_filter, "freetype-yuvp" ) )
            RenderYUVP( p_filter, p_region_out, p_lines,
                        result.x, result.y );
        else
            RenderYUVA( p_filter, p_region_out, p_lines,
                        result.x, result.y );


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
    p_sys->i_font_opacity = var_InheritInteger( p_filter,"freetype-opacity" );
    p_sys->i_font_opacity = __MAX( __MIN( p_sys->i_font_opacity, 255 ), 0 );
    p_sys->i_font_color = var_InheritInteger( p_filter, "freetype-color" );
    p_sys->i_font_color = __MAX( __MIN( p_sys->i_font_color , 0xFFFFFF ), 0 );

    p_sys->i_background_opacity = var_InheritInteger( p_filter,"freetype-background-opacity" );;
    p_sys->i_background_opacity = __MAX( __MIN( p_sys->i_background_opacity, 255 ), 0 );
    p_sys->i_background_color = var_InheritInteger( p_filter, "freetype-background-color" );
    p_sys->i_background_color = __MAX( __MIN( p_sys->i_background_color, 0xFFFFFF ), 0 );

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


