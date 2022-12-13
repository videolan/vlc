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

    if([_representedItem isKindOfClass:[VLCMediaLibraryMediaItem class]]) {
        VLCMediaLibraryMediaItem *mediaItem = (VLCMediaLibraryMediaItem *)_representedItem;
        if (mediaItem.mediaSubType != VLC_ML_MEDIA_SUBTYPE_UNKNOWN) {
            [textContent appendFormat:@"Type: %@ — %@\n", mediaItem.readableMediaType, mediaItem.readableMediaSubType];
        } else {
            [textContent appendFormat:@"Type: %@\n", mediaItem.readableMediaType];
        }
        [textContent appendFormat:@"Duration: %@\n", _representedItem.durationString];
        [textContent appendFormat:@"Play count: %u, last played: %@\n", mediaItem.playCount, [NSDateFormatter localizedStringFromDate:[NSDate dateWithTimeIntervalSince1970:mediaItem.lastPlayedDate] dateStyle:NSDateFormatterShortStyle timeStyle:NSDateFormatterShortStyle]];
        [textContent appendFormat:@"Small artwork generated? %@\n", _representedItem.smallArtworkGenerated == YES ? _NS("Yes") : _NS("No")];
        [textContent appendFormat:@"Favorited? %@\n", mediaItem.favorited == YES ? _NS("Yes") : _NS("No")];
        [textContent appendFormat:@"Playback progress: %2.f%%\n", mediaItem.progress * 100.]; // TODO: Calculate progress for other library item types

        [textContent appendFormat:@"\nNumber of tracks: %lu\n", mediaItem.tracks.count];
        for (VLCMediaLibraryTrack *track in mediaItem.tracks) {
            [textContent appendFormat:@"Type: %@\n", track.readableTrackType];
            [textContent appendFormat:@"Codec: %@ (%@) @ %u kB/s\n", track.readableCodecName, track.codec, track.bitrate / 1024 / 8];
            if (track.language.length > 0) {
                [textContent appendFormat:@"Language: %@\n", track.language];
            }
            if (track.trackDescription.length > 0) {
                [textContent appendFormat:@"Description: %@\n", track.trackDescription];
            }

            if (track.trackType == VLC_ML_TRACK_TYPE_AUDIO) {
                [textContent appendFormat:@"Number of Channels: %u, Sample rate: %u\n", track.numberOfAudioChannels, track.audioSampleRate];
            } else if (track.trackType == VLC_ML_TRACK_TYPE_VIDEO) {
                [textContent appendFormat:@"Dimensions: %ux%u px, Aspect-Ratio: %2.f\n", track.videoWidth, track.videoHeight, (float)track.sourceAspectRatio / track.sourceAspectRatioDenominator];
                [textContent appendFormat:@"Framerate: %2.f\n", (float)track.frameRate / track.frameRateDenominator];
            }
            [textContent appendString:@"\n"];
        }
    } else {
        [textContent appendFormat:@"Duration: %@\n", _representedItem.durationString];
        [textContent appendFormat:@"Small artwork generated? %@\n", _representedItem.smallArtworkGenerated == YES ? _NS("Yes") : _NS("No")];
    }

    __block NSUInteger fileCount = 0;
    NSMutableString *fileDetails = [[NSMutableString alloc] init];

    [_representedItem iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        for (VLCMediaLibraryFile *file in mediaItem.files) {
            ++fileCount;
            [fileDetails appendFormat:@"URL: %@\n", file.fileURL];
            [fileDetails appendFormat:@"Type: %@\n", file.readableFileType];
        }
    }];
    [textContent appendFormat:@"\nNumber of files: %lu\n", fileCount];
    [textContent appendString: fileDetails];
    
    _textField.attributedStringValue = [[NSAttributedString alloc] initWithString:textContent];
    _textField.font = [NSFont systemFontOfSize:13.];
    _textField.textColor = [NSColor whiteColor];
    _imageView.image = _representedItem.smallArtworkImage;
    self.window.title = _representedItem.displayString;
}

@end
