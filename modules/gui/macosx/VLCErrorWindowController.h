/*****************************************************************************
 * HelpWindowController.h
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Felix Paul Kühne <fkuehne -at- videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import <Cocoa/Cocoa.h>

@interface VLCErrorWindowController : NSWindowController
{
    IBOutlet id o_cleanup_button;
    IBOutlet id o_error_table;

    NSMutableArray *o_errors;
    NSMutableArray *o_icons;
}

- (IBAction)cleanupTable:(id)sender;

-(void)addError:(NSString *)o_error withMsg:(NSString *)o_msg;

@end
