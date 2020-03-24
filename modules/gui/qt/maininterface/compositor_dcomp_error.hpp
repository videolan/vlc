/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#ifndef COMPOSITOR_DCOMP_ERROR_HPP
#define COMPOSITOR_DCOMP_ERROR_HPP


#include <stdexcept>
#include <windows.h>

namespace vlc {

class DXError : public std::runtime_error
{
public:
    explicit DXError(const std::string& msg, HRESULT code)
        : std::runtime_error(msg)
        , m_code(code)
    {
    }

    explicit DXError(const char* msg, HRESULT code)
        : std::runtime_error(msg)
        , m_code(code)
    {
    }

    inline HRESULT code() const
    {
        return m_code;
    }

private:
    HRESULT m_code;
};

inline void HR( HRESULT hr, const std::string& msg )
{
    if( FAILED( hr ) )
        throw DXError{ msg, hr };
}

inline void HR( HRESULT hr, const char* msg = "" )
{
    if( FAILED( hr ) )
        throw DXError{ msg, hr  };
}

}


#endif // COMPOSITOR_DCOMP_ERROR_HPP
