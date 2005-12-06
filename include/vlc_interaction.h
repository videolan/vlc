/*****************************************************************************
 * vlc_interaction.h: structures and function for user interaction
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: vlc_interaction.h 7954 2004-06-07 22:19:12Z fenrir $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

/**
 * This structure describes an interaction widget
 */
struct user_widget_t
{
    int             i_type;             //< Type identifier;
    char           *psz_text;           //< Free text

    vlc_value_t     val;
};

/**
 * Possible widget types
 */
enum
{
    WIDGET_TEXT,                        //< Text display
    WIDGET_PROGRESS,                    //< A progress bar
    WIDGET_INPUT                       //< Input (backed up by a variable)
};

/**
 * This structure describes a piece of interaction with the user
 */
struct interaction_dialog_t
{
    int             i_id;               //< Unique ID
    int             i_type;             //< Type identifier
    char           *psz_title;          //< Title
    char           *psz_description;    //< Descriptor string

    /* For dialogs */
    int             i_widgets;          //< Nu,ber of dialog widgets
    user_widget_t **pp_widgets;         //< Dialog widgets

    vlc_bool_t      b_have_answer;      //< Has an answer been given ?
    vlc_bool_t      b_reusable;         //< Do we have to reuse this ?
    vlc_bool_t      b_updated;          //< Update for this one ?
    vlc_bool_t      b_finished;         //< Hidden by interface

    void *          p_private;          //< Private interface data
};

/**
 * Possible interaction types
 */
enum
{
    INTERACT_PROGRESS,          //< Progress bar
    INTERACT_WARNING,           //< Warning message ("codec not supported")
    INTERACT_FATAL,             //< Fatal message ("File not found")
    INTERACT_FATAL_LIST,        //< List of fatal messages ("File not found")
    INTERACT_ASK,               //< Full-featured dialog box (password)
};

/**
 * Predefined reusable dialogs
 */
enum
{
    DIALOG_NOACCESS,
    DIALOG_NOCODEC,
    DIALOG_NOAUDIO,

    DIALOG_LAST_PREDEFINED,
};

/**
 * This structure contains the active interaction dialogs, and is
 * used by teh manager
 */
struct interaction_t
{
    VLC_COMMON_MEMBERS

    int                         i_dialogs;      //< Number of dialogs
    interaction_dialog_t      **pp_dialogs;     //< Dialogs

    intf_thread_t              *p_intf;         //< Interface to use

    int                         i_last_id;      //< Last attributed ID
};
/**
 * Possible actions
 */
enum
{
    INTERACT_NEW,
    INTERACT_UPDATE,
    INTERACT_HIDE
};

/***************************************************************************
 * Exported symbols
 ***************************************************************************/

#define intf_Interact( a,b ) __intf_Interact( VLC_OBJECT(a), b )
VLC_EXPORT( int,__intf_Interact,( vlc_object_t *,interaction_dialog_t * ) );

#define intf_UserFatal( a,b, c, d, e... ) __intf_UserFatal( a,b,c,d, ## e )
VLC_EXPORT( void, __intf_UserFatal,( vlc_object_t*, int, const char*, const char*, ...) );

VLC_EXPORT( void, intf_InteractionManage,( playlist_t *) );
VLC_EXPORT( void, intf_InteractionDestroy,( interaction_t *) );
