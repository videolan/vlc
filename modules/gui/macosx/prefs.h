/*****************************************************************************
 * prefs.h: MacOS X module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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

@class VLCTreeMainItem;

/*****************************************************************************
 * VLCPrefs interface
 *****************************************************************************/
@interface VLCPrefs : NSObject
{
    intf_thread_t *p_intf;
    VLCTreeMainItem * _rootTreeItem;
    NSView *o_empty_view;
    NSMutableDictionary *o_save_prefs;

    IBOutlet id o_prefs_window;
    IBOutlet id o_title;
    IBOutlet id o_tree;
    IBOutlet id o_prefs_view;
    IBOutlet id o_save_btn;
    IBOutlet id o_cancel_btn;
    IBOutlet id o_reset_btn;
    IBOutlet id o_showBasic_btn;
}

+ (VLCPrefs *)sharedInstance;

- (void)initStrings;
- (void)setTitle: (NSString *) o_title_name;
- (void)showPrefsWithLevel:(NSInteger)i_window_level;
- (IBAction)savePrefs: (id)sender;
- (IBAction)closePrefs: (id)sender;
- (IBAction)buttonAction: (id)sender;

@end

@interface VLCFlippedView : NSView
{

}

@end
