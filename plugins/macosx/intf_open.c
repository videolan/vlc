/*****************************************************************************
 * intf_open.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_open.c,v 1.1 2002/04/23 03:21:21 jlj Exp $
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
#include <paths.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>

#import "intf_open.h"
#import "intf_vlc_wrapper.h"

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
 * Intf_Open implementation 
 *****************************************************************************/
@implementation Intf_Open

static Intf_Open *o_open = nil;

- (id)init
{
    if( o_open == nil )
    {
        o_open = [super init];
    }

    return( o_open );
}

+ (Intf_Open *)instance
{
    return( o_open );
}

- (void)awakeFromNib
{
    [o_net_channel_pstepper setEnabled: FALSE];
    
    [o_net_server_addr addItemWithObjectValue: @"vls"];
    [o_net_server_addr selectItemAtIndex: 0];
    
    [o_net_server_baddr setStringValue: @"138.195.143.255"];
    [o_net_server_port setIntValue: 1234];
    [o_net_server_pstepper setIntValue: [o_net_server_port intValue]];

    [o_net_channel_addr setStringValue: @"138.195.143.120"];
    [o_net_channel_port setIntValue: 6010];
    [o_net_channel_pstepper setIntValue: [o_net_channel_port intValue]];
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
        NSString *o_type = [[o_disc_type selectedCell] title];
        NSString *o_device = [o_disc_device stringValue];
        int i_title = [o_disc_title intValue];
        int i_chapter = [o_disc_chapter intValue];
        
        [[Intf_VLCWrapper instance] openDisc: [o_type lowercaseString]
            device: o_device title: i_title chapter: i_chapter];
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
        }
        else
        {
            [o_disc_device setStringValue: 
                [NSString stringWithFormat: @"No %@s found", o_type]];
        }
    }
}

- (IBAction)openFile:(id)sender
{
    NSOpenPanel *o_panel = [NSOpenPanel openPanel];
    
    [o_panel setAllowsMultipleSelection: YES];

    if( [o_panel runModalForDirectory: NSHomeDirectory() 
            file: nil types: nil] == NSOKButton )
    {
        [[Intf_VLCWrapper instance] openFiles: [o_panel filenames]];
    }
}

- (IBAction)openNet:(id)sender
{
    int i_result;

    [o_net_panel makeKeyAndOrderFront: self];
    i_result = [NSApp runModalForWindow: o_net_panel];
    [o_net_panel close];

    if( i_result )
    {
        BOOL b_channel;
        BOOL b_broadcast;
        NSString *o_protocol;

        o_protocol = [[o_net_protocol selectedCell] title];
        b_channel = [o_net_channel_checkbox state] == NSOnState;
        b_broadcast = [o_net_server_bcheckbox state] == NSOnState;

        if( [o_protocol isEqualToString: @"TS"] )
        {
            o_protocol = @"udpstream";
        }
        else if( [o_protocol isEqualToString: @"RTP"] ) 
        {
            o_protocol = @"rtp";
        }

        if( b_channel )
        {
            NSString *o_channel_addr = [o_net_channel_addr stringValue];
            int i_channel_port = [o_net_channel_port intValue];

            [[Intf_VLCWrapper instance]
                openNetChannel: o_channel_addr port: i_channel_port];
        }
        else
        {
            NSString *o_addr = [o_net_server_addr stringValue];
            int i_port = [o_net_server_port intValue];

            if( b_broadcast )
            {
                NSString *o_baddr = [o_net_server_baddr stringValue];

                [[Intf_VLCWrapper instance]
                    openNet: o_protocol addr: o_addr
                        port: i_port baddr: o_baddr]; 
            }
            else
            {
                [[Intf_VLCWrapper instance]
                    openNet: o_protocol addr: o_addr
                        port: i_port baddr: nil];
            } 
        } 
    }
}

- (IBAction)openNetBroadcast:(id)sender
{
    BOOL b_broadcast;
    
    b_broadcast = [o_net_server_bcheckbox state] == NSOnState;
    [o_net_server_baddr setEnabled: b_broadcast];
}

- (IBAction)openNetChannel:(id)sender
{
    BOOL b_channel;
    BOOL b_broadcast;
    NSColor *o_color;
    
    b_channel = [o_net_channel_checkbox state] == NSOnState;
    b_broadcast = [o_net_server_bcheckbox state] == NSOnState;
    
    o_color = b_channel ? [NSColor controlTextColor] : 
        [NSColor disabledControlTextColor];

    [o_net_channel_addr setEnabled: b_channel];
    [o_net_channel_port setEnabled: b_channel];
    [o_net_channel_port_label setTextColor: o_color];
    [o_net_channel_pstepper setEnabled: b_channel];
    
    o_color = !b_channel ? [NSColor controlTextColor] : 
        [NSColor disabledControlTextColor];    
        
    [o_net_server_addr setEnabled: !b_channel];
    [o_net_server_addr_label setTextColor: o_color];
    [o_net_server_port setEnabled: !b_channel];
    [o_net_server_port_label setTextColor: o_color];
    [o_net_server_pstepper setEnabled: !b_channel];
    [o_net_server_bcheckbox setEnabled: !b_channel];
    [o_net_server_baddr setEnabled: b_broadcast && !b_channel];
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
