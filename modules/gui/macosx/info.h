/*****************************************************************************
 * info.h: MacOS X info panel
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: info.h,v 1.1 2003/02/17 10:52:07 hartman Exp $
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
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

#include <vlc/vlc.h>
#include <vlc/intf.h>

#import <Cocoa/Cocoa.h>


/*****************************************************************************
 * VLCInfo interface 
 *****************************************************************************/
@interface VLCInfo : NSObject {

    IBOutlet id o_info_window;
    IBOutlet id o_info_view;
    IBOutlet id o_info_selector;
    
    intf_thread_t *p_intf;
    NSMutableDictionary *o_info_strings;
}

- (IBAction)toggleInfoPanel:(id)sender;
- (IBAction)showCategory:(id)sender;
- (void)updateInfo;
- (void)createInfoView:(input_info_category_t *)p_category;

@end