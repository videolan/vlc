/*****************************************************************************
 * libvlc.cpp: Main libvlc++ class
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

#include "libvlc.hpp"
#include "exception.hpp"

using namespace libvlc;

libVLC::libVLC( int i_argc, const char *const *argv )
{
    Exception ex;
    m_instance = libvlc_new( i_argc, argv, &ex.ex);
}

libVLC::~libVLC()
{
    libvlc_release( m_instance );
}

const char *libVLC::version()
{
    return libvlc_get_version();
}

const char *libVLC::compiler()
{
    return libvlc_get_compiler();
}

const char *libVLC::chanset()
{
    return libvlc_get_changeset();
}
