/*****************************************************************************
 * DebugMessageVisualizer.m: Mac OS X interface crash reporter
 *****************************************************************************
 * Copyright (C) 2004-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
 *          Derk-Jan Hartman <hartman at videolan.org>
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

#import "DebugMessageVisualizer.h"
#import "intf.h"
#import <vlc_common.h>

static void MsgCallback(void *data, int type, const vlc_log_t *item, const char *format, va_list ap);

/*****************************************************************************
 * MsgCallback: Callback triggered by the core once a new debug message is
 * ready to be displayed. We store everything in a NSArray in our Cocoa part
 * of this file.
 *****************************************************************************/

@interface VLCDebugMessageVisualizer () <NSWindowDelegate>
{
    NSMutableArray * _messageArray;
}
- (void)appendMessage:(NSMutableAttributedString *) message;

@end

static void MsgCallback(void *data, int type, const vlc_log_t *item, const char *format, va_list ap)
{
    @autoreleasepool {

        VLCDebugMessageVisualizer *visualizer = (__bridge VLCDebugMessageVisualizer*)data;

        int canc = vlc_savecancel();
        char *str;
        if (vasprintf(&str, format, ap) == -1) {
            vlc_restorecancel(canc);
            return;
        }

        if (!item->psz_module)
            return;
        if (!str)
            return;

        NSColor *_white = [NSColor whiteColor];
        NSColor *_red = [NSColor redColor];
        NSColor *_yellow = [NSColor yellowColor];
        NSColor *_gray = [NSColor grayColor];

        NSColor * pp_color[4] = { _white, _red, _yellow, _gray };
        static const char * ppsz_type[4] = { ": ", " error: ", " warning: ", " debug: " };


        NSString *firstString = [NSString stringWithFormat:@"%s%s", item->psz_module, ppsz_type[type]];
        NSString *secondString = [NSString stringWithFormat:@"%@%s\n", firstString, str];

        NSDictionary *colorAttrib = [NSDictionary dictionaryWithObject: pp_color[type]  forKey: NSForegroundColorAttributeName];
        NSMutableAttributedString *coloredMsg = [[NSMutableAttributedString alloc] initWithString: secondString attributes: colorAttrib];
        colorAttrib = [NSDictionary dictionaryWithObject: pp_color[3] forKey: NSForegroundColorAttributeName];
        [coloredMsg setAttributes: colorAttrib range: NSMakeRange(0, [firstString length])];

        [visualizer performSelectorOnMainThread:@selector(appendMessage:) withObject:coloredMsg waitUntilDone:NO];

        vlc_restorecancel(canc);
        free(str);
    }
}

@implementation VLCDebugMessageVisualizer

- (id)init
{
    self = [super initWithWindowNibName:@"DebugMessageVisualizer"];
    if (self) {
        _messageArray = [NSMutableArray arrayWithCapacity:600];
    }
    return self;
}

- (void)dealloc
{
    vlc_LogSet( getIntf()->obj.libvlc, NULL, NULL );
}

- (void)windowDidLoad
{
    [self.window setExcludedFromWindowsMenu: YES];
    [self.window setDelegate: self];
    [self.window setTitle: _NS("Messages")];
    [_saveButton setTitle: _NS("Save this Log...")];
    [_clearButton setTitle:_NS("Clear")];
    [_refreshButton setImage: [NSImage imageNamed: NSImageNameRefreshTemplate]];
}

#pragma mark - UI interaction

- (void)showWindow:(id)sender
{
    /* subscribe to LibVLCCore's messages */
    vlc_LogSet(getIntf()->obj.libvlc, MsgCallback, (__bridge void*)self);

    [super showWindow:sender];
}

- (IBAction)updateMessagesPanel:(id)sender
{
    [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowDidBecomeKeyNotification object:self.window];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    [_messageTable reloadData];
    [_messageTable scrollRowToVisible: [_messageArray count] - 1];
}

- (void)windowWillClose:(NSNotification *)notification
{
    /* unsubscribe from LibVLCCore's messages */
    vlc_LogSet( getIntf()->obj.libvlc, NULL, NULL );
    [_messageArray removeAllObjects];
}

- (IBAction)saveDebugLog:(id)sender
{
    NSSavePanel * saveFolderPanel = [[NSSavePanel alloc] init];

    [saveFolderPanel setCanSelectHiddenExtension: NO];
    [saveFolderPanel setCanCreateDirectories: YES];
    [saveFolderPanel setAllowedFileTypes: [NSArray arrayWithObject:@"txt"]];
    [saveFolderPanel setNameFieldStringValue:[NSString stringWithFormat: _NS("VLC Debug Log (%s).txt"), VERSION_MESSAGE]];
    [saveFolderPanel beginSheetModalForWindow: self.window completionHandler:^(NSInteger returnCode) {
        if (returnCode == NSOKButton) {
            NSUInteger count = [_messageArray count];
            NSMutableString *string = [[NSMutableString alloc] init];
            for (NSUInteger i = 0; i < count; i++)
                [string appendString: [[_messageArray objectAtIndex:i] string]];

            NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
            if ([data writeToFile: [[saveFolderPanel URL] path] atomically: YES] == NO)
                msg_Warn(getIntf(), "Error while saving the debug log");
        }
    }];
}

- (IBAction)clearLog:(id)sender
{
    [_messageArray removeAllObjects];

    // Reregister handler, to write new header to log
    vlc_LogSet(getIntf()->obj.libvlc, NULL, NULL);
    vlc_LogSet(getIntf()->obj.libvlc, MsgCallback, (__bridge void*)self);

    [_messageTable reloadData];
}

#pragma mark - data handling

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [_messageArray count];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    return [_messageArray objectAtIndex:rowIndex];
}

- (void)appendMessage:(NSMutableAttributedString *) message
{
    if ([_messageArray count] > 1000000) {
        [_messageArray removeObjectAtIndex: 0];
        [_messageArray removeObjectAtIndex: 1];
    }

    [_messageArray addObject:message];
}

@end
