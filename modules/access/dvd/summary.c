/*****************************************************************************
 * summary.c: set of functions to print options of selected title
 * found in .ifo.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: summary.c,v 1.1 2002/08/04 17:23:41 sam Exp $
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

#include <vlc/vlc.h>

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
#   include "dvdcss.h"
#else
#   include <dvdcss/dvdcss.h>
#endif

#include "dvd.h"
#include "ifo.h"
#include "iso_lang.h"

/*
 * Local tools to decode some data in ifo
 */

/****************************************************************************
 * IfoPrintTitle
 ****************************************************************************/
void IfoPrintTitle( thread_dvd_data_t * p_dvd )
{
//X    intf_WarnMsg( 5, "dvd info: title %d, %d chapter%s, %d angle%s",
//X                     p_dvd->i_title, p_dvd->i_chapter_nb,
//X                     (p_dvd->i_chapter_nb == 1) ? "" : "s",
//X                     p_dvd->i_angle_nb,
//X                     (p_dvd->i_angle_nb == 1) ? "" : "s" );
}

/****************************************************************************
 * IfoPrintVideo
 ****************************************************************************/
#define video p_dvd->p_ifo->vts.manager_inf.video_attr
void IfoPrintVideo( thread_dvd_data_t * p_dvd )
{
//X    char*    psz_perm_displ[4] =
//X             {
//X                "pan-scan & letterboxed",
//X                "pan-scan",
//X                "letterboxed",
//X                "not specified"
//X             };
//X    char*    psz_source_res[4] =
//X             {
//X                "720x480 ntsc or 720x576 pal",
//X                "704x480 ntsc or 704x576 pal",
//X                "352x480 ntsc or 352x576 pal",
//X                "352x240 ntsc or 352x288 pal"
//X             };

//X    intf_WarnMsg( 5, "dvd info: MPEG-%d video, %sHz, aspect ratio %s",
//X                     video.i_compression + 1,
//X                     video.i_system ? "pal 625 @50" : "ntsc 525 @60",
//X                     video.i_ratio ? (video.i_ratio == 3) ? "16:9"
//X                                                          : "unknown"
//X                                   : "4:3" );

//X    intf_WarnMsg( 5, "dvd info: display mode %s, %s, %s",
//X                     psz_perm_displ[video.i_perm_displ],
//X                     video.i_line21_1 ? "line21-1 data in GOP"
//X                                      : "no line21-1 data",
//X                     video.i_line21_2 ? "line21-2 data in GOP"
//X                                      : "no line21-2 data" );

//X    intf_WarnMsg( 5, "dvd info: source is %s, %sletterboxed, %s mode",
//X                     psz_source_res[video.i_source_res],
//X                     video.i_letterboxed ? "" : "not ",
//X                     video.i_mode ? "film (625/50 only)" : "camera" );
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
//X        char* ppsz_mode[8] =
//X            { "A52", "unknown", "MPEG", "MPEG-2", "LPCM", "SDDS", "DTS", "" };
//X        char* ppsz_appl_mode[4] =
//X            { "no application specified", "karaoke", "surround sound", "" };
//X        char* ppsz_quant[4] =
//X            { "16 bits", "20 bits", "24 bits", "drc" };
    
//X        intf_WarnMsg( 5, "dvd info: audio %d (%s) is %s, "
//X                         "%d%s channel%s, %dHz, %s", i,
//X                         DecodeLanguage( audio.i_lang_code ),
//X                         ppsz_mode[audio.i_coding_mode & 0x7],
//X                         audio.i_num_channels + 1,
//X                         audio.i_multichannel_extension ? " ext." : "",
//X                         audio.i_num_channels ? "s" : "",
//X                         audio.i_sample_freq ? 96000 : 48000,
//X                         ppsz_appl_mode[audio.i_appl_mode & 0x3] );

//X        intf_WarnMsg( 5, "dvd info: %s, quantization %s, status %x",
//X                         (audio.i_caption == 1) ? "normal caption"
//X                           : (audio.i_caption == 3) ? "directors comments"
//X                               : "unknown caption",
//X                         ppsz_quant[audio.i_quantization & 0x3],
//X                         audio_status.i_position );
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
//X        intf_WarnMsg( 5, "dvd info: spu %d (%s), caption %d "
//X                         "prefix %x, modes [%s%s%s%s ]", i,
//X                         DecodeLanguage( spu.i_lang_code ),
//X                         spu.i_caption, spu.i_prefix,
//X                         spu_status.i_position_43 ? " 4:3" : "",
//X                         spu_status.i_position_wide ? " wide" : "",
//X                         spu_status.i_position_letter ? " letter" : "",
//X                         spu_status.i_position_pan ? " pan" : "" );
    }
}
#undef spu_status
#undef spu
