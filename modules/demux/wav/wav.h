/*****************************************************************************
 * wav.h : wav file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: wav.h,v 1.3 2003/02/24 09:18:07 fenrir Exp $
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Structure needed for decoder
 *****************************************************************************/
struct demux_sys_t
{

    mtime_t         i_pcr;
    mtime_t         i_time;

    vlc_fourcc_t    i_fourcc;
    es_descriptor_t *p_es;

    int             i_wf;  /* taille de p_wf */
    WAVEFORMATEX    *p_wf;

    off_t           i_data_pos;
    uint64_t        i_data_size;

    /* Two case:
        - we have an internal demux(pcm)
        - we use an external demux(mp3, a52 ..)
    */

    /* module for external demux */
    module_t        *p_demux;
    int             (*pf_demux)( input_thread_t * );
    void            *p_demux_data;
    char            *psz_demux;

    /* getframe for internal demux */
    int (*GetFrame)( input_thread_t *p_input,
                     WAVEFORMATEX *p_wf,
                     pes_packet_t **pp_pes,
                     mtime_t *pi_length );
};



