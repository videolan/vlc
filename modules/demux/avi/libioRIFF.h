/*****************************************************************************
 * libioRIFF.h : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: libioRIFF.h,v 1.3 2002/10/15 00:55:07 fenrir Exp $
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
#define MKFOURCC( a, b, c, d ) \
    ( ((u32)a) | ( ((u32)b) << 8 ) | ( ((u32)c) << 16 ) | ( ((u32)d) << 24 ) )
typedef struct riffchunk_s
{
    vlc_fourcc_t i_id;
    u32 i_size;
    vlc_fourcc_t i_type;
    u32 i_pos;
    data_packet_t *p_data;
    u64 i_8bytes; /* it's the first 8 bytes after header 
                     used for key frame generation */
} riffchunk_t;

int         __RIFF_TellPos( input_thread_t *p_input, u32 *pos );
int 	    __RIFF_SkipBytes(input_thread_t * p_input,int nb);
void        RIFF_DeleteChunk( input_thread_t *p_input, riffchunk_t *p_chunk );
riffchunk_t *RIFF_ReadChunk(input_thread_t * p_input);
int         RIFF_NextChunk( input_thread_t * p_input,riffchunk_t *p_rifffather);

