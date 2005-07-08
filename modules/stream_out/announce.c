/*****************************************************************************
 * announce.c : Session announcement
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN (Centrale RÃ©seaux) and its contributors
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

#define DEFAULT_PORT 1234

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
