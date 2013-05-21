/*****************************************************************************
 * growl.m : growl notification plugin
 *****************************************************************************
 * VLC specific code:
 *
 * Copyright © 2008,2011,2012 the VideoLAN team
 * $Id$
 *
 * Authors: Rafaël Carré <funman@videolanorg>
 *          Felix Paul Kühne <fkuehne@videolan.org
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
 * Growl specific code, ripped from growlnotify:
 *
 * Copyright (c) The Growl Project, 2004-2005
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Growl nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import <Foundation/Foundation.h>
#import <Growl/Growl.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_interface.h>
#include <vlc_url.h>


/*****************************************************************************
 * intf_sys_t, VLCGrowlDelegate
 *****************************************************************************/
@interface VLCGrowlDelegate : NSObject <GrowlApplicationBridgeDelegate>
{
    NSString *o_applicationName;
    NSString *o_notificationType;
    NSMutableDictionary *o_registrationDictionary;
}

- (void)registerToGrowl;
- (void)notifyWithDescription: (const char *)psz_desc
                       artUrl: (const char *)psz_arturl;
@end

struct intf_sys_t
{
    VLCGrowlDelegate *o_growl_delegate;
    int             i_id;
    int             i_item_changes;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static int ItemChange( vlc_object_t *, const char *,
                      vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 ****************************************************************************/

vlc_module_begin ()
set_category( CAT_INTERFACE )
set_subcategory( SUBCAT_INTERFACE_CONTROL )
set_shortname( "Growl" )
set_description( N_("Growl Notification Plugin") )
set_capability( "interface", 0 )
set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist;
    intf_sys_t    *p_sys;

    p_sys = p_intf->p_sys = calloc( 1, sizeof(intf_sys_t) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->o_growl_delegate = [[VLCGrowlDelegate alloc] init];
    if( !p_sys->o_growl_delegate )
      return VLC_ENOMEM;

    p_playlist = pl_Get( p_intf );
    var_AddCallback( p_playlist, "item-change", ItemChange, p_intf );
    var_AddCallback( p_playlist, "activity", ItemChange, p_intf );

    [p_sys->o_growl_delegate registerToGrowl];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    playlist_t *p_playlist = pl_Get( p_this );
    intf_sys_t *p_sys = p_intf->p_sys;

    var_DelCallback( p_playlist, "item-change", ItemChange, p_intf );
    var_DelCallback( p_playlist, "activity", ItemChange, p_intf );

    [p_sys->o_growl_delegate release];
    free( p_sys );
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                      vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED(oldval);

    intf_thread_t *p_intf = (intf_thread_t *)param;
    char *psz_tmp           = NULL;
    char *psz_title         = NULL;
    char *psz_artist        = NULL;
    char *psz_album         = NULL;
    input_item_t *p_item = newval.p_address;

    bool b_is_item_current = !strcmp( "activity", psz_var );

    /* Don't update each time an item has been preparsed */
    if( b_is_item_current )
    { /* stores the current input item id */
        input_thread_t *p_input = playlist_CurrentInput( (playlist_t*)p_this );
        if( !p_input )
            return VLC_SUCCESS;

        p_item = input_GetItem( p_input );
        if( p_intf->p_sys->i_id != p_item->i_id )
        {
            p_intf->p_sys->i_id = p_item->i_id;
            p_intf->p_sys->i_item_changes = 0;
        }

        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    /* ignore items which weren't pre-parsed yet */
    else if( !input_item_IsPreparsed(p_item) )
        return VLC_SUCCESS;
    else
    {   /* "item-change" */
        
        if( p_item->i_id != p_intf->p_sys->i_id )
            return VLC_SUCCESS;

        /* Some variable bitrate inputs call "item-change" callbacks each time
         * their length is updated, that is several times per second.
         * We'll limit the number of changes to 1 per input. */
        if( p_intf->p_sys->i_item_changes > 0 )
            return VLC_SUCCESS;

        p_intf->p_sys->i_item_changes++;
    }

    /* Playing something ... */
    if( input_item_GetNowPlaying( p_item ) )
        psz_title = input_item_GetNowPlaying( p_item );
    else
        psz_title = input_item_GetTitleFbName( p_item );
    if( EMPTY_STR( psz_title ) )
    {
        free( psz_title );
        return VLC_SUCCESS;
    }

    psz_artist = input_item_GetArtist( p_item );
    if( EMPTY_STR( psz_artist ) ) FREENULL( psz_artist );
    psz_album = input_item_GetAlbum( p_item ) ;
    if( EMPTY_STR( psz_album ) ) FREENULL( psz_album );

    int i_ret;
    if( psz_artist && psz_album )
        i_ret = asprintf( &psz_tmp, "%s\n%s [%s]",
                         psz_title, psz_artist, psz_album );
    else if( psz_artist )
        i_ret = asprintf( &psz_tmp, "%s\n%s", psz_title, psz_artist );
    else
        i_ret = asprintf(&psz_tmp, "%s", psz_title );

    if( i_ret == -1 )
    {
        free( psz_title );
        free( psz_artist );
        free( psz_album );
        return VLC_ENOMEM;
    }

    char *psz_arturl = input_item_GetArtURL( p_item );
    if( psz_arturl )
    {
        char *psz = make_path( psz_arturl );
        free( psz_arturl );
        psz_arturl = psz;
    }

    [p_intf->p_sys->o_growl_delegate notifyWithDescription: psz_tmp artUrl: psz_arturl];

    free( psz_title );
    free( psz_artist );
    free( psz_album );
    free( psz_arturl );
    free( psz_tmp );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLCGrowlDelegate
 *****************************************************************************/
@implementation VLCGrowlDelegate

- (id)init
{
    if( !( self = [super init] ) )
        return nil;

    o_applicationName = nil;
    o_notificationType = nil;
    o_registrationDictionary = nil;

    return self;
}

- (void)dealloc
{
    [o_applicationName release];
    [o_notificationType release];
    [o_registrationDictionary release];
    [super dealloc];
}

- (void)registerToGrowl
{
    o_applicationName = [[NSString alloc] initWithUTF8String: _( "VLC media player" )];
    o_notificationType = [[NSString alloc] initWithUTF8String: _( "New input playing" )];

    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    NSArray *o_defaultAndAllNotifications = [NSArray arrayWithObject: o_notificationType];

    o_registrationDictionary = [[NSMutableDictionary alloc] init];
    [o_registrationDictionary setObject: o_defaultAndAllNotifications
                                 forKey: GROWL_NOTIFICATIONS_ALL];
    [o_registrationDictionary setObject: o_defaultAndAllNotifications
                                 forKey: GROWL_NOTIFICATIONS_DEFAULT];

    [GrowlApplicationBridge setGrowlDelegate: self];
    [o_pool drain];
}

- (void)notifyWithDescription: (const char *)psz_desc artUrl: (const char *)psz_arturl
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    NSData *o_art = nil;

    if( psz_arturl )
        o_art = [NSData dataWithContentsOfFile: [NSString stringWithUTF8String: psz_arturl]];

    [GrowlApplicationBridge notifyWithTitle: [NSString stringWithUTF8String: _( "Now playing" )]
                                description: [NSString stringWithUTF8String: psz_desc]
                           notificationName: o_notificationType
                                   iconData: o_art
                                   priority: 0
                                   isSticky: NO
                               clickContext: nil];
    [o_pool drain];
}

/*****************************************************************************
 * Delegate methods
 *****************************************************************************/
- (NSDictionary *)registrationDictionaryForGrowl
{
    return o_registrationDictionary;
}

- (NSString *)applicationNameForGrowl
{
    return o_applicationName;
}
@end
