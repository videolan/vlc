/*****************************************************************************
 * VLCProgressPanel.m: A Generic Progress Indicator Panel created for VLC
 *****************************************************************************
 * Copyright (C) 2009-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "VLCProgressPanel.h"


@implementation VLCProgressPanel

- (id)init
{
    NSRect windowRect;
    windowRect.size.height = 182;
    windowRect.size.width = 520;
    windowRect.origin.x = windowRect.origin.y = 0;

    return [super initWithContentRect:windowRect
                            styleMask:NSTitledWindowMask
                              backing:NSBackingStoreBuffered
                                defer:YES];
}

- (void)createContentView
{
    NSRect s_rc = [self frame];
    id ourContentView = [self contentView];

    s_rc.origin.x = 398;
    s_rc.origin.y = 28;
    s_rc.size.height = 32;
    s_rc.size.width = 108;
    _cancelButton = [[NSButton alloc] initWithFrame:s_rc];
    [_cancelButton setButtonType:NSMomentaryLightButton];
    [_cancelButton setTitle:@"Cancel"];
    [_cancelButton setBezelStyle:NSRoundedBezelStyle];
    [_cancelButton setBordered:YES];
    [_cancelButton setTarget:self];
    [_cancelButton setAction:@selector(cancelDialog:)];
    [_cancelButton setKeyEquivalent:@"\e"] ; // escape key
    [ourContentView addSubview:_cancelButton];

    s_rc.origin.x = 89;
    s_rc.origin.y = 153;
    s_rc.size.height = 17;
    s_rc.size.width = 414;
    _titleField = [[NSTextField alloc] initWithFrame:s_rc];
    [_titleField setFont:[NSFont boldSystemFontOfSize:0]];
    [_titleField setBezeled:NO];
    [_titleField setEditable:NO];
    [_titleField setSelectable:YES];
    [_titleField setDrawsBackground:NO];
    [ourContentView addSubview:_titleField];

    s_rc.origin.x = 89;
    s_rc.origin.y = 116;
    s_rc.size.height = 42;
    s_rc.size.width = 414;
    _messageField = [[NSTextField alloc] initWithFrame:s_rc];
    [_messageField setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [_messageField setBezeled:NO];
    [_messageField setEditable:NO];
    [_messageField setSelectable:YES];
    [_messageField setDrawsBackground:NO];
    [ourContentView addSubview:_messageField];

    s_rc.origin.x = 90;
    s_rc.origin.y = 66;
    s_rc.size.height = 20;
    s_rc.size.width = 412;
    _progressBar = [[NSProgressIndicator alloc] initWithFrame:s_rc];
    [_progressBar setMaxValue:1000.0];
    [_progressBar setUsesThreadedAnimation:YES];
    [_progressBar setStyle:NSProgressIndicatorBarStyle];
    [_progressBar setDisplayedWhenStopped:YES];
    [_progressBar setControlSize:NSRegularControlSize];
    [_progressBar setIndeterminate:NO];
    [ourContentView addSubview:_progressBar];
    [_progressBar startAnimation:nil];

    s_rc.origin.x = 20;
    s_rc.origin.y = 110;
    s_rc.size.height = s_rc.size.width = 64;
    _iconView = [[NSImageView alloc] initWithFrame:s_rc];
    [_iconView setImage:[NSImage imageNamed:@"NSApplicationIcon"]];
    [_iconView setEditable:NO];
    [_iconView setAllowsCutCopyPaste:NO];
    [ourContentView addSubview:_iconView];
}

- (void)setDialogTitle:(NSString *)title
{
    [_titleField setStringValue:title];
    [self setTitle:title];
}

- (void)setDialogMessage:(NSString *)message
{
    [_messageField setStringValue:message];
}

- (void)setCancelButtonLabel:(NSString *)cancelLabel
{
    [_cancelButton setTitle:cancelLabel];
}

- (void)setProgressAsDouble:(double)value
{
    [_progressBar setDoubleValue:value];
}

- (BOOL)isCancelled
{
    return _isCancelled;
}

- (IBAction)cancelDialog:(id)sender
{
    _isCancelled = YES;
    [_progressBar setIndeterminate:YES];
    [_progressBar startAnimation:self];
}

@end
