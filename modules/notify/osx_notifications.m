/*****************************************************************************
 * osx_notifications.m : OS X notification plugin
 *****************************************************************************
 * VLC specific code:
 *
 * Copyright © 2008,2011,2012,2015 the VideoLAN team
 * $Id$
 *
 * Authors: Rafaël Carré <funman@videolanorg>
 *          Felix Paul Kühne <fkuehne@videolan.org
 *          Marvin Scholz <epirat07@gmail.com>
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
 *
 * ---
 *
 * Growl specific code, ripped from growlnotify:
 *
 * Copyright (c) The Growl Project, 2004-2005
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Growl nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#pragma clang diagnostic ignored "-Wunguarded-availability"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <Growl/Growl.h>

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_meta.h>
#include <vlc_interface.h>
#include <vlc_url.h>

/*****************************************************************************
 * intf_sys_t, VLCGrowlDelegate
 *****************************************************************************/
@interface VLCGrowlDelegate : NSObject <GrowlApplicationBridgeDelegate>
{
    NSString *applicationName;
    NSString *notificationType;
    NSMutableDictionary *registrationDictionary;
    id lastNotification;
    bool isInForeground;
    bool hasNativeNotifications;
    intf_thread_t *interfaceThread;
}

- (id)initWithInterfaceThread:(intf_thread_t *)thread;
- (void)registerToGrowl;
- (void)notifyWithTitle:(const char *)title
                 artist:(const char *)artist
                  album:(const char *)album
              andArtUrl:(const char *)url;
@end

struct intf_sys_t
{
    VLCGrowlDelegate *o_growl_delegate;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static int InputCurrent( vlc_object_t *, const char *,
                      vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/
vlc_module_begin ()
set_category( CAT_INTERFACE )
set_subcategory( SUBCAT_INTERFACE_CONTROL )
set_shortname( "OSX-Notifications" )
add_shortcut( "growl" )
set_description( N_("OS X Notification Plugin") )
set_capability( "interface", 0 )
set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist = pl_Get( p_intf );
    intf_sys_t *p_sys = p_intf->p_sys = calloc( 1, sizeof(intf_sys_t) );

    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->o_growl_delegate = [[VLCGrowlDelegate alloc] initWithInterfaceThread:p_intf];
    if( !p_sys->o_growl_delegate )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    var_AddCallback( p_playlist, "input-current", InputCurrent, p_intf );

    [p_sys->o_growl_delegate registerToGrowl];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist = pl_Get( p_intf );
    intf_sys_t *p_sys = p_intf->p_sys;

    var_DelCallback( p_playlist, "input-current", InputCurrent, p_intf );

    [GrowlApplicationBridge setGrowlDelegate:nil];
    [p_sys->o_growl_delegate release];
    free( p_sys );
}

/*****************************************************************************
 * InputCurrent: Current playlist item changed callback
 *****************************************************************************/
static int InputCurrent( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED(oldval);

    intf_thread_t *p_intf = (intf_thread_t *)param;
    intf_sys_t *p_sys = p_intf->p_sys;
    input_thread_t *p_input = newval.p_address;
    char *psz_title = NULL;
    char *psz_artist = NULL;
    char *psz_album = NULL;
    char *psz_arturl = NULL;

    if( !p_input )
        return VLC_SUCCESS;

    input_item_t *p_item = input_GetItem( p_input );
    if( !p_item )
        return VLC_SUCCESS;

    /* Get title */
    psz_title = input_item_GetNowPlayingFb( p_item );
    if( !psz_title )
        psz_title = input_item_GetTitleFbName( p_item );

    if( EMPTY_STR( psz_title ) )
    {
        free( psz_title );
        return VLC_SUCCESS;
    }

    /* Get Artist name */
    psz_artist = input_item_GetArtist( p_item );
    if( EMPTY_STR( psz_artist ) )
        FREENULL( psz_artist );

    /* Get Album name */
    psz_album = input_item_GetAlbum( p_item ) ;
    if( EMPTY_STR( psz_album ) )
        FREENULL( psz_album );

    /* Get Art path */
    psz_arturl = input_item_GetArtURL( p_item );
    if( psz_arturl )
    {
        char *psz = vlc_uri2path( psz_arturl );
        free( psz_arturl );
        psz_arturl = psz;
    }

    [p_sys->o_growl_delegate notifyWithTitle:psz_title
                                      artist:psz_artist
                                       album:psz_album
                                   andArtUrl:psz_arturl];

    free( psz_title );
    free( psz_artist );
    free( psz_album );
    free( psz_arturl );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLCGrowlDelegate
 *****************************************************************************/
@implementation VLCGrowlDelegate

- (id)initWithInterfaceThread:(intf_thread_t *)thread {
    if( !( self = [super init] ) )
        return nil;

    @autoreleasepool {
        // Subscribe to notifications to determine if VLC is in foreground or not
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(applicationActiveChange:)
                                                     name:NSApplicationDidBecomeActiveNotification
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(applicationActiveChange:)
                                                     name:NSApplicationDidResignActiveNotification
                                                   object:nil];
    }
    // Start in background
    isInForeground = NO;

    // Check for native notification support
    Class userNotificationClass = NSClassFromString(@"NSUserNotification");
    Class userNotificationCenterClass = NSClassFromString(@"NSUserNotificationCenter");
    hasNativeNotifications = (userNotificationClass && userNotificationCenterClass) ? YES : NO;

    lastNotification = nil;
    applicationName = nil;
    notificationType = nil;
    registrationDictionary = nil;
    interfaceThread = thread;

    return self;
}

- (void)dealloc
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
    // Clear the remaining lastNotification in Notification Center, if any
    @autoreleasepool {
        if (lastNotification && hasNativeNotifications) {
            [NSUserNotificationCenter.defaultUserNotificationCenter
             removeDeliveredNotification:(NSUserNotification *)lastNotification];
            [lastNotification release];
        }
        [[NSNotificationCenter defaultCenter] removeObserver:self];
    }
#endif
#pragma clang diagnostic pop

    // Release everything
    [applicationName release];
    [notificationType release];
    [registrationDictionary release];
    [super dealloc];
}

- (void)registerToGrowl
{
    @autoreleasepool {
        applicationName = [[NSString alloc] initWithUTF8String:_( "VLC media player" )];
        notificationType = [[NSString alloc] initWithUTF8String:_( "New input playing" )];

        NSArray *defaultAndAllNotifications = [NSArray arrayWithObject: notificationType];
        registrationDictionary = [[NSMutableDictionary alloc] init];
        [registrationDictionary setObject:defaultAndAllNotifications
                                   forKey:GROWL_NOTIFICATIONS_ALL];
        [registrationDictionary setObject:defaultAndAllNotifications
                                   forKey: GROWL_NOTIFICATIONS_DEFAULT];

        [GrowlApplicationBridge setGrowlDelegate:self];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
        if (hasNativeNotifications) {
            [[NSUserNotificationCenter defaultUserNotificationCenter]
             setDelegate:(id<NSUserNotificationCenterDelegate>)self];
        }
#endif
#pragma clang diagnostic pop
    }
}

- (void)notifyWithTitle:(const char *)title
                 artist:(const char *)artist
                  album:(const char *)album
              andArtUrl:(const char *)url
{
    @autoreleasepool {
        // Do not notify if in foreground
        if (isInForeground)
            return;

        // Init Cover
        NSData *coverImageData = nil;
        NSImage *coverImage = nil;

        if (url) {
            coverImageData = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:url]];
            coverImage = [[NSImage alloc] initWithData:coverImageData];
        }

        // Init Track info
        NSString *titleStr = nil;
        NSString *artistStr = nil;
        NSString *albumStr = nil;

        if (title) {
            titleStr = [NSString stringWithUTF8String:title];
        } else {
            // Without title, notification makes no sense, so return here
            // title should never be empty, but better check than crash.
            [coverImage release];
            return;
        }
        if (artist)
            artistStr = [NSString stringWithUTF8String:artist];
        if (album)
            albumStr = [NSString stringWithUTF8String:album];

        // Notification stuff
        if ([GrowlApplicationBridge isGrowlRunning]) {
            // Make the Growl notification string
            NSString *desc = nil;

            if (artistStr && albumStr) {
                desc = [NSString stringWithFormat:@"%@\n%@ [%@]", titleStr, artistStr, albumStr];
            } else if (artistStr) {
                desc = [NSString stringWithFormat:@"%@\n%@", titleStr, artistStr];
            } else {
                desc = titleStr;
            }

            // Send notification
            [GrowlApplicationBridge notifyWithTitle:[NSString stringWithUTF8String:_("Now playing")]
                                        description:desc
                                   notificationName:notificationType
                                           iconData:coverImageData
                                           priority:0
                                           isSticky:NO
                                       clickContext:nil
                                         identifier:@"VLCNowPlayingNotification"];
        } else if (hasNativeNotifications) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
            // Make the OS X notification and string
            NSUserNotification *notification = [NSUserNotification new];
            NSString *desc = nil;

            if (artistStr && albumStr) {
                desc = [NSString stringWithFormat:@"%@ – %@", artistStr, albumStr];
            } else if (artistStr) {
                desc = artistStr;
            }

            notification.title              = titleStr;
            notification.subtitle           = desc;
            notification.hasActionButton    = YES;
            notification.actionButtonTitle  = [NSString stringWithUTF8String:_("Skip")];

            // Private APIs to set cover image, see rdar://23148801
            // and show action button, see rdar://23148733
            [notification setValue:coverImage forKey:@"_identityImage"];
            [notification setValue:@(YES) forKey:@"_showsButtons"];
            [NSUserNotificationCenter.defaultUserNotificationCenter deliverNotification:notification];
            [notification release];
#endif
#pragma clang diagnostic pop
        }

        // Release stuff
        [coverImage release];
    }
}

/*****************************************************************************
 * Delegate methods
 *****************************************************************************/
- (NSDictionary *)registrationDictionaryForGrowl
{
    return registrationDictionary;
}

- (NSString *)applicationNameForGrowl
{
    return applicationName;
}

- (void)applicationActiveChange:(NSNotification *)n {
    if (n.name == NSApplicationDidBecomeActiveNotification)
        isInForeground = YES;
    else if (n.name == NSApplicationDidResignActiveNotification)
        isInForeground = NO;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
- (void)userNotificationCenter:(NSUserNotificationCenter *)center
       didActivateNotification:(NSUserNotification *)notification
{
    // Skip to next song
    if (notification.activationType == NSUserNotificationActivationTypeActionButtonClicked) {
        playlist_Next(pl_Get(interfaceThread));
    }
}

- (void)userNotificationCenter:(NSUserNotificationCenter *)center
        didDeliverNotification:(NSUserNotification *)notification
{
    // Only keep the most recent notification in the Notification Center
    if (lastNotification) {
        [center removeDeliveredNotification: (NSUserNotification *)lastNotification];
        [lastNotification release];
    }
    [notification retain];
    lastNotification = notification;
}
#endif
#pragma clang diagnostic pop
@end
