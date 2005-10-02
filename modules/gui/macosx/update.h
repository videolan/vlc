/*****************************************************************************
 * update.h: MacOS X Check-For-Update window
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Felix KŸhne <fkuehne@users.sf.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#import <Cocoa/Cocoa.h>

@interface VLCUpdate : NSObject
{
    IBOutlet id o_bar_progress;
    IBOutlet id o_btn_cancel;
    IBOutlet id o_btn_DownloadNow;
    IBOutlet id o_btn_okay;
    IBOutlet id o_fld_currentVersion;
    IBOutlet id o_fld_dest;
    IBOutlet id o_fld_elpTime;
    IBOutlet id o_fld_releaseNote;
    IBOutlet id o_fld_remTime;
    IBOutlet id o_fld_size;
    IBOutlet id o_fld_source;
    IBOutlet id o_fld_userVersion;
    IBOutlet id o_lbl_currentVersion;
    IBOutlet id o_lbl_mirror;
    IBOutlet id o_lbl_size;
    IBOutlet id o_lbl_userVersion;
    IBOutlet id o_pop_mirror;
    IBOutlet id o_progress_window;
    IBOutlet id o_update_window;
    IBOutlet id o_bar_checking;
    IBOutlet id o_lbl_checkForUpdate;
    
    NSMutableArray * o_mirrors;
    NSMutableDictionary * o_files;
}


- (IBAction)cancel:(id)sender;
- (IBAction)download:(id)sender;
- (IBAction)okay:(id)sender;

- (void)showUpdateWindow;
- (void)initStrings;
- (void)getData;

+ (VLCUpdate *)sharedInstance;

@end
