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
    self.window.title = _NS("VLC media player Help");
    if (@available(macOS 10.12, *)) {
        self.window.tabbingMode = NSWindowTabbingModeDisallowed;
    }
    
    _helpWebView = [[WKWebView alloc] initWithFrame:self.window.contentView.bounds];
    self.helpWebView.navigationDelegate = self;
    self.helpWebView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.window.contentView addSubview:self.helpWebView positioned:NSWindowBelow relativeTo:self.visualEffectView];

    [self.helpWebView.topAnchor constraintEqualToAnchor:self.window.contentView.topAnchor].active = YES;
    [self.helpWebView.bottomAnchor constraintEqualToAnchor:self.visualEffectView.topAnchor].active = YES;
    [self.helpWebView.leadingAnchor constraintEqualToAnchor:self.window.contentView.leadingAnchor].active = YES;
    [self.helpWebView.trailingAnchor constraintEqualToAnchor:self.window.contentView.trailingAnchor].active = YES;

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
    [self.helpWebView loadHTMLString:htmlWithStyle baseURL:baseURL];
}

- (IBAction)helpGoBack:(id)sender
{
    [self.helpWebView goBack];
}

- (IBAction)helpGoForward:(id)sender
{
    [self.helpWebView goForward];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    /* Update back/forward button states whenever a new page is loaded */
    self.forwardButton.enabled = webView.canGoForward;
    self.backButton.enabled = webView.canGoBack;
    self.progressIndicator.hidden = YES;
    [self.progressIndicator stopAnimation:nil];
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    self.progressIndicator.hidden = NO;
    [self.progressIndicator startAnimation:nil];
}

@end
