/*****************************************************************************
 * device.cpp: Media library network device
 *****************************************************************************
 * Copyright (C) 2018 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "device.h"

#include <algorithm>
#include <cassert>
#include <strings.h>

namespace vlc {
  namespace medialibrary {

SDDevice::SDDevice(const std::string& uuid, std::string scheme, bool removable, bool isNetwork)
    : m_uuid(uuid)
    , m_scheme( std::move( scheme ) )
    , m_removable(removable)
    , m_isNetwork(isNetwork)
{
}

const std::string &
SDDevice::uuid() const
{
    return m_uuid;
}

bool
SDDevice::isRemovable() const
{
    return m_removable;
}

bool
SDDevice::isPresent() const
{
    return m_mountpoints.empty() == false;
}

bool SDDevice::isNetwork() const
{
    return m_isNetwork;
}

const
std::string &SDDevice::mountpoint() const
{
    return m_mountpoints[0].mrl;
}

void SDDevice::addMountpoint( std::string mrl )
{
    // Ensure the mountpoint always ends with a '/' to avoid mismatch between
    // smb://foo and smb://foo/
    if ( *mrl.crbegin() != '/' )
        mrl += '/';
    auto it = std::find_if( cbegin( m_mountpoints ), cend( m_mountpoints ),
                    [&mrl]( const Mountpoint& mp ) { return mp.mrl == mrl; } );
    if ( it != cend( m_mountpoints ) )
        return;

    try
    {
        auto mp = Mountpoint{ std::move( mrl ) };
        m_mountpoints.push_back( std::move( mp ) );
    }
    catch ( const vlc::url::invalid& )
    {
    }
}

void SDDevice::removeMountpoint( const std::string& mrl )
{
    auto it = std::find_if( begin( m_mountpoints ), end( m_mountpoints ),
                            [&mrl]( const Mountpoint& mp ) { return mp.mrl == mrl; } );
    if ( it != end( m_mountpoints ) )
        m_mountpoints.erase( it );
}

std::tuple<bool, std::string>
SDDevice::matchesMountpoint( const std::string& mrl ) const
{
    vlc::url probedUrl;
    try
    {
        probedUrl = vlc::url{ mrl };
    }
    catch ( const vlc::url::invalid& )
    {
        return std::make_tuple( false, "" );
    }

    for ( const auto& m : m_mountpoints )
    {
        if ( strcasecmp( probedUrl.psz_protocol, m.url.psz_protocol ) )
            continue;
        if ( strcasecmp( probedUrl.psz_host, m.url.psz_host ) )
            continue;
        /* Ignore path for plain network hosts, ie. without any path specified */
        if ( m.url.psz_path != nullptr && *m.url.psz_path != 0 &&
             probedUrl.psz_path != nullptr &&
             strncasecmp( m.url.psz_path, probedUrl.psz_path,
                          strlen( m.url.psz_path ) ) != 0 )
        {
            continue;
        }
        if ( probedUrl.i_port != m.url.i_port )
        {
            unsigned int defaultPort = 0;
            if ( !strcasecmp( probedUrl.psz_protocol, "smb" ) )
                defaultPort = 445;
            if ( defaultPort != 0 )
            {
                if ( probedUrl.i_port != 0 && probedUrl.i_port != defaultPort &&
                     m.url.i_port != 0 && m.url.i_port != defaultPort )
                {
                    continue;
                }
                vlc_url_t url = m.url;
                url.i_port = probedUrl.i_port;
                char* tmpUrl_psz = vlc_uri_compose(&url);
                if (!tmpUrl_psz)
                    continue;
                std::string tmpUrl(tmpUrl_psz);
                free(tmpUrl_psz);
                return std::make_tuple( true, tmpUrl );
            }
            continue;
        }
        return std::make_tuple( true, m.mrl );
    }
    return std::make_tuple( false, "" );
}

std::string SDDevice::relativeMrl( const std::string& absoluteMrl ) const
{
    auto match = matchesMountpoint( absoluteMrl );
    if ( std::get<0>( match ) == false )
        return absoluteMrl;
    const auto& mountpoint = std::get<1>( match );
    return absoluteMrl.substr( mountpoint.length() );
}

std::string SDDevice::absoluteMrl( const std::string& relativeMrl ) const
{
    assert( m_mountpoints.empty() == false );
    return m_mountpoints[0].mrl + relativeMrl;
}

const std::string& SDDevice::scheme() const
{
    return m_scheme;
}

  } /* namespace medialibrary */
} /* namespace vlc */
