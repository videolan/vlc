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
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>

#include <vlc/sout.h>
#undef DEBUG_BUFFER

#include <announce.h>
#include <network.h>

#define SAP_ADDR "224.2.127.254" /* Standard port and address for SAP */
#define SAP_PORT 9875

/*****************************************************************************
 * sout_SAPNew: Creates a SAP Session
 *****************************************************************************/
sap_session_t * sout_SAPNew ( sout_instance_t *p_sout , char * psz_url_arg , char *psz_port_arg , char * psz_name_arg )
{
        sap_session_t           *p_new;
        module_t                *p_network;
        network_socket_t        socket_desc;
        char                    psz_network[12];       
        struct                  sockaddr_in addr;
              
        p_new = (sap_session_t *)malloc( sizeof ( sap_session_t ) ) ;
        
        sprintf ( p_new->psz_url , "%s" , psz_url_arg );
        sprintf ( p_new->psz_name , "%s" , psz_name_arg );
        sprintf ( p_new->psz_port, "%s" , psz_port_arg ); /* Not implemented in SO */
        
        msg_Dbg (p_sout , "Creating SAP Socket" );
        
        socket_desc.i_type        = NETWORK_UDP;
        socket_desc.psz_bind_addr = "";
        socket_desc.i_bind_port   = 0;
        socket_desc.psz_server_addr   = SAP_ADDR;
        socket_desc.i_server_port     = SAP_PORT;
        socket_desc.i_handle          = 0;
        
        sprintf ( psz_network, "ipv4" ); 

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
    
        p_new->addr = addr;
    
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
      char *sap_head;
      char sap_msg[1000];
      char *sap_send;
      char *payload_type="application/sdp";
      int i_send_result;
      int i;
      int i_header_size;
      int i_msg_size;
      int i_size;
     
    if( p_this->sendnow == 1 )
    {
         i_header_size = 9 + strlen( payload_type );
         sap_head = ( char * )malloc( i_header_size * sizeof( char ) );
                
          sap_head[0]=0x20; /* Means IPv4, not encrypted, not compressed */
          sap_head[1]=0x00; /* No authentification */
          sap_head[2]=0x42; /* Version */
          sap_head[3]=0x12; /* Version */
          
          sap_head[4]=0x01; /* Source IP  FIXME: we should get the real address */
          sap_head[5]=0x02; /* idem */
          sap_head[6]=0x03; /* idem */
          sap_head[7]=0x04; /* idem */
          
          strncpy( sap_head+8 , payload_type , 15 );
          sap_head[ i_header_size-1 ] = '\0'; 
       
        /* Do not add spaces at beginning of the lines ! */  
          sprintf(sap_msg,"v=0\n\
o=VideoLAN 3247692199 3247895918 IN IP4 VideoLAN\n\
s=%s\n\
u=VideoLAN\n\
t=0 0\n\
m=audio %s udp 14\n\
c=IN IP4 %s/15\n\
a=type:test\n", p_this->psz_name , p_this->psz_port , p_this->psz_url );
     
     i_msg_size = strlen( sap_msg );     
     i_size = i_msg_size + i_header_size;
     
     sap_send = ( char* )malloc( i_size*sizeof(char) ); 
          
      for(i=0 ; i<i_header_size ; i++)
      {
           sap_send[i] = sap_head[i];
       }

     for( ;  i<i_size; i++)
     {
         sap_send[i] = sap_msg[i-i_header_size];
     }
    
    /* What we send is the SAP header and the SDP packet */
     
     if(i_size<1024) /* We mustn't send packets larger than 1024B */     
         i_send_result =  sendto( p_this->socket , sap_send , i_size , 0 , (struct sockaddr *)&p_this->addr , sizeof(p_this->addr) );
    
     if(i_send_result == -1)
     {
          msg_Warn(p_sout , "SAP Send failed on socket %i. " , p_this->socket );
          perror("sendto"); 
     }
     p_this->sendnow = 0;
     if(sap_send) free(sap_send);
     if(sap_head) free(sap_head);
   }
   p_this->sendnow++;  
}
