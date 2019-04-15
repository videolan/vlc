/*****************************************************************************
 * osx_notifications.m : macOS notification plugin
 *
 * This plugin provides support for macOS notifications on current playlist
 * item changes.
 *****************************************************************************
 * Copyright © 2008, 2011, 2012, 2015, 2018, 2019 the VideoLAN team
 *
 * Authors: Marvin Scholz <epirat07@gmail.com>
 *          Felix Paul Kühne <fkuehne # videolan.org>
 *          Rafaël Carré <funman@videolanorg>
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
 */

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import <Cocoa/Cocoa.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_player.h>
#include <vlc_interface.h>
#include <vlc_url.h>

#pragma mark -
#pragma mark Class interfaces
@interface VLCNotificationDelegate : NSObject <NSUserNotificationCenterDelegate>
{
    /** Holds the last notification so it can be cleared when the next one is delivered */
    NSUserNotification * _Nullable _lastNotification;

    /* the playlist reference */
    vlc_playlist_t *_p_playlist;

    /* the listener ID for player notifications */
    vlc_player_listener_id *_playerListenerID;
}

/**
 * Initializes a new  VLCNotification Delegate with a given intf_thread_t
 */
- (instancetype)initWithInterfaceThread:(intf_thread_t * _Nonnull)intf_thread;

/**
 * Delegate method called when the current input item changed
 */
- (void)currentInputItemChanged:(input_item_t *)inputItem;

@end

struct intf_sys_t
{
    void *vlcNotificationDelegate;
};

#pragma mark -
#pragma mark callback

static void on_current_media_changed(vlc_player_t *player,
                                     input_item_t *p_input_item, void *data)
{

    if (p_input_item)
        input_item_Hold(p_input_item);

    dispatch_async(dispatch_get_main_queue(), ^{
        VLCNotificationDelegate *notificationDelegate = (__bridge VLCNotificationDelegate *)data;
        [notificationDelegate currentInputItemChanged:p_input_item];
    });
}

#pragma mark -
#pragma mark C module functions
/*
 * Open: Initialization of the module
 */
static int Open(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys = calloc(1, sizeof(intf_sys_t));

    if (!p_sys)
        return VLC_ENOMEM;

    @autoreleasepool {
        VLCNotificationDelegate *notificationDelegate =
            [[VLCNotificationDelegate alloc] initWithInterfaceThread:p_intf];
        
        if (notificationDelegate == nil) {
            free(p_sys);
            return VLC_ENOMEM;
        }
        
        p_sys->vlcNotificationDelegate = (__bridge_retained void*)notificationDelegate;
    }

    return VLC_SUCCESS;
}

/*
 * Close: Destruction of the module
 */
static void Close(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    @autoreleasepool {
        // Transfer ownership of notification delegate object back to ARC
        VLCNotificationDelegate *notificationDelegate =
            (__bridge_transfer VLCNotificationDelegate*)p_sys->vlcNotificationDelegate;

        // Ensure the object is deallocated
        notificationDelegate = nil;
    }

    free(p_sys);
}

/**
  * Transfers a null-terminated UTF-8 C "string" to a NSString
  * in a way that the NSString takes ownership of it.
  *
  * \warning    After calling this function, passed cStr must not be used anymore!
  *
  * \param      cStr  Pointer to a zero-terminated UTF-8 encoded char array
  *
  * \return     An NSString instance that uses cStr as internal data storage and
  *             frees it when done. On error, nil is returned and cStr is freed.
  */
static inline NSString* CharsToNSString(char * _Nullable cStr)
{
    if (!cStr)
        return nil;

    NSString *resString = [[NSString alloc] initWithBytesNoCopy:cStr
                                                         length:strlen(cStr)
                                                       encoding:NSUTF8StringEncoding
                                                   freeWhenDone:YES];
    if (unlikely(resString == nil))
        free(cStr);

    return resString;
}

#pragma mark -
#pragma mark Class implementation
@implementation VLCNotificationDelegate

- (id)initWithInterfaceThread:(intf_thread_t *)intf_thread
{
    self = [super init];
    
    if (self) {

        _p_playlist = vlc_intf_GetMainPlaylist(intf_thread);
        vlc_player_t *player = vlc_playlist_GetPlayer(_p_playlist);
        static const struct vlc_player_cbs player_cbs =
        {
            .on_current_media_changed = on_current_media_changed
        };
        vlc_player_Lock(player);
        _playerListenerID = vlc_player_AddListener(player, &player_cbs, (__bridge void *)self);
        vlc_player_Unlock(player);

        [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:self];
    }
    
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    // Clear a remaining lastNotification in Notification Center, if any
    if (_lastNotification) {
        [[NSUserNotificationCenter defaultUserNotificationCenter]
         removeDeliveredNotification:_lastNotification];
        _lastNotification = nil;
    }

    if (_p_playlist) {
        if (_playerListenerID) {
            vlc_player_t *player = vlc_playlist_GetPlayer(_p_playlist);
            vlc_player_Lock(player);
            vlc_player_RemoveListener(player, _playerListenerID);
            vlc_player_Unlock(player);
        }
    }
}

- (void)currentInputItemChanged:(input_item_t *)inputItem
{
    if (inputItem == NULL) {
        return;
    }
    
    // Get title, first try now playing
    NSString *title = CharsToNSString(input_item_GetNowPlayingFb(inputItem));

    // Fallback to item title or name
    if ([title length] == 0)
        title = CharsToNSString(input_item_GetTitleFbName(inputItem));

    // If there is still not title, do not notify
    if (unlikely([title length] == 0)) {
        return;
    }

    // Get artist name
    NSString *artist = CharsToNSString(input_item_GetArtist(inputItem));

    // Get album name
    NSString *album = CharsToNSString(input_item_GetAlbum(inputItem));

    // Get coverart path
    NSString *artPath = nil;

    char *psz_arturl = input_item_GetArtURL(inputItem);
    if (psz_arturl) {
        artPath = CharsToNSString(vlc_uri2path(psz_arturl));
        free(psz_arturl);
    }

    // Construct final description string
    NSString *desc = nil;

    if (artist && album) {
        desc = [NSString stringWithFormat:@"%@ – %@", artist, album];
    } else if (artist) {
        desc = artist;
    }
    
    // Notify!
    [self notifyWithTitle:title description:desc imagePath:artPath];

    input_item_Release(inputItem);
}

/*
 * Called when the user interacts with a notification
 */
- (void)userNotificationCenter:(NSUserNotificationCenter *)center
       didActivateNotification:(NSUserNotification *)notification
{
    // Check if notification button ("Skip") was clicked
    if (notification.activationType == NSUserNotificationActivationTypeActionButtonClicked) {
        // Skip to next song
        vlc_playlist_Lock(_p_playlist);
        vlc_playlist_Next(_p_playlist);
        vlc_playlist_Unlock(_p_playlist);
    }
}

/*
 * Called when a new notification was delivered
 */
- (void)userNotificationCenter:(NSUserNotificationCenter *)center
        didDeliverNotification:(NSUserNotification *)notification
{
    // Only keep the most recent notification in the Notification Center
    if (_lastNotification)
        [center removeDeliveredNotification:_lastNotification];

    _lastNotification = notification;
}

/*
 * Send a notification to the default user notification center
 */
- (void)notifyWithTitle:(NSString * _Nonnull)titleText
            description:(NSString * _Nullable)descriptionText
              imagePath:(NSString * _Nullable)imagePath
{
    NSImage *image = nil;

    // Load image if any
    if (imagePath) {
        image = [[NSImage alloc] initWithContentsOfFile:imagePath];
    }

    // Create notification
    NSUserNotification *notification = [NSUserNotification new];

    notification.title              = titleText;
    notification.subtitle           = descriptionText;
    notification.hasActionButton    = YES;
    notification.actionButtonTitle  = [NSString stringWithUTF8String:_("Skip")];
    
    // Try to set private properties
    @try {
        // Private API to set cover image, see rdar://23148801
        [notification setValue:image forKey:@"_identityImage"];
        // Private API to show action button, see rdar://23148733
        [notification setValue:@(YES) forKey:@"_showsButtons"];
    } @catch (NSException *exception) {
        if (exception.name == NSUndefinedKeyException)
            NSLog(@"VLC macOS notifcations plugin failed to set private notification values.");
        else
            @throw exception;
    }

    // Send notification
    [[NSUserNotificationCenter defaultUserNotificationCenter]
        deliverNotification:notification];
}

@end


#pragma mark -
#pragma mark VLC Module descriptor

vlc_module_begin()
    set_shortname("OSX-Notifications")
    set_description(N_("macOS notifications plugin"))
    add_shortcut("growl") // Kept for backwards compatibility
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_CONTROL)
    set_capability("interface", 0)
    set_callbacks(Open, Close)
vlc_module_end()
