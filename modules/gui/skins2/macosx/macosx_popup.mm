/*****************************************************************************
 * macosx_popup.mm
 *****************************************************************************
 * Copyright (C) 2024 the VideoLAN team
 *
 * Authors: VLC contributors
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import <Cocoa/Cocoa.h>

#include "macosx_popup.hpp"


MacOSXPopup::MacOSXPopup( intf_thread_t *pIntf ):
    OSPopup( pIntf ), m_pMenu( nil )
{
    @autoreleasepool {
        NSScreen *mainScreen = [NSScreen mainScreen];
        m_screenHeight = (int)[mainScreen frame].size.height;

        // Create the menu
        m_pMenu = [[NSMenu alloc] initWithTitle:@""];
        [m_pMenu setAutoenablesItems:NO];
    }
}


MacOSXPopup::~MacOSXPopup()
{
    @autoreleasepool {
        m_pMenu = nil;
    }
}


void MacOSXPopup::show( int xPos, int yPos )
{
    @autoreleasepool {
        if( !m_pMenu )
            return;

        // Convert coordinates
        int y = m_screenHeight - yPos;

        // Show the popup menu
        NSPoint location = NSMakePoint( xPos, y );
        [m_pMenu popUpMenuPositioningItem:nil
                               atLocation:location
                                   inView:nil];
    }
}


void MacOSXPopup::hide()
{
    @autoreleasepool {
        if( m_pMenu )
        {
            [m_pMenu cancelTracking];
        }
    }
}


void MacOSXPopup::addItem( const std::string &rLabel, int pos )
{
    @autoreleasepool {
        if( !m_pMenu )
            return;

        NSString *title = [NSString stringWithUTF8String:rLabel.c_str()];
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title
                                                      action:nil
                                               keyEquivalent:@""];
        [item setTag:pos];
        [item setEnabled:YES];

        // Insert at the correct position
        if( pos >= 0 && pos < (int)[m_pMenu numberOfItems] )
        {
            [m_pMenu insertItem:item atIndex:pos];
        }
        else
        {
            [m_pMenu addItem:item];
        }

        // Update the ID->position map
        m_idPosMap[(int)[m_pMenu indexOfItem:item]] = pos;
    }
}


void MacOSXPopup::addSeparator( int pos )
{
    @autoreleasepool {
        if( !m_pMenu )
            return;

        NSMenuItem *separator = [NSMenuItem separatorItem];

        if( pos >= 0 && pos < (int)[m_pMenu numberOfItems] )
        {
            [m_pMenu insertItem:separator atIndex:pos];
        }
        else
        {
            [m_pMenu addItem:separator];
        }
    }
}


int MacOSXPopup::getPosFromId( int id ) const
{
    auto it = m_idPosMap.find( id );
    return ( it != m_idPosMap.end() ) ? it->second : -1;
}
