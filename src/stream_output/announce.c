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

#include <unistd.h>

#include <vlc/vlc.h>

#include <vlc/sout.h>
#undef DEBUG_BUFFER

#include <announce.h>
#include <network.h>

#define SAP_IPV4_ADDR "224.2.127.254" /* Standard port and address for SAP */
#define SAP_PORT 9875

#define SAP_IPV6_ADDR_1 "FF0"
#define SAP_IPV6_ADDR_2 "::2:7FFE"

/*****************************************************************************
 * sout_SAPNew: Creates a SAP Session
 *****************************************************************************/
sap_session_t * sout_SAPNew ( sout_instance_t *p_sout , 
                char * psz_url_arg , char *psz_port_arg , 
                char * psz_name_arg, int ip_version, 
                char * psz_v6_scope )
{
      
    sap_session_t       *p_new; /* The SAP structure */
    module_t            *p_network; /* Network module */
    network_socket_t    socket_desc; /* Socket descriptor */
    char                psz_network[6]; /* IPv4 or IPv6 */
    struct              sockaddr_in addr; /* IPv4 connection structure */
    struct              sockaddr_in6 addr6;/* IPv6 connection structure */
    char                *sap_ipv6_addr=NULL; /* IPv6 built address */     
    void                *net_ipv6_addr=NULL; /* IPv6 address in net-format */
    int                 i_status=0;  /* Problems on system calls*/
    
    /* Allocate the SAP structure */ 
    p_new = (sap_session_t *)malloc( sizeof ( sap_session_t ) ) ;
    if ( !p_new )
    {
        msg_Err( p_sout, "No memory left" );        
        return NULL;
    }
                
    /* Fill the information in the structure */
    sprintf ( p_new->psz_url , "%s" , psz_url_arg );
    sprintf ( p_new->psz_name , "%s" , psz_name_arg );
    /* Port is not implemented in sout */
    sprintf ( p_new->psz_port, "%s" , psz_port_arg );

    p_new->i_ip_version = ip_version;
                        
    /* Only "6" triggers IPv6. IPv4 is default */
    if(ip_version != 6)
    {
        msg_Dbg( p_sout , "Creating IPv4 SAP socket" );
        
        /* Fill the socket descriptor */
        socket_desc.i_type            = NETWORK_UDP;
        socket_desc.psz_bind_addr     = "";
        socket_desc.i_bind_port       = 0;
        socket_desc.psz_server_addr   = SAP_IPV4_ADDR;
        socket_desc.i_server_port     = SAP_PORT;
        socket_desc.i_handle          = 0;
       
        /* Call the network module */ 
        sprintf ( psz_network, "ipv4" ); 
        p_sout->p_private=(void*) &socket_desc;
        if( !( p_network = module_Need( p_sout, "network", psz_network ) ) )
        {
             msg_Warn( p_sout, "failed to open a connection (udp)" );
             return NULL;
        }
        module_Unneed( p_sout, p_network );
              
        p_new->socket   =       socket_desc.i_handle;
        if(p_new->socket <= 0 )
        {
            msg_Warn( p_sout, "Unable to initialize SAP" );
            return NULL;
        }
        
        /* Fill the sockaddr_in structure */ 
        memset( &addr , 0 , sizeof(addr) );
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = inet_addr(SAP_IPV4_ADDR);
        addr.sin_port        = htons( SAP_PORT );
        p_new->addr = addr;
    }
    else
    {
        msg_Dbg(p_sout , "Creating IPv6 SAP socket with scope %s" 
                        , psz_v6_scope );

        /* Initialize and build the IPv6 address to broadcast to */
        sap_ipv6_addr = (char *)malloc(28*sizeof(char));
        if ( !sap_ipv6_addr )
        {
            msg_Err( p_sout, "No memory left" );        
            return NULL;
        }
        sprintf(sap_ipv6_addr,"%s%c%s",
                         SAP_IPV6_ADDR_1,
                         psz_v6_scope[0],
                         SAP_IPV6_ADDR_2); 

        /* Convert it to network format */
        net_ipv6_addr = (struct in6_addr *)malloc( sizeof(struct in6_addr) );
        if ( !net_ipv6_addr )
        {
            msg_Err( p_sout, "No memory left" );        
            return NULL;
        }
        
        i_status         = inet_pton(AF_INET6,sap_ipv6_addr,net_ipv6_addr);
        if(i_status < 0 )
        {
           msg_Warn(p_sout,"Unable to convert address to network format");
           return NULL;
        }
        else if(i_status == 0)
        {
            msg_Warn(p_sout,"Adresse de diffusion SAP invalide"); 
            return NULL;
        }
        
        /* Fill the socket descriptor */
        socket_desc.i_type        = NETWORK_UDP;
        socket_desc.psz_bind_addr = ""; 
        socket_desc.i_bind_port   = 0;
        socket_desc.psz_server_addr = sap_ipv6_addr;
        socket_desc.i_server_port     = SAP_PORT;
        socket_desc.i_handle          = 0;
            
        sprintf ( psz_network, "ipv6" ); 
        
        /* Call the network module */
        p_sout->p_private=(void*) &socket_desc;
        if( !( p_network = module_Need( p_sout, "network", psz_network ) ) )
        {
            msg_Warn( p_sout, "failed to open a connection (udp)" );
            return NULL;
        }
        module_Unneed( p_sout, p_network );
               
        p_new->socket   =       socket_desc.i_handle;

        if(p_new->socket <= 0 )
        {
             msg_Warn( p_sout, "Unable to initialize SAP" );
             return NULL;
         }
                
        /* Fill the sockaddr_in structure */
        memset( &addr6 , 0 , sizeof(addr6) );
        addr6.sin6_family      = AF_INET6;
        memcpy( &addr6.sin6_addr.s6_addr , net_ipv6_addr ,
                   sizeof(addr6.sin6_addr.s6_addr) );
            
        addr6.sin6_port        = htons( SAP_PORT );
        p_new->addr6 = addr6;

        /* Free what we allocated */
        if( sap_ipv6_addr ) free(sap_ipv6_addr);
        if( net_ipv6_addr ) free(net_ipv6_addr);
    }
            
    msg_Dbg (p_sout,"SAP initialization complete");
  
    return(p_new);
}

/*****************************************************************************
 * sout_SAPDelete: Deletes a SAP Session 
 *****************************************************************************/
void sout_SAPDelete( sout_instance_t *p_sout , sap_session_t * p_this )
{
        if( close(p_this->socket) )
                msg_Err ( p_sout, "Unable to close SAP socket");
        
        if( p_this ) free( p_this );        
}       

/*****************************************************************************
 * sout_SAPSend: Sends a SAP packet 
 *****************************************************************************/
void sout_SAPSend( sout_instance_t *p_sout, sap_session_t * p_this)
{
        
    char *sap_head;                         /* SAP header */
    char sap_msg[1000];                     /* SDP content */
    char *sap_send;                         /* What we send */
    char *payload_type="application/sdp";   
    int i_send_result=0;                    /* Result of send */
    int i;
    int i_header_size;                      /* SAP header size */
    int i_msg_size;                         /* SDP content size */
    int i_size;                             /* Total size */
    
    /* We send a packet every 24 calls to the function */
    if( p_this->sendnow == 24 )
    {
        i_header_size = 9 + strlen( payload_type );
        sap_head = ( char * )malloc( i_header_size * sizeof( char ) );
        
        if( ! sap_head )         
        {
            msg_Warn( p_sout , "No memory left");
            return;
        }       
         
        /* Create the SAP headers */
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
       
        /* Create the SDP content */
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
     
       /* Create the message */
       sap_send = ( char* )malloc( i_size*sizeof(char) ); 
       if(! sap_send)
       {
           msg_Err( p_sout ,  "No memory left") ;
          return;
       } 
      
       for(i=0 ; i<i_header_size ; i++)
       {
           sap_send[i] = sap_head[i];
       }
       
       for( ;  i<i_size; i++)
       {
           sap_send[i] = sap_msg[i-i_header_size];
       }
    
      if(i_size<1024) /* We mustn't send packets larger than 1024B */     
      {
          if( p_this->i_ip_version == 6)
          {
               i_send_result =  sendto( p_this->socket , sap_send ,
                           i_size , 0 , 
                           (struct sockaddr *)&p_this->addr6, 
                           sizeof(p_this->addr6) );
          } 
          else
          {
                i_send_result =  sendto( p_this->socket , sap_send ,
                             i_size , 0 , 
                             (struct sockaddr *)&p_this->addr , 
                             sizeof(p_this->addr) );
           }
      }
     
      if(i_send_result == -1)
      {
          msg_Warn(p_sout , "SAP Send failed on socket %i. " ,
                          p_this->socket );
          perror("sendto"); 
       }
      
       p_this->sendnow = 0;
       
       /* Free what we allocated */
       if(sap_send) free(sap_send);
       if(sap_head) free(sap_head);
   }
    
   p_this->sendnow++;  
}
