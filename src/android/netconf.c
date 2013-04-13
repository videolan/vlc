/*****************************************************************************
 * netconf.c : Network configuration
 *****************************************************************************
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

/* This is empty, of course
 *
 * The reason is that there is no simple way to get the proxy settings on all
 * supported versions of Android, even from the Java side...
 *
 * The best way would be to follow this "solution"
 * http://stackoverflow.com/questions/10811698/getting-wifi-proxy-settings-in-android/13616054#13616054
 *
 * Or, in summary, using JNI:
 * if( version >= 4.0 ) {
 *     System.getProperty( "http.proxyHost" );
 *     System.getProperty( "http.proxyPort" );
 * } else {
 *     context = magically_find_context();
 *     android.net.Proxy.getHost( context );
 *     android.net.Proxy.getPort( context );
 * }
 *
 * */

/**
 * Determines the network proxy server to use (if any).
 * @param url absolute URL for which to get the proxy server
 * @return proxy URL, NULL if no proxy or error
 */
char *vlc_getProxyUrl(const char *url)
{
    return NULL;
}
