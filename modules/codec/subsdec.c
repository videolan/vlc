/*****************************************************************************
 * subsdec.c : text subtitles decoder
 *****************************************************************************
 * Copyright (C) 2000-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#include "osd.h"
#include "vlc_filter.h"

#include "charset.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    int                 i_align;          /* Subtitles alignment on the vout */
    vlc_iconv_t         iconv_handle;            /* handle to iconv instance */
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static subpicture_t *DecodeBlock   ( decoder_t *, block_t ** );
static subpicture_t *ParseText     ( decoder_t *, block_t * );
static void         StripTags      ( char * );

#define DEFAULT_NAME "System Default"

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
    vlc_value_t val;

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
        return VLC_EGENERIC;
    }

    var_Create( p_dec, "subsdec-align", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "subsdec-align", &val );
    p_sys->i_align = val.i_int;

    if( p_dec->fmt_in.subs.psz_encoding && *p_dec->fmt_in.subs.psz_encoding )
    {
        msg_Dbg( p_dec, "using character encoding: %s",
                 p_dec->fmt_in.subs.psz_encoding );
        p_sys->iconv_handle =
            vlc_iconv_open( "UTF-8", p_dec->fmt_in.subs.psz_encoding );
    }
    else
    {
        var_Create( p_dec, "subsdec-encoding",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_dec, "subsdec-encoding", &val );
        if( !strcmp( val.psz_string, DEFAULT_NAME ) )
        {
            char *psz_charset =(char*)malloc( 100 );
            vlc_current_charset( &psz_charset );
            p_sys->iconv_handle = vlc_iconv_open( "UTF-8", psz_charset );
            msg_Dbg( p_dec, "using character encoding: %s", psz_charset );
            free( psz_charset );
        }
        else if( val.psz_string )
        {
            msg_Dbg( p_dec, "using character encoding: %s", val.psz_string );
            p_sys->iconv_handle = vlc_iconv_open( "UTF-8", val.psz_string );
        }

        if( p_sys->iconv_handle == (vlc_iconv_t)-1 )
        {
            msg_Warn( p_dec, "unable to do requested conversion" );
        }

        if( val.psz_string ) free( val.psz_string );
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
    subpicture_t *p_spu;

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

    free( p_sys );
}

/*****************************************************************************
 * ParseText: parse an text subtitle packet and send it to the video output
 *****************************************************************************/
static subpicture_t *ParseText( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t *p_spu = 0;
    char *psz_subtitle;
    int i_align_h, i_align_v;
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
    psz_subtitle = strndup( p_block->p_buffer, p_block->i_buffer );

    i_align_h = p_sys->i_align ? 20 : 0;
    i_align_v = 10;

    if( p_sys->iconv_handle != (vlc_iconv_t)-1 )
    {
        char *psz_new_subtitle;
        char *psz_convert_buffer_out;
        char *psz_convert_buffer_in;
        size_t ret, inbytes_left, outbytes_left;

        psz_new_subtitle = malloc( 6 * strlen( psz_subtitle ) );
        psz_convert_buffer_out = psz_new_subtitle;
        psz_convert_buffer_in = psz_subtitle;
        inbytes_left = strlen( psz_subtitle );
        outbytes_left = 6 * inbytes_left;
        ret = vlc_iconv( p_sys->iconv_handle, &psz_convert_buffer_in,
                         &inbytes_left, &psz_convert_buffer_out,
                         &outbytes_left );
        *psz_convert_buffer_out = '\0';

        if( inbytes_left )
        {
            msg_Warn( p_dec, "Failed to convert subtitle encoding, "
                      "dropping subtitle.\nTry setting a different "
                      "character-encoding for the subtitle." );
            free( psz_subtitle );
            return NULL;
        }
        else
        {
            free( psz_subtitle );
            psz_subtitle = psz_new_subtitle;
        }
    }

    if( p_dec->fmt_in.i_codec == VLC_FOURCC('s','s','a',' ') )
    {
        /* Decode SSA strings */
        /* We expect: ReadOrder, Layer, Style, Name, MarginL, MarginR,
         * MarginV, Effect, Text */
        char *psz_new_subtitle;
        char *psz_buffer_sub;
        int         i_comma;
        int         i_text;

        psz_buffer_sub = psz_subtitle;
        for( ;; )
        {
            i_comma = 0;
            while( i_comma < 8 &&
                *psz_buffer_sub != '\0' )
            {
                if( *psz_buffer_sub == ',' )
                {
                    i_comma++;
                }
                psz_buffer_sub++;
            }
            psz_new_subtitle = malloc( strlen( psz_buffer_sub ) + 1);
            i_text = 0;
            while( psz_buffer_sub[0] != '\0' )
            {
                if( psz_buffer_sub[0] == '\\' && ( psz_buffer_sub[1] == 'n' ||
                    psz_buffer_sub[1] == 'N' ) )
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
            free( psz_subtitle );
            psz_subtitle = psz_new_subtitle;
            break;
        }
    }

    StripTags( psz_subtitle );

    p_spu = p_dec->pf_spu_buffer_new( p_dec );
    if( !p_spu )
    {
        msg_Warn( p_dec, "can't get spu buffer" );
        free( psz_subtitle );
        return 0;
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
        free( psz_subtitle );
        p_dec->pf_spu_buffer_del( p_dec, p_spu );
        return 0;
    }

    p_spu->p_region->psz_text = psz_subtitle;
    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer = (p_block->i_length == 0);
    p_spu->b_absolute = VLC_FALSE;

    p_spu->i_flags = OSD_ALIGN_BOTTOM | p_sys->i_align;
    p_spu->i_x = i_align_h;
    p_spu->i_y = i_align_v;

    return p_spu;
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
