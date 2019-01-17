/*****************************************************************************
 * VLCPlaylistView.m: OutlineView subclass for the playlist
 *****************************************************************************
* Copyright (C) 2003-2015 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman at videola/n dot org>
 *          Benjamin Pracht <bigben at videolab dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "VLCMain.h"
#import "VLCPlaylistView.h"
#import "VLCPlaylist.h"

@implementation VLCPlaylistView

- (NSMenu *)menuForEvent:(NSEvent *)event
{
    return([(VLCPlaylist *)[self delegate] menuForEvent: event]);
}

- (void)keyDown:(NSEvent *)event
{
    unichar key = 0;

    if ([[event characters] length])
        key = [[event characters] characterAtIndex: 0];

    switch(key) {
        case NSDeleteCharacter:
        case NSDeleteFunctionKey:
        case NSDeleteCharFunctionKey:
        case NSBackspaceCharacter:
            [(VLCPlaylist *)[self delegate] deleteItem:self];
            break;

        case NSEnterCharacter:
        case NSCarriageReturnCharacter:
            [(VLCPlaylist *)[[VLCMain sharedInstance] playlist] playItem:nil];
            break;

        default:
            [super keyDown: event];
            break;
    }
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    [self setNeedsDisplay:YES];
    return YES;
}

- (BOOL)resignFirstResponder
{
    [self setNeedsDisplay:YES];
    return YES;
}

@end
