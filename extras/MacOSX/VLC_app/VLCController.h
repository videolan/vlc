/*****************************************************************************
 * VLCController.h: VLC.app main controller
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#define VLCPanic( ex ) __VLCPanic( ex, __FUNCTION__, __FILE__, __LINE__ )
static inline void __VLCPanic( const char * str, const char * function, const char * file, int line_number )
{
    NSRunCriticalAlertPanel( @"Error", [NSString stringWithFormat:@"The following error was encountered: %s (%s:%d %s)", str, file, line_number, function], @"Quit", nil, nil );
    exit( -1 );
}

@interface VLCController : NSObject
{
    IBOutlet id categoryList;
    IBOutlet id detailList;
    IBOutlet id videoView;
    IBOutlet id detailSearchField;
}
@end
