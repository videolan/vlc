/*****************************************************************************
 * about.h: MacOS X About Panel
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
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

/*****************************************************************************
 * VLAboutBox interface 
 *****************************************************************************/
@interface VLAboutBox : NSObject
{
    IBOutlet id o_about_window;
    IBOutlet id o_name_version_field;
    IBOutlet id o_revision_field;
    IBOutlet id o_copyright_field;
    IBOutlet id o_credits_textview;
    IBOutlet id o_credits_scrollview;
    
    NSTimer *o_scroll_timer;
    float f_current;
    float f_end;
    NSTimeInterval i_start;
    BOOL b_restart;
    
    NSString *o_credits_path;
    NSString *o_credits;
    NSString *o_thanks;
    NSString *o_name_version;
    NSString *o_copyright;
    NSDictionary *o_info_dict;
    CFBundleRef localInfoBundle;
    NSDictionary *o_local_dict;
}

+ (VLAboutBox *)sharedInstance;
- (void)showPanel;

@end
