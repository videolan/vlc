/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Bernie Purcell <bitmap@videolan.org>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_osd.h>
#include <vlc_filter.h>
#include <vlc_stream.h>
#include <vlc_xml.h>
#include <vlc_input.h>
#include <vlc_strings.h>
#include <vlc_dialog.h>
#include <vlc_memory.h>

#include <math.h>
#include <errno.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#define FT_FLOOR(X)     ((X & -64) >> 6)
#define FT_CEIL(X)      (((X + 63) & -64) >> 6)
#ifndef FT_MulFix
 #define FT_MulFix(v, s) (((v)*(s))>>16)
#endif

#ifdef __APPLE__
#define DEFAULT_FONT "/Library/Fonts/Arial Black.ttf"
#define FC_DEFAULT_FONT "Arial Black"
#elif defined( SYS_BEOS )
#define DEFAULT_FONT "/boot/beos/etc/fonts/ttfonts/Swiss721.ttf"
#define FC_DEFAULT_FONT "Swiss"
#elif defined( WIN32 )
#define DEFAULT_FONT "" /* Default font found at run-time */
#define FC_DEFAULT_FONT "Arial"
#elif defined( HAVE_MAEMO )
#define DEFAULT_FONT "/usr/share/fonts/nokia/nosnb.ttf"
#define FC_DEFAULT_FONT "Nokia Sans Bold"
#else
#define DEFAULT_FONT "/usr/share/fonts/truetype/freefont/FreeSerifBold.ttf"
#define FC_DEFAULT_FONT "Serif Bold"
#endif

#if defined(HAVE_FRIBIDI)
#include <fribidi/fribidi.h>
#endif

#ifdef HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#undef DEFAULT_FONT
#define DEFAULT_FONT FC_DEFAULT_FONT
#endif

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Create ( vlc_object_t * );
static void Destroy( vlc_object_t * );

#define FONT_TEXT N_("Font")

#ifdef HAVE_FONTCONFIG
#define FONT_LONGTEXT N_("Font family for the font you want to use")
#else
#define FONT_LONGTEXT N_("Fontfile for the font you want to use")
#endif

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
    "relative size will be overriden." )

static const int pi_sizes[] = { 20, 18, 16, 12, 6 };
static const char *const ppsz_sizes_text[] = {
    N_("Smaller"), N_("Small"), N_("Normal"), N_("Large"), N_("Larger") };
#define YUVP_TEXT N_("Use YUVP renderer")
#define YUVP_LONGTEXT N_("This renders the font using \"paletized YUV\". " \
  "This option is only needed if you want to encode into DVB subtitles" )
#define EFFECT_TEXT N_("Font Effect")
#define EFFECT_LONGTEXT N_("It is possible to apply effects to the rendered " \
"text to improve its readability." )

#define EFFECT_BACKGROUND  1
#define EFFECT_OUTLINE     2
#define EFFECT_OUTLINE_FAT 3

static int const pi_effects[] = { 1, 2, 3 };
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

    add_font( "freetype-font", DEFAULT_FONT, NULL, FONT_TEXT, FONT_LONGTEXT,
              false )

    add_integer( "freetype-fontsize", 0, NULL, FONTSIZE_TEXT,
                 FONTSIZE_LONGTEXT, true )

    /* opacity valid on 0..255, with default 255 = fully opaque */
    add_integer_with_range( "freetype-opacity", 255, 0, 255, NULL,
        OPACITY_TEXT, OPACITY_LONGTEXT, true )

    /* hook to the color values list, with default 0x00ffffff = white */
    add_integer( "freetype-color", 0x00FFFFFF, NULL, COLOR_TEXT,
                 COLOR_LONGTEXT, false )
        change_integer_list( pi_color_values, ppsz_color_descriptions, NULL )

    add_integer( "freetype-rel-fontsize", 16, NULL, FONTSIZER_TEXT,
                 FONTSIZER_LONGTEXT, false )
        change_integer_list( pi_sizes, ppsz_sizes_text, NULL )
    add_integer( "freetype-effect", 2, NULL, EFFECT_TEXT,
                 EFFECT_LONGTEXT, false )
        change_integer_list( pi_effects, ppsz_effects_text, NULL )

    add_bool( "freetype-yuvp", false, NULL, YUVP_TEXT,
              YUVP_LONGTEXT, true )
    set_capability( "text renderer", 100 )
    add_shortcut( "text" )
    set_callbacks( Create, Destroy )
vlc_module_end ()



/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* The RenderText call maps to pf_render_string, defined in vlc_filter.h */
static int RenderText( filter_t *, subpicture_region_t *,
                       subpicture_region_t * );
#ifdef HAVE_FONTCONFIG
static int RenderHtml( filter_t *, subpicture_region_t *,
                       subpicture_region_t * );
static char *FontConfig_Select( FcConfig *, const char *,
                                bool, bool, int * );
#endif


static int LoadFontsFromAttachments( filter_t *p_filter );

static int GetFontSize( filter_t *p_filter );
static int SetFontSize( filter_t *, int );
static void YUVFromRGB( uint32_t i_argb,
                        uint8_t *pi_y, uint8_t *pi_u, uint8_t *pi_v );

typedef struct line_desc_t line_desc_t;
struct line_desc_t
{
    /** NULL-terminated list of glyphs making the string */
    FT_BitmapGlyph *pp_glyphs;
    /** list of relative positions for the glyphs */
    FT_Vector      *p_glyph_pos;
    /** list of RGB information for styled text
     * -- if the rendering mode supports it (RenderYUVA) and
     *  b_new_color_mode is set, then it becomes possible to
     *  have multicoloured text within the subtitles. */
    uint32_t       *p_fg_rgb;
    uint32_t       *p_bg_rgb;
    uint8_t        *p_fg_bg_ratio; /* 0x00=100% FG --> 0x7F=100% BG */
    bool      b_new_color_mode;
    /** underline information -- only supplied if text should be underlined */
    uint16_t       *pi_underline_offset;
    uint16_t       *pi_underline_thickness;

    int             i_height;
    int             i_width;
    int             i_red, i_green, i_blue;
    int             i_alpha;

    line_desc_t    *p_next;
};
static line_desc_t *NewLine( int );

typedef struct
{
    int         i_font_size;
    uint32_t    i_font_color;         /* ARGB */
    uint32_t    i_karaoke_bg_color;   /* ARGB */
    bool  b_italic;
    bool  b_bold;
    bool  b_underline;
    char       *psz_fontname;
} ft_style_t;

static int Render( filter_t *, subpicture_region_t *, line_desc_t *, int, int);
static void FreeLines( line_desc_t * );
static void FreeLine( line_desc_t * );

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
    bool     i_use_kerning;
    uint8_t        i_font_opacity;
    int            i_font_color;
    int            i_font_size;
    int            i_effect;

    int            i_default_font_size;
    int            i_display_height;
#ifdef HAVE_FONTCONFIG
    char*          psz_fontfamily;
    xml_t         *p_xml;
#endif

    input_attachment_t **pp_font_attachments;
    int                  i_font_attachments;

};

#define UCHAR uint32_t
#define TR_DEFAULT_FONT p_sys->psz_fontfamily
#define TR_FONT_STYLE_PTR ft_style_t *

#include "text_renderer.h"

/*****************************************************************************
 * Create: allocates osd-text video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t      *p_filter = (filter_t *)p_this;
    filter_sys_t  *p_sys;
    char          *psz_fontfile=NULL;
    char          *psz_fontfamily=NULL;
    int            i_error,fontindex;

#ifdef HAVE_FONTCONFIG
    FcPattern     *fontpattern = NULL, *fontmatch = NULL;
    /* Initialise result to Match, as fontconfig doesnt
     * really set this other than some error-cases */
    FcResult       fontresult = FcResultMatch;
#endif


    /* Allocate structure */
    p_filter->p_sys = p_sys = malloc( sizeof( filter_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
 #ifdef HAVE_FONTCONFIG
    p_sys->psz_fontfamily = NULL;
    p_sys->p_xml = NULL;
#endif
    p_sys->p_face = 0;
    p_sys->p_library = 0;
    p_sys->i_font_size = 0;
    p_sys->i_display_height = 0;

    var_Create( p_filter, "freetype-rel-fontsize",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    psz_fontfamily = var_CreateGetString( p_filter, "freetype-font" );
    p_sys->i_default_font_size = var_CreateGetInteger( p_filter, "freetype-fontsize" );
    p_sys->i_effect = var_CreateGetInteger( p_filter, "freetype-effect" );
    p_sys->i_font_opacity = var_CreateGetInteger( p_filter,"freetype-opacity" );
    p_sys->i_font_opacity = __MAX( __MIN( p_sys->i_font_opacity, 255 ), 0 );
    p_sys->i_font_color = var_CreateGetInteger( p_filter, "freetype-color" );
    p_sys->i_font_color = __MAX( __MIN( p_sys->i_font_color , 0xFFFFFF ), 0 );

    fontindex=0;
    if( !psz_fontfamily || !*psz_fontfamily )
    {
        free( psz_fontfamily );
#ifdef HAVE_FONTCONFIG
        psz_fontfamily=strdup( DEFAULT_FONT );
#else
        psz_fontfamily = (char *)malloc( PATH_MAX + 1 );
        if( !psz_fontfamily )
            goto error;
# ifdef WIN32
        GetWindowsDirectory( psz_fontfamily , PATH_MAX + 1 );
        strcat( psz_fontfamily, "\\fonts\\arial.ttf" );
# else
        strcpy( psz_fontfamily, DEFAULT_FONT );
# endif
        msg_Err( p_filter,"User didn't specify fontfile, using %s", psz_fontfamily);
#endif
    }

#ifdef HAVE_FONTCONFIG
    /* Lets find some fontfile from freetype-font variable family */
    char *psz_fontsize;
    if( asprintf( &psz_fontsize, "%d", p_sys->i_default_font_size ) == -1 )
        goto error;

#ifdef WIN32
    dialog_progress_bar_t *p_dialog = dialog_ProgressCreate( p_filter,
            _("Building font cache"),
            _("Please wait while your font cache is rebuilt.\n"
                "This should take less than few minutes."), NULL );
    char *path = xmalloc( PATH_MAX + 1 );
    /* Fontconfig doesnt seem to know where windows fonts are with
     * current contribs. So just tell default windows font directory
     * is the place to search fonts
     */
    GetWindowsDirectory( path, PATH_MAX + 1 );
    strcat( path, "\\fonts" );
    if( p_dialog )
        dialog_ProgressSet( p_dialog, NULL, 0.4 );

    FcConfigAppFontAddDir( NULL , path );
    free(path);


    if( p_dialog )
        dialog_ProgressSet( p_dialog, NULL, 0.5 );
#endif
    mtime_t t1, t2;

    msg_Dbg( p_filter, "Building font database.");
    t1 = mdate();
    FcConfigBuildFonts( NULL );
    t2 = mdate();

    msg_Dbg( p_filter, "Finished building font database." );
    msg_Dbg( p_filter, "Took %ld microseconds", (long)((t2 - t1)) );

    fontpattern = FcPatternCreate();

    if( !fontpattern )
    {
        msg_Err( p_filter, "Creating fontpattern failed");
        goto error;
    }

#ifdef WIN32
    if( p_dialog )
        dialog_ProgressSet( p_dialog, NULL, 0.7 );
#endif
    FcPatternAddString( fontpattern, FC_FAMILY, psz_fontfamily);
    FcPatternAddString( fontpattern, FC_SIZE, psz_fontsize );
    free( psz_fontsize );

    if( FcConfigSubstitute( NULL, fontpattern, FcMatchPattern ) == FcFalse )
    {
        msg_Err( p_filter, "FontSubstitute failed");
        goto error;
    }
    FcDefaultSubstitute( fontpattern );

#ifdef WIN32
    if( p_dialog )
        dialog_ProgressSet( p_dialog, NULL, 0.8 );
#endif
    /* testing fontresult here doesn't do any good really, but maybe it will
     * in future as fontconfig code doesn't set it in all cases and just
     * returns NULL or doesn't set to to Match on all Match cases.*/
    fontmatch = FcFontMatch( NULL, fontpattern, &fontresult );
    if( !fontmatch || fontresult == FcResultNoMatch )
    {
        msg_Err( p_filter, "Fontmatching failed");
        goto error;
    }

    FcPatternGetString( fontmatch, FC_FILE, 0, &psz_fontfile);
    FcPatternGetInteger( fontmatch, FC_INDEX, 0, &fontindex );
    if( !psz_fontfile )
    {
        msg_Err( p_filter, "Failed to get fontfile");
        goto error;
    }

    msg_Dbg( p_filter, "Using %s as font from file %s", psz_fontfamily, psz_fontfile );
    p_sys->psz_fontfamily = strdup( psz_fontfamily );
# ifdef WIN32
    if( p_dialog )
    {
        dialog_ProgressSet( p_dialog, NULL, 1.0 );
        dialog_ProgressDestroy( p_dialog );
    }
# endif

#else

#ifdef HAVE_FONTCONFIG
    p_sys->psz_fontfamily = strdup( DEFAULT_FONT );
    psz_fontfile = psz_fontfamily;
#endif

#endif

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
        msg_Err( p_filter, "file %s have unknown format", psz_fontfile );
        goto error;
    }
    else if( i_error )
    {
        msg_Err( p_filter, "failed to load font file %s", psz_fontfile );
        goto error;
    }

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
#ifdef HAVE_FONTCONFIG
    p_filter->pf_render_html = RenderHtml;
    FcPatternDestroy( fontmatch );
    FcPatternDestroy( fontpattern );
#else
    p_filter->pf_render_html = NULL;
#endif

    free( psz_fontfamily );
    LoadFontsFromAttachments( p_filter );

    return VLC_SUCCESS;

error:
#ifdef HAVE_FONTCONFIG
    if( fontmatch ) FcPatternDestroy( fontmatch );
    if( fontpattern ) FcPatternDestroy( fontpattern );
#endif
    if( p_sys->p_face ) FT_Done_Face( p_sys->p_face );
    if( p_sys->p_library ) FT_Done_FreeType( p_sys->p_library );
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
        int   k;

        for( k = 0; k < p_sys->i_font_attachments; k++ )
            vlc_input_attachment_Delete( p_sys->pp_font_attachments[k] );

        free( p_sys->pp_font_attachments );
    }

#ifdef HAVE_FONTCONFIG
    if( p_sys->p_xml ) xml_Delete( p_sys->p_xml );
    free( p_sys->psz_fontfamily );
#endif

    /* FcFini asserts calling the subfunction FcCacheFini()
     * even if no other library functions have been made since FcInit(),
     * so don't call it. */

    FT_Done_Face( p_sys->p_face );
    FT_Done_FreeType( p_sys->p_library );
    free( p_sys );
}

/*****************************************************************************
 * Make any TTF/OTF fonts present in the attachments of the media file
 * and store them for later use by the FreeType Engine
 *****************************************************************************/
static int LoadFontsFromAttachments( filter_t *p_filter )
{
    filter_sys_t         *p_sys = p_filter->p_sys;
    input_thread_t       *p_input;
    input_attachment_t  **pp_attachments;
    int                   i_attachments_cnt;
    int                   k;
    int                   rv = VLC_SUCCESS;

    p_input = (input_thread_t *)vlc_object_find( p_filter, VLC_OBJECT_INPUT, FIND_PARENT );
    if( ! p_input )
        return VLC_EGENERIC;

    if( VLC_SUCCESS != input_Control( p_input, INPUT_GET_ATTACHMENTS, &pp_attachments, &i_attachments_cnt ))
    {
        vlc_object_release(p_input);
        return VLC_EGENERIC;
    }

    p_sys->i_font_attachments = 0;
    p_sys->pp_font_attachments = malloc( i_attachments_cnt * sizeof( input_attachment_t * ));
    if(! p_sys->pp_font_attachments )
        rv = VLC_ENOMEM;

    for( k = 0; k < i_attachments_cnt; k++ )
    {
        input_attachment_t *p_attach = pp_attachments[k];

        if( p_sys->pp_font_attachments )
        {
            if(( !strcmp( p_attach->psz_mime, "application/x-truetype-font" ) || // TTF
                 !strcmp( p_attach->psz_mime, "application/x-font-otf" ) ) &&    // OTF
               ( p_attach->i_data > 0 ) &&
               ( p_attach->p_data != NULL ) )
            {
                p_sys->pp_font_attachments[ p_sys->i_font_attachments++ ] = p_attach;
            }
            else
            {
                vlc_input_attachment_Delete( p_attach );
            }
        }
        else
        {
            vlc_input_attachment_Delete( p_attach );
        }
    }
    free( pp_attachments );

    vlc_object_release(p_input);

    return rv;
}

/*****************************************************************************
 * Render: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
static int Render( filter_t *p_filter, subpicture_region_t *p_region,
                   line_desc_t *p_line, int i_width, int i_height )
{
    VLC_UNUSED(p_filter);
    static const uint8_t pi_gamma[16] =
        {0x00, 0x52, 0x84, 0x96, 0xb8, 0xca, 0xdc, 0xee, 0xff,
          0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    uint8_t *p_dst;
    video_format_t fmt;
    int i, x, y, i_pitch;
    uint8_t i_y; /* YUV values, derived from incoming RGB */
    int8_t i_u, i_v;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_CODEC_YUVP;
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_visible_width = i_width + 4;
    fmt.i_height = fmt.i_visible_height = i_height + 4;
    if( p_region->fmt.i_visible_width > 0 )
        fmt.i_visible_width = p_region->fmt.i_visible_width;
    if( p_region->fmt.i_visible_height > 0 )
        fmt.i_visible_height = p_region->fmt.i_visible_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;

    assert( !p_region->p_picture );
    p_region->p_picture = picture_NewFromFormat( &fmt );
    if( !p_region->p_picture )
        return VLC_EGENERIC;
    p_region->fmt = fmt;

    /* Calculate text color components */
    i_y = (uint8_t)(( 66 * p_line->i_red  + 129 * p_line->i_green +
                      25 * p_line->i_blue + 128) >> 8) +  16;
    i_u = (int8_t)(( -38 * p_line->i_red  -  74 * p_line->i_green +
                     112 * p_line->i_blue + 128) >> 8) + 128;
    i_v = (int8_t)(( 112 * p_line->i_red  -  94 * p_line->i_green -
                      18 * p_line->i_blue + 128) >> 8) + 128;

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
    int y, x, z;
    int i_pitch;
    uint8_t *p_dst_y,*p_dst_u,*p_dst_v,*p_dst_a;

    p_dst_y = p_region->p_picture->Y_PIXELS;
    p_dst_u = p_region->p_picture->U_PIXELS;
    p_dst_v = p_region->p_picture->V_PIXELS;
    p_dst_a = p_region->p_picture->A_PIXELS;
    i_pitch = p_region->p_picture->A_PITCH;

    int i_offset = ( p_this_glyph_pos->y + i_glyph_tmax + i_line_offset + 3 ) * i_pitch +
                     p_this_glyph_pos->x + p_this_glyph->left + 3 + i_align_offset;

    for( y = 0; y < i_line_thickness; y++ )
    {
        int i_extra = p_this_glyph->bitmap.width;

        if( b_ul_next_char )
        {
            i_extra = (p_next_glyph_pos->x + p_next_glyph->left) -
                      (p_this_glyph_pos->x + p_this_glyph->left);
        }
        for( x = 0; x < i_extra; x++ )
        {
            bool b_ok = true;

            /* break the underline around the tails of any glyphs which cross it */
            for( z = x - i_line_thickness;
                 z < x + i_line_thickness && b_ok;
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
    int x,y;

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
                for( x = 0; x < p_glyph->bitmap.width; x++, i_bitmap_offset++ )
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
 * Render: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
static int RenderYUVA( filter_t *p_filter, subpicture_region_t *p_region,
                   line_desc_t *p_line, int i_width, int i_height )
{
    uint8_t *p_dst_y,*p_dst_u,*p_dst_v,*p_dst_a;
    video_format_t fmt;
    int i, x, y, i_pitch, i_alpha;
    uint8_t i_y, i_u, i_v; /* YUV values, derived from incoming RGB */

    if( i_width == 0 || i_height == 0 )
        return VLC_SUCCESS;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_CODEC_YUVA;
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_visible_width = i_width + 6;
    fmt.i_height = fmt.i_visible_height = i_height + 6;
    if( p_region->fmt.i_visible_width > 0 )
        fmt.i_visible_width = p_region->fmt.i_visible_width;
    if( p_region->fmt.i_visible_height > 0 )
        fmt.i_visible_height = p_region->fmt.i_visible_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;

    p_region->p_picture = picture_NewFromFormat( &fmt );
    if( !p_region->p_picture )
        return VLC_EGENERIC;
    p_region->fmt = fmt;

    /* Calculate text color components */
    YUVFromRGB( (p_line->i_red   << 16) |
                (p_line->i_green <<  8) |
                (p_line->i_blue       ),
                &i_y, &i_u, &i_v);
    i_alpha = p_line->i_alpha;

    p_dst_y = p_region->p_picture->Y_PIXELS;
    p_dst_u = p_region->p_picture->U_PIXELS;
    p_dst_v = p_region->p_picture->V_PIXELS;
    p_dst_a = p_region->p_picture->A_PIXELS;
    i_pitch = p_region->p_picture->A_PITCH;

    /* Initialize the region pixels */
    if( p_filter->p_sys->i_effect != EFFECT_BACKGROUND )
    {
        memset( p_dst_y, 0x00, i_pitch * p_region->fmt.i_height );
        memset( p_dst_u, 0x80, i_pitch * p_region->fmt.i_height );
        memset( p_dst_v, 0x80, i_pitch * p_region->fmt.i_height );
        memset( p_dst_a, 0, i_pitch * p_region->fmt.i_height );
    }
    else
    {
        memset( p_dst_y, 0x0, i_pitch * p_region->fmt.i_height );
        memset( p_dst_u, 0x80, i_pitch * p_region->fmt.i_height );
        memset( p_dst_v, 0x80, i_pitch * p_region->fmt.i_height );
        memset( p_dst_a, 0x80, i_pitch * p_region->fmt.i_height );
    }
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

            if( p_line->b_new_color_mode )
            {
                /* Every glyph can (and in fact must) have its own color */
                YUVFromRGB( p_line->p_fg_rgb[ i ], &i_y, &i_u, &i_v );
            }

            for( y = 0, i_bitmap_offset = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++, i_bitmap_offset++ )
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

/**
 * This function renders a text subpicture region into another one.
 * It also calculates the size needed for this string, and renders the
 * needed glyphs into memory. It is used as pf_add_string callback in
 * the vout method by this module
 */
static int RenderText( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    line_desc_t  *p_lines = NULL, *p_line = NULL, *p_next = NULL, *p_prev = NULL;
    int i, i_pen_y, i_pen_x, i_error, i_glyph_index, i_previous;
    uint32_t *psz_unicode, *psz_unicode_orig = NULL, i_char, *psz_line_start;
    int i_string_length;
    char *psz_string;
    vlc_iconv_t iconv_handle = (vlc_iconv_t)(-1);
    int i_font_color, i_font_alpha, i_font_size, i_red, i_green, i_blue;
    vlc_value_t val;
    int i_scale = 1000;

    FT_BBox line;
    FT_BBox glyph_size;
    FT_Vector result;
    FT_Glyph tmp_glyph;

    /* Sanity check */
    if( !p_region_in || !p_region_out ) return VLC_EGENERIC;
    psz_string = p_region_in->psz_text;
    if( !psz_string || !*psz_string ) return VLC_EGENERIC;

    if( VLC_SUCCESS == var_Get( p_filter, "scale", &val ))
        i_scale = val.i_int;

    if( p_region_in->p_style )
    {
        i_font_color = __MAX( __MIN( p_region_in->p_style->i_font_color, 0xFFFFFF ), 0 );
        i_font_alpha = __MAX( __MIN( p_region_in->p_style->i_font_alpha, 255 ), 0 );
        i_font_size  = __MAX( __MIN( p_region_in->p_style->i_font_size, 255 ), 0 ) * i_scale / 1000;
    }
    else
    {
        i_font_color = p_sys->i_font_color;
        i_font_alpha = 255 - p_sys->i_font_opacity;
        i_font_size  = p_sys->i_default_font_size * i_scale / 1000;
    }

    if( i_font_color == 0xFFFFFF ) i_font_color = p_sys->i_font_color;
    if( !i_font_alpha ) i_font_alpha = 255 - p_sys->i_font_opacity;
    SetFontSize( p_filter, i_font_size );

    i_red   = ( i_font_color & 0x00FF0000 ) >> 16;
    i_green = ( i_font_color & 0x0000FF00 ) >>  8;
    i_blue  =   i_font_color & 0x000000FF;

    result.x =  result.y = 0;
    line.xMin = line.xMax = line.yMin = line.yMax = 0;

    psz_unicode = psz_unicode_orig =
        malloc( ( strlen(psz_string) + 1 ) * sizeof(uint32_t) );
    if( psz_unicode == NULL )
        goto error;
#if defined(WORDS_BIGENDIAN)
    iconv_handle = vlc_iconv_open( "UCS-4BE", "UTF-8" );
#else
    iconv_handle = vlc_iconv_open( "UCS-4LE", "UTF-8" );
#endif
    if( iconv_handle == (vlc_iconv_t)-1 )
    {
        msg_Warn( p_filter, "unable to do conversion" );
        goto error;
    }

    {
        char *p_out_buffer;
        const char *p_in_buffer = psz_string;
        size_t i_in_bytes, i_out_bytes, i_out_bytes_left, i_ret;
        i_in_bytes = strlen( psz_string );
        i_out_bytes = i_in_bytes * sizeof( uint32_t );
        i_out_bytes_left = i_out_bytes;
        p_out_buffer = (char *)psz_unicode;
        i_ret = vlc_iconv( iconv_handle, (const char**)&p_in_buffer,
                           &i_in_bytes,
                           &p_out_buffer, &i_out_bytes_left );

        vlc_iconv_close( iconv_handle );

        if( i_in_bytes )
        {
            msg_Warn( p_filter, "failed to convert string to unicode (%m), "
                      "bytes left %u", (unsigned)i_in_bytes );
            goto error;
        }
        *(uint32_t*)p_out_buffer = 0;
        i_string_length = (i_out_bytes - i_out_bytes_left) / sizeof(uint32_t);
    }

#if defined(HAVE_FRIBIDI)
    {
        uint32_t *p_fribidi_string;
        int32_t start_pos, pos = 0;

        p_fribidi_string = malloc( (i_string_length + 1) * sizeof(uint32_t) );
        if( !p_fribidi_string )
            goto error;

        /* Do bidi conversion line-by-line */
        while( pos < i_string_length )
        {
            while( pos < i_string_length ) 
            {
                i_char = psz_unicode[pos];
                if (i_char != '\r' && i_char != '\n')
                    break;
                p_fribidi_string[pos] = i_char;
                ++pos;
            }
            start_pos = pos;
            while( pos < i_string_length )
            {
                i_char = psz_unicode[pos];
                if (i_char == '\r' || i_char == '\n')
                    break;
                ++pos;
            }
            if (pos > start_pos)
            {
                FriBidiCharType base_dir = FRIBIDI_TYPE_LTR;
                fribidi_log2vis((FriBidiChar*)psz_unicode + start_pos,
                                pos - start_pos,
                                &base_dir,
                                (FriBidiChar*)p_fribidi_string + start_pos,
                                0, 0, 0);
            }
        }

        free( psz_unicode_orig );
        psz_unicode = psz_unicode_orig = p_fribidi_string;
        p_fribidi_string[ i_string_length ] = 0;
    }
#endif

    /* Calculate relative glyph positions and a bounding box for the
     * entire string */
    if( !(p_line = NewLine( strlen( psz_string ))) )
        goto error;
    p_lines = p_line;
    i_pen_x = i_pen_y = 0;
    i_previous = i = 0;
    psz_line_start = psz_unicode;

#define face p_sys->p_face
#define glyph face->glyph

    while( *psz_unicode )
    {
        i_char = *psz_unicode++;
        if( i_char == '\r' ) /* ignore CR chars wherever they may be */
        {
            continue;
        }

        if( i_char == '\n' )
        {
            psz_line_start = psz_unicode;
            if( !(p_next = NewLine( strlen( psz_string ))) )
                goto error;
            p_line->p_next = p_next;
            p_line->i_width = line.xMax;
            p_line->i_height = face->size->metrics.height >> 6;
            p_line->pp_glyphs[ i ] = NULL;
            p_line->i_alpha = i_font_alpha;
            p_line->i_red = i_red;
            p_line->i_green = i_green;
            p_line->i_blue = i_blue;
            p_prev = p_line;
            p_line = p_next;
            result.x = __MAX( result.x, line.xMax );
            result.y += face->size->metrics.height >> 6;
            i_pen_x = 0;
            i_previous = i = 0;
            line.xMin = line.xMax = line.yMin = line.yMax = 0;
            i_pen_y += face->size->metrics.height >> 6;
#if 0
            msg_Dbg( p_filter, "Creating new line, i is %d", i );
#endif
            continue;
        }

        i_glyph_index = FT_Get_Char_Index( face, i_char );
        if( p_sys->i_use_kerning && i_glyph_index
            && i_previous )
        {
            FT_Vector delta;
            FT_Get_Kerning( face, i_previous, i_glyph_index,
                            ft_kerning_default, &delta );
            i_pen_x += delta.x >> 6;

        }
        p_line->p_glyph_pos[ i ].x = i_pen_x;
        p_line->p_glyph_pos[ i ].y = i_pen_y;
        i_error = FT_Load_Glyph( face, i_glyph_index, FT_LOAD_DEFAULT );
        if( i_error )
        {
            msg_Err( p_filter, "unable to render text FT_Load_Glyph returned"
                               " %d", i_error );
            goto error;
        }
        i_error = FT_Get_Glyph( glyph, &tmp_glyph );
        if( i_error )
        {
            msg_Err( p_filter, "unable to render text FT_Get_Glyph returned "
                               "%d", i_error );
            goto error;
        }
        FT_Glyph_Get_CBox( tmp_glyph, ft_glyph_bbox_pixels, &glyph_size );
        i_error = FT_Glyph_To_Bitmap( &tmp_glyph, ft_render_mode_normal, 0, 1);
        if( i_error )
        {
            FT_Done_Glyph( tmp_glyph );
            continue;
        }
        p_line->pp_glyphs[ i ] = (FT_BitmapGlyph)tmp_glyph;

        /* Do rest */
        line.xMax = p_line->p_glyph_pos[i].x + glyph_size.xMax -
            glyph_size.xMin + ((FT_BitmapGlyph)tmp_glyph)->left;
        if( line.xMax > (int)p_filter->fmt_out.video.i_visible_width - 20 )
        {
            FT_Done_Glyph( (FT_Glyph)p_line->pp_glyphs[ i ] );
            p_line->pp_glyphs[ i ] = NULL;
            FreeLine( p_line );
            p_line = NewLine( strlen( psz_string ));
            if( p_prev ) p_prev->p_next = p_line;
            else p_lines = p_line;

            uint32_t *psz_unicode_saved = psz_unicode;
            while( psz_unicode > psz_line_start && *psz_unicode != ' ' )
            {
                psz_unicode--;
            }
            if( psz_unicode == psz_line_start )
            {   /* try harder to break that line */
                psz_unicode = psz_unicode_saved;
                while( psz_unicode > psz_line_start &&
                    *psz_unicode != '_'  && *psz_unicode != '/' &&
                    *psz_unicode != '\\' && *psz_unicode != '.' )
                {
                    psz_unicode--;
                }
            }
            if( psz_unicode == psz_line_start )
            {
                msg_Warn( p_filter, "unbreakable string" );
                goto error;
            }
            else
            {
                *psz_unicode = '\n';
            }
            psz_unicode = psz_line_start;
            i_pen_x = 0;
            i_previous = i = 0;
            line.xMin = line.xMax = line.yMin = line.yMax = 0;
            continue;
        }
        line.yMax = __MAX( line.yMax, glyph_size.yMax );
        line.yMin = __MIN( line.yMin, glyph_size.yMin );

        i_previous = i_glyph_index;
        i_pen_x += glyph->advance.x >> 6;
        i++;
    }

    p_line->i_width = line.xMax;
    p_line->i_height = face->size->metrics.height >> 6;
    p_line->pp_glyphs[ i ] = NULL;
    p_line->i_alpha = i_font_alpha;
    p_line->i_red = i_red;
    p_line->i_green = i_green;
    p_line->i_blue = i_blue;
    result.x = __MAX( result.x, line.xMax );
    result.y += line.yMax - line.yMin;

#undef face
#undef glyph

    p_region_out->i_x = p_region_in->i_x;
    p_region_out->i_y = p_region_in->i_y;

    if( config_GetInt( p_filter, "freetype-yuvp" ) )
        Render( p_filter, p_region_out, p_lines, result.x, result.y );
    else
        RenderYUVA( p_filter, p_region_out, p_lines, result.x, result.y );

    free( psz_unicode_orig );
    FreeLines( p_lines );
    return VLC_SUCCESS;

 error:
    free( psz_unicode_orig );
    FreeLines( p_lines );
    return VLC_EGENERIC;
}

#ifdef HAVE_FONTCONFIG
static ft_style_t *CreateStyle( char *psz_fontname, int i_font_size,
        uint32_t i_font_color, uint32_t i_karaoke_bg_color, bool b_bold,
        bool b_italic, bool b_uline )
{
    ft_style_t  *p_style = malloc( sizeof( ft_style_t ));

    if( p_style )
    {
        p_style->i_font_size        = i_font_size;
        p_style->i_font_color       = i_font_color;
        p_style->i_karaoke_bg_color = i_karaoke_bg_color;
        p_style->b_italic           = b_italic;
        p_style->b_bold             = b_bold;
        p_style->b_underline        = b_uline;

        p_style->psz_fontname = strdup( psz_fontname );
    }
    return p_style;
}

static void DeleteStyle( ft_style_t *p_style )
{
    if( p_style )
    {
        free( p_style->psz_fontname );
        free( p_style );
    }
}

static bool StyleEquals( ft_style_t *s1, ft_style_t *s2 )
{
    if( !s1 || !s2 )
        return false;
    if( s1 == s2 )
        return true;

    if(( s1->i_font_size  == s2->i_font_size ) &&
       ( s1->i_font_color == s2->i_font_color ) &&
       ( s1->b_italic     == s2->b_italic ) &&
       ( s1->b_bold       == s2->b_bold ) &&
       ( s1->b_underline  == s2->b_underline ) &&
       ( !strcmp( s1->psz_fontname, s2->psz_fontname )))
    {
        return true;
    }
    return false;
}

static void IconvText( filter_t *p_filter, const char *psz_string,
                       uint32_t *i_string_length, uint32_t **ppsz_unicode )
{
    vlc_iconv_t iconv_handle = (vlc_iconv_t)(-1);

    /* If memory hasn't been allocated for our output string, allocate it here
     * - the calling function must now be responsible for freeing it.
     */
    if( !*ppsz_unicode )
        *ppsz_unicode = (uint32_t *)
            malloc( (strlen( psz_string ) + 1) * sizeof( uint32_t ));

    /* We don't need to handle a NULL pointer in *ppsz_unicode
     * if we are instead testing for a non NULL value like we are here */

    if( *ppsz_unicode )
    {
#if defined(WORDS_BIGENDIAN)
        iconv_handle = vlc_iconv_open( "UCS-4BE", "UTF-8" );
#else
        iconv_handle = vlc_iconv_open( "UCS-4LE", "UTF-8" );
#endif
        if( iconv_handle != (vlc_iconv_t)-1 )
        {
            char *p_in_buffer, *p_out_buffer;
            size_t i_in_bytes, i_out_bytes, i_out_bytes_left, i_ret;
            i_in_bytes = strlen( psz_string );
            i_out_bytes = i_in_bytes * sizeof( uint32_t );
            i_out_bytes_left = i_out_bytes;
            p_in_buffer = (char *) psz_string;
            p_out_buffer = (char *) *ppsz_unicode;
            i_ret = vlc_iconv( iconv_handle, (const char**)&p_in_buffer,
                    &i_in_bytes, &p_out_buffer, &i_out_bytes_left );

            vlc_iconv_close( iconv_handle );

            if( i_in_bytes )
            {
                msg_Warn( p_filter, "failed to convert string to unicode (%m), "
                          "bytes left %u", (unsigned)i_in_bytes );
            }
            else
            {
                *(uint32_t*)p_out_buffer = 0;
                *i_string_length =
                    (i_out_bytes - i_out_bytes_left) / sizeof(uint32_t);
            }
        }
        else
        {
            msg_Warn( p_filter, "unable to do conversion" );
        }
    }
}

static ft_style_t *GetStyleFromFontStack( filter_sys_t *p_sys,
        font_stack_t **p_fonts, bool b_bold, bool b_italic,
        bool b_uline )
{
    ft_style_t   *p_style = NULL;

    char       *psz_fontname = NULL;
    uint32_t    i_font_color = p_sys->i_font_color & 0x00ffffff;
    uint32_t    i_karaoke_bg_color = i_font_color;
    int         i_font_size  = p_sys->i_font_size;

    if( VLC_SUCCESS == PeekFont( p_fonts, &psz_fontname, &i_font_size,
                                 &i_font_color, &i_karaoke_bg_color ))
    {
        p_style = CreateStyle( psz_fontname, i_font_size, i_font_color,
                i_karaoke_bg_color, b_bold, b_italic, b_uline );
    }
    return p_style;
}

static int RenderTag( filter_t *p_filter, FT_Face p_face, int i_font_color,
                      bool b_uline, int i_karaoke_bgcolor,
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
    for( i=0; i<*pi_start; i++ )
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

        i_error = FT_Load_Glyph( p_face, i_glyph_index, FT_LOAD_DEFAULT );
        if( i_error )
        {
            msg_Err( p_filter,
                   "unable to render text FT_Load_Glyph returned %d", i_error );
            p_line->pp_glyphs[ i ] = NULL;
            return VLC_EGENERIC;
        }
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
        if( b_uline )
        {
            float aOffset = FT_FLOOR(FT_MulFix(p_face->underline_position,
                                               p_face->size->metrics.y_scale));
            float aSize = FT_CEIL(FT_MulFix(p_face->underline_thickness,
                                            p_face->size->metrics.y_scale));

            p_line->pi_underline_offset[ i ]  =
                                       ( aOffset < 0 ) ? -aOffset : aOffset;
            p_line->pi_underline_thickness[ i ] =
                                       ( aSize < 0 ) ? -aSize   : aSize;
        }
        p_line->pp_glyphs[ i ] = (FT_BitmapGlyph)tmp_glyph;
        p_line->p_fg_rgb[ i ] = i_font_color & 0x00ffffff;
        p_line->p_bg_rgb[ i ] = i_karaoke_bgcolor & 0x00ffffff;
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
                       uint32_t **psz_text_out, uint32_t *pi_runs,
                       uint32_t **ppi_run_lengths, ft_style_t ***ppp_styles,
                       ft_style_t *p_style )
{
    uint32_t      i_string_length = 0;

    IconvText( p_filter, psz_text_in, &i_string_length, psz_text_out );
    *psz_text_out += i_string_length;

    if( ppp_styles && ppi_run_lengths )
    {
        (*pi_runs)++;

        /* XXX this logic looks somewhat broken */

        if( *ppp_styles )
        {
            *ppp_styles = realloc_or_free( *ppp_styles,
                                          *pi_runs * sizeof( ft_style_t * ) );
        }
        else if( *pi_runs == 1 )
        {
            *ppp_styles = malloc( *pi_runs * sizeof( ft_style_t * ) );
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
                                              *pi_runs * sizeof( uint32_t ) );
        }
        else if( *pi_runs == 1 )
        {
            *ppi_run_lengths = (uint32_t *)
                malloc( *pi_runs * sizeof( uint32_t ) );
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
    if( p_style ) DeleteStyle( p_style );
}

static int CheckForEmbeddedFont( filter_sys_t *p_sys, FT_Face *pp_face, ft_style_t *p_style )
{
    int k;

    for( k=0; k < p_sys->i_font_attachments; k++ )
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
                bool match = !strcasecmp( p_face->family_name,
                                                p_style->psz_fontname );

                if( p_face->style_flags & FT_STYLE_FLAG_BOLD )
                    match = match && p_style->b_bold;
                else
                    match = match && !p_style->b_bold;

                if( p_face->style_flags & FT_STYLE_FLAG_ITALIC )
                    match = match && p_style->b_italic;
                else
                    match = match && !p_style->b_italic;

                if(  match )
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

static int ProcessLines( filter_t *p_filter,
                         uint32_t *psz_text,
                         int i_len,

                         uint32_t i_runs,
                         uint32_t *pi_run_lengths,
                         ft_style_t **pp_styles,
                         line_desc_t **pp_lines,

                         FT_Vector *p_result,

                         bool b_karaoke,
                         uint32_t i_k_runs,
                         uint32_t *pi_k_run_lengths,
                         uint32_t *pi_k_durations )
{
    filter_sys_t   *p_sys = p_filter->p_sys;
    ft_style_t    **pp_char_styles;
    int            *p_new_positions = NULL;
    int8_t         *p_levels = NULL;
    uint8_t        *pi_karaoke_bar = NULL;
    uint32_t        i, j, k;
    int             i_prev;

    /* Assign each character in the text string its style explicitly, so that
     * after the characters have been shuffled around by Fribidi, we can re-apply
     * the styles, and to simplify the calculation of runs within a line.
     */
    pp_char_styles = (ft_style_t **) malloc( i_len * sizeof( ft_style_t * ));
    if( !pp_char_styles )
        return VLC_ENOMEM;

    if( b_karaoke )
    {
        pi_karaoke_bar = (uint8_t *) malloc( i_len * sizeof( uint8_t ));
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
        ft_style_t  **pp_char_styles_new;
        int         *p_old_positions;
        uint32_t    *p_fribidi_string;
        int start_pos, pos = 0;

        pp_char_styles_new  = (ft_style_t **)
            malloc( i_len * sizeof( ft_style_t * ));

        p_fribidi_string = (uint32_t *)
            malloc( (i_len + 1) * sizeof(uint32_t) );
        p_old_positions = (int *)
            malloc( (i_len + 1) * sizeof( int ) );
        p_new_positions = (int *)
            malloc( (i_len + 1) * sizeof( int ) );
        p_levels = (int8_t *)
            malloc( (i_len + 1) * sizeof( int8_t ) );

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
                FriBidiCharType base_dir = FRIBIDI_TYPE_LTR;
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
            ft_style_t *p_style = pp_char_styles[ k - 1 ];

            /* End of the current style run */
            FT_Face p_face = NULL;
            int      i_idx = 0;

            /* Look for a match amongst our attachments first */
            CheckForEmbeddedFont( p_sys, &p_face, p_style );

            if( ! p_face )
            {
                char *psz_fontfile = NULL;

                psz_fontfile = FontConfig_Select( NULL,
                                                  p_style->psz_fontname,
                                                  p_style->b_bold,
                                                  p_style->b_italic,
                                                  &i_idx );
                if( psz_fontfile && ! *psz_fontfile )
                {
                    msg_Warn( p_filter, "Fontconfig was unable to find a font: \"%s\" %s"
                        " so using default font", p_style->psz_fontname,
                        ((p_style->b_bold && p_style->b_italic) ? "(Bold,Italic)" :
                                               (p_style->b_bold ? "(Bold)" :
                                             (p_style->b_italic ? "(Italic)" : ""))) );
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


            uint32_t *psz_unicode = (uint32_t *)
                              malloc( (k - i_prev + 1) * sizeof( uint32_t ));
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
                    /* New Color mode only works in YUVA rendering mode --
                     * (RGB mode has palette constraints on it). We therefore
                     * need to populate the legacy colour fields also.
                     */
                    p_line->b_new_color_mode = true;
                    p_line->i_alpha = ( p_style->i_font_color & 0xff000000 ) >> 24;
                    p_line->i_red   = ( p_style->i_font_color & 0x00ff0000 ) >> 16;
                    p_line->i_green = ( p_style->i_font_color & 0x0000ff00 ) >>  8;
                    p_line->i_blue  = ( p_style->i_font_color & 0x000000ff );
                    p_line->p_next = NULL;
                    i_pen_x = 0;
                    i_pen_y += tmp_result.y;
                    tmp_result.x = 0;
                    tmp_result.y = 0;
                    i_posn = 0;
                    if( p_prev ) p_prev->p_next = p_line;
                    else *pp_lines = p_line;
                }

                if( RenderTag( p_filter, p_face ? p_face : p_sys->p_face,
                               p_style->i_font_color, p_style->b_underline,
                               p_style->i_karaoke_bg_color,
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

static int RenderHtml( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in )
{
    int          rv = VLC_SUCCESS;
    stream_t     *p_sub = NULL;
    xml_reader_t *p_xml_reader = NULL;

    if( !p_region_in || !p_region_in->psz_html )
        return VLC_EGENERIC;

    /* Reset the default fontsize in case screen metrics have changed */
    p_filter->p_sys->i_font_size = GetFontSize( p_filter );

    p_sub = stream_MemoryNew( VLC_OBJECT(p_filter),
                              (uint8_t *) p_region_in->psz_html,
                              strlen( p_region_in->psz_html ),
                              true );
    if( p_sub )
    {
        if( !p_filter->p_sys->p_xml ) p_filter->p_sys->p_xml = xml_Create( p_filter );
        if( p_filter->p_sys->p_xml )
        {
            bool b_karaoke = false;

            p_xml_reader = xml_ReaderCreate( p_filter->p_sys->p_xml, p_sub );
            if( p_xml_reader )
            {
                /* Look for Root Node */
                if( xml_ReaderRead( p_xml_reader ) == 1 )
                {
                    char *psz_node = xml_ReaderName( p_xml_reader );

                    if( !strcasecmp( "karaoke", psz_node ) )
                    {
                        /* We're going to have to render the text a number
                         * of times to show the progress marker on the text.
                         */
                        var_SetBool( p_filter, "text-rerender", true );
                        b_karaoke = true;
                    }
                    else if( !strcasecmp( "text", psz_node ) )
                    {
                        b_karaoke = false;
                    }
                    else
                    {
                        /* Only text and karaoke tags are supported */
                        msg_Dbg( p_filter, "Unsupported top-level tag '%s' ignored.", psz_node );
                        xml_ReaderDelete( p_filter->p_sys->p_xml, p_xml_reader );
                        p_xml_reader = NULL;
                        rv = VLC_EGENERIC;
                    }

                    free( psz_node );
                }
            }

            if( p_xml_reader )
            {
                uint32_t   *psz_text;
                int         i_len = 0;
                uint32_t    i_runs = 0;
                uint32_t    i_k_runs = 0;
                uint32_t   *pi_run_lengths = NULL;
                uint32_t   *pi_k_run_lengths = NULL;
                uint32_t   *pi_k_durations = NULL;
                ft_style_t  **pp_styles = NULL;
                FT_Vector    result;
                line_desc_t  *p_lines = NULL;

                psz_text = (uint32_t *)malloc( strlen( p_region_in->psz_html ) *
                                                sizeof( uint32_t ) );
                if( psz_text )
                {
                    uint32_t k;

                    rv = ProcessNodes( p_filter, p_xml_reader,
                                  p_region_in->p_style, psz_text, &i_len,
                                  &i_runs, &pi_run_lengths, &pp_styles,

                                  b_karaoke, &i_k_runs, &pi_k_run_lengths,
                                  &pi_k_durations );

                    p_region_out->i_x = p_region_in->i_x;
                    p_region_out->i_y = p_region_in->i_y;

                    if(( rv == VLC_SUCCESS ) && ( i_len > 0 ))
                    {
                        rv = ProcessLines( p_filter, psz_text, i_len, i_runs,
                                pi_run_lengths, pp_styles, &p_lines, &result,
                                b_karaoke, i_k_runs, pi_k_run_lengths,
                                pi_k_durations );
                    }

                    for( k=0; k<i_runs; k++)
                        DeleteStyle( pp_styles[k] );
                    free( pp_styles );
                    free( pi_run_lengths );
                    free( psz_text );

                    /* Don't attempt to render text that couldn't be layed out
                     * properly.
                     */
                    if(( rv == VLC_SUCCESS ) && ( i_len > 0 ))
                    {
                        if( config_GetInt( p_filter, "freetype-yuvp" ) )
                        {
                            Render( p_filter, p_region_out, p_lines,
                                    result.x, result.y );
                        }
                        else
                        {
                            RenderYUVA( p_filter, p_region_out, p_lines,
                                    result.x, result.y );
                        }
                    }
                }
                FreeLines( p_lines );

                xml_ReaderDelete( p_filter->p_sys->p_xml, p_xml_reader );
            }
        }
        stream_Delete( p_sub );
    }

    return rv;
}

static char* FontConfig_Select( FcConfig* priv, const char* family,
                          bool b_bold, bool b_italic, int *i_idx )
{
    FcResult result;
    FcPattern *pat, *p_pat;
    FcChar8* val_s;
    FcBool val_b;

    pat = FcPatternCreate();
    if (!pat) return NULL;

    FcPatternAddString( pat, FC_FAMILY, (const FcChar8*)family );
    FcPatternAddBool( pat, FC_OUTLINE, FcTrue );
    FcPatternAddInteger( pat, FC_SLANT, b_italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN );
    FcPatternAddInteger( pat, FC_WEIGHT, b_bold ? FC_WEIGHT_EXTRABOLD : FC_WEIGHT_NORMAL );

    FcDefaultSubstitute( pat );

    if( !FcConfigSubstitute( priv, pat, FcMatchPattern ) )
    {
        FcPatternDestroy( pat );
        return NULL;
    }

    p_pat = FcFontMatch( priv, pat, &result );
    FcPatternDestroy( pat );
    if( !p_pat ) return NULL;

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

    /*
    if( strcasecmp((const char*)val_s, family ) != 0 )
        msg_Warn( p_filter, "fontconfig: selected font family is not"
                            "the requested one: '%s' != '%s'\n",
                            (const char*)val_s, family );
    */

    if( FcResultMatch != FcPatternGetString( p_pat, FC_FILE, 0, &val_s ) )
    {
        FcPatternDestroy( p_pat );
        return NULL;
    }

    FcPatternDestroy( p_pat );
    return strdup( (const char*)val_s );
}
#else

static void SetupLine( filter_t *p_filter, const char *psz_text_in,
                       uint32_t **psz_text_out, uint32_t *pi_runs,
                       uint32_t **ppi_run_lengths, ft_style_t ***ppp_styles,
                       ft_style_t *p_style )
{
        VLC_UNUSED(p_filter);
        VLC_UNUSED(psz_text_in);
        VLC_UNUSED(psz_text_out);
        VLC_UNUSED(pi_runs);
        VLC_UNUSED(ppi_run_lengths);
        VLC_UNUSED(ppp_styles);
        VLC_UNUSED(p_style);
}

static ft_style_t *GetStyleFromFontStack( filter_sys_t *p_sys,
        font_stack_t **p_fonts, bool b_bold, bool b_italic,
        bool b_uline )
{
        VLC_UNUSED(p_sys);
        VLC_UNUSED(p_fonts);
        VLC_UNUSED(b_bold);
        VLC_UNUSED(b_italic);
        VLC_UNUSED(b_uline);
        return NULL;
}
#endif

static void FreeLine( line_desc_t *p_line )
{
    unsigned int i;
    for( i = 0; p_line->pp_glyphs[ i ] != NULL; i++ )
    {
        FT_Done_Glyph( (FT_Glyph)p_line->pp_glyphs[ i ] );
    }
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
    line_desc_t *p_line, *p_next;

    if( !p_lines ) return;

    for( p_line = p_lines; p_line != NULL; p_line = p_next )
    {
        p_next = p_line->p_next;
        FreeLine( p_line );
    }
}

static line_desc_t *NewLine( int i_count )
{
    line_desc_t *p_line = malloc( sizeof(line_desc_t) );

    if( !p_line ) return NULL;
    p_line->i_height = 0;
    p_line->i_width = 0;
    p_line->p_next = NULL;

    p_line->pp_glyphs = malloc( sizeof(FT_BitmapGlyph) * ( i_count + 1 ) );
    p_line->p_glyph_pos = malloc( sizeof( FT_Vector ) * ( i_count + 1 ) );
    p_line->p_fg_rgb = malloc( sizeof( uint32_t ) * ( i_count + 1 ) );
    p_line->p_bg_rgb = malloc( sizeof( uint32_t ) * ( i_count + 1 ) );
    p_line->p_fg_bg_ratio = calloc( i_count + 1, sizeof( uint8_t ) );
    p_line->pi_underline_offset = calloc( i_count + 1, sizeof( uint16_t ) );
    p_line->pi_underline_thickness = calloc( i_count + 1, sizeof( uint16_t ) );
    if( ( p_line->pp_glyphs == NULL ) ||
        ( p_line->p_glyph_pos == NULL ) ||
        ( p_line->p_fg_rgb == NULL ) ||
        ( p_line->p_bg_rgb == NULL ) ||
        ( p_line->p_fg_bg_ratio == NULL ) ||
        ( p_line->pi_underline_offset == NULL ) ||
        ( p_line->pi_underline_thickness == NULL ) )
    {
        free( p_line->pi_underline_thickness );
        free( p_line->pi_underline_offset );
        free( p_line->p_fg_rgb );
        free( p_line->p_bg_rgb );
        free( p_line->p_fg_bg_ratio );
        free( p_line->p_glyph_pos );
        free( p_line->pp_glyphs );
        free( p_line );
        return NULL;
    }
    p_line->pp_glyphs[0] = NULL;
    p_line->b_new_color_mode = false;

    return p_line;
}

static int GetFontSize( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_value_t   val;
    int           i_size = 0;

    if( p_sys->i_default_font_size )
    {
        if( VLC_SUCCESS == var_Get( p_filter, "scale", &val ))
            i_size = p_sys->i_default_font_size * val.i_int / 1000;
        else
            i_size = p_sys->i_default_font_size;
    }
    else
    {
        var_Get( p_filter, "freetype-rel-fontsize", &val );
        if( val.i_int  > 0 )
        {
            i_size = (int)p_filter->fmt_out.video.i_height / val.i_int;
            p_filter->p_sys->i_display_height =
                p_filter->fmt_out.video.i_height;
        }
    }
    if( i_size <= 0 )
    {
        msg_Warn( p_filter, "invalid fontsize, using 12" );
        if( VLC_SUCCESS == var_Get( p_filter, "scale", &val ))
            i_size = 12 * val.i_int / 1000;
        else
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
