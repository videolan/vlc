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
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCInputItem.h"
#import "extensions/NSString+Helpers.h"

@interface VLCLibraryInformationPanel ()
{
    id<VLCMediaLibraryItemProtocol> _representedItem;
}

@end

@implementation VLCLibraryInformationPanel

- (void)windowDidLoad {
    [super windowDidLoad];
    [self updateRepresentation];
}

- (void)setRepresentedItem:(id<VLCMediaLibraryItemProtocol>)representedItem
{
    _representedItem = representedItem;
    [self updateRepresentation];
}

- (void)updateRepresentation
{
    NSMutableString *textContent = [[NSMutableString alloc] initWithFormat:@"Title: '%@', ID: %lli\n", _representedItem.displayString, _representedItem.libraryID];

    NSString *itemDetailsString;
    if([_representedItem isKindOfClass:[VLCMediaLibraryMediaItem class]]) {
        itemDetailsString = [self detailsStringForMediaItem:(VLCMediaLibraryMediaItem *)_representedItem];
    } else {
        itemDetailsString = [self detailsStringForLibraryItem:_representedItem];
    }
    [textContent appendString:itemDetailsString];
    
    NSString *fileDetailsString = [self fileDetailsStringForLibraryItem:_representedItem];
    [textContent appendString:fileDetailsString];
    
    _textField.attributedStringValue = [[NSAttributedString alloc] initWithString:textContent];
    _textField.font = [NSFont systemFontOfSize:13.];
    _textField.textColor = [NSColor whiteColor];
    _imageView.image = _representedItem.smallArtworkImage;
    self.window.title = _representedItem.displayString;
}

- (NSString *)detailsStringForMediaItem:(VLCMediaLibraryMediaItem *)mediaItem
{
    NSMutableString *detailsString = [[NSMutableString alloc] init];

    if (mediaItem.mediaSubType != VLC_ML_MEDIA_SUBTYPE_UNKNOWN) {
        [detailsString appendFormat:@"Type: %@ — %@\n", mediaItem.readableMediaType, mediaItem.readableMediaSubType];
    } else {
        [detailsString appendFormat:@"Type: %@\n", mediaItem.readableMediaType];
    }

    [detailsString appendFormat:@"Duration: %@\n", _representedItem.durationString];

    [detailsString appendFormat:@"Play count: %u, last played: %@\n", mediaItem.playCount, [NSDateFormatter localizedStringFromDate:[NSDate dateWithTimeIntervalSince1970:mediaItem.lastPlayedDate] dateStyle:NSDateFormatterShortStyle timeStyle:NSDateFormatterShortStyle]];

    [detailsString appendFormat:@"Small artwork generated? %@\n", _representedItem.smallArtworkGenerated ? _NS("Yes") : _NS("No")];

    [detailsString appendFormat:@"Favorited? %@\n", mediaItem.favorited ? _NS("Yes") : _NS("No")];

    [detailsString appendFormat:@"Playback progress: %2.f%%\n", mediaItem.progress * 100.]; // TODO: Calculate progress for other library item types

    [detailsString appendFormat:@"\nNumber of tracks: %lu\n", mediaItem.tracks.count];

    for (VLCMediaLibraryTrack *track in mediaItem.tracks) {
        NSString *trackDetailsString = [self detailsStringForTrack:track];
        [detailsString appendString:trackDetailsString];
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
