/*****************************************************************************
 * HelpWindowController.m
 *****************************************************************************
 * Copyright (C) 2001-2014 VLC authors and VideoLAN
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

#import <vlc_intf_strings.h>
#import <vlc_about.h>

#import "extensions/NSString+Helpers.h"
#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"
#import "views/VLCScrollingClipView.h"

@implementation VLCHelpWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"Help"];
    if (self) {
        self.windowFrameAutosaveName = @"help";
    }

    return self;
}

- (void)windowDidLoad
{
    if (@available(macOS 10.12, *)) {
        self.window.tabbingMode = NSWindowTabbingModeDisallowed;
    }
    self.window.title = _NS("VLC media player Help");
    self.forwardButton.toolTip = _NS("Next");
    self.backButton.toolTip = _NS("Previous");
    self.homeButton.toolTip = _NS("Index");
}

- (void)showHelp
{
    [self showWindow:nil];
    [self helpGoHome:nil];
}

- (IBAction)helpGoHome:(id)sender
{
    NSString * const style = @"<style>body { font-family: -apple-system, Helvetica Neue; }</style>";
    NSString * const htmlWithStyle = [style stringByAppendingString:NSTR(I_LONGHELP)];
    NSURL * const baseURL = [NSURL URLWithString:@"https://videolan.org"];
    [self.helpWebView.mainFrame loadHTMLString:htmlWithStyle baseURL:baseURL];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    /* Update back/forward button states whenever a new page is loaded */
    self.forwardButton.enabled = self.helpWebView.canGoForward;
    self.backButton.enabled = self.helpWebView.canGoBack;
}

@end
