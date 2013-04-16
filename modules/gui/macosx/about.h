/*****************************************************************************
 * about.h: MacOS X About Panel
 *****************************************************************************
 * Copyright (C) 2001-2013 VLC authors and VideoLAN
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

#import <WebKit/WebKit.h> //we need to be here, because we're using a WebView object below

/*****************************************************************************
 * VLAboutBox interface
 *****************************************************************************/
@interface VLAboutBox : NSObject
{
    /* main about panel and stuff related to its views */
    IBOutlet id o_about_window;
    IBOutlet id o_name_version_field;
    IBOutlet id o_revision_field;
    IBOutlet id o_copyright_field;
    IBOutlet id o_credits_textview;
    IBOutlet id o_credits_scrollview;
    IBOutlet id o_gpl_btn;
    IBOutlet id o_credits_btn;
    IBOutlet id o_authors_btn;
    IBOutlet id o_name_field;
    IBOutlet id o_icon_view;
    IBOutlet id o_joinus_txt;
    IBOutlet id o_trademarks_txt;

    NSTimer *o_scroll_timer;
    float f_current;
    float f_end;
    NSTimeInterval i_start;
    BOOL b_restart;
    BOOL b_isSetUp;

    NSString *o_authors;

    /* generic help window */
    IBOutlet id o_help_window;
    IBOutlet WebView *o_help_web_view; //we may _not_ use id here because of method name collisions
    IBOutlet id o_help_bwd_btn;
    IBOutlet id o_help_fwd_btn;
    IBOutlet id o_help_home_btn;
}

+ (VLAboutBox *)sharedInstance;
- (void)showAbout;
- (void)showHelp;
- (void)showGPL;
- (IBAction)buttonAction:(id)sender;
- (IBAction)helpGoHome:(id)sender;

@end
