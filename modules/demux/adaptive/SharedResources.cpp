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
#include "http/HTTPConnection.hpp"
#include "encryption/Keyring.hpp"

using namespace adaptive;

SharedResources::SharedResources(AuthStorage *auth, Keyring *ring,
                                 AbstractConnectionManager *conn)
{
    authStorage = auth;
    encryptionKeyring = ring;
    connManager = conn;
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

SharedResources * SharedResources::createDefault(vlc_object_t *obj,
                                                 const std::string & playlisturl)
{
    AuthStorage *auth = new AuthStorage(obj);
    Keyring *keyring = new Keyring(obj);
    HTTPConnectionManager *m = new HTTPConnectionManager(obj);
    if(!var_InheritBool(obj, "adaptive-use-access")) /* only use http from access */
        m->addFactory(new NativeConnectionFactory(auth));
    m->addFactory(new StreamUrlConnectionFactory());
    ConnectionParams params(playlisturl);
    if(params.isLocal())
        m->setLocalConnectionsAllowed();
    return new SharedResources(auth, keyring, m);
}
