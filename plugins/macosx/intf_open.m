/*****************************************************************************
 * intf_open.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_open.m,v 1.3.2.1 2002/06/02 22:32:46 massiot Exp $
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
    [o_net_server_addr setEnabled: NSOffState];
    [o_net_server_addr_label setStringValue: @"Address"];
    [o_net_server_port setEnabled: NSOnState];
    [o_net_server_port setIntValue: 1234];
    [o_net_server_pstepper setEnabled: NSOnState];
    [o_net_server_pstepper setIntValue: [o_net_server_port intValue]];
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

    if( [o_panel runModalForDirectory: nil 
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
        NSString *o_protocol;
        int i_port = [o_net_server_port intValue];
        NSString *o_addr = [o_net_server_addr stringValue];

        o_protocol = [[o_net_protocol selectedCell] title];

        if( [o_protocol isEqualToString: @"UDP"] )
        {
            [[Intf_VLCWrapper instance] openNet: @"" port: i_port]; 
        }
        else if( [o_protocol isEqualToString: @"UDP - multicast"] ) 
        {
            [[Intf_VLCWrapper instance] openNet: o_addr port: i_port]; 
        }
        else if( [o_protocol isEqualToString: @"Channel server"] ) 
        {
            [[Intf_VLCWrapper instance] openNetChannel: o_addr port: i_port]; 
        }
        else if( [o_protocol isEqualToString: @"HTTP"] ) 
        {
            [[Intf_VLCWrapper instance] openNetHTTP: o_addr]; 
        }
    }
}

- (IBAction)openNetProtocol:(id)sender
{
    NSString *o_protocol;

    o_protocol = [[o_net_protocol selectedCell] title];
    
    if( [o_protocol isEqualToString: @"UDP"] )
    {
        [o_net_server_addr setEnabled: NSOffState];
        [o_net_server_port setEnabled: NSOnState];
        [o_net_server_port setIntValue: 1234];
        [o_net_server_pstepper setEnabled: NSOnState];
    }
    else if( [o_protocol isEqualToString: @"UDP - multicast"] ) 
    {
        [o_net_server_addr setEnabled: NSOnState];
        [o_net_server_addr_label setStringValue: @"Mult. addr."];
        [o_net_server_port setEnabled: NSOnState];
        [o_net_server_port setIntValue: 1234];
        [o_net_server_pstepper setEnabled: NSOnState];
    }
    else if( [o_protocol isEqualToString: @"Channel server"] ) 
    {
        [o_net_server_addr setEnabled: NSOnState];
        [o_net_server_addr_label setStringValue: @"Server"];
        [o_net_server_addr setStringValue: @"vlcs"];
        [o_net_server_port setEnabled: NSOnState];
        [o_net_server_port setIntValue: 6010];
        [o_net_server_pstepper setEnabled: NSOnState];
    }
    else if( [o_protocol isEqualToString: @"HTTP"] ) 
    {
        [o_net_server_addr setEnabled: NSOnState];
        [o_net_server_addr_label setStringValue: @"URL"];
        [o_net_server_addr setStringValue: @"http://"];
        [o_net_server_port setEnabled: NSOffState];
        [o_net_server_pstepper setEnabled: NSOffState];
    }
    [o_net_server_pstepper setIntValue: [o_net_server_port intValue]];
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
