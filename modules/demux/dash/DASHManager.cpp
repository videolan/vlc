/*****************************************************************************
 * DASHManager.cpp
 *****************************************************************************
 * Copyright © 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
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

#include <cinttypes>

#include "DASHManager.h"
#include "mpd/ProgramInformation.h"
#include "mpd/IsoffMainParser.h"
#include "../adaptive/xml/DOMParser.h"
#include "../adaptive/xml/Node.h"
#include "../adaptive/SharedResources.hpp"
#include "../adaptive/tools/Helper.h"
#include "../adaptive/http/HTTPConnectionManager.h"
#include <vlc_stream.h>
#include <vlc_demux.h>
#include <vlc_meta.h>
#include <vlc_block.h>
#include "../adaptive/tools/Retrieve.hpp"

#include <algorithm>
#include <ctime>

using namespace dash;
using namespace dash::mpd;
using namespace adaptive::logic;

DASHManager::DASHManager(demux_t *demux_,
                         SharedResources *res,
                         MPD *mpd,
                         AbstractStreamFactory *factory,
                         AbstractAdaptationLogic::LogicType type) :
             PlaylistManager(demux_, res, mpd, factory, type)
{
}

DASHManager::~DASHManager   ()
{
}

void DASHManager::scheduleNextUpdate()
{
    time_t now = time(nullptr);

    vlc_tick_t minbuffer = getMinAheadTime() / 2;

    if(playlist->minUpdatePeriod > minbuffer)
        minbuffer = playlist->minUpdatePeriod;

    minbuffer = std::max(minbuffer, VLC_TICK_FROM_SEC(5));

    nextPlaylistupdate = now + SEC_FROM_VLC_TICK(minbuffer);

    msg_Dbg(p_demux, "Updated MPD, next update in %" PRId64 "s", (int64_t) nextPlaylistupdate - now );
}

bool DASHManager::needsUpdate() const
{
    if(nextPlaylistupdate && time(nullptr) < nextPlaylistupdate)
        return false;

    return PlaylistManager::needsUpdate();
}

bool DASHManager::updatePlaylist()
{
    /* do update */
    if(nextPlaylistupdate)
    {
        std::string url(p_demux->psz_url);

        block_t *p_block = Retrieve::HTTP(resources, ChunkType::Playlist, url);
        if(!p_block)
            return false;

        stream_t *mpdstream = vlc_stream_MemoryNew(p_demux, p_block->p_buffer, p_block->i_buffer, true);
        if(!mpdstream)
        {
            block_Release(p_block);
            return false;
        }

        xml::DOMParser parser(mpdstream);
        if(!parser.parse(true))
        {
            vlc_stream_Delete(mpdstream);
            block_Release(p_block);
            return false;
        }

        IsoffMainParser mpdparser(parser.getRootNode(), VLC_OBJECT(p_demux),
                                  mpdstream, Helper::getDirectoryPath(url).append("/"));
        MPD *newmpd = mpdparser.parse();
        if(newmpd)
        {
            playlist->updateWith(newmpd);
            delete newmpd;
        }
        vlc_stream_Delete(mpdstream);
        block_Release(p_block);
    }

    return true;
}

int DASHManager::doControl(int i_query, va_list args)
{
    switch (i_query)
    {
        case DEMUX_GET_META:
        {
            MPD *mpd = dynamic_cast<MPD *>(playlist);
            if(!mpd)
                return VLC_EGENERIC;

            if(!mpd->programInfo)
                break;

            vlc_meta_t *p_meta = va_arg (args, vlc_meta_t *);
            vlc_meta_t *meta = vlc_meta_New();
            if (meta == nullptr)
                return VLC_EGENERIC;

            if(!mpd->programInfo->getTitle().empty())
                vlc_meta_SetTitle(meta, mpd->programInfo->getTitle().c_str());

            if(!mpd->programInfo->getSource().empty())
                vlc_meta_SetPublisher(meta, mpd->programInfo->getSource().c_str());

            if(!mpd->programInfo->getCopyright().empty())
                vlc_meta_SetCopyright(meta, mpd->programInfo->getCopyright().c_str());

            if(!mpd->programInfo->getMoreInformationUrl().empty())
                vlc_meta_SetURL(meta, mpd->programInfo->getMoreInformationUrl().c_str());

            vlc_meta_Merge(p_meta, meta);
            vlc_meta_Delete(meta);
            break;
        }
    }
    return PlaylistManager::doControl(i_query, args);
}

bool DASHManager::isDASH(xml::Node *root)
{
    static const std::string namespaces[] = {
        "urn:mpeg:dash:schema:mpd:2011",
        "urn:mpeg:DASH:schema:MPD:2011",
        "urn:mpeg:schema:dash:mpd:2011",
        "urn:mpeg:mpegB:schema:DASH:MPD:DIS2011",
    };

    for( size_t i=0; i<ARRAY_SIZE(namespaces); i++ )
        if(root->matches("MPD", namespaces[i]))
            return true;

    return false;
}

bool DASHManager::mimeMatched(const std::string &mime)
{
    return (mime == "application/dash+xml");
}
