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
#import <vlc_common.h>
#import <vlc_url.h>

@implementation VLCConvertAndSave

@synthesize MRL;

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
    // i18n, bla
}

- (void)toggleWindow
{
    [o_window makeKeyAndOrderFront: nil];
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
}

- (void)updateDropView
{

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
    NSPasteboard *o_paste = [sender draggingPasteboard];
    NSArray *o_types = [NSArray arrayWithObject: NSFilenamesPboardType];
    NSString *o_desired_type = [o_paste availableTypeFromArray:o_types];
    NSData *o_carried_data = [o_paste dataForType:o_desired_type];

    if( o_carried_data )
    {
        if( [o_desired_type isEqualToString:NSFilenamesPboardType] )
        {
            NSArray *o_values = [[o_paste propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

            if ([o_values count] > 0)
            {
                id VLCCAS = [VLCConvertAndSave sharedInstance];
                [VLCCAS setMRL: [NSString stringWithUTF8String:make_URI([[o_values objectAtIndex:0] UTF8String], NULL)]];
                [VLCCAS updateDropView];
                return YES;
            }
        }
    }
    return NO;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end