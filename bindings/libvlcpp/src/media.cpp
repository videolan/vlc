/*****************************************************************************
 * media.cpp: Represent a media
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "media.hpp"
#include "exception.hpp"

using namespace libvlc;

Media::Media( libVLC &libvlcInstance, const char *psz_mrl )
{
    m_media = libvlc_media_new( libvlcInstance.m_instance, psz_mrl );
    if( !m_media )
        throw libvlc_errmsg();
}

Media::Media( const Media& original )
{
    m_media = libvlc_media_duplicate( original.m_media );
}

Media::~Media()
{
    libvlc_media_release( m_media );
}

void Media::addOption( const char *ppsz_options )
{
    libvlc_media_add_option( m_media, ppsz_options );
}

void Media::addOption( const char *ppsz_options, libvlc_media_option_t flag )
{
    libvlc_media_add_option_flag( m_media, ppsz_options, flag );
}

int64_t Media::duration()
{
    return libvlc_media_get_duration( m_media );
}

int Media::isPreparsed()
{
    return libvlc_media_is_preparsed( m_media );
}

char *Media::mrl()
{
    return libvlc_media_get_mrl( m_media );
}

char *Media::meta( libvlc_meta_t e_meta )
{
    return libvlc_media_get_meta( m_media, e_meta );
}

void Media::setMeta( libvlc_meta_t e_meta, const char *psz_value )
{
    libvlc_media_set_meta( m_media, e_meta, psz_value );
}

int Media::saveMeta()
{
    return libvlc_media_save_meta( m_media );
}

libvlc_state_t Media::state()
{
    return libvlc_media_get_state( m_media );
}

void Media::setUserData( void *p_user_data )
{
    libvlc_media_set_user_data( m_media, p_user_data );
}

void *Media::userData()
{
    return libvlc_media_get_user_data( m_media );
}

