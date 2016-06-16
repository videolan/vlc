/*****************************************************************************
 * VLCRendererDialog.m: View controller class for the renderer dialog
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCRendererDialog.h"

#import "VLCRendererItem.h"
#import "intf.h"

#include <vlc_renderer_discovery.h>


@interface VLCRendererDialog ()
{
    IBOutlet NSTableView *tableView;
    IBOutlet NSArrayController *arrayController;

    NSMutableArray<VLCRendererDiscovery*> *renderer_discoveries;

    intf_thread_t          *p_intf;
    vlc_renderer_discovery *p_rd;
}
@end

@implementation VLCRendererDialog

- (id)init
{
    self = [super initWithWindowNibName:@"VLCRendererDialog"];
    if (self) {
        _rendererItems = [[NSMutableArray alloc] init];
        renderer_discoveries = [[NSMutableArray alloc] initWithCapacity:1];
        p_intf = getIntf();
    }
    return self;
}

- (void)dealloc
{
    [self stopRendererDiscoveries];
    [self clearRendererDiscoveries];
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    [self.window setDelegate:self];
    [self.window setTitle:_NS("Renderer selection")];

    [self loadRendererDiscoveries];
}

- (void)windowWillClose:(NSNotification *)notification
{
    // Stop all renderer discoveries here!
    [self stopRendererDiscoveries];
}

- (void)showWindow:(id)sender
{
    // Start all renderer discoveries here!
    [self startRendererDiscoveries];
    return [super showWindow:sender];
}

- (void)loadRendererDiscoveries
{
    playlist_t *playlist = pl_Get(p_intf);

    // Service Discovery subnodes
    char **ppsz_longnames;
    char **ppsz_names;

    if (vlc_rd_get_names(playlist, &ppsz_names, &ppsz_longnames) != VLC_SUCCESS) {
        return;
    }
    char **ppsz_name = ppsz_names;
    char **ppsz_longname = ppsz_longnames;

    for( ; *ppsz_name; ppsz_name++, ppsz_longname++) {
        VLCRendererDiscovery *dc = [[VLCRendererDiscovery alloc] initWithName:*ppsz_name andLongname:*ppsz_longname];
        [dc setDelegate:self];
        [dc startDiscovery];
        [renderer_discoveries addObject:dc];
    }
    free(ppsz_names);
    free(ppsz_longnames);
}

- (void)clearRendererDiscoveries
{
    [renderer_discoveries removeAllObjects];
}

- (void)startRendererDiscoveries
{
    for (VLCRendererDiscovery *dc in renderer_discoveries) {
        [dc startDiscovery];
    }
}

- (void)stopRendererDiscoveries
{
    for (VLCRendererDiscovery *dc in renderer_discoveries) {
        [dc stopDiscovery];
    }
}

- (IBAction)selectRenderer:(id)sender
{
    playlist_t *playlist = pl_Get(p_intf);
    VLCRendererItem* item = [arrayController.selectedObjects firstObject];
    if (item) {
        [item setSoutForPlaylist:playlist];
    } else {
        [self unsetSoutForPlaylist:playlist];
    }
}

- (void)unsetSoutForPlaylist:(playlist_t*)playlist
{
    var_SetString(playlist, "sout", "");
}

#pragma mark VLCRendererDiscoveryDelegate methods
- (void)addedRendererItem:(VLCRendererItem *)item from:(VLCRendererDiscovery *)sender
{
    [arrayController addObject:item];
}

- (void)removedRendererItem:(VLCRendererItem *)item from:(VLCRendererDiscovery *)sender
{
    [arrayController removeObject:item];
}

@end
