/*****************************************************************************
 * coredialogs.m: Mac OS X Core Dialogs
 *****************************************************************************
 * Copyright (C) 2005-2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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

#import "intf.h"
#import "coredialogs.h"
#import "misc.h"

/* for the icon in our custom error panel */
#import <ApplicationServices/ApplicationServices.h>


void updateProgressPanel (void *data, const char *text, float value)
{
    @autoreleasepool {
        VLCCoreDialogProvider *dialogProvider = (__bridge VLCCoreDialogProvider *)data;

        NSString *o_txt = toNSStr(text);
        dispatch_async(dispatch_get_main_queue(), ^{
            [dialogProvider updateProgressPanelWithText: o_txt andNumber: (double)(value * 1000.)];
        });
    }
}

void destroyProgressPanel (void *data)
{
    @autoreleasepool {
        VLCCoreDialogProvider *dialogProvider = (__bridge VLCCoreDialogProvider *)data;
        if ([[NSApplication sharedApplication] isRunning])
            [dialogProvider performSelectorOnMainThread:@selector(destroyProgressPanel) withObject:nil waitUntilDone:YES];
    }
}

bool checkProgressPanel (void *data)
{
    @autoreleasepool {
        VLCCoreDialogProvider *dialogProvider = (__bridge VLCCoreDialogProvider *)data;
        return [dialogProvider progressCancelled];
    }
}

static int DialogCallback(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    @autoreleasepool {
        VLCCoreDialogProvider *dialogProvider = (__bridge VLCCoreDialogProvider *)data;
        if ([toNSStr(type) isEqualToString: @"dialog-progress-bar"]) {
            /* the progress panel needs to update itself and therefore wants special treatment within this context */
            dialog_progress_bar_t *p_dialog = (dialog_progress_bar_t *)value.p_address;

            p_dialog->pf_update = updateProgressPanel;
            p_dialog->pf_check = checkProgressPanel;
            p_dialog->pf_destroy = destroyProgressPanel;
            p_dialog->p_sys = (__bridge void *)dialogProvider;
        }

        NSValue *o_value = [NSValue valueWithPointer:value.p_address];
        [dialogProvider performEventWithObject: o_value ofType: type];

        return VLC_SUCCESS;
    }
}

@interface VLCCoreDialogProvider()
{
    ErrorWindowController *o_error_panel;
}
@end

@implementation VLCCoreDialogProvider

- (instancetype)init
{
    self = [super init];

    if (self) {
        msg_Dbg(VLCIntf, "Register dialog provider");
        [NSBundle loadNibNamed:@"CoreDialogs" owner: self];

        intf_thread_t *p_intf = VLCIntf;
        /* subscribe to various interactive dialogues */
        var_Create(p_intf, "dialog-error", VLC_VAR_ADDRESS);
        var_AddCallback(p_intf, "dialog-error", DialogCallback, (__bridge void *)self);
        var_Create(p_intf, "dialog-critical", VLC_VAR_ADDRESS);
        var_AddCallback(p_intf, "dialog-critical", DialogCallback, (__bridge void *)self);
        var_Create(p_intf, "dialog-login", VLC_VAR_ADDRESS);
        var_AddCallback(p_intf, "dialog-login", DialogCallback, (__bridge void *)self);
        var_Create(p_intf, "dialog-question", VLC_VAR_ADDRESS);
        var_AddCallback(p_intf, "dialog-question", DialogCallback, (__bridge void *)self);
        var_Create(p_intf, "dialog-progress-bar", VLC_VAR_ADDRESS);
        var_AddCallback(p_intf, "dialog-progress-bar", DialogCallback, (__bridge void *)self);
        dialog_Register(p_intf);
    }

    return self;
}

- (void)dealloc
{
    msg_Dbg(VLCIntf, "Deinitializing dialog provider");

    intf_thread_t *p_intf = VLCIntf;
    var_DelCallback(p_intf, "dialog-error", DialogCallback, (__bridge void *)self);
    var_DelCallback(p_intf, "dialog-critical", DialogCallback, (__bridge void *)self);
    var_DelCallback(p_intf, "dialog-login", DialogCallback, (__bridge void *)self);
    var_DelCallback(p_intf, "dialog-question", DialogCallback, (__bridge void *)self);
    var_DelCallback(p_intf, "dialog-progress-bar", DialogCallback, (__bridge void *)self);
    dialog_Unregister(p_intf);
}

-(void)awakeFromNib
{
    _progressCancelled = NO;
    [o_auth_login_txt setStringValue: _NS("Username")];
    [o_auth_pw_txt setStringValue: _NS("Password")];
    [o_auth_cancel_btn setTitle: _NS("Cancel")];
    [o_auth_ok_btn setTitle: _NS("OK")];
    [o_prog_cancel_btn setTitle: _NS("Cancel")];
    [o_prog_bar setUsesThreadedAnimation: YES];
}

-(void)performEventWithObject: (NSValue *)o_value ofType: (const char*)type
{
    NSString *o_type = toNSStr(type);

    if ([o_type isEqualToString: @"dialog-error"])
        [self performSelectorOnMainThread:@selector(showFatalDialog:) withObject:o_value waitUntilDone:YES];
    else if ([o_type isEqualToString: @"dialog-critical"])
        [self performSelectorOnMainThread:@selector(showFatalWaitDialog:) withObject:o_value waitUntilDone:YES];
    else if ([o_type isEqualToString: @"dialog-question"])
        [self performSelectorOnMainThread:@selector(showQuestionDialog:) withObject:o_value waitUntilDone:YES];
    else if ([o_type isEqualToString: @"dialog-login"])
        [self performSelectorOnMainThread:@selector(showLoginDialog:) withObject:o_value waitUntilDone:YES];
    else if ([o_type isEqualToString: @"dialog-progress-bar"])
        [self performSelectorOnMainThread:@selector(showProgressDialogOnMainThread:) withObject: o_value waitUntilDone:YES];
    else
        msg_Err(VLCIntf, "unhandled dialog type: '%s'", type);
}

-(void)showFatalDialog: (NSValue *)o_value
{
    dialog_fatal_t *p_dialog = [o_value pointerValue];

    [[self errorPanel] addError: toNSStr(p_dialog->title) withMsg: toNSStr(p_dialog->message)];
    [[self errorPanel] showWindow:self];
}

-(void)showFatalWaitDialog: (NSValue *)o_value
{
    dialog_fatal_t *p_dialog = [o_value pointerValue];
    NSAlert *o_alert;

    o_alert = [NSAlert alertWithMessageText: toNSStr(p_dialog->title) defaultButton: _NS("OK") alternateButton: nil otherButton: nil informativeTextWithFormat: @"%@", toNSStr(p_dialog->message)];
    [o_alert setAlertStyle: NSCriticalAlertStyle];
    [o_alert runModal];
}

-(void)showQuestionDialog: (NSValue *)o_value
{
    dialog_question_t *p_dialog = [o_value pointerValue];
    NSAlert *o_alert;
    NSInteger i_returnValue = 0;
  
    o_alert = [NSAlert alertWithMessageText: toNSStr(p_dialog->title) defaultButton: toNSStr(p_dialog->yes) alternateButton: toNSStr(p_dialog->no) otherButton: toNSStr(p_dialog->cancel) informativeTextWithFormat:@"%@", toNSStr(p_dialog->message)];
    [o_alert setAlertStyle: NSInformationalAlertStyle];
    i_returnValue = [o_alert runModal];

    if (i_returnValue == NSAlertDefaultReturn)
        p_dialog->answer = 1;
    if (i_returnValue == NSAlertAlternateReturn)
        p_dialog->answer = 2;
    if (i_returnValue == NSAlertOtherReturn)
        p_dialog->answer = 3;
}

-(void)showLoginDialog: (NSValue *)o_value
{
    dialog_login_t *p_dialog = [o_value pointerValue];
    NSInteger i_returnValue = 0;

    [o_auth_title_txt setStringValue: toNSStr(p_dialog->title)];
    [o_auth_win setTitle: toNSStr(p_dialog->title)];
    [o_auth_description_txt setStringValue: toNSStr(p_dialog->message)];
    [o_auth_login_fld setStringValue: @""];
    [o_auth_pw_fld setStringValue: @""];

    [o_auth_win center];
    i_returnValue = [NSApp runModalForWindow: o_auth_win];
    [o_auth_win close];
    if (i_returnValue)
    {
        *p_dialog->username = strdup([[o_auth_login_fld stringValue] UTF8String]);
        *p_dialog->password = strdup([[o_auth_pw_fld stringValue] UTF8String]);
    } else
        *p_dialog->username = *p_dialog->password = NULL;
}

-(IBAction)loginDialogAction:(id)sender
{
    if ([[sender title] isEqualToString: _NS("OK")])
        [NSApp stopModalWithCode: 1];
    else
        [NSApp stopModalWithCode: 0];
}

-(void)showProgressDialogOnMainThread: (NSValue *)o_value
{
    /* we work-around a Cocoa limitation here, since you cannot delay an execution
     * on the main thread within a single call */
    [self setProgressCancelled:NO];

    dialog_progress_bar_t *p_dialog = [o_value pointerValue];
    if (!p_dialog)
        return;

    [o_prog_win setTitle: toNSStr(p_dialog->title)];
    [o_prog_title_txt setStringValue: toNSStr(p_dialog->title)];

    if (p_dialog->cancel != NULL)
        [o_prog_cancel_btn setTitle: toNSStr(p_dialog->cancel)];
    else
        [o_prog_cancel_btn setTitle: _NS("Cancel")];

    [o_prog_description_txt setStringValue: toNSStr(p_dialog->message)];

    if (VLCIntf)
        [self performSelector:@selector(showProgressDialog:) withObject: o_value afterDelay:3.00];
}

-(void)showProgressDialog: (NSValue *)o_value
{
    dialog_progress_bar_t *p_dialog = [o_value pointerValue];

    if (!p_dialog || [self progressCancelled])
        return;

    [o_prog_bar setDoubleValue: 0];
    [o_prog_bar setIndeterminate: YES];
    [o_prog_bar startAnimation: self];

    [o_prog_win makeKeyAndOrderFront: self];
}

-(void)updateProgressPanelWithText: (NSString *)string andNumber: (double)d_number
{
    [o_prog_description_txt setStringValue: string];
    if (d_number > 0)
        [o_prog_bar setIndeterminate: NO];
    [o_prog_bar setDoubleValue: d_number];
}

-(void)destroyProgressPanel
{
    [self setProgressCancelled:YES];
    [o_prog_bar performSelectorOnMainThread:@selector(stopAnimation:) withObject:self waitUntilDone:YES];
    [o_prog_win performSelectorOnMainThread:@selector(close) withObject:nil waitUntilDone:YES];
}

-(IBAction)progDialogAction:(id)sender
{
    [self setProgressCancelled:YES];
}

-(id)errorPanel
{
    if (!o_error_panel)
        o_error_panel = [[ErrorWindowController alloc] init];

    return o_error_panel;
}

@end

/*****************************************************************************
 * VLCErrorPanel implementation
 *****************************************************************************/

@implementation ErrorWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"ErrorPanel"];
    if (self) {
        /* init data sources */
        o_errors = [[NSMutableArray alloc] init];
        o_icons = [[NSMutableArray alloc] init];
    }

    return self;
}

- (void)windowDidLoad
{
    /* init strings */
    [[self window] setTitle: _NS("Errors and Warnings")];
    [o_cleanup_button setTitle: _NS("Clean up")];
}

-(void)addError: (NSString *)o_error withMsg:(NSString *)o_msg
{
    /* format our string as desired */
    NSMutableAttributedString * ourError;
    ourError = [[NSMutableAttributedString alloc] initWithString:
        [NSString stringWithFormat:@"%@\n%@", o_error, o_msg]
        attributes:
        [NSDictionary dictionaryWithObject: [NSFont systemFontOfSize:11] forKey: NSFontAttributeName]];
    [ourError
        addAttribute: NSFontAttributeName
        value: [NSFont boldSystemFontOfSize:11]
        range: NSMakeRange(0, [o_error length])];
    [o_errors addObject: ourError];

    [o_icons addObject: [[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeForHFSTypeCode(kAlertStopIcon)]];

    [o_error_table reloadData];
}

-(IBAction)cleanupTable:(id)sender
{
    [o_errors removeAllObjects];
    [o_icons removeAllObjects];
    [o_error_table reloadData];
}

/*----------------------------------------------------------------------------
 * data source methods
 *---------------------------------------------------------------------------*/
- (NSInteger)numberOfRowsInTableView:(NSTableView *)theDataTable
{
    return [o_errors count];
}

- (id)tableView:(NSTableView *)theDataTable objectValueForTableColumn:
    (NSTableColumn *)theTableColumn row: (NSInteger)row
{
    if ([[theTableColumn identifier] isEqualToString: @"error_msg"])
        return [o_errors objectAtIndex:row];

    if ([[theTableColumn identifier] isEqualToString: @"icon"])
        return [o_icons objectAtIndex:row];

    return @"unknown identifier";
}

@end
