/*****************************************************************************
 * libioRIFF.h : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: libioRIFF.h,v 1.1 2002/08/04 17:23:42 sam Exp $
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

typedef struct riffchunk_s
{
    u32 i_id;
    u32 i_size;
    u32 i_type;
    u32 i_pos;
    data_packet_t *p_data;
    u64 i_8bytes; /* it's the first 8 bytes after header 
                     used for key frame generation */
} riffchunk_t;

int  __RIFF_TellPos( input_thread_t *p_input, u32 *pos );
int 	__RIFF_SkipBytes(input_thread_t * p_input,int nb);
void RIFF_DeleteChunk( input_thread_t *p_input, riffchunk_t *p_chunk );
riffchunk_t *RIFF_ReadChunk(input_thread_t * p_input);
int  RIFF_NextChunk( input_thread_t * p_input,riffchunk_t *p_rifffather);
int	RIFF_DescendChunk(input_thread_t * p_input);
int	RIFF_AscendChunk(input_thread_t * p_input ,riffchunk_t *p_riff);
int	RIFF_FindChunk(input_thread_t * p_input,
                           u32 i_id,riffchunk_t *p_rifffather);
int  RIFF_GoToChunkData(input_thread_t * p_input);
int	RIFF_LoadChunkData(input_thread_t * p_input,
                               riffchunk_t *p_riff );
int RIFF_LoadChunkDataInPES(input_thread_t * p_input,
                            pes_packet_t **pp_pes,
                            int i_size_index);

int  RIFF_GoToChunk(input_thread_t * p_input, 
                           riffchunk_t *p_riff);
int  RIFF_TestFileHeader( input_thread_t * p_input, 
                                 riffchunk_t ** pp_riff, 
                                 u32 i_type );
int  RIFF_FindAndLoadChunk( input_thread_t * p_input, 
                                   riffchunk_t *p_riff, 
                                   riffchunk_t **pp_fmt, 
                                   u32 i_type );
int  RIFF_FindAndGotoDataChunk( input_thread_t * p_input, 
                                       riffchunk_t *p_riff, 
                                       riffchunk_t **pp_data, 
                                       u32 i_type );
int  RIFF_FindListChunk( input_thread_t *p_input, 
                                riffchunk_t **pp_riff, 
                                riffchunk_t *p_rifffather, 
                                u32 i_type );

