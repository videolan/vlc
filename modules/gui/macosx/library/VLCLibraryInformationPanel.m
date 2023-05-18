/*****************************************************************************
* VLCLibraryInformationPanel.m: MacOS X interface module
*****************************************************************************
* Copyright (C) 2020 VLC authors and VideoLAN
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

#import "VLCLibraryInformationPanel.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCInputItem.h"

@interface VLCLibraryInformationPanel ()
{
    id<VLCMediaLibraryItemProtocol> _representedItem;
    NSFont *_boldSystemFont;
    NSDictionary<NSAttributedStringKey, id> *_boldStringAttribute;
}

@end

@implementation VLCLibraryInformationPanel

- (void)windowDidLoad
{
    [super windowDidLoad];

    _titleTextFieldTopConstraint.constant = self.window.titlebarHeight + [VLCLibraryUIUnits smallSpacing];

    NSEdgeInsets scrollViewInsets = _scrollView.contentInsets;
    scrollViewInsets.top = _topBarView.frame.size.height + [VLCLibraryUIUnits mediumSpacing];
    _scrollView.contentInsets = scrollViewInsets;

    _boldSystemFont = [NSFont boldSystemFontOfSize:NSFont.systemFontSize];
    _boldStringAttribute = @{NSFontAttributeName: _boldSystemFont};

    [self updateRepresentation];
}

- (void)setRepresentedItem:(id<VLCMediaLibraryItemProtocol>)representedItem
{
    _representedItem = representedItem;
    [self updateRepresentation];
}

- (NSAttributedString *)detailLineWithTitle:(NSString *)title detailText:(NSString *)detailText
{
    NSString * const detailStringStart = [NSString stringWithFormat:@"%@:", title];
    NSMutableAttributedString * const detailLine = [[NSMutableAttributedString alloc] initWithString:detailStringStart attributes:_boldStringAttribute];
    NSString * const detailStringEnd = [NSString stringWithFormat:@" %@\n", detailText];
    [detailLine appendAttributedString:[[NSAttributedString alloc] initWithString:detailStringEnd]];

    return [detailLine copy];
}

- (void)updateRepresentation
{
    _titleTextField.stringValue = _representedItem.displayString;

    NSMutableAttributedString * const textContent = [[NSMutableAttributedString alloc] init];
    [textContent appendAttributedString:[self detailLineWithTitle:@"Title" detailText:_representedItem.displayString]];
    [textContent appendAttributedString:[self detailLineWithTitle:@"ID" detailText:[NSString stringWithFormat:@"%lli", _representedItem.libraryID]]];

    if([_representedItem isKindOfClass:[VLCMediaLibraryMediaItem class]]) {
        [textContent appendAttributedString:[self detailsStringForMediaItem:(VLCMediaLibraryMediaItem *)_representedItem]];
    } else {
        NSString * const itemDetailsString = [self detailsStringForLibraryItem:_representedItem];
        [textContent appendAttributedString:[[NSAttributedString alloc] initWithString:itemDetailsString]];
    }

    NSString * const fileDetailsString = [self fileDetailsStringForLibraryItem:_representedItem];
    [textContent appendAttributedString:[[NSAttributedString alloc] initWithString:fileDetailsString]];
    
    _textField.attributedStringValue = textContent;
    _textField.font = [NSFont systemFontOfSize:13.];
    _textField.textColor = [NSColor whiteColor];

    [VLCLibraryImageCache thumbnailForLibraryItem:_representedItem withCompletion:^(NSImage * const thumbnail) {
        self->_imageView.image = thumbnail;
    }];
}

- (NSAttributedString *)detailsStringForMediaItem:(VLCMediaLibraryMediaItem *)mediaItem
{
    NSMutableAttributedString * const detailsString = [[NSMutableAttributedString alloc] init];

    NSMutableString * const mediaTypeString = [[NSMutableString alloc] initWithFormat:@" %@", mediaItem.readableMediaType];
    if (mediaItem.mediaSubType != VLC_ML_MEDIA_SUBTYPE_UNKNOWN) {
        [mediaTypeString appendFormat:@" — %@", mediaItem.readableMediaSubType];
    }
    [mediaTypeString appendString:@"\n"];

    [detailsString appendAttributedString:[self detailLineWithTitle:@"Type" detailText:mediaTypeString]];
    [detailsString appendAttributedString:[self detailLineWithTitle:@"Duration" detailText:_representedItem.durationString]];

    NSString * const playCountString = [NSString stringWithFormat:@"%u", mediaItem.playCount];
    [detailsString appendAttributedString:[self detailLineWithTitle:@"Play count" detailText:playCountString]];

    NSDate * const lastPlayedDate = [NSDate dateWithTimeIntervalSince1970:mediaItem.lastPlayedDate];
    NSString * const lastPlayedString = [NSDateFormatter localizedStringFromDate:lastPlayedDate
                                                                       dateStyle:NSDateFormatterShortStyle
                                                                       timeStyle:NSDateFormatterShortStyle];
    [detailsString appendAttributedString:[self detailLineWithTitle:@"Last played" detailText:lastPlayedString]];

    NSString * const smallArtworkGeneratedString = _representedItem.smallArtworkGenerated ? _NS("Yes") : _NS("No");
    [detailsString appendAttributedString:[self detailLineWithTitle:@"Small artwork generated" detailText:smallArtworkGeneratedString]];

    NSString * const favouritedString = mediaItem.favorited ? _NS("Yes") : _NS("No");
    [detailsString appendAttributedString:[self detailLineWithTitle:@"Favourited" detailText:favouritedString]];

    NSString * const playbackProgressString = [NSString stringWithFormat:@"%2.f%%", mediaItem.progress * 100.];
    [detailsString appendAttributedString:[self detailLineWithTitle:@"Playback progress" detailText:playbackProgressString]];
    // TODO: Calculate progress for other library item types

    NSString * const trackCountString = [NSString stringWithFormat:@"%lu", mediaItem.tracks.count];
    [detailsString appendAttributedString:[self detailLineWithTitle:@"Number of tracks" detailText:trackCountString]];

    for (VLCMediaLibraryTrack *track in mediaItem.tracks) {
        [detailsString appendAttributedString:[[NSAttributedString alloc] initWithString:[self detailsStringForTrack:track]]];
    }

    return detailsString;
}

- (NSString *)detailsStringForTrack:(VLCMediaLibraryTrack *)track
{
    NSMutableString *detailsString = [[NSMutableString alloc] init];

    [detailsString appendFormat:@"Type: %@\n", track.readableTrackType];
    [detailsString appendFormat:@"Codec: %@ (%@) @ %u kB/s\n", track.readableCodecName, track.codec, track.bitrate / 1024 / 8];
    if (track.language.length > 0) {
        [detailsString appendFormat:@"Language: %@\n", track.language];
    }
    if (track.trackDescription.length > 0) {
        [detailsString appendFormat:@"Description: %@\n", track.trackDescription];
    }

    if (track.trackType == VLC_ML_TRACK_TYPE_AUDIO) {
        [detailsString appendFormat:@"Number of Channels: %u, Sample rate: %u\n", track.numberOfAudioChannels, track.audioSampleRate];
    } else if (track.trackType == VLC_ML_TRACK_TYPE_VIDEO) {
        [detailsString appendFormat:@"Dimensions: %ux%u px, Aspect-Ratio: %2.f\n", track.videoWidth, track.videoHeight, (float)track.sourceAspectRatio / track.sourceAspectRatioDenominator];
        [detailsString appendFormat:@"Framerate: %2.f\n", (float)track.frameRate / track.frameRateDenominator];
    }
    [detailsString appendString:@"\n"];

    return detailsString;
}

- (NSString *)detailsStringForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    NSMutableString *detailsString = [[NSMutableString alloc] init];

    [detailsString appendFormat:@"Duration: %@\n", libraryItem.durationString];
    [detailsString appendFormat:@"Small artwork generated? %@\n", libraryItem.smallArtworkGenerated ? _NS("Yes") : _NS("No")];

    return detailsString;
}

- (NSString *)fileDetailsStringForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    __block NSUInteger fileCount = 0;
    NSMutableString *fileDetails = [[NSMutableString alloc] init];

    [_representedItem iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        for (VLCMediaLibraryFile *file in mediaItem.files) {
            ++fileCount;
            [fileDetails appendFormat:@"URL: %@\n", file.fileURL];
            [fileDetails appendFormat:@"Type: %@\n", file.readableFileType];
        }
    }];
    [fileDetails appendFormat:@"\nNumber of files: %lu\n", fileCount];

    return fileDetails;
}

@end
