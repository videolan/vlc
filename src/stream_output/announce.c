/*****************************************************************************
 * announce.c : Session announcement
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
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
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>

#include <vlc/sout.h>
#undef DEBUG_BUFFER

#include <announce.h>
#include <network.h>

#define SAP_ADDR "224.2.127.254"
#define SAP_PORT 9875

/*****************************************************************************
 * sout_SAPNew: Creates a SAP Session
 *****************************************************************************/
sap_session_t * sout_SAPNew ( sout_instance_t *p_sout , char * psz_url_arg , char * psz_name_arg )
{
        sap_session_t           *p_new;
        module_t                *p_network;
        network_socket_t        socket_desc;
        char                    psz_network[12];       
        struct                  sockaddr_in addr;
        int                     ttl=15;
              
        p_new = (sap_session_t *)malloc( sizeof ( sap_session_t ) ) ;
        
        sprintf ( p_new->psz_url , "%s" , psz_url_arg );
        sprintf ( p_new->psz_name , "%s" , psz_name_arg );
        
        msg_Dbg (p_sout , "Creating SAP Socket" );
        
        socket_desc.i_type        = NETWORK_UDP;
        socket_desc.psz_bind_addr = SAP_ADDR;
        socket_desc.i_bind_port   = SAP_PORT;
        socket_desc.psz_server_addr   = "";
        socket_desc.i_server_port     = 0;
        socket_desc.i_handle          = 0;
        
        sprintf ( psz_network,"ipv4" ); 

        p_sout->p_private=(void*) &socket_desc;
        
        if( !( p_network = module_Need( p_sout, "network", psz_network ) ) )
        {
            msg_Warn( p_sout, "failed to open a connection (udp)" );
        }

        module_Unneed( p_sout, p_network );
               
        p_new->socket   =       socket_desc.i_handle;

        memset( &addr , 0 , sizeof(addr) );
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = inet_addr(SAP_ADDR);
        addr.sin_port        = htons( SAP_PORT );
    
        setsockopt( p_new->socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl) );
     
        p_new->addr=(struct sockaddr_in)addr;
    
        return(p_new);
}

/*****************************************************************************
 * sout_SAPDelete: Deletes a SAP Session 
 *****************************************************************************/
void sout_SAPDelete( sap_session_t * p_this )
{
        shutdown(p_this->socket,0);
        free(p_this);        
}       

/*****************************************************************************
 * sout_SAPSend: Sends a SAP packet 
 *****************************************************************************/
void sout_SAPSend( sout_instance_t *p_sout, sap_session_t * p_this )
{ 
      char sap_msg[2048];
      char *user="VideoLAN";
      char *machine="VideoLAN";
      char *site="VideoLAN";
      int i_send_result;
     
    if(p_this->sendnow == 24)
    {      
      sprintf(sap_msg,"         ***øv=0 \n\
      o=%s 3247692199 3247895918 IN IP4 %s \n\
      s=%s\n\
      u=%s \n\
      t=3247691400 3250117800 \n\
      a=type:test   \n\
      m=audio 1234 udp 14 \n\
      c=IN IP4 %s/15  \n\
      xxxxxxxxxxxxxxxxxxxxx \n ",user,machine,p_this->psz_name,site,p_this->psz_url);
        
     i_send_result =  sendto( p_this->socket , sap_msg , strlen(sap_msg) , 0 , (struct sockaddr *)&p_this->addr , sizeof(p_this->addr) );
     
     if(i_send_result == -1)
     {
              msg_Warn(p_sout , "SAP Send failed on socket %i. " , p_this->socket );
             perror("send"); 
     }
     p_this->sendnow=0;
    }
     p_this->sendnow++;  
}
