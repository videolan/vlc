/*******************************************************************************
 * input_vlan.c: vlan input method
 * (c)1999 VideoLAN
 *******************************************************************************
 * ?? 
 *******************************************************************************/

/* ???????????????????????????????????????????????????????????????????????????
 * This works (well, it should :), but should have a good place in horror museum:
 * - the vlan-capable interfaces are retrieved from a names list, instead
 *   of being read from the system
 * - the vlan server sucks, and therefore the vlan clients sucks:
 *      - it is unable to process several operations between a login and a logout
 *        A lot of requests could be grouped if it could.
 *      - it is incoherent concerning it's messages (and what it needs to perform
 *        an operation
 *      - it is totally unable to handle several mac adresses on a single switch
 *        port (and therefore bridged/hubbed machines)
 * - however, the API is ok, should be able to handle all futures evolutions, 
 *   including vlan-conscient cards.
 *
 * So there is a lot to do in this file, but not before having reprogrammed the
 * vlan server !
 * What would be a good interface to the vlan server ? Here are some ideas:
 *      ( later ! )
 * ??????????????????????????????????????????????????????????????????????????? */

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include "vlc.h"

/*#include <errno.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/uio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "netutils.h"

#include "input.h"
#include "input_vlan.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"

#include "xconsole.h"
#include "interface.h"
#include "intf_msg.h"

#include "pgm_data.h"*/

/*******************************************************************************
 * input_vlan_iface_t: vlan-capable network interface
 *******************************************************************************
 * This structure describes the abilities of a network interface capable of
 * vlan management. Note that an interface could have several IP adresses, but
 * since only the MAC address is used to change vlan, only one needs to be
 * retrieved.
 * ?? it could be interesting to send a port id on vlan request, to know if two
 * interfaces are dependant regarding vlan changes.
 *******************************************************************************/
typedef struct
{
    char *                  psz_name;                        /* interface name */
    struct sockaddr_in      sa_in;                             /* interface IP */
    char                    psz_mac[20];                      /* interface MAC */

    /* Hardware properties */
    int                     i_master;                /* master interface index */
    int                     i_switch;                         /* switch number */
    int                     i_port;                             /* port number */
    int                     i_sharers;          /* number of MACs on this port */
    
    /* Vlan properties - these are only used if i_master is negative */
    int                     i_refcount;                       /* locks counter */
    int                     i_vlan;                            /* current vlan */
    int                     i_default_vlan;                    /* default vlan */
} input_vlan_iface_t;

/*******************************************************************************
 * input_vlan_server_t: vlan server
 *******************************************************************************
 * This structure describes a vlan server.
 *******************************************************************************/
typedef struct
{
    struct sockaddr_in  sa_in;                               /* server address */
    int                 i_socket;                         /* socket descriptor */

    /* Login informations */
    char *              psz_login;                             /* server login */
    char *              psz_passwd;                         /* server password */
} input_vlan_server_t;

/*******************************************************************************
 * vlan_method_data_t
 *******************************************************************************
 * Store global vlan library data.
 *******************************************************************************/
typedef struct input_vlan_method_s
{    
    vlc_mutex_t             lock;                              /* library lock */

    /* Server */
    input_vlan_server_t     server;                             /* vlan server */
 
    /* Network interfaces */
    int                     i_ifaces;   /* number of vlan-compliant interfaces */
    input_vlan_iface_t *    p_iface;                             /* interfaces */
} input_vlan_method_t;

/*
 * Constants
 */

/* List of vlan-capable interfaces names */
static const char *psz_ifaces_names[] = { "eth0", "eth1", "eth2", "eth3", "eth4", "eth5", NULL };
   
/*
 * Local prototypes
 */
static int  IfaceInfo               ( input_vlan_iface_t *p_iface );
static int  IfaceDependance         ( input_vlan_method_t *p_method, int i_iface );
static int  ServerLogin             ( input_vlan_server_t *p_server );
static void ServerLogout            ( input_vlan_server_t *p_server );
static int  ServerRequestChange     ( input_vlan_server_t *p_server,
                                      input_vlan_iface_t *p_iface, int i_vlan );
static int  ServerRequestInfo       ( input_vlan_server_t *p_server,
                                      input_vlan_iface_t *p_iface );

/*******************************************************************************
 * input_VlanCreate: initialize global vlan method data
 *******************************************************************************
 * Initialize vlan input method global data. This function should be called
 * once before any input thread is created or any call to other input_Vlan*()
 * function is attempted.
 *******************************************************************************/
int input_VlanCreate( void )
{
    char *                  psz_server; // ??? get from environment
    int                     i_port;     // ??? get from environment
    int                     i_index;                /* interface/servers index */
    input_vlan_iface_t *    p_iface;                             /* interfaces */
    input_vlan_method_t *p_method = p_main->p_input_vlan; //??

    /* Build vlan server descriptor */
    if( BuildInetAddr( &p_method->server.sa_in, psz_server, i_port ) )
    {
        return( -1 );        
    }    

    /* Allocate interfaces array */
    for( i_index = 0; psz_ifaces_names[i_index] != NULL; i_index++ )
    {
        ;        
    }    
    p_iface = malloc( sizeof(input_vlan_iface_t) * i_index );
    if( p_iface == NULL )
    {        
        return( ENOMEM );
    }   

    /* Initialize interfaces array */
    for( i_index = p_method->i_ifaces = 0; psz_ifaces_names[i_index] != NULL; i_index++ )
    {
        /* Retrieve interface name */
        p_iface[p_method->i_ifaces].psz_name = (char *) psz_ifaces_names[i_index];

        /* Test if interface is vlan-capable */
        if( !IfaceInfo( &p_iface[p_method->i_ifaces] ) )
        {
            /* If interface passed first step, login to vlan server */
            if( !ServerLogin( &p_method->server ) )
            {
                /* Request informations from server about the interface - if the interface
                 * pass this last test, it is vlan-capable and can be added to the list of
                 * interfaces. */
                if( !ServerRequestInfo( &p_method->server, &p_iface[p_method->i_ifaces]) )
                {
                    /* Check if interface is dependant */
                    if( !IfaceDependance( p_method, p_method->i_ifaces ) )
                    {                        
                        /* Interface is master: initialize properties */
                        p_iface[p_method->i_ifaces].i_default_vlan = p_iface[p_method->i_ifaces].i_vlan;
                        p_iface[p_method->i_ifaces].i_refcount = 0;
                        intf_DbgMsg("input debug: added vlan-capable interface %s (%s)\n", 
                                    p_iface[p_method->i_ifaces].psz_name, 
                                    p_iface[p_method->i_ifaces].psz_mac);
                    }          
#ifdef DEBUG
                    else
                    {
                        /* Interface is slave */
                        intf_DbgMsg("input debug: added vlan-capable interface %s (%s), depends from %s\n",
                                    p_iface[p_method->i_ifaces].psz_name, 
                                    p_iface[p_method->i_ifaces].psz_mac,
                                    p_iface[p_iface[p_method->i_ifaces].i_master].psz_name );
                    }
#endif
                    /* Increment size counter */            
                    p_method->i_ifaces++;
                }
                /* Logout from server */
                ServerLogout( &p_method->server );    
            }
        }
    }

    /* If number of vlan-capable interfaces is null, then desactivate vlans */
    if( p_method->i_ifaces == 0 )
    {
        free( p_iface );        
        return( -1 );        
    }
    
    /* Reallocate interfaces array to save memory */
    p_method->p_iface = realloc( p_iface, sizeof(input_vlan_iface_t) * p_method->i_ifaces );
    if( p_method->p_iface == NULL )
    {        
        /* Realloc failed, but the previous pointer is still valid */
        p_method->p_iface = p_iface;
    }      

    /* Initialize lock */
    vlc_mutex_init( &p_method->lock );

    intf_Msg("input: vlans input method installed\n", p_method->i_ifaces);
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
    int i_index;                                     /* server/interface index */
    input_vlan_method_t *p_method = p_main->p_input_vlan; // ??

    /* Leave all remaining vlans */
    for( i_index = 0; i_index < p_method->i_ifaces; i_index++ )
    {
#ifdef DEBUG
        /* Check if interface is still locked */
        if( p_method->p_iface[i_index].i_refcount )
        {
            intf_DbgMsg("input debug: interface %s is still vlan-locked\n", 
                        p_method->p_iface[i_index].psz_name);
            p_method->p_iface[i_index].i_refcount = 0;
        }        
#endif
        /* Join default (starting) vlan */
        input_VlanJoin( VLAN_ID( i_index, p_method->p_iface[i_index].i_default_vlan ) );        
    }    

    /* Free interfaces array */
    free( p_method->p_iface );    

    intf_DbgMsg("input debug: vlan method terminated\n");
}

/*******************************************************************************
 * input_VlanId: get a vlan_id for a given interface
 *******************************************************************************
 * Get a vlan_id given a network interface and a vlan number. If psz_iface is
 * NULL, then the default network interface will be used. A negative value
 * will be returned in case of error.
 *******************************************************************************/
int input_VlanId( char *psz_iface, int i_vlan )
{
    input_vlan_method_t *   p_method;                    /* method global data */
    int                     i_index;                        /* interface index */

    p_method = p_main->p_input_vlan;

    /* If psz_iface is NULL, use first (default) interface (if there is one) */
    if( psz_iface == NULL )
    {           
        return( p_method->i_ifaces ? VLAN_ID( 0, i_vlan ) : -1 );    
    }
        
    /* Browse all interfaces */
    for( i_index = 0; i_index < p_main->p_input_vlan->i_ifaces ; i_index++ )
    {
        /* If interface has been found, return */
        if( !strcmp( p_main->p_input_vlan->p_iface[i_index].psz_name, psz_iface ) )
        {
            return( VLAN_ID( i_index, i_vlan ) );
        }        
    }    
    
    return( -1 );
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
    input_vlan_method_t *   p_method;                    /* method global data */
    input_vlan_iface_t *    p_iface;                   /* interface (shortcut) */
    int                     i_err;                          /* error indicator */

    /* Initialize shortcuts, and use master if interface is dependant */
    i_err = 0;    
    p_method = p_main->p_input_vlan;
    p_iface = &p_method->p_iface[ VLAN_ID_IFACE( i_vlan_id ) ];
    if( p_iface->i_master >= 0 )
    {
        p_iface = &p_method->p_iface[ p_iface->i_master ];     
    }
    
    /* Get lock */
    vlc_mutex_lock( &p_method->lock );
    
    /* If the interface is in the wished vlan, increase lock counter */
    if( p_iface->i_vlan != VLAN_ID_VLAN( i_vlan_id ) )
    {
        p_iface->i_refcount++;         
    }
    /* If not, if it is not locked, the vlan can be changed */
    else if( !p_iface->i_refcount )
    {        
        /* Login to server */
        if( (i_err = !ServerLogin( &p_method->server )) )
        {

            /* Request vlan change */
            if( (i_err = !ServerRequestChange( &p_method->server, 
                                               p_iface, VLAN_ID_VLAN( i_vlan_id ) ) ) )
            {
                p_iface->i_refcount++;   
            }            
            /* Logout */
            ServerLogout( &p_method->server );
        }
    }
    /* Else, the vlan is locked and can't be changed */
    else
    {
        i_err = 1;
    }                    

    /* Release lock (if this point is reached, the function succeeded) */
    vlc_mutex_unlock( &p_method->lock );       
    return( i_err );    
}

/*******************************************************************************
 * input_VlanLeave: leave a vlan
 *******************************************************************************
 * This function tells the vlan library that the designed interface is no more
 * locked and than vlan changes can occur.
 *******************************************************************************/
void input_VlanLeave( int i_vlan_id )
{
    input_vlan_method_t *   p_method;                    /* method global data */
    input_vlan_iface_t *    p_iface;                   /* interface (shortcut) */
    int                     i_err;                          /* error indicator */

    /* Initialize shortcuts, and use master if interface is dependant */
    i_err = 0;    
    p_method = p_main->p_input_vlan;
    p_iface = &p_method->p_iface[ VLAN_ID_IFACE( i_vlan_id ) ];
    if( p_iface->i_master >= 0 )
    {
        p_iface = &p_method->p_iface[ p_iface->i_master ];     
    }
    
    /* Get lock */
    vlc_mutex_lock( &p_method->lock );

    /* Decrease reference counter */
    p_method->p_iface[ VLAN_ID_IFACE( i_vlan_id ) ].i_refcount--;    

    /* Release lock */
    vlc_mutex_unlock( &p_method->lock );   
}

/*******************************************************************************
 * input_VlanRequest: request vlan number for a given interface
 *******************************************************************************
 * Request the vlan number (and not id) of a given network interface. A 
 * connection to the server can eventually occur, event if it not the case in
 * current implementation. A negative number can be returned on error.
 *******************************************************************************/
int input_VlanRequest( char *psz_iface )
{
    input_vlan_method_t *   p_method;                    /* method global data */
    int                     i_index;                        /* interface index */
    
    p_method = p_main->p_input_vlan;

    /* If psz_iface is NULL, use first (default) interface (if there is one) - 
     * note that interface 0 can't be dependant, so dependance does not need
     * to be tested */
    if( psz_iface == NULL )
    {           
        return( p_method->i_ifaces ? p_method->p_iface[0].i_vlan : -1 );    
    }
        
    /* Browse all interfaces */
    for( i_index = 0; i_index < p_method->i_ifaces ; i_index++ )
    {
        /* If interface has been found, return vlan */
        if( !strcmp( p_method->p_iface[i_index].psz_name, psz_iface ) )
        {
            /* If interface is dependant, use master, else return own vlan */
            return( (p_method->p_iface[i_index].i_master >= 0) ?
                    p_method->p_iface[p_method->p_iface[i_index].i_master].i_vlan :
                    p_method->p_iface[i_index].i_vlan );
        }        
    }    

    /* If not found, return an error */
    return( -1 );    
}

/*******************************************************************************
 * input_VlanSynchronize: resynchronize with vlan server
 *******************************************************************************
 * Resynchronize with the vlan server. Vlans for all interfaces are requested
 * and changed if required. This call may take a lot of time, and is intended
 * for emergency situations.
 *******************************************************************************/
int input_VlanSynchronize( void )
{
    input_vlan_method_t *   p_method;                    /* method global data */
    input_vlan_iface_t *    p_iface;                   /* interface (shortcut) */
    int                     i_index;                        /* interface index */
    int                     i_vlan;              /* vlan for current interface */
    
    /* Get lock */
    p_method = p_main->p_input_vlan;
    pthread_mutex_lock( &p_method->lock );
/* ??
    p_method = &p_program_data->input_vlan_method;
    vlc_mutex_lock( &p_method->lock );
*/

    for( i_index = 0; i_index < p_method->i_ifaces; i_index++ )
    {        
        p_iface = &p_method->p_iface[ i_index ];
        
        /* Ignore dependant interfaces and interfaces for wich login failed */
        if( (p_iface->i_master < 0) && !ServerLogin( &p_method->server ) )
        {            
            /* Request interface informations */
            i_vlan = p_iface->i_vlan;
            if( !ServerRequestInfo( &p_method->server, p_iface ) )
            {
                /* If synchronization has been lost, then try to resynchronize -
                 * this part is ugly because of the vlan server bug requiring a 
                 * logout between two requests */
                if( p_iface->i_vlan != i_vlan )
                {
                    intf_Msg("input: lost vlan synchronization for interface %s\n", 
                             p_iface->psz_name );                    
                    ServerLogout( &p_method->server );
                    if( !ServerLogin( &p_method->server ) )
                    {
                        if( !ServerRequestChange( &p_method->server, p_iface, i_vlan ) )
                        {
                            intf_Msg("input: retrieved vlan synchronization for interface %s\n", 
                                     p_iface->psz_name );          
                        }                        
                    }
                    /* Note that when this login fails, then the next logout will
                     * also fail... but I don't want to spend time finding a 
                     * clean way to avoid this if the vlan server bug is fixed */
                }                                
            }            
            /* Logout */
            ServerLogout( &p_method->server );            
        }        
    }    

    /* Release lock */
    vlc_mutex_unlock( &p_method->lock );   
    return( 0 );    
}

/* following functions are local */

/*******************************************************************************
 * IfaceInfo: read info about an interface
 *******************************************************************************
 * This function reads informations about a network interface. It should return
 * 0 and updated interface informations for vlan capable interfaces, and non 0
 * if interface is not vlan-capable or informations request failed.
 *******************************************************************************/
static int IfaceInfo( input_vlan_iface_t *p_iface )
{
    int             i_socket;
    struct ifreq    device;

    /* Copy interface name */
    strcpy(device.ifr_name, p_iface->psz_name);

    /* Open a datagram socket */
    i_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(i_socket < 0)
    {
        intf_ErrMsg("input error: unable to open socket on %s: %s\n", 
                    p_iface->psz_name, strerror(errno));
        return( -1 );        
    }

    /* Read IP address */
    if(ioctl(i_socket, SIOCGIFDSTADDR, &device) < 0)
    {
        intf_ErrMsg("input error: can not read IP address for %s: %s\n", 
                    p_iface->psz_name, strerror(errno));
        return( -1 );
    }
    memcpy( &p_iface->sa_in, &device.ifr_hwaddr, sizeof(struct sockaddr_in));

    /* Read MAC address */
    if(ioctl(i_socket, SIOCGIFHWADDR, &device) < 0)
    {
        intf_ErrMsg("input error: can not read MAC address for %s: %s\n",
                    p_iface->psz_name, strerror(errno));
        return( -1 );
    }

    /* Translate MAC address to ASCII standard */
    sprintf(p_iface->psz_mac, "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
            device.ifr_hwaddr.sa_data[0]&0xff,
            device.ifr_hwaddr.sa_data[1]&0xff,
            device.ifr_hwaddr.sa_data[2]&0xff,
            device.ifr_hwaddr.sa_data[3]&0xff,
            device.ifr_hwaddr.sa_data[4]&0xff,
            device.ifr_hwaddr.sa_data[5]&0xff);

    return( 0 );
}

/*******************************************************************************
 * IfaceDependance: check interface dependance
 *******************************************************************************
 * Check if an interface designed by it's index is dependant from another one.
 * All the interfaces from 0 to i_index excluded are tested. If a 'master'  
 * interface is found, then the 'i_master' field is set to a positive value.
 * Non 0 is returned if the interface is dependant.
 *******************************************************************************/
static int IfaceDependance( input_vlan_method_t *p_method, int i_iface )
{
    int i_index;                                            /* interface index */
    
    for( i_index = 0; i_index < i_iface; i_index++ )
    {
        /* Two interface are dependant if they are on the same switch and
         * port */
        if( ( p_method->p_iface[i_index].i_switch == p_method->p_iface[i_iface].i_switch )
            && ( p_method->p_iface[i_index].i_port == p_method->p_iface[i_iface].i_port ) )
        {
            /* Interface is slave */
            p_method->p_iface[i_iface].i_master = i_index;
            return( 1 );            
        }        
    }

    /* Interface is master */
    p_method->p_iface[i_iface].i_master = -1;    
    return( 0 );    
}

/*******************************************************************************
 * ServerLogin: login to a vlan server
 *******************************************************************************
 * Initiate login sequence to a vlan server: open a socket, bind it and send
 * login sequence. If the login fails for any reason, non 0 is returned.
 *******************************************************************************/
static int ServerLogin( input_vlan_server_t *p_server )
{
    struct sockaddr_in  sa_client;                           /* client address */
    char                psz_msg[VLAN_SERVER_MSG_LENGTH  + 1];/* server message */
    int                 i_bytes;                       /* number of bytes read */    

    psz_msg[VLAN_SERVER_MSG_LENGTH] = '\0';       /* make sure the string ends */

    /* Initialize local socket */
    BuildInetAddr( &sa_client, NULL, 0 );
    p_server->i_socket = socket(AF_INET, SOCK_STREAM, 0);
    if( p_server->i_socket < 0 )
    {
        /* Error: return an error */
        intf_ErrMsg("input error: can not open socket (%s)\n", strerror(errno));
        return( errno );
    }
           
    /* Bind the server socket to client */
    if( bind( p_server->i_socket, (struct sockaddr *) &sa_client, sizeof(sa_client)) < 0)
    {
        /* Error: close socket and return an error */
        intf_ErrMsg("input error: can not bind socket (%s)\n", strerror(errno));        
        close( p_server->i_socket );
        return( errno );
    }

    /* Try to connect to the VLANserver */
    if( connect( p_server->i_socket, (struct sockaddr *) &p_server->sa_in, 
                 sizeof(p_server->sa_in)) < 0)
    {
        /* Error: close socket and return an error */
        intf_ErrMsg("input error: unable to connect to the VLAN server (%s)\n", 
                    strerror(errno));
        close( p_server->i_socket );
        return( errno );        
    }

    /* Send login message */
    snprintf(psz_msg, VLAN_SERVER_MSG_LENGTH, "%d %s %s %s\n", 
             VLAN_LOGIN_REQUEST, VLAN_CLIENT_VERSION, 
             p_server->psz_login, p_server->psz_passwd );
    if( send(p_server->i_socket, psz_msg, sizeof(char) * strlen( psz_msg ), 0) < 0)
    {
        intf_ErrMsg("input error: unable to login to the VLANserver: %s", 
                    strerror(errno));
        close( p_server->i_socket );
        return( errno );        
    }

    /* Listen to response */
    i_bytes = recv(p_server->i_socket, psz_msg, VLAN_SERVER_MSG_LENGTH, 0);
    if( i_bytes < 0 )
    {        
        intf_ErrMsg("input error: no response from VLANserver: %s\n",
                    strerror(errno));
        ServerLogout( p_server );
        return( -1 );
    }

    /* Parse answer to login request */
    psz_msg[ i_bytes ] = '\0';                         /* terminate string */    
    if( atoi(psz_msg) == VLAN_LOGIN_REJECTED )
    {
        intf_ErrMsg("input error: login rejected by VLANserver: %s\n", psz_msg);
        ServerLogout( p_server );
        return( -1 );        
    }
    else if( atoi(psz_msg) != VLAN_LOGIN_ANSWER )
    {
        intf_ErrMsg("input error: unexpected answer from VLAN server: %s\n", psz_msg);
        ServerLogout( p_server );
        return( -1 );        
    }
    
    intf_DbgMsg("input debug: VLANserver login ok.\n");    
    return 0;
}

/*******************************************************************************
 * ServerLogout: logout from a vlan server
 *******************************************************************************
 * Logout from a vlan server. This function sends the logout message to the
 * server and close the socket.
 *******************************************************************************/
static void ServerLogout( input_vlan_server_t *p_server )
{
    char    psz_msg[VLAN_SERVER_MSG_LENGTH  + 1];            /* server message */

    psz_msg[VLAN_SERVER_MSG_LENGTH] = '\0';       /* make sure the string ends */

    /* Send logout */
    snprintf(psz_msg, VLAN_SERVER_MSG_LENGTH, "%d\n", VLAN_LOGOUT);
    if( send(p_server->i_socket, psz_msg, sizeof(char) * strlen(psz_msg), 0) < 0)
    {
        intf_ErrMsg("input error: can't send logout message to VLANserver: %s\n", 
                    strerror(errno));
    }
  
    /* Close socket */
    if( close(p_server->i_socket) < 0)
    {
        intf_ErrMsg("input error: unable to close socket: %s\n", strerror(errno));
    }

    intf_DbgMsg("input debug: VLANserver logout ok\n");    
}

/*******************************************************************************
 * ServerRequestChange: request vlan change from a server
 *******************************************************************************
 * Request vlan change from a vlan server. The client must be logged in. If the
 * change succeeded, the interface structure is updated. Note that only masters
 * should be sent to this function.
 *******************************************************************************/
static int ServerRequestChange( input_vlan_server_t *p_server, 
                                input_vlan_iface_t *p_iface, int i_vlan )
{
    char    psz_msg[VLAN_SERVER_MSG_LENGTH  + 1];            /* server message */
    int     i_bytes;                                   /* number of bytes read */
          
    psz_msg[VLAN_SERVER_MSG_LENGTH] = '\0';       /* make sure the string ends */

    /* Send request */
    snprintf(psz_msg, VLAN_SERVER_MSG_LENGTH, "%d %s %s %d %d", 
             VLAN_CHANGE_REQUEST, p_iface->psz_mac, 
             inet_ntoa(p_iface->sa_in.sin_addr), i_vlan, p_iface->i_vlan);
    if( send( p_server->i_socket, psz_msg, sizeof(char) * strlen(psz_msg), 0) < 0)
    {
        intf_ErrMsg("input error: unable to send request to VLANserver: %s\n", 
                    strerror(errno));        
        return( -1 );
    }

    /* Listen to response */
    i_bytes = recv(p_server->i_socket, psz_msg, VLAN_SERVER_MSG_LENGTH, 0);
    if( i_bytes < 0 )
    {        
        intf_ErrMsg("input error: no response from VLANserver: %s",
                    strerror(errno));
        return( -1 );
    }

    /* Parse answer to vlan request */
    psz_msg[ i_bytes ] = '\0';                         /* terminate string */    
    if( atoi( psz_msg ) == VLAN_CHANGE_REJECTED )
    {
        intf_ErrMsg("input error: change request rejected by VLANserver: %s\n", psz_msg );
        return( -1 );      
    }
    else if( atoi( psz_msg ) != VLAN_CHANGE_ANSWER )
    {
        intf_ErrMsg("input error: unexpected answer from VLAN server: %s\n", psz_msg);
        return( -1 );                
    }

    /* ?? send packet for the switch to learn mac again */

    /* Update interface and return */
    intf_DbgMsg("input debug: interface %s moved to vlan %d\n", 
                p_iface->psz_name, i_vlan );   
    p_iface->i_vlan = i_vlan;    
    return( 0 ); 
}

/*******************************************************************************
 * ServerRequestInfo: ask current vlan to server
 *******************************************************************************
 * Request current vlan from a vlan server. The client must be logged in. This
 * function updates the p_iface structure or returns non 0. Note that only
 * masters should be sent to this function.
 *******************************************************************************/
static int ServerRequestInfo( input_vlan_server_t *p_server, 
                              input_vlan_iface_t *p_iface )
{
    char    psz_msg[VLAN_SERVER_MSG_LENGTH  + 1];            /* server message */
    int     i_bytes;                                   /* number of bytes read */
    int     i_switch;                                         /* switch number */
    int     i_port;                                             /* port number */
    int     i_vlan;                                             /* vlan number */
    int     i_sharers;                 /* number of mac addresses on this port */    
          
    psz_msg[VLAN_SERVER_MSG_LENGTH] = '\0';       /* make sure the string ends */

    /* Send request */
    snprintf(psz_msg, VLAN_SERVER_MSG_LENGTH, "%d", VLAN_INFO_REQUEST);
    if( send( p_server->i_socket, psz_msg, sizeof(char) * strlen(psz_msg), 0) < 0)
    {
        intf_ErrMsg("input error: unable to send request to VLANserver: %s\n", 
                    strerror(errno));        
        return( -1 );
    }

    /* Listen to response */
    i_bytes = recv(p_server->i_socket, psz_msg, VLAN_SERVER_MSG_LENGTH, 0);
    if( i_bytes < 0 )
    {        
        intf_ErrMsg("input error: no response from VLANserver: %s",
                    strerror(errno));
        return( -1 );
    }

    /* Parse answer to vlan request */
    psz_msg[ i_bytes ] = '\0';                         /* terminate string */    
    if( atoi( psz_msg ) == VLAN_INFO_REJECTED )
    {
        intf_ErrMsg("input error: info request rejected by VLANserver: %s\n", psz_msg );
        return( -1 );      
    }
    else if( atoi( psz_msg ) != VLAN_INFO_ANSWER )
    {
        intf_ErrMsg("input error: unexpected answer from VLAN server: %s\n", psz_msg);
        return( -1 );                
    }
    else if( sscanf(psz_msg, "%*d %d %d %d %d", &i_switch, &i_port, &i_vlan, &i_sharers) != 4 )
    {
        intf_ErrMsg("input error: invalid answer from VLAN server: %s\n", psz_msg);
        return( -1 );                        
    }

    /* Update interface and return */
    intf_DbgMsg("input debug: interface %s is on switch %d, port %d, vlan %d, %d sharers\n", 
                p_iface->psz_name, i_switch, i_port, i_vlan, i_sharers);    
    p_iface->i_switch = i_switch;    
    p_iface->i_port = i_port;    
    p_iface->i_vlan = i_vlan;    
    p_iface->i_sharers = i_sharers;    
    return( 0 );    
}




