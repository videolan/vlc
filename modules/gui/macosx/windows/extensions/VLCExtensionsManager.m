/*****************************************************************************
 * VLCExtensionsManager.m: Extensions manager for Cocoa
 ****************************************************************************
 * Copyright (C) 2009-2012 VideoLAN and authors
 *
 * Authors: Brendon Justin <brendonjustin@gmail.com>,
 *          Jean-Philippe André < jpeg # videolan.org >
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

#import "VLCExtensionsManager.h"

#import <assert.h>

#import <vlc_modules.h>
#import <vlc_extensions.h>

#import "main/VLCMain.h"
#import "extensions/NSString+Helpers.h"
#import "windows/extensions/VLCExtensionsDialogProvider.h"

#define MENU_MAP(a,e) ((uint32_t)((((uint16_t)a) << 16) | ((uint16_t)e)))
#define MENU_GET_ACTION(a) ((uint16_t)(((uint32_t)a) >> 16))
#define MENU_GET_EXTENSION(a) ((uint16_t)(((uint32_t)a) & 0xFFFF))

@interface VLCExtensionsManager()
{
    extensions_manager_t *p_extensions_manager;
    VLCExtensionsDialogProvider *_extensionDialogProvider;

    BOOL b_failed; ///< Flag set to true if we could not load the module
}
@end

@implementation VLCExtensionsManager

- (id)init
{
    self = [super init];

    if (self) {
        p_extensions_manager = NULL;
        _extensionDialogProvider = [[VLCExtensionsDialogProvider alloc] init];

        _isUnloading = false;
        b_failed = false;
    }

    return self;
}

- (void)buildMenu:(NSMenu *)extMenu
{
    assert(extMenu != nil);

    /* check if stuff was loaded, which can fail */
    if (![self isLoaded]) {
        BOOL b_ret = [self loadExtensions];
        if (b_ret == NO)
            return;
    }
    intf_thread_t *p_intf = getIntf();

    vlc_mutex_lock(&p_extensions_manager->lock);

    extension_t *p_ext = NULL;
    int i_ext = 0;

    ARRAY_FOREACH(p_ext, p_extensions_manager->extensions) {
        bool b_Active = extension_IsActivated(p_extensions_manager, p_ext);

        NSString *titleString = toNSStr(p_ext->psz_title);

        if (b_Active && extension_HasMenu(p_extensions_manager, p_ext)) {
            NSMenu *submenu = [[NSMenu alloc] initWithTitle:titleString];
            NSMenuItem *submenuItem = [extMenu addItemWithTitle:titleString
                                                         action:nil
                                                  keyEquivalent:@""];

            [extMenu setSubmenu:submenu forItem:submenuItem];

            char **ppsz_titles = NULL;
            uint16_t *pi_ids = NULL;
            size_t i_num = 0;

            if (extension_GetMenu(p_extensions_manager, p_ext, &ppsz_titles, &pi_ids) == VLC_SUCCESS) {
                for (int i = 0; ppsz_titles[i] != NULL; ++i) {
                    ++i_num;
                    titleString = toNSStr(ppsz_titles[i]);
                    NSMenuItem *menuItem = [submenu addItemWithTitle:titleString
                                                              action:@selector(triggerMenu:)
                                                       keyEquivalent:@""];
                    [menuItem setTarget:self];
                    menuItem.tag = MENU_MAP(pi_ids[i], i_ext);

                    free(ppsz_titles[i]);
                }
                if (!i_num) {
                    NSMenuItem *menuItem = [submenu addItemWithTitle:@"Empty"
                                                              action:@selector(triggerMenu:)
                                                       keyEquivalent:@""];
                    [menuItem setEnabled:NO];
                }
                free(ppsz_titles);
                free(pi_ids);
            } else {
                msg_Warn(p_intf, "Could not get menu for extension '%s'",
                          p_ext->psz_title);
                NSMenuItem *menuItem = [submenu addItemWithTitle:@"Empty"
                                                          action:@selector(triggerMenu:)
                                                   keyEquivalent:@""];
                [menuItem setEnabled:NO];
            }

            [submenu addItem:[NSMenuItem separatorItem]];

            NSMenuItem *deactivateItem = [submenu addItemWithTitle:@"Deactivate"
                                                            action:@selector(triggerMenu:)
                                                     keyEquivalent:@""];
            [deactivateItem setTarget:self];
            deactivateItem.tag = MENU_MAP(0, i_ext);
        }
        else
        {
            NSMenuItem *menuItem = [extMenu addItemWithTitle:titleString
                                                     action:@selector(triggerMenu:)
                                              keyEquivalent:@""];
            [menuItem setTarget:self];

            if (!extension_TriggerOnly(p_extensions_manager, p_ext)) {
                if (b_Active)
                    [menuItem setState:NSOnState];
            }
            menuItem.tag = MENU_MAP(0, i_ext);
        }
        i_ext++;
    }

    vlc_mutex_unlock(&p_extensions_manager->lock);
}

- (BOOL)loadExtensions
{
    intf_thread_t *p_intf = getIntf();
    if (!p_extensions_manager) {
        p_extensions_manager = (extensions_manager_t*)vlc_object_create(p_intf, sizeof(extensions_manager_t));
        if (!p_extensions_manager) {
            b_failed = true;
            return false;
        }

        p_extensions_manager->p_module = module_need(p_extensions_manager, "extension", NULL, false);

        if (!p_extensions_manager->p_module) {
            msg_Err(p_intf, "Unable to load extensions module");
            vlc_object_delete(p_extensions_manager);
            p_extensions_manager = NULL;
            b_failed = true;
            return false;
        }

        _isUnloading = false;
    }
    b_failed = false;
    return true;
}

- (void)unloadExtensions
{
    if (!p_extensions_manager)
        return;
    _isUnloading = true;

    module_unneed(p_extensions_manager, p_extensions_manager->p_module);
    vlc_object_delete(p_extensions_manager);
    p_extensions_manager = NULL;
}

- (void)reloadExtensions
{
    [self unloadExtensions];
    [self loadExtensions];
}

- (void)triggerMenu:(id)sender
{
    intf_thread_t *p_intf = getIntf();
    uint32_t identifier = (unsigned int)[(NSMenuItem *)sender tag];

    uint16_t i_ext = MENU_GET_EXTENSION(identifier);
    uint16_t i_action = MENU_GET_ACTION(identifier);

    vlc_mutex_lock(&p_extensions_manager->lock);

    if ((int) i_ext > p_extensions_manager->extensions.i_size) {
        msg_Dbg(p_intf, "can't trigger extension with wrong id %d",
                 (int) i_ext);
        return;
    }

    extension_t *p_ext = ARRAY_VAL(p_extensions_manager->extensions, i_ext);
    assert(p_ext != NULL);

    vlc_mutex_unlock(&p_extensions_manager->lock);

    if (i_action == 0) {
        msg_Dbg(p_intf, "activating or triggering extension '%s', id %d",
                 p_ext->psz_title, i_ext);

        if (extension_TriggerOnly(p_extensions_manager, p_ext)) {
            extension_Trigger(p_extensions_manager, p_ext);
        } else {
            if (!extension_IsActivated(p_extensions_manager, p_ext))
                extension_Activate(p_extensions_manager, p_ext);
            else
                extension_Deactivate(p_extensions_manager, p_ext);
        }
    }
    else
    {
        msg_Dbg(p_intf, "triggering extension '%s', on menu with id = 0x%x",
                 p_ext->psz_title, i_action);

        extension_TriggerMenu(p_extensions_manager, p_ext, i_action);
    }
}

- (void)playingChanged:(int)state
{
    //This is unlikely, but can happen if no extension modules can be loaded.
    if (p_extensions_manager == NULL)
        return;
    vlc_mutex_lock(&p_extensions_manager->lock);

    extension_t *p_ext;
    ARRAY_FOREACH(p_ext, p_extensions_manager->extensions) {
        if (extension_IsActivated(p_extensions_manager, p_ext))
            extension_PlayingChanged(p_extensions_manager, p_ext, state);
    }

    vlc_mutex_unlock(&p_extensions_manager->lock);
}

- (void)metaChanged:(input_item_t *)p_input
{
    //This is unlikely, but can happen if no extension modules can be loaded.
    if (p_extensions_manager == NULL)
        return;
    vlc_mutex_lock(&p_extensions_manager->lock);
    extension_t *p_ext;
    ARRAY_FOREACH(p_ext, p_extensions_manager->extensions) {
        if (extension_IsActivated(p_extensions_manager, p_ext))
            extension_MetaChanged(p_extensions_manager, p_ext);
    }
    vlc_mutex_unlock(&p_extensions_manager->lock);
}

- (void)dealloc
{
    intf_thread_t *p_intf = getIntf();
    msg_Dbg(p_intf, "Deinitializing extensions manager");

    _extensionDialogProvider = nil;
    if (p_extensions_manager)
        vlc_object_delete(p_extensions_manager);
}

- (BOOL)isLoaded
{
    return p_extensions_manager != NULL;
}

- (BOOL)cannotLoad
{
    return self.isUnloading || b_failed;
}

@end
