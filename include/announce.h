/*****************************************************************************
 * announce.h : Session announcement
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: announce.h,v 1.2 2003/05/26 13:45:51 zorglub Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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
 * sap_session_t: SAP Session descriptor
 *****************************************************************************/

#if defined( UNDER_CE )
#   include <winsock.h>
#elif defined( WIN32 )
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   define close closesocket
#else
#   include <netdb.h>                                         /* hostent ... */
#   include <sys/socket.h>
#   include <netinet/in.h>
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>                    /* inet_ntoa(), inet_aton() */
#   endif
#endif

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif


struct sap_session_t
{
        char psz_url[256];
        char psz_name[1024];
        char psz_port[8];
        module_t p_network;
        unsigned int socket;
        unsigned int sendnow;
        struct sockaddr_in addr;
};



/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( sap_session_t *,            sout_SAPNew,         ( sout_instance_t *,char * , char * , char * ) );
VLC_EXPORT( void,            sout_SAPSend,        ( sout_instance_t *,sap_session_t * ) );
VLC_EXPORT( void,            sout_SAPDelete,      ( sap_session_t * ) );
