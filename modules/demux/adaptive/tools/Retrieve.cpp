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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Retrieve.hpp"

#include "../http/HTTPConnectionManager.h"
#include "../http/HTTPConnection.hpp"
#include "../http/Chunk.h"

using namespace adaptive;
using namespace adaptive::http;

block_t * Retrieve::HTTP(vlc_object_t *obj, AuthStorage *auth, const std::string &uri)
{
    HTTPConnectionManager connManager(obj, auth);
    HTTPChunk *datachunk;
    try
    {
        datachunk = new HTTPChunk(uri, &connManager, ID());
    } catch (int) {
        return NULL;
    }

    block_t *block = datachunk->read(1<<21);
    delete datachunk;
    return block;
}
