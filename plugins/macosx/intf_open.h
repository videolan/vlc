/*****************************************************************************
 * intf_open.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: intf_open.h,v 1.4 2002/07/15 01:54:03 jlj Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

NSArray *GetEjectableMediaOfClass( const char *psz_class );

/*****************************************************************************
 * Intf_Open interface
 *****************************************************************************/
@interface VLCOpen : NSObject
{
    IBOutlet id o_playlist;

    IBOutlet id o_disc_panel;
    IBOutlet id o_disc_btn_ok;
    IBOutlet id o_disc_btn_cancel;
    IBOutlet id o_disc_lbl_type;
    IBOutlet id o_disc_lbl_sp;
    IBOutlet id o_disc_type;
    IBOutlet id o_disc_title;
    IBOutlet id o_disc_chapter;
    IBOutlet id o_disc_device;
    
    IBOutlet id o_net_panel;
    IBOutlet id o_net_btn_ok;
    IBOutlet id o_net_btn_cancel;
    IBOutlet id o_net_box_mode;
    IBOutlet id o_net_box_addr;
    IBOutlet id o_net_mode;
    IBOutlet id o_net_address;
    IBOutlet id o_net_port;
    IBOutlet id o_net_port_lbl;
    IBOutlet id o_net_port_stp;

    IBOutlet id o_quickly_panel;
    IBOutlet id o_quickly_btn_ok;
    IBOutlet id o_quickly_btn_cancel;
    IBOutlet id o_quickly_source;
}

- (IBAction)openFile:(id)sender;

- (IBAction)openDisc:(id)sender;
- (IBAction)openDiscTypeChanged:(id)sender;

- (IBAction)openNet:(id)sender;
- (IBAction)openNetModeChanged:(id)sender;

- (IBAction)openQuickly:(id)sender;

- (IBAction)panelCancel:(id)sender;
- (IBAction)panelOk:(id)sender;

@end
