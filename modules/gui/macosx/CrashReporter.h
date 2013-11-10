/*****************************************************************************
 * CrashReporter.h: Mac OS X interface crash reporter
 *****************************************************************************
 * Copyright (C) 2009-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
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

#import <Cocoa/Cocoa.h>

@class VLCCrashReporter;
@protocol VLCCrashReporterDelegate <NSObject>

@required
- (void)reporterFinishedAction:(VLCCrashReporter *)reporter;

@end

@interface VLCCrashReporter : NSObject
{
    IBOutlet NSButton * _crashrep_dontSend_btn;
    IBOutlet NSButton * _crashrep_send_btn;
    IBOutlet NSTextView * _crashrep_fld;
    IBOutlet NSTextField * _crashrep_title_txt;
    IBOutlet NSTextField * _crashrep_desc_txt;
    IBOutlet NSWindow * _crashrep_win;
    IBOutlet NSButton * _crashrep_includeEmail_ckb;
    IBOutlet NSButton * _crashrep_dontaskagain_ckb;
    IBOutlet NSTextField * _crashrep_includeEmail_txt;
    NSURLConnection * _crashLogURLConnection;
}
+ (VLCCrashReporter *)sharedInstance;

@property (retain) id delegate;

- (void)showDialogAndSendLogIfDesired;

- (IBAction)buttonAction:(id)sender;

@end
