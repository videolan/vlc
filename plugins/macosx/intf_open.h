/*****************************************************************************
 * intf_open.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_open.h,v 1.2.2.1 2002/06/02 22:32:46 massiot Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import <Cocoa/Cocoa.h>

NSArray *GetEjectableMediaOfClass( const char *psz_class );

/*****************************************************************************
 * Intf_Open interface
 *****************************************************************************/
@interface Intf_Open : NSObject
{
    IBOutlet id o_disc_panel;
    IBOutlet id o_disc_type;
    IBOutlet id o_disc_title;
    IBOutlet id o_disc_chapter;
    IBOutlet id o_disc_device;
    
    IBOutlet id o_net_panel;
    IBOutlet id o_net_protocol;
    IBOutlet id o_net_server_addr;
    IBOutlet id o_net_server_addr_label;
    IBOutlet id o_net_server_port;
    IBOutlet id o_net_server_port_label;
    IBOutlet id o_net_server_pstepper;
}

- (id)init;
+ (Intf_Open *)instance;
- (void)awakeFromNib;

- (IBAction)openDisc:(id)sender;
- (IBAction)openDiscTypeChanged:(id)sender;

- (IBAction)openFile:(id)sender;

- (IBAction)openNet:(id)sender;
- (IBAction)openNetProtocol:(id)sender;

- (IBAction)panelCancel:(id)sender;
- (IBAction)panelOk:(id)sender;

@end
