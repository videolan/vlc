/*******************************************************************************
 * netutils.h: various network functions
 * (c)1999 VideoLAN
 *******************************************************************************
 * This header describe miscellanous utility functions shared between several
 * modules.
 *******************************************************************************
 * Required headers:
 *  <netinet/in.h>
 *******************************************************************************/


/*******************************************************************************
 * if_descr_t: describes a network interface.
 *******************************************************************************
 * Note that if the interface is a point to point one, the broadcast address is
 * set to the destination address of that interface
 *******************************************************************************/
typedef struct
{
    /* Interface device name (e.g. "eth0") */
    char* psz_ifname;
    /* Interface physical address */
    struct sockaddr sa_phys_addr;  
    /* Interface network address */
    struct sockaddr_in sa_net_addr;
    /* Interface broadcast address */
    struct sockaddr_in sa_bcast_addr;
    /* Interface flags: see if.h for their description) */
    u16 i_flags;
} if_descr_t;


/*******************************************************************************
 * net_descr_t: describes all the interfaces of the computer
 *******************************************************************************
 * Nothing special to say :)
 *******************************************************************************/
typedef struct
{
    /* Number of networks interfaces described below */
    int i_if_number;
    /* Table of if_descr_t describing each interface */
    if_descr_t* a_if;
} net_descr_t;


/*******************************************************************************
 * Prototypes
 *******************************************************************************/
int ReadIfConf      (int i_sockfd, if_descr_t* p_ifdescr, char* psz_name);
int ReadNetConf     (int i_sockfd, net_descr_t* p_net_descr);
int BuildInetAddr   ( struct sockaddr_in *p_sa_in, char *psz_in_addr, int i_port );
int ServerPort      ( char *psz_addr );

