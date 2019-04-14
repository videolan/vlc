/*****************************************************************************
 * misc.m: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003-2015 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "misc.h"

#import <vlc_common.h>
#import <vlc_actions.h>

#import "extensions/NSString+Helpers.h"
#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"
#import "menus/VLCMainMenu.h"
#import "windows/mainwindow/VLCControlsBarCommon.h"

/*****************************************************************************
 * VLCDragDropView
 *****************************************************************************/

@implementation VLCDropDisabledImageView

- (void)awakeFromNib
{
    [self unregisterDraggedTypes];
}

@end

/*****************************************************************************
 * VLCDragDropView
 *****************************************************************************/

@interface VLCDragDropView()
{
    bool b_activeDragAndDrop;
}
@end

@implementation VLCDragDropView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // default value
        [self setDrawBorder:YES];
    }

    return self;
}

- (void)enablePlaylistItems
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, @"VLCPlaylistItemPboardType", nil]];
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (void)dealloc
{
    [self unregisterDraggedTypes];
}

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric) {
        b_activeDragAndDrop = YES;
        [self setNeedsDisplay:YES];

        return NSDragOperationCopy;
    }

    return NSDragOperationNone;
}

- (void)draggingEnded:(id < NSDraggingInfo >)sender
{
    b_activeDragAndDrop = NO;
    [self setNeedsDisplay:YES];
}

- (void)draggingExited:(id < NSDraggingInfo >)sender
{
    b_activeDragAndDrop = NO;
    [self setNeedsDisplay:YES];
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    BOOL b_returned = NO;

    if (_dropHandler && [_dropHandler respondsToSelector:@selector(performDragOperation:)])
        b_returned = [_dropHandler performDragOperation:sender];
    // default
        // FIXME: implement drag and drop _on_ new playlist
//        b_returned = [[[VLCMain sharedInstance] playlist] performDragOperation:sender];

    [self setNeedsDisplay:YES];
    return b_returned;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect
{
    if ([self drawBorder] && b_activeDragAndDrop) {
        NSRect frameRect = [self bounds];

        [[NSColor selectedControlColor] set];
        NSFrameRectWithWidthUsingOperation(frameRect, 2., NSCompositeSourceOver);
    }

    [super drawRect:dirtyRect];
}

@end

@interface PositionFormatter()
{
    NSCharacterSet *o_forbidden_characters;
}
@end

@implementation PositionFormatter

- (id)init
{
    self = [super init];
    NSMutableCharacterSet *nonNumbers = [[[NSCharacterSet decimalDigitCharacterSet] invertedSet] mutableCopy];
    [nonNumbers removeCharactersInString:@"-:"];
    o_forbidden_characters = [nonNumbers copy];

    return self;
}

- (NSString*)stringForObjectValue:(id)obj
{
    if([obj isKindOfClass:[NSString class]])
        return obj;
    if([obj isKindOfClass:[NSNumber class]])
        return [obj stringValue];

    return nil;
}

- (BOOL)getObjectValue:(id*)obj forString:(NSString*)string errorDescription:(NSString**)error
{
    *obj = [string copy];
    return YES;
}

- (BOOL)isPartialStringValid:(NSString*)partialString newEditingString:(NSString**)newString errorDescription:(NSString**)error
{
    if ([partialString rangeOfCharacterFromSet:o_forbidden_characters options:NSLiteralSearch].location != NSNotFound) {
        return NO;
    } else {
        return YES;
    }
}

@end

@implementation NSView (EnableSubviews)

- (void)enableSubviews:(BOOL)b_enable
{
    for (NSView *o_view in [self subviews]) {
        [o_view enableSubviews:b_enable];

        // enable NSControl
        if ([o_view respondsToSelector:@selector(setEnabled:)]) {
            [(NSControl *)o_view setEnabled:b_enable];
        }
        // also "enable / disable" text views
        if ([o_view respondsToSelector:@selector(setTextColor:)]) {
            if (b_enable == NO) {
                [(NSTextField *)o_view setTextColor:[NSColor disabledControlTextColor]];
            } else {
                [(NSTextField *)o_view setTextColor:[NSColor controlTextColor]];
            }
        }

    }
}

@end

/*****************************************************************************
 * VLCByteCountFormatter addition
 *****************************************************************************/

@implementation VLCByteCountFormatter

+ (NSString *)stringFromByteCount:(long long)byteCount countStyle:(NSByteCountFormatterCountStyle)countStyle
{
    // Use native implementation on >= mountain lion
    Class byteFormatterClass = NSClassFromString(@"NSByteCountFormatter");
    if (byteFormatterClass && [byteFormatterClass respondsToSelector:@selector(stringFromByteCount:countStyle:)]) {
        return [byteFormatterClass stringFromByteCount:byteCount countStyle:NSByteCountFormatterCountStyleFile];
    }

    float devider = 0.;
    float returnValue = 0.;
    NSString *suffix;

    NSNumberFormatter *theFormatter = [[NSNumberFormatter alloc] init];
    [theFormatter setLocale:[NSLocale currentLocale]];
    [theFormatter setAllowsFloats:YES];

    NSString *returnString = @"";

    if (countStyle != NSByteCountFormatterCountStyleDecimal)
        devider = 1024.;
    else
        devider = 1000.;

    if (byteCount < 1000) {
        returnValue = byteCount;
        suffix = _NS("B");
        [theFormatter setMaximumFractionDigits:0];
        goto end;
    }

    if (byteCount < 1000000) {
        returnValue = byteCount / devider;
        suffix = _NS("KB");
        [theFormatter setMaximumFractionDigits:0];
        goto end;
    }

    if (byteCount < 1000000000) {
        returnValue = byteCount / devider / devider;
        suffix = _NS("MB");
        [theFormatter setMaximumFractionDigits:1];
        goto end;
    }

    [theFormatter setMaximumFractionDigits:2];
    if (byteCount < 1000000000000) {
        returnValue = byteCount / devider / devider / devider;
        suffix = _NS("GB");
        goto end;
    }

    returnValue = byteCount / devider / devider / devider / devider;
    suffix = _NS("TB");

end:
    returnString = [NSString stringWithFormat:@"%@ %@", [theFormatter stringFromNumber:[NSNumber numberWithFloat:returnValue]], suffix];

    return returnString;
}

@end
