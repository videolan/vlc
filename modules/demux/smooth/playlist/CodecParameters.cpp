/*
 * CodecParameters.cpp
 *****************************************************************************
 * Copyright (C) 2021 - VideoLabs, VideoLAN and VLC Authors
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

#include "CodecParameters.hpp"

using namespace smooth::playlist;

CodecParameters::CodecParameters()
{
    formatex.cbSize = 0;
    formatex.nAvgBytesPerSec = 0;
    formatex.nBlockAlign = 0;
    formatex.nChannels = 0;
    formatex.nSamplesPerSec = 0;
    formatex.wBitsPerSample = 0;
    formatex.wFormatTag = 0;
    fourcc = 0;
    es_type = UNKNOWN_ES;
}

CodecParameters::~CodecParameters()
{}

static void HexDecode(const std::string &s, std::vector<uint8_t> &v)
{
    v.resize(s.size() / 2);
    for(std::vector<uint8_t>::size_type i=0; i<v.size(); i++)
        v[i] = std::strtoul(s.substr(i*2, 2).c_str(), nullptr, 16);
}

void CodecParameters::setCodecPrivateData(const std::string &extra)
{
    HexDecode(extra, extradata);
    if(fourcc == VLC_CODEC_WMAP)
    {
        //fromWaveFormatEx(const std::string &extra);
    }
}

void CodecParameters::setWaveFormatEx(const std::string &waveformat)
{
    std::vector<uint8_t> decoded;
    HexDecode(waveformat, decoded);
    fromWaveFormatEx(decoded);
}

void CodecParameters::setChannels(uint16_t i)
{
    formatex.nChannels = i;
}

void CodecParameters::setPacketSize(uint16_t i)
{
    formatex.nBlockAlign = i;
}

void CodecParameters::setSamplingRate(uint32_t i)
{
    formatex.nSamplesPerSec = i;
}

void CodecParameters::setBitsPerSample(uint16_t i)
{
    formatex.wBitsPerSample = i;
}

void CodecParameters::setAudioTag(uint16_t i)
{
    wf_tag_to_fourcc(i, &fourcc, nullptr);
}

static void FillExtradata(es_format_t *fmt, const std::vector<uint8_t> &extradata)
{
    if(extradata.size())
    {
        free(fmt->p_extra);
        fmt->i_extra = 0;
        fmt->p_extra = malloc(extradata.size());
        if(fmt->p_extra)
        {
            memcpy(fmt->p_extra, &extradata[0], extradata.size());
            fmt->i_extra = extradata.size();
        }
    }
}

void CodecParameters::initAndFillEsFmt(es_format_t *fmt) const
{
    es_format_Init(fmt, es_type, fourcc);
    fmt->i_original_fourcc = fmt->i_codec;
    switch(fmt->i_cat)
    {
        case VIDEO_ES:
            if( fmt->i_codec == VLC_FOURCC( 'A', 'V', 'C', '1' ) ||
                fmt->i_codec == VLC_FOURCC( 'A', 'V', 'C', 'B' ) ||
                fmt->i_codec == VLC_FOURCC( 'H', '2', '6', '4' ) )
            {
                fmt->i_codec = VLC_CODEC_H264;
            }
            else if( fmt->i_codec == VLC_FOURCC( 'W', 'V', 'C', '1' ) )
            {
                fmt->i_codec = VLC_CODEC_VC1;
//                fmt->video.i_bits_per_pixel = 0x18; // No clue why this was set in smooth streamfilter
            }

            FillExtradata(fmt, extradata);
            break;

        case AUDIO_ES:
            fmt->audio.i_channels = formatex.nChannels;
            fmt->audio.i_rate = formatex.nSamplesPerSec;
            fmt->audio.i_bitspersample = formatex.wBitsPerSample;
            fmt->audio.i_blockalign = formatex.nBlockAlign;
            fmt->i_bitrate = formatex.nAvgBytesPerSec * 8; // FIXME (use bitrate) ?

            FillExtradata(fmt, extradata);
            break;

        case SPU_ES:
            break;

        default:
            break;
    }
}

void CodecParameters::setFourCC(const std::string &fcc)
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

void CodecParameters::fromWaveFormatEx(const std::vector<uint8_t> &data)
{
    if(data.size() >= sizeof(WAVEFORMATEX))
    {
        formatex.wFormatTag = GetWLE(&data[0]);
        wf_tag_to_fourcc(formatex.wFormatTag, &fourcc, nullptr);
        formatex.nChannels = GetWLE(&data[2]);
        formatex.nSamplesPerSec = GetDWLE(&data[4]);
        formatex.nAvgBytesPerSec = GetDWLE(&data[8]);
        formatex.nBlockAlign = GetWLE(&data[12]);
        formatex.wBitsPerSample = GetWLE(&data[14]);
        formatex.cbSize = GetWLE(&data[16]);
        if(data.size() > sizeof(WAVEFORMATEX))
        {
            formatex.cbSize = std::min(data.size() - sizeof(WAVEFORMATEX),
                                       (std::vector<uint8_t>::size_type) formatex.cbSize);
            if(formatex.cbSize)
            {
                extradata.resize(formatex.cbSize);
                memcpy(&extradata[0], &data[sizeof(WAVEFORMATEX)], formatex.cbSize);
            }
        }
        es_type = AUDIO_ES;
    }
}

void CodecParameters::fromVideoInfoHeader(const std::vector<uint8_t> &)
{
//    VIDEOINFOHEADER
    es_type = VIDEO_ES;
}
