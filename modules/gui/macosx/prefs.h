/*****************************************************************************
 * prefs.h: MacOS X module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2004 VideoLAN
 * $Id: prefs.h,v 1.14 2004/01/30 12:44:21 hartman Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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

@interface VLCTreeItem : NSObject
{
    int i_object_id;
    char *psz_help;
    char *psz_section;
    NSString *o_name;
    
    NSMutableArray *o_config_controls;

    VLCTreeItem *o_parent;
    NSMutableArray *o_children;
}

+ (VLCTreeItem *)rootItem;
- (id)initWithID: (int)i_id parent: (VLCTreeItem *)o_parent_item;
- (int)numberOfChildren;
- (VLCTreeItem *)childAtIndex:(int)i_index;
- (int)objectID;
- (NSString *)name;
- (void)setName:(NSString *)a_name;

@end

/*****************************************************************************
 * VLCPrefs interface
 *****************************************************************************/
@interface VLCPrefs : NSObject
{
    intf_thread_t *p_intf;
    vlc_bool_t b_advanced;
    NSView *o_empty_view;
    
    IBOutlet id o_prefs_window;
    IBOutlet id o_tree;
    IBOutlet id o_prefs_view;
    IBOutlet id o_save_btn;
    IBOutlet id o_cancel_btn;
    IBOutlet id o_reset_btn;
    IBOutlet id o_advanced_ckb;
}

- (void)initStrings;
- (void)showPrefs;
- (IBAction)savePrefs: (id)sender;
- (IBAction)closePrefs: (id)sender;
- (IBAction)resetAll: (id)sender;
- (void)sheetDidEnd:(NSWindow *)o_sheet returnCode:(int)i_return contextInfo:(void *)o_context;
- (IBAction)advancedToggle: (id)sender;
- (void)showViewForID: (int)i_id;

@end