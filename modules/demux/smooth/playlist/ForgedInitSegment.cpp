/*
 * ForgedInitSegment.cpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

#include "ForgedInitSegment.hpp"
#include "MemoryChunk.hpp"
#include "../adaptive/playlist/SegmentChunk.hpp"

#include <vlc_common.h>

#include <cstdlib>

extern "C"
{
    #include "../../mux/mp4/libmp4mux.h"
    #include "../../demux/mp4/libmp4.h" /* majors */
}

using namespace adaptive::playlist;
using namespace smooth::playlist;
using namespace smooth::http;

ForgedInitSegment::ForgedInitSegment(ICanonicalUrl *parent,
                                     const std::string &type_,
                                     uint64_t timescale_,
                                     uint64_t duration_) :
    InitSegment(parent), TimescaleAble()
{
    type = type_;
    duration.Set(duration_);
    extradata = NULL;
    i_extradata = 0;
    setTimescale(timescale_);
    formatex.cbSize = 0;
    formatex.nAvgBytesPerSec = 0;
    formatex.nBlockAlign = 0;
    formatex.nChannels = 0;
    formatex.nSamplesPerSec = 0;
    formatex.wBitsPerSample = 0;
    formatex.wFormatTag = 0;
    width = height = 0;
    fourcc = 0;
    es_type = UNKNOWN_ES;
    track_id = 1;
}

ForgedInitSegment::~ForgedInitSegment()
{
    free(extradata);
}

static uint8_t *HexDecode(const std::string &s, size_t *decoded_size)
{
    *decoded_size = s.size() / 2;
    uint8_t *data = (uint8_t *) malloc(*decoded_size);
    if(data)
    {
        for(size_t i=0; i<*decoded_size; i++)
            data[i] = std::strtoul(s.substr(i*2, 2).c_str(), NULL, 16);
    }
    return data;
}

void ForgedInitSegment::fromWaveFormatEx(const uint8_t *p_data, size_t i_data)
{
    if(i_data >= sizeof(WAVEFORMATEX))
    {
        formatex.wFormatTag = GetWLE(p_data);
        wf_tag_to_fourcc(formatex.wFormatTag, &fourcc, NULL);
        formatex.nChannels = GetWLE(&p_data[2]);
        formatex.nSamplesPerSec = GetDWLE(&p_data[4]);
        formatex.nAvgBytesPerSec = GetDWLE(&p_data[8]);
        formatex.nBlockAlign = GetWLE(&p_data[12]);
        formatex.wBitsPerSample = GetWLE(&p_data[14]);
        formatex.cbSize = GetWLE(&p_data[16]);
        if(i_data > sizeof(WAVEFORMATEX))
        {
            if(extradata)
            {
                free(extradata);
                extradata = NULL;
                i_extradata = 0;
            }
            formatex.cbSize = __MIN(i_data - sizeof(WAVEFORMATEX), formatex.cbSize);
            extradata = (uint8_t*)malloc(formatex.cbSize);
            if(extradata)
            {
                memcpy(extradata, &p_data[sizeof(WAVEFORMATEX)], formatex.cbSize);
                i_extradata = formatex.cbSize;
            }
        }
        es_type = AUDIO_ES;
    }
}

void ForgedInitSegment::fromVideoInfoHeader(const uint8_t *, size_t)
{
//    VIDEOINFOHEADER
    es_type = VIDEO_ES;
}

void ForgedInitSegment::setWaveFormatEx(const std::string &waveformat)
{
    size_t i_data;
    uint8_t *p_data = HexDecode(waveformat, &i_data);
    fromWaveFormatEx(p_data, i_data);
    free(p_data);
}

void ForgedInitSegment::setCodecPrivateData(const std::string &extra)
{
    if(extradata)
    {
        free(extradata);
        extradata = NULL;
        i_extradata = 0;
    }
    extradata = HexDecode(extra, &i_extradata);
    if(fourcc == VLC_CODEC_WMAP)
    {
        //fromWaveFormatEx(const std::string &extra);
    }
}

void ForgedInitSegment::setChannels(uint16_t i)
{
    formatex.nChannels = i;
}

void ForgedInitSegment::setPacketSize(uint16_t i)
{
    formatex.nBlockAlign = i;
}

void ForgedInitSegment::setSamplingRate(uint32_t i)
{
    formatex.nSamplesPerSec = i;
}

void ForgedInitSegment::setBitsPerSample(uint16_t i)
{
    formatex.wBitsPerSample = i;
}

void ForgedInitSegment::setVideoSize(unsigned w, unsigned h)
{
    width = w;
    height = h;
}

void ForgedInitSegment::setTrackID(unsigned i)
{
    track_id = i;
}

void ForgedInitSegment::setAudioTag(uint16_t i)
{
    wf_tag_to_fourcc(i, &fourcc, NULL);
}

void ForgedInitSegment::setFourCC(const std::string &fcc)
{
    if(fcc.size() == 4)
    {
        fourcc = VLC_FOURCC(fcc[0], fcc[1], fcc[2], fcc[3]);
        switch(fourcc)
        {
            case VLC_FOURCC( 'A', 'V', 'C', '1' ):
            case VLC_FOURCC( 'A', 'V', 'C', 'B' ):
            case VLC_FOURCC( 'H', '2', '6', '4' ):
            case VLC_FOURCC( 'W', 'V', 'C', '1' ):
                es_type = VIDEO_ES;
                break;
            case VLC_FOURCC( 'A', 'A', 'C', 'L' ):
            case VLC_FOURCC( 'W', 'M', 'A', 'P' ):
            default:
                es_type = AUDIO_ES;
                break;
        }
    }
}

void ForgedInitSegment::setLanguage(const std::string &lang)
{
    language = lang;
}

block_t * ForgedInitSegment::buildMoovBox()
{
    const Timescale &trackTimescale = inheritTimescale();
    mp4mux_trackinfo_t trackinfo;
    mp4mux_trackinfo_Init(&trackinfo,
                          0x01, /* Will always be 1st and unique track; tfhd patched on block read */
                          (uint32_t) trackTimescale);
    trackinfo.i_read_duration = duration.Get();
    trackinfo.i_trex_default_length = 1;
    trackinfo.i_trex_default_size = 1;

    es_format_Init(&trackinfo.fmt, es_type, vlc_fourcc_GetCodec(es_type, fourcc));
    trackinfo.fmt.i_original_fourcc = fourcc;
    switch(es_type)
    {
        case VIDEO_ES:
            if( fourcc == VLC_FOURCC( 'A', 'V', 'C', '1' ) ||
                fourcc == VLC_FOURCC( 'A', 'V', 'C', 'B' ) ||
                fourcc == VLC_FOURCC( 'H', '2', '6', '4' ) )
            {
                trackinfo.fmt.i_codec = VLC_CODEC_H264;
            }
            else if( fourcc == VLC_FOURCC( 'W', 'V', 'C', '1' ) )
            {
                trackinfo.fmt.i_codec = VLC_CODEC_VC1;
//                trackinfo.fmt.video.i_bits_per_pixel = 0x18; // No clue why this was set in smooth streamfilter
            }

            trackinfo.fmt.video.i_width = width;
            trackinfo.fmt.video.i_height = height;
            trackinfo.fmt.video.i_visible_width = width;
            trackinfo.fmt.video.i_visible_height = height;

            if(i_extradata && extradata)
            {
                trackinfo.fmt.p_extra = malloc(i_extradata);
                if(trackinfo.fmt.p_extra)
                {
                    memcpy(trackinfo.fmt.p_extra, extradata, i_extradata);
                    trackinfo.fmt.i_extra = i_extradata;
                }
            }
            break;

        case AUDIO_ES:
            trackinfo.fmt.audio.i_channels = formatex.nChannels;
            trackinfo.fmt.audio.i_rate = formatex.nSamplesPerSec;
            trackinfo.fmt.audio.i_bitspersample = formatex.wBitsPerSample;
            trackinfo.fmt.audio.i_blockalign = formatex.nBlockAlign;
            trackinfo.fmt.i_bitrate = formatex.nAvgBytesPerSec * 8; // FIXME (use bitrate) ?

            if(i_extradata && extradata)
            {
                trackinfo.fmt.p_extra = malloc(i_extradata);
                if(trackinfo.fmt.p_extra)
                {
                    memcpy(trackinfo.fmt.p_extra, extradata, i_extradata);
                    trackinfo.fmt.i_extra = i_extradata;
                }
            }
        default:
            break;
    }

    if(!language.empty())
        trackinfo.fmt.psz_language = strdup(language.c_str());

    mp4mux_trackinfo_t *p_tracks = &trackinfo;
    bo_t *box = NULL;

    if(mp4mux_CanMux( NULL, &trackinfo.fmt ))
       box = mp4mux_GetMoovBox(NULL, &p_tracks, 1,
                               trackTimescale.ToTime(duration.Get()),
                               true, false, false, false);

    mp4mux_trackinfo_Clear(&trackinfo);

    block_t *moov = NULL;
    if(box)
    {
        moov = box->b;
        free(box);
    }

    if(!moov)
        return NULL;

    vlc_fourcc_t extra[] = {MAJOR_isom, VLC_FOURCC('p','i','f','f'), VLC_FOURCC('i','s','o','2'), VLC_FOURCC('s','m','o','o')};
    box = mp4mux_GetFtyp(VLC_FOURCC('i','s','m','l'), 1, extra, ARRAY_SIZE(extra));

    if(box)
    {
        block_ChainAppend(&box->b, moov);
        moov = block_ChainGather(box->b);
        free(box);
    }

    return moov;
}

SegmentChunk* ForgedInitSegment::toChunk(size_t, BaseRepresentation *rep, AbstractConnectionManager *)
{
    block_t *moov = buildMoovBox();
    if(moov)
    {
        MemoryChunkSource *source = new (std::nothrow) MemoryChunkSource(moov);
        if( source )
        {
            SegmentChunk *chunk = new (std::nothrow) SegmentChunk(this, source, rep);
            if( chunk )
                return chunk;
            else
                delete source;
        }
    }
    return NULL;
}
