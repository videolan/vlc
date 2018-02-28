/*****************************************************************************
 * netconf.c : Network configuration
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
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

#include <string.h>
#include <windows.h>

#include <vlc_common.h>
#include <vlc_network.h>
#include <vlc_url.h>

char *vlc_getProxyUrl(const char *psz_url)
{
    VLC_UNUSED(psz_url);

    char *proxy = config_GetPsz( "http-proxy" );
    if (proxy == NULL)
        return NULL;

    char *proxy_pwd = config_GetPsz( "http-proxy-pwd" );
    if (proxy_pwd == NULL)
        return proxy;

    vlc_url_t url;
    if (vlc_UrlParse(&url, proxy) < 0) {
        vlc_UrlClean(&url);
        free (proxy);
        free (proxy_pwd);
        return NULL;
    }

    if (url.psz_password == NULL )
        url.psz_password = proxy_pwd;

    char *proxy_url = vlc_uri_compose (&url);
    vlc_UrlClean (&url);

    free (proxy_pwd);
    free (proxy);

#if 0
    /* Try to get the proxy server address from Windows internet settings. */
    HKEY h_key;

    /* Open the key */
    if( RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft"
                      "\\Windows\\CurrentVersion\\Internet Settings",
                      0, KEY_READ, &h_key ) == ERROR_SUCCESS )
        return NULL;

    DWORD len = sizeof( DWORD );
    BYTE proxyEnable;

    /* Get the proxy enable value */
    if( RegQueryValueEx( h_key, "ProxyEnable", NULL, NULL,
                         &proxyEnable, &len ) != ERROR_SUCCESS
     || !proxyEnable )
        goto out;

    /* Proxy is enabled */
    /* Get the proxy URL :
       Proxy server value in the registry can be something like "address:port"
       or "ftp=address1:port1;http=address2:port2 ..."
       depending of the configuration. */
    unsigned char key[256];

    len = sizeof( key );
    if( RegQueryValueEx( h_key, "ProxyServer", NULL, NULL,
                         key, &len ) == ERROR_SUCCESS )
    {
        /* FIXME: This is lame. The string should be tokenized. */
#warning FIXME.
        char *psz_proxy = strstr( (char *)key, "http=" );
        if( psz_proxy != NULL )
        {
            psz_proxy += 5;
            char *end = strchr( psz_proxy, ';' );
            if( end != NULL )
                *end = '\0';
        }
        else
            psz_proxy = (char *)key;
        proxy_url = strdup( psz_proxy );
    }

out:
    RegCloseKey( h_key );
#endif
    return proxy_url;
}
