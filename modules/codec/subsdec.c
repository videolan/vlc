/*****************************************************************************
 * subsdec.c : text subtitles decoder
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: subsdec.c,v 1.6 2003/11/15 15:40:19 hartman Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>
#include <osd.h>
#include <codecs.h>

#if defined(HAVE_ICONV)
#include <iconv.h>
#endif

#include "charset.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    int                 i_align;          /* Subtitles alignment on the vout */

#if defined(HAVE_ICONV)
    iconv_t             iconv_handle;            /* handle to iconv instance */
#endif
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );

static int  InitDecoder   ( decoder_t * );
static int  RunDecoder    ( decoder_t *, block_t * );
static int  EndDecoder    ( decoder_t * );

static void ParseText     ( decoder_t *, block_t *, vout_thread_t * );

#define DEFAULT_NAME "System Default"

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
#if defined(HAVE_ICONV)
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
#endif

static int  pi_justification[] = { 0, 1, 2 };
static char *ppsz_justification_text[] = {N_("Center"),N_("Left"),N_("Right")};

#define ENCODING_TEXT N_("Subtitles text encoding")
#define ENCODING_LONGTEXT N_("Change the encoding used in text subtitles")
#define ALIGN_TEXT N_("Subtitles justification")
#define ALIGN_LONGTEXT N_("Change the justification of substitles")

vlc_module_begin();
    set_description( _("text subtitles decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, NULL );

    add_category_hint( N_("Subtitles"), NULL, VLC_FALSE );
    add_integer( "subsdec-align", 0, NULL, ALIGN_TEXT, ALIGN_LONGTEXT,
                 VLC_TRUE );
        change_integer_list( pi_justification, ppsz_justification_text, 0 );
#if defined(HAVE_ICONV)
    add_string( "subsdec-encoding", "UTF-8", NULL,
                ENCODING_TEXT, ENCODING_LONGTEXT, VLC_FALSE );
        change_string_list( ppsz_encodings, 0, 0 );
#endif
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC('s','u','b','t') && 
        p_dec->p_fifo->i_fourcc != VLC_FOURCC('s','s','a',' ') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_init = InitDecoder;
    p_dec->pf_decode = RunDecoder;
    p_dec->pf_end = EndDecoder;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * InitDecoder: Initalize the decoder
 *****************************************************************************/
static int InitDecoder( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subtitle_data_t *p_demux_data = (subtitle_data_t *)p_dec->p_fifo->p_demux_data;
    vlc_value_t val;

    var_Create( p_dec, "subsdec-align", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "subsdec-align", &val );
    p_sys->i_align = val.i_int;

#if defined(HAVE_ICONV)
    var_Create( p_dec, "subsdec-encoding",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "subsdec-encoding", &val );
    if( !strcmp( val.psz_string, DEFAULT_NAME ) )
    {
        char *psz_charset =(char*)malloc( 100 );
        vlc_current_charset( &psz_charset );
        p_sys->iconv_handle = iconv_open( "UTF-8", psz_charset );
        free( psz_charset );
    }
    else
    {
        p_sys->iconv_handle = iconv_open( "UTF-8", val.psz_string );
    }

    if( p_sys->iconv_handle == (iconv_t)-1 )
    {
        msg_Warn( p_dec, "Unable to do requested conversion" );
    }

    if( val.psz_string ) free( val.psz_string );
#else
    msg_Dbg( p_dec, "No iconv support available" );
#endif

#if 0
    if( p_demux_data )
        msg_Dbg( p_dec, p_demux_data->psz_header );
#endif

    return VLC_SUCCESS;
}

/****************************************************************************
 * RunDecoder: the whole thing
 ****************************************************************************
 * This function must be fed with complete subtitles units.
 ****************************************************************************/
static int RunDecoder( decoder_t *p_dec, block_t *p_block )
{
    vout_thread_t *p_vout;

    /* Here we are dealing with text subtitles */
    p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( !p_vout )
    {
        msg_Warn( p_dec, "couldn't find a video output, trashing subtitle" );
        return VLC_SUCCESS;
    }

    ParseText( p_dec, p_block, p_vout );
    vlc_object_release( p_vout );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EndDecoder: clean up the decoder
 *****************************************************************************/
static int EndDecoder( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    vout_thread_t *p_vout;

    p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout != NULL && p_vout->p_subpicture != NULL )
    {
        subpicture_t *p_subpic;
        int          i_subpic;

        for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
        {
            p_subpic = &p_vout->p_subpicture[i_subpic];

            if( p_subpic != NULL &&
              ( p_subpic->i_status == RESERVED_SUBPICTURE
                || p_subpic->i_status == READY_SUBPICTURE ) )
            {
                vout_DestroySubPicture( p_vout, p_subpic );
            }
        }
    }
    if( p_vout ) vlc_object_release( p_vout );

#if defined(HAVE_ICONV)
    if( p_sys->iconv_handle != (iconv_t)-1 )
    {
        iconv_close( p_sys->iconv_handle );
    }
#endif

    free( p_sys );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParseText: parse an text subtitle packet and send it to the video output
 *****************************************************************************/
static void ParseText( decoder_t *p_dec, block_t *p_block,
                       vout_thread_t *p_vout )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    char *psz_subtitle;
    int i_align_h, i_align_v;

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts == 0 )
    {
        msg_Warn( p_dec, "subtitle without a date" );
        return;
    }

    /* Check validity of packet data */
    if( p_block->i_buffer <= 1 ||  p_block->p_buffer[0] == '\0' )
    {
        msg_Warn( p_dec, "empty subtitle" );
        return;
    }

    /* Should be resiliant against bad subtitles */
    psz_subtitle = strndup( p_block->p_buffer, p_block->i_buffer );
    
    i_align_h = p_sys->i_align ? 20 : 0;
    i_align_v = 10;

#if defined(HAVE_ICONV)
    if( p_sys->iconv_handle != (iconv_t)-1 )
    {
        char *psz_new_subtitle;
        char *psz_convert_buffer_out;
        const char *psz_convert_buffer_in;
        size_t ret, inbytes_left, outbytes_left;

        psz_new_subtitle = malloc( 6 * strlen( psz_subtitle ) );
        psz_convert_buffer_out = psz_new_subtitle;
        psz_convert_buffer_in = psz_subtitle;
        inbytes_left = strlen( psz_subtitle );
        outbytes_left = 6 * inbytes_left;
        ret = iconv( p_sys->iconv_handle, &psz_convert_buffer_in,
                     &inbytes_left, &psz_convert_buffer_out, &outbytes_left );
        *psz_convert_buffer_out = '\0';

        if( inbytes_left )
        {
            msg_Warn( p_dec, "Something fishy happened during conversion" );
        }
        else
        {
            free( psz_subtitle );
            psz_subtitle = psz_new_subtitle;
        }
    }
#endif

    if( p_dec->p_fifo->i_fourcc == VLC_FOURCC('s','s','a',' ') )
    {
        /* Decode SSA strings */
        /* We expect: ReadOrder, Layer, Style, Name, MarginL, MarginR, MarginV, Effect, Text */
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
                if( psz_buffer_sub[0] == '\\' && ( psz_buffer_sub[1] =='n' || psz_buffer_sub[1] =='N' ) )
                {
                    psz_new_subtitle[i_text] = '\n';
                    i_text++;
                    psz_buffer_sub += 2;
                }
                else if( psz_buffer_sub[0] == '{' && psz_buffer_sub[1] == '\\' )
                {
                    /* SSA control code */
                    while( psz_buffer_sub[0] != '\0' && psz_buffer_sub[0] != '}' )
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

    vout_ShowTextAbsolute( p_vout, psz_subtitle, NULL, 
                           OSD_ALIGN_BOTTOM | p_sys->i_align,
                           i_align_h, i_align_v, 
                           p_block->i_pts, p_block->i_dts );

    free( psz_subtitle );
}
