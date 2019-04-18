/*****************************************************************************
 * HLSManager.cpp
 *****************************************************************************
 * Copyright © 2015 VideoLAN and VLC authors
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

#include "HLSManager.hpp"
#include "../adaptive/tools/Retrieve.hpp"
#include "../adaptive/http/HTTPConnectionManager.h"
#include "playlist/Parser.hpp"
#include <vlc_stream.h>
#include <vlc_demux.h>
#include <time.h>

using namespace adaptive;
using namespace adaptive::logic;
using namespace hls;
using namespace hls::playlist;

HLSManager::HLSManager(demux_t *demux_,
                       SharedResources *res,
                       M3U8 *playlist,
                       AbstractStreamFactory *factory,
                       AbstractAdaptationLogic::LogicType type) :
             PlaylistManager(demux_, res, playlist, factory, type)
{
}

HLSManager::~HLSManager()
{
}

bool HLSManager::isHTTPLiveStreaming(stream_t *s)
{
    const uint8_t *peek;

    int size = vlc_stream_Peek(s, &peek, 7);
    if (size < 7 || memcmp(peek, "#EXTM3U", 7))
        return false;

    size = vlc_stream_Peek(s, &peek, 8192);
    if (size < 7)
        return false;

    peek += 7;
    size -= 7;

    /* Parse stream and search for
     * EXT-X-TARGETDURATION or EXT-X-STREAM-INF tag, see
     * http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8 */
    while (size--)
    {
        static const char *const ext[] = {
            "TARGETDURATION",
            "MEDIA-SEQUENCE",
            "KEY",
            "ALLOW-CACHE",
            "ENDLIST",
            "STREAM-INF",
            "DISCONTINUITY",
            "VERSION"
        };

        if (*peek++ != '#')
            continue;

        if (size < 6)
            continue;

        if (memcmp(peek, "EXT-X-", 6))
            continue;

        peek += 6;
        size -= 6;

        for (size_t i = 0; i < ARRAY_SIZE(ext); i++)
        {
            size_t len = strlen(ext[i]);
            if (size < 0 || (size_t)size < len)
                continue;
            if (!memcmp(peek, ext[i], len))
                return true;
        }
    }

    return false;
}

vlc_tick_t HLSManager::getFirstPlaybackTime() const
{
    return demux.i_firstpcr;
}
