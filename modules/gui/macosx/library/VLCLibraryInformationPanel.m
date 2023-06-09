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

    _titleTextFieldTopConstraint.constant = self.window.titlebarHeight + VLCLibraryUIUnits.smallSpacing;

    NSEdgeInsets scrollViewInsets = _scrollView.contentInsets;
    scrollViewInsets.top = _topBarView.frame.size.height + VLCLibraryUIUnits.mediumSpacing;
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
        [textContent appendAttributedString:[self detailsStringForLibraryItem:_representedItem]];
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
        [detailsString appendAttributedString:[self detailsStringForTrack:track]];
    }

    return detailsString;
}

- (NSAttributedString *)detailsStringForTrack:(VLCMediaLibraryTrack *)track
{
    NSMutableAttributedString * const detailsString = [[NSMutableAttributedString alloc] init];

    [detailsString appendAttributedString:[self detailLineWithTitle:@"Type" detailText:track.readableTrackType]];

    NSString * const codecString = [NSString stringWithFormat:@"%@ (%@) @ %u kB/s\n",
                                    track.readableCodecName,
                                    track.codec,
                                    track.bitrate / 1024 / 8];
    [detailsString appendAttributedString:[self detailLineWithTitle:@"Codec" detailText:codecString]];

    if (track.language.length > 0) {
        [detailsString appendAttributedString:[self detailLineWithTitle:@"Language" detailText:track.language]];
    }

    if (track.trackDescription.length > 0) {
        [detailsString appendAttributedString:[self detailLineWithTitle:@"Description" detailText:track.trackDescription]];
    }

    if (track.trackType == VLC_ML_TRACK_TYPE_AUDIO) {
        NSString * const numChannelsString = [NSString stringWithFormat:@"%u", track.numberOfAudioChannels];
        [detailsString appendAttributedString:[self detailLineWithTitle:@"Number of channels" detailText:numChannelsString]];

        NSString * const sampleRateString = [NSString stringWithFormat:@"%u", track.audioSampleRate];
        [detailsString appendAttributedString:[self detailLineWithTitle:@"Sample rate" detailText:sampleRateString]];
    } else if (track.trackType == VLC_ML_TRACK_TYPE_VIDEO) {
        NSString * const dimensionsString = [NSString stringWithFormat:@"%ux%u px", track.videoWidth, track.videoHeight];
        [detailsString appendAttributedString:[self detailLineWithTitle:@"Dimensions" detailText:dimensionsString]];

        NSString * const aspectRatioString = [NSString stringWithFormat:@"%2.f", (float)track.sourceAspectRatio / track.sourceAspectRatioDenominator];
        [detailsString appendAttributedString:[self detailLineWithTitle:@"Aspect ratio" detailText:aspectRatioString]];

        NSString * const frameRateString = [NSString stringWithFormat:@"%2.f", (float)track.frameRate / track.frameRateDenominator];
        [detailsString appendAttributedString:[self detailLineWithTitle:@"Framerate" detailText:frameRateString]];
    }

    return [detailsString copy];
}

- (NSAttributedString *)detailsStringForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    NSMutableAttributedString * const detailsString = [[NSMutableAttributedString alloc] init];
    [detailsString appendAttributedString:[self detailLineWithTitle:@"Duration" detailText:libraryItem.durationString]];

    NSString * const smallArtworkGeneratedString = libraryItem.smallArtworkGenerated ? _NS("Yes") : _NS("No");
    [detailsString appendAttributedString:[self detailLineWithTitle:@"Small artwork generated" detailText:smallArtworkGeneratedString]];

    return [detailsString copy];
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
