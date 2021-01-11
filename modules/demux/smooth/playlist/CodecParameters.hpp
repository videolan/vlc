/*
 * CodecParameters.hpp
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
#ifndef CODECPARAMETERS_HPP
#define CODECPARAMETERS_HPP

#include <vlc_es.h>
#include <vlc_codecs.h>

#include <string>
#include <vector>

namespace smooth
{
    namespace playlist
    {
        class CodecParameters
        {
            public:
                CodecParameters();
                ~CodecParameters();
                void setFourCC(const std::string &);
                void setWaveFormatEx(const std::string &);
                void setCodecPrivateData(const std::string &);
                void setChannels(uint16_t);
                void setPacketSize(uint16_t);
                void setSamplingRate(uint32_t);
                void setBitsPerSample(uint16_t);
                void setAudioTag(uint16_t);
                void initAndFillEsFmt(es_format_t *) const;

                std::vector<uint8_t> extradata;
                WAVEFORMATEX formatex;
                vlc_fourcc_t fourcc;
                enum es_format_category_e es_type;

            private:
                void fromWaveFormatEx(const std::vector<uint8_t> &);
                void fromVideoInfoHeader(const std::vector<uint8_t> &);
        };
    }
}

#endif // CODECPARAMETERS_HPP
