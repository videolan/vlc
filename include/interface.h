/*****************************************************************************
 * interface.h: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: interface.h,v 1.27 2002/02/19 00:50:18 sam Exp $
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

    /* XXX: new message passing stuff will go here */
    vlc_mutex_t         change_lock;
    boolean_t           b_menu_change;
    boolean_t           b_menu;
    
} intf_thread_t;

/*****************************************************************************
 * msg_item_t
 *****************************************************************************
 * Store a single message. Messages have a maximal size of INTF_MSG_MSGSIZE.
 *****************************************************************************/
typedef struct
{
    int     i_type;                               /* message type, see below */
    char *  psz_msg;                                   /* the message itself */

#if 0
    mtime_t date;                                     /* date of the message */
    char *  psz_file;               /* file in which the function was called */
    char *  psz_function;     /* function from which the function was called */
    int     i_line;                 /* line at which the function was called */
#endif
} msg_item_t;

/* Message types */
#define INTF_MSG_STD    0                                /* standard message */
#define INTF_MSG_ERR    1                                   /* error message */
#define INTF_MSG_WARN   2                                 /* warning message */
#define INTF_MSG_STAT   3                               /* statistic message */

/*****************************************************************************
 * intf_subscription_t
 *****************************************************************************
 * Used by interface plugins which subscribe to the message queue.
 *****************************************************************************/
typedef struct intf_subscription_s
{
    int   i_start;
    int*  pi_stop;

    msg_item_t*  p_msg;
    vlc_mutex_t* p_lock;
} intf_subscription_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
intf_thread_t * intf_Create       ( void );
void            intf_Destroy      ( intf_thread_t * p_intf );

void            intf_MsgCreate    ( void );
void            intf_MsgDestroy   ( void );

#ifndef PLUGIN
intf_subscription_t* intf_MsgSub    ( void );
void                 intf_MsgUnsub  ( intf_subscription_t * );
#else
#   define intf_MsgSub p_symbols->intf_MsgSub
#   define intf_MsgUnsub p_symbols->intf_MsgUnsub
#endif

