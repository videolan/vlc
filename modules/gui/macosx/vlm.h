/*****************************************************************************
 * vlm.h: VLM Configuration panel for Mac OS X
 *****************************************************************************
 * Copyright (c) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne@videolan.org>
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


#ifdef HAVE_CONFIG_H
#   import "config.h"
#endif

#ifdef ENABLE_VLM

#import <Cocoa/Cocoa.h>
#import "intf.h"

#import <vlc_vlm.h>

@interface VLCVLMController : NSObject
{
    /* broadcast panel */
    IBOutlet NSButton *o_bcast_add_btn;
    IBOutlet NSBox *o_bcast_box;
    IBOutlet NSButton *o_bcast_cancel_btn;
    IBOutlet NSButton *o_bcast_enable_ckb;
    IBOutlet NSButton *o_bcast_input_btn;
    IBOutlet NSTextField *o_bcast_input_fld;
    IBOutlet NSButton *o_bcast_loop_ckb;
    IBOutlet NSTextField *o_bcast_name_fld;
    IBOutlet NSButton *o_bcast_output_btn;
    IBOutlet NSTextField *o_bcast_output_fld;
    IBOutlet NSPanel *o_bcast_panel;

    /* schedule panel */
    IBOutlet id o_sched_add_btn;
    IBOutlet id o_sched_box;
    IBOutlet id o_sched_cancel_btn;
    IBOutlet NSDatePicker *o_sched_date_datePicker;
    IBOutlet NSTextField *o_sched_date_lbl;
    IBOutlet NSButton *o_sched_input_btn;
    IBOutlet NSTextField *o_sched_input_fld;
    IBOutlet NSTextField *o_sched_input_lbl;
    IBOutlet NSTextField *o_sched_name_fld;
    IBOutlet NSTextField *o_sched_name_lbl;
    IBOutlet NSButton *o_sched_output_btn;
    IBOutlet NSTextField *o_sched_output_fld;
    IBOutlet NSTextField *o_sched_output_lbl;
    IBOutlet NSTextField *o_sched_repeat_fld;
    IBOutlet NSTextField *o_sched_repeat_lbl;
    IBOutlet NSDatePicker *o_sched_repeatDelay_datePicker;
    IBOutlet id o_sched_time_box;
    IBOutlet NSWindow *o_sched_panel;

    /* VLM Window */
    IBOutlet NSTableView *o_vlm_list;
    IBOutlet NSWindow *o_vlm_win;

    /* VOD Panel */
    IBOutlet NSButton *o_vod_add_btn;
    IBOutlet id o_vod_box;
    IBOutlet NSButton *o_vod_cancel_btn;
    IBOutlet NSButton *o_vod_input_btn;
    IBOutlet NSTextField *o_vod_input_fld;
    IBOutlet NSTextField *o_vod_input_lbl;
    IBOutlet NSButton *o_vod_loop_ckb;
    IBOutlet NSTextField *o_vod_name_fld;
    IBOutlet NSTextField *o_vod_name_lbl;
    IBOutlet NSButton *o_vod_output_btn;
    IBOutlet NSTextField *o_vod_output_fld;
    IBOutlet NSTextField *o_vod_output_lbl;
    IBOutlet NSWindow *o_vod_panel;
}
+ (VLCVLMController *)sharedInstance;

/* toolbar */
- (NSToolbarItem *) toolbar: (NSToolbar *)o_toolbar 
      itemForItemIdentifier: (NSString *)o_itemIdent 
  willBeInsertedIntoToolbar: (BOOL)b_willBeInserted;
- (NSArray *)toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar;
- (NSArray *)toolbarAllowedItemIdentifiers: (NSToolbar *)toolbar;

- (void)showVLMWindow;
- (void)initStrings;

- (void)addBcast;
- (void)addVOD;
- (void)addSched;

- (IBAction)bcastButtonAction:(id)sender;
- (IBAction)listDoubleClickAction:(id)sender;
- (IBAction)schedButtonAction:(id)sender;
- (IBAction)vodButtonAction:(id)sender;

- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
@end

#endif