/*****************************************************************************
 * subsdec.c : SPU decoder thread
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: subsdec.c,v 1.3 2003/07/24 19:07:03 sigmunau Exp $
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

#include "subsdec.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  RunDecoder    ( decoder_fifo_t * );
static int  InitThread    ( subsdec_thread_t * );
static void EndThread     ( subsdec_thread_t * );
static vout_thread_t *FindVout( subsdec_thread_t * );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static char *ppsz_encodings[] = { "ASCII", "ISO-8859-1", "ISO-8859-2", "ISO-8859-3",
    "ISO-8859-4", "ISO-8859-5", "ISO-8859-6", "ISO-8859-7", "ISO-8859-8", 
    "ISO-8859-9", "ISO-8859-10", "ISO-8859-13", "ISO-8859-14", "ISO-8859-15",
    "ISO-8859-16", "ISO-2022-JP", "ISO-2022-JP-1", "ISO-2022-JP-2", "ISO-2022-CN",
    "ISO-2022-CN-EXT", "ISO-2022-KR",
    "CP850", "CP862", "CP866", "CP874", "CP932", "CP949", "CP950", "CP1133",
    "CP1250", "CP1251", "CP1252", "CP1253", "CP1254", "CP1255", "CP1256", "CP1257", "CP1258",
    "MacRoman", "MacCentralEurope", "MacIceland", "MacCroatian", "MacRomania",
    "MacCyrillic", "MacUkraine", "MacGreek", "MacTurkish", "MacHebrew", "MacArabic",
    "MacThai", "Macintosh",
    "UTF-7", "UTF-8", "UTF-16", "UTF-16BE", "UTF-16LE", "UTF-32", "UTF-32BE", "UTF-32LE",
    "C99", "JAVA", "UCS-2", "UCS-2BE", "UCS-2LE", "UCS-4", "UCS-4BE", "UCS-4LE",
    "KOI8-R", "KOI8-U", "KOI8-RU", "KOI8-T",
    "EUC-JP", "EUC-CN", "EUC-KR", "EUC-TW",
    "SHIFT_JIS", "HZ", "GBK", "GB18030", "BIG5", "BIG5-HKSCS", "JOHAB", "ARMSCII-8",
    "Georgian-Academy", "Georgian-PS", "TIS-620", "MuleLao-1", "VISCII", "TCVN",
    "HPROMAN8", "NEXTSTEP", NULL };

#define ENCODING_TEXT N_("subtitle text encoding")
#define ENCODING_LONGTEXT N_("change the encoding used in text subtitles")

vlc_module_begin();
    set_description( _("file subtitles decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, NULL );
    add_category_hint( N_("Subtitles"), NULL, VLC_FALSE );

#if defined(HAVE_ICONV)
    add_string_from_list( "subsdec-encoding", "ISO-8859-1", ppsz_encodings, NULL,
                          ENCODING_TEXT, ENCODING_LONGTEXT, VLC_FALSE );
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
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('s','u','b','t') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t * p_fifo )
{
    subsdec_thread_t *    p_subsdec;

    /* Allocate the memory needed to store the thread's structure */
    p_subsdec = (subsdec_thread_t *)malloc( sizeof(subsdec_thread_t) );

    if ( p_subsdec == NULL )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }

    /*
     * Initialize the thread properties
     */
    p_subsdec->p_vout = NULL;
    p_subsdec->p_fifo = p_fifo;
#if defined(HAVE_ICONV)
    p_subsdec->iconv_handle = (iconv_t)-1;
#endif

    /*
     * Initialize thread and free configuration
     */
    p_subsdec->p_fifo->b_error = InitThread( p_subsdec );

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    if( p_fifo->i_fourcc == VLC_FOURCC('s','u','b','t') )
    {
        /* Here we are dealing with text subtitles */
#if defined(HAVE_ICONV)
	p_subsdec->iconv_handle = iconv_open( "UTF-8",
            config_GetPsz( p_subsdec->p_fifo, "subsdec-encoding" ) );
	if( p_subsdec->iconv_handle == (iconv_t)-1 )
	{
	    msg_Warn( p_subsdec->p_fifo, "Unable to do requested conversion" );
	}
#endif
        while( (!p_subsdec->p_fifo->b_die) && (!p_subsdec->p_fifo->b_error) )
        {
            /* Find/Wait for a video output */
            p_subsdec->p_vout = FindVout( p_subsdec );

            if( p_subsdec->p_vout )
            {
                E_(ParseText)( p_subsdec );
                vlc_object_release( p_subsdec->p_vout );
            }
        }
    }

    /*
     * Error loop
     */
    if( p_subsdec->p_fifo->b_error )
    {
        DecoderError( p_subsdec->p_fifo );

        /* End of thread */
        EndThread( p_subsdec );
        return -1;
    }

    /* End of thread */
    EndThread( p_subsdec );
    return 0;
}

/* following functions are local */

/*****************************************************************************
 * InitThread: initialize spu decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
static int InitThread( subsdec_thread_t *p_subsdec )
{
    int i_ret;

    /* Call InitBitstream anyway so p_subsdec->bit_stream is in a known
     * state before calling CloseBitstream */
    i_ret = InitBitstream( &p_subsdec->bit_stream, p_subsdec->p_fifo,
                           NULL, NULL );

    /* Check for a video output */
    p_subsdec->p_vout = FindVout( p_subsdec );

    if( !p_subsdec->p_vout )
    {
        return -1;
    }

    /* It was just a check */
    vlc_object_release( p_subsdec->p_vout );
    p_subsdec->p_vout = NULL;

    return i_ret;
}

/*****************************************************************************
 * FindVout: Find a vout or wait for one to be created.
 *****************************************************************************/
static vout_thread_t *FindVout( subsdec_thread_t *p_subsdec )
{
    vout_thread_t *p_vout = NULL;

    /* Find an available video output */
    do
    {
        if( p_subsdec->p_fifo->b_die || p_subsdec->p_fifo->b_error )
        {
            break;
        }

        p_vout = vlc_object_find( p_subsdec->p_fifo, VLC_OBJECT_VOUT,
                                  FIND_ANYWHERE );

        if( p_vout )
        {
            break;
        }

        msleep( VOUT_OUTMEM_SLEEP );
    }
    while( 1 );

    return p_vout;
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( subsdec_thread_t *p_subsdec )
{
    if( p_subsdec->p_vout != NULL
         && p_subsdec->p_vout->p_subpicture != NULL )
    {
        subpicture_t *  p_subpic;
        int             i_subpic;

        for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
        {
            p_subpic = &p_subsdec->p_vout->p_subpicture[i_subpic];

            if( p_subpic != NULL &&
              ( ( p_subpic->i_status == RESERVED_SUBPICTURE )
             || ( p_subpic->i_status == READY_SUBPICTURE ) ) )
            {
                vout_DestroySubPicture( p_subsdec->p_vout, p_subpic );
            }
        }
    }
#if defined(HAVE_ICONV)
    if( p_subsdec->iconv_handle != (iconv_t)-1 )
    {
	iconv_close( p_subsdec->iconv_handle );
    }
#endif
    CloseBitstream( &p_subsdec->bit_stream );
    free( p_subsdec );
}

/*****************************************************************************
 * ParseText: parse an text subtitle packet and send it to the video output
 *****************************************************************************/
void E_(ParseText)( subsdec_thread_t *p_subsdec )
{
    char         * psz_subtitle;
    mtime_t        i_pts, i_dts;
    /* We cannot display a subpicture with no date */
    i_pts = p_subsdec->bit_stream.p_pes->i_pts;
    i_dts = p_subsdec->bit_stream.p_pes->i_dts;
    if( i_pts == 0 )
    {
        /* Dump the packet */
        NextDataPacket( p_subsdec->p_fifo, &p_subsdec->bit_stream );
        msg_Warn( p_subsdec->p_fifo, "subtitle without a date" );
        return;
    }

    /* Check validity of packet data */
    if( (p_subsdec->bit_stream.p_data->p_payload_end
          - p_subsdec->bit_stream.p_data->p_payload_start) <= 0
        || (strlen(p_subsdec->bit_stream.p_data->p_payload_start)
            > (size_t)(p_subsdec->bit_stream.p_data->p_payload_end
                        - p_subsdec->bit_stream.p_data->p_payload_start)) )
    {
        /* Dump the packet */
        NextDataPacket( p_subsdec->p_fifo, &p_subsdec->bit_stream );
        msg_Warn( p_subsdec->p_fifo, "invalid subtitle" );
        return;
    }
    psz_subtitle = p_subsdec->bit_stream.p_data->p_payload_start;

    if( psz_subtitle[0] != '\0' )
    {
#if defined(HAVE_ICONV)
	char *psz_new_subtitle, *psz_convert_buffer_out, *psz_convert_buffer_in;
	size_t ret, inbytes_left, outbytes_left;

	psz_new_subtitle = malloc( 6 * strlen( psz_subtitle ) * sizeof(char) );
	psz_convert_buffer_out = psz_new_subtitle;
	psz_convert_buffer_in = psz_subtitle;
	inbytes_left = strlen( psz_subtitle );
	outbytes_left = 6 * inbytes_left;
	ret = iconv( p_subsdec->iconv_handle, &psz_convert_buffer_in,
                     &inbytes_left, &psz_convert_buffer_out, &outbytes_left );
	*psz_convert_buffer_out = '\0';

	if( inbytes_left )
	{
	    msg_Warn( p_subsdec->p_fifo, "Something fishy happened during conversion" );
	}
	else
	{
	    msg_Dbg( p_subsdec->p_fifo, "reencoded \"%s\" into \"%s\"", psz_subtitle, psz_new_subtitle );
            psz_subtitle = psz_new_subtitle;
	}
#endif
	vout_ShowTextAbsolute( p_subsdec->p_vout, psz_subtitle, NULL, 
			       OSD_ALIGN_BOTTOM|OSD_ALIGN_LEFT, 20, 20, 
			       i_pts, i_dts );
#if defined(HAVE_ICONV)
        free( psz_new_subtitle );
#endif
    }

    /* Prepare for next time. No need to check that
     * p_subsdec->bit_stream->p_data is valid since we check later on
     * for b_die and b_error */
    NextDataPacket( p_subsdec->p_fifo, &p_subsdec->bit_stream );
}
