/*****************************************************************************
 * VLCLogWindowController.m: Log message window controller
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

#import "VLCLogWindowController.h"
#import "VLCMain.h"
#import <vlc_common.h>

@interface VLCLogWindowController () <NSWindowDelegate>

/* This array stores messages that are managed by the arrayController */
@property (retain) NSMutableArray *messagesArray;

/* We do not want to refresh the table for every message, as that would be very frequent when
 * there are a lot of messages, therefore we use a timer to refresh the table every now and
 * then, which is much more efficient and still fast enough for a good user experience.
 */
@property (retain) NSTimer        *refreshTimer;

/*
 * Indicates if an update is needed, which is the case when new messages were added
 * after the last firing of the update timer. It is used to prevent unnecessary
 * rearranging of the NSArrayController content.
 */
@property (atomic) BOOL           needsUpdate;

- (void)addMessage:(NSDictionary *)message;

@end

/*
 * MsgCallback: Callback triggered by the core once a new debug message is
 * ready to be displayed. We store everything in a NSArray in our Cocoa part
 * of this file.
 */
static void MsgCallback(void *data, int type, const vlc_log_t *item, const char *format, va_list ap)
{
    @autoreleasepool {
        char *msg;
        VLCLogWindowController *controller = (__bridge VLCLogWindowController*)data;
        static NSString *types[4] = { @"info", @"error", @"warning", @"debug" };

        if (vasprintf(&msg, format, ap) == -1) {
            return;
        }

        if (!item->psz_module || !msg) {
            free(msg);
            return;
        }

        NSString *position = [NSString stringWithFormat:@"%s:%i", item->file, item->line];

        NSDictionary *messageDict = @{
                                      @"type"       : types[type],
                                      @"message"    : toNSStr(msg),
                                      @"component"  : toNSStr(item->psz_module),
                                      @"position"   : position,
                                      @"func"       : toNSStr(item->func)
                                      };
        [controller addMessage:messageDict];
        free(msg);
    }
}

@implementation VLCLogWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"LogMessageWindow"];
    if (self) {
        _messagesArray = [[NSMutableArray alloc] initWithCapacity:1000];
    }
    return self;
}

- (void)dealloc
{
    if (getIntf())
        vlc_LogSet( getIntf()->obj.libvlc, NULL, NULL );
}

- (void)windowDidLoad
{
    [self.window setExcludedFromWindowsMenu:YES];
    [self.window setDelegate:self];
    [self.window setTitle:_NS("Messages")];
    [self.window setLevel:NSModalPanelWindowLevel];

#define setupButton(target, title, desc)                                              \
    [target accessibilitySetOverrideValue:title                                       \
                             forAttribute:NSAccessibilityTitleAttribute];             \
    [target accessibilitySetOverrideValue:desc                                        \
                             forAttribute:NSAccessibilityDescriptionAttribute];       \
    [target setToolTip:desc];

    setupButton(_saveButton,
                _NS("Save log"),
                _NS("Click to save the debug log to a file."));
    setupButton(_refreshButton,
                _NS("Refresh log"),
                _NS("Click to frefresh the log output."));
    setupButton(_clearButton,
                _NS("Clear log"),
                _NS("Click to clear the log output."));
    setupButton(_toggleDetailsButton,
                _NS("Toggle details"),
                _NS("Click to show/hide details about a log message."));

#undef setupButton
}

- (void)showWindow:(id)sender
{
    // Do nothing if window is already visible
    if ([self.window isVisible]) {
        return [super showWindow:sender];
    }

    // Subscribe to LibVLCCore's messages
    vlc_LogSet(getIntf()->obj.libvlc, MsgCallback, (__bridge void*)self);
    _refreshTimer = [NSTimer scheduledTimerWithTimeInterval:0.3
                                                     target:self
                                                   selector:@selector(updateArrayController:)
                                                   userInfo:nil
                                                    repeats:YES];
    return [super showWindow:sender];
}

- (void)windowWillClose:(NSNotification *)notification
{
    // Unsubscribe from LibVLCCore's messages
    vlc_LogSet( getIntf()->obj.libvlc, NULL, NULL );

    // Remove all messages
    [self removeAllMessages];

    // Invalidate timer
    [_refreshTimer invalidate];
    _refreshTimer = nil;
}

/**
 Called by the timer to re-sync the array controller with the backing array
 */
- (void)updateArrayController:(NSTimer *)timer
{
    if (_needsUpdate)
        [_arrayController rearrangeObjects];

    _needsUpdate = NO;
}

#pragma mark -
#pragma mark Delegate methods

/*
 * Called when a row is added to the table
 * We use this to set the correct background color for the row, depending on the
 * message type.
 */
- (void)tableView:(NSTableView *)tableView didAddRowView:(NSTableRowView *)rowView forRow:(NSInteger)row
{
    // Initialize background colors
    static NSDictionary *colors = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        colors = @{
                   @"info"     : [NSColor colorWithCalibratedRed:0.65 green:0.91 blue:1.0 alpha:0.7],
                   @"error"    : [NSColor colorWithCalibratedRed:1.0 green:0.49 blue:0.45 alpha:0.5],
                   @"warning"  : [NSColor colorWithCalibratedRed:1.0 green:0.88 blue:0.45 alpha:0.7],
                   @"debug"    : [NSColor colorWithCalibratedRed:0.96 green:0.96 blue:0.96 alpha:0.5]
                   };
    });

    // Lookup color for message type
    NSDictionary *message = [[_arrayController arrangedObjects] objectAtIndex:row];
    rowView.backgroundColor = [colors objectForKey:[message objectForKey:@"type"]];
}

- (void)splitViewDidResizeSubviews:(NSNotification *)notification
{
    if ([_splitView isSubviewCollapsed:_detailView]) {
        [_toggleDetailsButton setState:NSOffState];
    } else {
        [_toggleDetailsButton setState:NSOnState];
    }
}

#pragma mark -
#pragma mark UI actions

/* Save debug log to file action
 */
- (IBAction)saveDebugLog:(id)sender
{
    NSSavePanel * saveFolderPanel = [[NSSavePanel alloc] init];

    [saveFolderPanel setCanSelectHiddenExtension: NO];
    [saveFolderPanel setCanCreateDirectories: YES];
    [saveFolderPanel setAllowedFileTypes: [NSArray arrayWithObject:@"txt"]];
    [saveFolderPanel setNameFieldStringValue:[NSString stringWithFormat: _NS("VLC Debug Log (%s).txt"), VERSION_MESSAGE]];
    [saveFolderPanel beginSheetModalForWindow: self.window completionHandler:^(NSInteger returnCode) {
        if (returnCode != NSOKButton) {
            return;
        }
        NSMutableString *string = [[NSMutableString alloc] init];

        for (NSDictionary *line in _messagesArray) {
            NSString *message = [NSString stringWithFormat:@"%@ %@ %@\n",
                                 [line objectForKey:@"component"],
                                 [line objectForKey:@"type"],
                                 [line objectForKey:@"message"]];
            [string appendString:message];
        }
        NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
        if ([data writeToFile: [[saveFolderPanel URL] path] atomically: YES] == NO)
            msg_Warn(getIntf(), "Error while saving the debug log");
    }];
}

/* Clear log action
 */
- (IBAction)clearLog:(id)sender
{
    // Unregister handler
    vlc_LogSet(getIntf()->obj.libvlc, NULL, NULL);

    // Remove all messages
    [self removeAllMessages];

    // Reregister handler, to write new header to log
    vlc_LogSet(getIntf()->obj.libvlc, MsgCallback, (__bridge void*)self);
}

/* Refresh log action
 */
- (IBAction)refreshLog:(id)sender
{
    [_arrayController rearrangeObjects];
    [_messageTable scrollToEndOfDocument:self];
}

/* Show/Hide details action
 */
- (IBAction)toggleDetails:(id)sender
{
    if ([_splitView isSubviewCollapsed:_detailView]) {
        [_detailView setHidden:NO];
    } else {
        [_detailView setHidden:YES];
    }
}

/* Called when the user hits CMD + C or copy is clicked in the edit menu
 */
- (void)copy:(id)sender {
    NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
    [pasteBoard clearContents];
    for (NSDictionary *line in [_arrayController selectedObjects]) {
        NSString *message = [NSString stringWithFormat:@"%@ %@: %@",
                             [line objectForKey:@"component"],
                             [line objectForKey:@"type"],
                             [line objectForKey:@"message"]];
        [pasteBoard writeObjects:@[message]];
    }
}

#pragma mark -
#pragma mark UI validation

/* Validate the copy menu item
 */
- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)anItem
{
    SEL theAction = [anItem action];

    if (theAction == @selector(copy:)) {
        if ([[_arrayController selectedObjects] count] > 0) {
            return YES;
        }
        return NO;
    }
    /* Indicate that we handle the validation method,
     * even if we don’t implement the action
     */
    return YES;
}

#pragma mark -
#pragma mark Data handling

/** 
 Adds a message, it is only added visibly to the table on the next firing
 of the update timer.
 */
- (void)addMessage:(NSDictionary *)messageDict
{
    @synchronized (_messagesArray) {
        if ([_messagesArray count] > 1000000) {
            [_messagesArray removeObjectsInRange:NSMakeRange(0, 2)];
        }
        [_messagesArray addObject:messageDict];

        _needsUpdate = YES;
    }
    
}

/**
 Clears all messages in the message table by removing all items from the array and
 calling `rearrangeObjects` on the array controller.
 */
- (void)removeAllMessages
{
    [_messagesArray removeAllObjects];
    [_arrayController rearrangeObjects];
}

@end
