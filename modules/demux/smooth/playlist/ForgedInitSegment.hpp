/*
 * ForgedInitSegment.hpp
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
#ifndef FORGEDINITSEGMENT_HPP
#define FORGEDINITSEGMENT_HPP

#include "../../adaptive/playlist/Segment.h"
#include "../../adaptive/playlist/Inheritables.hpp"

#include <vlc_es.h>
#include <vlc_codecs.h>

namespace smooth
{
    namespace playlist
    {
        using namespace adaptive;
        using namespace adaptive::playlist;
        using namespace adaptive::http;

        class ForgedInitSegment : public InitSegment,
                                  public TimescaleAble
        {
            public:
                ForgedInitSegment(ICanonicalUrl *parent, const std::string &,
                                  uint64_t, vlc_tick_t);
                virtual ~ForgedInitSegment();
                virtual SegmentChunk* toChunk(SharedResources *, AbstractConnectionManager *,
                                              size_t, BaseRepresentation *); /* reimpl */
                void setWaveFormatEx(const std::string &);
                void setCodecPrivateData(const std::string &);
                void setChannels(uint16_t);
                void setPacketSize(uint16_t);
                void setSamplingRate(uint32_t);
                void setBitsPerSample(uint16_t);
                void setVideoSize(unsigned w, unsigned h);
                void setFourCC(const std::string &);
                void setAudioTag(uint16_t);
                void setTrackID(unsigned);
                void setLanguage(const std::string &);

            private:
                void fromWaveFormatEx(const uint8_t *p_data, size_t i_data);
                void fromVideoInfoHeader(const uint8_t *p_data, size_t i_data);
                block_t * buildMoovBox();
                std::string data;
                std::string type;
                std::string language;
                uint8_t *extradata;
                size_t   i_extradata;
                WAVEFORMATEX formatex;
                unsigned width, height;
                vlc_fourcc_t fourcc;
                enum es_format_category_e es_type;
                unsigned track_id;
        };
    }
}

#endif // FORGEDINITSEGMENT_HPP
