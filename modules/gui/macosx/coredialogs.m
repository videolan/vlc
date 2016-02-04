/*****************************************************************************
 * coredialogs.m: Mac OS X Core Dialogs
 *****************************************************************************
 * Copyright (C) 2005-2016 VLC authors and VideoLAN
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

@interface VLCCoreDialogProvider ()

- (void)displayLoginDialogWithID:(vlc_dialog_id *)p_id
                           title:(const char *)psz_title
                     description:(const char *)psz_text
                 defaultUserName:(const char *)psz_default_username
                      askToStore:(bool )b_ask_store;

- (void)displayProgressDialogWithID:(vlc_dialog_id *)p_id
                              title:(const char *)psz_title
                        description:(const char *)psz_text
                    isIndeterminate:(bool)b_indeterminate
                           position:(float)f_position
                        cancelTitle:(const char *)psz_cancel;

- (void)updateDisplayedProgressDialogWithID:(vlc_dialog_id *)p_id
                                      value:(float)f_value
                                description:(const char *)psz_text;

@end


static void displayErrorCallback(const char *psz_title,
                                 const char *psz_text,
                                 void *p_data)
{
    @autoreleasepool {
        NSAlert *alert = [NSAlert alertWithMessageText:toNSStr(psz_title)
                                         defaultButton:_NS("OK")
                                       alternateButton:nil
                                           otherButton:nil
                             informativeTextWithFormat:@"%@", toNSStr(psz_text)];
        [alert setAlertStyle:NSCriticalAlertStyle];
        [alert runModal];
    }
}

static void displayLoginCallback(vlc_dialog_id *p_id,
                                 const char *psz_title,
                                 const char *psz_text,
                                 const char *psz_default_username,
                                 bool b_ask_store,
                                 void *p_data)
{
    @autoreleasepool {
        VLCCoreDialogProvider *dialogProvider = (__bridge VLCCoreDialogProvider *)p_data;
        [dialogProvider displayLoginDialogWithID:p_id
                                           title:psz_title
                                     description:psz_text
                                 defaultUserName:psz_default_username
                                      askToStore:b_ask_store];
    }
}

static void displayQuestionCallback(vlc_dialog_id *p_id,
                                    const char *psz_title,
                                    const char *psz_text,
                                    vlc_dialog_question_type i_type,
                                    const char *psz_cancel,
                                    const char *psz_action1,
                                    const char *psz_action2,
                                    void *p_data)
{
    @autoreleasepool {
        NSAlert *alert = [NSAlert alertWithMessageText:toNSStr(psz_title)
                                         defaultButton:toNSStr(psz_action1)
                                       alternateButton:toNSStr(psz_action2)
                                           otherButton:toNSStr(psz_cancel)
                             informativeTextWithFormat:@"%@", toNSStr(psz_text)];

        switch (i_type) {
            case VLC_DIALOG_QUESTION_WARNING:
                [alert setAlertStyle:NSWarningAlertStyle];
                break;
            case VLC_DIALOG_QUESTION_CRITICAL:
                [alert setAlertStyle:NSCriticalAlertStyle];
                break;
            default:
                [alert setAlertStyle:NSInformationalAlertStyle];
                break;
        }

        NSInteger returnValue = [alert runModal];
        switch (returnValue) {
            case NSAlertAlternateReturn:
                vlc_dialog_id_post_action(p_id, 2);
                break;

            case NSAlertOtherReturn:
                vlc_dialog_id_post_action(p_id, 3);
                break;

            default:
                vlc_dialog_id_post_action(p_id, 1);
                break;
        }
    }
}

static void displayProgressCallback(vlc_dialog_id *p_id,
                                    const char *psz_title,
                                    const char *psz_text,
                                    bool b_indeterminate,
                                    float f_position,
                                    const char *psz_cancel,
                                    void *p_data)
{
    @autoreleasepool {
        VLCCoreDialogProvider *dialogProvider = (__bridge VLCCoreDialogProvider *)p_data;
        [dialogProvider displayProgressDialogWithID:p_id
                                              title:psz_title
                                        description:psz_text
                                    isIndeterminate:b_indeterminate
                                           position:f_position
                                        cancelTitle:psz_cancel];
    }
}

static void cancelCallback(vlc_dialog_id *p_id,
                           void *p_data)
{
    @autoreleasepool {
        [NSApp stopModalWithCode: 0];
    }
}

static void updateProgressCallback(vlc_dialog_id *p_id,
                                   float f_value,
                                   const char *psz_text,
                                   void *p_data)
{
    @autoreleasepool {
        VLCCoreDialogProvider *dialogProvider = (__bridge VLCCoreDialogProvider *)p_data;
        [dialogProvider updateDisplayedProgressDialogWithID:p_id
                                                      value:f_value
                                                description:psz_text];
    }
}

@implementation VLCCoreDialogProvider

- (instancetype)init
{
    self = [super init];

    if (self) {
        msg_Dbg(getIntf(), "Register dialog provider");
        [NSBundle loadNibNamed:@"CoreDialogs" owner: self];

        intf_thread_t *p_intf = getIntf();
        /* subscribe to various interactive dialogues */

        const vlc_dialog_cbs cbs = {
            displayErrorCallback,
            displayLoginCallback,
            displayQuestionCallback,
            displayProgressCallback,
            cancelCallback,
            updateProgressCallback
        };

        vlc_dialog_provider_set_callbacks(p_intf, &cbs, (__bridge void *)self);
    }

    return self;
}

- (void)dealloc
{
    msg_Dbg(getIntf(), "Deinitializing dialog provider");

    intf_thread_t *p_intf = getIntf();
    vlc_dialog_provider_set_callbacks(p_intf, NULL, NULL);
}

-(void)awakeFromNib
{
    _progressCancelled = NO;
    [authenticationLoginLabel setStringValue: _NS("Username")];
    [authenticationPasswordLabel setStringValue: _NS("Password")];
    [authenticationCancelButton setTitle: _NS("Cancel")];
    [authenticationOkButton setTitle: _NS("OK")];
    [authenticationStorePasswordCheckbox setTitle:_NS("Remember")];

    [progressCancelButton setTitle: _NS("Cancel")];
    [progressIndicator setUsesThreadedAnimation: YES];
}

- (void)displayLoginDialogWithID:(vlc_dialog_id *)p_id
                           title:(const char *)psz_title
                     description:(const char *)psz_text
                 defaultUserName:(const char *)psz_default_username
                      askToStore:(bool )b_ask_store
{
    [authenticationTitleLabel setStringValue:toNSStr(psz_title)];
    authenticationWindow.title = authenticationTitleLabel.stringValue;
    [authenticationDescriptionLabel setStringValue:toNSStr(psz_text)];

    [authenticationLoginTextField setStringValue:toNSStr(psz_default_username)];
    [authenticationPasswordTextField setStringValue:@""];

    authenticationStorePasswordCheckbox.hidden = !b_ask_store;
    authenticationStorePasswordCheckbox.state = NSOffState;

    [authenticationWindow center];
    NSInteger returnValue = [NSApp runModalForWindow:authenticationWindow];
    [authenticationWindow close];

    NSString *username = authenticationLoginTextField.stringValue;
    NSString *password = authenticationPasswordTextField.stringValue;

    vlc_dialog_id_post_login(p_id,
                             username ? [username UTF8String] : NULL,
                             password ? [password UTF8String] : NULL,
                             authenticationStorePasswordCheckbox.state == NSOnState);
}

- (IBAction)authenticationDialogAction:(id)sender
{
    if ([[sender title] isEqualToString: _NS("OK")])
        [NSApp stopModalWithCode: 1];
    else
        [NSApp stopModalWithCode: 0];
}

- (void)displayProgressDialogWithID:(vlc_dialog_id *)p_id
                              title:(const char *)psz_title
                        description:(const char *)psz_text
                    isIndeterminate:(bool)b_indeterminate
                           position:(float)f_position
                        cancelTitle:(const char *)psz_cancel
{
    progressTitleLabel.stringValue = toNSStr(psz_title);
    progressWindow.title = progressTitleLabel.stringValue;

    progressDescriptionLabel.stringValue = toNSStr(psz_text);

    progressIndicator.indeterminate = b_indeterminate;
    progressIndicator.doubleValue = f_position;

    if (psz_cancel) {
        progressCancelButton.title = toNSStr(psz_cancel);
    } else {
        progressCancelButton.title = _NS("Cancel");
    }

    [progressIndicator startAnimation:self];

    [progressWindow center];
    [NSApp runModalForWindow:progressWindow];
    [progressWindow close];

    if (p_id != NULL) {
        vlc_dialog_id_dismiss(p_id);
    }
    p_id = NULL;
}

- (void)updateDisplayedProgressDialogWithID:(vlc_dialog_id *)p_id
                                      value:(float)f_value
                                description:(const char *)psz_text
{
    if (!progressIndicator.indeterminate) {
        progressIndicator.doubleValue = f_value;
        progressDescriptionLabel.stringValue = toNSStr(psz_text);
    }
}

- (IBAction)progressDialogAction:(id)sender
{
    [NSApp stopModalWithCode: 0];
}

@end
