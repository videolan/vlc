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
    VLCMediaLibraryMediaItem *_representedMediaItem;
}

@end

@implementation VLCLibraryInformationPanel

- (void)windowDidLoad {
    [super windowDidLoad];
    [self updateRepresentation];
}

- (void)setRepresentedMediaItem:(VLCMediaLibraryMediaItem *)representedMediaItem
{
    _representedMediaItem = representedMediaItem;
    [self updateRepresentation];
}

- (void)updateRepresentation
{
    NSMutableString *textContent = [[NSMutableString alloc] initWithFormat:@"Title: '%@', ID: %lli\n", _representedMediaItem.title, _representedMediaItem.libraryID];
    if (_representedMediaItem.mediaSubType != VLC_ML_MEDIA_SUBTYPE_UNKNOWN) {
        [textContent appendFormat:@"Type: %@ — %@\n", _representedMediaItem.readableMediaType, _representedMediaItem.readableMediaSubType];
    } else {
        [textContent appendFormat:@"Type: %@\n", _representedMediaItem.readableMediaType];
    }
    [textContent appendFormat:@"Duration: %@\n", [NSString stringWithTime:_representedMediaItem.duration / VLCMediaLibraryMediaItemDurationDenominator]];
    [textContent appendFormat:@"Play count: %u, last played: %@\n", _representedMediaItem.playCount, [NSDateFormatter localizedStringFromDate:[NSDate dateWithTimeIntervalSince1970:_representedMediaItem.lastPlayedDate] dateStyle:NSDateFormatterShortStyle timeStyle:NSDateFormatterShortStyle]];
    [textContent appendFormat:@"Small artwork generated? %@\n", _representedMediaItem.smallArtworkGenerated == YES ? _NS("Yes") : _NS("No")];
    [textContent appendFormat:@"Favorited? %@, Playback progress: %2.f%%\n", _representedMediaItem.smallArtworkGenerated == YES ? _NS("Yes") : _NS("No"), _representedMediaItem.lastPlaybackPosition * 100.];

    NSArray *array = _representedMediaItem.files;
    NSUInteger count = array.count;
    [textContent appendFormat:@"\nNumber of files: %lu\n", count];
    for (NSUInteger x = 0; x < count; x++) {
        VLCMediaLibraryFile *file = array[x];
        [textContent appendFormat:@"URL: %@\n", file.fileURL];
        [textContent appendFormat:@"Type: %@\n", file.readableFileType];
    }

    array = _representedMediaItem.tracks;
    count = array.count;
    [textContent appendFormat:@"\nNumber of tracks: %lu\n", count];
    for (NSUInteger x = 0; x < count; x++) {
        VLCMediaLibraryTrack *track = array[x];
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
            [textContent appendFormat:@"Framerate: %2.f\n", (float)track.frameRate / track.frameRateDenominator];;
        }
        [textContent appendString:@"\n"];
    }

    self.multiLineTextLabel.stringValue = textContent;
    self.window.title = _representedMediaItem.title;
    self.imageView.image = _representedMediaItem.smallArtworkImage;
}

@end
