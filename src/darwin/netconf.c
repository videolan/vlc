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
#include <SystemConfiguration/SystemConfiguration.h>

/**
 * Determines the network proxy server to use (if any).
 * @param url absolute URL for which to get the proxy server (not used)
 * @return proxy URL, NULL if no proxy or error
 */
char *vlc_getProxyUrl(const char *url)
{
    VLC_UNUSED(url);
    CFDictionaryRef proxies = SCDynamicStoreCopyProxies(NULL);
    char *proxy_url = NULL;

    if (proxies) {
        CFNumberRef cfn_httpProxyOn =
            (CFNumberRef)CFDictionaryGetValue(proxies,
                                              kSCPropNetProxiesHTTPEnable);
        int i_httpProxyOn;
        CFNumberGetValue(cfn_httpProxyOn, kCFNumberIntType, &i_httpProxyOn);
        CFRelease(cfn_httpProxyOn);

        if (i_httpProxyOn == 1) // http proxy is on
        {
            CFStringRef httpProxy =
                (CFStringRef)CFDictionaryGetValue(proxies,
                                                  kSCPropNetProxiesHTTPProxy);

            if (httpProxy) {
                CFNumberRef cfn_httpProxyPort =
                    (CFNumberRef)CFDictionaryGetValue(proxies,
                                                    kSCPropNetProxiesHTTPPort);
                int i_httpProxyPort;
                CFNumberGetValue(cfn_httpProxyPort,
                                 kCFNumberIntType,
                                 &i_httpProxyPort);
                CFRelease(cfn_httpProxyPort);

                CFMutableStringRef outputURL =
                    CFStringCreateMutableCopy(kCFAllocatorDefault,
                                              0,
                                              httpProxy);
                if (i_httpProxyPort > 0)
                    CFStringAppendFormat(outputURL,
                                         NULL,
                                         CFSTR(":%i"),
                                         i_httpProxyPort);

                CFStringGetCString(outputURL,
                                   proxy_url,
                                   sizeof(proxy_url),
                                   kCFStringEncodingASCII);
                CFRelease(outputURL);
            }
            CFRelease(httpProxy);
        }
        CFRelease(proxies);
    }

    return proxy_url;
}
