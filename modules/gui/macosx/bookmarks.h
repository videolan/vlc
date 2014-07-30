/*****************************************************************************
 * bookmarks.h: MacOS X Bookmarks window
 *****************************************************************************
 * Copyright (C) 2005, 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Kühne <fkuehne at videolan dot org>
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
#import "intf.h"
#import <vlc_common.h>

@interface VLCBookmarks : NSObject
{
    /* main window */
    IBOutlet id o_bookmarks_window;
    IBOutlet id o_btn_add;
    IBOutlet id o_btn_clear;
    IBOutlet id o_btn_edit;
    IBOutlet id o_btn_extract;
    IBOutlet id o_btn_rm;
    IBOutlet id o_tbl_dataTable;

    /* edit window */
    IBOutlet id o_edit_window;
    IBOutlet id o_edit_btn_ok;
    IBOutlet id o_edit_btn_cancel;
    IBOutlet id o_edit_lbl_name;
    IBOutlet id o_edit_lbl_time;
    IBOutlet id o_edit_lbl_bytes;
    IBOutlet id o_edit_fld_name;
    IBOutlet id o_edit_fld_time;
    IBOutlet id o_edit_fld_bytes;

    input_thread_t *p_old_input;
}
+ (VLCBookmarks *)sharedInstance;

- (void)updateCocoaWindowLevel:(NSInteger)i_level;

- (IBAction)add:(id)sender;
- (IBAction)clear:(id)sender;
- (IBAction)edit:(id)sender;
- (IBAction)extract:(id)sender;
- (IBAction)remove:(id)sender;
- (IBAction)goToBookmark:(id)sender;

- (IBAction)edit_cancel:(id)sender;
- (IBAction)edit_ok:(id)sender;

- (void)showBookmarks;
- (id)dataTable;
@end
