/*****************************************************************************
 * ConvertAndSave.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012 Felix Paul Kühne
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import "ConvertAndSave.h"
#import "intf.h"
#import <vlc_common.h>
#import <vlc_url.h>

@interface VLCConvertAndSave ()
@property (readwrite, retain) NSArray * profileNames;
@property (readwrite, retain) NSArray * profileValueList;
@property (readwrite, retain) NSMutableArray * currentProfile;
@end

@implementation VLCConvertAndSave

@synthesize MRL=_MRL, outputDestination=_outputDestination, profileNames=_profileNames, profileValueList=_profileValueList, currentProfile=_currentProfile;

static VLCConvertAndSave *_o_sharedInstance = nil;

+ (VLCConvertAndSave *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)dealloc
{
    if (_MRL)
        [_MRL release];
    if (_outputDestination)
        [_outputDestination release];
    if (_profileNames)
        [_profileNames release];
    if (_profileValueList)
        [_profileValueList release];
    if (_currentProfile)
        [_currentProfile release];

    [super dealloc];
}

- (void)awakeFromNib
{
    [_window setTitle: _NS("Convert & Save")];
    [_cancel_btn setTitle: _NS("Cancel")];
    [_ok_btn setTitle: _NS("Save")];
    [_drop_lbl setStringValue: _NS("Drop Media here")];
    [_drop_btn setTitle: _NS("Open Media...")];
    [_profile_lbl setStringValue: _NS("Choose Profile")];
    [_profile_btn setTitle: _NS("Customize")];
    [_destination_lbl setStringValue: _NS("Choose Destination")];
    [_destination_filename_stub_lbl setStringValue: _NS("Choose an output location")];
    [_destination_filename_lbl setHidden: YES];
    [_customize_ok_btn setTitle: _NS("Apply")];
    [_customize_cancel_btn setTitle: _NS("Cancel")];
    [[_customize_tabview tabViewItemAtIndex:0] setLabel: _NS("Encapsulation")];
    [[_customize_tabview tabViewItemAtIndex:1] setLabel: _NS("Video codec")];
    [[_customize_tabview tabViewItemAtIndex:2] setLabel: _NS("Audio codec")];
    [[_customize_tabview tabViewItemAtIndex:3] setLabel: _NS("Subtitles")];
    [_customize_tabview selectTabViewItemAtIndex: 0];
    [_customize_vid_ckb setTitle: _NS("Video")];
    [_customize_vid_keep_ckb setTitle: _NS("Keep original video track")];
    [_customize_vid_codec_lbl setStringValue: _NS("Codec")];
    [_customize_vid_bitrate_lbl setStringValue: _NS("Bitrate")];
    [_customize_vid_framerate_lbl setStringValue: _NS("Frame Rate")];
    [_customize_vid_res_box setTitle: _NS("Resolution")];
    [_customize_vid_res_lbl setStringValue: _NS("You just need to fill one of the three following parameters, VLC will autodetect the other using the original aspect ratio")];
    [_customize_vid_width_lbl setStringValue: _NS("Width")];
    [_customize_vid_height_lbl setStringValue: _NS("Height")];
    [_customize_vid_scale_lbl setStringValue: _NS("Scale")];
    [_customize_aud_ckb setTitle: _NS("Audio")];
    [_customize_aud_keep_ckb setTitle: _NS("Keep original audio track")];
    [_customize_aud_codec_lbl setStringValue: _NS("Codec")];
    [_customize_aud_bitrate_lbl setStringValue: _NS("Bitrate")];
    [_customize_aud_channels_lbl setStringValue: _NS("Channels")];
    [_customize_aud_samplerate_lbl setStringValue: _NS("Sample Rate")];
    [_customize_subs_ckb setTitle: _NS("Subtitles")];
    [_customize_subs_overlay_ckb setTitle: _NS("Overlay subtitles on the video")];

    _profileNames = [[NSArray alloc] initWithObjects:
                     @"Video - H.264 + MP3 (MP4)",
                     @"Video - VP80 + Vorbis (Webm)",
                     @"Video - H.264 + MP3 (TS)",
                     @"Video - Dirac + MP3 (TS)",
                     @"Video - Theora + Vorbis (OGG)",
                     @"Video - Theora + Flac (OGG)",
                     @"Video - MPEG-2 + MPGA (TS)",
                     @"Video - WMV + WMA (ASF)",
                     @"Video - DIV3 + MP3 (ASF)",
                     @"Audio - Vorbis (OGG)",
                     @"Audio - MP3",
                     @"Audio - MP3 (MP4)",
                     @"Audio - FLAC",
                     @"Audio - CD",
                     _NS("Custom"),
                     nil];

    /* We are using the same format as the Qt4 intf here:
     * Container(string), transcode video(bool), transcode audio(bool),
     * use subtitles(bool), video codec(string), video bitrate(integer),
     * scale(float), fps(float), width(integer, height(integer),
     * audio codec(string), audio bitrate(integer), channels(integer),
     * samplerate(integer), subtitle codec(string), subtitle overlay(bool) */
    _profileValueList = [[NSArray alloc] initWithObjects:
                         @"mp4;1;1;0;h264;0;0;0;0;0;mpga;128;2;44100;0;1",
                         @"webm;1;1;0;VP80;2000;0;0;0;0;vorb;128;2;44100;0;1",
                         @"ts;1;1;0;h264;800;1;0;0;0;mpga;128;2;44100;0;0",
                         @"ts;1;1;0;drac;800;1;0;0;0;mpga;128;2;44100;0;0",
                         @"ogg;1;1;0;theo;800;1;0;0;0;vorb;128;2;44100;0;0",
                         @"ogg;1;1;0;theo;800;1;0;0;0;flac;128;2;44100;0;0",
                         @"ts;1;1;0;mp2v;800;1;0;0;0;mpga;128;2;44100;0;0",
                         @"asf;1;1;0;WMV2;800;1;0;0;0;wma2;128;2;44100;0;0",
                         @"asf;1;1;0;DIV3;800;1;0;0;0;mp3;128;2;44100;0;0",
                         @"ogg;1;1;0;none;800;1;0;0;0;vorb;128;2;44100;none;0",
                         @"raw;1;1;0;none;800;1;0;0;0;mp3;128;2;44100;none;0",
                         @"mp4;1;1;0;none;800;1;0;0;0;mpga;128;2;44100;none;0",
                         @"raw;1;1;0;none;800;1;0;0;0;flac;128;2;44100;none;0",
                         @"wav;1;1;0;none;800;1;0;0;0;s16l;128;2;44100;none;0", nil];

    [_profile_pop removeAllItems];
    [_profile_pop addItemsWithTitles: _profileNames];
}

- (void)toggleWindow
{
    [_window makeKeyAndOrderFront: nil];
}

- (IBAction)windowButtonAction:(id)sender
{
}

- (IBAction)openMedia:(id)sender
{
}

- (IBAction)profileSelection:(id)sender
{
    NSInteger index = [sender indexOfSelectedItem];
    if (index != ([sender numberOfItems] - 1))
    {
        if (_currentProfile)
            [_currentProfile release];
        _currentProfile = [[NSMutableArray alloc] initWithArray: [[_profileValueList objectAtIndex:index] componentsSeparatedByString:@";"]];
    }
}

- (IBAction)customizeProfile:(id)sender
{
    [NSApp beginSheet:_customize_panel modalForWindow:_window modalDelegate:self didEndSelector:NULL contextInfo:nil];
}

- (IBAction)closeCustomizationSheet:(id)sender
{
    // sender == _customize_ok_btn ?
    [_customize_panel orderOut:sender];
    [NSApp endSheet: _customize_panel];
}

- (IBAction)chooseDestination:(id)sender
{
    NSSavePanel * saveFilePanel = [[NSSavePanel alloc] init];
    [saveFilePanel setCanSelectHiddenExtension: YES];
    [saveFilePanel setCanCreateDirectories: YES];
    [saveFilePanel beginSheetForDirectory:nil file:nil modalForWindow:_window modalDelegate:self didEndSelector:@selector(savePanelDidEnd:returnCode:contextInfo:) contextInfo:nil];
}

- (void)savePanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if (returnCode == NSOKButton) {
        _outputDestination = [[sheet URL] path];
        [_destination_filename_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath:_outputDestination]];
        [[_destination_filename_stub_lbl animator] setHidden: YES];
        [[_destination_filename_lbl animator] setHidden: NO];
    } else {
        _outputDestination = @"";
        [[_destination_filename_lbl animator] setHidden: YES];
        [[_destination_filename_stub_lbl animator] setHidden: NO];
    }
}

- (void)updateDropView
{
    if ([_MRL length] > 0) {
        NSString * path = [[NSURL URLWithString:_MRL] path];
        [_dropin_media_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath: path]];
        NSImage * image = [[NSWorkspace sharedWorkspace] iconForFile: path];
        [image setSize:NSMakeSize(64,64)];
        [_dropin_icon_view setImage: image];

        if (![_dropin_view superview]) {
            NSRect boxFrame = [_drop_box frame];
            NSRect subViewFrame = [_dropin_view frame];
            subViewFrame.origin.x = (boxFrame.size.width - subViewFrame.size.width) / 2;
            subViewFrame.origin.y = (boxFrame.size.height - subViewFrame.size.height) / 2;
            [_dropin_view setFrame: subViewFrame];
            [[_drop_image_view animator] setHidden: YES];
            [_drop_box performSelector:@selector(addSubview:) withObject:_dropin_view afterDelay:0.4];
        }
    } else {
        [_dropin_view removeFromSuperview];
        [[_drop_image_view animator] setHidden: NO];
    }
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *paste = [sender draggingPasteboard];
    NSArray *types = [NSArray arrayWithObject: NSFilenamesPboardType];
    NSString *desired_type = [paste availableTypeFromArray: types];
    NSData *carried_data = [paste dataForType: desired_type];

    if( carried_data ) {
        if( [desired_type isEqualToString:NSFilenamesPboardType] ) {
            NSArray *values = [[paste propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

            if ([values count] > 0) {
                [self setMRL: [NSString stringWithUTF8String:make_URI([[values objectAtIndex:0] UTF8String], NULL)]];
                [self updateDropView];
                return YES;
            }
        }
    }
    return NO;
}

@end


@implementation VLCDropEnabledBox

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObject: NSFilenamesPboardType]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric)
        return NSDragOperationGeneric;

    return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    return [[VLCConvertAndSave sharedInstance] performDragOperation: sender];
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end

@implementation VLCDropEnabledImageView

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObject: NSFilenamesPboardType]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric)
        return NSDragOperationGeneric;

    return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    return [[VLCConvertAndSave sharedInstance] performDragOperation: sender];
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end

@implementation VLCDropEnabledButton

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObject: NSFilenamesPboardType]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric)
        return NSDragOperationGeneric;

    return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    return [[VLCConvertAndSave sharedInstance] performDragOperation: sender];
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end
