/*****************************************************************************
 * quartztext.c : Put text on the video, using Mac OS X Quartz Engine
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Bernie Purcell <bitmap@videolan.org>
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

//////////////////////////////////////////////////////////////////////////////
// Preamble
//////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_osd.h>
#include <vlc_block.h>
#include <vlc_filter.h>
#include <vlc_stream.h>
#include <vlc_xml.h>
#include <vlc_input.h>
#include <vlc_strings.h>

#include <math.h>

#include <Carbon/Carbon.h>

#define DEFAULT_FONT           "Arial Black"
#define DEFAULT_FONT_COLOR     0xffffff
#define DEFAULT_REL_FONT_SIZE  16

#define VERTICAL_MARGIN 3
#define HORIZONTAL_MARGIN 10

//////////////////////////////////////////////////////////////////////////////
// Local prototypes
//////////////////////////////////////////////////////////////////////////////
static int  Create ( vlc_object_t * );
static void Destroy( vlc_object_t * );

static int LoadFontsFromAttachments( filter_t *p_filter );

static int RenderText( filter_t *, subpicture_region_t *,
                       subpicture_region_t * );
static int RenderHtml( filter_t *, subpicture_region_t *,
                       subpicture_region_t * );

static int GetFontSize( filter_t *p_filter );
static int RenderYUVA( filter_t *p_filter, subpicture_region_t *p_region,
                       UniChar *psz_utfString, uint32_t i_text_len,
                       uint32_t i_runs, uint32_t *pi_run_lengths,
                       ATSUStyle *pp_styles );
static ATSUStyle CreateStyle( char *psz_fontname, int i_font_size,
                              uint32_t i_font_color,
                              bool b_bold, bool b_italic,
                              bool b_uline );
//////////////////////////////////////////////////////////////////////////////
// Module descriptor
//////////////////////////////////////////////////////////////////////////////

// The preferred way to set font style information is for it to come from the
// subtitle file, and for it to be rendered with RenderHtml instead of
// RenderText. This module, unlike Freetype, doesn't provide any options to
// override the fallback font selection used when this style information is
// absent.
#define FONT_TEXT N_("Font")
#define FONT_LONGTEXT N_("Name for the font you want to use")
#define FONTSIZER_TEXT N_("Relative font size")
#define FONTSIZER_LONGTEXT N_("This is the relative default size of the " \
    "fonts that will be rendered on the video. If absolute font size is set, "\
    "relative size will be overriden." )
#define COLOR_TEXT N_("Text default color")
#define COLOR_LONGTEXT N_("The color of the text that will be rendered on "\
    "the video. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white" )

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
    set_shortname( N_("Mac Text renderer"))
    set_description( N_("Quartz font renderer") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SUBPIC )

    add_string( "quartztext-font", DEFAULT_FONT, NULL, FONT_TEXT, FONT_LONGTEXT,
              false )
    add_integer( "quartztext-rel-fontsize", DEFAULT_REL_FONT_SIZE, NULL, FONTSIZER_TEXT,
                 FONTSIZER_LONGTEXT, false )
        change_integer_list( pi_sizes, ppsz_sizes_text, NULL );
    add_integer( "quartztext-color", 0x00FFFFFF, NULL, COLOR_TEXT,
                 COLOR_LONGTEXT, false )
        change_integer_list( pi_color_values, ppsz_color_descriptions, NULL );
    set_capability( "text renderer", 150 )
    add_shortcut( "text" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

typedef struct offscreen_bitmap_t offscreen_bitmap_t;
struct offscreen_bitmap_t
{
    uint8_t       *p_data;
    int            i_bitsPerChannel;
    int            i_bitsPerPixel;
    int            i_bytesPerPixel;
    int            i_bytesPerRow;
};

//////////////////////////////////////////////////////////////////////////////
// filter_sys_t: quartztext local data
//////////////////////////////////////////////////////////////////////////////
// This structure is part of the video output thread descriptor.
// It describes the freetype specific properties of an output thread.
//////////////////////////////////////////////////////////////////////////////
struct filter_sys_t
{
    char          *psz_font_name;
    uint8_t        i_font_opacity;
    int            i_font_color;
    int            i_font_size;

    ATSFontContainerRef    *p_fonts;
    int                     i_fonts;
};

#define UCHAR UniChar
#define TR_DEFAULT_FONT p_sys->psz_font_name
#define TR_FONT_STYLE_PTR ATSUStyle

#include "text_renderer.h"

//////////////////////////////////////////////////////////////////////////////
// Create: allocates osd-text video thread output method
//////////////////////////////////////////////////////////////////////////////
// This function allocates and initializes a Clone vout method.
//////////////////////////////////////////////////////////////////////////////
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    // Allocate structure
    p_filter->p_sys = p_sys = malloc( sizeof( filter_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->psz_font_name  = var_CreateGetString( p_this, "quartztext-font" );
    p_sys->i_font_opacity = 255;
    p_sys->i_font_color = __MAX( __MIN( var_CreateGetInteger( p_this, "quartztext-color" ) , 0xFFFFFF ), 0 );
    p_sys->i_font_size    = GetFontSize( p_filter );

    p_filter->pf_render_text = RenderText;
    p_filter->pf_render_html = RenderHtml;

    p_sys->p_fonts = NULL;
    p_sys->i_fonts = 0;

    LoadFontsFromAttachments( p_filter );

    return VLC_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
// Destroy: destroy Clone video thread output method
//////////////////////////////////////////////////////////////////////////////
// Clean up all data and library connections
//////////////////////////////////////////////////////////////////////////////
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->p_fonts )
    {
        int   k;

        for( k = 0; k < p_sys->i_fonts; k++ )
        {
            ATSFontDeactivate( p_sys->p_fonts[k], NULL, kATSOptionFlagsDefault );
        }

        free( p_sys->p_fonts );
    }

    free( p_sys->psz_font_name );
    free( p_sys );
}

//////////////////////////////////////////////////////////////////////////////
// Make any TTF/OTF fonts present in the attachments of the media file
// available to the Quartz engine for text rendering
//////////////////////////////////////////////////////////////////////////////
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

    p_sys->i_fonts = 0;
    p_sys->p_fonts = malloc( i_attachments_cnt * sizeof( ATSFontContainerRef ) );
    if(! p_sys->p_fonts )
        rv = VLC_ENOMEM;

    for( k = 0; k < i_attachments_cnt; k++ )
    {
        input_attachment_t *p_attach = pp_attachments[k];

        if( p_sys->p_fonts )
        {
            if(( !strcmp( p_attach->psz_mime, "application/x-truetype-font" ) || // TTF
                 !strcmp( p_attach->psz_mime, "application/x-font-otf" ) ) &&    // OTF
               ( p_attach->i_data > 0 ) &&
               ( p_attach->p_data != NULL ) )
            {
                ATSFontContainerRef  container;

                if( noErr == ATSFontActivateFromMemory( p_attach->p_data,
                                                        p_attach->i_data,
                                                        kATSFontContextLocal,
                                                        kATSFontFormatUnspecified,
                                                        NULL,
                                                        kATSOptionFlagsDefault,
                                                        &container ))
                {
                    p_sys->p_fonts[ p_sys->i_fonts++ ] = container;
                }
            }
        }
        vlc_input_attachment_Delete( p_attach );
    }
    free( pp_attachments );

    vlc_object_release(p_input);

    return rv;
}

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_4
// Original version of these functions available on:
// http://developer.apple.com/documentation/Carbon/Conceptual/QuickDrawToQuartz2D/tq_color/chapter_4_section_3.html

#define kGenericRGBProfilePathStr "/System/Library/ColorSync/Profiles/Generic RGB Profile.icc"

static CMProfileRef OpenGenericProfile( void )
{
    static CMProfileRef cached_rgb_prof = NULL;

    // Create the profile reference only once
    if( cached_rgb_prof == NULL )
    {
        OSStatus            err;
        CMProfileLocation   loc;

        loc.locType = cmPathBasedProfile;
        strcpy( loc.u.pathLoc.path, kGenericRGBProfilePathStr );

        err = CMOpenProfile( &cached_rgb_prof, &loc );

        if( err != noErr )
        {
            cached_rgb_prof = NULL;
        }
    }

    if( cached_rgb_prof )
    {
        // Clone the profile reference so that the caller has
        // their own reference, not our cached one.
        CMCloneProfileRef( cached_rgb_prof );
    }

    return cached_rgb_prof;
}

static CGColorSpaceRef CreateGenericRGBColorSpace( void )
{
    static CGColorSpaceRef p_generic_rgb_cs = NULL;

    if( p_generic_rgb_cs == NULL )
    {
        CMProfileRef generic_rgb_prof = OpenGenericProfile();

        if( generic_rgb_prof )
        {
            p_generic_rgb_cs = CGColorSpaceCreateWithPlatformColorSpace( generic_rgb_prof );

            CMCloseProfile( generic_rgb_prof );
        }
    }

    return p_generic_rgb_cs;
}
#endif

static char *EliminateCRLF( char *psz_string )
{
    char *p;
    char *q;

    for( p = psz_string; p && *p; p++ )
    {
        if( ( *p == '\r' ) && ( *(p+1) == '\n' ) )
        {
            for( q = p + 1; *q; q++ )
                *( q - 1 ) = *q;

            *( q - 1 ) = '\0';
        }
    }
    return psz_string;
}

// Convert UTF-8 string to UTF-16 character array -- internal Mac Endian-ness ;
// we don't need to worry about bidirectional text conversion as ATSUI should
// handle that for us automatically
static void ConvertToUTF16( const char *psz_utf8_str, uint32_t *pi_strlen, UniChar **ppsz_utf16_str )
{
    CFStringRef   p_cfString;
    int           i_string_length;

    p_cfString = CFStringCreateWithCString( NULL, psz_utf8_str, kCFStringEncodingUTF8 );
    if( !p_cfString )
        return;

    i_string_length = CFStringGetLength( p_cfString );

    if( pi_strlen )
        *pi_strlen = i_string_length;

    if( !*ppsz_utf16_str )
        *ppsz_utf16_str = (UniChar *) calloc( i_string_length, sizeof( UniChar ) );

    CFStringGetCharacters( p_cfString, CFRangeMake( 0, i_string_length ), *ppsz_utf16_str );

    CFRelease( p_cfString );
}

// Renders a text subpicture region into another one.
// It is used as pf_add_string callback in the vout method by this module
static int RenderText( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    UniChar      *psz_utf16_str = NULL;
    uint32_t      i_string_length;
    char         *psz_string;
    int           i_font_color, i_font_alpha, i_font_size;
    vlc_value_t val;
    int i_scale = 1000;

    p_sys->i_font_size    = GetFontSize( p_filter );

    // Sanity check
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
        i_font_size  = p_sys->i_font_size;
    }

    if( !i_font_alpha ) i_font_alpha = 255 - p_sys->i_font_opacity;

    ConvertToUTF16( EliminateCRLF( psz_string ), &i_string_length, &psz_utf16_str );

    p_region_out->i_x = p_region_in->i_x;
    p_region_out->i_y = p_region_in->i_y;

    if( psz_utf16_str != NULL )
    {
        ATSUStyle p_style = CreateStyle( p_sys->psz_font_name, i_font_size,
                                         (i_font_color & 0xffffff) |
                                         ((i_font_alpha & 0xff) << 24),
                                         false, false, false );
        if( p_style )
        {
            RenderYUVA( p_filter, p_region_out, psz_utf16_str, i_string_length,
                        1, &i_string_length, &p_style );
        }

        ATSUDisposeStyle( p_style );
        free( psz_utf16_str );
    }

    return VLC_SUCCESS;
}


static ATSUStyle CreateStyle( char *psz_fontname, int i_font_size, uint32_t i_font_color,
                              bool b_bold, bool b_italic, bool b_uline )
{
    ATSUStyle   p_style;
    OSStatus    status;
    uint32_t    i_tag_cnt;

    float f_red   = (float)(( i_font_color & 0x00FF0000 ) >> 16) / 255.0;
    float f_green = (float)(( i_font_color & 0x0000FF00 ) >>  8) / 255.0;
    float f_blue  = (float)(  i_font_color & 0x000000FF        ) / 255.0;
    float f_alpha = ( 255.0 - (float)(( i_font_color & 0xFF000000 ) >> 24)) / 255.0;

    ATSUFontID           font;
    Fixed                font_size  = IntToFixed( i_font_size );
    ATSURGBAlphaColor    font_color = { f_red, f_green, f_blue, f_alpha };
    Boolean              bold       = b_bold;
    Boolean              italic     = b_italic;
    Boolean              uline      = b_uline;

    ATSUAttributeTag tags[]        = { kATSUSizeTag, kATSURGBAlphaColorTag, kATSUQDItalicTag,
                                       kATSUQDBoldfaceTag, kATSUQDUnderlineTag, kATSUFontTag };
    ByteCount sizes[]              = { sizeof( Fixed ), sizeof( ATSURGBAlphaColor ), sizeof( Boolean ),
                                       sizeof( Boolean ), sizeof( Boolean ), sizeof( ATSUFontID )};
    ATSUAttributeValuePtr values[] = { &font_size, &font_color, &italic, &bold, &uline, &font };

    i_tag_cnt = sizeof( tags ) / sizeof( ATSUAttributeTag );

    status = ATSUFindFontFromName( psz_fontname,
                                   strlen( psz_fontname ),
                                   kFontFullName,
                                   kFontNoPlatform,
                                   kFontNoScript,
                                   kFontNoLanguageCode,
                                   &font );

    if( status != noErr )
    {
        // If we can't find a suitable font, just do everything else
        i_tag_cnt--;
    }

    if( noErr == ATSUCreateStyle( &p_style ) )
    {
        if( noErr == ATSUSetAttributes( p_style, i_tag_cnt, tags, sizes, values ) )
        {
            return p_style;
        }
        ATSUDisposeStyle( p_style );
    }
    return NULL;
}

static ATSUStyle GetStyleFromFontStack( filter_sys_t *p_sys,
        font_stack_t **p_fonts, bool b_bold, bool b_italic,
        bool b_uline )
{
    ATSUStyle   p_style = NULL;

    char     *psz_fontname = NULL;
    uint32_t  i_font_color = p_sys->i_font_color;
    uint32_t  i_karaoke_bg_color = i_font_color; /* Use it */
    int       i_font_size  = p_sys->i_font_size;

    if( VLC_SUCCESS == PeekFont( p_fonts, &psz_fontname, &i_font_size,
                                 &i_font_color, &i_karaoke_bg_color ))
    {
        p_style = CreateStyle( psz_fontname, i_font_size, i_font_color,
                               b_bold, b_italic, b_uline );
    }
    return p_style;
}

static void SetupLine( filter_t *p_filter, const char *psz_text_in,
                       UniChar **psz_text_out, uint32_t *pi_runs,
                       uint32_t **ppi_run_lengths, ATSUStyle **ppp_styles,
                       ATSUStyle p_style )
{
    uint32_t i_string_length = 0;

    ConvertToUTF16( psz_text_in, &i_string_length, psz_text_out );
    *psz_text_out += i_string_length;

    if( ppp_styles && ppi_run_lengths )
    {
        (*pi_runs)++;

        if( *ppp_styles )
            *ppp_styles = (ATSUStyle *) realloc( *ppp_styles, *pi_runs * sizeof( ATSUStyle ) );
        else
            *ppp_styles = (ATSUStyle *) malloc( *pi_runs * sizeof( ATSUStyle ) );

        (*ppp_styles)[ *pi_runs - 1 ] = p_style;

        if( *ppi_run_lengths )
            *ppi_run_lengths = (uint32_t *) realloc( *ppi_run_lengths, *pi_runs * sizeof( uint32_t ) );
        else
            *ppi_run_lengths = (uint32_t *) malloc( *pi_runs * sizeof( uint32_t ) );

        (*ppi_run_lengths)[ *pi_runs - 1 ] = i_string_length;
    }
}

static int RenderHtml( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in )
{
    int          rv = VLC_SUCCESS;
    stream_t     *p_sub = NULL;
    xml_t        *p_xml = NULL;
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
        p_xml = xml_Create( p_filter );
        if( p_xml )
        {
            bool b_karaoke = false;

            p_xml_reader = xml_ReaderCreate( p_xml, p_sub );
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
                        xml_ReaderDelete( p_xml, p_xml_reader );
                        p_xml_reader = NULL;
                        rv = VLC_EGENERIC;
                    }

                    free( psz_node );
                }
            }

            if( p_xml_reader )
            {
                UniChar    *psz_text;
                int         i_len = 0;
                uint32_t    i_runs = 0;
                uint32_t    i_k_runs = 0;
                uint32_t   *pi_run_lengths = NULL;
                uint32_t   *pi_k_run_lengths = NULL;
                uint32_t   *pi_k_durations = NULL;
                ATSUStyle  *pp_styles = NULL;

                psz_text = (UniChar *) malloc( strlen( p_region_in->psz_html ) *
                                                sizeof( UniChar ) );
                if( psz_text )
                {
                    uint32_t k;

                    rv = ProcessNodes( p_filter, p_xml_reader,
                                  p_region_in->p_style, psz_text, &i_len,
                                  &i_runs, &pi_run_lengths, &pp_styles,
                                  /* No karaoke support */
                                  false, &i_k_runs, &pi_k_run_lengths, &pi_k_durations );

                    assert( pi_k_run_lengths == NULL && pi_k_durations == NULL );

                    p_region_out->i_x = p_region_in->i_x;
                    p_region_out->i_y = p_region_in->i_y;

                    if(( rv == VLC_SUCCESS ) && ( i_len > 0 ))
                    {
                        RenderYUVA( p_filter, p_region_out, psz_text, i_len, i_runs,
                             pi_run_lengths, pp_styles);
                    }

                    for( k=0; k<i_runs; k++)
                        ATSUDisposeStyle( pp_styles[k] );
                    free( pp_styles );
                    free( pi_run_lengths );
                    free( psz_text );
                }

                xml_ReaderDelete( p_xml, p_xml_reader );
            }
            xml_Delete( p_xml );
        }
        stream_Delete( p_sub );
    }

    return rv;
}

static CGContextRef CreateOffScreenContext( int i_width, int i_height,
                         offscreen_bitmap_t **pp_memory, CGColorSpaceRef *pp_colorSpace )
{
    offscreen_bitmap_t *p_bitmap;
    CGContextRef        p_context = NULL;

    p_bitmap = (offscreen_bitmap_t *) malloc( sizeof( offscreen_bitmap_t ));
    if( p_bitmap )
    {
        p_bitmap->i_bitsPerChannel = 8;
        p_bitmap->i_bitsPerPixel   = 4 * p_bitmap->i_bitsPerChannel; // A,R,G,B
        p_bitmap->i_bytesPerPixel  = p_bitmap->i_bitsPerPixel / 8;
        p_bitmap->i_bytesPerRow    = i_width * p_bitmap->i_bytesPerPixel;

        p_bitmap->p_data = calloc( i_height, p_bitmap->i_bytesPerRow );

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4
        *pp_colorSpace = CGColorSpaceCreateWithName( kCGColorSpaceGenericRGB );
#else
        *pp_colorSpace = CreateGenericRGBColorSpace();
#endif

        if( p_bitmap->p_data && *pp_colorSpace )
        {
            p_context = CGBitmapContextCreate( p_bitmap->p_data, i_width, i_height,
                                p_bitmap->i_bitsPerChannel, p_bitmap->i_bytesPerRow,
                                *pp_colorSpace, kCGImageAlphaPremultipliedFirst);
        }
        if( p_context )
        {
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_1
            // OS X 10.1 doesn't support weak linking of this call which is only available
            // int 10.4 and later
            if( CGContextSetAllowsAntialiasing != NULL )
            {
                CGContextSetAllowsAntialiasing( p_context, true );
            }
#endif
        }
        *pp_memory = p_bitmap;
    }

    return p_context;
}

static offscreen_bitmap_t *Compose( int i_text_align, UniChar *psz_utf16_str, uint32_t i_text_len,
                                    uint32_t i_runs, uint32_t *pi_run_lengths, ATSUStyle *pp_styles,
                                    int i_width, int i_height, int *pi_textblock_height )
{
    offscreen_bitmap_t  *p_offScreen  = NULL;
    CGColorSpaceRef      p_colorSpace = NULL;
    CGContextRef         p_context = NULL;

    p_context = CreateOffScreenContext( i_width, i_height, &p_offScreen, &p_colorSpace );

    if( p_context )
    {
        ATSUTextLayout p_textLayout;
        OSStatus status = noErr;

        status = ATSUCreateTextLayoutWithTextPtr( psz_utf16_str, 0, i_text_len, i_text_len,
                                                  i_runs,
                                                  (const UniCharCount *) pi_run_lengths,
                                                  pp_styles,
                                                  &p_textLayout );
        if( status == noErr )
        {
            // Attach our offscreen Image Graphics Context to the text style
            // and setup the line alignment (have to specify the line width
            // also in order for our chosen alignment to work)

            Fract   alignment  = kATSUStartAlignment;
            Fixed   line_width = Long2Fix( i_width - HORIZONTAL_MARGIN * 2 );

            ATSUAttributeTag tags[]        = { kATSUCGContextTag, kATSULineFlushFactorTag, kATSULineWidthTag };
            ByteCount sizes[]              = { sizeof( CGContextRef ), sizeof( Fract ), sizeof( Fixed ) };
            ATSUAttributeValuePtr values[] = { &p_context, &alignment, &line_width };

            int i_tag_cnt = sizeof( tags ) / sizeof( ATSUAttributeTag );

            if( i_text_align == SUBPICTURE_ALIGN_RIGHT )
            {
                alignment = kATSUEndAlignment;
            }
            else if( i_text_align != SUBPICTURE_ALIGN_LEFT )
            {
                alignment = kATSUCenterAlignment;
            }

            ATSUSetLayoutControls( p_textLayout, i_tag_cnt, tags, sizes, values );

            // let ATSUI deal with characters not-in-our-specified-font
            ATSUSetTransientFontMatching( p_textLayout, true );

            Fixed x = Long2Fix( HORIZONTAL_MARGIN );
            Fixed y = Long2Fix( i_height );

            // Set the line-breaks and draw individual lines
            uint32_t i_start = 0;
            uint32_t i_end = i_text_len;

            // Set up black outlining of the text --
            CGContextSetRGBStrokeColor( p_context, 0, 0, 0, 0.5 );
            CGContextSetTextDrawingMode( p_context, kCGTextFillStroke );
            CGContextSetShadow( p_context, CGSizeMake( 0, 0 ), 5 );
            float black_components[4] = {0, 0, 0, 1};
            CGContextSetShadowWithColor (p_context, CGSizeMake( 0, 0 ), 5, CGColorCreate( CGColorSpaceCreateWithName( kCGColorSpaceGenericRGB ), black_components ));
            do
            {
                // ATSUBreakLine will automatically pick up any manual '\n's also
                status = ATSUBreakLine( p_textLayout, i_start, line_width, true, (UniCharArrayOffset *) &i_end );
                if( ( status == noErr ) || ( status == kATSULineBreakInWord ) )
                {
                    Fixed     ascent;
                    Fixed     descent;
                    uint32_t  i_actualSize;

                    // Come down far enough to fit the height of this line --
                    ATSUGetLineControl( p_textLayout, i_start, kATSULineAscentTag,
                                    sizeof( Fixed ), &ascent, (ByteCount *) &i_actualSize );

                    // Quartz uses an upside-down co-ordinate space -> y values decrease as
                    // you move down the page
                    y -= ascent;

                    // Set the outlining for this line to be dependent on the size of the line -
                    // make it about 5% of the ascent, with a minimum at 1.0
                    float f_thickness = FixedToFloat( ascent ) * 0.05;
                    CGContextSetLineWidth( p_context, (( f_thickness < 1.0 ) ? 1.0 : f_thickness ));
                    ATSUDrawText( p_textLayout, i_start, i_end - i_start, x, y );

                    // and now prepare for the next line by coming down far enough for our
                    // descent
                    ATSUGetLineControl( p_textLayout, i_start, kATSULineDescentTag,
                                    sizeof( Fixed ), &descent, (ByteCount *) &i_actualSize );
                    y -= descent;

                    i_start = i_end;
                }
                else
                    break;
            }
            while( i_end < i_text_len );

            *pi_textblock_height = i_height - Fix2Long( y );
            CGContextFlush( p_context );

            ATSUDisposeTextLayout( p_textLayout );
        }

        CGContextRelease( p_context );
    }
    if( p_colorSpace ) CGColorSpaceRelease( p_colorSpace );

    return p_offScreen;
}

static int GetFontSize( filter_t *p_filter )
{
    return p_filter->fmt_out.video.i_height / __MAX(1, var_CreateGetInteger( p_filter, "quartztext-rel-fontsize" ));
}

static int RenderYUVA( filter_t *p_filter, subpicture_region_t *p_region, UniChar *psz_utf16_str,
                       uint32_t i_text_len, uint32_t i_runs, uint32_t *pi_run_lengths, ATSUStyle *pp_styles )
{
    offscreen_bitmap_t *p_offScreen = NULL;
    int      i_textblock_height = 0;

    int i_width = p_filter->fmt_out.video.i_visible_width;
    int i_height = p_filter->fmt_out.video.i_visible_height;
    int i_text_align = p_region->i_align & 0x3;

    if( !psz_utf16_str )
    {
        msg_Err( p_filter, "Invalid argument to RenderYUVA" );
        return VLC_EGENERIC;
    }

    p_offScreen = Compose( i_text_align, psz_utf16_str, i_text_len,
                           i_runs, pi_run_lengths, pp_styles,
                           i_width, i_height, &i_textblock_height );

    if( !p_offScreen )
    {
        msg_Err( p_filter, "No offscreen buffer" );
        return VLC_EGENERIC;
    }

    uint8_t *p_dst_y,*p_dst_u,*p_dst_v,*p_dst_a;
    video_format_t fmt;
    int x, y, i_offset, i_pitch;
    uint8_t i_y, i_u, i_v; // YUV values, derived from incoming RGB

    // Create a new subpicture region
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('Y','U','V','A');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_visible_width = i_width;
    fmt.i_height = fmt.i_visible_height = i_textblock_height + VERTICAL_MARGIN * 2;
    fmt.i_x_offset = fmt.i_y_offset = 0;

    p_region->p_picture = picture_New( fmt.i_chroma, fmt.i_width, fmt.i_height, fmt.i_aspect );
    if( !p_region->p_picture )
        return VLC_EGENERIC;
    p_region->fmt = fmt;

    p_dst_y = p_region->p_picture->Y_PIXELS;
    p_dst_u = p_region->p_picture->U_PIXELS;
    p_dst_v = p_region->p_picture->V_PIXELS;
    p_dst_a = p_region->p_picture->A_PIXELS;
    i_pitch = p_region->p_picture->A_PITCH;

    i_offset = VERTICAL_MARGIN *i_pitch;
    for( y=0; y<i_textblock_height; y++)
    {
        for( x=0; x<i_width; x++)
        {
            int i_alpha = p_offScreen->p_data[ y * p_offScreen->i_bytesPerRow + x * p_offScreen->i_bytesPerPixel     ];
            int i_red   = p_offScreen->p_data[ y * p_offScreen->i_bytesPerRow + x * p_offScreen->i_bytesPerPixel + 1 ];
            int i_green = p_offScreen->p_data[ y * p_offScreen->i_bytesPerRow + x * p_offScreen->i_bytesPerPixel + 2 ];
            int i_blue  = p_offScreen->p_data[ y * p_offScreen->i_bytesPerRow + x * p_offScreen->i_bytesPerPixel + 3 ];

            i_y = (uint8_t)__MIN(abs( 2104 * i_red  + 4130 * i_green +
                              802 * i_blue + 4096 + 131072 ) >> 13, 235);
            i_u = (uint8_t)__MIN(abs( -1214 * i_red  + -2384 * i_green +
                             3598 * i_blue + 4096 + 1048576) >> 13, 240);
            i_v = (uint8_t)__MIN(abs( 3598 * i_red + -3013 * i_green +
                              -585 * i_blue + 4096 + 1048576) >> 13, 240);

            p_dst_y[ i_offset + x ] = i_y;
            p_dst_u[ i_offset + x ] = i_u;
            p_dst_v[ i_offset + x ] = i_v;
            p_dst_a[ i_offset + x ] = i_alpha;
        }
        i_offset += i_pitch;
    }

    free( p_offScreen->p_data );
    free( p_offScreen );

    return VLC_SUCCESS;
}
