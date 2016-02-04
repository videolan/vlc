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

static void displayErrorCallback(const char *psz_title, const char *psz_text, void * p_data)
{
    @autoreleasepool {
        VLCCoreDialogProvider *dialogProvider = (__bridge VLCCoreDialogProvider *)p_data;
        NSAlert *alert = [NSAlert alertWithMessageText: toNSStr(psz_title) defaultButton: _NS("OK") alternateButton: nil otherButton: nil informativeTextWithFormat: @"%@", toNSStr(psz_text)];
        [alert setAlertStyle: NSCriticalAlertStyle];
        [alert runModal];
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

/*        const vlc_dialog_cbs cbs = {
            displayErrorCallback,
            displayLoginCallback,
            displayQuestionCallback,
            displayProgressCallback,
            cancelCallback,
            updateProgressCallback
        };*/

        const vlc_dialog_cbs cbs = {
            displayErrorCallback,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
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

    [progressCancelButton setTitle: _NS("Cancel")];
    [progressIndicator setUsesThreadedAnimation: YES];
}

- (IBAction)authenticationDialogAction:(id)sender
{
}

- (IBAction)progressDialogAction:(id)sender
{
}

@end
