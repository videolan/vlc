/*****************************************************************************
 * pes.h
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#define PES_PROGRAM_STREAM_MAP          0xbc
#define PES_PRIVATE_STREAM_1            0xbd
#define PES_PADDING                     0xbe
#define PES_PRIVATE_STREAM_2            0xbf
#define PES_ECM                         0xb0
#define PES_EMM                         0xb1
#define PES_PROGRAM_STREAM_DIRECTORY    0xff
#define PES_DSMCC_STREAM                0xf2
#define PES_ITU_T_H222_1_TYPE_E_STREAM  0xf8
#define PES_EXTENDED_STREAM_ID          0xfd

#define PES_PAYLOAD_SIZE_MAX 65500

int  EStoPES ( block_t **pp_pes, block_t *p_es,
                   es_format_t *p_fmt, int i_stream_id,
                   int b_mpeg2, int b_data_alignment, int i_header_size,
                   int i_max_pes_size );
