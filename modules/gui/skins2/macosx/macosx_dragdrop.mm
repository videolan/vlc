/*****************************************************************************
 * macosx_dragdrop.mm
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

#include "macosx_dragdrop.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_add_item.hpp"

#include <vlc_url.h>


/// NSView subclass that handles drag and drop
@interface VLCDropView : NSView
{
    MacOSXDragDrop *m_pHandler;
}
- (instancetype)initWithFrame:(NSRect)frame handler:(MacOSXDragDrop *)handler;
@end

@implementation VLCDropView

- (instancetype)initWithFrame:(NSRect)frame handler:(MacOSXDragDrop *)handler
{
    self = [super initWithFrame:frame];
    if( self )
    {
        m_pHandler = handler;

        // Register for drag types
        [self registerForDraggedTypes:@[
            NSPasteboardTypeFileURL,
            NSPasteboardTypeURL,
            NSPasteboardTypeString
        ]];
    }
    return self;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
    return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    NSPasteboard *pasteboard = [sender draggingPasteboard];

    // Try to get file URLs
    NSArray *fileURLs = [pasteboard readObjectsForClasses:@[[NSURL class]]
                                                  options:@{NSPasteboardURLReadingFileURLsOnlyKey: @YES}];

    if( fileURLs && [fileURLs count] > 0 )
    {
        NSMutableArray *paths = [NSMutableArray array];
        for( NSURL *url in fileURLs )
        {
            [paths addObject:[url path]];
        }

        // Convert to C strings
        int count = (int)[paths count];
        const char **files = (const char **)malloc( count * sizeof(char*) );
        for( int i = 0; i < count; i++ )
        {
            files[i] = [[paths objectAtIndex:i] UTF8String];
        }

        if( m_pHandler )
        {
            m_pHandler->handleDrop( files, count );
        }

        free( files );
        return YES;
    }

    // Try generic URLs
    NSArray *urls = [pasteboard readObjectsForClasses:@[[NSURL class]] options:nil];
    if( urls && [urls count] > 0 )
    {
        int count = (int)[urls count];
        const char **files = (const char **)malloc( count * sizeof(char*) );
        for( int i = 0; i < count; i++ )
        {
            NSURL *url = [urls objectAtIndex:i];
            files[i] = [[url absoluteString] UTF8String];
        }

        if( m_pHandler )
        {
            m_pHandler->handleDrop( files, count );
        }

        free( files );
        return YES;
    }

    return NO;
}

@end


MacOSXDragDrop::MacOSXDragDrop( intf_thread_t *pIntf, NSWindow *pWindow,
                                bool playOnDrop, GenericWindow *pWin ):
    SkinObject( pIntf ), m_pWindow( pWindow ), m_pDropView( nil ),
    m_playOnDrop( playOnDrop ), m_pWin( pWin )
{
    @autoreleasepool {
        if( m_pWindow )
        {
            // Create and add drop view
            NSView *contentView = [m_pWindow contentView];
            NSRect frame = [contentView bounds];

            m_pDropView = [[VLCDropView alloc] initWithFrame:frame handler:this];
            [m_pDropView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
            [contentView addSubview:m_pDropView positioned:NSWindowAbove relativeTo:nil];
        }
    }
}


MacOSXDragDrop::~MacOSXDragDrop()
{
    @autoreleasepool {
        if( m_pDropView )
        {
            [m_pDropView removeFromSuperview];
            m_pDropView = nil;
        }
    }
}


void MacOSXDragDrop::handleDrop( const char **files, int count )
{
    for( int i = 0; i < count; i++ )
    {
        const char *file = files[i];

        // Convert path to URI if needed
        char *uri = vlc_path2uri( file, NULL );
        if( !uri )
        {
            // Might already be a URI
            uri = strdup( file );
        }

        if( uri )
        {
            bool playNow = m_playOnDrop && (i == 0);

            CmdAddItem *pCmd = new CmdAddItem( getIntf(), uri, playNow );
            AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
            pQueue->push( CmdGenericPtr( pCmd ) );

            free( uri );
        }
    }
}
