/*******************************************************************************
 * input_vlan.c: vlan management library
 * (c)1999 VideoLAN
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "netutils.h"
#include "input_vlan.h"
#include "intf_msg.h"
#include "main.h"

/*******************************************************************************
 * input_vlan_t: vlan library data
 *******************************************************************************
 * Store global vlan library data.
 *******************************************************************************/
typedef struct input_vlan_s
{    
    int         i_vlan_id;                              /* current vlan number */
    mtime_t     last_change;                               /* last change date */
} input_vlan_t;

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/
static int ZeTrucMucheFunction( int Channel );

/*******************************************************************************
 * input_VlanCreate: initialize global vlan method data
 *******************************************************************************
 * Initialize vlan input method global data. This function should be called
 * once before any input thread is created or any call to other input_Vlan*()
 * function is attempted.
 *******************************************************************************/
int input_VlanCreate( void )
{
    /* Allocate structure */
    p_main->p_vlan = malloc( sizeof( input_vlan_t ) );
    if( p_main->p_vlan == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        return( 1 );        
    }

    /* Initialize structure */
    p_main->p_vlan->i_vlan_id   = 0;
    p_main->p_vlan->last_change = 0;    

    intf_Msg("VLANs initialized\n");    
    return( 0 );    
}

/*******************************************************************************
 * input_VlanDestroy: free global vlan method data
 *******************************************************************************
 * Free resources allocated by input_VlanMethodInit. This function should be
 * called at the end of the program.
 *******************************************************************************/
void input_VlanDestroy( void )
{
    /* Return to default vlan */
    if( p_main->p_vlan->i_vlan_id != 0 )
    {
        input_VlanJoin( 0 );        
    }    
    
    /* Free structure */
    free( p_main->p_vlan );        
}

/*******************************************************************************
 * input_VlanJoin: join a vlan
 *******************************************************************************
 * This function will try to join a vlan. If the relevant interface is already
 * on the good vlan, nothing will be done. Else, and if possible (if the 
 * interface is not locked), the vlan server will be contacted and a change will
 * be requested. The function will block until the change is effective. Note
 * that once a vlan is no more used, it's interface should be unlocked using
 * input_VlanLeave().
 * Non 0 will be returned in case of error.
 *******************************************************************************/
int input_VlanJoin( int i_vlan_id )
{    
    /* If last change is too recent, wait a while */
    if( mdate() - p_main->p_vlan->last_change < INPUT_VLAN_CHANGE_DELAY )
    {
        intf_Msg("Waiting before changing VLAN...\n");
        mwait( p_main->p_vlan->last_change + INPUT_VLAN_CHANGE_DELAY );        
    }
    p_main->p_vlan->last_change = mdate();
    p_main->p_vlan->i_vlan_id = i_vlan_id;    

    intf_Msg("Joining VLAN %d (channel %d)\n", i_vlan_id + 2, i_vlan_id );
    return( ZeTrucMucheFunction( i_vlan_id ) ); // ?? join vlan
}

/*******************************************************************************
 * input_VlanLeave: leave a vlan
 *******************************************************************************
 * This function tells the vlan library that the designed interface is no more
 * locked and than vlan changes can occur.
 *******************************************************************************/
void input_VlanLeave( int i_vlan_id )
{
    // ??
}

/* following functions are local */

static int ZeTrucMucheFunction( int Channel)
{
	int  			i_socket;
	char	*		ipaddr;
	struct ifreq		interface;
	struct sockaddr_in	sa_server;
	struct sockaddr_in	sa_client;
        char mess[80];

	return( 0 );
        
  	/*      
	 *Looking for informations about the eth0 interface
	 */

	interface.ifr_addr.sa_family = AF_INET;
	strcpy( interface.ifr_name, main_GetPszVariable( INPUT_IFACE_VAR, INPUT_IFACE_DEFAULT ) );

	i_socket = socket( AF_INET, SOCK_DGRAM, 0 );

	/* Looking for the interface IP address */
	ioctl( i_socket, SIOCGIFDSTADDR, &interface );
	ipaddr = inet_ntoa((*(struct sockaddr_in *)(&(interface.ifr_addr))).sin_addr ); 

	/* Looking for the interface MAC address */
	ioctl( i_socket, SIOCGIFHWADDR, &interface );
	close( i_socket );
	
	/* 
	 * Getting address, port, ... of the server
	 */

	/* Initialize */
	bzero( &sa_server, sizeof(struct sockaddr_in) );
	/* sin_family is ALWAYS set to AF_INET (see in man 7 ip)*/
	sa_server.sin_family = AF_INET;
	/* Giving port on to connect after having convert it*/
	sa_server.sin_port = htons ( main_GetIntVariable( INPUT_VLAN_PORT_VAR, INPUT_VLAN_PORT_DEFAULT ));
	/* Giving address after having convert it into binary data*/
	inet_aton( main_GetPszVariable( INPUT_VLAN_SERVER_VAR, INPUT_VLAN_SERVER_DEFAULT ), &(sa_server.sin_addr) );
	
	/*
	 * Getting address, port, ... of the client 
	 */

	/* Initialize */
	bzero( &sa_client, sizeof(struct sockaddr_in) );
	/* sin_family is ALWAYS set to AF_INET (see in man 7 ip)*/
	sa_client.sin_family = AF_INET;
	/* Giving port on to connect after having convert it*/
	sa_client.sin_port = htons( 0 );
	/* Giving address after having convert it into binary data*/
	inet_aton( ipaddr, &(sa_client.sin_addr) );
	
	/* Initialization of the socket */
	i_socket = socket(AF_INET, SOCK_DGRAM, 17 ); // ?? UDP
	 /*  SOCK_DGRAM because here we use DATAGRAM
	  * Sachant qu'il y a un #define AF_INET = PF_INET dans sys/socket.h et que PF_INET est le IP protocol family ...
	  * Protocol is in #define, should be 17 for udp */

	/* Elaborate the message to send */
        sprintf( mess , "%d %s %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x \n",
		    Channel, ipaddr,
		    interface.ifr_hwaddr.sa_data[0] & 0xff,
		    interface.ifr_hwaddr.sa_data[1] & 0xff,
		    interface.ifr_hwaddr.sa_data[2] & 0xff,
		    interface.ifr_hwaddr.sa_data[3] & 0xff,
		    interface.ifr_hwaddr.sa_data[4] & 0xff,
		    interface.ifr_hwaddr.sa_data[5] & 0xff,
                    interface.ifr_hwaddr.sa_data[0] & 0xff,
		    interface.ifr_hwaddr.sa_data[1] & 0xff,
		    interface.ifr_hwaddr.sa_data[2] & 0xff,
		    interface.ifr_hwaddr.sa_data[3] & 0xff,
		    interface.ifr_hwaddr.sa_data[4] & 0xff,
		    interface.ifr_hwaddr.sa_data[5] & 0xff
	    );
	
	/* Send the message */
	intf_DbgMsg("%s\n", mess);
        sendto(i_socket,mess,80,0,(struct sockaddr *)&sa_server,sizeof(struct sockaddr));
	
	/*Close the socket */
	close( i_socket );

	return 0;
}
