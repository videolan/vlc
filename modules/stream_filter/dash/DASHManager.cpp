/*
 * DASHManager.cpp
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

#include "DASHManager.h"

using namespace dash;
using namespace dash::http;
using namespace dash::xml;
using namespace dash::logic;
using namespace dash::mpd;
using namespace dash::exception;

DASHManager::DASHManager    (HTTPConnectionManager *conManager, Node *node, IAdaptationLogic::LogicType type, Profile profile)
{
    this->conManager        = conManager;
    this->node              = node;
    this->logicType         = type;
    this->profile           = profile;
    this->mpdManagerFactory = new MPDManagerFactory();
    this->mpdManager        = this->mpdManagerFactory->create(this->profile, this->node);
    this->logicFactory      = new AdaptationLogicFactory();
    this->adaptationLogic   = this->logicFactory->create(this->logicType, this->mpdManager);
    this->currentChunk      = NULL;

    this->conManager->attach(this->adaptationLogic);
}
DASHManager::~DASHManager   ()
{
    delete(this->logicFactory);
    delete(this->adaptationLogic);
    delete(this->mpdManager);
}

int DASHManager::read   (void *p_buffer, size_t len)
{
    if(this->currentChunk == NULL)
    {
        try
        {
            this->currentChunk = this->adaptationLogic->getNextChunk();
        }
        catch(EOFException &e)
        {
            this->currentChunk = NULL;
            return 0;
        }
    }

    int ret = this->conManager->read(this->currentChunk, p_buffer, len);

    if(ret <= 0)
    {
        this->currentChunk = NULL;
        return this->read(p_buffer, len);
    }

    return ret;
}
int DASHManager::peek   (const uint8_t **pp_peek, size_t i_peek)
{
    if(this->currentChunk == NULL)
    {
        try
        {
            this->currentChunk = this->adaptationLogic->getNextChunk();
        }
        catch(EOFException &e)
        {
            return 0;
        }
    }

    int ret = this->conManager->peek(this->currentChunk, pp_peek, i_peek);
    return ret;
}
