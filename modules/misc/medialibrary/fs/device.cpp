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
#include <vlc_common.h>
#include <vlc_url.h>

#include <algorithm>
#include <cassert>
#include <strings.h>

namespace vlc {
  namespace medialibrary {

SDDevice::SDDevice( const std::string& uuid, std::string mrl )
    : m_uuid(uuid)
{
    // Ensure the mountpoint always ends with a '/' to avoid mismatch between
    // smb://foo and smb://foo/
    if ( *mrl.crbegin() != '/' )
        mrl += '/';
    m_mountpoints.push_back( std::move( mrl ) );
}

const std::string &
SDDevice::uuid() const
{
    return m_uuid;
}

bool
SDDevice::isRemovable() const
{
    return true;
}

bool
SDDevice::isPresent() const
{
    return m_mountpoints.empty() == false;
}

const
std::string &SDDevice::mountpoint() const
{
    return m_mountpoints[0];
}

void SDDevice::addMountpoint( std::string mrl )
{
    m_mountpoints.push_back( std::move( mrl ) );
}

void SDDevice::removeMountpoint( const std::string& mrl )
{
    auto it = std::find( begin( m_mountpoints ), end( m_mountpoints ), mrl );
    if ( it != end( m_mountpoints ) )
        m_mountpoints.erase( it );
}

std::tuple<bool, std::string>
SDDevice::matchesMountpoint( const std::string& mrl ) const
{
    vlc_url_t probedUrl;
    vlc_UrlParse( &probedUrl, mrl.c_str() );

    for ( const auto& m : m_mountpoints )
    {
        vlc_url_t url;
        vlc_UrlParse( &url, m.c_str() );
        if ( strcasecmp( probedUrl.psz_protocol, url.psz_protocol ) )
        {
            vlc_UrlClean( &url );
            continue;
        }
        if ( strcasecmp( probedUrl.psz_host, url.psz_host ) )
        {
            vlc_UrlClean( &url );
            continue;
        }
        if ( probedUrl.i_port != url.i_port )
        {
            unsigned int defaultPort = 0;
            if ( !strcasecmp( probedUrl.psz_protocol, "smb" ) )
                defaultPort = 445;
            if ( defaultPort != 0 )
            {
                if ( probedUrl.i_port != 0 && probedUrl.i_port != defaultPort &&
                     url.i_port != 0 && url.i_port != defaultPort )
                {
                    vlc_UrlClean( &url );
                    continue;
                }
                else
                {
                    url.i_port = probedUrl.i_port;
                    char* tmpUrl_psz = vlc_uri_compose(&url);
                    vlc_UrlClean( &url );
                    if (!tmpUrl_psz)
                        continue;
                    std::string tmpUrl(tmpUrl_psz);
                    free(tmpUrl_psz);
                    return std::make_tuple( true, tmpUrl );
                }
            }
            else
            {
                vlc_UrlClean( &url );
                continue;
            }
        }
        vlc_UrlClean( &url );
        return std::make_tuple( true, m );
    }
    vlc_UrlClean( &probedUrl );
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
    return m_mountpoints[0] + relativeMrl;
}

  } /* namespace medialibrary */
} /* namespace vlc */
