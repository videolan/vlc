/*
 * RateBasedAdaptationLogic.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "RateBasedAdaptationLogic.h"

using namespace dash::logic;
using namespace dash::xml;
using namespace dash::http;
using namespace dash::mpd;
using namespace dash::exception;

RateBasedAdaptationLogic::RateBasedAdaptationLogic  (IMPDManager *mpdManager) :
    AbstractAdaptationLogic( mpdManager ),
    mpdManager( mpdManager ),
    count( 0 ),
    currentPeriod( mpdManager->getFirstPeriod() )
{
}

Chunk*  RateBasedAdaptationLogic::getNextChunk () throw(EOFException)
{
    if(this->mpdManager == NULL)
        throw EOFException();

    if(this->currentPeriod == NULL)
        throw EOFException();

    long bitrate = this->getBpsAvg();

    Representation *rep = this->mpdManager->getRepresentation(this->currentPeriod, bitrate);

    if(rep == NULL)
        throw EOFException();

    std::vector<Segment *> segments = this->mpdManager->getSegments(rep);

    if(this->count == segments.size())
    {
        this->currentPeriod = this->mpdManager->getNextPeriod(this->currentPeriod);
        this->count = 0;
        return this->getNextChunk();
    }

    if ( segments.size() > this->count )
    {
        Chunk *chunk = new Chunk;
        chunk->setUrl( segments.at( this->count )->getSourceUrl() );
        this->count++;
        return chunk;
    }
    return NULL;
}
