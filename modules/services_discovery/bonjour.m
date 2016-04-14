/*****************************************************************************
 * bonjour.m: mDNS services discovery module based on Bonjour
 *****************************************************************************
 * Copyright (C) 2016 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Felix Paul Kühne <fkuehne@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_services_discovery.h>

#import <Foundation/Foundation.h>

static int Open( vlc_object_t * );
static void Close( vlc_object_t * );

VLC_SD_PROBE_HELPER( "Bonjour", "Bonjour Network Discovery", SD_CAT_LAN )

/*
 * Module descriptor
 */
vlc_module_begin()
    set_shortname( "Bonjour" )
    set_description( N_( "Bonjour Network Discovery" ) )
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )
    set_capability( "services_discovery", 0 )
    set_callbacks( Open, Close )
    add_shortcut( "mdns", "bonjour" )

    VLC_SD_PROBE_SUBMODULE
vlc_module_end ()

NSString *const VLCBonjourProtocolName = @"VLCBonjourProtocolName";
NSString *const VLCBonjourProtocolServiceName = @"VLCBonjourProtocolServiceName";

@interface VLCNetServiceDiscoveryController : NSObject <NSNetServiceBrowserDelegate, NSNetServiceDelegate>
{
#ifdef MAC_OS_X_VERSION_10_11
    NSArray<NSNetServiceBrowser *> *_serviceBrowsers;

    NSMutableArray<NSNetService *> *_rawNetServices;
    NSMutableArray<NSNetService *> *_resolvedNetServices;
    NSMutableArray<NSValue *> *_inputItemsForNetServices;
#else
    NSArray *_serviceBrowsers;

    NSMutableArray *_rawNetServices;
    NSMutableArray *_resolvedNetServices;
    NSMutableArray *_inputItemsForNetServices;
#endif

    NSArray *_activeProtocols;
}

@property (readwrite, nonatomic) services_discovery_t *p_sd;

- (void)startDiscovery;
- (void)stopDiscovery;

@end

struct services_discovery_sys_t
{
    CFTypeRef _Nullable discoveryController;
};

@implementation VLCNetServiceDiscoveryController

- (void)startDiscovery
{
    _rawNetServices = [[NSMutableArray alloc] init];
    _resolvedNetServices = [[NSMutableArray alloc] init];
    _inputItemsForNetServices = [[NSMutableArray alloc] init];

    NSDictionary *VLCFtpProtocol = @{ VLCBonjourProtocolName : @"ftp",
                                      VLCBonjourProtocolServiceName : @"_ftp._tcp." };
    NSDictionary *VLCSmbProtocol = @{ VLCBonjourProtocolName : @"smb",
                                      VLCBonjourProtocolServiceName : @"_smb._tcp." };
    NSDictionary *VLCNfsProtocol = @{ VLCBonjourProtocolName : @"nfs",
                                      VLCBonjourProtocolServiceName : @"_nfs._tcp." };
    NSDictionary *VLCSftpProtocol = @{ VLCBonjourProtocolName : @"sftp",
                                       VLCBonjourProtocolServiceName : @"_sftp-ssh._tcp." };

    NSArray *VLCSupportedProtocols = @[VLCFtpProtocol,
                                       VLCSmbProtocol,
                                       VLCNfsProtocol,
                                       VLCSftpProtocol];

    NSUInteger count = VLCSupportedProtocols.count;
    NSMutableArray *discoverers = [[NSMutableArray alloc] init];
    NSMutableArray *protocols = [[NSMutableArray alloc] init];

    for (NSUInteger i = 0; i < count; i++) {
        NSDictionary *protocol = VLCSupportedProtocols[i];

        /* only discover hosts if we actually have a module that can handle those */
        if (!module_exists([protocol[VLCBonjourProtocolName] UTF8String]))
            continue;

        NSNetServiceBrowser *serviceBrowser = [[NSNetServiceBrowser alloc] init];
        serviceBrowser.delegate = self;
        [serviceBrowser searchForServicesOfType:protocol[VLCBonjourProtocolServiceName] inDomain:@"local."];
        [discoverers addObject:serviceBrowser];
        [protocols addObject:protocol];
    }

    _serviceBrowsers = [discoverers copy];
    _activeProtocols = [protocols copy];
}

- (void)stopDiscovery
{
    [_serviceBrowsers makeObjectsPerformSelector:@selector(stop)];

    NSUInteger inputItemCount = _inputItemsForNetServices.count;
    for (NSUInteger i = 0; i < inputItemCount; i++) {
        input_item_t *p_input_item = [_inputItemsForNetServices[i] pointerValue];
        if (p_input_item != NULL) {
            services_discovery_RemoveItem(self.p_sd, p_input_item);
            input_item_Release(p_input_item);
        }
    }

    [_inputItemsForNetServices removeAllObjects];
    [_resolvedNetServices removeAllObjects];
}

#pragma mark - functional delegation

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didFindService:(NSNetService *)aNetService moreComing:(BOOL)moreComing
{
    msg_Dbg(self.p_sd, "found bonjour service: %s (%s)", [aNetService.name UTF8String], [aNetService.type UTF8String]);
    [_rawNetServices addObject:aNetService];
    aNetService.delegate = self;
    [aNetService resolveWithTimeout:5.];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)aNetServiceBrowser didRemoveService:(NSNetService *)aNetService moreComing:(BOOL)moreComing
{
    msg_Dbg(self.p_sd, "bonjour service disappeared: %s", [aNetService.name UTF8String]);
    if ([_rawNetServices containsObject:aNetService])
        [_rawNetServices removeObject:aNetService];

    if ([_resolvedNetServices containsObject:aNetService]) {
        NSInteger index = [_resolvedNetServices indexOfObject:aNetService];
        if (index == NSNotFound)
            return;

        [_resolvedNetServices removeObjectAtIndex:index];
        input_item_t *p_input_item = [_inputItemsForNetServices[index] pointerValue];
        if (p_input_item != NULL) {
            services_discovery_RemoveItem(self.p_sd, p_input_item);
            input_item_Release(p_input_item);
        }
    }
}

- (void)netServiceDidResolveAddress:(NSNetService *)aNetService
{
    if (![_resolvedNetServices containsObject:aNetService]) {
        NSString *serviceType = aNetService.type;
        NSUInteger count = _activeProtocols.count;
        NSString *protocol = nil;
        for (NSUInteger i = 0; i < count; i++) {
            NSDictionary *protocolDefinition = _activeProtocols[i];
            if ([serviceType isEqualToString:protocolDefinition[VLCBonjourProtocolServiceName]]) {
                protocol = protocolDefinition[VLCBonjourProtocolName];
            }
        }

        NSString *uri = [NSString stringWithFormat:@"%@://%@:%ld",
                         protocol,
                         aNetService.hostName,
                         aNetService.port];

        input_item_t *p_input_item = input_item_NewDirectory([uri UTF8String],
                                                             [aNetService.name UTF8String],
                                                             ITEM_NET );

        if (p_input_item != NULL) {
            services_discovery_AddItem(self.p_sd, p_input_item, NULL);
            [_inputItemsForNetServices addObject:[NSValue valueWithPointer:p_input_item]];
            [_resolvedNetServices addObject:aNetService];
        }
    }

    [_rawNetServices removeObject:aNetService];
}

- (void)netService:(NSNetService *)aNetService didNotResolve:(NSDictionary *)errorDict
{
    msg_Dbg(self.p_sd, "failed to resolve: %s", [aNetService.name UTF8String]);
    [_rawNetServices removeObject:aNetService];
}

@end

static int Open(vlc_object_t *p_this)
{
    services_discovery_t *p_sd = (services_discovery_t *)p_this;
    services_discovery_sys_t *p_sys = NULL;

    p_sd->p_sys = p_sys = calloc(1, sizeof(services_discovery_sys_t));
    if (!p_sys) {
        return VLC_ENOMEM;
    }

    VLCNetServiceDiscoveryController *discoveryController = [[VLCNetServiceDiscoveryController alloc] init];
    discoveryController.p_sd = p_sd;

    p_sys->discoveryController = CFBridgingRetain(discoveryController);

    [discoveryController startDiscovery];

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *p_this)
{
    services_discovery_t *p_sd = (services_discovery_t *)p_this;
    services_discovery_sys_t *p_sys = p_sd->p_sys;

    VLCNetServiceDiscoveryController *discoveryController = (__bridge VLCNetServiceDiscoveryController *)(p_sys->discoveryController);
    [discoveryController stopDiscovery];

    CFBridgingRelease(p_sys->discoveryController);
    discoveryController = nil;

    free(p_sys);
}
