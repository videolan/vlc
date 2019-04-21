/*****************************************************************************
 * VLCLibraryMenuController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCLibraryMenuController.h"

#import "main/VLCMain.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"

#import "extensions/NSString+Helpers.h"

@interface VLCLibraryMenuController ()
{
    NSMenu *_libraryMenu;
    NSIndexPath *_actionIndexPath;
}
@end

@implementation VLCLibraryMenuController

- (instancetype)init
{
    self = [super init];
    if (self) {
        _libraryMenu = [[NSMenu alloc] initWithTitle:@""];
        [_libraryMenu addItemWithTitle:_NS("Play") action:@selector(play:) keyEquivalent:@""];
        [[_libraryMenu itemAtIndex:0] setTarget:self];
        [_libraryMenu addItemWithTitle:_NS("Add Media...") action:@selector(addMedia:) keyEquivalent:@""];
        [[_libraryMenu itemAtIndex:1] setTarget:self];
        [_libraryMenu addItemWithTitle:_NS("Reveal in Finder") action:@selector(revealInFinder:) keyEquivalent:@""];
        [[_libraryMenu itemAtIndex:2] setTarget:self];
    }
    return self;
}

- (void)popupMenuWithEvent:(NSEvent *)theEvent forView:(NSView *)theView
{
    _actionIndexPath = [self.libraryCollectionView indexPathForItemAtPoint:[NSEvent mouseLocation]];

    [NSMenu popUpContextMenu:_libraryMenu withEvent:theEvent forView:theView];
}

#pragma mark - actions

- (void)play:(id)sender
{

}

- (void)addMedia:(id)sender
{

}

- (void)revealInFinder:(id)sender
{
    VLCMediaLibraryMediaItem *mediaItem = [[[[VLCMain sharedInstance] libraryController] libraryModel] mediaItemAtIndexPath:_actionIndexPath];
    if (mediaItem == nil) {
        return;
    }
    VLCMediaLibraryFile *firstFile = mediaItem.files.firstObject;

    if (firstFile) {
        NSURL *URL = [NSURL URLWithString:firstFile.MRL];
        if (URL) {
            [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[URL]];
        }
    }
}

@end
