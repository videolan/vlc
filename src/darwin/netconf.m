/*****************************************************************************
 * netconf.m : Network configuration
 *****************************************************************************
 * Copyright (C) 2013-2018 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_network.h>

#import <Foundation/Foundation.h>

#import <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <CFNetwork/CFProxySupport.h>
#else
#include <CoreServices/CoreServices.h>
#endif

/**
 * Determines the network proxy server to use (if any).
 * @param url absolute URL for which to get the proxy server
 * @return proxy URL, NULL if no proxy or error
 */
char *vlc_getProxyUrl(const char *url)
{
    if (url == NULL) {
        return NULL;
    }
#if !TARGET_OS_IPHONE
    NSDictionary *proxySettings = CFBridgingRelease(CFNetworkCopySystemProxySettings());
    if (NULL != proxySettings) {
        NSURL *requestedURL = [[NSURL alloc] initWithString:[NSString stringWithUTF8String:url]];
        NSString *scheme = requestedURL.scheme;
        NSString *proxyHost;
        NSNumber *proxyPort;

        if ([scheme caseInsensitiveCompare:@"http"] == NSOrderedSame) {
            proxyHost = proxySettings[(NSString *)kCFNetworkProxiesHTTPProxy];
            proxyPort = proxySettings[(NSString *)kCFNetworkProxiesHTTPPort];
        } else if ([scheme caseInsensitiveCompare:@"https"] == NSOrderedSame) {
            proxyHost = proxySettings[(NSString *)kCFNetworkProxiesHTTPSProxy];
            proxyPort = proxySettings[(NSString *)kCFNetworkProxiesHTTPSPort];
        } else if ([scheme caseInsensitiveCompare:@"rtsp"] == NSOrderedSame) {
            proxyHost = proxySettings[(NSString *)kCFNetworkProxiesRTSPProxy];
            proxyPort = proxySettings[(NSString *)kCFNetworkProxiesRTSPPort];
        } else if ([scheme caseInsensitiveCompare:@"ftp"] == NSOrderedSame) {
            proxyHost = proxySettings[(NSString *)kCFNetworkProxiesFTPProxy];
            proxyPort = proxySettings[(NSString *)kCFNetworkProxiesFTPPort];
        } else {
            return NULL;
        }
        if (proxyHost == NULL)
            return NULL;

        NSString *returnValue = [[NSString alloc] initWithFormat:@"%@://%@:%i", scheme, proxyHost, proxyPort.intValue];
        return strdup([returnValue UTF8String]);
    }
#endif

    return NULL;
}
