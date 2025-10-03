/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef VLCMEDIASOURCEWRAPPER_HPP
#define VLCMEDIASOURCEWRAPPER_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <vlc_media_source.h>
#include <vlc_cxx_helpers.hpp>

using MediaSourcePtr = ::vlc::vlc_shared_data_ptr<vlc_media_source_t,
                                                  &vlc_media_source_Hold,
                                                  &vlc_media_source_Release>;

using MediaTreePtr = ::vlc::vlc_shared_data_ptr<vlc_media_tree_t,
                                                &vlc_media_tree_Hold,
                                                &vlc_media_tree_Release>;

class MediaTreeLocker
{
public:
    MediaTreeLocker(MediaTreePtr& tree)
        : m_tree(tree)
    {
        vlc_media_tree_Lock(m_tree.get());
    }

    ~MediaTreeLocker() {
        vlc_media_tree_Unlock(m_tree.get());
    }

    MediaTreeLocker( const MediaTreeLocker& ) = delete;
    MediaTreeLocker( MediaTreeLocker&& ) = delete;

    MediaTreeLocker& operator=( const MediaTreeLocker& ) = delete;
    MediaTreeLocker& operator=( MediaTreeLocker&& ) = delete;

private:
    MediaTreePtr& m_tree;
};

#endif // VLCMEDIASOURCEWRAPPER_HPP
