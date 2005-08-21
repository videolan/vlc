/*****************************************************************************
 * update.m: MacOS X Check-For-Update window
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
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


/*****************************************************************************
 * Note: 
 * the code used to bind with VLC's core and the download of files is heavily 
 * based upon ../wxwidgets/updatevlc.cpp, written by Antoine Cellerier. 
 * (he is a member of the VideoLAN team) 
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import "update.h"
#import "intf.h"

#import <vlc/vlc.h>
#import <vlc/intf.h>

#import "vlc_block.h"
#import "vlc_stream.h"
#import "vlc_xml.h"

#define UPDATE_VLC_OS "macosx"
#define UPDATE_VLC_ARCH "ppc"


/*****************************************************************************
 * VLCExtended implementation
 *****************************************************************************/

@implementation VLCUpdate

static VLCUpdate *_o_sharedInstance = nil;

+ (VLCUpdate *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)awakeFromNib
{
    /* clean the interface */
    [o_fld_userVersion setStringValue: [[[NSBundle mainBundle] infoDictionary] \
        objectForKey:@"CFBundleVersion"]];
    [o_fld_releaseNote setString: @""];
    [o_fld_releasedOn setStringValue: @""];
    [o_fld_size setStringValue: @""];
    [o_fld_currentVersion setStringValue: @""];
    
    [self initStrings];
}

- (void)initStrings
{
    /* translate strings to the user's language */
    [o_btn_cancel setTitle: _NS("Cancel")];
    [o_btn_DownloadNow setTitle: _NS("Download now")];
    [o_btn_okay setTitle: _NS("OK")];
    [o_lbl_currentVersion setStringValue: [_NS("Current version") \
        stringByAppendingString: @":"]];
    [o_lbl_releasedOn setStringValue: [_NS("Released on") \
        stringByAppendingString: @":"]];
    [o_lbl_size setStringValue: [_NS("Size") \
        stringByAppendingString: @":"]];
    [o_lbl_userVersion setStringValue: [_NS("Your version") \
        stringByAppendingString: @":"]];
    [o_lbl_mirror setStringValue: [_NS("Mirror") \
        stringByAppendingString: @":"]];
    [o_lbl_checkForUpdate setStringValue: _NS("Checking for update...")];
}

- (void)showUpdateWindow
{
    /* show the window and check for a potential update */
    [o_update_window center];
    [o_update_window displayIfNeeded];
    [o_update_window makeKeyAndOrderFront:nil];
}

- (IBAction)cancel:(id)sender
{
    /* cancel the download and close the sheet */
}

- (IBAction)download:(id)sender
{
    /* open the sheet and start the download */
}

- (IBAction)okay:(id)sender
{
    /* just close the window */
    [o_update_window close];
}

@end
