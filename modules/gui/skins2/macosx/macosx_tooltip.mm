/*****************************************************************************
 * macosx_tooltip.mm
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

#include "macosx_tooltip.hpp"
#include "macosx_graphics.hpp"


MacOSXTooltip::MacOSXTooltip( intf_thread_t *pIntf ):
    OSTooltip( pIntf ), m_pWindow( nil )
{
    @autoreleasepool {
        NSScreen *mainScreen = [NSScreen mainScreen];
        m_screenHeight = (int)[mainScreen frame].size.height;

        // Create a tooltip window
        m_pWindow = [[NSWindow alloc]
            initWithContentRect:NSMakeRect( 0, 0, 1, 1 )
                      styleMask:NSWindowStyleMaskBorderless
                        backing:NSBackingStoreBuffered
                          defer:YES];

        [m_pWindow setOpaque:NO];
        [m_pWindow setBackgroundColor:[NSColor colorWithCalibratedWhite:0.95 alpha:0.95]];
        [m_pWindow setLevel:NSPopUpMenuWindowLevel];
        [m_pWindow setIgnoresMouseEvents:YES];
        [m_pWindow setHasShadow:YES];

        // Create an image view for the tooltip content
        NSImageView *imageView = [[NSImageView alloc] initWithFrame:NSZeroRect];
        [imageView setImageScaling:NSImageScaleNone];
        [m_pWindow setContentView:imageView];
    }
}


MacOSXTooltip::~MacOSXTooltip()
{
    @autoreleasepool {
        if( m_pWindow )
        {
            [m_pWindow orderOut:nil];
            m_pWindow = nil;
        }
    }
}


void MacOSXTooltip::show( int left, int top, OSGraphics &rText )
{
    @autoreleasepool {
        if( !m_pWindow )
            return;

        const MacOSXGraphics &rGraphics = static_cast<const MacOSXGraphics&>(rText);
        int width = rGraphics.getWidth();
        int height = rGraphics.getHeight();

        // Convert coordinates
        int y = m_screenHeight - top - height;

        // Resize and position the window
        NSRect frame = NSMakeRect( left, y, width, height );
        [m_pWindow setFrame:frame display:NO];

        // Create image from graphics
        CGImageRef cgImage = (CGImageRef)rGraphics.getImage();
        if( cgImage )
        {
            NSImage *nsImage = [[NSImage alloc] initWithCGImage:cgImage
                                                           size:NSMakeSize(width, height)];
            NSImageView *imageView = (NSImageView *)[m_pWindow contentView];
            [imageView setFrame:NSMakeRect(0, 0, width, height)];
            [imageView setImage:nsImage];

            CGImageRelease( cgImage );
        }

        [m_pWindow orderFront:nil];
    }
}


void MacOSXTooltip::hide()
{
    @autoreleasepool {
        if( m_pWindow )
        {
            [m_pWindow orderOut:nil];
        }
    }
}
