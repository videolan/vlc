/*****************************************************************************
 * libvlc.hpp: Main libvlc++ class
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

#ifndef LIBVLCPP_HPP
#define LIBVLCPP_HPP

#include <vlc/libvlc.h>

namespace libvlc
{

class Media;
class MediaPlayer;

class libVLC
{
public:
    /**
     * Contructor
     * @param i_argc: the number of arguments
     * @param argv: arguments given to libvlc
     */
    libVLC( int i_argc, const char* const* argv );

    /* Destructor */
    ~libVLC();

    /**
     * Get the version of libVLC
     * @return the version
     */
    const char *version();

    /**
     * Get the compiler use for this binari
     * @return the compiler used
     */
    const char *compiler();

    /**
     * Get the chanset of libvlc
     * @return thje changeset
     */
    const char *chanset();

private:
    /** The instance of libvlc */
    libvlc_instance_t *m_instance;

    /**
     * Some friends class to access the instance of libvlc
     */
    friend class Media;
    friend class MediaPlayer;
};

};

#endif // LIBVLCPP_HPP

