/*****************************************************************************
 * subsdec.c : text subtitles decoder
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Bernie Purcell <b dot purcell at adbglobal dot com>
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
#include <vlc/vlc.h>
#include <vlc_vout.h>
#include <vlc_codec.h>
#include <vlc_input.h>

#include <vlc_osd.h>
#include <vlc_filter.h>
#include <vlc_image.h>
#include <vlc_charset.h>
#include <vlc_stream.h>
#include <vlc_xml.h>
#include <errno.h>
#include <string.h>

#define NO_BREAKING_SPACE  "&#160;"

enum
{
    ATTRIBUTE_ALIGNMENT = (1 << 0),
    ATTRIBUTE_X         = (1 << 1),
    ATTRIBUTE_X_PERCENT = (1 << 2),
    ATTRIBUTE_Y         = (1 << 3),
    ATTRIBUTE_Y_PERCENT = (1 << 4),
};

typedef struct
{
    char       *psz_filename;
    picture_t  *p_pic;
} image_attach_t;

typedef struct
{
    char *          psz_stylename; /* The name of the style, no comma's allowed */
    text_style_t    font_style;
    int             i_align;
    int             i_margin_h;
    int             i_margin_v;
    int             i_margin_percent_h;
    int             i_margin_percent_v;
}  ssa_style_t;

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    vlc_bool_t          b_ass;                           /* The subs are ASS */
    int                 i_original_height;
    int                 i_original_width;
    int                 i_align;          /* Subtitles alignment on the vout */
    vlc_iconv_t         iconv_handle;            /* handle to iconv instance */
    vlc_bool_t          b_autodetect_utf8;

    ssa_style_t         **pp_ssa_styles;
    int                 i_ssa_styles;

    image_attach_t      **pp_images;
    int                 i_images;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static subpicture_t *DecodeBlock   ( decoder_t *, block_t ** );
static subpicture_t *ParseText     ( decoder_t *, block_t * );
static void         ParseSSAHeader ( decoder_t * );
static void         ParseUSFHeader ( decoder_t * );
static void         ParseUSFHeaderTags( decoder_t *, xml_reader_t * );
static void         ParseSSAString ( decoder_t *, char *, subpicture_t * );
static subpicture_region_t *ParseUSFString ( decoder_t *, char *, subpicture_t * );
static void         ParseColor     ( decoder_t *, char *, int *, int * );
static char        *StripTags      ( char * );
static char        *CreateHtmlSubtitle ( char * );
static char        *CreatePlainText( char * );
static int          ParseImageAttachments( decoder_t *p_dec );
static subpicture_region_t *LoadEmbeddedImage( decoder_t *p_dec, subpicture_t *p_spu, const char *psz_filename, int i_transparent_color );

#define DEFAULT_NAME "Default"
#define MAX_LINE 8192

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static const char *ppsz_encodings[] = { DEFAULT_NAME, "ASCII", "UTF-8", "",
    "ISO-8859-1", "CP1252", "MacRoman", "MacIceland","ISO-8859-15", "",
    "ISO-8859-2", "CP1250", "MacCentralEurope", "MacCroatian", "MacRomania", "",
    "ISO-8859-5", "CP1251", "MacCyrillic", "MacUkraine", "KOI8-R", "KOI8-U", "KOI8-RU", "",
    "ISO-8859-6", "CP1256", "MacArabic", "",
    "ISO-8859-7", "CP1253", "MacGreek", "",
    "ISO-8859-8", "CP1255", "MacHebrew", "",
    "ISO-8859-9", "CP1254", "MacTurkish", "",
    "ISO-8859-13", "CP1257", "",
    "ISO-2022-JP", "ISO-2022-JP-1", "ISO-2022-JP-2", "EUC-JP", "SHIFT_JIS", "",
    "ISO-2022-CN", "ISO-2022-CN-EXT", "EUC-CN", "EUC-TW", "BIG5", "BIG5-HKSCS", "",
    "ISO-2022-KR", "EUC-KR", "",
    "MacThai", "KOI8-T", "",
    "ISO-8859-3", "ISO-8859-4", "ISO-8859-10", "ISO-8859-14", "ISO-8859-16", "",
    "CP850", "CP862", "CP866", "CP874", "CP932", "CP949", "CP950", "CP1133", "CP1258", "",
    "Macintosh", "",
    "UTF-7", "UTF-16", "UTF-16BE", "UTF-16LE", "UTF-32", "UTF-32BE", "UTF-32LE",
    "C99", "JAVA", "UCS-2", "UCS-2BE", "UCS-2LE", "UCS-4", "UCS-4BE", "UCS-4LE", "",
    "HZ", "GBK", "GB18030", "JOHAB", "ARMSCII-8",
    "Georgian-Academy", "Georgian-PS", "TIS-620", "MuleLao-1", "VISCII", "TCVN",
    "HPROMAN8", "NEXTSTEP" };
/*
SSA supports charset selection.
The following known charsets are used:

0 = Ansi - Western European
1 = default
2 = symbol
3 = invalid
77 = Mac
128 = Japanese (Shift JIS)
129 = Hangul
130 = Johab
134 = GB2312 Simplified Chinese
136 = Big5 Traditional Chinese
161 = Greek
162 = Turkish
163 = Vietnamese
177 = Hebrew
178 = Arabic
186 = Baltic
204 = Russian (Cyrillic)
222 = Thai
238 = Eastern European
254 = PC 437
*/

static int  pi_justification[] = { 0, 1, 2 };
static const char *ppsz_justification_text[] = {N_("Center"),N_("Left"),N_("Right")};

#define ENCODING_TEXT N_("Subtitles text encoding")
#define ENCODING_LONGTEXT N_("Set the encoding used in text subtitles")
#define ALIGN_TEXT N_("Subtitles justification")
#define ALIGN_LONGTEXT N_("Set the justification of subtitles")
#define AUTODETECT_UTF8_TEXT N_("UTF-8 subtitles autodetection")
#define AUTODETECT_UTF8_LONGTEXT N_("This enables automatic detection of " \
            "UTF-8 encoding within subtitles files.")
#define FORMAT_TEXT N_("Formatted Subtitles")
#define FORMAT_LONGTEXT N_("Some subtitle formats allow for text formatting. " \
 "VLC partly implements this, but you can choose to disable all formatting.")


vlc_module_begin();
    set_shortname( _("Subtitles"));
    set_description( _("Text subtitles decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, CloseDecoder );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_SCODEC );

    add_integer( "subsdec-align", 0, NULL, ALIGN_TEXT, ALIGN_LONGTEXT,
                 VLC_FALSE );
        change_integer_list( pi_justification, ppsz_justification_text, 0 );
    add_string( "subsdec-encoding", DEFAULT_NAME, NULL,
                ENCODING_TEXT, ENCODING_LONGTEXT, VLC_FALSE );
        change_string_list( ppsz_encodings, 0, 0 );
    add_bool( "subsdec-autodetect-utf8", VLC_TRUE, NULL,
              AUTODETECT_UTF8_TEXT, AUTODETECT_UTF8_LONGTEXT, VLC_FALSE );
    add_bool( "subsdec-formatted", VLC_TRUE, NULL, FORMAT_TEXT, FORMAT_LONGTEXT,
                 VLC_FALSE );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    vlc_value_t    val;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('s','u','b','t') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('u','s','f',' ') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('s','s','a',' ') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_decode_sub = DecodeBlock;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)calloc(1, sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_ENOMEM;
    }

    /* init of p_sys */
    p_sys->i_align = 0;
    p_sys->iconv_handle = (vlc_iconv_t)-1;
    p_sys->b_autodetect_utf8 = VLC_FALSE;
    p_sys->b_ass = VLC_FALSE;
    p_sys->i_original_height = -1;
    p_sys->i_original_width = -1;
    TAB_INIT( p_sys->i_ssa_styles, p_sys->pp_ssa_styles );
    TAB_INIT( p_sys->i_images, p_sys->pp_images );

    char *psz_charset = NULL;
    /* First try demux-specified encoding */
    if( p_dec->fmt_in.subs.psz_encoding && *p_dec->fmt_in.subs.psz_encoding )
    {
        psz_charset = strdup (p_dec->fmt_in.subs.psz_encoding);
        msg_Dbg (p_dec, "trying demuxer-specified character encoding: %s",
                 p_dec->fmt_in.subs.psz_encoding ?: "not specified");
    }

    /* Second, try configured encoding */
    if (psz_charset == NULL)
    {
        psz_charset = var_CreateGetNonEmptyString (p_dec, "subsdec-encoding");
        if ((psz_charset != NULL) && !strcasecmp (psz_charset, DEFAULT_NAME))
        {
            free (psz_charset);
            psz_charset = NULL;
        }

        msg_Dbg (p_dec, "trying configured character encoding: %s",
                 psz_charset ?: "not specified");
    }

    /* Third, try "local" encoding with optional UTF-8 autodetection */
    if (psz_charset == NULL)
    {
        psz_charset = strdup (GetFallbackEncoding ());
        msg_Dbg (p_dec, "trying default character encoding: %s",
                 psz_charset ?: "not specified");

        if (var_CreateGetBool (p_dec, "subsdec-autodetect-utf8"))
        {
            msg_Dbg (p_dec, "using automatic UTF-8 detection");
            p_sys->b_autodetect_utf8 = VLC_TRUE;
        }
    }

    if (psz_charset == NULL)
    {
        psz_charset = strdup ("UTF-8");
        msg_Dbg (p_dec, "trying hard-coded character encoding: %s",
                 psz_charset ?: "error");
    }

    if (psz_charset == NULL)
    {
        free (p_sys);
        return VLC_ENOMEM;
    }

    if (strcasecmp (psz_charset, "UTF-8") && strcasecmp (psz_charset, "utf8"))
    {
        p_sys->iconv_handle = vlc_iconv_open ("UTF-8", psz_charset);
        if (p_sys->iconv_handle == (vlc_iconv_t)(-1))
            msg_Err (p_dec, "cannot convert from %s: %s", psz_charset,
                     strerror (errno));
    }
    free (psz_charset);

    var_Create( p_dec, "subsdec-align", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "subsdec-align", &val );
    p_sys->i_align = val.i_int;

    ParseImageAttachments( p_dec );

    if( p_dec->fmt_in.i_codec == VLC_FOURCC('s','s','a',' ') && var_CreateGetBool( p_dec, "subsdec-formatted" ) )
    {
        if( p_dec->fmt_in.i_extra > 0 )
            ParseSSAHeader( p_dec );
    }
    else if( p_dec->fmt_in.i_codec == VLC_FOURCC('u','s','f',' ') && var_CreateGetBool( p_dec, "subsdec-formatted" ) )
    {
        if( p_dec->fmt_in.i_extra > 0 )
            ParseUSFHeader( p_dec );
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete subtitles units.
 ****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    subpicture_t *p_spu = NULL;

    if( !pp_block || *pp_block == NULL ) return NULL;

    p_spu = ParseText( p_dec, *pp_block );

    block_Release( *pp_block );
    *pp_block = NULL;

    return p_spu;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->iconv_handle != (vlc_iconv_t)-1 )
        vlc_iconv_close( p_sys->iconv_handle );

    if( p_sys->pp_ssa_styles )
    {
        int i;
        for( i = 0; i < p_sys->i_ssa_styles; i++ )
        {
            if( !p_sys->pp_ssa_styles[i] )
                continue;

            if( p_sys->pp_ssa_styles[i]->psz_stylename )
                free( p_sys->pp_ssa_styles[i]->psz_stylename );
            if( p_sys->pp_ssa_styles[i]->font_style.psz_fontname )
                free( p_sys->pp_ssa_styles[i]->font_style.psz_fontname );
            if( p_sys->pp_ssa_styles[i] )
                free( p_sys->pp_ssa_styles[i] );
        }
        TAB_CLEAN( p_sys->i_ssa_styles, p_sys->pp_ssa_styles );
    }
    if( p_sys->pp_images )
    {
        int i;
        for( i = 0; i < p_sys->i_images; i++ )
        {
            if( !p_sys->pp_images[i] )
                continue;

            if( p_sys->pp_images[i]->p_pic )
                p_sys->pp_images[i]->p_pic->pf_release( p_sys->pp_images[i]->p_pic );
            if( p_sys->pp_images[i]->psz_filename )
                free( p_sys->pp_images[i]->psz_filename );

            free( p_sys->pp_images[i] );
        }
        TAB_CLEAN( p_sys->i_images, p_sys->pp_images );
    }

    free( p_sys );
}

/*****************************************************************************
 * ParseText: parse an text subtitle packet and send it to the video output
 *****************************************************************************/
static subpicture_t *ParseText( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t *p_spu = NULL;
    char *psz_subtitle = NULL;
    video_format_t fmt;

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts == 0 )
    {
        msg_Warn( p_dec, "subtitle without a date" );
        return NULL;
    }

    /* Check validity of packet data */
    /* An "empty" line containing only \0 can be used to force
       and ephemer picture from the screen */
    if( p_block->i_buffer < 1 )
    {
        msg_Warn( p_dec, "no subtitle data" );
        return NULL;
    }

    /* Should be resiliant against bad subtitles */
    psz_subtitle = strndup( (const char *)p_block->p_buffer,
                            p_block->i_buffer );
    if( psz_subtitle == NULL )
        return NULL;

    if( p_sys->iconv_handle == (vlc_iconv_t)-1 )
    {
        if (EnsureUTF8( psz_subtitle ) == NULL)
        {
            msg_Err( p_dec, _("failed to convert subtitle encoding.\n"
                     "Try manually setting a character-encoding "
                     "before you open the file.") );
        }
    }
    else
    {

        if( p_sys->b_autodetect_utf8 )
        {
            if( IsUTF8( psz_subtitle ) == NULL )
            {
                msg_Dbg( p_dec, "invalid UTF-8 sequence: "
                         "disabling UTF-8 subtitles autodetection" );
                p_sys->b_autodetect_utf8 = VLC_FALSE;
            }
        }

        if( !p_sys->b_autodetect_utf8 )
        {
            size_t inbytes_left = strlen( psz_subtitle );
            size_t outbytes_left = 6 * inbytes_left;
            char *psz_new_subtitle = malloc( outbytes_left + 1 );
            char *psz_convert_buffer_out = psz_new_subtitle;
            const char *psz_convert_buffer_in = psz_subtitle;

            size_t ret = vlc_iconv( p_sys->iconv_handle,
                                    &psz_convert_buffer_in, &inbytes_left,
                                    &psz_convert_buffer_out, &outbytes_left );

            *psz_convert_buffer_out++ = '\0';
            free( psz_subtitle );

            if( ( ret == (size_t)(-1) ) || inbytes_left )
            {
                free( psz_new_subtitle );
                msg_Err( p_dec, _("failed to convert subtitle encoding.\n"
                        "Try manually setting a character-encoding "
                                "before you open the file.") );
                return NULL;
            }

            psz_subtitle = realloc( psz_new_subtitle,
                                    psz_convert_buffer_out - psz_new_subtitle );
        }
    }

    /* Create the subpicture unit */
    p_spu = p_dec->pf_spu_buffer_new( p_dec );
    if( !p_spu )
    {
        msg_Warn( p_dec, "can't get spu buffer" );
        if( psz_subtitle ) free( psz_subtitle );
        return NULL;
    }

    p_spu->b_pausable = VLC_TRUE;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_dec), &fmt );
    if( !p_spu->p_region )
    {
        msg_Err( p_dec, "cannot allocate SPU region" );
        if( psz_subtitle ) free( psz_subtitle );
        p_dec->pf_spu_buffer_del( p_dec, p_spu );
        return NULL;
    }

    /* Decode and format the subpicture unit */
    if( p_dec->fmt_in.i_codec != VLC_FOURCC('s','s','a',' ') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('u','s','f',' ') )
    {
        /* Normal text subs, easy markup */
        p_spu->p_region->i_align = SUBPICTURE_ALIGN_BOTTOM | p_sys->i_align;
        p_spu->i_x = p_sys->i_align ? 20 : 0;
        p_spu->i_y = 10;

        /* Remove formatting from string */

        p_spu->p_region->psz_text = StripTags( psz_subtitle );
        if( var_CreateGetBool( p_dec, "subsdec-formatted" ) )
        {
            p_spu->p_region->psz_html = CreateHtmlSubtitle( psz_subtitle );
        }

        p_spu->i_start = p_block->i_pts;
        p_spu->i_stop = p_block->i_pts + p_block->i_length;
        p_spu->b_ephemer = (p_block->i_length == 0);
        p_spu->b_absolute = VLC_FALSE;
    }
    else
    {
        /* Decode SSA/USF strings */
        if( p_dec->fmt_in.i_codec == VLC_FOURCC('s','s','a',' ') )
            ParseSSAString( p_dec, psz_subtitle, p_spu );
        else
        {
            p_spu->pf_destroy_region( VLC_OBJECT(p_dec), p_spu->p_region );
            p_spu->p_region = ParseUSFString( p_dec, psz_subtitle, p_spu );
        }

        p_spu->i_start = p_block->i_pts;
        p_spu->i_stop = p_block->i_pts + p_block->i_length;
        p_spu->b_ephemer = (p_block->i_length == 0);
        p_spu->b_absolute = VLC_FALSE;
        p_spu->i_original_picture_width = p_sys->i_original_width;
        p_spu->i_original_picture_height = p_sys->i_original_height;
    }
    if( psz_subtitle ) free( psz_subtitle );

    return p_spu;
}

static char *GrabAttributeValue( const char *psz_attribute,
                                 const char *psz_tag_start )
{
    if( psz_attribute && psz_tag_start )
    {
        char *psz_tag_end = strchr( psz_tag_start, '>' );
        char *psz_found   = strcasestr( psz_tag_start, psz_attribute );

        if( psz_found )
        {
            psz_found += strlen( psz_attribute );

            if(( *(psz_found++) == '=' ) &&
               ( *(psz_found++) == '\"' ))
            {
                if( psz_found < psz_tag_end )
                {
                    int   i_len = strcspn( psz_found, "\"" );
                    return strndup( psz_found, i_len );
                }
            }
        }
    }
    return NULL;
}

static ssa_style_t *ParseStyle( decoder_sys_t *p_sys, char *psz_subtitle )
{
    ssa_style_t *p_style   = NULL;
    char        *psz_style = GrabAttributeValue( "style", psz_subtitle );

    if( psz_style )
    {
        int i;

        for( i = 0; i < p_sys->i_ssa_styles; i++ )
        {
            if( !strcmp( p_sys->pp_ssa_styles[i]->psz_stylename, psz_style ) )
                p_style = p_sys->pp_ssa_styles[i];
        }
        free( psz_style );
    }
    return p_style;
}

static int ParsePositionAttributeList( char *psz_subtitle, int *i_align, int *i_x, int *i_y )
{
    int   i_mask = 0;

    char *psz_align    = GrabAttributeValue( "alignment", psz_subtitle );
    char *psz_margin_x = GrabAttributeValue( "horizontal-margin", psz_subtitle );
    char *psz_margin_y = GrabAttributeValue( "vertical-margin", psz_subtitle );
    /* -- UNSUPPORTED
    char *psz_relative = GrabAttributeValue( "relative-to", psz_subtitle );
    char *psz_rotate_x = GrabAttributeValue( "rotate-x", psz_subtitle );
    char *psz_rotate_y = GrabAttributeValue( "rotate-y", psz_subtitle );
    char *psz_rotate_z = GrabAttributeValue( "rotate-z", psz_subtitle );
    */

    *i_align = SUBPICTURE_ALIGN_BOTTOM;
    *i_x = 0;
    *i_y = 0;

    if( psz_align )
    {
        if( !strcasecmp( "TopLeft", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;
        else if( !strcasecmp( "TopCenter", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_TOP;
        else if( !strcasecmp( "TopRight", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_RIGHT;
        else if( !strcasecmp( "MiddleLeft", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_LEFT;
        else if( !strcasecmp( "MiddleCenter", psz_align ) )
            *i_align = 0;
        else if( !strcasecmp( "MiddleRight", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_RIGHT;
        else if( !strcasecmp( "BottomLeft", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_LEFT;
        else if( !strcasecmp( "BottomCenter", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_BOTTOM;
        else if( !strcasecmp( "BottomRight", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_RIGHT;

        i_mask |= ATTRIBUTE_ALIGNMENT;
        free( psz_align );
    }
    if( psz_margin_x )
    {
        *i_x = atoi( psz_margin_x );
        if( strchr( psz_margin_x, '%' ) )
            i_mask |= ATTRIBUTE_X_PERCENT;
        else
            i_mask |= ATTRIBUTE_X;

        free( psz_margin_x );
    }
    if( psz_margin_y )
    {
        *i_y = atoi( psz_margin_y );
        if( strchr( psz_margin_y, '%' ) )
            i_mask |= ATTRIBUTE_Y_PERCENT;
        else
            i_mask |= ATTRIBUTE_Y;

        free( psz_margin_y );
    }
    return i_mask;
}

static void SetupPositions( subpicture_region_t *p_region, char *psz_subtitle )
{
    int           i_mask = 0;
    int           i_align;
    int           i_x, i_y;

    i_mask = ParsePositionAttributeList( psz_subtitle, &i_align, &i_x, &i_y );

    if( i_mask & ATTRIBUTE_ALIGNMENT )
        p_region->i_align = i_align;

    /* TODO: Setup % based offsets properly, without adversely affecting
     *       everything else in vlc. Will address with separate patch, to
     *       prevent this one being any more complicated.
     */
    if( i_mask & ATTRIBUTE_X )
        p_region->i_x = i_x;
    else if( i_mask & ATTRIBUTE_X_PERCENT )
        p_region->i_x = 0;

    if( i_mask & ATTRIBUTE_Y )
        p_region->i_y = i_y;
    else if( i_mask & ATTRIBUTE_Y_PERCENT )
        p_region->i_y = 0;
}

static subpicture_region_t *CreateTextRegion( decoder_t *p_dec,
                                              subpicture_t *p_spu,
                                              char *psz_subtitle,
                                              int i_len,
                                              int i_sys_align )
{
    decoder_sys_t        *p_sys = p_dec->p_sys;
    subpicture_region_t  *p_text_region;
    video_format_t        fmt;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_text_region = p_spu->pf_create_region( VLC_OBJECT(p_dec), &fmt );

    if( p_text_region != NULL )
    {
        ssa_style_t  *p_style = NULL;

        p_text_region->psz_text = NULL;
        p_text_region->psz_html = strndup( psz_subtitle, i_len );
        if( ! p_text_region->psz_html )
        {
            msg_Err( p_dec, "out of memory" );
            p_spu->pf_destroy_region( VLC_OBJECT(p_dec), p_text_region );
            return NULL;
        }

        p_style = ParseStyle( p_sys, p_text_region->psz_html );
        if( !p_style )
        {
            int i;

            for( i = 0; i < p_sys->i_ssa_styles; i++ )
            {
                if( !strcasecmp( p_sys->pp_ssa_styles[i]->psz_stylename, "Default" ) )
                    p_style = p_sys->pp_ssa_styles[i];
            }
        }

        if( p_style )
        {
            msg_Dbg( p_dec, "style is: %s", p_style->psz_stylename );

            p_text_region->p_style = &p_style->font_style;
            p_text_region->i_align = p_style->i_align;

            /* TODO: Setup % based offsets properly, without adversely affecting
             *       everything else in vlc. Will address with separate patch,
             *       to prevent this one being any more complicated.

                     * p_style->i_margin_percent_h;
                     * p_style->i_margin_percent_v;
             */
            p_text_region->i_x         = p_style->i_margin_h;
            p_text_region->i_y         = p_style->i_margin_v;

        }
        else
        {
            p_text_region->i_align = SUBPICTURE_ALIGN_BOTTOM | i_sys_align;
            p_text_region->i_x = i_sys_align ? 20 : 0;
            p_text_region->i_y = 10;
        }
        /* Look for position arguments which may override the style-based
         * defaults.
         */
        SetupPositions( p_text_region, psz_subtitle );

        p_text_region->p_next = NULL;
    }
    return p_text_region;
}

static subpicture_region_t *ParseUSFString( decoder_t *p_dec, char *psz_subtitle, subpicture_t *p_spu_in )
{
    decoder_sys_t        *p_sys = p_dec->p_sys;
    subpicture_t         *p_spu = p_spu_in;
    subpicture_region_t  *p_region_first = NULL;
    subpicture_region_t  *p_region_upto  = p_region_first;

    while( *psz_subtitle )
    {
        if( *psz_subtitle == '<' )
        {
            char *psz_end = NULL;

            if(( !strncasecmp( psz_subtitle, "<text ", 6 )) ||
               ( !strncasecmp( psz_subtitle, "<text>", 6 )))
            {
                psz_end = strcasestr( psz_subtitle, "</text>" );

                if( psz_end )
                {
                    subpicture_region_t  *p_text_region;

                    psz_end += strcspn( psz_end, ">" ) + 1;

                    p_text_region = CreateTextRegion( p_dec,
                                                      p_spu,
                                                      psz_subtitle,
                                                      psz_end - psz_subtitle,
                                                      p_sys->i_align );

                    if( p_text_region )
                    {
                        p_text_region->psz_text = CreatePlainText( p_text_region->psz_html );

                        if( ! var_CreateGetBool( p_dec, "subsdec-formatted" ) )
                        {
                            free( p_text_region->psz_html );
                            p_text_region->psz_html = NULL;
                        }
                    }

                    if( !p_region_first )
                    {
                        p_region_first = p_region_upto = p_text_region;
                    }
                    else if( p_text_region )
                    {
                        p_region_upto->p_next = p_text_region;
                        p_region_upto = p_region_upto->p_next;
                    }
                }
            }
            else if(( !strncasecmp( psz_subtitle, "<karaoke ", 9 )) ||
                    ( !strncasecmp( psz_subtitle, "<karaoke>", 9 )))
            {
                psz_end = strcasestr( psz_subtitle, "</karaoke>" );

                if( psz_end )
                {
                    subpicture_region_t  *p_text_region;

                    psz_end += strcspn( psz_end, ">" ) + 1;

                    p_text_region = CreateTextRegion( p_dec,
                                                      p_spu,
                                                      psz_subtitle,
                                                      psz_end - psz_subtitle,
                                                      p_sys->i_align );

                    if( p_text_region )
                    {
                        if( ! var_CreateGetBool( p_dec, "subsdec-formatted" ) )
                        {
                            free( p_text_region->psz_html );
                            p_text_region->psz_html = NULL;
                        }
                    }
                    if( !p_region_first )
                    {
                        p_region_first = p_region_upto = p_text_region;
                    }
                    else if( p_text_region )
                    {
                        p_region_upto->p_next = p_text_region;
                        p_region_upto = p_region_upto->p_next;
                    }
                }
            }
            else if(( !strncasecmp( psz_subtitle, "<image ", 7 )) ||
                    ( !strncasecmp( psz_subtitle, "<image>", 7 )))
            {
                subpicture_region_t *p_image_region = NULL;

                char *psz_end = strcasestr( psz_subtitle, "</image>" );
                char *psz_content = strchr( psz_subtitle, '>' );
                int   i_transparent = -1;

                /* If a colorkey parameter is specified, then we have to map
                 * that index in the picture through as transparent (it is
                 * required by the USF spec but is also recommended that if the
                 * creator really wants a transparent colour that they use a
                 * type like PNG that properly supports it; this goes doubly
                 * for VLC because the pictures are stored internally in YUV
                 * and the resulting colour-matching may not produce the
                 * desired results.)
                 */
                char *psz_tmp = GrabAttributeValue( "colorkey", psz_subtitle );
                if( psz_tmp )
                {
                    if( *psz_tmp == '#' )
                        i_transparent = strtol( psz_tmp + 1, NULL, 16 ) & 0x00ffffff;
                    free( psz_tmp );
                }
                if( psz_content && ( psz_content < psz_end ) )
                {
                    char *psz_filename = strndup( &psz_content[1], psz_end - &psz_content[1] );
                    if( psz_filename )
                    {
                        p_image_region = LoadEmbeddedImage( p_dec, p_spu, psz_filename, i_transparent );
                        free( psz_filename );
                    }
                }

                if( psz_end ) psz_end += strcspn( psz_end, ">" ) + 1;

                if( p_image_region )
                {
                    SetupPositions( p_image_region, psz_subtitle );

                    p_image_region->p_next   = NULL;
                    p_image_region->psz_text = NULL;
                    p_image_region->psz_html = NULL;

                }
                if( !p_region_first )
                {
                    p_region_first = p_region_upto = p_image_region;
                }
                else if( p_image_region )
                {
                    p_region_upto->p_next = p_image_region;
                    p_region_upto = p_region_upto->p_next;
                }
            }
            if( psz_end )
                psz_subtitle = psz_end - 1;

            psz_subtitle += strcspn( psz_subtitle, ">" );
        }

        psz_subtitle++;
    }

    return p_region_first;
}

static void ParseSSAString( decoder_t *p_dec, char *psz_subtitle, subpicture_t *p_spu_in )
{
    /* We expect MKV formatted SSA:
     * ReadOrder, Layer, Style, CharacterName, MarginL, MarginR,
     * MarginV, Effect, Text */
    decoder_sys_t   *p_sys = p_dec->p_sys;
    subpicture_t    *p_spu = p_spu_in;
    ssa_style_t     *p_style = NULL;
    char            *psz_new_subtitle = NULL;
    char            *psz_buffer_sub = NULL;
    char            *psz_style = NULL;
    char            *psz_style_start = NULL;
    char            *psz_style_end = NULL;
    int             i_text = 0, i_comma = 0, i_strlen = 0, i;
    int             i_margin_l = 0, i_margin_r = 0, i_margin_v = 0;

    psz_buffer_sub = psz_subtitle;

    p_spu->p_region->psz_html = NULL;

    i_comma = 0;
    while( i_comma < 8 && *psz_buffer_sub != '\0' )
    {
        if( *psz_buffer_sub == ',' )
        {
            i_comma++;
            if( i_comma == 2 )
                psz_style_start = &psz_buffer_sub[1];
            else if( i_comma == 3 )
                psz_style_end = &psz_buffer_sub[0];
            else if( i_comma == 4 )
                i_margin_l = (int)strtol( &psz_buffer_sub[1], NULL, 10 );
            else if( i_comma == 5 )
                i_margin_r = (int)strtol( &psz_buffer_sub[1], NULL, 10 );
            else if( i_comma == 6 )
                i_margin_v = (int)strtol( &psz_buffer_sub[1], NULL, 10 );
        }
        psz_buffer_sub++;
    }

    if( *psz_buffer_sub == '\0' && i_comma == 8 )
    {
        msg_Dbg( p_dec, "couldn't find all fields in this SSA line" );
        return;
    }

    psz_new_subtitle = malloc( strlen( psz_buffer_sub ) + 1);
    i_text = 0;
    while( psz_buffer_sub[0] != '\0' )
    {
        if( psz_buffer_sub[0] == '\\' && psz_buffer_sub[1] == 'n' )
        {
            psz_new_subtitle[i_text] = ' ';
            i_text++;
            psz_buffer_sub += 2;
        }
        else if( psz_buffer_sub[0] == '\\' && psz_buffer_sub[1] == 'N' )
        {
            psz_new_subtitle[i_text] = '\n';
            i_text++;
            psz_buffer_sub += 2;
        }
        else if( psz_buffer_sub[0] == '{' &&
                 psz_buffer_sub[1] == '\\' )
        {
            /* SSA control code */
            while( psz_buffer_sub[0] != '\0' &&
                   psz_buffer_sub[0] != '}' )
            {
                psz_buffer_sub++;
            }
            psz_buffer_sub++;
        }
        else
        {
            psz_new_subtitle[i_text] = psz_buffer_sub[0];
            i_text++;
            psz_buffer_sub++;
        }
    }
    psz_new_subtitle[i_text] = '\0';

    i_strlen = __MAX( psz_style_end - psz_style_start, 0);
    psz_style = strndup( psz_style_start, i_strlen );

    for( i = 0; i < p_sys->i_ssa_styles; i++ )
    {
        if( !strcmp( p_sys->pp_ssa_styles[i]->psz_stylename, psz_style ) )
            p_style = p_sys->pp_ssa_styles[i];
    }
    if( psz_style ) free( psz_style );

    p_spu->p_region->psz_text = psz_new_subtitle;
    if( p_style == NULL )
    {
        p_spu->p_region->i_align = SUBPICTURE_ALIGN_BOTTOM | p_sys->i_align;
        p_spu->i_x = p_sys->i_align ? 20 : 0;
        p_spu->i_y = 10;
    }
    else
    {
        msg_Dbg( p_dec, "style is: %s", p_style->psz_stylename);
        p_spu->p_region->p_style = &p_style->font_style;
        p_spu->p_region->i_align = p_style->i_align;
        if( p_style->i_align & SUBPICTURE_ALIGN_LEFT )
        {
            p_spu->i_x = (i_margin_l) ? i_margin_l : p_style->i_margin_h;
        }
        else if( p_style->i_align & SUBPICTURE_ALIGN_RIGHT )
        {
            p_spu->i_x = (i_margin_r) ? i_margin_r : p_style->i_margin_h;
        }
        p_spu->i_y = (i_margin_v) ? i_margin_v : p_style->i_margin_v;
    }
}

static char* GotoNextLine( char *psz_text )
{
    char *p_newline = psz_text;

    while( p_newline[0] != '\0' )
    {
        if( p_newline[0] == '\n' || p_newline[0] == '\r' )
        {
            p_newline++;
            while( p_newline[0] == '\n' || p_newline[0] == '\r' )
                p_newline++;
            break;
        }
        else p_newline++;
    }
    return p_newline;
}

/*****************************************************************************
 * ParseColor: SSA stores color in BBGGRR, in ASS it uses AABBGGRR
 * The string value in the string can be a pure integer, or hexadecimal &HBBGGRR
 *****************************************************************************/
static void ParseColor( decoder_t *p_dec, char *psz_color, int *pi_color, int *pi_alpha )
{
    int i_color = 0;
    if( !strncasecmp( psz_color, "&H", 2 ) )
    {
        /* textual HEX representation */
        i_color = (int) strtol( psz_color+2, NULL, 16 );
    }
    else i_color = (int) strtol( psz_color, NULL, 0 );

    *pi_color = 0;
    *pi_color |= ( ( i_color & 0x000000FF ) << 16 ); /* Red */
    *pi_color |= ( ( i_color & 0x0000FF00 ) );       /* Green */
    *pi_color |= ( ( i_color & 0x00FF0000 ) >> 16 ); /* Blue */

    if( pi_alpha != NULL )
        *pi_alpha = ( i_color & 0xFF000000 ) >> 24;
}

static int ParseImageAttachments( decoder_t *p_dec )
{
    decoder_sys_t        *p_sys = p_dec->p_sys;
    input_attachment_t  **pp_attachments;
    int                   i_attachments_cnt;
    int                   k = 0;

    if( VLC_SUCCESS != decoder_GetInputAttachments( p_dec, &pp_attachments, &i_attachments_cnt ))
        return VLC_EGENERIC;

    for( k = 0; k < i_attachments_cnt; k++ )
    {
        input_attachment_t *p_attach = pp_attachments[k];

        vlc_fourcc_t  type  = 0;

        if( ( !strcmp( p_attach->psz_mime, "image/bmp" ) )      || /* BMP */
            ( !strcmp( p_attach->psz_mime, "image/x-bmp" ) )    ||
            ( !strcmp( p_attach->psz_mime, "image/x-bitmap" ) ) ||
            ( !strcmp( p_attach->psz_mime, "image/x-ms-bmp" ) ) )
        {
             type = VLC_FOURCC('b','m','p',' ');
        }
        else if( ( !strcmp( p_attach->psz_mime, "image/x-portable-anymap" ) )  || /* PNM */
                 ( !strcmp( p_attach->psz_mime, "image/x-portable-bitmap" ) )  || /* PBM */
                 ( !strcmp( p_attach->psz_mime, "image/x-portable-graymap" ) ) || /* PGM */
                 ( !strcmp( p_attach->psz_mime, "image/x-portable-pixmap" ) ) )   /* PPM */
        {
            type = VLC_FOURCC('p','n','m',' ');
        }
        else if ( !strcmp( p_attach->psz_mime, "image/gif" ) )         /* GIF */
            type = VLC_FOURCC('g','i','f',' ');
        else if ( !strcmp( p_attach->psz_mime, "image/jpeg" ) )        /* JPG, JPEG */
            type = VLC_FOURCC('j','p','e','g');
        else if ( !strcmp( p_attach->psz_mime, "image/pcx" ) )         /* PCX */
            type = VLC_FOURCC('p','c','x',' ');
        else if ( !strcmp( p_attach->psz_mime, "image/png" ) )         /* PNG */
            type = VLC_FOURCC('p','n','g',' ');
        else if ( !strcmp( p_attach->psz_mime, "image/tiff" ) )        /* TIF, TIFF */
            type = VLC_FOURCC('t','i','f','f');
        else if ( !strcmp( p_attach->psz_mime, "image/x-tga" ) )       /* TGA */
            type = VLC_FOURCC('t','g','a',' ');
        else if ( !strcmp( p_attach->psz_mime, "image/x-xpixmap") )    /* XPM */
            type = VLC_FOURCC('x','p','m',' ');

        if( ( type != 0 ) &&
            ( p_attach->i_data > 0 ) &&
            ( p_attach->p_data != NULL ) )
        {
            picture_t         *p_pic = NULL;
            image_handler_t   *p_image;

            p_image = image_HandlerCreate( p_dec );
            if( p_image != NULL )
            {
                block_t   *p_block;

                p_block = block_New( p_image->p_parent, p_attach->i_data );

                if( p_block != NULL )
                {
                    video_format_t     fmt_in;
                    video_format_t     fmt_out;

                    memcpy( p_block->p_buffer, p_attach->p_data, p_attach->i_data );

                    memset( &fmt_in,  0, sizeof( video_format_t));
                    memset( &fmt_out, 0, sizeof( video_format_t));

                    fmt_in.i_chroma  = type;
                    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');

                    /* Find a suitable decoder module */
                    if( module_Exists( p_dec, "SDL Image decoder" ) )
                    {
                        /* ffmpeg thinks it can handle bmp properly but it can't (at least
                         * not all of them), so use sdl_image if it is available */

                        vlc_value_t val;

                        var_Create( p_dec, "codec", VLC_VAR_MODULE | VLC_VAR_DOINHERIT );
                        val.psz_string = (char*) "sdl_image";
                        var_Set( p_dec, "codec", val );
                    }

                    p_pic = image_Read( p_image, p_block, &fmt_in, &fmt_out );
                    var_Destroy( p_dec, "codec" );
                }

                image_HandlerDelete( p_image );
            }
            if( p_pic )
            {
                image_attach_t *p_picture = malloc( sizeof(image_attach_t) );

                if( p_picture )
                {
                    p_picture->psz_filename = strdup( p_attach->psz_name );
                    p_picture->p_pic = p_pic;

                    TAB_APPEND( p_sys->i_images, p_sys->pp_images, p_picture );
                }
            }
        }
        vlc_input_attachment_Delete( pp_attachments[ k ] );
    }
    free( pp_attachments );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParseUSFHeader: Retrieve global formatting information etc
 *****************************************************************************/
static void ParseUSFHeader( decoder_t *p_dec )
{
    stream_t      *p_sub = NULL;
    xml_t         *p_xml = NULL;
    xml_reader_t  *p_xml_reader = NULL;

    p_sub = stream_MemoryNew( VLC_OBJECT(p_dec),
                              p_dec->fmt_in.p_extra,
                              p_dec->fmt_in.i_extra,
                              VLC_TRUE );
    if( !p_sub )
        return;

    p_xml = xml_Create( p_dec );
    if( p_xml )
    {
        p_xml_reader = xml_ReaderCreate( p_xml, p_sub );
        if( p_xml_reader )
        {
            /* Look for Root Node */
            if( xml_ReaderRead( p_xml_reader ) == 1 )
            {
                char *psz_node = xml_ReaderName( p_xml_reader );

                if( !strcasecmp( "usfsubtitles", psz_node ) )
                    ParseUSFHeaderTags( p_dec, p_xml_reader );

                free( psz_node );
            }

            xml_ReaderDelete( p_xml, p_xml_reader );
        }
        xml_Delete( p_xml );
    }
    stream_Delete( p_sub );
}

static void ParseUSFHeaderTags( decoder_t *p_dec, xml_reader_t *p_xml_reader )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    char *psz_node;
    ssa_style_t *p_style = NULL;
    int i_style_level = 0;
    int i_metadata_level = 0;

    while ( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        switch ( xml_ReaderNodeType( p_xml_reader ) )
        {
            case XML_READER_TEXT:
            case XML_READER_NONE:
                break;
            case XML_READER_ENDELEM:
                psz_node = xml_ReaderName( p_xml_reader );

                if( !psz_node )
                    break;
                switch (i_style_level)
                {
                    case 0:
                        if( !strcasecmp( "metadata", psz_node ) && (i_metadata_level == 1) )
                        {
                            i_metadata_level--;
                        }
                        break;
                    case 1:
                        if( !strcasecmp( "styles", psz_node ) )
                        {
                            i_style_level--;
                        }
                        break;
                    case 2:
                        if( !strcasecmp( "style", psz_node ) )
                        {
                            TAB_APPEND( p_sys->i_ssa_styles, p_sys->pp_ssa_styles, p_style );

                            p_style = NULL;
                            i_style_level--;
                        }
                        break;
                }

                free( psz_node );
                break;
            case XML_READER_STARTELEM:
                psz_node = xml_ReaderName( p_xml_reader );

                if( !psz_node )
                    break;

                if( !strcasecmp( "metadata", psz_node ) && (i_style_level == 0) )
                {
                    i_metadata_level++;
                }
                else if( !strcasecmp( "resolution", psz_node ) && (i_metadata_level == 1) )
                {
                    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_name = xml_ReaderName ( p_xml_reader );
                        char *psz_value = xml_ReaderValue ( p_xml_reader );

                        if( psz_name && psz_value )
                        {
                            if( !strcasecmp( "x", psz_name ) )
                                p_sys->i_original_width = atoi( psz_value );
                            else if( !strcasecmp( "y", psz_name ) )
                                p_sys->i_original_height = atoi( psz_value );
                        }
                        if( psz_name )  free( psz_name );
                        if( psz_value ) free( psz_value );
                    }
                }
                else if( !strcasecmp( "styles", psz_node ) && (i_style_level == 0) )
                {
                    i_style_level++;
                }
                else if( !strcasecmp( "style", psz_node ) && (i_style_level == 1) )
                {
                    i_style_level++;

                    p_style = calloc( 1, sizeof(ssa_style_t) );
                    if( ! p_style )
                    {
                        msg_Err( p_dec, "out of memory" );
                        free( psz_node );
                        break;
                    }
                    /* All styles are supposed to default to Default, and then
                     * one or more settings are over-ridden.
                     * At the moment this only effects styles defined AFTER
                     * Default in the XML
                     */
                    int i;
                    for( i = 0; i < p_sys->i_ssa_styles; i++ )
                    {
                        if( !strcasecmp( p_sys->pp_ssa_styles[i]->psz_stylename, "Default" ) )
                        {
                            ssa_style_t *p_default_style = p_sys->pp_ssa_styles[i];

                            memcpy( p_style, p_default_style, sizeof( ssa_style_t ) );
                            p_style->font_style.psz_fontname = strdup( p_style->font_style.psz_fontname );
                            p_style->psz_stylename = NULL;
                        }
                    }

                    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_name = xml_ReaderName ( p_xml_reader );
                        char *psz_value = xml_ReaderValue ( p_xml_reader );

                        if( psz_name && psz_value )
                        {
                            if( !strcasecmp( "name", psz_name ) )
                                p_style->psz_stylename = strdup( psz_value);
                        }
                        if( psz_name )  free( psz_name );
                        if( psz_value ) free( psz_value );
                    }
                }
                else if( !strcasecmp( "fontstyle", psz_node ) && (i_style_level == 2) )
                {
                    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_name = xml_ReaderName ( p_xml_reader );
                        char *psz_value = xml_ReaderValue ( p_xml_reader );

                        if( psz_name && psz_value )
                        {
                            if( !strcasecmp( "face", psz_name ) )
                            {
                                if( p_style->font_style.psz_fontname ) free( p_style->font_style.psz_fontname );
                                p_style->font_style.psz_fontname = strdup( psz_value );
                            }
                            else if( !strcasecmp( "size", psz_name ) )
                            {
                                if( ( *psz_value == '+' ) || ( *psz_value == '-' ) )
                                {
                                    int i_value = atoi( psz_value );

                                    if( ( i_value >= -5 ) && ( i_value <= 5 ) )
                                        p_style->font_style.i_font_size  += ( i_value * p_style->font_style.i_font_size ) / 10;
                                    else if( i_value < -5 )
                                        p_style->font_style.i_font_size  = - i_value;
                                    else if( i_value > 5 )
                                        p_style->font_style.i_font_size  = i_value;
                                }
                                else
                                    p_style->font_style.i_font_size  = atoi( psz_value );
                            }
                            else if( !strcasecmp( "italic", psz_name ) )
                            {
                                if( !strcasecmp( "yes", psz_value ))
                                    p_style->font_style.i_style_flags |= STYLE_ITALIC;
                                else
                                    p_style->font_style.i_style_flags &= ~STYLE_ITALIC;
                            }
                            else if( !strcasecmp( "weight", psz_name ) )
                            {
                                if( !strcasecmp( "bold", psz_value ))
                                    p_style->font_style.i_style_flags |= STYLE_BOLD;
                                else
                                    p_style->font_style.i_style_flags &= ~STYLE_BOLD;
                            }
                            else if( !strcasecmp( "underline", psz_name ) )
                            {
                                if( !strcasecmp( "yes", psz_value ))
                                    p_style->font_style.i_style_flags |= STYLE_UNDERLINE;
                                else
                                    p_style->font_style.i_style_flags &= ~STYLE_UNDERLINE;
                            }
                            else if( !strcasecmp( "color", psz_name ) )
                            {
                                if( *psz_value == '#' )
                                {
                                    unsigned long col = strtol(psz_value+1, NULL, 16);
                                    p_style->font_style.i_font_color = (col & 0x00ffffff);
                                    p_style->font_style.i_font_alpha = (col >> 24) & 0xff;
                                }
                            }
                            else if( !strcasecmp( "outline-color", psz_name ) )
                            {
                                if( *psz_value == '#' )
                                {
                                    unsigned long col = strtol(psz_value+1, NULL, 16);
                                    p_style->font_style.i_outline_color = (col & 0x00ffffff);
                                    p_style->font_style.i_outline_alpha = (col >> 24) & 0xff;
                                }
                            }
                            else if( !strcasecmp( "outline-level", psz_name ) )
                            {
                                p_style->font_style.i_outline_width = atoi( psz_value );
                            }
                            else if( !strcasecmp( "shadow-color", psz_name ) )
                            {
                                if( *psz_value == '#' )
                                {
                                    unsigned long col = strtol(psz_value+1, NULL, 16);
                                    p_style->font_style.i_shadow_color = (col & 0x00ffffff);
                                    p_style->font_style.i_shadow_alpha = (col >> 24) & 0xff;
                                }
                            }
                            else if( !strcasecmp( "shadow-level", psz_name ) )
                            {
                                p_style->font_style.i_shadow_width = atoi( psz_value );
                            }
                            else if( !strcasecmp( "back-color", psz_name ) )
                            {
                                if( *psz_value == '#' )
                                {
                                    unsigned long col = strtol(psz_value+1, NULL, 16);
                                    p_style->font_style.i_karaoke_background_color = (col & 0x00ffffff);
                                    p_style->font_style.i_karaoke_background_alpha = (col >> 24) & 0xff;
                                }
                            }
                            else if( !strcasecmp( "spacing", psz_name ) )
                            {
                                p_style->font_style.i_spacing = atoi( psz_value );
                            }
                        }
                        if( psz_name )  free( psz_name );
                        if( psz_value ) free( psz_value );
                    }
                }
                else if( !strcasecmp( "position", psz_node ) && (i_style_level == 2) )
                {
                    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_name = xml_ReaderName ( p_xml_reader );
                        char *psz_value = xml_ReaderValue ( p_xml_reader );

                        if( psz_name && psz_value )
                        {
                            if( !strcasecmp( "alignment", psz_name ) )
                            {
                                if( !strcasecmp( "TopLeft", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;
                                else if( !strcasecmp( "TopCenter", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_TOP;
                                else if( !strcasecmp( "TopRight", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_RIGHT;
                                else if( !strcasecmp( "MiddleLeft", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_LEFT;
                                else if( !strcasecmp( "MiddleCenter", psz_value ) )
                                    p_style->i_align = 0;
                                else if( !strcasecmp( "MiddleRight", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_RIGHT;
                                else if( !strcasecmp( "BottomLeft", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_LEFT;
                                else if( !strcasecmp( "BottomCenter", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_BOTTOM;
                                else if( !strcasecmp( "BottomRight", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_RIGHT;
                            }
                            else if( !strcasecmp( "horizontal-margin", psz_name ) )
                            {
                                if( strchr( psz_value, '%' ) )
                                {
                                    p_style->i_margin_h = 0;
                                    p_style->i_margin_percent_h = atoi( psz_value );
                                }
                                else
                                {
                                    p_style->i_margin_h = atoi( psz_value );
                                    p_style->i_margin_percent_h = 0;
                                }
                            }
                            else if( !strcasecmp( "vertical-margin", psz_name ) )
                            {
                                if( strchr( psz_value, '%' ) )
                                {
                                    p_style->i_margin_v = 0;
                                    p_style->i_margin_percent_v = atoi( psz_value );
                                }
                                else
                                {
                                    p_style->i_margin_v = atoi( psz_value );
                                    p_style->i_margin_percent_v = 0;
                                }
                            }
                        }
                        if( psz_name )  free( psz_name );
                        if( psz_value ) free( psz_value );
                    }
                }

                free( psz_node );
                break;
        }
    }
    if( p_style ) free( p_style );
}
/*****************************************************************************
 * ParseSSAHeader: Retrieve global formatting information etc
 *****************************************************************************/
static void ParseSSAHeader( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    char *psz_parser = NULL;
    char *psz_header = malloc( p_dec->fmt_in.i_extra+1 );
    int i_section_type = 1;

    memcpy( psz_header, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra );
    psz_header[ p_dec->fmt_in.i_extra] = '\0';

    /* Handle [Script Info] section */
    psz_parser = strcasestr( psz_header, "[Script Info]" );
    if( psz_parser == NULL ) goto eof;

    psz_parser = GotoNextLine( psz_parser );

    while( psz_parser[0] != '\0' )
    {
        int temp;
        char buffer_text[MAX_LINE + 1];

        if( psz_parser[0] == '!' || psz_parser[0] == ';' ) /* comment */;
        else if( sscanf( psz_parser, "PlayResX: %d", &temp ) == 1 )
            p_sys->i_original_width = ( temp > 0 ) ? temp : -1;
        else if( sscanf( psz_parser, "PlayResY: %d", &temp ) == 1 )
            p_sys->i_original_height = ( temp > 0 ) ? temp : -1;
        else if( sscanf( psz_parser, "Script Type: %8192s", buffer_text ) == 1 )
        {
            if( !strcasecmp( buffer_text, "V4.00+" ) ) p_sys->b_ass = VLC_TRUE;
        }
        else if( !strncasecmp( psz_parser, "[V4 Styles]", 11 ) )
            i_section_type = 1;
        else if( !strncasecmp( psz_parser, "[V4+ Styles]", 12) )
        {
            i_section_type = 2;
            p_sys->b_ass = VLC_TRUE;
        }
        else if( !strncasecmp( psz_parser, "[Events]", 8 ) )
            i_section_type = 4;
        else if( !strncasecmp( psz_parser, "Style:", 6 ) )
        {
            int i_font_size, i_bold, i_italic, i_border, i_outline, i_shadow, i_underline,
                i_strikeout, i_scale_x, i_scale_y, i_spacing, i_align, i_margin_l, i_margin_r, i_margin_v;

            char psz_temp_stylename[MAX_LINE+1];
            char psz_temp_fontname[MAX_LINE+1];
            char psz_temp_color1[MAX_LINE+1];
            char psz_temp_color2[MAX_LINE+1];
            char psz_temp_color3[MAX_LINE+1];
            char psz_temp_color4[MAX_LINE+1];

            if( i_section_type == 1 ) /* V4 */
            {
                if( sscanf( psz_parser, "Style: %8192[^,],%8192[^,],%d,%8192[^,],%8192[^,],%8192[^,],%8192[^,],%d,%d,%d,%d,%d,%d,%d,%d,%d%*[^\r\n]",
                    psz_temp_stylename, psz_temp_fontname, &i_font_size,
                    psz_temp_color1, psz_temp_color2, psz_temp_color3, psz_temp_color4, &i_bold, &i_italic,
                    &i_border, &i_outline, &i_shadow, &i_align, &i_margin_l, &i_margin_r, &i_margin_v ) == 16 )
                {
                    ssa_style_t *p_style = malloc( sizeof(ssa_style_t) );

                    p_style->psz_stylename = strdup( psz_temp_stylename );
                    p_style->font_style.psz_fontname = strdup( psz_temp_fontname );
                    p_style->font_style.i_font_size = i_font_size;

                    ParseColor( p_dec, psz_temp_color1, &p_style->font_style.i_font_color, NULL );
                    ParseColor( p_dec, psz_temp_color4, &p_style->font_style.i_shadow_color, NULL );
                    p_style->font_style.i_outline_color = p_style->font_style.i_shadow_color;
                    p_style->font_style.i_font_alpha = p_style->font_style.i_outline_alpha = p_style->font_style.i_shadow_alpha = 0x00;
                    p_style->font_style.i_style_flags = 0;
                    if( i_bold ) p_style->font_style.i_style_flags |= STYLE_BOLD;
                    if( i_italic ) p_style->font_style.i_style_flags |= STYLE_ITALIC;

                    if( i_border == 1 ) p_style->font_style.i_style_flags |= (STYLE_ITALIC | STYLE_OUTLINE);
                    else if( i_border == 3 )
                    {
                        p_style->font_style.i_style_flags |= STYLE_BACKGROUND;
                        p_style->font_style.i_background_color = p_style->font_style.i_shadow_color;
                        p_style->font_style.i_background_alpha = p_style->font_style.i_shadow_alpha;
                    }
                    p_style->font_style.i_shadow_width = i_shadow;
                    p_style->font_style.i_outline_width = i_outline;

                    p_style->i_align = 0;
                    if( i_align == 1 || i_align == 5 || i_align == 9 ) p_style->i_align |= SUBPICTURE_ALIGN_LEFT;
                    if( i_align == 3 || i_align == 7 || i_align == 11 ) p_style->i_align |= SUBPICTURE_ALIGN_RIGHT;
                    if( i_align < 4 ) p_style->i_align |= SUBPICTURE_ALIGN_BOTTOM;
                    else if( i_align < 8 ) p_style->i_align |= SUBPICTURE_ALIGN_TOP;

                    p_style->i_margin_h = ( p_style->i_align & SUBPICTURE_ALIGN_RIGHT ) ? i_margin_r : i_margin_l;
                    p_style->i_margin_v = i_margin_v;
                    p_style->i_margin_percent_h = 0;
                    p_style->i_margin_percent_v = 0;

                    p_style->font_style.i_karaoke_background_color = 0xffffff;
                    p_style->font_style.i_karaoke_background_alpha = 0xff;

                    TAB_APPEND( p_sys->i_ssa_styles, p_sys->pp_ssa_styles, p_style );
                }
                else msg_Warn( p_dec, "SSA v4 styleline parsing failed" );
            }
            else if( i_section_type == 2 ) /* V4+ */
            {
                /* Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour,
                   Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline,
                   Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
                */
                if( sscanf( psz_parser, "Style: %8192[^,],%8192[^,],%d,%8192[^,],%8192[^,],%8192[^,],%8192[^,],%d,%d,%d,%d,%d,%d,%d,%*f,%d,%d,%d,%d,%d,%d,%d%*[^\r\n]",
                    psz_temp_stylename, psz_temp_fontname, &i_font_size,
                    psz_temp_color1, psz_temp_color2, psz_temp_color3, psz_temp_color4, &i_bold, &i_italic,
                    &i_underline, &i_strikeout, &i_scale_x, &i_scale_y, &i_spacing, &i_border, &i_outline,
                    &i_shadow, &i_align, &i_margin_l, &i_margin_r, &i_margin_v ) == 21 )
                {
                    ssa_style_t *p_style = malloc( sizeof(ssa_style_t) );

                    p_style->psz_stylename = strdup( psz_temp_stylename );
                    p_style->font_style.psz_fontname = strdup( psz_temp_fontname );
                    p_style->font_style.i_font_size = i_font_size;
                    msg_Dbg( p_dec, psz_temp_color1 );
                    ParseColor( p_dec, psz_temp_color1, &p_style->font_style.i_font_color, &p_style->font_style.i_font_alpha );
                    ParseColor( p_dec, psz_temp_color3, &p_style->font_style.i_outline_color, &p_style->font_style.i_outline_alpha );
                    ParseColor( p_dec, psz_temp_color4, &p_style->font_style.i_shadow_color, &p_style->font_style.i_shadow_alpha );

                    p_style->font_style.i_style_flags = 0;
                    if( i_bold ) p_style->font_style.i_style_flags |= STYLE_BOLD;
                    if( i_italic ) p_style->font_style.i_style_flags |= STYLE_ITALIC;
                    if( i_underline ) p_style->font_style.i_style_flags |= STYLE_UNDERLINE;
                    if( i_strikeout ) p_style->font_style.i_style_flags |= STYLE_STRIKEOUT;
                    if( i_border == 1 ) p_style->font_style.i_style_flags |= (STYLE_ITALIC | STYLE_OUTLINE);
                    else if( i_border == 3 )
                    {
                        p_style->font_style.i_style_flags |= STYLE_BACKGROUND;
                        p_style->font_style.i_background_color = p_style->font_style.i_shadow_color;
                        p_style->font_style.i_background_alpha = p_style->font_style.i_shadow_alpha;
                    }
                    p_style->font_style.i_shadow_width  = ( i_border == 1 ) ? i_shadow : 0;
                    p_style->font_style.i_outline_width = ( i_border == 1 ) ? i_outline : 0;
                    p_style->font_style.i_spacing = i_spacing;
                    //p_style->font_style.f_angle = f_angle;

                    p_style->i_align = 0;
                    if( i_align == 0x1 || i_align == 0x4 || i_align == 0x7 ) p_style->i_align |= SUBPICTURE_ALIGN_LEFT;
                    if( i_align == 0x3 || i_align == 0x6 || i_align == 0x9 ) p_style->i_align |= SUBPICTURE_ALIGN_RIGHT;
                    if( i_align == 0x7 || i_align == 0x8 || i_align == 0x9 ) p_style->i_align |= SUBPICTURE_ALIGN_TOP;
                    if( i_align == 0x1 || i_align == 0x2 || i_align == 0x3 ) p_style->i_align |= SUBPICTURE_ALIGN_BOTTOM;
                    p_style->i_margin_h = ( p_style->i_align & SUBPICTURE_ALIGN_RIGHT ) ? i_margin_r : i_margin_l;
                    p_style->i_margin_v = i_margin_v;
                    p_style->i_margin_percent_h = 0;
                    p_style->i_margin_percent_v = 0;

                    p_style->font_style.i_karaoke_background_color = 0xffffff;
                    p_style->font_style.i_karaoke_background_alpha = 0xff;

                    /*TODO: Ignored: angle i_scale_x|y (fontscaling), i_encoding */
                    TAB_APPEND( p_sys->i_ssa_styles, p_sys->pp_ssa_styles, p_style );
                }
                else msg_Dbg( p_dec, "SSA V4+ styleline parsing failed" );
            }
        }
        psz_parser = GotoNextLine( psz_parser );
    }

eof:
    if( psz_header ) free( psz_header );
    return;
}

/* Function now handles tags which has attribute values, and tries
 * to deal with &' commands too. It no longer modifies the string
 * in place, so that the original text can be reused
 */
static char *StripTags( char *psz_subtitle )
{
    char *psz_text_start;
    char *psz_text;

    psz_text = psz_text_start = malloc( strlen( psz_subtitle ) + 1 );
    if( !psz_text_start )
        return NULL;

    while( *psz_subtitle )
    {
        if( *psz_subtitle == '<' )
        {
            if( strncasecmp( psz_subtitle, "<br/>", 5 ) == 0 )
                *psz_text++ = '\n';

            psz_subtitle += strcspn( psz_subtitle, ">" );
        }
        else if( *psz_subtitle == '&' )
        {
            if( !strncasecmp( psz_subtitle, "&lt;", 4 ))
            {
                *psz_text++ = '<';
                psz_subtitle += strcspn( psz_subtitle, ";" );
            }
            else if( !strncasecmp( psz_subtitle, "&gt;", 4 ))
            {
                *psz_text++ = '>';
                psz_subtitle += strcspn( psz_subtitle, ";" );
            }
            else if( !strncasecmp( psz_subtitle, "&amp;", 5 ))
            {
                *psz_text++ = '&';
                psz_subtitle += strcspn( psz_subtitle, ";" );
            }
            else if( !strncasecmp( psz_subtitle, "&quot;", 6 ))
            {
                *psz_text++ = '\"';
                psz_subtitle += strcspn( psz_subtitle, ";" );
            }
            else
            {
                /* Assume it is just a normal ampersand */
                *psz_text++ = '&';
            }
        }
        else
        {
            *psz_text++ = *psz_subtitle;
        }

        psz_subtitle++;
    }
    *psz_text = '\0';
    psz_text_start = realloc( psz_text_start, strlen( psz_text_start ) + 1 );

    return psz_text_start;
}

/* Try to respect any style tags present in the subtitle string. The main
 * problem here is a lack of adequate specs for the subtitle formats.
 * SSA/ASS and USF are both detail spec'ed -- but they are handled elsewhere.
 * SAMI has a detailed spec, but extensive rework is needed in the demux
 * code to prevent all this style information being excised, as it presently
 * does.
 * That leaves the others - none of which were (I guess) originally intended
 * to be carrying style information. Over time people have used them that way.
 * In the absence of specifications from which to work, the tags supported
 * have been restricted to the simple set permitted by the USF DTD, ie. :
 *  Basic: <br>, <i>, <b>, <u>
 *  Extended: <font>
 *    Attributes: face
 *                family
 *                size
 *                color
 *                outline-color
 *                shadow-color
 *                outline-level
 *                shadow-level
 *                back-color
 *                alpha
 * There is also the further restriction that the subtitle be well-formed
 * as an XML entity, ie. the HTML sentence:
 *        <b><i>Bold and Italics</b></i>
 * doesn't qualify because the tags aren't nested one inside the other.
 * <text> tags are automatically added to the output to ensure
 * well-formedness.
 * If the text doesn't qualify for any reason, a NULL string is
 * returned, and the rendering engine will fall back to the
 * plain text version of the subtitle.
 */
static char *CreateHtmlSubtitle( char *psz_subtitle )
{
    char    psz_tagStack[ 100 ];
    size_t  i_buf_size     = strlen( psz_subtitle ) + 100;
    char   *psz_html_start = malloc( i_buf_size );

    psz_tagStack[ 0 ] = '\0';

    if( psz_html_start != NULL )
    {
        char *psz_html = psz_html_start;

        strcpy( psz_html, "<text>" );
        psz_html += 6;

        while( *psz_subtitle )
        {
            if( *psz_subtitle == '\n' )
            {
                strcpy( psz_html, "<br/>" );
                psz_html += 5;
                psz_subtitle++;
            }
            else if( *psz_subtitle == '<' )
            {
                if( !strncasecmp( psz_subtitle, "<br/>", 5 ))
                {
                    strcpy( psz_html, "<br/>" );
                    psz_html += 5;
                    psz_subtitle += 5;
                }
                else if( !strncasecmp( psz_subtitle, "<b>", 3 ) )
                {
                    strcpy( psz_html, "<b>" );
                    strcat( psz_tagStack, "b" );
                    psz_html += 3;
                    psz_subtitle += 3;
                }
                else if( !strncasecmp( psz_subtitle, "<i>", 3 ) )
                {
                    strcpy( psz_html, "<i>" );
                    strcat( psz_tagStack, "i" );
                    psz_html += 3;
                    psz_subtitle += 3;
                }
                else if( !strncasecmp( psz_subtitle, "<u>", 3 ) )
                {
                    strcpy( psz_html, "<u>" );
                    strcat( psz_tagStack, "u" );
                    psz_html += 3;
                    psz_subtitle += 3;
                }
                else if( !strncasecmp( psz_subtitle, "<font ", 6 ))
                {
                    const char *psz_attribs[] = { "face=\"", "family=\"", "size=\"",
                            "color=\"", "outline-color=\"", "shadow-color=\"",
                            "outline-level=\"", "shadow-level=\"", "back-color=\"",
                            "alpha=\"", NULL };

                    strcpy( psz_html, "<font " );
                    strcat( psz_tagStack, "f" );
                    psz_html += 6;
                    psz_subtitle += 6;

                    while( *psz_subtitle != '>' )
                    {
                        int  k;

                        for( k=0; psz_attribs[ k ]; k++ )
                        {
                            int i_len = strlen( psz_attribs[ k ] );

                            if( !strncasecmp( psz_subtitle, psz_attribs[ k ], i_len ))
                            {
                                i_len += strcspn( psz_subtitle + i_len, "\"" ) + 1;

                                strncpy( psz_html, psz_subtitle, i_len );
                                psz_html += i_len;
                                psz_subtitle += i_len;
                                break;
                            }
                        }
                        if( psz_attribs[ k ] == NULL )
                        {
                            /* Jump over unrecognised tag */
                            int i_len = strcspn( psz_subtitle, "\"" ) + 1;

                            i_len += strcspn( psz_subtitle + i_len, "\"" ) + 1;
                            psz_subtitle += i_len;
                        }
                        while (*psz_subtitle == ' ')
                            *psz_html++ = *psz_subtitle++;
                    }
                    *psz_html++ = *psz_subtitle++;
                }
                else if( !strncmp( psz_subtitle, "</", 2 ))
                {
                    vlc_bool_t  b_match     = VLC_FALSE;
                    int         i_len       = strlen( psz_tagStack ) - 1;
                    char       *psz_lastTag = NULL;

                    if( i_len >= 0 )
                    {
                        psz_lastTag = psz_tagStack + i_len;
                        i_len = 0;

                        switch( *psz_lastTag )
                        {
                            case 'b':
                                b_match = !strncasecmp( psz_subtitle, "</b>", 4 );
                                i_len   = 4;
                                break;
                            case 'i':
                                b_match = !strncasecmp( psz_subtitle, "</i>", 4 );
                                i_len   = 4;
                                break;
                            case 'u':
                                b_match = !strncasecmp( psz_subtitle, "</u>", 4 );
                                i_len   = 4;
                                break;
                            case 'f':
                                b_match = !strncasecmp( psz_subtitle, "</font>", 7 );
                                i_len   = 7;
                                break;
                        }
                    }
                    if( ! b_match )
                    {
                        /* Not well formed -- kill everything */
                        free( psz_html_start );
                        psz_html_start = NULL;
                        break;
                    }
                    *psz_lastTag = '\0';
                    strncpy( psz_html, psz_subtitle, i_len );
                    psz_html += i_len;
                    psz_subtitle += i_len;
                }
                else
                {
                    psz_subtitle += strcspn( psz_subtitle, ">" );
                }
            }
            else if( *psz_subtitle == '&' )
            {
                if( !strncasecmp( psz_subtitle, "&lt;", 4 ))
                {
                    strcpy( psz_html, "&lt;" );
                    psz_html += 4;
                    psz_subtitle += 4;
                }
                else if( !strncasecmp( psz_subtitle, "&gt;", 4 ))
                {
                    strcpy( psz_html, "&gt;" );
                    psz_html += 4;
                    psz_subtitle += 4;
                }
                else if( !strncasecmp( psz_subtitle, "&amp;", 5 ))
                {
                    strcpy( psz_html, "&amp;" );
                    psz_html += 5;
                    psz_subtitle += 5;
                }
                else
                {
                    strcpy( psz_html, "&amp;" );
                    psz_html += 5;
                    psz_subtitle++;
                }
            }
            else
            {
                *psz_html = *psz_subtitle;
                if( psz_html > psz_html_start )
                {
                    /* Check for double whitespace */
                    if((( *psz_html == ' ' ) ||
                        ( *psz_html == '\t' )) &&
                       (( *(psz_html-1) == ' ' ) ||
                        ( *(psz_html-1) == '\t' )))
                    {
                        strcpy( psz_html, NO_BREAKING_SPACE );
                        psz_html += strlen( NO_BREAKING_SPACE ) - 1;
                    }
                }
                psz_html++;
                psz_subtitle++;
            }

            if( ( size_t )( psz_html - psz_html_start ) > i_buf_size - 10 )
            {
                int i_len = psz_html - psz_html_start;

                i_buf_size += 100;
                psz_html_start = realloc( psz_html_start, i_buf_size );
                psz_html = psz_html_start + i_len;
                *psz_html = '\0';
            }
        }
        strcpy( psz_html, "</text>" );
        psz_html += 7;

        if( psz_tagStack[ 0 ] != '\0' )
        {
            /* Not well formed -- kill everything */
            free( psz_html_start );
            psz_html_start = NULL;
        }
        else if( psz_html_start )
        {
            /* Shrink the memory requirements */
            psz_html_start = realloc( psz_html_start,  psz_html - psz_html_start + 1 );
        }
    }
    return psz_html_start;
}

/* The reverse of the above function - given a HTML subtitle, turn it
 * into a plain-text version, complete with sensible whitespace compaction
 */

static char *CreatePlainText( char *psz_subtitle )
{
    char *psz_text = StripTags( psz_subtitle );
    char *s;

    if( !psz_text )
        return NULL;

    s = strpbrk( psz_text, "\t\r\n " );
    while( s )
    {
        int   k;
        char  spc = ' ';
        int   i_whitespace = strspn( s, "\t\r\n " );

        /* Favour '\n' over other whitespaces - if one of these
         * occurs in the whitespace use a '\n' as our value,
         * otherwise just use a ' '
         */
        for( k = 0; k < i_whitespace; k++ )
            if( s[k] == '\n' ) spc = '\n';

        if( i_whitespace > 1 )
        {
            memmove( &s[1],
                     &s[i_whitespace],
                     strlen( s ) - i_whitespace + 1 );
        }
        *s++ = spc;

        s = strpbrk( s, "\t\r\n " );
    }
    return psz_text;
}

/****************************************************************************
 * download and resize image located at psz_url
 ***************************************************************************/
static subpicture_region_t *LoadEmbeddedImage( decoder_t *p_dec, subpicture_t *p_spu, const char *psz_filename, int i_transparent_color )
{
    decoder_sys_t         *p_sys = p_dec->p_sys;
    subpicture_region_t   *p_region;
    video_format_t         fmt_out;
    int                    k;
    picture_t             *p_pic = NULL;

    for( k = 0; k < p_sys->i_images; k++ )
    {
        if( p_sys->pp_images &&
            !strcmp( p_sys->pp_images[k]->psz_filename, psz_filename ) )
        {
            p_pic = p_sys->pp_images[k]->p_pic;
            break;
        }
    }

    if( !p_pic )
    {
        msg_Err( p_dec, "Unable to read image %s", psz_filename );
        return NULL;
    }

    /* Display the feed's image */
    memset( &fmt_out, 0, sizeof( video_format_t));

    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
    fmt_out.i_aspect = VOUT_ASPECT_FACTOR;
    fmt_out.i_sar_num = fmt_out.i_sar_den = 1;
    fmt_out.i_width =
        fmt_out.i_visible_width = p_pic->p[Y_PLANE].i_visible_pitch;
    fmt_out.i_height =
        fmt_out.i_visible_height = p_pic->p[Y_PLANE].i_visible_lines;

    p_region = p_spu->pf_create_region( VLC_OBJECT(p_dec), &fmt_out );
    if( !p_region )
    {
        msg_Err( p_dec, "cannot allocate SPU region" );
        return NULL;
    }
    vout_CopyPicture( p_dec, &p_region->picture, p_pic );

    /* This isn't the best way to do this - if you really want transparency, then
     * you're much better off using an image type that supports it like PNG. The
     * spec requires this support though.
     */
    if( i_transparent_color > 0 )
    {
        uint8_t i_r = ( i_transparent_color >> 16 ) & 0xff;
        uint8_t i_g = ( i_transparent_color >>  8 ) & 0xff;
        uint8_t i_b = ( i_transparent_color       ) & 0xff;
        uint8_t i_y = ( ( (  66 * i_r + 129 * i_g +  25 * i_b + 128 ) >> 8 ) + 16 );
        uint8_t i_u =   ( ( -38 * i_r -  74 * i_g + 112 * i_b + 128 ) >> 8 ) + 128 ;
        uint8_t i_v =   ( ( 112 * i_r -  94 * i_g -  18 * i_b + 128 ) >> 8 ) + 128 ;

        if( ( p_region->picture.Y_PITCH == p_region->picture.U_PITCH ) &&
            ( p_region->picture.Y_PITCH == p_region->picture.V_PITCH ) &&
            ( p_region->picture.Y_PITCH == p_region->picture.A_PITCH ) )
        {
            int i_lines = p_region->picture.p[ Y_PLANE ].i_lines;
            if( i_lines > p_region->picture.p[ U_PLANE ].i_lines )
                i_lines = p_region->picture.p[ U_PLANE ].i_lines;
            if( i_lines > p_region->picture.p[ V_PLANE ].i_lines )
                i_lines = p_region->picture.p[ V_PLANE ].i_lines;
            if( i_lines > p_region->picture.p[ A_PLANE ].i_lines )
                i_lines = p_region->picture.p[ A_PLANE ].i_lines;

            int   i;

            for( i = 0; i < p_region->picture.A_PITCH * i_lines; i++ )
            {
                if(( p_region->picture.Y_PIXELS[ i ] == i_y ) &&
                   ( p_region->picture.U_PIXELS[ i ] == i_u ) &&
                   ( p_region->picture.V_PIXELS[ i ] == i_v ) )
                {
                    p_region->picture.A_PIXELS[ i ] = 1;
                }
            }
        }
    }
    return p_region;
}
