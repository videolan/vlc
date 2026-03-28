/*****************************************************************************
 * macosx_popup.mm
 *****************************************************************************
 * Copyright (C) 2026 the VideoLAN team
 *
 * Authors: Fletcher Holt <fletcherholt649@gmail.com>
 *          Felix Paul Kühne <fkuehne@videolan.org>
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
#include "macosx_factory.hpp"
#include "../events/evt_menu.hpp"
#include "../src/os_factory.hpp"
#include "../src/generic_window.hpp"

@interface VLCPopupMenuTarget : NSObject
{
    int m_selectedTag;
}
@property (nonatomic) int selectedTag;
- (void)menuItemSelected:(id)sender;
@end

@implementation VLCPopupMenuTarget
@synthesize selectedTag = m_selectedTag;

- (instancetype)init
{
    self = [super init];
    if( self )
        m_selectedTag = -1;
    return self;
}

- (void)menuItemSelected:(id)sender
{
    m_selectedTag = [sender tag];
}

@end


MacOSXPopup::MacOSXPopup( intf_thread_t *pIntf ):
    OSPopup( pIntf ), m_pMenu( nil ), m_pTarget( nil )
{
    @autoreleasepool {
        // Create the menu
        m_pMenu = [[NSMenu alloc] initWithTitle:@""];
        [m_pMenu setAutoenablesItems:NO];
        m_pTarget = [[VLCPopupMenuTarget alloc] init];
    }
}


MacOSXPopup::~MacOSXPopup()
{
    @autoreleasepool {
        m_pMenu = nil;
        m_pTarget = nil;
    }
}


void MacOSXPopup::show( int xPos, int yPos )
{
    @autoreleasepool {
        if( !m_pMenu )
            return;

        // Reset selected tag
        [(VLCPopupMenuTarget *)m_pTarget setSelectedTag:-1];

        // Convert coordinates
        int y = [NSScreen mainScreen].frame.size.height - yPos;

        // Show the popup menu
        NSPoint location = NSMakePoint( xPos, y );
        [m_pMenu popUpMenuPositioningItem:nil
                               atLocation:location
                                   inView:nil];

        // After menu dismissal, dispatch EvtMenu if an item was selected
        int selectedTag = [(VLCPopupMenuTarget *)m_pTarget selectedTag];
        if( selectedTag >= 0 )
        {
            MacOSXFactory *pFactory = static_cast<MacOSXFactory*>(
                OSFactory::instance( getIntf() ) );
            NSWindow *keyWindow = [NSApp keyWindow];
            if( pFactory && keyWindow )
            {
                auto it = pFactory->m_windowMap.find( (__bridge void *)keyWindow );
                if( it != pFactory->m_windowMap.end() && it->second )
                {
                    EvtMenu evt( getIntf(), selectedTag );
                    it->second->processEvent( evt );
                }
            }
        }
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
                                                      action:@selector(menuItemSelected:)
                                               keyEquivalent:@""];
        [item setTarget:m_pTarget];
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
