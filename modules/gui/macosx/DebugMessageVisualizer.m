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
    NSMutableArray * _msg_arr;
    NSLock * _msg_lock;
}
- (void)processReceivedlibvlcMessage:(const vlc_log_t *) item ofType: (int)i_type withStr: (char *)str;

@end

static void MsgCallback(void *data, int type, const vlc_log_t *item, const char *format, va_list ap)
{
    int canc = vlc_savecancel();
    char *str;

    if (vasprintf(&str, format, ap) == -1) {
        vlc_restorecancel(canc);
        return;
    }

    NSAutoreleasePool *_pool = [[NSAutoreleasePool alloc] init];
    [[VLCDebugMessageVisualizer sharedInstance] processReceivedlibvlcMessage: item ofType: type withStr: str];
    [_pool release];

    vlc_restorecancel(canc);
    free(str);
}

@implementation VLCDebugMessageVisualizer

static VLCDebugMessageVisualizer *_sharedMainInstance = nil;

+ (VLCDebugMessageVisualizer *)sharedInstance
{
    return _sharedMainInstance ? _sharedMainInstance : [[self alloc] init];
}

- (id)init
{
    if (_sharedMainInstance)
        [self dealloc];
    else {
        _sharedMainInstance = [super init];
        _msg_lock = [[NSLock alloc] init];
        _msg_arr = [[NSMutableArray arrayWithCapacity:600] retain];
        BOOL loaded = [NSBundle loadNibNamed:@"DebugMessageVisualizer" owner:self];
    }

    return _sharedMainInstance;
}

- (void)awakeFromNib
{
    [_msgs_panel setExcludedFromWindowsMenu: YES];
    [_msgs_panel setDelegate: self];
    [_msgs_panel setTitle: _NS("Messages")];
    [_msgs_save_btn setTitle: _NS("Save this Log...")];
    [_msgs_refresh_btn setImage: [NSImage imageNamed: NSImageNameRefreshTemplate]];
}

- (void)dealloc
{
    [_msg_arr removeAllObjects];
    [_msg_arr release];
    _msg_arr = NULL;

    [_msg_lock release];

    [super dealloc];
}

#pragma mark - UI interaction

- (void)showPanel
{
    /* subscribe to LibVLCCore's messages */
    vlc_LogSet(VLCIntf->p_libvlc, MsgCallback, NULL);

    /* show panel */
    [_msgs_panel makeKeyAndOrderFront:nil];
}

- (IBAction)updateMessagesPanel:(id)sender
{
    [self windowDidBecomeKey:nil];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    [_msgs_table reloadData];
    [_msgs_table scrollRowToVisible: [_msg_arr count] - 1];
}

- (void)windowWillClose:(NSNotification *)notification
{
    /* unsubscribe from LibVLCCore's messages */
    vlc_LogSet( VLCIntf->p_libvlc, NULL, NULL );
}

- (IBAction)saveDebugLog:(id)sender
{
    NSSavePanel * saveFolderPanel = [[NSSavePanel alloc] init];

    [saveFolderPanel setCanSelectHiddenExtension: NO];
    [saveFolderPanel setCanCreateDirectories: YES];
    [saveFolderPanel setAllowedFileTypes: [NSArray arrayWithObject:@"rtf"]];
    [saveFolderPanel setNameFieldStringValue:[NSString stringWithFormat: _NS("VLC Debug Log (%s).rtf"), VERSION_MESSAGE]];
    [saveFolderPanel beginSheetModalForWindow: _msgs_panel completionHandler:^(NSInteger returnCode) {
        if (returnCode == NSOKButton) {
            NSUInteger count = [_msg_arr count];
            NSMutableAttributedString * string = [[NSMutableAttributedString alloc] init];
            for (NSUInteger i = 0; i < count; i++)
                [string appendAttributedString: [_msg_arr objectAtIndex:i]];

            NSData *data = [string RTFFromRange:NSMakeRange(0, [string length])
                             documentAttributes:[NSDictionary dictionaryWithObject: NSRTFTextDocumentType forKey: NSDocumentTypeDocumentAttribute]];

            if ([data writeToFile: [[saveFolderPanel URL] path] atomically: YES] == NO)
                msg_Warn(VLCIntf, "Error while saving the debug log");

            [string release];
        }
    }];
    [saveFolderPanel release];
}

#pragma mark - data handling

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    if (aTableView == _msgs_table)
        return [_msg_arr count];
    return 0;
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    NSMutableAttributedString *result = NULL;

    [_msg_lock lock];
    if (rowIndex < [_msg_arr count])
        result = [_msg_arr objectAtIndex:rowIndex];
    [_msg_lock unlock];

    if (result != NULL)
        return result;
    else
        return @"";
}

- (void)processReceivedlibvlcMessage:(const vlc_log_t *) item ofType: (int)i_type withStr: (char *)str
{
    if (_msg_arr) {
        NSColor *_white = [NSColor whiteColor];
        NSColor *_red = [NSColor redColor];
        NSColor *_yellow = [NSColor yellowColor];
        NSColor *_gray = [NSColor grayColor];
        NSString * firstString, * secondString;

        NSColor * pp_color[4] = { _white, _red, _yellow, _gray };
        static const char * ppsz_type[4] = { ": ", " error: ", " warning: ", " debug: " };

        NSDictionary *_attr;
        NSMutableAttributedString *_msg_color;

        [_msg_lock lock];

        if ([_msg_arr count] > 10000) {
            [_msg_arr removeObjectAtIndex: 0];
            [_msg_arr removeObjectAtIndex: 1];
        }
        if (!item->psz_module)
            return;
        if (!str)
            return;

        firstString = [NSString stringWithFormat:@"%s%s", item->psz_module, ppsz_type[i_type]];
        secondString = [NSString stringWithFormat:@"%@%s\n", firstString, str];

        _attr = [NSDictionary dictionaryWithObject: pp_color[i_type]  forKey: NSForegroundColorAttributeName];
        _msg_color = [[NSMutableAttributedString alloc] initWithString: secondString attributes: _attr];
        _attr = [NSDictionary dictionaryWithObject: pp_color[3] forKey: NSForegroundColorAttributeName];
        [_msg_color setAttributes: _attr range: NSMakeRange(0, [firstString length])];
        [_msg_arr addObject: [_msg_color autorelease]];

        [_msg_lock unlock];
    }
}

@end
