/*****************************************************************************
 * vlc_interface.h: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vlc_interface.h,v 1.2 2003/07/17 17:30:40 gbazin Exp $
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

    /* Specific for dialogs providers */
    void      ( *pf_show_dialog ) ( intf_thread_t *, int, int );

    /* XXX: new message passing stuff will go here */
    vlc_mutex_t  change_lock;
    vlc_bool_t   b_menu_change;
    vlc_bool_t   b_menu;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define intf_Create(a,b) __intf_Create(VLC_OBJECT(a),b)
VLC_EXPORT( intf_thread_t *, __intf_Create,     ( vlc_object_t *, const char * ) );
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
                             "\"vlc -I wxwin\"\n") )
#else
#    define CONSOLE_INTRO_MSG
#endif

/* Interface dialog ids for dialog providers */
#define INTF_DIALOG_FILE_SIMPLE 1
#define INTF_DIALOG_FILE        2
#define INTF_DIALOG_DISC        3
#define INTF_DIALOG_NET         4
#define INTF_DIALOG_SAT         5

#define INTF_DIALOG_PLAYLIST   10
#define INTF_DIALOG_MESSAGES   11
#define INTF_DIALOG_FILEINFO   12
#define INTF_DIALOG_PREFS      13
