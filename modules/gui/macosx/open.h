/*****************************************************************************
 * open.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: open.h,v 1.6 2003/01/06 02:45:09 massiot Exp $
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

#define OPEN_PANEL_FULL_HEIGHT 494
#define OPEN_PANEL_SHORT_HEIGHT 325
#define WINDOW_TITLE_HEIGHT 21

/*****************************************************************************
 * Intf_Open interface
 *****************************************************************************/
@interface VLCOpen : NSObject
{
    IBOutlet id o_playlist;

    IBOutlet id o_panel;

    IBOutlet id o_mrl;
    IBOutlet id o_mrl_lbl;
    IBOutlet id o_tabview;

    IBOutlet id o_btn_ok;
    IBOutlet id o_btn_cancel;

    IBOutlet id o_file_path;
    IBOutlet id o_file_btn_browse;
    IBOutlet id o_file_stream;

    IBOutlet id o_disc_type;
    IBOutlet id o_disc_device;
    IBOutlet id o_disc_device_lbl;
    IBOutlet id o_disc_title;
    IBOutlet id o_disc_title_lbl;
    IBOutlet id o_disc_title_stp;
    IBOutlet id o_disc_chapter;
    IBOutlet id o_disc_chapter_lbl;
    IBOutlet id o_disc_chapter_stp;
    IBOutlet id o_disc_videots_folder;
    IBOutlet id o_disc_videots_btn_browse;
    IBOutlet id o_disc_dvd_menus;

    IBOutlet id o_net_mode;
    IBOutlet id o_net_udp_port;
    IBOutlet id o_net_udp_port_lbl;
    IBOutlet id o_net_udp_port_stp;
    IBOutlet id o_net_udpm_addr;
    IBOutlet id o_net_udpm_addr_lbl;
    IBOutlet id o_net_udpm_port;
    IBOutlet id o_net_udpm_port_lbl;
    IBOutlet id o_net_udpm_port_stp;
    IBOutlet id o_net_cs_addr;
    IBOutlet id o_net_cs_addr_lbl;
    IBOutlet id o_net_cs_port;
    IBOutlet id o_net_cs_port_lbl;
    IBOutlet id o_net_cs_port_stp;
    IBOutlet id o_net_http_url;
    IBOutlet id o_net_http_url_lbl;

    IBOutlet id o_sout_cbox;
    IBOutlet id o_sout_mrl_lbl;
    IBOutlet id o_sout_mrl;
    IBOutlet id o_sout_access;
    IBOutlet id o_sout_file_path;
    IBOutlet id o_sout_file_btn_browse;
    IBOutlet id o_sout_udp_addr;
    IBOutlet id o_sout_udp_addr_lbl;
    IBOutlet id o_sout_udp_port;
    IBOutlet id o_sout_udp_port_lbl;
    IBOutlet id o_sout_udp_port_stp;
    IBOutlet id o_sout_mux;
}

- (void)openTarget:(int)i_type;
- (void)tabView:(NSTabView *)o_tv didSelectTabViewItem:(NSTabViewItem *)o_tvi;

- (IBAction)openFileGeneric:(id)sender;
- (void)openFilePathChanged:(NSNotification *)o_notification;
- (IBAction)openFileBrowse:(id)sender;
- (IBAction)openFileStreamChanged:(id)sender;

- (IBAction)openDisc:(id)sender;
- (IBAction)openDiscTypeChanged:(id)sender;
- (IBAction)openDiscStepperChanged:(id)sender;
- (void)openDiscInfoChanged:(NSNotification *)o_notification;
- (IBAction)openDiscMenusChanged:(id)sender;
- (IBAction)openVTSBrowse:(id)sender;

- (IBAction)openNet:(id)sender;
- (IBAction)openNetModeChanged:(id)sender;
- (IBAction)openNetStepperChanged:(id)sender;
- (void)openNetInfoChanged:(NSNotification *)o_notification;

- (IBAction)soutChanged:(id)sender;
- (IBAction)soutFileBrowse:(id)sender;
- (void)soutModeChanged:(NSNotification *)o_notification;
- (void)soutInfoChanged:(NSNotification *)o_notification;
- (IBAction)soutStepperChanged:(id)sender;

- (IBAction)panelCancel:(id)sender;
- (IBAction)panelOk:(id)sender;

- (IBAction)openFile:(id)sender;

@end
