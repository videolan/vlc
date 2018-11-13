/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util.h"

#include <stdexcept>

namespace vlc {
  namespace medialibrary {
    namespace utils {

#ifdef _WIN32
# define DIR_SEPARATORS "\\/"
#else
# define DIR_SEPARATORS "/"
#endif

std::string
extension(const std::string &fileName)
{
    auto pos = fileName.find_last_of('.');
    if (pos == std::string::npos)
        return {};
    return fileName.substr(pos + 1);
}

std::string
fileName(const std::string &filePath)
{
    auto pos = filePath.find_last_of(DIR_SEPARATORS);
    if (pos == std::string::npos)
        return filePath;
    return filePath.substr(pos + 1);
}

    } /* namespace utils */
  } /* namespace medialibrary */
} /* namespace vlc */
