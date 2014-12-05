/*
 * DASHDownloader.h
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
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

#include "DASHDownloader.h"

using namespace dash;
using namespace dash::http;
using namespace dash::logic;
using namespace dash::buffer;


DASHDownloader::DASHDownloader  (HTTPConnectionManager *conManager_, BlockBuffer *buffer_)
{
    conManager = conManager_;
    buffer = buffer_;
}
DASHDownloader::~DASHDownloader ()
{
    buffer->setEOF(true);
    vlc_join(this->dashDLThread, NULL);
}

bool        DASHDownloader::start       ()
{
    if(vlc_clone(&(this->dashDLThread), download, static_cast<void*>(this), VLC_THREAD_PRIORITY_LOW))
        return false;

    return true;
}
void* DASHDownloader::download(void *dashDownloader)
{
    DASHDownloader *me = static_cast<DASHDownloader*>(dashDownloader);
    int ret = 0;

    do
    {
        block_t *block = NULL;
        ret = me->conManager->read(Streams::VIDEO, &block);
        if(ret > 0)
            me->buffer->put(block);
    }while(ret > 0 && !me->buffer->getEOF());

    me->buffer->setEOF(true);

    return NULL;
}
