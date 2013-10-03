/*****************************************************************************
 * coredialogs.h: Mac OS X Core Dialogs
 *****************************************************************************
 * Copyright (C) 2005-2012 VLC authors and VideoLAN
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
#import <vlc_common.h>
#import <vlc_dialog.h>
#import <Cocoa/Cocoa.h>


/*****************************************************************************
 * VLCErrorPanel interface
 *****************************************************************************/

@interface VLCErrorPanel : NSObject
{
    IBOutlet id o_window;
    IBOutlet id o_cleanup_button;
    IBOutlet id o_error_table;

    NSMutableArray * o_errors;
    NSMutableArray * o_icons;

    BOOL b_nib_loaded;
}
- (IBAction)cleanupTable:(id)sender;

-(void)showPanel;
-(void)addError: (NSString *)o_error withMsg:(NSString *)o_msg;

@end

/*****************************************************************************
 * VLCCoreDialogProvider interface
 *****************************************************************************/
@interface VLCCoreDialogProvider : NSObject
{
    VLCErrorPanel *o_error_panel;

    /* authentication dialogue */
    IBOutlet id o_auth_cancel_btn;
    IBOutlet id o_auth_description_txt;
    IBOutlet id o_auth_login_fld;
    IBOutlet id o_auth_login_txt;
    IBOutlet id o_auth_ok_btn;
    IBOutlet id o_auth_pw_fld;
    IBOutlet id o_auth_pw_txt;
    IBOutlet id o_auth_title_txt;
    IBOutlet id o_auth_win;

    /* progress dialogue */
    IBOutlet NSProgressIndicator * o_prog_bar;
    IBOutlet id o_prog_cancel_btn;
    IBOutlet id o_prog_description_txt;
    IBOutlet id o_prog_title_txt;
    IBOutlet id o_prog_win;
    BOOL b_progress_cancelled;
}
+ (VLCCoreDialogProvider *)sharedInstance;

-(void)performEventWithObject: (NSValue *)o_value ofType: (const char*)type;

-(void)showFatalDialog: (NSValue *)o_value;
-(void)showFatalWaitDialog: (NSValue *)o_value;
-(void)showQuestionDialog: (NSValue *)o_value;

-(void)showLoginDialog: (NSValue *)o_value;
-(IBAction)loginDialogAction:(id)sender;

-(void)showProgressDialogOnMainThread: (NSValue *)o_value;
-(void)showProgressDialog: (NSValue *)o_value;
-(IBAction)progDialogAction:(id)sender;
-(BOOL)progressCancelled;
-(void)updateProgressPanelWithText: (NSString *)string andNumber: (double)d_number;
-(void)destroyProgressPanel;

-(id)errorPanel;

@end
