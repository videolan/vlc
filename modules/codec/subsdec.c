/*****************************************************************************
 * subsdec.c : text subtitle decoder
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Bernie Purcell <bitmap@videolan.org>
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

#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_charset.h>

#include "substext.h"

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static const char *const ppsz_encodings[] = {
    "",
    "system",
    "UTF-8",
    "UTF-16",
    "UTF-16BE",
    "UTF-16LE",
    "GB18030",
    "ISO-8859-15",
    "Windows-1252",
    "IBM850",
    "ISO-8859-2",
    "Windows-1250",
    "ISO-8859-3",
    "ISO-8859-10",
    "Windows-1251",
    "KOI8-R",
    "KOI8-U",
    "ISO-8859-6",
    "Windows-1256",
    "ISO-8859-7",
    "Windows-1253",
    "ISO-8859-8",
    "Windows-1255",
    "ISO-8859-9",
    "Windows-1254",
    "ISO-8859-11",
    "Windows-874",
    "ISO-8859-13",
    "Windows-1257",
    "ISO-8859-14",
    "ISO-8859-16",
    "ISO-2022-CN-EXT",
    "EUC-CN",
    "ISO-2022-JP-2",
    "EUC-JP",
    "Shift_JIS",
    "CP949",
    "ISO-2022-KR",
    "Big5",
    "ISO-2022-TW",
    "Big5-HKSCS",
    "VISCII",
    "Windows-1258",
};

static const char *const ppsz_encoding_names[] = {
    /* xgettext:
      The character encoding name in parenthesis corresponds to that used for
      the GetACP translation. "Windows-1252" applies to Western European
      languages using the Latin alphabet. */
    N_("Default (Windows-1252)"),
    N_("System codeset"),
    N_("Universal (UTF-8)"),
    N_("Universal (UTF-16)"),
    N_("Universal (big endian UTF-16)"),
    N_("Universal (little endian UTF-16)"),
    N_("Universal, Chinese (GB18030)"),

  /* ISO 8859 and the likes */
    /* 1 */
    N_("Western European (Latin-9)"), /* mostly superset of Latin-1 */
    N_("Western European (Windows-1252)"),
    N_("Western European (IBM 00850)"),
    /* 2 */
    N_("Eastern European (Latin-2)"),
    N_("Eastern European (Windows-1250)"),
    /* 3 */
    N_("Esperanto (Latin-3)"),
    /* 4 */
    N_("Nordic (Latin-6)"), /* Latin 6 supersedes Latin 4 */
    /* 5 */
    N_("Cyrillic (Windows-1251)"), /* ISO 8859-5 is not practically used */
    N_("Russian (KOI8-R)"),
    N_("Ukrainian (KOI8-U)"),
    /* 6 */
    N_("Arabic (ISO 8859-6)"),
    N_("Arabic (Windows-1256)"),
    /* 7 */
    N_("Greek (ISO 8859-7)"),
    N_("Greek (Windows-1253)"),
    /* 8 */
    N_("Hebrew (ISO 8859-8)"),
    N_("Hebrew (Windows-1255)"),
    /* 9 */
    N_("Turkish (ISO 8859-9)"),
    N_("Turkish (Windows-1254)"),
    /* 10 -> 4 */
    /* 11 */
    N_("Thai (TIS 620-2533/ISO 8859-11)"),
    N_("Thai (Windows-874)"),
    /* 13 */
    N_("Baltic (Latin-7)"),
    N_("Baltic (Windows-1257)"),
    /* 12 -> /dev/null */
    /* 14 */
    N_("Celtic (Latin-8)"),
    /* 15 -> 1 */
    /* 16 */
    N_("South-Eastern European (Latin-10)"),
  /* CJK families */
    N_("Simplified Chinese (ISO-2022-CN-EXT)"),
    N_("Simplified Chinese Unix (EUC-CN)"),
    N_("Japanese (7-bits JIS/ISO-2022-JP-2)"),
    N_("Japanese Unix (EUC-JP)"),
    N_("Japanese (Shift JIS)"),
    N_("Korean (EUC-KR/CP949)"),
    N_("Korean (ISO-2022-KR)"),
    N_("Traditional Chinese (Big5)"),
    N_("Traditional Chinese Unix (EUC-TW)"),
    N_("Hong-Kong Supplementary (HKSCS)"),
  /* Other */
    N_("Vietnamese (VISCII)"),
    N_("Vietnamese (Windows-1258)"),
};

static const int  pi_justification[] = { 0, 1, 2 };
static const char *const ppsz_justification_text[] = {
    N_("Center"),N_("Left"),N_("Right")};

#define ENCODING_TEXT N_("Subtitle text encoding")
#define ENCODING_LONGTEXT N_("Set the encoding used in text subtitles")
#define ALIGN_TEXT N_("Subtitle justification")
#define ALIGN_LONGTEXT N_("Set the justification of subtitles")
#define AUTODETECT_UTF8_TEXT N_("UTF-8 subtitle autodetection")
#define AUTODETECT_UTF8_LONGTEXT N_("This enables automatic detection of " \
            "UTF-8 encoding within subtitle files.")
#define FORMAT_TEXT N_("Formatted Subtitles")
#define FORMAT_LONGTEXT N_("Some subtitle formats allow for text formatting. " \
 "VLC partly implements this, but you can choose to disable all formatting.")

static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Subtitles"))
    set_description( N_("Text subtitle decoder") )
    set_capability( "decoder", 50 )
    set_callbacks( OpenDecoder, CloseDecoder )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )

    add_integer( "subsdec-align", 0, ALIGN_TEXT, ALIGN_LONGTEXT,
                 false )
        change_integer_list( pi_justification, ppsz_justification_text )
    add_string( "subsdec-encoding", "",
                ENCODING_TEXT, ENCODING_LONGTEXT, false )
        change_string_list( ppsz_encodings, ppsz_encoding_names )
    add_bool( "subsdec-autodetect-utf8", true,
              AUTODETECT_UTF8_TEXT, AUTODETECT_UTF8_LONGTEXT, false )
    add_bool( "subsdec-formatted", true, FORMAT_TEXT, FORMAT_LONGTEXT,
                 false )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define NO_BREAKING_SPACE  "&#160;"

struct decoder_sys_t
{
    int                 i_align;          /* Subtitles alignment on the vout */

    vlc_iconv_t         iconv_handle;            /* handle to iconv instance */
    bool                b_autodetect_utf8;
};


static subpicture_t   *DecodeBlock   ( decoder_t *, block_t ** );
static subpicture_t   *ParseText     ( decoder_t *, block_t * );
static char           *StripTags      ( char * );
static char           *CreateHtmlSubtitle( int *pi_align, char * );

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

    switch( p_dec->fmt_in.i_codec )
    {
        case VLC_CODEC_SUBT:
        case VLC_CODEC_ITU_T140:
            break;
        default:
            return VLC_EGENERIC;
    }

    p_dec->pf_decode_sub = DecodeBlock;
    p_dec->fmt_out.i_cat = SPU_ES;
    p_dec->fmt_out.i_codec = 0;

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    /* init of p_sys */
    p_sys->i_align = 0;
    p_sys->iconv_handle = (vlc_iconv_t)-1;
    p_sys->b_autodetect_utf8 = false;

    const char *encoding;
    char *var = NULL;

    /* First try demux-specified encoding */
    if( p_dec->fmt_in.i_codec == VLC_CODEC_ITU_T140 )
        encoding = "UTF-8"; /* IUT T.140 is always using UTF-8 */
    else
    if( p_dec->fmt_in.subs.psz_encoding && *p_dec->fmt_in.subs.psz_encoding )
    {
        encoding = p_dec->fmt_in.subs.psz_encoding;
        msg_Dbg (p_dec, "trying demuxer-specified character encoding: %s",
                 encoding);
    }
    else
    {
        /* Second, try configured encoding */
        if ((var = var_InheritString (p_dec, "subsdec-encoding")) != NULL)
        {
            msg_Dbg (p_dec, "trying configured character encoding: %s", var);
            if (!strcmp (var, "system"))
            {
                free (var);
                var = NULL;
                encoding = "";
                /* ^ iconv() treats "" as nl_langinfo(CODESET) */
            }
            else
                encoding = var;
        }
        else
        /* Third, try "local" encoding */
        {
        /* xgettext:
           The Windows ANSI code page most commonly used for this language.
           VLC uses this as a guess of the subtitle files character set
           (if UTF-8 and UTF-16 autodetection fails).
           Western European languages normally use "CP1252", which is a
           Microsoft-variant of ISO 8859-1. That suits the Latin alphabet.
           Other scripts use other code pages.

           This MUST be a valid iconv character set. If unsure, please refer
           the VideoLAN translators mailing list. */
            encoding = vlc_pgettext("GetACP", "CP1252");
            msg_Dbg (p_dec, "trying default character encoding: %s", encoding);
        }

        /* Check UTF-8 autodetection */
        if (var_InheritBool (p_dec, "subsdec-autodetect-utf8"))
        {
            msg_Dbg (p_dec, "using automatic UTF-8 detection");
            p_sys->b_autodetect_utf8 = true;
        }
    }

    if (strcasecmp (encoding, "UTF-8") && strcasecmp (encoding, "utf8"))
    {
        p_sys->iconv_handle = vlc_iconv_open ("UTF-8", encoding);
        if (p_sys->iconv_handle == (vlc_iconv_t)(-1))
            msg_Err (p_dec, "cannot convert from %s: %m", encoding);
    }
    free (var);

    p_sys->i_align = var_InheritInteger( p_dec, "subsdec-align" );

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete subtitles units.
 ****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    subpicture_t *p_spu;
    block_t *p_block;

    if( !pp_block || *pp_block == NULL )
        return NULL;

    p_block = *pp_block;
    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        block_Release( p_block );
        return NULL;
    }

    p_spu = ParseText( p_dec, p_block );

    block_Release( p_block );
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

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts <= VLC_TS_INVALID )
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
    psz_subtitle = malloc( p_block->i_buffer + 1 );
    if( psz_subtitle == NULL )
        return NULL;
    memcpy( psz_subtitle, p_block->p_buffer, p_block->i_buffer );
    psz_subtitle[p_block->i_buffer] = '\0';

    if( p_sys->iconv_handle == (vlc_iconv_t)-1 )
    {
        if (EnsureUTF8( psz_subtitle ) == NULL)
        {
            msg_Err( p_dec, "failed to convert subtitle encoding.\n"
                     "Try manually setting a character-encoding "
                     "before you open the file." );
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
                p_sys->b_autodetect_utf8 = false;
            }
        }

        if( !p_sys->b_autodetect_utf8 )
        {
            size_t inbytes_left = strlen( psz_subtitle );
            size_t outbytes_left = 6 * inbytes_left;
            char *psz_new_subtitle = xmalloc( outbytes_left + 1 );
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
                msg_Err( p_dec, "failed to convert subtitle encoding.\n"
                        "Try manually setting a character-encoding "
                                "before you open the file." );
                return NULL;
            }

            psz_subtitle = realloc( psz_new_subtitle,
                                    psz_convert_buffer_out - psz_new_subtitle );
            if( !psz_subtitle )
                psz_subtitle = psz_new_subtitle;
        }
    }

    /* Create the subpicture unit */
    p_spu = decoder_NewSubpictureText( p_dec );
    if( !p_spu )
    {
        free( psz_subtitle );
        return NULL;
    }
    p_spu->i_start    = p_block->i_pts;
    p_spu->i_stop     = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer  = (p_block->i_length == 0);
    p_spu->b_absolute = false;

    subpicture_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;

    p_spu_sys->align = SUBPICTURE_ALIGN_BOTTOM | p_sys->i_align;
    p_spu_sys->text  = StripTags( psz_subtitle );
    if( var_InheritBool( p_dec, "subsdec-formatted" ) )
        p_spu_sys->html = CreateHtmlSubtitle( &p_spu_sys->align, psz_subtitle );

    free( psz_subtitle );

    return p_spu;
}

/* Function now handles tags with attribute values, and tries
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

        /* Security fix: Account for the case where input ends early */
        if( *psz_subtitle == '\0' ) break;

        psz_subtitle++;
    }
    *psz_text = '\0';
    char *psz = realloc( psz_text_start, strlen( psz_text_start ) + 1 );
    if( psz ) psz_text_start = psz;

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
 *  Basic: <br>, <i>, <b>, <u>, <s>
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
/* TODO: highly suboptimal, offset should be cached */
static void HtmlNPut( char **ppsz_html, const char *psz_text, int i_max )
{
    char *psz_html = *ppsz_html;
    if( psz_html == NULL )
        return;

    const size_t i_offset = strlen(psz_html);
    const size_t i_len = strnlen(psz_text, i_max);

    psz_html = realloc( psz_html, i_offset + i_len + 1 );
    if( psz_html != NULL )
    {
        memcpy( psz_html + i_offset, psz_text, i_len );
        psz_html[i_offset + i_len] = '\0';
    }
    else
        free( *ppsz_html );
    *ppsz_html = psz_html;
}

static void HtmlPut( char **ppsz_html, const char *psz_text )
{
    HtmlNPut( ppsz_html, psz_text, INT_MAX );
}

static void HtmlCopy( char **ppsz_html, char **ppsz_subtitle, const char *psz_text )
{
    HtmlPut( ppsz_html, psz_text );
    *ppsz_subtitle += strlen(psz_text);
}

static char *CreateHtmlSubtitle( int *pi_align, char *psz_subtitle )
{
    char *psz_tag = malloc( 1 );
    if( psz_tag == NULL )
        return NULL;

    char *psz_html = malloc( 1 );
    if( psz_html == NULL )
    {
        free( psz_tag );
        return NULL;
    }
    psz_tag[0] = '\0';
    psz_html[0] = '\0';

    bool b_has_align = false;

    HtmlPut( &psz_html, "<text>" );

    /* */
    while( *psz_subtitle )
    {
        if( *psz_subtitle == '\n' )
        {
            HtmlPut( &psz_html, "<br/>" );
            psz_subtitle++;
        }
        else if( *psz_subtitle == '<' )
        {
            if( !strncasecmp( psz_subtitle, "<br/>", 5 ))
            {
                HtmlCopy( &psz_html, &psz_subtitle, "<br/>" );
            }
            else if( !strncasecmp( psz_subtitle, "<b>", 3 ) )
            {
                HtmlCopy( &psz_html, &psz_subtitle, "<b>" );
                HtmlPut( &psz_tag, "b" );
            }
            else if( !strncasecmp( psz_subtitle, "<i>", 3 ) )
            {
                HtmlCopy( &psz_html, &psz_subtitle, "<i>" );
                HtmlPut( &psz_tag, "i" );
            }
            else if( !strncasecmp( psz_subtitle, "<u>", 3 ) )
            {
                HtmlCopy( &psz_html, &psz_subtitle, "<u>" );
                HtmlPut( &psz_tag, "u" );
            }
            else if( !strncasecmp( psz_subtitle, "<s>", 3 ) )
            {
                HtmlCopy( &psz_html, &psz_subtitle, "<s>" );
                HtmlPut( &psz_tag, "s" );
            }
            else if( !strncasecmp( psz_subtitle, "<font ", 6 ))
            {
                const char *psz_attribs[] = { "face=", "family=", "size=",
                        "color=", "outline-color=", "shadow-color=",
                        "outline-level=", "shadow-level=", "back-color=",
                        "alpha=", NULL };

                HtmlCopy( &psz_html, &psz_subtitle, "<font " );
                HtmlPut( &psz_tag, "f" );

                while( *psz_subtitle != '>' )
                {
                    int  k;

                    /* <font       color= */
                    while (*psz_subtitle == ' ')
                        psz_subtitle++;

                    for( k=0; psz_attribs[ k ]; k++ )
                    {
                        int i_len = strlen( psz_attribs[ k ] );

                        if( !strncasecmp( psz_subtitle, psz_attribs[k], i_len ) )
                        {
                            /* */
                            HtmlPut( &psz_html, psz_attribs[k] );
                            psz_subtitle += i_len;

                            /* <font       color=   red */
                            while (*psz_subtitle == ' ')
                                psz_subtitle++;

                            /* */
                            if( *psz_subtitle == '"' )
                            {
                                psz_subtitle++;
                                i_len = strcspn( psz_subtitle, "\"" );
                            }
                            else if( *psz_subtitle == '\'' )
                            {
                                psz_subtitle++;
                                i_len = strcspn( psz_subtitle, "'" );
                            }
                            else
                            {
                                i_len = strcspn( psz_subtitle, " \t>" );
                            }
                            HtmlPut( &psz_html, "\"" );
                            HtmlNPut( &psz_html, psz_subtitle, i_len );
                            HtmlPut( &psz_html, "\"" );

                            psz_subtitle += i_len;
                            if( *psz_subtitle == '\"' || *psz_subtitle == '\'' )
                                psz_subtitle++;
                            break;
                        }
                    }
                    if( psz_attribs[ k ] == NULL )
                    {
                        /* Jump over unrecognised tag */
                        int i_len = strcspn( psz_subtitle, "\"" );
                        if( psz_subtitle[i_len] == '\"' )
                        {
                            i_len += 1 + strcspn( &psz_subtitle[i_len + 1], "\"" );
                            if( psz_subtitle[i_len] == '\"' )
                                i_len++;
                        }
                        /* Not a tag, something else we do not understand */
                        if( i_len == 0 )
                            psz_subtitle++;

                        psz_subtitle += i_len;
                    }

                    HtmlNPut( &psz_html, psz_subtitle, strspn(psz_subtitle, " ") );
                }
                HtmlPut( &psz_html, ">" );
                psz_subtitle++;
            }
            else if( !strncmp( psz_subtitle, "</", 2 ))
            {
                bool   b_match     = false;
                bool   b_ignore    = false;
                int    i_len       = (psz_tag ? strlen(psz_tag) : 0) - 1;
                char  *psz_lastTag = NULL;

                if( i_len >= 0 )
                {
                    psz_lastTag = psz_tag + i_len;
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
                    case 's':
                        b_match = !strncasecmp( psz_subtitle, "</s>", 4 );
                        i_len   = 4;
                        break;
                    case 'f':
                        b_match = !strncasecmp( psz_subtitle, "</font>", 7 );
                        i_len   = 7;
                        break;
                    case 'I':
                        i_len = strcspn( psz_subtitle, ">" );
                        b_match = psz_subtitle[i_len] == '>';
                        b_ignore = true;
                        if( b_match )
                            i_len++;
                        break;
                    }
                }
                if( !b_match )
                {
                    /* Not well formed -- kill everything */
                    free( psz_html );
                    psz_html = NULL;
                    break;
                }
                *psz_lastTag = '\0';
                if( !b_ignore )
                    HtmlNPut( &psz_html, psz_subtitle, i_len );

                psz_subtitle += i_len;
            }
            else if( ( psz_subtitle[1] < 'a' || psz_subtitle[1] > 'z' ) &&
                     ( psz_subtitle[1] < 'A' || psz_subtitle[1] > 'Z' ) )
            {
                /* We have a single < */
                HtmlPut( &psz_html, "&lt;" );
                psz_subtitle++;
            }
            else
            {
                /* We have an unknown tag or a single < */

                /* Search for the next tag or end of tag or end of string */
                char *psz_stop = psz_subtitle + 1 + strcspn( &psz_subtitle[1], "<>" );
                char *psz_closing = strstr( psz_subtitle, "/>" );

                if( psz_closing && psz_closing < psz_stop )
                {
                    /* We have a self closed tag, remove it */
                    psz_subtitle = &psz_closing[2];
                }
                else if( *psz_stop == '>' )
                {
                    char psz_match[256];

                    snprintf( psz_match, sizeof(psz_match), "</%s", &psz_subtitle[1] );
                    psz_match[strcspn( psz_match, " \t>" )] = '\0';

                    if( strstr( psz_subtitle, psz_match ) )
                    {
                        /* We have the closing tag, ignore it TODO */
                        psz_subtitle = &psz_stop[1];
                        HtmlPut( &psz_tag, "I" );
                    }
                    else
                    {
                        int i_len = psz_stop + 1 - psz_subtitle;

                        /* Copy the whole data */
                        for( ; i_len > 0; i_len--, psz_subtitle++ )
                        {
                            if( *psz_subtitle == '<' )
                                HtmlPut( &psz_html, "&lt;" );
                            else if( *psz_subtitle == '>' )
                                HtmlPut( &psz_html, "&gt;" );
                            else
                                HtmlNPut( &psz_html, psz_subtitle, 1 );
                        }
                    }
                }
                else
                {
                    /* We have a single < */
                    HtmlPut( &psz_html, "&lt;" );
                    psz_subtitle++;
                }
            }
        }
        else if( *psz_subtitle == '&' )
        {
            if( !strncasecmp( psz_subtitle, "&lt;", 4 ))
            {
                HtmlCopy( &psz_html, &psz_subtitle, "&lt;" );
            }
            else if( !strncasecmp( psz_subtitle, "&gt;", 4 ))
            {
                HtmlCopy( &psz_html, &psz_subtitle, "&gt;" );
            }
            else if( !strncasecmp( psz_subtitle, "&amp;", 5 ))
            {
                HtmlCopy( &psz_html, &psz_subtitle, "&amp;" );
            }
            else
            {
                HtmlPut( &psz_html, "&amp;" );
                psz_subtitle++;
            }
        }
        else if( *psz_subtitle == '>' )
        {
            HtmlPut( &psz_html, "&gt;" );
            psz_subtitle++;
        }
        else if( psz_subtitle[0] == '{' && psz_subtitle[1] == '\\' &&
                 strchr( psz_subtitle, '}' ) )
        {
            /* Check for forced alignment */
            if( !b_has_align &&
                !strncmp( psz_subtitle, "{\\an", 4 ) && psz_subtitle[4] >= '1' && psz_subtitle[4] <= '9' && psz_subtitle[5] == '}' )
            {
                static const int pi_vertical[3] = { SUBPICTURE_ALIGN_BOTTOM, 0, SUBPICTURE_ALIGN_TOP };
                static const int pi_horizontal[3] = { SUBPICTURE_ALIGN_LEFT, 0, SUBPICTURE_ALIGN_RIGHT };
                const int i_id = psz_subtitle[4] - '1';

                b_has_align = true;
                *pi_align = pi_vertical[i_id/3] | pi_horizontal[i_id%3];
            }
            /* TODO fr -> rotation */

            /* Hide {\stupidity} */
            psz_subtitle = strchr( psz_subtitle, '}' ) + 1;
        }
        else if( psz_subtitle[0] == '{' &&
                ( psz_subtitle[1] == 'Y' || psz_subtitle[1] == 'y' )
                && psz_subtitle[2] == ':' && strchr( psz_subtitle, '}' ) )
        {
            // FIXME: We don't do difference between Y and y, and we should.
            if( psz_subtitle[3] == 'i' )
            {
                HtmlPut( &psz_html, "<i>" );
                HtmlPut( &psz_tag, "i" );
            }
            if( psz_subtitle[3] == 'b' )
            {
                HtmlPut( &psz_html, "<b>" );
                HtmlPut( &psz_tag, "b" );
            }
            if( psz_subtitle[3] == 'u' )
            {
                HtmlPut( &psz_html, "<u>" );
                HtmlPut( &psz_tag, "u" );
            }
            psz_subtitle = strchr( psz_subtitle, '}' ) + 1;
        }
        else if( psz_subtitle[0] == '{' &&  psz_subtitle[2] == ':' && strchr( psz_subtitle, '}' ) )
        {
            // Hide other {x:y} atrocities, like {c:$bbggrr} or {P:x}
            psz_subtitle = strchr( psz_subtitle, '}' ) + 1;
        }
        else if( psz_subtitle[0] == '\\' && psz_subtitle[1] )
        {
            if( psz_subtitle[1] == 'N' || psz_subtitle[1] == 'n' )
            {
                HtmlPut( &psz_html, "<br/>" );
                psz_subtitle += 2;
            }
            else if( psz_subtitle[1] == 'h' )
            {
                /* Non breakable space */
                HtmlPut( &psz_html, NO_BREAKING_SPACE );
                psz_subtitle += 2;
            }
            else
            {
                HtmlPut( &psz_html, "\\" );
                psz_subtitle++;
            }
        }
        else
        {
            HtmlNPut( &psz_html, psz_subtitle, 1 );
#if 0
            if( *psz_html )
            {
                /* Check for double whitespace */
# error This test does not make sense.
                if( ( *psz_html == ' '  || *psz_html == '\t' ) &&
                    ( *(psz_html-1) == ' ' || *(psz_html-1) == '\t' ) )
                {
                    HtmlPut( &psz_html, NO_BREAKING_SPACE );
                    psz_html--;
                }
            }
#endif
            psz_subtitle++;
        }
    }

    while( psz_tag && *psz_tag )
    {
        /* */
        char *psz_last = &psz_tag[strlen(psz_tag)-1];
        switch( *psz_last )
        {
            case 'b':
                HtmlPut( &psz_html, "</b>" );
                break;
            case 'i':
                HtmlPut( &psz_html, "</i>" );
                break;
            case 'u':
                HtmlPut( &psz_html, "</u>" );
                break;
            case 's':
                HtmlPut( &psz_html, "</s>" );
                break;
            case 'f':
                HtmlPut( &psz_html, "</font>" );
                break;
            case 'I':
                break;
        }
        *psz_last = '\0';
    }
    /* Close not well formed subtitle */
    HtmlPut( &psz_html, "</text>" );

    free( psz_tag );

    return psz_html;
}
