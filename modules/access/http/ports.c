/*****************************************************************************
 * ports.c: special ports block list
 *****************************************************************************
 * Copyright © 2019 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <stdbool.h>
#include <stdlib.h>
#include "transport.h"
#include <vlc_common.h>

/* Must be in ascending order */
static const unsigned short blocked_ports[] = {
        1, // tcpmux
        7, // echo
        9, // discard
       11, // systat
       13, // daytime
       15, // netstat
       17, // QOTD
       19, // character generator
       20, // FTP data
       21, // FTP access
       22, // SSH
       23, // Telnet
       25, // SMTP
       37, // time
       42, // name
       43, // nicname
       53, // DNS
       77, // priv-rjs
       79, // finger
       87, // ttylink
       95, // supdup
      101, // hostriame
      102, // iso-tsap
      103, // gppitnp
      104, // acr-nema
      109, // POP2
      110, // POP3
      111, // Sun RPC
      113, // auth
      115, // SFTP
      117, // UUCP path service
      119, // NNTP (i.e. Usenet)
      123, // NTP
      135, // DCE endpoint resolution
      139, // NetBIOS
      143, // IMAP2
      179, // BGP
      389, // LDAP
      465, // SMTP/TLS
      512, // remote exec
      513, // remote login
      514, // remote shell
      515, // printer
      526, // tempo
      530, // courier
      531, // chat
      532, // netnews
      540, // UUCP
      556, // remotefs
      563, // NNTP/TLS
      587, // Submission (i.e. first hop SMTP)
      601, // rsyslog
      636, // LDAP/TLS
      993, // LDAP/TLS
      995, // POP3/TLS
     2049, // NFS
     3659, // Apple SASL
     4045, // NFS RPC lockd
     6000, // X11
     6665, // IRC
     6666, // IRC
     6667, // IRC
     6668, // IRC
     6669, // IRC
};

static int portcmp(const void *key, const void *entry)
{
    const unsigned *port = key;
    const unsigned short *blocked_port = entry;

    return ((int)*port) - ((int)*blocked_port);
}

bool vlc_http_port_blocked(unsigned port)
{
    if (port > 0xffff)
        return true;

    return bsearch(&port, blocked_ports, ARRAY_SIZE(blocked_ports),
                   sizeof (unsigned short), portcmp) != NULL;
}
