/*****************************************************************************
 * subsdec.c : text subtitle decoder
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
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
#include <errno.h>
#include <ctype.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_charset.h>
#include <vlc_xml.h>

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

static const int  pi_justification[] = { -1, 0, 1, 2 };
static const char *const ppsz_justification_text[] = {
    N_("Auto"),N_("Center"),N_("Left"),N_("Right")
};

#define ENCODING_TEXT N_("Subtitle text encoding")
#define ENCODING_LONGTEXT N_("Set the encoding used in text subtitles")
#define ALIGN_TEXT N_("Subtitle justification")
#define ALIGN_LONGTEXT N_("Set the justification of subtitles")
#define AUTODETECT_UTF8_TEXT N_("UTF-8 subtitle autodetection")
#define AUTODETECT_UTF8_LONGTEXT N_("This enables automatic detection of " \
            "UTF-8 encoding within subtitle files.")

static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Subtitles"))
    set_description( N_("Text subtitle decoder") )
    set_capability( "spu decoder", 50 )
    set_callbacks( OpenDecoder, CloseDecoder )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )

    add_integer( "subsdec-align", -1, ALIGN_TEXT, ALIGN_LONGTEXT,
                 false )
        change_integer_list( pi_justification, ppsz_justification_text )
    add_string( "subsdec-encoding", "",
                ENCODING_TEXT, ENCODING_LONGTEXT, false )
        change_string_list( ppsz_encodings, ppsz_encoding_names )
    add_bool( "subsdec-autodetect-utf8", true,
              AUTODETECT_UTF8_TEXT, AUTODETECT_UTF8_LONGTEXT, false )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define NO_BREAKING_SPACE  "&#160;"

typedef struct
{
    int                 i_align;          /* Subtitles alignment on the vout */

    vlc_iconv_t         iconv_handle;            /* handle to iconv instance */
    bool                b_autodetect_utf8;
} decoder_sys_t;


static int             DecodeBlock   ( decoder_t *, block_t * );
static subpicture_t   *ParseText     ( decoder_t *, block_t * );
static text_segment_t *ParseSubtitles(int *pi_align, const char * );

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

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_dec->pf_decode = DecodeBlock;
    p_dec->fmt_out.i_codec = 0;

    /* init of p_sys */
    p_sys->i_align = -1;
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
            msg_Err (p_dec, "cannot convert from %s: %s", encoding,
                     vlc_strerror_c(errno));
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
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    subpicture_t *p_spu;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
    {
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    p_spu = ParseText( p_dec, p_block );

    block_Release( p_block );
    if( p_spu != NULL )
        decoder_QueueSub( p_dec, p_spu );
    return VLCDEC_SUCCESS;
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

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        return NULL;

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts == VLC_TICK_INVALID )
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

    char *psz_subtitle = NULL;

    /* Should be resiliant against bad subtitles */
    if( p_sys->iconv_handle == (vlc_iconv_t)-1 ||
        p_sys->b_autodetect_utf8 )
    {
        psz_subtitle = malloc( p_block->i_buffer + 1 );
        if( psz_subtitle == NULL )
            return NULL;
        memcpy( psz_subtitle, p_block->p_buffer, p_block->i_buffer );
        psz_subtitle[p_block->i_buffer] = '\0';
    }

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
            size_t inbytes_left = p_block->i_buffer;
            size_t outbytes_left = 6 * inbytes_left;
            char *psz_new_subtitle = xmalloc( outbytes_left + 1 );
            char *psz_convert_buffer_out = psz_new_subtitle;
            const char *psz_convert_buffer_in =
                    psz_subtitle ? psz_subtitle : (char *)p_block->p_buffer;

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
    p_spu->b_ephemer  = (p_block->i_length == VLC_TICK_INVALID);
    p_spu->b_absolute = false;

    subtext_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;

    int i_inline_align = -1;
    p_spu_sys->region.p_segments = ParseSubtitles( &i_inline_align, psz_subtitle );
    free( psz_subtitle );
    if( p_sys->i_align >= 0 ) /* bottom ; left, right or centered */
    {
        p_spu_sys->region.align = SUBPICTURE_ALIGN_BOTTOM | p_sys->i_align;
        p_spu_sys->region.inner_align = p_sys->i_align;
    }
    else if( i_inline_align >= 0 )
    {
        p_spu_sys->region.align = i_inline_align;
        p_spu_sys->region.inner_align = i_inline_align;
    }
    else /* default, bottom ; centered */
    {
        p_spu_sys->region.align = SUBPICTURE_ALIGN_BOTTOM;
        p_spu_sys->region.inner_align = 0;
    }

    return p_spu;
}

static bool AppendCharacter( text_segment_t* p_segment, char c )
{
    char* tmp;
    if ( asprintf( &tmp, "%s%c", p_segment->psz_text ? p_segment->psz_text : "", c ) < 0 )
        return false;
    free( p_segment->psz_text );
    p_segment->psz_text = tmp;
    return true;
}

static bool AppendString( text_segment_t* p_segment, const char* psz_str )
{
    char* tmp;
    if ( asprintf( &tmp, "%s%s", p_segment->psz_text ? p_segment->psz_text : "", psz_str ) < 0 )
        return false;
    free( p_segment->psz_text );
    p_segment->psz_text = tmp;
    return true;
}

static char* ConsumeAttribute( const char** ppsz_subtitle, char** ppsz_attribute_value )
{
    const char* psz_subtitle = *ppsz_subtitle;
    char* psz_attribute_name;
    *ppsz_attribute_value = NULL;

    while (*psz_subtitle == ' ')
        psz_subtitle++;

    size_t attr_len = 0;
    char delimiter;

    while ( *psz_subtitle && isalpha( *psz_subtitle ) )
    {
        psz_subtitle++;
        attr_len++;
    }
    if ( !*psz_subtitle || attr_len == 0 )
        return NULL;
    psz_attribute_name = malloc( attr_len + 1 );
    if ( unlikely( !psz_attribute_name ) )
        return NULL;
    strncpy( psz_attribute_name, psz_subtitle - attr_len, attr_len );
    psz_attribute_name[attr_len] = 0;

    // Skip over to the attribute value
    while ( *psz_subtitle && *psz_subtitle != '=' )
        psz_subtitle++;
    if ( !*psz_subtitle )
    {
        *ppsz_subtitle = psz_subtitle;
        return psz_attribute_name;
    }
    // Skip the '=' sign
    psz_subtitle++;

    // Aknoledge the delimiter if any
    while ( *psz_subtitle && isspace( *psz_subtitle) )
        psz_subtitle++;

    if ( *psz_subtitle == '\'' || *psz_subtitle == '"' )
    {
        // Save the delimiter and skip it
        delimiter = *psz_subtitle;
        psz_subtitle++;
    }
    else
        delimiter = 0;

    // Skip spaces, just in case
    while ( *psz_subtitle && isspace( *psz_subtitle ) )
        psz_subtitle++;

    attr_len = 0;
    while ( *psz_subtitle && ( ( delimiter != 0 && *psz_subtitle != delimiter ) ||
                               ( delimiter == 0 && ( !isspace(*psz_subtitle) && *psz_subtitle != '>' ) ) ) )
    {
        psz_subtitle++;
        attr_len++;
    }
    if ( attr_len == 0 )
    {
        *ppsz_subtitle = psz_subtitle;
        return psz_attribute_name;
    }
    if ( unlikely( !( *ppsz_attribute_value = malloc( attr_len + 1 ) ) ) )
    {
        free( psz_attribute_name );
        return NULL;
    }
    strncpy( *ppsz_attribute_value, psz_subtitle - attr_len, attr_len );
    (*ppsz_attribute_value)[attr_len] = 0;
    // Finally, skip over the final delimiter
    if (delimiter != 0 && *psz_subtitle)
        psz_subtitle++;
    *ppsz_subtitle = psz_subtitle;
    return psz_attribute_name;
}

// Returns the next tag and consume the string up to after the tag name, or
// returns NULL and doesn't advance if the angle bracket was not a tag opening
// For instance, if psz_subtitle == "<some_tag attribute=value>"
// GetTag will return "some_tag", and will advance up to the first 'a' in "attribute"
// The returned value must be freed.
static char* GetTag( const char** ppsz_subtitle, bool b_closing )
{
    const char* psz_subtitle = *ppsz_subtitle;
    if ( *psz_subtitle != '<' )
        return NULL;
    // Skip the '<'
    psz_subtitle++;
    if ( b_closing && *psz_subtitle == '/' )
        psz_subtitle++;
    // Skip potential spaces
    while ( *psz_subtitle == ' ' )
        psz_subtitle++;
    // Now we need to verify if what comes next is a valid tag:
    if ( !isalpha( *psz_subtitle ) )
        return NULL;
    size_t tag_size = 1;
    while ( isalnum( psz_subtitle[tag_size] ) || psz_subtitle[tag_size] == '_' )
        tag_size++;
    char* psz_tagname = vlc_alloc( tag_size + 1, sizeof( *psz_tagname ) );
    if ( unlikely( !psz_tagname ) )
        return NULL;
    strncpy( psz_tagname, psz_subtitle, tag_size );
    psz_tagname[tag_size] = 0;
    psz_subtitle += tag_size;
    *ppsz_subtitle = psz_subtitle;
    return psz_tagname;
}

static bool IsClosed( const char* psz_subtitle, const char* psz_tagname )
{
    const char* psz_tagpos = strcasestr( psz_subtitle, psz_tagname );
    if ( !psz_tagpos )
        return false;
    // Search for '</' and '>' immediatly before & after (minding the potential spaces)
    const char* psz_endtag = psz_tagpos + strlen( psz_tagname );
    while ( *psz_endtag == ' ' )
        psz_endtag++;
    if ( *psz_endtag != '>' )
        return false;
    // Skip back before the tag itself
    psz_tagpos--;
    while ( *psz_tagpos == ' ' && psz_tagpos > psz_subtitle )
        psz_tagpos--;
    if ( *psz_tagpos-- != '/' )
        return false;
    if ( *psz_tagpos != '<' )
        return false;
    return true;
}

typedef struct tag_stack tag_stack_t;
struct tag_stack
{
    char* psz_tagname;
    tag_stack_t *p_next;
};

static void AppendTag( tag_stack_t **pp_stack, char* psz_tagname )
{
    tag_stack_t* p_elem = malloc( sizeof( *p_elem ) );
    if ( unlikely( !p_elem ) )
        return;
    p_elem->p_next = *pp_stack;
    p_elem->psz_tagname = psz_tagname;
    *pp_stack = p_elem;
}

static bool HasTag( tag_stack_t **pp_stack, const char* psz_tagname )
{
    tag_stack_t *p_prev = NULL;
    for ( tag_stack_t* p_current = *pp_stack; p_current; p_current = p_current->p_next )
    {
        if ( !strcasecmp( psz_tagname, p_current->psz_tagname ) )
        {
            if ( p_current == *pp_stack )
            {
                *pp_stack = p_current->p_next;
            }
            else
            {
                p_prev->p_next = p_current->p_next;
            }
            free( p_current->psz_tagname );
            free( p_current );
            return true;
        }
        p_prev = p_current;
    }
    return false;
}

/*
 * mini style stack implementation
 */
typedef struct style_stack style_stack_t;
struct  style_stack
{
    text_style_t* p_style;
    style_stack_t* p_next;
};

static text_style_t* DuplicateAndPushStyle(style_stack_t** pp_stack)
{
    text_style_t* p_dup = ( *pp_stack ) ? text_style_Duplicate( (*pp_stack)->p_style ) : text_style_Create( STYLE_NO_DEFAULTS );
    if ( unlikely( !p_dup ) )
        return NULL;
    style_stack_t* p_entry = malloc( sizeof( *p_entry ) );
    if ( unlikely( !p_entry ) )
    {
        text_style_Delete( p_dup );
        return NULL;
    }
    // Give the style ownership to the segment.
    p_entry->p_style = p_dup;
    p_entry->p_next = *pp_stack;
    *pp_stack = p_entry;
    return p_dup;
}

static void PopStyle(style_stack_t** pp_stack)
{
    style_stack_t* p_old = *pp_stack;
    if ( !p_old )
        return;
    *pp_stack = p_old->p_next;
    // Don't free the style, it is now owned by the text_segment_t
    free( p_old );
}

static text_segment_t* NewTextSegmentPushStyle( text_segment_t* p_segment, style_stack_t** pp_stack )
{
    text_segment_t* p_new = text_segment_New( NULL );
    if ( unlikely( p_new == NULL ) )
        return NULL;
    text_style_t* p_style = DuplicateAndPushStyle( pp_stack );
    p_new->style = p_style;
    p_segment->p_next = p_new;
    return p_new;
}

static text_segment_t* NewTextSegmentPopStyle( text_segment_t* p_segment, style_stack_t** pp_stack )
{
    text_segment_t* p_new = text_segment_New( NULL );
    if ( unlikely( p_new == NULL ) )
        return NULL;
    // We shouldn't have an empty stack since this happens when closing a tag,
    // but better be safe than sorry if (/when) we encounter a broken subtitle file.
    PopStyle( pp_stack );
    text_style_t* p_dup = ( *pp_stack ) ? text_style_Duplicate( (*pp_stack)->p_style ) : text_style_Create( STYLE_NO_DEFAULTS );
    p_new->style = p_dup;
    p_segment->p_next = p_new;
    return p_new;
}

static text_segment_t* ParseSubtitles( int *pi_align, const char *psz_subtitle )
{
    text_segment_t* p_segment;
    text_segment_t* p_first_segment;
    style_stack_t* p_stack = NULL;
    tag_stack_t* p_tag_stack = NULL;

    //FIXME: Remove initial allocation? Might make the below code more complicated
    p_first_segment = p_segment = text_segment_New( "" );

    *pi_align = -1;

    /* */
    while( *psz_subtitle )
    {
        /* HTML extensions */
        if( *psz_subtitle == '<' )
        {
            char *psz_tagname = GetTag( &psz_subtitle, false );
            if ( psz_tagname != NULL )
            {
                if( !strcasecmp( psz_tagname, "br" ) )
                {
                    if ( !AppendCharacter( p_segment, '\n' ) )
                    {
                        free( psz_tagname );
                        goto fail;
                    }
                }
                else if( !strcasecmp( psz_tagname, "b" ) )
                {
                    p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                    p_segment->style->i_style_flags |= STYLE_BOLD;
                    p_segment->style->i_features |= STYLE_HAS_FLAGS;
                }
                else if( !strcasecmp( psz_tagname, "i" ) )
                {
                    p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                    p_segment->style->i_style_flags |= STYLE_ITALIC;
                    p_segment->style->i_features |= STYLE_HAS_FLAGS;
                }
                else if( !strcasecmp( psz_tagname, "u" ) )
                {
                    p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                    p_segment->style->i_style_flags |= STYLE_UNDERLINE;
                    p_segment->style->i_features |= STYLE_HAS_FLAGS;
                }
                else if( !strcasecmp( psz_tagname, "s" ) )
                {
                    p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                    p_segment->style->i_style_flags |= STYLE_STRIKEOUT;
                    p_segment->style->i_features |= STYLE_HAS_FLAGS;
                }
                else if( !strcasecmp( psz_tagname, "font" ) )
                {
                    p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );

                    char* psz_attribute_name;
                    char* psz_attribute_value;

                    while( ( psz_attribute_name = ConsumeAttribute( &psz_subtitle, &psz_attribute_value ) ) )
                    {
                        if ( !psz_attribute_value )
                        {
                            free( psz_attribute_name );
                            continue;
                        }
                        if ( !strcasecmp( psz_attribute_name, "face" ) )
                        {
                            free(p_segment->style->psz_fontname);
                            p_segment->style->psz_fontname = psz_attribute_value;
                            // We don't want to free the attribute value since it has become our fontname
                            psz_attribute_value = NULL;
                        }
                        else if ( !strcasecmp( psz_attribute_name, "family" ) )
                        {
                            free(p_segment->style->psz_monofontname);
                            p_segment->style->psz_monofontname = psz_attribute_value;
                            psz_attribute_value = NULL;
                        }
                        else if ( !strcasecmp( psz_attribute_name, "size" ) )
                        {
                            int size = atoi( psz_attribute_value );
                            if( size )
                            {
                                p_segment->style->i_font_size = size;
                                p_segment->style->f_font_relsize = STYLE_DEFAULT_REL_FONT_SIZE *
                                        STYLE_DEFAULT_FONT_SIZE / p_segment->style->i_font_size;
                            }
                        }
                        else if ( !strcasecmp( psz_attribute_name, "color" ) )
                        {
                            p_segment->style->i_font_color = vlc_html_color( psz_attribute_value, NULL );
                            p_segment->style->i_features |= STYLE_HAS_FONT_COLOR;
                        }
                        else if ( !strcasecmp( psz_attribute_name, "outline-color" ) )
                        {
                            p_segment->style->i_outline_color = vlc_html_color( psz_attribute_value, NULL );
                            p_segment->style->i_features |= STYLE_HAS_OUTLINE_COLOR;
                        }
                        else if ( !strcasecmp( psz_attribute_name, "shadow-color" ) )
                        {
                            p_segment->style->i_shadow_color = vlc_html_color( psz_attribute_value, NULL );
                            p_segment->style->i_features |= STYLE_HAS_SHADOW_COLOR;
                        }
                        else if ( !strcasecmp( psz_attribute_name, "outline-level" ) )
                        {
                            p_segment->style->i_outline_width = atoi( psz_attribute_value );
                        }
                        else if ( !strcasecmp( psz_attribute_name, "shadow-level" ) )
                        {
                            p_segment->style->i_shadow_width = atoi( psz_attribute_value );
                        }
                        else if ( !strcasecmp( psz_attribute_name, "back-color" ) )
                        {
                            p_segment->style->i_background_color = vlc_html_color( psz_attribute_value, NULL );
                            p_segment->style->i_features |= STYLE_HAS_BACKGROUND_COLOR;
                        }
                        else if ( !strcasecmp( psz_attribute_name, "alpha" ) )
                        {
                            p_segment->style->i_font_alpha = atoi( psz_attribute_value );
                            p_segment->style->i_features |= STYLE_HAS_FONT_ALPHA;
                        }

                        free( psz_attribute_name );
                        free( psz_attribute_value );
                    }
                }
                else
                {
                    // This is an unknown tag. We need to hide it if it's properly closed, and display it otherwise
                    if ( !IsClosed( psz_subtitle, psz_tagname ) )
                    {
                        AppendCharacter( p_segment, '<' );
                        AppendString( p_segment, psz_tagname );
                        AppendCharacter( p_segment, '>' );
                    }
                    else
                    {
                        AppendTag( &p_tag_stack, psz_tagname );
                        // We don't want to free the tagname now, it will be freed when the tag
                        // gets poped from the stack.
                        psz_tagname = NULL;
                    }
                    // In any case, fall through and skip to the closing tag.
                }
                // Skip potential spaces & end tag
                while ( *psz_subtitle && *psz_subtitle != '>' )
                    psz_subtitle++;
                if ( *psz_subtitle == '>' )
                    psz_subtitle++;

                free( psz_tagname );
            }
            else if( !strncmp( psz_subtitle, "</", 2 ))
            {
                char* psz_closetagname = GetTag( &psz_subtitle, true );
                if ( psz_closetagname != NULL )
                {
                    if ( !strcasecmp( psz_closetagname, "b" ) ||
                         !strcasecmp( psz_closetagname, "i" ) ||
                         !strcasecmp( psz_closetagname, "u" ) ||
                         !strcasecmp( psz_closetagname, "s" ) ||
                         !strcasecmp( psz_closetagname, "font" ) )
                    {
                        // A closing tag for one of the tags we handle, meaning
                        // we pushed a style onto the stack earlier
                        p_segment = NewTextSegmentPopStyle( p_segment, &p_stack );
                    }
                    else
                    {
                        // Unknown closing tag. If it is closing an unknown tag, ignore it. Otherwise, display it
                        if ( !HasTag( &p_tag_stack, psz_closetagname ) )
                        {
                            AppendString( p_segment, "</" );
                            AppendString( p_segment, psz_closetagname );
                            AppendCharacter( p_segment, '>' );
                        }
                    }
                    while ( *psz_subtitle == ' ' )
                        psz_subtitle++;
                    if ( *psz_subtitle == '>' )
                        psz_subtitle++;
                    free( psz_closetagname );
                }
                else
                {
                    /**
                      * This doesn't appear to be a valid tag closing syntax.
                      * Simply append the text
                      */
                    AppendString( p_segment, "</" );
                    psz_subtitle += 2;
                }
            }
            else
            {
                /* We have an unknown tag, just append it, and move on.
                 * The rest of the string won't be recognized as a tag, and
                 * we will ignore unknown closing tag
                 */
                AppendCharacter( p_segment, '<' );
                psz_subtitle++;
            }
        }
        /* SSA extensions */
        else if( psz_subtitle[0] == '{' && psz_subtitle[1] == '\\' &&
                 strchr( psz_subtitle, '}' ) )
        {
            /* Check for forced alignment */
            if( *pi_align < 0 &&
                !strncmp( psz_subtitle, "{\\an", 4 ) && psz_subtitle[4] >= '1' && psz_subtitle[4] <= '9' && psz_subtitle[5] == '}' )
            {
                static const int pi_vertical[3] = { SUBPICTURE_ALIGN_BOTTOM, 0, SUBPICTURE_ALIGN_TOP };
                static const int pi_horizontal[3] = { SUBPICTURE_ALIGN_LEFT, 0, SUBPICTURE_ALIGN_RIGHT };
                const int i_id = psz_subtitle[4] - '1';

                *pi_align = pi_vertical[i_id/3] | pi_horizontal[i_id%3];
            }
            /* TODO fr -> rotation */

            /* Hide {\stupidity} */
            psz_subtitle = strchr( psz_subtitle, '}' ) + 1;
        }
        /* MicroDVD extensions */
        /* FIXME:
         *  - Currently, we don't do difference between X and x, and we should:
         *    Capital Letters applies to the whole text and not one line
         *  - We don't support Position and Coordinates
         *  - We don't support the DEFAULT flag (HEADER)
         */

        else if( psz_subtitle[0] == '{' && psz_subtitle[1] != 0 &&
                 psz_subtitle[2] == ':' && strchr( &psz_subtitle[2], '}' ) )
        {
            const char *psz_tag_end = strchr( &psz_subtitle[2], '}' );
            size_t i_len = psz_tag_end - &psz_subtitle[3];

            if( psz_subtitle[1] == 'Y' || psz_subtitle[1] == 'y' )
            {
                if( psz_subtitle[3] == 'i' )
                {
                    p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                    p_segment->style->i_style_flags |= STYLE_ITALIC;
                    p_segment->style->i_features |= STYLE_HAS_FLAGS;
                    psz_subtitle++;
                }
                if( psz_subtitle[3] == 'b' )
                {
                    p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                    p_segment->style->i_style_flags |= STYLE_BOLD;
                    p_segment->style->i_features |= STYLE_HAS_FLAGS;
                    psz_subtitle++;
                }
                if( psz_subtitle[3] == 'u' )
                {
                    p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                    p_segment->style->i_style_flags |= STYLE_UNDERLINE;
                    p_segment->style->i_features |= STYLE_HAS_FLAGS;
                    psz_subtitle++;
                }
            }
            else if( (psz_subtitle[1] == 'C' || psz_subtitle[1] == 'c' )
                    && psz_subtitle[3] == '$' && i_len >= 7 )
            {
                /* Yes, they use BBGGRR, instead of RRGGBB */
                char psz_color[7];
                psz_color[0] = psz_subtitle[8]; psz_color[1] = psz_subtitle[9];
                psz_color[2] = psz_subtitle[6]; psz_color[3] = psz_subtitle[7];
                psz_color[4] = psz_subtitle[4]; psz_color[5] = psz_subtitle[5];
                psz_color[6] = '\0';
                p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                p_segment->style->i_font_color = vlc_html_color( psz_color, NULL );
                p_segment->style->i_features |= STYLE_HAS_FONT_COLOR;
            }
            else if( psz_subtitle[1] == 'F' || psz_subtitle[1] == 'f' )
            {
                p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                free(p_segment->style->psz_fontname);
                p_segment->style->psz_fontname = strndup( &psz_subtitle[3], i_len );
            }
            else if( psz_subtitle[1] == 'S' || psz_subtitle[1] == 's' )
            {
                int size = atoi( &psz_subtitle[3] );
                if( size )
                {
                    p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                    p_segment->style->i_font_size = size;
                    p_segment->style->f_font_relsize = STYLE_DEFAULT_REL_FONT_SIZE *
                                STYLE_DEFAULT_FONT_SIZE / p_segment->style->i_font_size;

                }
            }
            /* Currently unsupported since we don't have access to the i_align flag here
            else if( psz_subtitle[1] == 'P' )
            {
                if( psz_subtitle[3] == "1" )
                    i_align = SUBPICTURE_ALIGN_TOP;
                else if( psz_subtitle[3] == "0" )
                    i_align = SUBPICTURE_ALIGN_BOTTOM;
            } */
            // Hide other {x:y} atrocities, notably {o:x}
            psz_subtitle = psz_tag_end + 1;
        }
        else
        {
            if( *psz_subtitle == '\n' || !strncasecmp( psz_subtitle, "\\n", 2 ) )
            {
                if ( !AppendCharacter( p_segment, '\n' ) )
                    goto fail;
                if ( *psz_subtitle == '\n' )
                    psz_subtitle++;
                else
                    psz_subtitle += 2;
            }
            else if( !strncasecmp( psz_subtitle, "\\h", 2 ) )
            {
                if ( !AppendString( p_segment, "\xC2\xA0" ) )
                    goto fail;
                psz_subtitle += 2;
            }
            else
            {
                //FIXME: Highly inneficient
                AppendCharacter( p_segment, *psz_subtitle );
                psz_subtitle++;
            }
        }
    }
    while ( p_stack )
        PopStyle( &p_stack );
    while ( p_tag_stack )
    {
        tag_stack_t *p_tag = p_tag_stack;
        p_tag_stack = p_tag_stack->p_next;
        free( p_tag->psz_tagname );
        free( p_tag );
    }

    return p_first_segment;

fail:
    text_segment_ChainDelete( p_first_segment );
    return NULL;
}
