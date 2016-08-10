/*****************************************************************************
 * HelpWindowController.m
 *****************************************************************************
 * Copyright (C) 2001-2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Felix Paul Kühne <fkuehne -at- videolan.org>
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

#import "VLCHelpWindowController.h"

#import "VLCMain.h"
#import <vlc_intf_strings.h>
#import <vlc_about.h>
#import "CompatibilityFixes.h"
#import "VLCScrollingClipView.h"

@implementation VLCHelpWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"Help"];
    if (self) {

        [self setWindowFrameAutosaveName:@"help"];
    }

    return self;
}

- (void)windowDidLoad
{
    [[self window] setTitle: _NS("VLC media player Help")];
    [o_help_fwd_btn setToolTip: _NS("Next")];
    [o_help_bwd_btn setToolTip: _NS("Previous")];
    [o_help_home_btn setToolTip: _NS("Index")];
}

- (void)showHelp
{
    [self showWindow:nil];
    [self helpGoHome:nil];
}

- (IBAction)helpGoHome:(id)sender
{
    [[o_help_web_view mainFrame] loadHTMLString: _NS(I_LONGHELP)
                                        baseURL: [NSURL URLWithString:@"http://videolan.org"]];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    /* delegate to update button states (we're the frameLoadDelegate for our help's webview)« */
    [o_help_fwd_btn setEnabled: [o_help_web_view canGoForward]];
    [o_help_bwd_btn setEnabled: [o_help_web_view canGoBack]];
}

@end
