/*****************************************************************************
 * netutils.h: various network functions
 * This header describes miscellanous utility functions shared between several
 * modules.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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
 * Required headers:
 *  <netinet/in.h>
 *  <sys/socket.h>
 *****************************************************************************/


/*****************************************************************************
 * if_descr_t: describes a network interface.
 *****************************************************************************
 * Note that if the interface is a point to point one, the broadcast address is
 * set to the destination address of that interface
 *****************************************************************************/
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


/*****************************************************************************
 * net_descr_t: describes all the interfaces of the computer
 *****************************************************************************
 * Nothing special to say :)
 *****************************************************************************/
typedef struct
{
    /* Number of networks interfaces described below */
    int i_if_number;
    /* Table of if_descr_t describing each interface */
    if_descr_t* a_if;
} net_descr_t;


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int ReadIfConf      (int i_sockfd, if_descr_t* p_ifdescr, char* psz_name);
int ReadNetConf     (int i_sockfd, net_descr_t* p_net_descr);
int BuildInetAddr   ( struct sockaddr_in *p_sa_in, char *psz_in_addr, int i_port );
int ServerPort      ( char *psz_addr );

