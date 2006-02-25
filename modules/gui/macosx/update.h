/*****************************************************************************
 * update.h: MacOS X Check-For-Update window
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
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
#import <vlc_update.h>

@interface VLCUpdate : NSObject
{
    IBOutlet id o_btn_DownloadNow;
    IBOutlet id o_btn_okay;
    IBOutlet id o_fld_releaseNote;
    IBOutlet id o_fld_source;
    IBOutlet id o_fld_currentVersionAndSize;
    IBOutlet id o_fld_status;
    IBOutlet id o_update_window;
    IBOutlet id o_bar_checking;

    NSString * o_hashOfOurBinary;
    NSString * o_urlOfBinary;
    update_t * p_u;
    intf_thread_t * p_intf;
}

- (IBAction)download:(id)sender;
- (IBAction)okay:(id)sender;

- (void)showUpdateWindow;
- (void)initStrings;
- (void)checkForUpdate;
- (void)performDownload:(NSString *)path;

+ (VLCUpdate *)sharedInstance;

@end
