/*
 * Debug.hpp
 *****************************************************************************
 * Copyright © 2015 VideoLAN and VLC Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef DEBUG_HPP
#define DEBUG_HPP

//#define ADAPTATIVE_ADVANCED_DEBUG 0
//#define ADAPTATIVE_BW_DEBUG 0

#ifdef ADAPTATIVE_ADVANCED_DEBUG
  #define AdvDebug(code) code
#else
  #define AdvDebug(code)
#endif

#ifdef ADAPTATIVE_BW_DEBUG
  #define BwDebug(code) code
#else
  #define BwDebug(code)
#endif

#endif // DEBUG_HPP

