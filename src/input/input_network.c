/*******************************************************************************
 * network.c: functions to read from the network 
 * (c)1999 VideoLAN
 *******************************************************************************
 * Manages a socket.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <sys/uio.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>      /* servent, getservbyname(), hostent, gethostbyname() */
#include <sys/socket.h>          /* socket(), setsockopt(), bind(), connect() */
#include <unistd.h>                                                /* close() */
#include <netinet/in.h>                      /* sockaddr_in, htons(), htonl() */
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
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

/******************************************************************************
 * Local prototypes
 ******************************************************************************/


/******************************************************************************
 * input_NetworkCreateMethod: initialize a network stream
 ******************************************************************************/
int input_NetworkCreateMethod( input_thread_t *p_input,
                               input_cfg_t *p_cfg )
{
    int                     i_socket_option, i_port, i_dummy;
    struct sockaddr_in      sa_in;
    char                    psz_hostname[INPUT_MAX_HOSTNAME_LENGTH];

    /* First and foremost, in the VLAN method, we join the desired VLAN. */
    if( p_input->i_method == INPUT_METHOD_TS_VLAN_BCAST )
    {
        /* Get a VLAN ID (VlanLib). */
        if( ( p_input->i_vlan_id = input_VlanId( NULL, p_cfg->i_vlan ) )
                == (-1) )
        {
            intf_ErrMsg("input error: VlanId() failed (%d)\n",
                        p_input->i_vlan_id);
            return( -1 );
        }
        /* Join the VLAN. */
        if( ( i_dummy = input_VlanJoin( p_input->i_vlan_id ) ) != 0 )
        {
            intf_ErrMsg("input error: VlanJoin() failed (%d)\n",
                        i_dummy);
            return( -1 );
        }
    }

    /* We open a SOCK_DGRAM (UDP) socket, in the AF_INET domain, automatic (0)
     * protocol */
    if( (p_input->i_handle = socket( AF_INET, SOCK_DGRAM, 0 )) == (-1) )
    {
        intf_ErrMsg("input error: socket() error (%s)\n",
                    strerror(errno));
        return( -1 );
    }

    intf_DbgMsg("input debug: socket %d opened (cfg: %p)\n", p_input->i_handle,
		p_cfg);

    /* we set up the options of our socket. It doesn't need to be non-blocking,
     * on the contrary (when this thread is waiting for data, it doesn't have
     * the lock, so decoders can work. */

    /* set SO_REUSEADDR option which allows to re-bind() a busy port */
    i_socket_option = 1;
    if( setsockopt( p_input->i_handle,
                    SOL_SOCKET,
                    SO_REUSEADDR,
                    &i_socket_option,
                    sizeof( i_socket_option ) ) == (-1) )
    {
        intf_ErrMsg("input error: setsockopt(SO_REUSEADDR) error (%s)\n",
                    strerror(errno));
        close( p_input->i_handle );
        return( -1 );
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
        intf_ErrMsg("input error: setsockopt(SO_RCVBUF) error (%s)\n",
                    strerror(errno));
        close( p_input->i_handle );
        return( -1 );
    }
    
    /* Now, we bind the socket. */

    /* Find the port. */
    if( p_cfg->i_properties & INPUT_CFG_PORT )
    {
        i_port = p_cfg->i_port;
        intf_DbgMsg("input debug: using port %d\n", i_port);
    }
    else
    {
#ifdef VIDEOLAN_DEFAULT_PORT
        /* default port */
        i_port = VIDEOLAN_DEFAULT_PORT;
        intf_DbgMsg("input debug: using default port (%d)\n", i_port);
#else
        intf_ErrMsg("input error: no default port\n");
        return( -1 );
#endif
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
            if( BuildInetAddr( &sa_in, INPUT_BCAST_ADDR, i_port ) == (-1) )
            {                                               /* see netutils.c */
                close( p_input->i_handle );
                return( -1 );
            }
#else
            /* We bind with any address. Security problem ! */
            if( BuildInetAddr( &sa_in, NULL, i_port ) == (-1) )
            {
                close( p_input->i_handle );
                return( -1 ),
            }
#endif
            break;

        case INPUT_METHOD_TS_UCAST:
            /* We bind with the local address. */
            if( gethostname( psz_hostname, sizeof( psz_hostname ) ) == (-1) )
            {
                intf_ErrMsg("input error: gethostname failed (%s)\n",
                            strerror(errno));
                close( p_input->i_handle );
                return( -1 );
            }
            if( BuildInetAddr( &sa_in, psz_hostname, i_port ) == (-1) )
            {
                close( p_input->i_handle );
                return( -1 );
            }
            break;
        case INPUT_METHOD_TS_MCAST:
            /* We bind with 239.0.0.1. */
            if( BuildInetAddr( &sa_in, "239.0.0.1", i_port ) == (-1) )
            {
                close( p_input->i_handle );
                return( -1 );
            }
            break;
    }

    /* Effectively bind the socket. */
    if( bind( p_input->i_handle,
              (struct sockaddr *) &sa_in,
              sizeof( sa_in ) ) < 0 )
    {
        intf_ErrMsg("input error: bind() failed (%s)\n",
                    strerror(errno));
        close( p_input->i_handle );
        return( -1 );
    }

    /* Connect the socket to the remote server. */

    /* Find which server we have to use. */
    if( p_cfg->i_properties & INPUT_CFG_HOSTNAME )
    {
        if( BuildInetAddr( &sa_in, p_cfg->psz_hostname, htons(0) ) == (-1) )
        {
            close( p_input->i_handle );
            return( -1 );
        }
    }
    else if( p_cfg->i_properties & INPUT_CFG_IP )
    {
        if( BuildInetAddr( &sa_in, p_cfg->psz_ip, htons(0) ) == (-1) )
        {
            close( p_input->i_handle );
            return( -1 );
        }
    }
    else
    {
#ifdef VIDEOLAN_DEFAULT_SERVER
        /* default server */
        if( BuildInetAddr( &sa_in, VIDEOLAN_DEFAULT_SERVER, htons(0) ) == (-1) )
        {
            close( p_input->i_handle );
            return( -1 );
        }
#else
        intf_ErrMsg("input error: no default videolan server\n");
        return( -1 );
#endif
    }

    /* Effectively connect the socket. */
    if( connect( p_input->i_handle,
                 (struct sockaddr *) &sa_in,
                 sizeof( sa_in ) ) == (-1) )
    {
        intf_ErrMsg("input error: connect() failed\n");
        close( p_input->i_handle );
        return( -1 );
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
 * input_NetworkDestroyMethod: close a network stream
 ******************************************************************************/
void input_NetworkDestroyMethod( input_thread_t *p_input )
{
    /* Close local socket. */
    if( p_input->i_handle )
    {
        if( close( p_input->i_handle) == (-1) )
        {
            intf_ErrMsg("input error: can't close network socket (%s)\n",
                        strerror(errno) );
        }
    }

    /* In case of VLAN method, leave the current VLAN. */
    if( p_input->i_method == INPUT_METHOD_TS_VLAN_BCAST )
    {
        input_VlanLeave( p_input->i_vlan_id );
    }
}
