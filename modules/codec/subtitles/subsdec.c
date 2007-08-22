/*****************************************************************************
 * subsdec.c : text subtitles decoder
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id: subsdec.c 20996 2007-08-05 20:01:21Z jb $
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

#include "subsdec.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static subpicture_t   *DecodeBlock   ( decoder_t *, block_t ** );
static subpicture_t   *ParseText     ( decoder_t *, block_t * );
static char           *StripTags      ( char * );
static char           *CreateHtmlSubtitle ( char * );
static int            ParseImageAttachments( decoder_t *p_dec );

static int            ParsePositionAttributeList( char *, int *, int *, int * );

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

char *GrabAttributeValue( const char *psz_attribute,
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

static int ParsePositionAttributeList( char *psz_subtitle, int *i_align,
                                       int *i_x, int *i_y )
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

void SetupPositions( subpicture_region_t *p_region, char *psz_subtitle )
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

subpicture_region_t *CreateTextRegion( decoder_t *p_dec,
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
                    if( module_Exists( p_dec, "sdl_image" ) )
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

char* GotoNextLine( char *psz_text )
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

char *CreatePlainText( char *psz_subtitle )
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
subpicture_region_t *LoadEmbeddedImage( decoder_t *p_dec,
                                        subpicture_t *p_spu,
                                        const char *psz_filename,
                                        int i_transparent_color )
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
