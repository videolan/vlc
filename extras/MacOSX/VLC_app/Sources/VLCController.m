/*****************************************************************************
 * VLCController.m: VLC.app main controller
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

#import <VLC/VLC.h>

#import "VLCController.h" 
#import "VLCAppAdditions.h" 
#import "VLCValueTransformer.h" 

@interface VLCController ()
@property (readwrite,retain) NSArray * categories;
@end

/******************************************************************************
 * VLCBrowsableVideoView
 */
@implementation VLCController
@synthesize categories;

- (void)awakeFromNib
{
    /***********************************
     * Register our bindings value transformer
     */
    VLCFloat10000FoldTransformer *float100fold;
    float100fold = [[[VLCFloat10000FoldTransformer alloc] init] autorelease];
    [NSValueTransformer setValueTransformer:(id)float100fold forName:@"Float10000FoldTransformer"];
    VLCNonNilAsBoolTransformer *nonNilAsBool;
    nonNilAsBool = [[[VLCNonNilAsBoolTransformer alloc] init] autorelease];
    [NSValueTransformer setValueTransformer:(id)nonNilAsBool forName:@"NonNilAsBoolTransformer"];
    VLCURLToRepresentedFileNameTransformer *urlToRepresentedFileName;
    urlToRepresentedFileName = [[[VLCURLToRepresentedFileNameTransformer alloc] init] autorelease];
    [NSValueTransformer setValueTransformer:(id)urlToRepresentedFileName forName:@"URLToRepresentedFileNameTransformer"];

    /***********************************
     * categories: Main content
     */
    NSArray * mediaDiscoverers = [NSArray arrayWithObjects:
        [[[VLCMediaDiscoverer alloc] initWithName:@"shoutcasttv"] autorelease],
        [[[VLCMediaDiscoverer alloc] initWithName:@"shoutcast"] autorelease],
        [[[VLCMediaDiscoverer alloc] initWithName:@"sap"] autorelease],
        [[[VLCMediaDiscoverer alloc] initWithName:@"freebox"] autorelease], nil];

    NSArray * playlists = [NSMutableArray arrayWithObjects:[VLCMedia mediaAsNodeWithName:@"Default Playlist"], nil];

    NSDictionary * playlistsAsDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                                [@"Playlists" uppercaseString], @"descriptionInCategoriesList",
                                @"Playlists", @"descriptionInVideoView",
                                [NSNumber numberWithBool:NO], @"selectableInCategoriesList",
                                playlists, @"childrenInCategoriesList",
                                playlists, @"childrenInVideoView",
                                nil];

    self.categories = [NSArray arrayWithObjects:
                    [NSMutableDictionary dictionaryWithObjectsAndKeys:
                        [@"Service Discovery" uppercaseString], @"descriptionInCategoriesList",
                        @"Service Discovery", @"descriptionInVideoView",
                        [NSNumber numberWithBool:NO], @"selectableInCategoriesList",
                        mediaDiscoverers, @"childrenInCategoriesList",
                        mediaDiscoverers, @"childrenInVideoView",
                        nil],
                    playlistsAsDictionary,
                    nil];

    /* Execution will continue in applicationDidFinishLaunching */
    [NSApp setDelegate:self];
}

- (void)newMainWindow:(id)sender
{
    if (![NSBundle loadNibNamed:@"MainWindow" owner:self])
    {
        NSLog(@"Warning! Could not load MainWindow file.\n");
    }
    /* We are done. Should be on screen if Visible at launch time is checked */
}

- (void)addPlaylist:(id)sender
{
    // TODO
    NSLog(@"unimplemented!");
}

@end

@implementation VLCController (NSAppDelegate)
- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    [self newMainWindow: self];
}
@end
