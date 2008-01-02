/*****************************************************************************
 * VLCAppBindings.m: Helpful addition code related to bindings uses
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

#import "VLCAppBindings.h"

/* This is globally a big hack to ease binding uses */


/******************************************************************************
 * VLCMediaDiscoverer (CategoriesListBindings)
 */
@implementation VLCMediaDiscoverer (CategoriesListBindings)
+(void)initialize
{
    [VLCMediaDiscoverer setKeys:[NSArray arrayWithObject:@"running"] triggerChangeNotificationsForDependentKey:@"currentlyFetchingItems"];
}

+ (NSSet *)keyPathsForValuesAffectingValueForKey:(NSString *)key
{
    /* Thanks to Julien Robert, we'll have some nice auto triggered KVO event from here */
    static NSDictionary * dict = nil;
    if( !dict )
    {
        dict = [[NSDictionary dictionaryWithObjectsAndKeys:
            [NSSet setWithObject:@"discoveredMedia.flatAspect"], @"childrenInCategoriesListForDetailView",
            nil] retain];
    }
    return [dict objectForKey: key];
}

/* General shortcuts */
- (BOOL)currentlyFetchingItems
{
    return [self isRunning];
}
- (NSImage *)image
{
    static NSImage * sdImage = nil;
    if( !sdImage )
        sdImage = [[NSImage imageNamed:@"applications-internet.png"] retain];
    return sdImage;
}

/* CategoriesList specific bindings */
- (NSArray *)childrenInCategoriesList
{
    return nil;
}
- (NSString *)descriptionInCategoriesList
{
    return [self localizedName];
}
- (VLCMediaListAspect *)childrenInCategoriesListForDetailView
{
    return [[self discoveredMedia] flatAspect];
}
- (BOOL)editableInCategoriesList
{
    return NO;
}
- (BOOL)selectableInCategoriesList
{
    return YES;
}

/* VideoView specific bindings */
- (NSArray *)childrenInVideoView
{
    return [[[self discoveredMedia] flatAspect] valueForKeyPath:@"media"];
}

- (NSString *)descriptionInVideoView
{
    return [self localizedName];
}
- (BOOL)isLeaf
{
    return YES;
}

@end

/******************************************************************************
 * VLCMedia (VLCAppBindings)
 */
@implementation VLCMedia (VLCAppBindings)

+ (NSSet *)keyPathsForValuesAffectingValueForKey:(NSString *)key
{
    /* Thanks to Julien Robert, we'll have some nice auto triggered KVO event from here */
    static NSDictionary * dict = nil;
    if( !dict )
    {
        dict = [[NSDictionary dictionaryWithObjectsAndKeys:
            [NSSet setWithObject:@"subitems.hierarchicalNodeAspect.media"], @"childrenInCategoriesList",
            [NSSet setWithObject:@"metaDictionary.title"], @"descriptionInCategoriesList",
            [NSSet setWithObject:@"subitems.flatAspect"], @"childrenInCategoriesListForDetailView",
            [NSSet setWithObject:@"metaDictionary.title"], @"descriptionInVideoView",
            [NSSet setWithObject:@"state"], @"stateAsImage",
            nil] retain];
    }
    return [dict objectForKey: key];
}

/* CategoriesList specific bindings */
- (NSArray *)childrenInCategoriesList
{
    return [[[self subitems] hierarchicalNodeAspect] valueForKeyPath:@"media"];
}
- (void)setDescriptionInCategoriesList:(NSString *)description
{
    NSLog(@"unimplemented: meta edition");
}
- (NSString *)descriptionInCategoriesList
{
    return [[self metaDictionary] objectForKey:@"title"];
}
- (VLCMediaListAspect *)childrenInCategoriesListForDetailView
{
    return [[self subitems] flatAspect];
}
- (BOOL)editableInCategoriesList
{
    return YES;
}
- (BOOL)selectableInCategoriesList
{
    return YES;
}
- (BOOL)currentlyFetchingItems
{
    return NO;
}
- (NSImage *)image
{
    static NSImage * playlistImage = nil;
    if( !playlistImage )
        playlistImage = [[NSImage imageNamed:@"type_playlist.png"] retain];
    return playlistImage;
}

/* VideoView specific bindings */
- (NSArray *)childrenInVideoView
{
    return [[[self subitems] flatAspect] valueForKeyPath:@"media"];
}
- (NSString *)descriptionInVideoView
{
    return [[self metaDictionary] objectForKey:@"title"];
}

/* mediaListView specific bindings */
- (NSImage *)stateAsImage
{
    static NSImage * playing = nil;
    static NSImage * error = nil;

    if(!playing)
        playing = [[NSImage imageNamed:@"volume_high.png"] retain];
    if(!error)
        error = [[NSImage imageNamed:@"dialog-error.png"] retain];

    if( [self state] == VLCMediaStatePlaying )
        return playing;
    else if( [self state] == VLCMediaStateBuffering )
        return playing;
    else if( [self state] == VLCMediaStateError )
        return error;

    return nil;
}
@end

@implementation VLCMediaPlayer (VLCAppBindings)
+ (void)initialize
{
    [self setKeys:[NSArray arrayWithObjects:@"playing", @"media", nil] triggerChangeNotificationsForDependentKey:@"description"];
}

- (NSString *)description
{
    if([self media])
        return [self valueForKeyPath:@"media.metaDictionary.title"];
    else
        return @"VLC Media Player";
}
@end
