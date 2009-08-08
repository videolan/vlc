/*****************************************************************************
 * VLC backend for the Phonon library                                        *
 * Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>               *
 * Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>                *
 * Copyright (C) 2009 Fathi Boudra <fabo@kde.org>                            *
 *                                                                           *
 * This program is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Lesser General Public                *
 * License as published by the Free Software Foundation; either              *
 * version 3 of the License, or (at your option) any later version.          *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Lesser General Public License for more details.                           *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public          *
 * License along with this package; if not, write to the Free Software       *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA *
 *****************************************************************************/

#ifndef PHONON_VLC_VLC_LOADER_H
#define PHONON_VLC_VLC_LOADER_H

#include <vlc/vlc.h>

class QString;

/**
 * VLC library instance global variable.
 */
extern libvlc_instance_t *vlc_instance;

/**
 * VLC library exception handling global variable.
 */
extern libvlc_exception_t *vlc_exception;

/**
 * VLC library media player global variable.
 */
extern libvlc_media_player_t *vlc_current_media_player;

namespace Phonon
{
namespace VLC {

/**
 * Get VLC path.
 *
 * @return the VLC path
 */
QString vlcPath();

/**
 * Unload VLC library.
 */
void vlcUnload();

/**
 * Check for a VLC library exception.
 *
 * show an error message when an exception has been raised.
 */
void vlcExceptionRaised();

/**
 * Initialize and launch VLC library.
 *
 * instance and exception handling global variables are initialized.
 *
 * @return VLC initialization result
 */
bool vlcInit();

/**
 * Stop VLC library.
 */
void vlcRelease();

}
} // Namespace Phonon::VLC

#endif // PHONON_VLC_VLC_LOADER_H
