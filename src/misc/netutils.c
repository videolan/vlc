/*****************************************************************************
 * netutils.c: various network functions
 * (c)1999 VideoLAN
 *****************************************************************************
 * ??
 *****************************************************************************
 * Required headers:
 * <netinet/in.h>
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "config.h"
#include "common.h"
#include "mtime.h"

#include "intf_msg.h"
#include "debug.h"

#include "netutils.h"

/*****************************************************************************
 * BuildInetAddr: build an Internet address descriptor
 *****************************************************************************
 * Build an internet socket descriptor from a host name, or an ip, and a port.
 * If the address is NULL, then INADDR_ANY will be used, allowing to receive
 * on any address for a local socket. Usually, in this case, 'port' is also null
 * and the function always succeeds.
 *****************************************************************************/
int BuildInetAddr( struct sockaddr_in *p_sa_in, char *psz_in_addr, int i_port )
{
    struct hostent *p_hostent;                            /* host descriptor */

    bzero( p_sa_in, sizeof( struct sockaddr_in ) );
    p_sa_in->sin_family = AF_INET;                                 /* family */
    p_sa_in->sin_port = htons( i_port );                             /* port */

    /* Use INADDR_ANY if psz_in_addr is NULL */
    if( psz_in_addr == NULL )
    {
        p_sa_in->sin_addr.s_addr = htonl(INADDR_ANY);
    }
    /* Try to convert address directly from in_addr - this will work if
     * psz_in_addr is dotted decimal. */
    else if( !inet_aton( psz_in_addr, &p_sa_in->sin_addr) )
    {
        /* The convertion failed: the address is an host name, which needs
         * to be resolved */
        intf_DbgMsg("debug: resolving internet address %s...\n", psz_in_addr);
        if ( (p_hostent = gethostbyname(psz_in_addr)) == NULL)
        {
            intf_ErrMsg("error: unknown host %s\n", psz_in_addr);
            return( -1 );
        }

        /* Copy the first address of the host in the socket address */
        bcopy( p_hostent->h_addr_list[0], &p_sa_in->sin_addr, p_hostent->h_length);
    }
    return( 0 );
}


/*****************************************************************************
 * ServerPort: extract port from a "server:port" adress
 *****************************************************************************
 * Returns the port number in a "server:port" address and replace the ":" by
 * a NUL character, or returns -1.
 *****************************************************************************/
int ServerPort( char *psz_addr )
{
    char *psz_index;

    /* Scan string for ':' */
    for( psz_index = psz_addr; *psz_index && (*psz_index != ':'); psz_index++ )
    {
        ;
    }

    /* If a port number has been found, convert it and return it */
    if( *psz_index == ':' )
    {
        *psz_index = '\0';
        return( atoi( psz_index + 1 ) );
    }

    return( - 1 );
}


/*****************************************************************************
 * ReadIfConf: Read the configuration of an interface
 *****************************************************************************
 * i_sockfd must reference a socket open as follow: AF_INET, DOCK_DGRAM, 0
 *****************************************************************************/
int ReadIfConf(int i_sockfd, if_descr_t* p_ifdescr, char* psz_name)
{
    struct ifreq ifr_config;
    int i_rc = 0;

    ASSERT(p_ifdescr);
    ASSERT(psz_name);

    /* Which interface are we interested in ? */
    strcpy(ifr_config.ifr_name, psz_name);

    /* Read the flags for that interface */
    i_rc = ioctl(i_sockfd, SIOCGIFFLAGS, (byte_t *)&ifr_config);
    if( !i_rc )
    {
        p_ifdescr->i_flags = ifr_config.ifr_flags;
        intf_DbgMsg("%s flags: 0x%x\n", psz_name, p_ifdescr->i_flags);
    }
    else
    {
        intf_ErrMsg("Cannot read flags for interface %s: %s\n", psz_name,
                    strerror(errno));
        return -1;
    }

   /* Read physical address of the interface and store it in our description */
    i_rc = ioctl(i_sockfd, SIOCGIFHWADDR, (byte_t *)&ifr_config);
    if( !i_rc )
    {
        memcpy(&p_ifdescr->sa_phys_addr, &ifr_config.ifr_addr, sizeof(struct sockaddr));
        intf_DbgMsg("%s MAC address: %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n", psz_name,
                    p_ifdescr->sa_phys_addr.sa_data[0]&0xff,
                    p_ifdescr->sa_phys_addr.sa_data[1]&0xff,
                    p_ifdescr->sa_phys_addr.sa_data[2]&0xff,
                    p_ifdescr->sa_phys_addr.sa_data[3]&0xff,
                    p_ifdescr->sa_phys_addr.sa_data[4]&0xff,
                    p_ifdescr->sa_phys_addr.sa_data[5]&0xff);
    }
    else
    {
        intf_ErrMsg("Cannot read hardware address for interface %s: %s\n",
                    psz_name, strerror(errno));
        return -1;
    }

    /* Read IP address of the interface and store it in our description */
    i_rc = ioctl(i_sockfd, SIOCGIFADDR, (byte_t *)&ifr_config);
    if( !i_rc )
    {
        memcpy(&p_ifdescr->sa_net_addr, &ifr_config.ifr_addr, sizeof(struct sockaddr));
        intf_DbgMsg("%s IP address: %s\n", psz_name,
                    inet_ntoa(p_ifdescr->sa_net_addr.sin_addr));
    }
    else
    {
        intf_ErrMsg("Cannot read network address for interface %s: %s\n",
                    psz_name, strerror(errno));
        return -1;
    }

  /* Read broadcast address of the interface and store it in our description */
    if(p_ifdescr->i_flags & IFF_POINTOPOINT)
    {
        intf_DbgMsg("%s doen't not support broadcast\n", psz_name);
        i_rc = ioctl(i_sockfd, SIOCGIFDSTADDR, (byte_t *)&ifr_config);
    }
    else
    {
        intf_DbgMsg("%s supports broadcast\n", psz_name);
        i_rc = ioctl(i_sockfd, SIOCGIFBRDADDR, (byte_t *)&ifr_config);
    }
    if( !i_rc )
    {
        memcpy(&p_ifdescr->sa_bcast_addr, &ifr_config.ifr_addr, sizeof(struct sockaddr));
        intf_DbgMsg("%s broadcast address: %s\n", psz_name,
                    inet_ntoa(p_ifdescr->sa_bcast_addr.sin_addr));
    }
    else
    {
        intf_ErrMsg("Cannot read broadcast address for interface %s: %s\n",
                    psz_name, strerror(errno));
        return -1;
    }

    return i_rc;
}



/*****************************************************************************
 * ReadNetConf: Retrieve the network configuration of the host
 *****************************************************************************
 * Only IP interfaces are listed, and only if they are up
 * i_sockfd must reference a socket open as follow: AF_INET, DOCK_DGRAM, 0
 *****************************************************************************/
int ReadNetConf(int i_sockfd, net_descr_t* p_net_descr)
{
    struct ifreq* a_ifr_ifconf = NULL;
    struct ifreq* p_ifr_current_if;
    struct ifconf ifc_netconf;

    int i_if_number;
    int i_remaining;
    int i_rc = 0;

    ASSERT(p_net_descr);

    /* Start by assuming we have few than 3 interfaces (i_if_number will
       be incremented by 1 when entering the loop) */
    i_if_number = 2;

    /* Retrieve network configuration for that host */
    do
    {
        i_if_number++;
        a_ifr_ifconf = realloc(a_ifr_ifconf, i_if_number*sizeof(struct ifreq));
        ifc_netconf.ifc_len = i_if_number*sizeof(struct ifreq);
        ifc_netconf.ifc_req = a_ifr_ifconf;

        i_rc = ioctl(i_sockfd, SIOCGIFCONF, (byte_t*)&ifc_netconf);
        if( i_rc )
        {
            intf_ErrMsg("Cannot read network configuration: %s\n",
                        strerror(errno));
            break;
        }
    }
    /* If we detected ifc_len interfaces, this may mean that others have
       been missed because the a_ifr_ifconf was to little, so increase
       it's size and retry */
    while( ifc_netconf.ifc_len >= i_if_number*sizeof(struct ifreq) );

    /* No see what we detected */
    if( !i_rc )
    {
        /* Init the given net_descr_t struct */
        p_net_descr->i_if_number = 0;
        p_net_descr->a_if = NULL;

        /* Iterate through the entries of the a_ifr_ifconf table */
        p_ifr_current_if = ifc_netconf.ifc_req;
        for( i_remaining = ifc_netconf.ifc_len / sizeof (struct ifreq);
             i_remaining-- > 0; p_ifr_current_if++ )
        {
            intf_DbgMsg("Found interface %s\n", p_ifr_current_if->ifr_name);

            /* Don't use an interface devoted to an address family other than IP */
            if(p_ifr_current_if->ifr_addr.sa_family != AF_INET)
                continue;

            /* Read the status of this interface */
            if( ioctl(i_sockfd, SIOCGIFFLAGS, (byte_t *)p_ifr_current_if) < 0 )
            {
                intf_ErrMsg("Cannot access interface %s: %s\n",
                            p_ifr_current_if->ifr_name, strerror(errno));
                i_rc = -1;
                break;
            }
            else
            {
                /* Skip this interface if it is not up or if this is a loopback one */
                if( !p_ifr_current_if->ifr_flags & IFF_UP ||
                    p_ifr_current_if->ifr_flags & IFF_LOOPBACK )
                  continue;

                /* Add an entry to the net_descr struct to store the description of
                   that interface */
                p_net_descr->i_if_number++;
                p_net_descr->a_if = realloc(p_net_descr->a_if,
                                            p_net_descr->i_if_number*sizeof(if_descr_t));
                /* Read the info ??? */
                i_rc = ReadIfConf(i_sockfd, &p_net_descr->a_if[p_net_descr->i_if_number-1],
                                  p_ifr_current_if->ifr_name);
            }
        }
    }

    /* Don't need the a_ifr_ifconf anymore */
    free( a_ifr_ifconf );
    return i_rc;
}


