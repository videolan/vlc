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
#include "../../adaptive/playlist/SegmentChunk.hpp"

#include <vlc_common.h>

#include <cstdlib>

extern "C"
{
    #include "../../../mux/mp4/libmp4mux.h"
    #include "../../mp4/libmp4.h" /* majors */
}

using namespace adaptive;
using namespace adaptive::playlist;
using namespace smooth::playlist;
using namespace smooth::http;

ForgedInitSegment::ForgedInitSegment(ICanonicalUrl *parent,
                                     const std::string &type_,
                                     uint64_t timescale_,
                                     vlc_tick_t duration_) :
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
            case VLC_FOURCC( 'T', 'T', 'M', 'L' ):
                es_type = SPU_ES;
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

    es_format_t fmt;
    es_format_Init(&fmt, es_type, vlc_fourcc_GetCodec(es_type, fourcc));
    fmt.i_original_fourcc = fourcc;
    switch(es_type)
    {
        case VIDEO_ES:
            if( fourcc == VLC_FOURCC( 'A', 'V', 'C', '1' ) ||
                fourcc == VLC_FOURCC( 'A', 'V', 'C', 'B' ) ||
                fourcc == VLC_FOURCC( 'H', '2', '6', '4' ) )
            {
                fmt.i_codec = VLC_CODEC_H264;
            }
            else if( fourcc == VLC_FOURCC( 'W', 'V', 'C', '1' ) )
            {
                fmt.i_codec = VLC_CODEC_VC1;
//                fmt.video.i_bits_per_pixel = 0x18; // No clue why this was set in smooth streamfilter
            }

            fmt.video.i_width = width;
            fmt.video.i_height = height;
            fmt.video.i_visible_width = width;
            fmt.video.i_visible_height = height;

            if(i_extradata && extradata)
            {
                fmt.p_extra = malloc(i_extradata);
                if(fmt.p_extra)
                {
                    memcpy(fmt.p_extra, extradata, i_extradata);
                    fmt.i_extra = i_extradata;
                }
            }
            break;

        case AUDIO_ES:
            fmt.audio.i_channels = formatex.nChannels;
            fmt.audio.i_rate = formatex.nSamplesPerSec;
            fmt.audio.i_bitspersample = formatex.wBitsPerSample;
            fmt.audio.i_blockalign = formatex.nBlockAlign;
            fmt.i_bitrate = formatex.nAvgBytesPerSec * 8; // FIXME (use bitrate) ?

            if(i_extradata && extradata)
            {
                fmt.p_extra = malloc(i_extradata);
                if(fmt.p_extra)
                {
                    memcpy(fmt.p_extra, extradata, i_extradata);
                    fmt.i_extra = i_extradata;
                }
            }
            break;

        case SPU_ES:
            break;

        default:
            break;
    }

    if(!language.empty())
        fmt.psz_language = strdup(language.c_str());

    bo_t *box = NULL;
    mp4mux_handle_t *muxh = mp4mux_New(FRAGMENTED);
    if(muxh)
    {
        if(mp4mux_CanMux(NULL, &fmt, VLC_FOURCC('s', 'm', 'o', 'o'), true ))
        {
            mp4mux_trackinfo_t *p_track = mp4mux_track_Add(muxh,
                                     0x01, /* Will always be 1st and unique track; tfhd patched on block read */
                                     &fmt, (uint32_t) trackTimescale);
            if(p_track)
                mp4mux_track_ForceDuration(p_track, duration.Get());
        }

        box = mp4mux_GetMoov(muxh, NULL, trackTimescale.ToTime(duration.Get()));
    }
    es_format_Clean(&fmt);

    if(!box)
    {
        mp4mux_Delete(muxh);
        return NULL;
    }

    block_t *moov = box->b;
    free(box);

    mp4mux_SetBrand(muxh, BRAND_isml, 0x01);
    mp4mux_AddExtraBrand(muxh, BRAND_isom);
    mp4mux_AddExtraBrand(muxh, BRAND_piff);
    mp4mux_AddExtraBrand(muxh, BRAND_iso2);
    mp4mux_AddExtraBrand(muxh, BRAND_smoo);

    box = mp4mux_GetFtyp(muxh);
    if(box)
    {
        block_ChainAppend(&box->b, moov);
        moov = block_ChainGather(box->b);
        free(box);
    }

    mp4mux_Delete(muxh);
    return moov;
}

SegmentChunk* ForgedInitSegment::toChunk(SharedResources *, AbstractConnectionManager *,
                                         size_t, BaseRepresentation *rep)
{
    block_t *moov = buildMoovBox();
    if(moov)
    {
        MemoryChunkSource *source = new (std::nothrow) MemoryChunkSource(moov);
        if( source )
        {
            SegmentChunk *chunk = new (std::nothrow) SegmentChunk(source, rep);
            if( chunk )
                return chunk;
            else
                delete source;
        }
    }
    return NULL;
}
