/*****************************************************************************
 * cinepak.h: Cinepak video decoder
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: cinepak.h,v 1.2 2002/11/27 13:17:27 fenrir Exp $
 *
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

#define CINEPAK_MAXSTRIP 32

typedef struct cinepak_codebook_s
{
    u8 i_y[4];
    u8 i_u, i_v;
    
} cinepak_codebook_t;


typedef struct cinepak_context_s
{
    int b_grayscale; /* force to grayscale */
    
    int i_width;
    int i_height;

    int i_stride_x;
    int i_stride_y;
     
    u8  *p_y, *p_u, *p_v;

    int i_stride[3]; /* our 3 planes */
    int i_lines[3];
    u8  *p_pix[3];
    
    cinepak_codebook_t codebook_v1[CINEPAK_MAXSTRIP][256];
    cinepak_codebook_t codebook_v4[CINEPAK_MAXSTRIP][256];
    
} cinepak_context_t;

typedef struct videodec_thread_s
{
    decoder_fifo_t  *p_fifo;    

    vout_thread_t   *p_vout; 

    cinepak_context_t *p_context;

    /* private */
    mtime_t i_pts;
    u8      *p_buffer;      /* buffer for gather pes */  \
    int     i_buffer;       /* size of allocated p_framedata */
    
//    int     i_framesize;
//    byte_t  *p_framedata;
} videodec_thread_t;
