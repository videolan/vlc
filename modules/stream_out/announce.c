/*****************************************************************************
 * announce.c : Session announcement
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
 *          Damien Lucas <nitrox@via.ecp.fr>
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
#include <stdlib.h>                                                /* free() */

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>                                              /* sprintf() */

#include <vlc/vlc.h>
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <sys/socket.h>
#endif

#ifdef HAVE_SLP_H
# include <slp.h>
#endif

#include "announce.h"
#include "network.h"

#define SAP_IPV4_ADDR "224.2.127.254" /* Standard port and address for SAP */
#define SAP_PORT 9875

#define SAP_IPV6_ADDR_1 "FF0"
#define SAP_IPV6_ADDR_2 "::2:7FFE"

#define DEFAULT_PORT 1234


/*****************************************************************************
 * SDPGenerateUDP: create a SDP file
 *****************************************************************************/
char * SDPGenerateUDP(char * psz_name_arg,char * psz_url_arg)
{
    /* Create the SDP content */
    /* Do not add spaces at beginning of the lines ! */

    char                * psz_sdp;
    vlc_url_t           * p_url; /*where parsed url will be stored*/


    /*Allocate the URL structure*/
    p_url = (vlc_url_t *) malloc( sizeof( vlc_url_t) );
    if ( ! p_url )
    {
        return NULL;
    }


    vlc_UrlParse( p_url, psz_url_arg , 0);

    if (!p_url->psz_host)
    {
        return NULL;
    }

    if(p_url->i_port == 0)
    {
        p_url->i_port = DEFAULT_PORT;
    }

    psz_sdp = malloc( sizeof("v=0\n"
                   "o=VideoLAN 3247692199 3247895918 IN IP4 VideoLAN\n"
                   "s=\n"
                   "u=VideoLAN\n"
                   "t=0 0\n"
                   "m=video  udp 33\n"
                   "c=IN IP4 /15\n"
                   "a=type:test\n")
           + strlen(psz_name_arg)
           + 20                             /*lengh of a 64 bits int"*/
           + strlen(p_url->psz_host)+1);


    if ( !psz_sdp )
    {
        return NULL;
    }

    sprintf( psz_sdp,"v=0\n"
                      "o=VideoLAN 3247692199 3247895918 IN IP4 VideoLAN\n"
                      "s=%s\n"
                      "u=VideoLAN\n"
                      "t=0 0\n"
                      "m=video %i udp 33\n"
                      "c=IN IP4 %s/15\n"
                      "a=type:test\n",
             psz_name_arg, p_url->i_port, p_url->psz_host );

    vlc_UrlClean( p_url );

    if (p_url)
    {
        free(p_url);
    }

    p_url = NULL;

    return psz_sdp;
}


/*****************************************************************************
 * sout_SAPNew: Creates a SAP Session
 *****************************************************************************/
sap_session_t2 * sout_SAPNew ( sout_instance_t *p_sout,
                                     char * psz_sdp_arg,
                                     int ip_version,
                                     char * psz_v6_scope )
{
    sap_session_t2       *p_sap; /* The SAP structure */
    char                *sap_ipv6_addr = NULL; /* IPv6 built address */
    vlc_value_t         val;

    var_Create( p_sout, "ipv6", VLC_VAR_BOOL );
    var_Create( p_sout, "ipv4", VLC_VAR_BOOL );

    /* Allocate the SAP structure */
    p_sap = (sap_session_t2 *) malloc( sizeof ( sap_session_t2 ) ) ;
    if ( !p_sap )
    {
        msg_Err( p_sout, "out of memory" );
        return NULL;
    }

    p_sap->i_socket = 0;

    p_sap->psz_sdp=NULL;

    p_sap->i_ip_version = ip_version;

    p_sap->psz_sdp = psz_sdp_arg;

    if( ip_version != 6 )
    {
        val.b_bool = VLC_FALSE;
        var_Set( p_sout, "ipv6", val);
        val.b_bool = VLC_TRUE;
        var_Set( p_sout, "ipv4", val);
        p_sap->i_socket = net_OpenUDP(p_sout, "", 0, SAP_IPV4_ADDR, SAP_PORT );
    }
    else
    {
        val.b_bool = VLC_TRUE;
        var_Set( p_sout, "ipv6", val);
        val.b_bool = VLC_FALSE;
        var_Set( p_sout, "ipv4", val);

        sap_ipv6_addr = (char *) malloc( 28 * sizeof(char) );
        if ( !sap_ipv6_addr )
        {
            msg_Err( p_sout, "out of memory" );
            return NULL;
        }
        sprintf( sap_ipv6_addr, "%s%c%s",
                 SAP_IPV6_ADDR_1, psz_v6_scope[0], SAP_IPV6_ADDR_2 );

        p_sap->i_socket = net_OpenUDP(p_sout, "", 0, sap_ipv6_addr, SAP_PORT );

        if( sap_ipv6_addr )
        {
            free( sap_ipv6_addr );
        }
    }

    if((int)p_sap->i_socket <= 0)
    {
        msg_Warn(p_sout, "invalid SAP socket");
        return NULL;
    }

        /* Free what we allocated */

    msg_Dbg( p_sout, "SAP initialization complete" );

    return p_sap;
}

/*****************************************************************************
 * sout_SAPDelete: Deletes a SAP Session
 *****************************************************************************/
void sout_SAPDelete( sout_instance_t *p_sout, sap_session_t2 * p_sap )
{
    int i_ret;

#if defined( UNDER_CE )
    i_ret = CloseHandle( (HANDLE)p_sap->i_socket );
#elif defined( WIN32 )
    i_ret = closesocket( p_sap->i_socket );
#else
    i_ret = close( p_sap->i_socket );
#endif

    if( i_ret )
    {
        msg_Err( p_sout, "unable to close SAP socket" );
    }

    if (p_sap->psz_sdp)
    {
        free(p_sap->psz_sdp);
        p_sap->psz_sdp = NULL;
    }

    free( p_sap );
}

/*****************************************************************************
 * sout_SAPSend: Sends a SAP packet
 *****************************************************************************/
void sout_SAPSend( sout_instance_t *p_sout, sap_session_t2 * p_sap )
{
    char *psz_msg;                     /* SDP content */
    char *psz_head;                         /* SAP header */
    char *psz_send;                         /* What we send */
    char *psz_type = "application/sdp";
    int i_header_size;                      /* SAP header size */
    int i_msg_size;                         /* SDP content size */
    int i_size;                             /* Total size */
    int i_ret = 0;

    /* We send a packet every 24 calls to the function */
    if( p_sap->i_calls++ < 24 )
    {
        return;
    }

    i_header_size = 8 + strlen( psz_type ) + 1;
    psz_head = (char *) malloc( i_header_size * sizeof( char ) );

    if( ! psz_head )
    {
        msg_Err( p_sout, "out of memory" );
        return;
    }

    /* Create the SAP headers */
    psz_head[0] = 0x20; /* Means IPv4, not encrypted, not compressed */
    psz_head[1] = 0x00; /* No authentification */
    psz_head[2] = 0x42; /* Version */
    psz_head[3] = 0x12; /* Version */

    psz_head[4] = 0x01; /* Source IP  FIXME: we should get the real address */
    psz_head[5] = 0x02; /* idem */
    psz_head[6] = 0x03; /* idem */
    psz_head[7] = 0x04; /* idem */

    strncpy( psz_head + 8, psz_type, 15 );
    psz_head[ i_header_size-1 ] = '\0';

    psz_msg = p_sap->psz_sdp;

    if(!psz_msg)
    {
        msg_Err( p_sout, "no sdp packet" );
        return;
    }

    i_msg_size = strlen( psz_msg );
    i_size = i_msg_size + i_header_size;

    /* Create the message */
    psz_send = (char *) malloc( i_size*sizeof(char) );
    if( !psz_send )
    {
        msg_Err( p_sout, "out of memory" );
        return;
    }

    memcpy( psz_send, psz_head, i_header_size );
    memcpy( psz_send + i_header_size, psz_msg, i_msg_size );

    if( i_size < 1024 ) /* We mustn't send packets larger than 1024B */
    {
        i_ret = net_Write(p_sout, p_sap->i_socket, psz_send, i_size );
    }

    if( i_ret <= 0 )
    {
        msg_Warn( p_sout, "SAP send failed on socket %i (%s)",
                          p_sap->i_socket, strerror(errno) );
    }

    p_sap->i_calls = 0;

    /* Free what we allocated */
    free( psz_send );
    free( psz_head );
}

#ifdef HAVE_SLP_H
/*****************************************************************************
 * sout_SLPBuildName: Builds a service name according to SLP standard
 *****************************************************************************/
static char * sout_SLPBuildName(char *psz_url,char *psz_name)
{
    char *psz_service;
    unsigned int i_size;

    /* name to build is: service:vlc.services.videolan://$(url) */

    i_size =  8 + 12 + 12 + 5 + strlen(psz_url) + 1;

    psz_service=(char *)malloc(i_size * sizeof(char));

    snprintf( psz_service , i_size,
              "service:vlc.services.videolan://udp:@%s",
              psz_url);
        /* How piggy  ! */

    psz_service[i_size]='\0'; /* Just to make sure */

    return psz_service;

}

/*****************************************************************************
 * sout_SLPReport: Reporting function. Unused at the moment but needed
 *****************************************************************************/
static void sout_SLPReport(SLPHandle slp_handle,SLPError slp_error,void* cookie)
{
}
#endif

/*****************************************************************************
 * sout_SLPReg: Registers the program with SLP
 *****************************************************************************/
int sout_SLPReg( sout_instance_t *p_sout, char * psz_url,
                               char * psz_name)
{
#ifdef HAVE_SLP_H
    SLPHandle   slp_handle;
    SLPError    slp_res;
    char *psz_service= sout_SLPBuildName(psz_url,psz_name);

    if( SLPOpen( NULL, SLP_FALSE, &slp_handle ) != SLP_OK)
    {
        msg_Warn(p_sout,"Unable to initialize SLP");
        return -1;
    }

    msg_Info(p_sout , "Registering %s (name: %s) in SLP",
                      psz_service , psz_name);

    slp_res = SLPReg ( slp_handle,
            psz_service,
            SLP_LIFETIME_MAXIMUM,
            NULL,
            psz_name,
            SLP_TRUE,
            sout_SLPReport,
            NULL );

    if( slp_res != SLP_OK )
    {
        msg_Warn(p_sout,"Error while registering service: %i", slp_res );
        return -1;
    }

    return 0;

#else /* This function should never be called if this is false */
    return -1;
#endif
}


/*****************************************************************************
 * sout_SLDePReg: Unregisters the program from SLP
 *****************************************************************************/
int sout_SLPDereg( sout_instance_t *p_sout, char * psz_url,
                               char * psz_name)
{
#ifdef HAVE_SLP_H

    SLPHandle   slp_handle;
    SLPError    slp_res;
    char *psz_service= sout_SLPBuildName(psz_url,psz_name);

    if( SLPOpen( NULL, SLP_FALSE, &slp_handle ) != SLP_OK)
    {
        msg_Warn(p_sout,"Unable to initialize SLP");
        return -1;
    }

    msg_Info(p_sout , "Unregistering %s from SLP",
                      psz_service);

    slp_res = SLPDereg ( slp_handle,
            psz_service,
            sout_SLPReport,
            NULL );

    if( slp_res != SLP_OK )
    {
        msg_Warn(p_sout,"Error while registering service: %i", slp_res );
        return -1;
    }

    return 0;

#else /* This function should never be called if this is false */
    return -1;
#endif
}
