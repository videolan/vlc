/*****************************************************************************
 * input_dec.h: prototypes needed by an application to use a VideoLAN
 * decoder
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_dec.h,v 1.2 2000/12/22 10:58:27 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
 * Prototypes
 *****************************************************************************/
//decoder_capabilities_s * input_ProbeDecoder( void );
vlc_thread_t input_RunDecoder( struct decoder_capabilities_s *, void * );
void input_EndDecoder( struct decoder_fifo_s *, vlc_thread_t );
void input_DecodePES( struct decoder_fifo_s *, struct pes_packet_s * );
