/*****************************************************************************
 * equalizer.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: JŽr™me Decoodt <djc@videolan.org>
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

/*****************************************************************************
 * VLCEqualizer interface 
 *****************************************************************************/
@interface VLCEqualizer : NSObject
{
    IBOutlet id o_btn_equalizer;
    IBOutlet id o_ckb_2pass;
    IBOutlet id o_ckb_enable;
    IBOutlet id o_fld_preamp;
    IBOutlet id o_popup_presets;
    IBOutlet id o_slider_band1;
    IBOutlet id o_slider_band10;
    IBOutlet id o_slider_band2;
    IBOutlet id o_slider_band3;
    IBOutlet id o_slider_band4;
    IBOutlet id o_slider_band5;
    IBOutlet id o_slider_band6;
    IBOutlet id o_slider_band7;
    IBOutlet id o_slider_band8;
    IBOutlet id o_slider_band9;
    IBOutlet id o_slider_preamp;
    IBOutlet id o_window;
}
- (void)initStrings;
- (void)equalizerUpdated;
- (IBAction)bandSliderUpdated:(id)sender;
- (IBAction)changePreset:(id)sender;
- (IBAction)enable:(id)sender;
- (IBAction)preampSliderUpdated:(id)sender;
- (IBAction)toggleWindow:(id)sender;
- (IBAction)twopass:(id)sender;
- (void)windowWillClose:(NSNotification *)aNotification;
- (void)awakeFromNib;
@end
