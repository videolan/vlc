/*****************************************************************************
 * dvd_summary.c: set of functions to print options of selected title
 * found in .ifo.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_summary.c,v 1.16 2002/04/03 06:23:08 sam Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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
#include <stdio.h>
#include <stdlib.h>

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if !defined( WIN32 )
#   include <netinet/in.h>
#endif

#include <fcntl.h>
#include <sys/types.h>

#include <string.h>
#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif
#include <errno.h>

#ifdef GOD_DAMN_DMCA
#   include "dummy_dvdcss.h"
#else
#   include <dvdcss/dvdcss.h>
#endif

#include "dvd.h"
#include "dvd_ifo.h"
#include "iso_lang.h"

#include "debug.h"

/*
 * Local tools to decode some data in ifo
 */

/****************************************************************************
 * IfoPrintTitle
 ****************************************************************************/
void IfoPrintTitle( thread_dvd_data_t * p_dvd )
{
    intf_WarnMsg( 5, "dvd info: title %d, %d chapter%s, %d angle%s",
                     p_dvd->i_title, p_dvd->i_chapter_nb,
                     (p_dvd->i_chapter_nb == 1) ? "" : "s",
                     p_dvd->i_angle_nb,
                     (p_dvd->i_angle_nb == 1) ? "" : "s" );
}

/****************************************************************************
 * IfoPrintVideo
 ****************************************************************************/
#define video p_dvd->p_ifo->vts.manager_inf.video_attr
void IfoPrintVideo( thread_dvd_data_t * p_dvd )
{
    char*    psz_perm_displ[4] =
             {
                "pan-scan & letterboxed",
                "pan-scan",
                "letterboxed",
                "not specified"
             };
    char*    psz_source_res[4] =
             {
                "720x480 ntsc or 720x576 pal",
                "704x480 ntsc or 704x576 pal",
                "352x480 ntsc or 352x576 pal",
                "352x240 ntsc or 352x288 pal"
             };

    intf_WarnMsg( 5, "dvd info: MPEG-%d video, %sHz, aspect ratio %s",
                     video.i_compression + 1,
                     video.i_system ? "pal 625 @50" : "ntsc 525 @60",
                     video.i_ratio ? (video.i_ratio == 3) ? "16:9"
                                                          : "unknown"
                                   : "4:3" );

    intf_WarnMsg( 5, "dvd info: display mode %s, %s, %s",
                     psz_perm_displ[video.i_perm_displ],
                     video.i_line21_1 ? "line21-1 data in GOP"
                                      : "no line21-1 data",
                     video.i_line21_2 ? "line21-2 data in GOP"
                                      : "no line21-2 data" );

    intf_WarnMsg( 5, "dvd info: source is %s, %sletterboxed, %s mode",
                     psz_source_res[video.i_source_res],
                     video.i_letterboxed ? "" : "not ",
                     video.i_mode ? "film (625/50 only)" : "camera" );
}
#undef video

/****************************************************************************
 * IfoPrintAudio
 ****************************************************************************/
#define audio p_dvd->p_ifo->vts.manager_inf.p_audio_attr[i-1]
#define audio_status \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_title_id-1].title.pi_audio_status[i-1]
void IfoPrintAudio( thread_dvd_data_t * p_dvd, int i )
{
    if( audio_status.i_available )
    {
        char* ppsz_mode[7] =
                { "ac3", "unknown", "mpeg-1", "mpeg-2", "lpcm", "sdds", "dts" };
        char* ppsz_appl_mode[3] =
                { "no application specified", "karaoke", "surround sound" };
        char* ppsz_quant[4] =
                { "16 bits", "20 bits", "24 bits", "drc" };
    
        intf_WarnMsg( 5, "dvd info: audio %d (%s) is %s, "
                         "%d%s channel%s, %dHz, %s", i,
                         DecodeLanguage( hton16( audio.i_lang_code ) ),
                         ppsz_mode[audio.i_coding_mode & 0x7],
                         audio.i_num_channels + 1,
                         audio.i_multichannel_extension ? " ext." : "",
                         audio.i_num_channels ? "s" : "",
                         audio.i_sample_freq ? 96000 : 48000,
                         ppsz_appl_mode[audio.i_appl_mode & 0x2] );

        intf_WarnMsg( 5, "dvd info: %s, quantization %s, status %x",
                         (audio.i_caption == 1) ? "normal caption"
                           : (audio.i_caption == 3) ? "directors comments"
                               : "unknown caption",
                         ppsz_quant[audio.i_quantization & 0x3],
                         audio_status.i_position );
    }
}
#undef audio_status
#undef audio

/****************************************************************************
 * IfoPrintSpu
 ****************************************************************************/
#define spu p_dvd->p_ifo->vts.manager_inf.p_spu_attr[i-1]
#define spu_status \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_title_id-1].title.pi_spu_status[i-1]

void IfoPrintSpu( thread_dvd_data_t * p_dvd, int i )
{
    if( spu_status.i_available )
    {
        intf_WarnMsg( 5, "dvd info: spu %d (%s), caption %d "
                         "prefix %x, modes [%s%s%s%s ]", i,
                         DecodeLanguage( hton16( spu.i_lang_code ) ),
                         spu.i_caption, spu.i_prefix,
                         spu_status.i_position_43 ? " 4:3" : "",
                         spu_status.i_position_wide ? " wide" : "",
                         spu_status.i_position_letter ? " letter" : "",
                         spu_status.i_position_pan ? " pan" : "" );
    }
}
#undef spu_status
#undef spu
