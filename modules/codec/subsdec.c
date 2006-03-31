/*****************************************************************************
 * subsdec.c : text subtitles decoder
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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
#include <vlc/vout.h>
#include <vlc/decoder.h>

#include "vlc_osd.h"
#include "vlc_filter.h"
#include "charset.h"

typedef struct
{
    char *          psz_stylename; /* The name of the style, no comma's allowed */
    text_style_t    font_style;
    int             i_align;
    int             i_margin_h;
    int             i_margin_v;
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
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static subpicture_t *DecodeBlock   ( decoder_t *, block_t ** );
static subpicture_t *ParseText     ( decoder_t *, block_t * );
static void         ParseSSAHeader ( decoder_t * );
static void         ParseSSAString ( decoder_t *, char *, subpicture_t * );
static void         ParseColor     ( decoder_t *, char *, int *, int * );
static void         StripTags      ( char * );

#define DEFAULT_NAME "Default"
#define MAX_LINE 8192

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static char *ppsz_encodings[] = { DEFAULT_NAME, "ASCII", "UTF-8", "",
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

static int  pi_justification[] = { 0, 1, 2 };
static char *ppsz_justification_text[] = {N_("Center"),N_("Left"),N_("Right")};

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
        p_dec->fmt_in.i_codec != VLC_FOURCC('s','s','a',' ') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_decode_sub = DecodeBlock;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
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
    p_sys->pp_ssa_styles = NULL;
    p_sys->i_ssa_styles = 0;

    if( p_dec->fmt_in.subs.psz_encoding && *p_dec->fmt_in.subs.psz_encoding )
    {
        msg_Dbg( p_dec, "using character encoding: %s",
                 p_dec->fmt_in.subs.psz_encoding );
        if( strcmp( p_dec->fmt_in.subs.psz_encoding, "UTF-8" ) )
            p_sys->iconv_handle = vlc_iconv_open( "UTF-8", p_dec->fmt_in.subs.psz_encoding );
    }
    else
    {
        var_Create( p_dec, "subsdec-encoding",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_dec, "subsdec-encoding", &val );
        if( !strcmp( val.psz_string, DEFAULT_NAME ) )
        {
            const char *psz_charset = GetFallbackEncoding();

            p_sys->b_autodetect_utf8 = var_CreateGetBool( p_dec,
                    "subsdec-autodetect-utf8" );

            p_sys->iconv_handle = vlc_iconv_open( "UTF-8", psz_charset );
            msg_Dbg( p_dec, "using default character encoding: %s", psz_charset );
        }
        else if( !strcmp( val.psz_string, "UTF-8" ) )
        {
            msg_Dbg( p_dec, "using character encoding: UTF-8" );
        }
        else if( val.psz_string )
        {
            msg_Dbg( p_dec, "using character encoding: %s", val.psz_string );
            p_sys->iconv_handle = vlc_iconv_open( "UTF-8", val.psz_string );
            if( p_sys->iconv_handle == (vlc_iconv_t)-1 )
            {
                msg_Warn( p_dec, "unable to do requested conversion" );
            }
        }
        if( val.psz_string ) free( val.psz_string );
    }

    var_Create( p_dec, "subsdec-align", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "subsdec-align", &val );
    p_sys->i_align = val.i_int;

    if( p_dec->fmt_in.i_codec == VLC_FOURCC('s','s','a',' ') && var_CreateGetBool( p_dec, "subsdec-formatted" ) )
    {
        if( p_dec->fmt_in.i_extra > 0 )
            ParseSSAHeader( p_dec );
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
    {
        vlc_iconv_close( p_sys->iconv_handle );
    }

    if( p_sys->pp_ssa_styles )
    {
        int i;
        for( i = 0; i < p_sys->i_ssa_styles; i++ )
        {
            if( p_sys->pp_ssa_styles[i]->psz_stylename ) free( p_sys->pp_ssa_styles[i]->psz_stylename );
            p_sys->pp_ssa_styles[i]->psz_stylename = NULL;
            if( p_sys->pp_ssa_styles[i]->font_style.psz_fontname ) free( p_sys->pp_ssa_styles[i]->font_style.psz_fontname );
            p_sys->pp_ssa_styles[i]->font_style.psz_fontname = NULL;
            if( p_sys->pp_ssa_styles[i] ) free( p_sys->pp_ssa_styles[i] ); p_sys->pp_ssa_styles[i] = NULL;
        }
        free( p_sys->pp_ssa_styles ); p_sys->pp_ssa_styles = NULL;
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
    if( p_block->i_buffer <= 1 || p_block->p_buffer[0] == '\0' )
    {
        msg_Warn( p_dec, "empty subtitle" );
        return NULL;
    }

    /* Should be resiliant against bad subtitles */
    psz_subtitle = strndup( (const char *)p_block->p_buffer,
                            p_block->i_buffer );
    if( psz_subtitle == NULL )
        return NULL;

    if( p_sys->iconv_handle == (vlc_iconv_t)-1 )
        EnsureUTF8( psz_subtitle );
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
    if( p_dec->fmt_in.i_codec != VLC_FOURCC('s','s','a',' ') )
    {
        /* Normal text subs, easy markup */
        p_spu->i_flags = SUBPICTURE_ALIGN_BOTTOM | p_sys->i_align;
        p_spu->i_x = p_sys->i_align ? 20 : 0;
        p_spu->i_y = 10;

        /* Remove formatting from string */
        StripTags( psz_subtitle );

        p_spu->p_region->psz_text = psz_subtitle;
        p_spu->i_start = p_block->i_pts;
        p_spu->i_stop = p_block->i_pts + p_block->i_length;
        p_spu->b_ephemer = (p_block->i_length == 0);
        p_spu->b_absolute = VLC_FALSE;
    }
    else
    {
        /* Decode SSA strings */
        ParseSSAString( p_dec, psz_subtitle, p_spu );
        p_spu->i_start = p_block->i_pts;
        p_spu->i_stop = p_block->i_pts + p_block->i_length;
        p_spu->b_ephemer = (p_block->i_length == 0);
        p_spu->b_absolute = VLC_FALSE;
        p_spu->i_original_picture_width = p_sys->i_original_width;
        p_spu->i_original_picture_height = p_sys->i_original_height;
        if( psz_subtitle ) free( psz_subtitle );
    }
    return p_spu;
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

    i_comma = 0;
    while( i_comma < 8 && *psz_buffer_sub != '\0' )
    {
        if( *psz_buffer_sub == ',' )
        {
            i_comma++;
            if( i_comma == 2 ) psz_style_start = &psz_buffer_sub[1];
            if( i_comma == 3 ) psz_style_end = &psz_buffer_sub[0];
            if( i_comma == 4 ) i_margin_l = (int)strtol( psz_buffer_sub+1, NULL, 10 );
            if( i_comma == 5 ) i_margin_r = (int)strtol( psz_buffer_sub+1, NULL, 10 );
            if( i_comma == 6 ) i_margin_v = (int)strtol( psz_buffer_sub+1, NULL, 10 );
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
    psz_style = (char *)malloc( i_strlen + 1);
    psz_style = memcpy( psz_style, psz_style_start, i_strlen );
    psz_style[i_strlen] = '\0';

    for( i = 0; i < p_sys->i_ssa_styles; i++ )
    {
        if( !strcmp( p_sys->pp_ssa_styles[i]->psz_stylename, psz_style ) )
            p_style = p_sys->pp_ssa_styles[i];
    }
    if( psz_style ) free( psz_style );

    p_spu->p_region->psz_text = psz_new_subtitle;
    if( p_style == NULL )
    {
        p_spu->i_flags = SUBPICTURE_ALIGN_BOTTOM | p_sys->i_align;
        p_spu->i_x = p_sys->i_align ? 20 : 0;
        p_spu->i_y = 10;
    }
    else
    {
        msg_Dbg( p_dec, "style is: %s", p_style->psz_stylename);
        p_spu->p_region->p_style = &p_style->font_style;
        p_spu->i_flags = p_style->i_align;
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
                    if( i_align == 0x1 || i_align == 0x4 || i_align == 0x1 ) p_style->i_align |= SUBPICTURE_ALIGN_LEFT;
                    if( i_align == 0x3 || i_align == 0x6 || i_align == 0x9 ) p_style->i_align |= SUBPICTURE_ALIGN_RIGHT;
                    if( i_align == 0x7 || i_align == 0x8 || i_align == 0x9 ) p_style->i_align |= SUBPICTURE_ALIGN_TOP;
                    if( i_align == 0x1 || i_align == 0x2 || i_align == 0x3 ) p_style->i_align |= SUBPICTURE_ALIGN_BOTTOM;
                    p_style->i_margin_h = ( p_style->i_align & SUBPICTURE_ALIGN_RIGHT ) ? i_margin_r : i_margin_l;
                    p_style->i_margin_v = i_margin_v;

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

static void StripTags( char *psz_text )
{
    int i_left_moves = 0;
    vlc_bool_t b_inside_tag = VLC_FALSE;
    int i = 0;
    int i_tag_start = -1;
    while( psz_text[ i ] )
    {
        if( !b_inside_tag )
        {
            if( psz_text[ i ] == '<' )
            {
                b_inside_tag = VLC_TRUE;
                i_tag_start = i;
            }
            psz_text[ i - i_left_moves ] = psz_text[ i ];
        }
        else
        {
            if( ( psz_text[ i ] == ' ' ) ||
                ( psz_text[ i ] == '\t' ) ||
                ( psz_text[ i ] == '\n' ) ||
                ( psz_text[ i ] == '\r' ) )
            {
                b_inside_tag = VLC_FALSE;
                i_tag_start = -1;
            }
            else if( psz_text[ i ] == '>' )
            {
                i_left_moves += i - i_tag_start + 1;
                i_tag_start = -1;
                b_inside_tag = VLC_FALSE;
            }
            else
            {
                psz_text[ i - i_left_moves ] = psz_text[ i ];
            }
        }
        i++;
    }
    psz_text[ i - i_left_moves ] = '\0';
}
