/*****************************************************************************
 * open.m: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: open.m,v 1.2 2002/08/06 23:43:58 jlj Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net> 
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>

#include <paths.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>

#import <Cocoa/Cocoa.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "netutils.h"

#import "intf.h"
#import "playlist.h"
#import "open.h"

/*****************************************************************************
 * GetEjectableMediaOfClass 
 *****************************************************************************/
NSArray *GetEjectableMediaOfClass( const char *psz_class )
{
    io_object_t next_media;
    mach_port_t master_port;
    kern_return_t kern_result;
    NSArray *o_devices = nil;
    NSMutableArray *p_list = nil;
    io_iterator_t media_iterator;
    CFMutableDictionaryRef classes_to_match;

    kern_result = IOMasterPort( MACH_PORT_NULL, &master_port );
    if( kern_result != KERN_SUCCESS )
    {
        return( nil );
    }
    
    classes_to_match = IOServiceMatching( psz_class );
    if( classes_to_match == NULL )
    {
        return( nil );
    }
    
    CFDictionarySetValue( classes_to_match, CFSTR( kIOMediaEjectable ), 
                          kCFBooleanTrue );
    
    kern_result = IOServiceGetMatchingServices( master_port, classes_to_match, 
                                                &media_iterator );
    if( kern_result != KERN_SUCCESS )
    {
        return( nil );
    }

    p_list = [NSMutableArray arrayWithCapacity: 1];
    
    next_media = IOIteratorNext( media_iterator );
    if( next_media != NULL )
    {
        char psz_buf[0x32];
        size_t dev_path_length;
        CFTypeRef str_bsd_path;
    
        do
        {
            str_bsd_path = IORegistryEntryCreateCFProperty( next_media,
                                                            CFSTR( kIOBSDName ),
                                                            kCFAllocatorDefault,
                                                            0 );
            if( str_bsd_path == NULL )
            {
                IOObjectRelease( next_media );
                continue;
            }
            
            snprintf( psz_buf, sizeof(psz_buf), "%s%c", _PATH_DEV, 'r' );
            dev_path_length = strlen( psz_buf );
            
            if( CFStringGetCString( str_bsd_path,
                                    (char*)&psz_buf + dev_path_length,
                                    sizeof(psz_buf) - dev_path_length,
                                    kCFStringEncodingASCII ) )
            {
                [p_list addObject: [NSString stringWithCString: psz_buf]];
            }
            
            CFRelease( str_bsd_path );
            
            IOObjectRelease( next_media );
        
        } while( ( next_media = IOIteratorNext( media_iterator ) ) != NULL );
    }
    
    IOObjectRelease( media_iterator );

    o_devices = [NSArray arrayWithArray: p_list];

    return( o_devices );
}

/*****************************************************************************
 * VLCOpen implementation 
 *****************************************************************************/
@implementation VLCOpen

- (void)awakeFromNib
{
    [o_disc_panel setTitle: _NS("Open Disc")];
    [o_disc_btn_ok setTitle: _NS("OK")];
    [o_disc_btn_cancel setTitle: _NS("Cancel")];
    [o_disc_lbl_type setTitle: _NS("Disc type")];
    [o_disc_lbl_sp setTitle: _NS("Starting position")];
    [o_disc_title setTitle: _NS("Title")];
    [o_disc_chapter setTitle: _NS("Chapter")];

    [o_net_panel setTitle: _NS("Open Network")];
    [o_net_box_mode setTitle: _NS("Network mode")];
    [o_net_box_addr setTitle: _NS("Address")];
    [o_net_port_lbl setStringValue: _NS("Port")];

    [o_quickly_panel setTitle: _NS("Open Quickly")];
    [o_quickly_btn_ok setTitle: _NS("OK")];
    [o_quickly_btn_cancel setTitle: _NS("Cancel")];
}

- (IBAction)openDisc:(id)sender
{
    int i_result;

    [self openDiscTypeChanged: nil];
    
    [o_disc_panel makeKeyAndOrderFront: self];
    i_result = [NSApp runModalForWindow: o_disc_panel];
    [o_disc_panel close];

    if( i_result )
    {
        NSString *o_source;

        NSString *o_type = [[o_disc_type selectedCell] title];
        NSString *o_device = [o_disc_device stringValue];
        int i_title = [o_disc_title intValue];
        int i_chapter = [o_disc_chapter intValue];

        o_source = [NSString stringWithFormat: @"%@:%@@%d,%d",
            [o_type lowercaseString], o_device, i_title, i_chapter];

        [o_playlist appendArray: 
            [NSArray arrayWithObject: o_source] atPos: -1];
    }
}

- (IBAction)openDiscTypeChanged:(id)sender
{
    NSString *o_type;
    NSArray *o_devices;
    const char *psz_class = NULL;
    
    [o_disc_device removeAllItems];
    
    o_type = [[o_disc_type selectedCell] title];

    if( [o_type isEqualToString: @"DVD"] )
    {
        psz_class = kIODVDMediaClass;
    }
    else
    {
        psz_class = kIOCDMediaClass;
    }
    
    o_devices = GetEjectableMediaOfClass( psz_class );
    if( o_devices != nil )
    {
        int i_devices = [o_devices count];
        
        if( i_devices )
        {
            int i;
        
            for( i = 0; i < i_devices; i++ )
            {
                [o_disc_device 
                    addItemWithObjectValue: [o_devices objectAtIndex: i]];
            }
            
            [o_disc_device selectItemAtIndex: 0];
            [o_disc_btn_ok setEnabled: TRUE];
        }
        else
        {
            [o_disc_device setStringValue: 
                [NSString stringWithFormat: @"No %@s found", o_type]];
            [o_disc_btn_ok setEnabled: FALSE];
        }
    }
}

- (IBAction)openFile:(id)sender
{
    NSOpenPanel *o_panel = [NSOpenPanel openPanel];
    
    [o_panel setAllowsMultipleSelection: YES];

    if( [o_panel runModalForDirectory: nil 
            file: nil types: nil] == NSOKButton )
    {
        [o_playlist appendArray: [o_panel filenames] atPos: -1];
    }
}

- (IBAction)openNet:(id)sender
{
    int i_result;
    intf_thread_t * p_intf = [NSApp getIntf];

    [o_net_panel makeKeyAndOrderFront: self];
    i_result = [NSApp runModalForWindow: o_net_panel];
    [o_net_panel close];

    if( i_result )
    {
        NSString * o_source = nil;
        UInt32 i_port = [o_net_port intValue];
        NSString * o_addr = [o_net_address stringValue];
        NSString * o_mode = [[o_net_mode selectedCell] title];

        if( i_port > 65536 )
        {
            NSBeep();
            return;
        }

        if( [o_mode isEqualToString: @"UDP"] )
        {
            o_source = [NSString 
                stringWithFormat: @"udp:@:%i", i_port];
        } 
        else if( [o_mode isEqualToString: @"UDP Multicase"] )
        {
            o_source = [NSString 
                stringWithFormat: @"udp:@%@:%i", o_addr, i_port];
        }
        else if( [o_mode isEqualToString: @"Channel server"] )
        {
            if( p_intf->p_vlc->p_channel == NULL )
            {
                network_ChannelCreate( p_intf );
            }

            config_PutPsz( p_intf, "channel-server", 
                           (char *)[o_addr lossyCString] );
            config_PutInt( p_intf, "channel-port", i_port );

            p_intf->p_sys->b_playing = 1;
        }
        else if( [o_mode isEqualToString: @"HTTP"] )
        {
            o_source = o_addr;
        }

        if( o_source != nil )
        {
            [o_playlist appendArray:
                [NSArray arrayWithObject: o_source] atPos: -1];
        }
    }
}

- (IBAction)openNetModeChanged:(id)sender
{
    NSString * o_mode;
    SInt32 i_port = 1234;
    NSString * o_addr = nil;

    o_mode = [[o_net_mode selectedCell] title];

    if( [o_mode isEqualToString: @"UDP Multicast"] )
    {
        o_addr = @"";
    }
    else if( [o_mode isEqualToString: @"Channel server"] )
    {
        o_addr = @"localhost";
        i_port = 6010;
    }
    else if( [o_mode isEqualToString: @"HTTP"] )
    {
        o_addr = @"http://";
        i_port = -1;
    }

    if( o_addr != nil )
    {
        [o_net_address setEnabled: TRUE];
        [o_net_address setStringValue: o_addr];
    }
    else
    {
        [o_net_address setEnabled: FALSE];
    }

    if( i_port > -1 )
    {
        [o_net_port setEnabled: TRUE];
        [o_net_port_stp setEnabled: TRUE];
        [o_net_port setIntValue: i_port];
    }
    else
    {
        [o_net_port setEnabled: FALSE];
        [o_net_port_stp setEnabled: FALSE];
    }
}

- (IBAction)openQuickly:(id)sender
{
    int i_result;

    [o_quickly_source setStringValue: @""];
    [o_quickly_panel makeKeyAndOrderFront: self];
    i_result = [NSApp runModalForWindow: o_quickly_panel];
    [o_quickly_panel close];

    if( i_result )
    {
        NSString * o_source;

        o_source = [o_quickly_source stringValue];

        if( [o_source length] > 0 )
        {
            [o_playlist appendArray: 
                [NSArray arrayWithObject: o_source] atPos: -1];
        }
        else
        {
            NSBeep();
        }
    }
}

- (IBAction)panelCancel:(id)sender
{
    [NSApp stopModalWithCode: 0];
}

- (IBAction)panelOk:(id)sender
{
    [NSApp stopModalWithCode: 1];
}

@end
