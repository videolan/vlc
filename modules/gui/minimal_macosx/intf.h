/*****************************************************************************
 * intf.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#ifdef HAVE_CONFIG_H
#   import "config.h"
#endif

#import <Cocoa/Cocoa.h>
#import <vlc_common.h>
#import <vlc_interface.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
struct intf_sys_t
{
    int nothing_for_now;
};

