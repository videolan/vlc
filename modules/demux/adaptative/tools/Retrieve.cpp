/*
 * Parser.cpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
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
#include "Retrieve.hpp"

#include "../http/HTTPConnectionManager.h"
#include "../http/HTTPConnection.hpp"
#include "../http/Chunk.h"

using namespace adaptative;
using namespace adaptative::http;

uint64_t Retrieve::HTTP(vlc_object_t *obj, const std::string &uri, void **pp_data)
{
    HTTPConnectionManager connManager(obj);
    Chunk *datachunk;
    try
    {
        datachunk = new Chunk(uri);
    } catch (int) {
        *pp_data = NULL;
        return 0;
    }

    if(!connManager.connectChunk(datachunk) ||
        datachunk->getConnection()->query(datachunk->getPath()) != VLC_SUCCESS ||
        datachunk->getBytesToRead() == 0 )
    {
        datachunk->getConnection()->releaseChunk();
        delete datachunk;
        *pp_data = NULL;
        return 0;
    }

    size_t i_data = datachunk->getBytesToRead();
    *pp_data = malloc(i_data);
    if(*pp_data)
    {
        ssize_t ret = datachunk->getConnection()->read(*pp_data, i_data);
        if(ret < 0)
        {
            free(*pp_data);
            *pp_data = NULL;
            i_data = 0;
        }
        else
        {
            i_data = ret;
        }
    }
    datachunk->getConnection()->releaseChunk();
    delete datachunk;
    return i_data;
}
