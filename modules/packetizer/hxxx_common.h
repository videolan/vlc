/*****************************************************************************
 * hxxx_common.h: AVC/HEVC packetizers shared code
 *****************************************************************************
 * Copyright (C) 2001-2015 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef HXXX_COMMON_H
#define HXXX_COMMON_H

#include <vlc_common.h>

/* */
typedef struct cc_storage_t cc_storage_t;

cc_storage_t * cc_storage_new( void );
void cc_storage_delete( cc_storage_t *p_ccs );

void cc_storage_reset( cc_storage_t *p_ccs );
void cc_storage_append( cc_storage_t *p_ccs, bool b_top_field_first,
                                      const uint8_t *p_buf, size_t i_buf );
void cc_storage_commit( cc_storage_t *p_ccs, block_t *p_pic );

block_t * cc_storage_get_current( cc_storage_t *p_ccs, decoder_cc_desc_t * );

/* */

typedef block_t * (*pf_annexb_nal_packetizer)(decoder_t *, bool *, block_t *);
block_t *PacketizeXXC1( decoder_t *, uint8_t, block_t **, pf_annexb_nal_packetizer );

#endif // HXXX_COMMON_H

