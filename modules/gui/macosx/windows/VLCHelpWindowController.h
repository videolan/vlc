/*****************************************************************************
 * HelpWindowController.h
 *****************************************************************************
 * Copyright (C) 2001-2013 VLC authors and VideoLAN
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

#import <WebKit/WebKit.h>

@interface VLCHelpWindowController : NSWindowController<WKNavigationDelegate>

@property (readonly) WKWebView *helpWebView;
@property (readwrite, weak) IBOutlet NSButton *backButton;
@property (readwrite, weak) IBOutlet NSButton *forwardButton;
@property (readwrite, weak) IBOutlet NSButton *homeButton;
@property (readwrite, weak) IBOutlet NSProgressIndicator *progressIndicator;
@property (readwrite, weak) IBOutlet NSVisualEffectView *visualEffectView;

- (IBAction)helpGoHome:(id)sender;
- (IBAction)helpGoBack:(id)sender;
- (IBAction)helpGoForward:(id)sender;
- (void)showHelp;

@end
