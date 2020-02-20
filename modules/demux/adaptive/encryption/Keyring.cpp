/*****************************************************************************
 * Keyring.cpp
 *****************************************************************************
 * Copyright (C) 2019 VideoLabs, VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#include "Keyring.hpp"
#include "../tools/Retrieve.hpp"

#include <vlc_block.h>

#include <algorithm>

using namespace adaptive::encryption;

Keyring::Keyring(vlc_object_t *obj_)
{
    obj = obj_;
    vlc_mutex_init(&lock);
}

Keyring::~Keyring()
{
}

KeyringKey Keyring::getKey(SharedResources *resources, const std::string &uri)
{
    KeyringKey key;

    vlc_mutex_lock(&lock);
    std::map<std::string, KeyringKey>::iterator it = keys.find(uri);
    if(it == keys.end())
    {
        /* Pretty bad inside the lock */
        msg_Dbg(obj, "Retrieving AES key %s", uri.c_str());
        block_t *p_block = Retrieve::HTTP(resources, uri);
        if(p_block)
        {
            if(p_block->i_buffer == 16)
            {
                key.resize(16);
                memcpy(&key[0], p_block->p_buffer, 16);
                keys.insert(std::pair<std::string, KeyringKey>(uri, key));
                lru.push_front(uri);
                if(lru.size() > Keyring::MAX_KEYS)
                {
                    keys.erase(keys.find(lru.back()));
                    lru.pop_back();
                }
            }
            block_Release(p_block);
        }
    }
    else
    {
        std::list<std::string>::iterator it2 = std::find(lru.begin(), lru.end(), uri);
        if(it2 != lru.begin())
        {
            lru.erase(it2);
            lru.push_front(uri);
        }
        key = (*it).second;
    }
    vlc_mutex_unlock(&lock);

    return key;
}
