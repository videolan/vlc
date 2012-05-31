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

@implementation VLCConvertAndSave

@synthesize MRL=_MRL, outputDestination=_outputDestination;

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
}

- (IBAction)customizeProfile:(id)sender
{
}

- (IBAction)chooseDestination:(id)sender
{
    NSSavePanel * saveFilePanel = [[NSSavePanel alloc] init];
    [saveFilePanel setCanSelectHiddenExtension: YES];
    [saveFilePanel setCanCreateDirectories: YES];
    [saveFilePanel beginSheetForDirectory:nil file:nil modalForWindow: _window modalDelegate:self didEndSelector:@selector(savePanelDidEnd:returnCode:contextInfo:) contextInfo:nil];
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
