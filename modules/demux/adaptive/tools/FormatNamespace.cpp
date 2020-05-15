/*
 * FormatNamespace.cpp
 *****************************************************************************
 * Copyright © 2019 VideoLabs, VideoLAN and VLC Authors
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

#include "FormatNamespace.hpp"
#include "../../mp4/mpeg4.h"
#include "../../../packetizer/mpeg4audio.h"
#include "../tools/Helper.h"

#include <list>
#include <string>
#include <ios>

using namespace adaptive;

#define MP4RA(fcc) VLC_FOURCC(fcc[0], fcc[1], fcc[2], fcc[3])
#define MSFCC(fcc) MP4RA(fcc)

FormatNamespace::FormatNamespace(const std::string &codec)
{
    es_format_Init(&fmt, UNKNOWN_ES, 0);
    ParseString(codec);
}

FormatNamespace::~FormatNamespace()
{
    es_format_Clean(&fmt);
}

const es_format_t * FormatNamespace::getFmt() const
{
    return &fmt;
}

void FormatNamespace::ParseMPEG4Elements(const std::vector<std::string> &elements)
{
    /* As described in RFC 6381 3.3 */
    if(elements.size() < 1)
        return;

    uint8_t objectType = std::stoi(elements.at(0).substr(0,1), nullptr, 16);
    if(!MPEG4_Codec_By_ObjectType(objectType, NULL, 0,
                                  &fmt.i_codec,
                                  &fmt.i_profile))
        return;

    switch(objectType)
    {
        case 0x40:
            if(elements.size() > 1)
                fmt.i_profile = std::stoi(elements.at(1).substr(0,1), nullptr, 16);
            break;
        default:
            break;
    }
}

void FormatNamespace::ParseString(const std::string &codecstring)
{
    std::list<std::string> tokens = Helper::tokenize(codecstring, '.');
    if(tokens.empty())
        return;

    std::string fourcc = tokens.front();
    if(fourcc.size() != 4)
        return;

    tokens.pop_front();
    std::vector<std::string> elements;
    elements.assign(tokens.begin(), tokens.end());

    Parse(MP4RA(fourcc), elements);
}

void FormatNamespace::Parse(vlc_fourcc_t fcc, const std::vector<std::string> &elements)
{
    switch(fcc)
    {
        /* VIDEO */
        case MP4RA("mp4v"):
            /* set default if no elements */
            es_format_Change(&fmt, AUDIO_ES, VLC_CODEC_MP4V);
            ParseMPEG4Elements(elements);
            break;
        case MP4RA("avc1"):
        case MP4RA("avc2"):
        case MP4RA("avc3"):
        case MP4RA("avc4"):
        case MP4RA("svc1"):
        case MP4RA("mvc1"):
        case MP4RA("mvc2"):
            es_format_Change(&fmt, VIDEO_ES, VLC_FOURCC('a','v','c','1'));
            if(elements.size() > 0 && elements.at(0).size() == 6)
            {
                const std::string &sixbytes = elements.at(0);
                fmt.i_profile = std::stoi(sixbytes.substr(0,2), nullptr, 16);
                fmt.i_level = std::stoi(sixbytes.substr(2,2), nullptr, 16);
            }
            break;
        case MP4RA("hev1"):
        case MP4RA("hev2"):
        case MP4RA("hevc"):
        case MP4RA("hvc1"):
        case MP4RA("hvc2"):
        case MP4RA("hvt1"):
        case MP4RA("lhv1"):
        case MP4RA("lhe1"):
        case MP4RA("dvhe"): /* DolbyVision */
        //case MP4RA("dvh1"): /* DolbyVision (Collides with DV) */
            es_format_Change(&fmt, VIDEO_ES, VLC_CODEC_HEVC);
            break;
        case MP4RA("av01"):
            es_format_Change(&fmt, VIDEO_ES, VLC_CODEC_AV1);
            if(elements.size() > 1)
            {
                fmt.i_profile = std::stoi(elements.at(0), nullptr, 16);
                fmt.i_level = std::stoi(elements.at(1), nullptr, 16);
            }
            break;
        case MP4RA("vp09"):
        case MP4RA("vp08"):
            es_format_Change(&fmt, VIDEO_ES,
                             vlc_fourcc_GetCodec(VIDEO_ES, fcc == MP4RA("vp09")
                                                           ? VLC_CODEC_VP9
                                                           : VLC_CODEC_VP8));
            if(elements.size() > 1)
            {
                fmt.i_profile = std::stoi(elements.at(0), nullptr, 16);
                fmt.i_level = std::stoi(elements.at(1), nullptr, 16);
            }
            break;
        case MSFCC("AVC1"):
        case MSFCC("AVCB"):
        case MSFCC("H264"):
            es_format_Change(&fmt, VIDEO_ES, VLC_FOURCC('a','v','c','1'));
            break;
        case MSFCC("WVC1"):
            es_format_Change(&fmt, VIDEO_ES, VLC_CODEC_VC1);
            break;
        /* AUDIO */
        case MP4RA("mp4a"):
            /* set default if no elements */
            es_format_Change(&fmt, AUDIO_ES, VLC_CODEC_MP4A);
            ParseMPEG4Elements(elements);
            break;
        case MP4RA("dtsh"):
        case MP4RA("ac-3"):
        case MP4RA("ec-3"):
        case MP4RA("ac-4"):
        case MP4RA("opus"):
            es_format_Change(&fmt, AUDIO_ES, vlc_fourcc_GetCodec(AUDIO_ES, fcc));
            break;
        case MSFCC("AACL"):
            es_format_Change(&fmt, AUDIO_ES, VLC_CODEC_MP4A);
            fmt.i_profile = AAC_PROFILE_LC;
            break;
        case MSFCC("WMAP"):
            es_format_Change(&fmt, AUDIO_ES, VLC_CODEC_WMAP);
            break;
        /* SUBTITLES */
        case MP4RA("stpp"):
        case MSFCC("TTML"):
            es_format_Change(&fmt, SPU_ES, VLC_CODEC_TTML);
            break;
        case MP4RA("wvtt"):
            es_format_Change(&fmt, SPU_ES, VLC_CODEC_WEBVTT);
            break;
        default: /* dumb and probably broken lookup as fcc ~~ mp4ra */
//            {
//                const enum es_format_category_e cats[] = { VIDEO_ES, AUDIO_ES, SPU_ES };
//                for(size_t i=0; i < ARRAY_SIZE(cats); i++)
//                {
//                    vlc_fourcc_t rfcc = vlc_fourcc_GetCodec(cats[i], fcc);
//                    if(rfcc)
//                    {
//                        printf("%4.4s --> %4.4s %d\n", &rfcc, &fcc, cats[i]);
//                        es_format_Change(&fmt, cats[i], rfcc);
//                        break;
//                    }
//                }
//            }
            break;
    }
}
