/*****************************************************************************
 * text.c: text subtitles parser
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: text.c,v 1.5 2003/05/11 14:33:32 sigmunau Exp $
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

/* define USE_FREETYPE here to disable the old style subtitles */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#include "spudec.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

/*****************************************************************************
 * ParseText: parse an text subtitle packet and send it to the video output
 *****************************************************************************/
void E_(ParseText)( spudec_thread_t *p_spudec, subtitler_font_t *p_font )
{
#if !defined(USE_FREETYPE)
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
        subtitler_PlotSubtitle( p_spudec->p_vout,
                                psz_subtitle, p_font,
                                i_pts,
                                i_dts );
    }

    /* Prepare for next time. No need to check that
     * p_spudec->bit_stream->p_data is valid since we check later on
     * for b_die and b_error */
    NextDataPacket( p_spudec->p_fifo, &p_spudec->bit_stream );
#else
    msleep(10);
#endif
}
