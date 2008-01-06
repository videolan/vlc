/*****************************************************************************
 * VLCBrowsableVideoView.h: VideoView subclasses that allow fullscreen
 * browsing
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

#import <QuartzCore/QuartzCore.h>
#import <VLCKit/VLCKit.h>

@interface VLCBrowsableVideoView : VLCVideoView {
    BOOL            menuDisplayed;
    NSArray *       itemsTree;
    NSRange         displayedItems;
    NSInteger       selectedIndex;
    CALayer *       selectionLayer;
    CALayer *       backLayer;
    CALayer *       menuLayer;
    NSIndexPath *   selectedPath;
    NSString *      nodeKeyPath;
    NSString *      contentKeyPath;
    id              selectedObject;
    BOOL            fullScreen;
    
    /* Actions on non-node items*/
    id target;
    SEL action;
}

/* Binds an nsarray to that property. But don't forget the set the access keys. */
@property (retain) NSArray * itemsTree;
@property (copy) NSString * nodeKeyPath;
@property (copy) NSString * contentKeyPath;

@property (readonly, retain) id selectedObject;

@property (readwrite) BOOL fullScreen;

/* Set up a specific action to do, on items that don't have node.
 * action first argument is the browsableVideoView. You can get the selected object,
 * with -selectedObject */
@property (retain) id target;
@property  SEL action;

- (void)toggleMenu;
- (void)displayMenu;
- (void)hideMenu;
@end
