/*****************************************************************************
 * fourcc.h : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: fourcc.h,v 1.1 2002/04/23 23:44:36 fenrir Exp $
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


#define mmioFOURCC( ch0, ch1, ch2, ch3 ) \
   ( ((u32)ch0) | ( ((u32)ch1) << 8 ) | \
     ( ((u32)ch2) << 16 ) | ( ((u32)ch3) << 24 ) )

#define mmioTWOCC( ch0, ch1 ) \
        ( (u32)(ch0) | ( (u32)(ch1) << 8 ) )
        
#define WAVE_FORMAT_UNKNOWN         0x0000
#define WAVE_FORMAT_PCM             0x0001
#define WAVE_FORMAT_MPEG            0x0050
#define WAVE_FORMAT_MPEGLAYER3      0x0055
#define WAVE_FORMAT_AC3             0x2000
        
#define FOURCC_RIFF                 mmioFOURCC( 'R', 'I', 'F', 'F' )
#define FOURCC_LIST                 mmioFOURCC( 'L', 'I', 'S', 'T' )
#define FOURCC_JUNK                 mmioFOURCC( 'J', 'U', 'N', 'K' )
#define FOURCC_AVI                  mmioFOURCC( 'A', 'V', 'I', ' ' )
#define FOURCC_WAVE                 mmioFOURCC( 'W', 'A', 'V', 'E' )

#define FOURCC_avih                 mmioFOURCC( 'a', 'v', 'i', 'h' )
#define FOURCC_hdrl                 mmioFOURCC( 'h', 'd', 'r', 'l' )
#define FOURCC_movi                 mmioFOURCC( 'm', 'o', 'v', 'i' )
#define FOURCC_idx1                 mmioFOURCC( 'i', 'd', 'x', '1' )

#define FOURCC_strl                 mmioFOURCC( 's', 't', 'r', 'l' )
#define FOURCC_strh                 mmioFOURCC( 's', 't', 'r', 'h' )
#define FOURCC_strf                 mmioFOURCC( 's', 't', 'r', 'f' )
#define FOURCC_strd                 mmioFOURCC( 's', 't', 'r', 'd' )

#define FOURCC_rec                  mmioFOURCC( 'r', 'e', 'c', ' ' )
#define FOURCC_auds                 mmioFOURCC( 'a', 'u', 'd', 's' )
#define FOURCC_vids                 mmioFOURCC( 'v', 'i', 'd', 's' )
        

#define TWOCC_wb                    mmioTWOCC( 'w', 'b' )
#define TWOCC_db                    mmioTWOCC( 'd', 'b' )
#define TWOCC_dc                    mmioTWOCC( 'd', 'c' )
#define TWOCC_pc                    mmioTWOCC( 'p', 'c' )
        
        
/* definition of mpeg4 (opendivx) codec */
#define FOURCC_DIVX         mmioFOURCC( 'D', 'I', 'V', 'X' )
#define FOURCC_divx         mmioFOURCC( 'd', 'i', 'v', 'x' )
#define FOURCC_DX50         mmioFOURCC( 'D', 'X', '5', '0' )
#define FOURCC_MP4S         mmioFOURCC( 'M', 'P', '4', 'S' )
#define FOURCC_MPG4         mmioFOURCC( 'M', 'P', 'G', '4' )
#define FOURCC_mpg4         mmioFOURCC( 'm', 'p', 'g', '4' )
#define FOURCC_mp4v         mmioFOURCC( 'm', 'p', '4', 'v' )
        
/* definition of msmepg (divx v3) codec */
#define FOURCC_DIV3         mmioFOURCC( 'D', 'I', 'V', '3' )
#define FOURCC_div3         mmioFOURCC( 'd', 'i', 'v', '3' )
#define FOURCC_DIV4         mmioFOURCC( 'D', 'I', 'V', '4' )
#define FOURCC_div4         mmioFOURCC( 'd', 'i', 'v', '4' )
#define FOURCC_DIV5         mmioFOURCC( 'D', 'I', 'V', '5' )
#define FOURCC_div5         mmioFOURCC( 'd', 'i', 'v', '5' )
#define FOURCC_DIV6         mmioFOURCC( 'D', 'I', 'V', '6' )
#define FOURCC_div6         mmioFOURCC( 'd', 'i', 'v', '6' )
#define FOURCC_3IV1         mmioFOURCC( '3', 'I', 'V', '1' )
#define FOURCC_AP41         mmioFOURCC( 'A', 'P', '4', '1' )
#define FOURCC_MP43         mmioFOURCC( 'M', 'P', '4', '3' )
#define FOURCC_mp43         mmioFOURCC( 'm', 'p', '4', '3' )
        
