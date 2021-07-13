/*****************************************************************************
 * stream_output.h : internal stream output
 *****************************************************************************
 * Copyright (C) 2002-2005 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
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
 ***************************************************************************/

#ifndef VLC_SRC_STREAMOUT_H
# define VLC_SRC_STREAMOUT_H 1

# include <vlc_sout.h>
# include <vlc_network.h>

/** Stream output instance */
sout_stream_t *sout_NewInstance( vlc_object_t *, const char * );
#define sout_NewInstance(a,b) sout_NewInstance(VLC_OBJECT(a),b)
void sout_DeleteInstance( sout_stream_t * );

bool sout_instance_ControlsPace( sout_stream_t *sout );

sout_packetizer_input_t *sout_InputNew( sout_stream_t *, const es_format_t * );
int sout_InputDelete( sout_stream_t *, sout_packetizer_input_t * );
int sout_InputSendBuffer( sout_stream_t *, sout_packetizer_input_t *,
                          block_t * );

enum sout_input_query_e
{
    SOUT_INPUT_SET_SPU_HIGHLIGHT, /* arg1=const vlc_spu_highlight_t *, can fail */
};
int  sout_InputControl( sout_stream_t *, sout_packetizer_input_t *,
                        int i_query, ... );
void sout_InputFlush( sout_stream_t *, sout_packetizer_input_t * );

#endif
