/*****************************************************************************
 * file.cpp: Media library network file
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

#include "file.h"
#include "util.h"

namespace vlc {
  namespace medialibrary {

SDFile::SDFile( const std::string mrl, const int64_t size, const time_t lastModificationDate )
    : m_mrl( std::move( mrl ) )
    , m_name( utils::fileName( m_mrl ) )
    , m_extension( utils::extension( m_mrl ) )
    , m_isNetwork( m_mrl.find( "file://" ) != 0 )
    , m_size( size )
    , m_lastModificationTime( lastModificationDate )
{
}
SDFile::SDFile( const std::string mrl,
                const LinkedFileType fType,
                const std::string linkedFile,
                const int64_t size,
                const time_t lastModificationDate )
    : m_mrl( std::move( mrl ) )
    , m_name( utils::fileName( m_mrl ) )
    , m_extension( utils::extension( m_mrl ) )
    , m_linkedFile( std::move( linkedFile ) )
    , m_linkedType( fType )
    , m_isNetwork( m_mrl.find( "file://" ) != 0 )
    , m_size( size )
    , m_lastModificationTime( lastModificationDate )
{
}

const std::string &
SDFile::mrl() const
{
    return m_mrl;
}

const std::string &
SDFile::name() const
{
    return m_name;
}

const std::string &
SDFile::extension() const
{
    return m_extension;
}

time_t
SDFile::lastModificationDate() const
{
    return m_lastModificationTime;
}

bool
SDFile::isNetwork() const
{
  return m_isNetwork;
}

int64_t
SDFile::size() const
{
    return m_size;
}

IFile::LinkedFileType SDFile::linkedType() const
{
    return m_linkedType;
}

const std::string &SDFile::linkedWith() const
{
    return m_linkedFile;
}

  } /* namespace medialibrary */
} /* namespace vlc */
