/*
 * SharedResources.cpp
 *****************************************************************************
 * Copyright © 2019 VideoLabs, VideoLAN and VLC Authors
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

#include "SharedResources.hpp"
#include "http/AuthStorage.hpp"
#include "http/HTTPConnectionManager.h"
#include "encryption/Keyring.hpp"

#include <vlc_common.h>

using namespace adaptive;

SharedResources::SharedResources(vlc_object_t *obj, bool local)
{
    authStorage = new AuthStorage(obj);
    encryptionKeyring = new Keyring(obj);
    HTTPConnectionManager *m = new HTTPConnectionManager(obj, authStorage);
    if(m && local)
        m->setLocalConnectionsAllowed();
    connManager = m;
}

SharedResources::~SharedResources()
{
    delete connManager;
    delete encryptionKeyring;
    delete authStorage;
}

AuthStorage * SharedResources::getAuthStorage()
{
    return authStorage;
}

Keyring * SharedResources::getKeyring()
{
    return encryptionKeyring;
}

AbstractConnectionManager * SharedResources::getConnManager()
{
    return connManager;
}
