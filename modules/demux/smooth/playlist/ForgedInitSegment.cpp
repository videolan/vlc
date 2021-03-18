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
#include "QualityLevel.hpp"
#include "CodecParameters.hpp"

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
    InitSegment(parent)
{
    type = type_;
    duration.Set(duration_);
    timescale = timescale_;
    width = height = 0;
    track_id = 1;
}

ForgedInitSegment::~ForgedInitSegment()
{
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

void ForgedInitSegment::setLanguage(const std::string &lang)
{
    language = lang;
}

block_t * ForgedInitSegment::buildMoovBox(const CodecParameters &codecparameters)
{
    es_format_t fmt;
    codecparameters.initAndFillEsFmt(&fmt);
    if(fmt.i_cat == VIDEO_ES)
    {
        fmt.video.i_width = width;
        fmt.video.i_height = height;
        fmt.video.i_visible_width = width;
        fmt.video.i_visible_height = height;
    }

    if(!language.empty())
        fmt.psz_language = strdup(language.c_str());

    bo_t *box = nullptr;
    mp4mux_handle_t *muxh = mp4mux_New(FRAGMENTED);
    if(muxh)
    {
        if(mp4mux_CanMux(nullptr, &fmt, VLC_FOURCC('s', 'm', 'o', 'o'), true ))
        {
            mp4mux_trackinfo_t *p_track = mp4mux_track_Add(muxh,
                                     0x01, /* Will always be 1st and unique track; tfhd patched on block read */
                                     &fmt, (uint32_t) timescale);
            if(p_track)
                mp4mux_track_ForceDuration(p_track, duration.Get());
        }

        box = mp4mux_GetMoov(muxh, nullptr, timescale.ToTime(duration.Get()));
    }
    es_format_Clean(&fmt);

    if(!box)
    {
        mp4mux_Delete(muxh);
        return nullptr;
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
    QualityLevel *lvl = dynamic_cast<QualityLevel *>(rep);
    if(lvl == nullptr)
        return nullptr;

    block_t *moov = buildMoovBox(lvl->getCodecParameters());
    if(moov)
    {
        MemoryChunkSource *source = new (std::nothrow) MemoryChunkSource(ChunkType::Init, moov);
        if( source )
        {
            SegmentChunk *chunk = new (std::nothrow) SegmentChunk(source, rep);
            if( chunk )
                return chunk;
            else
                delete source;
        }
    }
    return nullptr;
}
