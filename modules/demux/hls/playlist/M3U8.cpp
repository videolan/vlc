/*
 * M3U8.cpp
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

#include "M3U8.hpp"
#include "Representation.hpp"
#include "../adaptive/playlist/BasePeriod.h"
#include "../adaptive/playlist/BaseAdaptationSet.h"
#include "../adaptive/tools/Retrieve.hpp"

#include <vlc_common.h>
#include <vlc_stream.h>
#include <vlc_block.h>

using namespace hls::playlist;

M3U8::M3U8 (vlc_object_t *p_object, AuthStorage *auth_) :
    AbstractPlaylist(p_object)
{
    auth = auth_;
    minUpdatePeriod.Set( 5 * CLOCK_FREQ );
    vlc_mutex_init(&keystore_lock);
}

M3U8::~M3U8()
{
    vlc_mutex_destroy(&keystore_lock);
}

std::vector<uint8_t> M3U8::getEncryptionKey(const std::string &uri)
{
    std::vector<uint8_t> key;

    vlc_mutex_lock( &keystore_lock );
    std::map<std::string, std::vector<uint8_t> >::iterator it = keystore.find(uri);
    if(it == keystore.end())
    {
        /* Pretty bad inside the lock */
        block_t *p_block = Retrieve::HTTP(p_object, auth, uri);
        if(p_block)
        {
            if(p_block->i_buffer == 16)
            {
                key.resize(16);
                memcpy(&key[0], p_block->p_buffer, 16);
                keystore.insert(std::pair<std::string, std::vector<uint8_t> >(uri, key));
            }
            block_Release(p_block);
        }
    }
    else
    {
        key = (*it).second;
    }
    vlc_mutex_unlock(&keystore_lock);

    return key;
}

bool M3U8::isLive() const
{
    bool b_live = false;
    std::vector<BasePeriod *>::const_iterator itp;
    for(itp = periods.begin(); itp != periods.end(); ++itp)
    {
        const BasePeriod *period = *itp;
        std::vector<BaseAdaptationSet *>::const_iterator ita;
        for(ita = period->getAdaptationSets().begin(); ita != period->getAdaptationSets().end(); ++ita)
        {
            BaseAdaptationSet *adaptSet = *ita;
            std::vector<BaseRepresentation *>::iterator itr;
            for(itr = adaptSet->getRepresentations().begin(); itr != adaptSet->getRepresentations().end(); ++itr)
            {
                const Representation *rep = dynamic_cast<const Representation *>(*itr);
                if(rep->initialized())
                {
                    if(rep->isLive())
                        b_live = true;
                    else
                        return false; /* Any non live has higher priority */
                }
            }
        }
    }

    return b_live;
}

AuthStorage * M3U8::getAuth()
{
    return auth;
}

void M3U8::debug()
{
    std::vector<BasePeriod *>::const_iterator i;
    for(i = periods.begin(); i != periods.end(); ++i)
        (*i)->debug(VLC_OBJECT(p_object));
}

