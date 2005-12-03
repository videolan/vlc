/*****************************************************************************
 * sfilter.h: MacOS X Subpicture filters dialogue
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id:$
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
#import <vlc/intf.h>

@interface VLCsFilters : NSObject
{
    /* window stuff */
    IBOutlet id o_sfilter_tabView;
    IBOutlet id o_sfilter_win;

    /* logo section */
    IBOutlet id o_logo_enabled_ckb;
    IBOutlet id o_logo_image_btn;
    IBOutlet id o_logo_image_fld;
    IBOutlet id o_logo_image_lbl;
    IBOutlet id o_logo_opaque_lbl;
    IBOutlet id o_logo_opaque_sld;
    IBOutlet id o_logo_pos_fix_btn;
    IBOutlet id o_logo_pos_fix_X_fld;
    IBOutlet id o_logo_pos_fix_X_lbl;
    IBOutlet id o_logo_pos_fix_Y_fld;
    IBOutlet id o_logo_pos_fix_Y_lbl;
    IBOutlet id o_logo_pos_lbl;
    IBOutlet id o_logo_pos_rel_btn;
    IBOutlet id o_logo_pos_rel_pop;
    IBOutlet id o_logo_pos_matrix;
    
    /* marquee section */
    IBOutlet id o_marq_enabled_ckb;
    IBOutlet id o_marq_color_lbl;
    IBOutlet id o_marq_color_pop;
    IBOutlet id o_marq_marq_fld;
    IBOutlet id o_marq_marq_lbl;
    IBOutlet id o_marq_opaque_lbl;
    IBOutlet id o_marq_opaque_sld;
    IBOutlet id o_marq_pos_fix_btn;
    IBOutlet id o_marq_pos_fix_X_fld;
    IBOutlet id o_marq_pos_fix_X_lbl;
    IBOutlet id o_marq_pos_fix_Y_fld;
    IBOutlet id o_marq_pos_fix_Y_lbl;
    IBOutlet id o_marq_pos_lbl;
    IBOutlet id o_marq_pos_rel_btn;
    IBOutlet id o_marq_pos_rel_pop;
    IBOutlet id o_marq_size_inPx_lbl;
    IBOutlet id o_marq_size_lbl;
    IBOutlet id o_marq_size_pop;
    IBOutlet id o_marq_tmOut_fld;
    IBOutlet id o_marq_tmOut_lbl;
    IBOutlet id o_marq_tmOut_ms_lbl;
    IBOutlet id o_marq_pos_matrix;

    /* time section */
    IBOutlet id o_time_enabled_ckb;
    IBOutlet id o_time_color_lbl;
    IBOutlet id o_time_color_pop;
    IBOutlet id o_time_opaque_lbl;
    IBOutlet id o_time_opaque_sld;
    IBOutlet id o_time_pos_fix_btn;
    IBOutlet id o_time_pos_fix_X_fld;
    IBOutlet id o_time_pos_fix_X_lbl;
    IBOutlet id o_time_pos_fix_Y_fld;
    IBOutlet id o_time_pos_fix_Y_lbl;
    IBOutlet id o_time_pos_lbl;
    IBOutlet id o_time_pos_rel_btn;
    IBOutlet id o_time_pos_rel_pop;
    IBOutlet id o_time_size_inPx_lbl;
    IBOutlet id o_time_size_lbl;
    IBOutlet id o_time_size_pop;
    IBOutlet id o_time_stamp_fld;
    IBOutlet id o_time_stamp_lbl;
    IBOutlet id o_time_pos_matrix;
    
    BOOL o_config_changed;
    BOOL o_save_settings;
    NSArray * o_colors;
}


+ (VLCsFilters *)sharedInstance;
- (IBAction)logo_selectFile:(id)sender;
- (IBAction)propertyChanged:(id)sender;
- (IBAction)enableFilter:(id)sender;
- (IBAction)otherPositionForLogo:(id)sender;
- (IBAction)otherPositionForMarq:(id)sender;
- (IBAction)otherPositionForTime:(id)sender;

- (void)showAsPanel;
- (void)initStrings;
- (void)changeFiltersString: (char *)psz_name onOrOff: (vlc_bool_t )b_add;
- (void)enableTime;
- (void)enableLogo;
- (void)enableMarq;

@end
