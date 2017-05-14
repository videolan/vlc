/*****************************************************************************
 * VLCAddonsWindowController.m: Addons manager for the Mac
 ****************************************************************************
 * Copyright (C) 2014 VideoLAN and authors
 * Author:       Felix Paul Kühne <fkuehne # videolan.org>
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

#import <vlc_common.h>
#import <vlc_addons.h>

#import "VLCAddonsWindowController.h"
#import "VLCMain.h"
#import "VLCMainWindow.h"
#import "VLCAddonListItem.h"

@interface VLCAddonsWindowController() <NSTableViewDataSource, NSTableViewDelegate>
{
    addons_manager_t *_manager;
    NSMutableArray *_addons;
    NSArray *_displayedAddons;
    BOOL _shouldRefreshSideBarOnAddonChange;
}

- (void)addAddon:(NSValue *)o_value;
- (void)discoveryEnded;
- (void)addonChanged:(NSValue *)o_value;

@end

static void addonFoundCallback( addons_manager_t *manager,
                                addon_entry_t *entry )
{
    VLCAddonsWindowController *controller = (__bridge VLCAddonsWindowController *) manager->owner.sys;

    @autoreleasepool {
        [controller performSelectorOnMainThread:@selector(addAddon:) withObject:[[VLCAddonListItem alloc] initWithAddon:entry] waitUntilDone:NO];
    }
}

static void addonsDiscoveryEndedCallback( addons_manager_t *manager )
{
    VLCAddonsWindowController *controller = (__bridge VLCAddonsWindowController *) manager->owner.sys;

    @autoreleasepool {
        [controller performSelectorOnMainThread:@selector(discoveryEnded) withObject:nil waitUntilDone:NO];
    }
}

static void addonChangedCallback( addons_manager_t *manager,
                                  addon_entry_t *entry )
{
    VLCAddonsWindowController *controller = (__bridge VLCAddonsWindowController *) manager->owner.sys;

    @autoreleasepool {
        [controller performSelectorOnMainThread:@selector(addonChanged:) withObject:[[VLCAddonListItem alloc] initWithAddon:entry] waitUntilDone:NO];
    }
}

@implementation VLCAddonsWindowController

#pragma mark - object handling

- (id)init
{
    self = [super initWithWindowNibName:@"AddonManager"];
    if (self) {
        [self setWindowFrameAutosaveName:@"addons"];
        _addons = [[NSMutableArray alloc] init];
    }

    return self;
}

- (void)dealloc
{
    if (_manager)
        addons_manager_Delete(_manager);
}

#pragma mark - UI handling

- (void)windowDidLoad
{
    [_typeSwitcher removeAllItems];
    [_typeSwitcher addItemWithTitle:_NS("All")];
    [[_typeSwitcher lastItem] setTag: -1];
    /* no skins on OS X so far
    [_typeSwitcher addItemWithTitle:_NS("Skins")];
    [[_typeSwitcher lastItem] setTag:ADDON_SKIN2]; */
    [_typeSwitcher addItemWithTitle:_NS("Playlist parsers")];
    [[_typeSwitcher lastItem] setTag:ADDON_PLAYLIST_PARSER];
    [_typeSwitcher addItemWithTitle:_NS("Service Discovery")];
    [[_typeSwitcher lastItem] setTag:ADDON_SERVICE_DISCOVERY];
    [_typeSwitcher addItemWithTitle:_NS("Interfaces")];
    [[_typeSwitcher lastItem] setTag:ADDON_INTERFACE];
    [_typeSwitcher addItemWithTitle:_NS("Art and meta fetchers")];
    [[_typeSwitcher lastItem] setTag:ADDON_META];
    [_typeSwitcher addItemWithTitle:_NS("Extensions")];
    [[_typeSwitcher lastItem] setTag:ADDON_EXTENSION];

    [_localAddonsOnlyCheckbox setTitle:_NS("Show Installed Only")];
    [_localAddonsOnlyCheckbox setState:NSOffState];
    [_downloadCatalogButton setTitle:_NS("Find more addons online")];
    [_spinner setUsesThreadedAnimation:YES];

    [self updateInstallButton:NO];
    [_installButton setHidden:YES];

    [_name setStringValue:@""];
    [_author setStringValue:@""];
    [_version setStringValue:@""];
    [_description setString:@""];
    [[self window] setTitle:_NS("Addons Manager")];

    [[[_addonsTable tableColumnWithIdentifier:@"installed"] headerCell] setStringValue:_NS("Installed")];
    [[[_addonsTable tableColumnWithIdentifier:@"name"] headerCell] setStringValue:_NS("Name")];
    [[[_addonsTable tableColumnWithIdentifier:@"author"] headerCell] setStringValue:_NS("Author")];
    [[[_addonsTable tableColumnWithIdentifier:@"type"] headerCell] setStringValue:_NS("Type")];

    struct addons_manager_owner owner =
    {
        (__bridge void *)self,
        addonFoundCallback,
        addonsDiscoveryEndedCallback,
        addonChangedCallback,
    };

    _manager = addons_manager_New((vlc_object_t *)getIntf(), &owner);
    if (!_manager)
        return;

    [self _findInstalled];
}

- (IBAction)switchType:(id)sender
{
    [self _refactorDataModel];
}

- (IBAction)toggleLocalCheckbox:(id)sender
{
    [self _refactorDataModel];
}

- (IBAction)downloadCatalog:(id)sender
{
    [self _findNewAddons];
    [_downloadCatalogButton setHidden:YES];
    [_localAddonsOnlyCheckbox setHidden:NO];
}

- (IBAction)installSelection:(id)sender
{
    NSInteger selectedRow = [_addonsTable selectedRow];
    if (selectedRow > _displayedAddons.count - 1 || selectedRow < 0)
        return;

    VLCAddonListItem *currentAddon = [_displayedAddons objectAtIndex:selectedRow];
    [self _installAddonWithID:[currentAddon uuid] type:[currentAddon type]];

    [_installButton setEnabled:NO];
}

- (IBAction)uninstallSelection:(id)sender
{
    NSInteger selectedRow = [_addonsTable selectedRow];
    if (selectedRow > _displayedAddons.count - 1 || selectedRow < 0)
        return;

    VLCAddonListItem *currentAddon = [_displayedAddons objectAtIndex:selectedRow];
    [self _removeAddonWithID:[currentAddon uuid] type:[currentAddon type]];

    [_installButton setEnabled:NO];
}

- (void)updateInstallButton:(BOOL)b_is_installed
{
    [_installButton setHidden:NO];
    [_installButton setEnabled:YES];

    if (b_is_installed) {
        [_installButton setTitle:_NS("Uninstall")];
        [_installButton setAction:@selector(uninstallSelection:)];
    } else {
        [_installButton setTitle:_NS("Install")];
        [_installButton setAction:@selector(installSelection:)];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [_displayedAddons count];
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
    NSInteger selectedRow = [_addonsTable selectedRow];
    if (selectedRow > _displayedAddons.count - 1 || selectedRow < 0) {
        [_name setStringValue:@""];
        [_author setStringValue:@""];
        [_version setStringValue:@""];
        [_description setString:@""];
        [_installButton setHidden:YES];
        return;
    }

    VLCAddonListItem *currentItem = [_displayedAddons objectAtIndex:selectedRow];
    [_name setStringValue:[currentItem name]];
    [_author setStringValue:[currentItem author]];
    [_version setStringValue:[currentItem version]];

    // Parse HTML description properly
    NSMutableString *htmlDescription = [NSMutableString stringWithFormat:@"<style>body{ font-family: -apple-system-body, -apple-system, HelveticaNeue, Arial, sans-serif; }</style>%@", [currentItem description]];
    [htmlDescription replaceOccurrencesOfString:@"\n" withString:@"<br />" options:NSLiteralSearch range:NSMakeRange(0, [htmlDescription length])];
    NSAttributedString *attributedDescription = [[NSAttributedString alloc] initWithHTML:[htmlDescription dataUsingEncoding:NSUTF8StringEncoding]
                                                                      documentAttributes:NULL];
    [[_description textStorage] setAttributedString:attributedDescription];

    [self updateInstallButton:[currentItem isInstalled]];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    NSString *identifier = [aTableColumn identifier];
    if ([identifier isEqualToString:@"installed"]) {
        return [[_displayedAddons objectAtIndex:rowIndex] isInstalled] ? @"✔" : @"✘";
    } else if([identifier isEqualToString:@"name"])
        return [[_displayedAddons objectAtIndex:rowIndex] name];

    return @"";
}

#pragma mark - data handling

- (void)addAddon:(VLCAddonListItem *)entry
{
    /* no skin support on OS X so far */
    if ([entry type] != ADDON_SKIN2)
        [_addons addObject:entry];
}

- (void)discoveryEnded
{
    [self _refactorDataModel];
    [_spinner stopAnimation:nil];
}

- (void)addonChanged:(VLCAddonListItem *)entry
{
    [self _refactorDataModel];
    if (_shouldRefreshSideBarOnAddonChange) {
        [[[VLCMain sharedInstance] mainWindow] performSelector:@selector(reloadSidebar) withObject:nil afterDelay:0.5];
        _shouldRefreshSideBarOnAddonChange = NO;
    }
}

#pragma mark - helpers

- (void)_refactorDataModel
{
    BOOL installedOnly = _localAddonsOnlyCheckbox.state == NSOnState;
    int type = [[_typeSwitcher selectedItem] tag];

    NSUInteger count = _addons.count;
    NSMutableArray *filteredItems = [[NSMutableArray alloc] initWithCapacity:count];
    for (NSUInteger x = 0; x < count; x++) {
        VLCAddonListItem *currentItem = [_addons objectAtIndex:x];
        if (type != -1) {
            if ([currentItem type] == type) {
                if (installedOnly) {
                    if ([currentItem isInstalled])
                        [filteredItems addObject:currentItem];
                } else
                    [filteredItems addObject:currentItem];
            }
        } else {
            if (installedOnly) {
                if ([currentItem isInstalled])
                    [filteredItems addObject:currentItem];
            } else
                [filteredItems addObject:currentItem];
        }
    }

    _displayedAddons = [NSArray arrayWithArray:filteredItems];

    // update ui
    [_addonsTable reloadData];
    [[NSNotificationCenter defaultCenter] postNotificationName:NSTableViewSelectionDidChangeNotification object:_addonsTable];
}

- (void)_findNewAddons
{
    [_spinner startAnimation:nil];
    addons_manager_Gather(_manager, "repo://");
}

/* FIXME: un-used */
- (void)_findDesignatedAddon:(NSString *)uri
{
    addons_manager_Gather(_manager, [uri UTF8String]);
}

- (void)_findInstalled
{
    addons_manager_LoadCatalog(_manager);

    // enqueue, to process the addons first
    [self performSelectorOnMainThread:@selector(_refactorDataModel) withObject:nil waitUntilDone:NO];
}

- (void)_installAddonWithID:(NSData *)o_data type:(addon_type_t)type
{
    addon_uuid_t uuid;
    [o_data getBytes:uuid length:sizeof(uuid)];

    if (type == ADDON_SERVICE_DISCOVERY)
        _shouldRefreshSideBarOnAddonChange = YES;

    addons_manager_Install(_manager, uuid);
}

- (void)_removeAddonWithID:(NSData *)o_data type:(addon_type_t)type
{
    addon_uuid_t uuid;
    [o_data getBytes:uuid length:sizeof(uuid)];

    if (type == ADDON_SERVICE_DISCOVERY)
        _shouldRefreshSideBarOnAddonChange = YES;

    addons_manager_Remove(_manager, uuid);
}

- (NSString *)_getAddonType:(int)i_type
{
    switch (i_type)
    {
        case ADDON_SKIN2:
            return _NS("Skins");
        case ADDON_PLAYLIST_PARSER:
            return _NS("Playlist parsers");
        case ADDON_SERVICE_DISCOVERY:
            return _NS("Service Discovery");
        case ADDON_EXTENSION:
            return _NS("Extensions");
        default:
            return _NS("Unknown");
    }
}

@end
