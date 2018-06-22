/*****************************************************************************
 * ogg_granule.h : ogg granule functions
 *****************************************************************************
 * Copyright (C) 2008 - 2018 VideoLAN Authors and VideoLabs
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

int64_t Ogg_GetKeyframeGranule ( const logical_stream_t *, int64_t i_granule );
bool    Ogg_IsKeyFrame ( const logical_stream_t *, const ogg_packet * );
vlc_tick_t Ogg_GranuleToTime( const logical_stream_t *, int64_t i_granule,
                           bool b_packetstart, bool b_pts );
vlc_tick_t Ogg_SampleToTime( const logical_stream_t *, int64_t i_sample,
                          bool b_packetstart );
bool    Ogg_GranuleIsValid( const logical_stream_t *, int64_t i_granule );
