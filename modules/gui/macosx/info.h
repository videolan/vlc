/*****************************************************************************
 * info.h: MacOS X info panel
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: info.h,v 1.2 2003/02/23 05:53:53 jlj Exp $
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
 * VLCInfo interface 
 *****************************************************************************/
@interface VLCInfo : NSObject
{
    IBOutlet id o_window;
    IBOutlet id o_view;
    IBOutlet id o_selector;

    NSMutableDictionary * o_strings;
}

- (void)updateInfo;
- (IBAction)toggleInfoPanel:(id)sender;
- (IBAction)showCategory:(id)sender;
- (void)createInfoView:(input_info_category_t *)p_category;

@end
