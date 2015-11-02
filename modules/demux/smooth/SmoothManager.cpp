/*
 * SmoothManager.cpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC authors
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
#include "SmoothManager.hpp"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../adaptative/logic/RateBasedAdaptationLogic.h"
#include "../adaptative/tools/Retrieve.hpp"
#include "playlist/Parser.hpp"
#include "../adaptative/xml/DOMParser.h"
#include <vlc_stream.h>
#include <vlc_demux.h>
#include <vlc_charset.h>
#include <time.h>

using namespace adaptative;
using namespace adaptative::logic;
using namespace smooth;
using namespace smooth::playlist;

SmoothManager::SmoothManager(demux_t *demux_, Manifest *playlist,
                       AbstractStreamFactory *factory,
                       AbstractAdaptationLogic::LogicType type) :
             PlaylistManager(demux_, playlist, factory, type)
{
}

SmoothManager::~SmoothManager()
{
}

Manifest * SmoothManager::fetchManifest()
{
    std::string playlisturl(p_demux->psz_access);
    playlisturl.append("://");
    playlisturl.append(p_demux->psz_location);

    block_t *p_block = Retrieve::HTTP(VLC_OBJECT(p_demux), playlisturl);
    if(!p_block)
        return NULL;

    stream_t *memorystream = stream_MemoryNew(p_demux, p_block->p_buffer, p_block->i_buffer, true);
    if(!memorystream)
    {
        block_Release(p_block);
        return NULL;
    }

    xml::DOMParser parser(memorystream);
    if(!parser.parse())
    {
        stream_Delete(memorystream);
        block_Release(p_block);
        return NULL;
    }

    Manifest *manifest = NULL;

    ManifestParser *manifestParser = new (std::nothrow) ManifestParser(parser.getRootNode(), memorystream, playlisturl);
    if(manifestParser)
    {
        manifest = manifestParser->parse();
        delete manifestParser;
    }

    stream_Delete(memorystream);
    block_Release(p_block);

    return manifest;
}

bool SmoothManager::updatePlaylist()
{
    /* FIXME: do update from manifest after resuming from pause */
    if(!playlist->isLive() || !playlist->minUpdatePeriod.Get())
        return true;

    time_t now = time(NULL);

    if(nextPlaylistupdate && now < nextPlaylistupdate)
        return true;

    mtime_t mininterval = 0;
    mtime_t maxinterval = 0;

    std::vector<AbstractStream *>::iterator it;
    for(it=streams.begin(); it!=streams.end(); it++)
        (*it)->prune();

    /* Timelines updates should be inlined in tfrf atoms.
       We'll just care about pruning live timeline then. */
#if 0
    if(nextPlaylistupdate)
    {
        Manifest *newManifest = fetchManifest();
        if(newManifest)
        {
            newManifest->getPlaylistDurationsRange(&mininterval, &maxinterval);
            playlist->mergeWith(newManifest, 0);
            delete newManifest;

            std::vector<AbstractStream *>::iterator it;
            for(it=streams.begin(); it!=streams.end(); it++)
                (*it)->prune();
#ifdef NDEBUG
            playlist->debug();
#endif
        }
    }
#endif

    /* Compute new Manifest update time */
    if(!mininterval && !maxinterval)
        playlist->getPlaylistDurationsRange(&mininterval, &maxinterval);

    if(playlist->minUpdatePeriod.Get() > mininterval)
        mininterval = playlist->minUpdatePeriod.Get();

    if(mininterval < 5 * CLOCK_FREQ)
        mininterval = 5 * CLOCK_FREQ;

    if(maxinterval < mininterval)
        maxinterval = mininterval;

    nextPlaylistupdate = now + (mininterval + (maxinterval - mininterval) / 2) / CLOCK_FREQ;

//    msg_Dbg(p_demux, "Updated Manifest, next update in %" PRId64 "s (%" PRId64 "..%" PRId64 ")",
//            nextPlaylistupdate - now, mininterval/ CLOCK_FREQ, maxinterval/ CLOCK_FREQ );

    return true;
}

bool SmoothManager::isSmoothStreaming(stream_t *stream)
{
    const uint8_t *peek;
    int i_size = stream_Peek( stream, &peek, 512 );
    if( i_size < 512 )
        return false;

    char *peeked = (char*) malloc( 512 );
    if( unlikely( peeked == NULL ) )
        return false;

    memcpy( peeked, peek, 512 );
    peeked[511] = peeked[510] = '\0';

    char *str;

    if( !memcmp( peeked, "\xFF\xFE", 2 ) )
    {
        str = FromCharset( "UTF-16LE", peeked, 512 );
        free( peeked );
    }
    else if( !memcmp( peeked, "\xFE\xFF", 2 ) )
    {
        str = FromCharset( "UTF-16BE", peeked, 512 );
        free( peeked );
    }
    else
        str = peeked;

    if( str == NULL )
        return false;

    bool ret = strstr( str, "<SmoothStreamingMedia" ) != NULL;
    free( str );
    return ret;
}

AbstractAdaptationLogic *SmoothManager::createLogic(AbstractAdaptationLogic::LogicType type,
                                                    HTTPConnectionManager *conn)
{
    switch(type)
    {
        case AbstractAdaptationLogic::FixedRate:
        {
            size_t bps = var_InheritInteger(p_demux, "adaptative-bw") * 8192;
            return new (std::nothrow) FixedRateAdaptationLogic(bps);
        }
        case AbstractAdaptationLogic::Default:
        case AbstractAdaptationLogic::RateBased:
        {
            int width = var_InheritInteger(p_demux, "adaptative-width");
            int height = var_InheritInteger(p_demux, "adaptative-height");
            return new (std::nothrow) RateBasedAdaptationLogic(width, height);
        }
        default:
            return PlaylistManager::createLogic(type, conn);
    }
}
