/*****************************************************************************
 * text.c: text subtitles parser
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: text.c,v 1.6 2003/07/14 21:32:58 sigmunau Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

#include "spudec.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

/*****************************************************************************
 * ParseText: parse an text subtitle packet and send it to the video output
 *****************************************************************************/
void E_(ParseText)( spudec_thread_t *p_spudec, subtitler_font_t *p_font )
{
    char         * psz_subtitle;
    mtime_t        i_pts, i_dts;
    /* We cannot display a subpicture with no date */
    i_pts = p_spudec->bit_stream.p_pes->i_pts;
    i_dts = p_spudec->bit_stream.p_pes->i_dts;
    if( i_pts == 0 )
    {
        /* Dump the packet */
        NextDataPacket( p_spudec->p_fifo, &p_spudec->bit_stream );
        msg_Warn( p_spudec->p_fifo, "subtitle without a date" );
        return;
    }

    /* Check validity of packet data */
    if( (p_spudec->bit_stream.p_data->p_payload_end
          - p_spudec->bit_stream.p_data->p_payload_start) <= 0
        || (strlen(p_spudec->bit_stream.p_data->p_payload_start)
            > (size_t)(p_spudec->bit_stream.p_data->p_payload_end
                        - p_spudec->bit_stream.p_data->p_payload_start)) )
    {
        /* Dump the packet */
        NextDataPacket( p_spudec->p_fifo, &p_spudec->bit_stream );
        msg_Warn( p_spudec->p_fifo, "invalid subtitle" );
        return;
    }
    psz_subtitle = p_spudec->bit_stream.p_data->p_payload_start;

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
	ret = iconv( p_spudec->iconv_handle, &psz_convert_buffer_in, &inbytes_left, &psz_convert_buffer_out, &outbytes_left );
	*psz_convert_buffer_out = '\0';
	if( inbytes_left )
	{
	    msg_Warn( p_spudec->p_fifo, "Something fishy happened during conversion" );
	}
	else
	{
	    msg_Dbg( p_spudec->p_fifo, "reencoded \"%s\" into \"%s\"", psz_subtitle, psz_new_subtitle );
	    vout_ShowTextAbsolute( p_spudec->p_vout, psz_new_subtitle, NULL, 
				   OSD_ALIGN_BOTTOM|OSD_ALIGN_LEFT, 20, 20, 
				   i_pts, i_dts );
	}
	free( psz_new_subtitle );
#else
	vout_ShowTextAbsolute( p_spudec->p_vout, psz_subtitle, NULL, 
			       OSD_ALIGN_BOTTOM|OSD_ALIGN_LEFT, 20, 20, 
			       i_pts, i_dts );
#endif
#if 0
        subtitler_PlotSubtitle( p_spudec->p_vout,
                                psz_subtitle, p_font,
                                i_pts,
                                i_dts );
#endif
    }

    /* Prepare for next time. No need to check that
     * p_spudec->bit_stream->p_data is valid since we check later on
     * for b_die and b_error */
    NextDataPacket( p_spudec->p_fifo, &p_spudec->bit_stream );
}
