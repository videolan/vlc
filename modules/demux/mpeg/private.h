/*****************************************************************************
 * system.c: helper module for TS, PS and PES management
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: private.h,v 1.1 2004/01/03 17:52:15 rocky Exp $
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
 * Private structure
 *****************************************************************************/
struct demux_sys_t
{
    module_t *   p_module;
    mpeg_demux_t mpeg;

    /* 
       rocky:
     i_cur_mux_rate and cur_scr_time below are a bit of a hack.
     
     Background: VLC uses the System Clock Reference (SCR) of a PACK
     header to read the stream at the right pace (contrary to other
     players like xine/mplayer which don't use this info and
     synchronise everything on the audio output clock).
     
     The popular SVCD/VCD subtitling WinSubMux does not renumber the
     SCRs when merging subtitles into the PES. Perhaps other
     multipliexing tools are equally faulty. Until such time as
     WinSubMux is fixed or other tools become available and widely
     used, we will cater to the WinSubMux kind of buggy stream. The
     hack here delays using the PACK SCR until the first PES that
     would need it is received. For this we need to temporarily save
     this information in the variables below.
    */
    uint32_t i_cur_mux_rate;
    mtime_t  cur_scr_time;

};

