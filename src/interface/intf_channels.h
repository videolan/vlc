/*****************************************************************************
 * intf_channels.h: Channel handling functions
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: intf_channels.h,v 1.2 2001/03/21 13:42:34 sam Exp $
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
 * intf_channel_t: channel description
 *****************************************************************************
 * A 'channel' is a descriptor of an input method. It is used to switch easily
 * from source to source without having to specify the whole input thread
 * configuration. The channels array, stored in the interface thread object, is
 * loaded in intf_Create, and unloaded in intf_Destroy.
 *****************************************************************************/
typedef struct intf_channel_s
{
    /* Channel description */
    int         i_channel;            /* channel number, -1 for end of array */
    char *      psz_description;              /* channel description (owned) */

    /* Input configuration */
    int         i_input_method;                   /* input method descriptor */
    char *      psz_input_source;                   /* source string (owned) */
    int         i_input_port;                                        /* port */
    int         i_input_vlan_id;                                  /* vlan id */
} intf_channel_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int     intf_LoadChannels   ( struct intf_thread_s *, char * );
void    intf_UnloadChannels ( struct intf_thread_s * );
int     intf_SelectChannel  ( struct intf_thread_s *, int );

