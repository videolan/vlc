/*******************************************************************************
 * network.c: functions to read from the network 
 * (c)1999 VideoLAN
 *******************************************************************************
 * Manages a socket.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <sys/types.h>
#include <sys/uio.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>      /* servent, getservbyname(), hostent, gethostbyname() */
#include <sys/socket.h>          /* socket(), setsockopt(), bind(), connect() */
#include <unistd.h>                                                /* close() */
#include <netinet/in.h>                      /* sockaddr_in, htons(), htonl() */
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "netutils.h"

#include "input.h"
#include "input_network.h"
#include "input_vlan.h"

#include "intf_msg.h"
#include "main.h"

/******************************************************************************
 * input_NetworkOpen: initialize a network stream
 ******************************************************************************/
int input_NetworkOpen( input_thread_t *p_input )
{
    int                     i_socket_option;
    struct sockaddr_in      sa_in;
    char                    psz_hostname[INPUT_MAX_SOURCE_LENGTH];

    /* First and foremost, in the VLAN method, join the desired VLAN. */
    if( p_input->i_method == INPUT_METHOD_TS_VLAN_BCAST )
    {
        if( input_VlanJoin( p_input->i_vlan ) )
        {
            intf_ErrMsg("error: can't join vlan %d\n", p_input->i_vlan);            
            return( 1 );            
        }        
    }

    /* Open a SOCK_DGRAM (UDP) socket, in the AF_INET domain, automatic (0)
     * protocol */
    if( (p_input->i_handle = socket( AF_INET, SOCK_DGRAM, 0 )) == (-1) )
    {
        intf_ErrMsg("error: can't create socket (%s)\n", strerror(errno));
        return( 1 );
    }

    /* 
     * Set up the options of the socket 
     */

    /* Set SO_REUSEADDR option which allows to re-bind() a busy port */
    i_socket_option = 1;
    if( setsockopt( p_input->i_handle,
                    SOL_SOCKET,
                    SO_REUSEADDR,
                    &i_socket_option,
                    sizeof( i_socket_option ) ) == (-1) )
    {
        intf_ErrMsg("error: can't configure socket (%s)\n", strerror(errno));
        close( p_input->i_handle );
        return( 1 );
    }

    /* Increase the receive buffer size to 1/2MB (8Mb/s during 1/2s) to avoid
     * packet loss caused by scheduling problems */
    i_socket_option = 524288;
    if( setsockopt( p_input->i_handle,
                    SOL_SOCKET,
                    SO_RCVBUF,
                    &i_socket_option,
                    sizeof( i_socket_option ) ) == (-1) )
    {
        intf_ErrMsg("error: can't configure socket (%s)\n", strerror(errno));
        close( p_input->i_handle );
        return( 1 );
    }
    
    /* 
     * Bind the socket
     */

    /* Use default port if not specified */
    if( p_input->i_port == 0 )
    {
        p_input->i_port = main_GetIntVariable( INPUT_PORT_VAR, INPUT_PORT_DEFAULT );        
    }

    /* Find the address. */
    switch( p_input->i_method )
    {
    case INPUT_METHOD_TS_BCAST:
    case INPUT_METHOD_TS_VLAN_BCAST:
        /* In that case, we have to bind with the broadcast address.
         * broadcast addresses are very hard to find and depends on
         * implementation, so we thought using a #define would be much
         * simpler. */
#ifdef INPUT_BCAST_ADDR
        if( BuildInetAddr( &sa_in, INPUT_BCAST_ADDR, p_input->i_port ) == (-1) )
        {
            close( p_input->i_handle );
            return( 1 );
        }
#else
        /* We bind with any address. Security problem ! */
        if( BuildInetAddr( &sa_in, NULL, p_input->i_port ) == (-1) )
        {
            close( p_input->i_handle );
            return( -1 );            
        }
#endif
        break;

    case INPUT_METHOD_TS_UCAST:
        /* Unicast: bind with the local address. */
        if( gethostname( psz_hostname, sizeof( psz_hostname ) ) == (-1) )
        {
            intf_ErrMsg("error: can't get hostname (%s)\n", strerror(errno));
            close( p_input->i_handle );
            return( 1 );
        }
        if( BuildInetAddr( &sa_in, psz_hostname, p_input->i_port ) == (-1) )
        {
            close( p_input->i_handle );
            return( 1 );
        }
        break;

    case INPUT_METHOD_TS_MCAST:
        /* Multicast: bind with 239.0.0.1. */
        if( BuildInetAddr( &sa_in, "239.0.0.1", p_input->i_port ) == (-1) )
        {
            close( p_input->i_handle );
            return( 1 );
        }
        break;
    }

    /* Effectively bind the socket. */
    if( bind( p_input->i_handle, (struct sockaddr *) &sa_in, sizeof( sa_in ) ) < 0 )
    {
        intf_ErrMsg("error: can't bind socket (%s)\n", strerror(errno));
        close( p_input->i_handle );
        return( 1 );
    }

    /* 
     * Connect the socket to the remote server
     */

    /* Use default host if not specified */
    if( p_input->psz_source == NULL )
    {
        p_input->psz_source = main_GetPszVariable( INPUT_SERVER_VAR, INPUT_SERVER_DEFAULT );
    }
    
    if( BuildInetAddr( &sa_in, p_input->psz_source, htons(0) ) == (-1) )
    {
        close( p_input->i_handle );
        return( -1 );
    }

    /* Connect the socket. */
    if( connect( p_input->i_handle, (struct sockaddr *) &sa_in,  
                 sizeof( sa_in ) ) == (-1) )
    {
        intf_ErrMsg("error: can't connect socket\n" );
        close( p_input->i_handle );
        return( 1 );
    }
    return( 0 );
}

/******************************************************************************
 * input_NetworkRead: read a stream from the network
 ******************************************************************************
 * Wait for data during up to 1 second and then abort if none is arrived. The
 * number of bytes read is returned or -1 if an error occurs (so 0 is returned
 * after a timeout)
 * We don't have to make any test on presentation times, since we suppose
 * the network server sends us data when we need it.
 ******************************************************************************/
int input_NetworkRead( input_thread_t *p_input, const struct iovec *p_vector,
                       size_t i_count )
{
    fd_set rfds;
    struct timeval tv;
    int i_rc;

    /* Watch the given fd to see when it has input */
    FD_ZERO(&rfds);
    FD_SET(p_input->i_handle, &rfds);
  
    /* Wait up to 1 second */
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    i_rc = select(p_input->i_handle+1, &rfds, NULL, NULL, &tv);

    if( i_rc > 0 )
    {
        /* Data were received before timeout */
        i_rc = readv( p_input->i_handle, p_vector, i_count );
    }

    return( i_rc );
}

/******************************************************************************
 * input_NetworkClose: close a network stream
 ******************************************************************************/
void input_NetworkClose( input_thread_t *p_input )
{
    /* Close local socket. */
    if( p_input->i_handle )
    {
        close( p_input->i_handle );
    }

    /* Leave vlan if required */
    if( p_input->i_method == INPUT_METHOD_TS_VLAN_BCAST )
    {        
        input_VlanLeave( p_input->i_vlan );
    }    
}

