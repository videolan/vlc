/*****************************************************************************
 * exception.hpp: handle exceptions
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

#ifndef LIBVLCPP_EXCEPTION_HPP
#define LIBVLCPP_EXCEPTION_HPP

#include <vlc/libvlc.h>

#include "libvlc.hpp"

namespace libvlc
{

class Exception
{
public:
    /** Create the exception */
    Exception();

    /** Destroy te exception */
    ~Exception();

    /** The exception */
    libvlc_exception_t ex;
};

};

#endif // LIBVLCPP_EXCEPTION_HPP

