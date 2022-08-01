/*****************************************************************************
 * VLCLibrarySongTableCellView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

#import "VLCLibrarySongTableCellView.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"
#import "main/VLCMain.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryAlbumTracksDataSource.h"

NSString *VLCAudioLibrarySongCellIdentifier = @"VLCAudioLibrarySongCellIdentifier";

@interface VLCLibrarySongTableCellView ()
{
    VLCLibraryController *_libraryController;
}
@end

@implementation VLCLibrarySongTableCellView

- (void)awakeFromNib
{
    self.playInstantlyButton.target = self;
    self.playInstantlyButton.action = @selector(playInstantly:);

    self.trackingView.viewToHide = self.playInstantlyButton;
    self.trackingView.viewToShow = self.trackNumberTextField;
    [self prepareForReuse];
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    self.songNameTextField.stringValue = @"";
    self.durationTextField.stringValue = @"";
    self.trackNumberTextField.stringValue = @"";
    self.playInstantlyButton.hidden = YES;
    self.trackNumberTextField.hidden = NO;
}

- (IBAction)playInstantly:(id)sender
{
    if(_representedMediaItem == nil) {
        return;
    }

    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    BOOL playImmediately = YES;
    [_libraryController appendItemToPlaylist:_representedMediaItem playImmediately:playImmediately];
}

- (void)setRepresentedMediaItem:(VLCMediaLibraryMediaItem *)representedMediaItem
{
    _representedMediaItem = representedMediaItem;
    self.songNameTextField.stringValue = representedMediaItem.displayString;
    self.durationTextField.stringValue = representedMediaItem.durationString;
    self.trackNumberTextField.stringValue = [NSString stringWithFormat:@"%d", representedMediaItem.trackNumber];

}

@end
