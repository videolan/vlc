/*****************************************************************************
 * netconf.c : Network configuration
 *****************************************************************************
 * Copyright (C) 2013 VLC authors and VideoLAN
 * $Id$
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

#include <CoreFoundation/CoreFoundation.h>

#import <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <CFNetwork/CFProxySupport.h>
#else
#include <CoreServices/CoreServices.h>
#endif

/**
 * Determines the network proxy server to use (if any).
 * @param url absolute URL for which to get the proxy server (not used)
 * @return proxy URL, NULL if no proxy or error
 */
char *vlc_getProxyUrl(const char *url)
{
    VLC_UNUSED(url);
    char *proxy_url = NULL;
    CFDictionaryRef dicRef = CFNetworkCopySystemProxySettings();
    if (NULL != dicRef) {
        const CFStringRef proxyCFstr = (const CFStringRef)CFDictionaryGetValue(
            dicRef, (const void*)kCFNetworkProxiesHTTPProxy);
        const CFNumberRef portCFnum = (const CFNumberRef)CFDictionaryGetValue(
            dicRef, (const void*)kCFNetworkProxiesHTTPPort);
        if (NULL != proxyCFstr && NULL != portCFnum) {
            int port = 0;
            if (!CFNumberGetValue(portCFnum, kCFNumberIntType, &port)) {
                CFRelease(dicRef);
                return NULL;
            }

            char host_buffer[4096];
            memset(host_buffer, 0, sizeof(host_buffer));
            if (CFStringGetCString(proxyCFstr, host_buffer, sizeof(host_buffer)
                                   - 1, kCFStringEncodingUTF8)) {
                char buffer[4096];
                sprintf(buffer, "%s:%d", host_buffer, port);
                proxy_url = strdup(buffer);
            }
        }

        CFRelease(dicRef);
    }

    return proxy_url;
}
