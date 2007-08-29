/*****************************************************************************
 * VLCMedia.h: VLC.framework VLCMedia header
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
#import <VLC/VLCPlaylist.h>

/* Meta */
extern NSString * VLCMetaInformationTitle; /* Associated to an NSString */
extern NSString * VLCMetaInformationAuthor; /* Associated to an NSString */
extern NSString * VLCMetaInformationArtwork; /* Associated to an NSImage */

/* Notification */
extern NSString * VLCMediaSubItemAdded;

@class VLCPlaylist;

@interface VLCMedia : NSObject
{
    void * md;
    NSString * url;
    VLCPlaylist *subitems;
    NSMutableDictionary *metaInformation;
}

- (id)initWithURL:(NSString *)anURL;
+ (id)mediaWithURL:(NSString *)anURL;

- (void) dealloc;

- (NSString *)url;
- (VLCPlaylist *)subitems;

/* Returns a dictionary with corresponding object associated with a meta */
- (NSDictionary *)metaInformation;
@end
