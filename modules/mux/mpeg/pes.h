/*****************************************************************************
 * pes.h
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: pes.h,v 1.1 2002/12/14 21:32:41 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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


#define PES_PROGRAM_STREAM_MAP          0xbc
#define PES_PADDING                     0xbe
#define PES_PRIVATE_STREAM_2            0xbf
#define PES_ECM                         0xb0
#define PES_EMM                         0xb1
#define PES_PROGRAM_STREAM_DIRECTORY    0xff
#define PES_DSMCC_STREAM                0xf2
#define PES_ITU_T_H222_1_TYPE_E_STREAM  0xf8


int EStoPES( sout_instance_t *p_sout,
             sout_buffer_t **pp_pes, sout_buffer_t *p_es, 
             int i_stream_id, int b_mpeg2 );
