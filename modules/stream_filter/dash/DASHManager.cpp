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

#include "DASHManager.h"

using namespace dash;
using namespace dash::http;
using namespace dash::xml;
using namespace dash::logic;
using namespace dash::mpd;
using namespace dash::buffer;

DASHManager::DASHManager    ( MPD *mpd,
                              IAdaptationLogic::LogicType type, stream_t *stream) :
             conManager     ( NULL ),
             currentChunk   ( NULL ),
             adaptationLogic( NULL ),
             logicType      ( type ),
             mpdManager     ( NULL ),
             mpd            ( mpd ),
             stream         ( stream ),
             downloader     ( NULL ),
             buffer         ( NULL )
{
}
DASHManager::~DASHManager   ()
{
    delete this->downloader;
    delete this->buffer;
    delete this->conManager;
    delete this->adaptationLogic;
    delete this->mpdManager;
}

bool    DASHManager::start()
{
    this->mpdManager = mpd::MPDManagerFactory::create( mpd );

    if ( this->mpdManager == NULL )
        return false;

    this->adaptationLogic = AdaptationLogicFactory::create( this->logicType, this->mpdManager, this->stream);

    if ( this->adaptationLogic == NULL )
        return false;

    this->conManager = new dash::http::HTTPConnectionManager(this->adaptationLogic, this->stream);
    this->buffer     = new BlockBuffer(this->stream);
    this->downloader = new DASHDownloader(this->conManager, this->buffer);

    this->conManager->attach(this->adaptationLogic);
    this->buffer->attach(this->adaptationLogic);

    return this->downloader->start();
}
int     DASHManager::read( void *p_buffer, size_t len )
{
    return this->buffer->get(p_buffer, len);
}

int     DASHManager::seekBackwards( unsigned i_len )
{
    return this->buffer->seekBackwards( i_len );
}

int     DASHManager::peek( const uint8_t **pp_peek, size_t i_peek )
{
    return this->buffer->peek(pp_peek, i_peek);
}

const mpd::IMPDManager*         DASHManager::getMpdManager() const
{
    return this->mpdManager;
}

const logic::IAdaptationLogic*  DASHManager::getAdaptionLogic() const
{
    return this->adaptationLogic;
}

const Chunk *DASHManager::getCurrentChunk() const
{
    return this->currentChunk;
}
