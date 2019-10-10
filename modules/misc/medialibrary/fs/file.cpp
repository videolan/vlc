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

SDFile::SDFile(const std::string &mrl)
    : m_mrl(mrl)
    , m_name(utils::fileName(mrl))
    , m_extension(utils::extension(mrl))
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

unsigned int
SDFile::lastModificationDate() const
{
    return 0;
}

int64_t
SDFile::size() const
{
    return 0;
}

  } /* namespace medialibrary */
} /* namespace vlc */
