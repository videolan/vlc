/*****************************************************************************
 * asf.h : ASFv01 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: asf.h,v 1.1 2002/10/20 17:22:33 fenrir Exp $
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

typedef struct asf_stream_s
{
    int i_cat;
    es_descriptor_t *p_es;
    asf_object_stream_properties_t *p_sp;

    mtime_t i_time;

    pes_packet_t    *p_pes; // used to keep uncomplete frames

} asf_stream_t;

struct demux_sys_t
{

    mtime_t             i_pcr;  // 1/90000 s
    mtime_t             i_time; //  µs

    asf_object_root_t   root;
    asf_object_file_properties_t    *p_fp;
    
    int                 i_streams;
    asf_stream_t        *stream[128];

    off_t               i_data_begin;
    off_t               i_data_end;
    
};
