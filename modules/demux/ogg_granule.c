/*****************************************************************************
 * ogg_granule.c : ogg granule functions
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_LIBVORBIS
  #include <vorbis/codec.h>
#endif

#include <ogg/ogg.h>

#include <vlc_common.h>
#include <vlc_codecs.h>
#include <vlc_es.h>

#include "ogg.h"
#include "ogg_granule.h"

/* Theora spec 7.1 */
#define THEORA_FTYPE_NOTDATA       0x80
#define THEORA_FTYPE_INTERFRAME    0x40

/* Checks if current packet matches codec keyframe */
bool Ogg_IsKeyFrame( const logical_stream_t *p_stream, const ogg_packet *p_packet )
{
    if ( p_stream->b_oggds )
    {
        return ( p_packet->bytes > 0 && p_packet->packet[0] & PACKET_IS_SYNCPOINT );
    }
    else switch ( p_stream->fmt.i_codec )
    {
        case VLC_CODEC_THEORA:
        case VLC_CODEC_DAALA: /* Same convention used in daala */
            if ( p_packet->bytes <= 0 || p_packet->packet[0] & THEORA_FTYPE_NOTDATA )
                return false;
            else
                return !( p_packet->packet[0] & THEORA_FTYPE_INTERFRAME );
        case VLC_CODEC_VP8:
            return ( ( ( p_packet->granulepos >> 3 ) & 0x07FFFFFF ) == 0 );
        case VLC_CODEC_DIRAC:
            if( p_stream->special.dirac.b_old )
                return (p_packet->granulepos & 0x3FFFFFFF) == 0;
            else
                return (p_packet->granulepos & 0xFF8000FF) == 0;
        default:
            return true;
    }
}

int64_t Ogg_GetKeyframeGranule( const logical_stream_t *p_stream, int64_t i_granule )
{
    if ( p_stream->b_oggds )
    {
           return -1; /* We have no way to know */
    }
    else switch( p_stream->fmt.i_codec )
    {
        case VLC_CODEC_THEORA:
        case VLC_CODEC_DAALA:
            return ( i_granule >> p_stream->i_granule_shift ) << p_stream->i_granule_shift;
        case VLC_CODEC_DIRAC:
            if( p_stream->special.dirac.b_old )
                return ( i_granule >> 30 ) << 30;
            else
                return ( i_granule >> 31 ) << 31;
        default:
            /* No change, that's keyframe */
            return i_granule;
    }
}

static int64_t Ogg_GranuleToSampleDelta( const logical_stream_t *p_stream, int64_t i_granule )
{
    if( p_stream->fmt.i_codec == VLC_CODEC_DIRAC && !p_stream->special.dirac.b_old )
        return (i_granule >> 9) & 0x1fff;
    else
        return -1;
}

static int64_t Ogg_GranuleToSample( const logical_stream_t *p_stream, int64_t i_granule )
{
    switch( p_stream->fmt.i_codec )
    {
        case VLC_CODEC_THEORA:
            if( p_stream->i_first_frame_index == 0 && !p_stream->b_oggds )
                i_granule++;
            /* fallthrough */
        case VLC_CODEC_DAALA:
        case VLC_CODEC_KATE:
        {
            ogg_int64_t iframe = i_granule >> p_stream->i_granule_shift;
            ogg_int64_t pframe = i_granule - ( iframe << p_stream->i_granule_shift );
            return iframe + pframe;
        }
        case VLC_CODEC_VP8:
        case VLC_CODEC_OGGSPOTS:
            return i_granule >> p_stream->i_granule_shift;
        case VLC_CODEC_DIRAC:
            if( p_stream->special.dirac.b_old )
                return (i_granule >> 30) + (i_granule & 0x3FFFFFFF);
            else
                return (i_granule >> 31);
        case VLC_CODEC_OPUS:
        case VLC_CODEC_VORBIS:
        case VLC_CODEC_SPEEX:
        case VLC_CODEC_FLAC:
            return i_granule/* - p_stream->i_pre_skip*/;
        default:
            return i_granule;
    }
}

static int64_t Ogg_ShiftPacketSample( const logical_stream_t *p_stream,
                                      int64_t i_sample, bool b_start )
{
    /* /!\ Packet Granule as sample value ! */

    /* granule always point to end time of packet
       Except with OggDS where it is reversed */
    int64_t i_endtostartoffset = 0; /* in interval # */
    if( p_stream->b_oggds )
        i_endtostartoffset = (b_start ? 0 : 1);
    else
        i_endtostartoffset = (b_start ? -1 : 0);

    if( p_stream->fmt.i_cat == VIDEO_ES )
    {
        if( p_stream->fmt.i_codec == VLC_CODEC_DIRAC ) /* points to start */
            i_sample += (p_stream->special.dirac.b_interlaced ? 1 : 2) * (i_endtostartoffset + 1);
        else
            i_sample += i_endtostartoffset * 1;
    }
    else if( p_stream->fmt.i_cat == AUDIO_ES )
    {
        if( p_stream->fmt.i_codec == VLC_CODEC_SPEEX )
        {
            i_sample += i_endtostartoffset *
                        p_stream->special.speex.i_framesize *
                        p_stream->special.speex.i_framesperpacket;
        }
        else /* we can't tell */
        {
            if( i_endtostartoffset != 0 )
                return -1;
        }
    }
    return i_sample;
}

vlc_tick_t Ogg_SampleToTime( const logical_stream_t *p_stream, int64_t i_sample, bool b_start )
{
    i_sample = Ogg_ShiftPacketSample( p_stream, i_sample, b_start );
    if( i_sample < 0 )
        return VLC_TICK_INVALID;

    date_t d = p_stream->dts;
    date_Set(&d, VLC_TICK_0);
    return date_Increment( &d, i_sample );
}

bool Ogg_GranuleIsValid( const logical_stream_t *p_stream, int64_t i_granule )
{
    /* First frame in ogm is 0 (0[header] 0[frame] -1 2 3 -1 5 ...) */
    return !( i_granule < p_stream->i_first_frame_index - !!p_stream->b_oggds );
}

vlc_tick_t Ogg_GranuleToTime( const logical_stream_t *p_stream, int64_t i_granule,
                           bool b_start, bool b_pts )
{
    if( !Ogg_GranuleIsValid( p_stream, i_granule ) )
        return VLC_TICK_INVALID;

    int64_t i_sample = Ogg_GranuleToSample( p_stream, i_granule );
    if( b_pts )
    {
        int64_t i_delta = Ogg_GranuleToSampleDelta( p_stream, i_granule );
        if( i_delta != -1 )
            i_sample += i_delta;
    }
    return Ogg_SampleToTime( p_stream, i_sample, b_start );
}
