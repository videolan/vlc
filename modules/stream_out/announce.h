/*****************************************************************************
 * announce.h : Session announcement
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
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

#ifdef HAVE_SLP_H
#   include <slp.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif

/*****************************************************************************
 * sap_session_t: SAP Session descriptor
 *****************************************************************************/
struct sap_session_t2
{
        char * psz_sdp;
        module_t p_network;
        unsigned int i_socket;
        unsigned int i_calls;
        int i_ip_version;
};

typedef struct sap_session_t2 sap_session_t2;
/*****************************************************************************
 * slp_session_t: SLP Session descriptor
 *****************************************************************************/
struct slp_session_t
{
        char *psz_url;
        char *psz_name;
};

typedef struct slp_session_t slp_session_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
sap_session_t2 *  sout_SAPNew        (sout_instance_t *, char * psz_sdp_arg,
                                                      int ip_version,
                                                      char * psz_v6_scope );

char * SDPGenerateUDP(char * ,char *);
void             sout_SAPDelete     (sout_instance_t *,sap_session_t2*);
void             sout_SAPSend       (sout_instance_t *,sap_session_t2 *);
int              sout_SLPReg        (sout_instance_t *,char *,char *);
int              sout_SLPDereg      (sout_instance_t *,char *,char *);

/*
VLC_EXPORT( sap_session_t *, sout_SAPNew, ( sout_instance_t *,char * , char * , int , char *) );
VLC_EXPORT( void, sout_SAPSend, ( sout_instance_t *,sap_session_t *) );
VLC_EXPORT( void, sout_SAPDelete, ( sout_instance_t *,sap_session_t * ) );

VLC_EXPORT( int, sout_SLPReg, (sout_instance_t*,char*,char*) );
VLC_EXPORT( int, sout_SLPDereg, (sout_instance_t*,char*,char*) );*/
