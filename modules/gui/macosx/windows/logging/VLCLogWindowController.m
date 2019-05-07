/*****************************************************************************
 * VLCLogWindowController.m: Log message window controller
 *****************************************************************************
 * Copyright (C) 2004-2013 VLC authors and VideoLAN
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

#import <vlc_common.h>

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "windows/logging/VLCLogMessage.h"

@interface VLCLogWindowController () <NSWindowDelegate>

/* This array stores messages that are managed by the arrayController */
@property (retain) NSMutableArray *messagesArray;

/* This array stores messages before they are added to the messagesArray on refresh */
@property (retain) NSMutableArray *messageBuffer;

/* We do not want to refresh the table for every message, as that would be very frequent when
 * there are a lot of messages, therefore we use a timer to refresh the table with new data
 * from the messageBuffer every now and then, which is much more efficient and still fast
 * enough for a good user experience
 */
@property (retain) NSTimer        *refreshTimer;

- (void)addMessage:(VLCLogMessage *)message;

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

        if (vasprintf(&msg, format, ap) == -1) {
            return;
        }
        
        [controller addMessage:[VLCLogMessage logMessage:msg
                                                    type:type
                                                    info:item]];
        free(msg);
    }
}

static const struct vlc_logger_operations log_ops = { MsgCallback, NULL };

@implementation VLCLogWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"LogMessageWindow"];
    if (self) {
        _messagesArray = [[NSMutableArray alloc] initWithCapacity:500];
        _messageBuffer = [[NSMutableArray alloc] initWithCapacity:100];
    }
    return self;
}

- (void)dealloc
{
    if (getIntf())
        vlc_LogSet( vlc_object_instance(getIntf()), NULL, NULL );
}

- (void)windowDidLoad
{
    [self.window setExcludedFromWindowsMenu:YES];
    [self.window setDelegate:self];
    [self.window setTitle:_NS("Messages")];

#define setupButton(target, title, desc)                                              \
    target.accessibilityTitle = title;                                                \
    target.accessibilityLabel = desc;                                                 \
    [target setToolTip:desc];

    setupButton(_saveButton,
                _NS("Save log"),
                _NS("Save the debug log to a file"));
    setupButton(_refreshButton,
                _NS("Refresh log"),
                _NS("Refresh the log output"));
    setupButton(_clearButton,
                _NS("Clear log"),
                _NS("Clear the log output"));
    setupButton(_toggleDetailsButton,
                _NS("Toggle details"),
                _NS("Show/hide details about a log message"));

#undef setupButton
}

- (void)showWindow:(id)sender
{
    // Do nothing if window is already visible
    if ([self.window isVisible]) {
        return [super showWindow:sender];
    }

    // Subscribe to LibVLCCore's messages
    vlc_LogSet(vlc_object_instance(getIntf()), &log_ops, (__bridge void*)self);
    _refreshTimer = [NSTimer scheduledTimerWithTimeInterval:0.3
                                                     target:self
                                                   selector:@selector(appendMessageBuffer)
                                                   userInfo:nil
                                                    repeats:YES];
    return [super showWindow:sender];
}

- (void)windowWillClose:(NSNotification *)notification
{
    // Unsubscribe from LibVLCCore's messages
    vlc_LogSet( vlc_object_instance(getIntf()), NULL, NULL );

    // Remove all messages
    [self clearMessageBuffer];
    [self clearMessageTable];

    // Invalidate timer
    [_refreshTimer invalidate];
    _refreshTimer = nil;
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
                   @(VLC_MSG_INFO): [NSColor colorWithCalibratedRed:0.65 green:0.91 blue:1.0 alpha:0.7],
                   @(VLC_MSG_ERR) : [NSColor colorWithCalibratedRed:1.0 green:0.49 blue:0.45 alpha:0.5],
                   @(VLC_MSG_WARN): [NSColor colorWithCalibratedRed:1.0 green:0.88 blue:0.45 alpha:0.7],
                   @(VLC_MSG_DBG) : [NSColor colorWithCalibratedRed:0.96 green:0.96 blue:0.96 alpha:0.5]
                   };
    });

    // Lookup color for message type
    VLCLogMessage *message = [[_arrayController arrangedObjects] objectAtIndex:row];
    rowView.backgroundColor = [colors objectForKey:@(message.type)];
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
        if (returnCode != NSModalResponseOK) {
            return;
        }
        NSMutableString *string = [[NSMutableString alloc] init];

        for (VLCLogMessage *message in self->_messagesArray) {
            [string appendFormat:@"%@\r\n", message.fullMessage];
        }
        NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
        if ([data writeToFile:[[saveFolderPanel URL] path] atomically:YES] == NO)
            msg_Warn(getIntf(), "Error while saving the debug log");
    }];
}

/* Clear log action
 */
- (IBAction)clearLog:(id)sender
{
    // Unregister handler
    vlc_LogSet(vlc_object_instance(getIntf()), NULL, NULL);

    // Remove all messages
    [self clearMessageBuffer];
    [self clearMessageTable];

    // Reregister handler, to write new header to log
    vlc_LogSet(vlc_object_instance(getIntf()), &log_ops, (__bridge void*)self);
}

/* Refresh log action
 */
- (IBAction)refreshLog:(id)sender
{
    [self appendMessageBuffer];
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
- (void) copy:(id)sender {
    NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
    [pasteBoard clearContents];
    for (VLCLogMessage *message in [_arrayController selectedObjects]) {
        [pasteBoard writeObjects:@[message.fullMessage]];
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
 Adds a message to the messageBuffer, it does not has to be called from the main thread, as
 items are only added to the messageArray on refresh.
 */
- (void)addMessage:(VLCLogMessage *)message
{
    if (!message)
        return;

    @synchronized (_messageBuffer) {
        [_messageBuffer addObject:message];
    }
}

/**
 Clears the message buffer
 */
- (void)clearMessageBuffer
{
    @synchronized (_messageBuffer) {
        [_messageBuffer removeAllObjects];
    }
}

/**
 Clears all messages in the message table by removing all items from the messagesArray
 */
- (void)clearMessageTable
{
    [self willChangeValueForKey:@"messagesArray"];
    [_messagesArray removeAllObjects];
    [self didChangeValueForKey:@"messagesArray"];}

/**
 Appends all messages from the buffer to the messagesArray and clears the buffer
 */
- (void)appendMessageBuffer
{
    static const NSUInteger limit = 1000000;

    [self willChangeValueForKey:@"messagesArray"];
    @synchronized (_messageBuffer) {
        [_messagesArray addObjectsFromArray:_messageBuffer];
        [_messageBuffer removeAllObjects];
    }

    if ([_messagesArray count] > limit) {
        [_messagesArray removeObjectsInRange:NSMakeRange(0, _messagesArray.count - limit)];
    }
    [self didChangeValueForKey:@"messagesArray"];
}

@end
