/*****************************************************************************
 * interface.h: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: interface.h,v 1.30 2002/06/01 18:04:48 sam Exp $
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
 * intf_thread_t: describe an interface thread
 *****************************************************************************
 * This structe describes all interface-specific data of the main (interface)
 * thread.
 *****************************************************************************/
struct intf_thread_s
{
    VLC_COMMON_MEMBERS

    /* Thread properties and locks */
    vlc_bool_t          b_block;

    /* Specific interfaces */
    intf_console_t *    p_console;                                /* console */
    intf_sys_t *        p_sys;                           /* system interface */
    
    /* Plugin used and shortcuts to access its capabilities */
    module_t *   p_module;
    int       ( *pf_open )   ( intf_thread_t * );
    void      ( *pf_close )  ( intf_thread_t * );
    void      ( *pf_run )    ( intf_thread_t * );

    /* XXX: new message passing stuff will go here */
    vlc_mutex_t  change_lock;
    vlc_bool_t   b_menu_change;
    vlc_bool_t   b_menu;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define intf_Create(a) __intf_Create(CAST_TO_VLC_OBJECT(a))
intf_thread_t * __intf_Create     ( vlc_object_t * );
vlc_error_t       intf_RunThread  ( intf_thread_t * );
void              intf_StopThread ( intf_thread_t * );
void              intf_Destroy    ( intf_thread_t * );

