/*****************************************************************************
 * interface.h: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: interface.h,v 1.25 2001/12/30 07:09:54 sam Exp $
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
typedef struct intf_thread_s
{
    boolean_t           b_die;                                 /* `die' flag */

    /* Specific interfaces */
    p_intf_console_t    p_console;                                /* console */
    p_intf_sys_t        p_sys;                           /* system interface */
    
    /* Plugin used and shortcuts to access its capabilities */
    struct module_s *   p_module;
    int              ( *pf_open )   ( struct intf_thread_s * );
    void             ( *pf_close )  ( struct intf_thread_s * );
    void             ( *pf_run )    ( struct intf_thread_s * );

    /* Interface callback */
    void             ( *pf_manage ) ( struct intf_thread_s * );

    /* Input thread - NULL if not active */
    p_input_thread_t    p_input;

    /* XXX: new message passing stuff will go here */
    vlc_mutex_t         change_lock;
    boolean_t           b_menu_change;
    boolean_t           b_menu;
    
} intf_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
intf_thread_t * intf_Create             ( void );
void            intf_Destroy            ( intf_thread_t * p_intf );

