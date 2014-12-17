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

#define __STDC_CONSTANT_MACROS

#include "DASHManager.h"
#include "adaptationlogic/AdaptationLogicFactory.h"

using namespace dash;
using namespace dash::http;
using namespace dash::logic;
using namespace dash::mpd;
using namespace dash::buffer;

DASHManager::DASHManager    ( MPD *mpd,
                              IAdaptationLogic::LogicType type, stream_t *stream) :
             conManager     ( NULL ),
             logicType      ( type ),
             mpd            ( mpd ),
             stream         ( stream )
{
    for(int i=0; i<Streams::count; i++)
        streams[i] = NULL;
}

DASHManager::~DASHManager   ()
{
    delete conManager;
    for(int i=0; i<Streams::count; i++)
        delete streams[i];
}

bool DASHManager::start(demux_t *demux)
{
    const Period *period = mpd->getFirstPeriod();
    if(!period)
        return false;

    for(int i=0; i<Streams::count; i++)
    {
        Streams::Type type = static_cast<Streams::Type>(i);
        const AdaptationSet *set = period->getAdaptationSet(type);
        if(set)
        {
            streams[type] = new Streams::Stream(set->getMimeType());
            try
            {
                streams[type]->create(demux, AdaptationLogicFactory::create( logicType, mpd ) );
            } catch (int) {
                delete streams[type];
                streams[type] = NULL;
            }
        }
    }

    conManager = new HTTPConnectionManager(stream);
    if(!conManager)
        return false;

    return true;
}

size_t DASHManager::read()
{
    size_t i_ret = 0;
    for(int type=0; type<Streams::count; type++)
    {
        if(!streams[type])
            continue;
        i_ret += streams[type]->read(conManager);
    }
    return i_ret;
}

mtime_t DASHManager::getPCR() const
{
    mtime_t pcr = VLC_TS_INVALID;
    for(int type=0; type<Streams::count; type++)
    {
        if(!streams[type])
            continue;
        if(pcr == VLC_TS_INVALID || pcr > streams[type]->getPCR())
            pcr = streams[type]->getPCR();
    }
    return pcr;
}

int DASHManager::getGroup() const
{
    for(int type=0; type<Streams::count; type++)
    {
        if(!streams[type])
            continue;
        return streams[type]->getGroup();
    }
    return -1;
}

int DASHManager::esCount() const
{
    int es = 0;
    for(int type=0; type<Streams::count; type++)
    {
        if(!streams[type])
            continue;
        es += streams[type]->esCount();
    }
    return es;
}

mtime_t DASHManager::getDuration() const
{
    if (mpd->isLive())
        return 0;
    else
        return CLOCK_FREQ * mpd->getDuration();
}
