/*****************************************************************************
 * interface.h: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: interface.h,v 1.37 2002/11/11 14:39:11 sam Exp $
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
 * This struct describes all interface-specific data of the main (interface)
 * thread.
 *****************************************************************************/
struct intf_thread_t
{
    VLC_COMMON_MEMBERS

    /* Thread properties and locks */
    vlc_bool_t          b_block;

    /* Specific interfaces */
    intf_console_t *    p_console;                                /* console */
    intf_sys_t *        p_sys;                           /* system interface */
    
    /* Interface module */
    module_t *   p_module;
    void      ( *pf_run )    ( intf_thread_t * );

    /* XXX: new message passing stuff will go here */
    vlc_mutex_t  change_lock;
    vlc_bool_t   b_menu_change;
    vlc_bool_t   b_menu;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define intf_Create(a) __intf_Create(VLC_OBJECT(a))
VLC_EXPORT( intf_thread_t *, __intf_Create,     ( vlc_object_t * ) );
VLC_EXPORT( int,               intf_RunThread,  ( intf_thread_t * ) );
VLC_EXPORT( void,              intf_StopThread, ( intf_thread_t * ) );
VLC_EXPORT( void,              intf_Destroy,    ( intf_thread_t * ) );

/*****************************************************************************
 * Macros
 *****************************************************************************/
#if defined( WIN32 ) && !defined( UNDER_CE )
#    define CONSOLE_INTRO_MSG \
         AllocConsole(); \
         freopen( "CONOUT$", "w", stdout ); \
         freopen( "CONOUT$", "w", stderr ); \
         freopen( "CONIN$", "r", stdin ); \
         msg_Info( p_intf, COPYRIGHT_MESSAGE ); \
         msg_Info( p_intf, _("\nWarning: if you can't access the GUI " \
                             "anymore, open a dos command box, go to the " \
                             "directory where you installed VLC and run " \
                             "\"vlc -I win32\"\n") )
#else
#    define CONSOLE_INTRO_MSG
#endif
