/*****************************************************************************
 * vlc_interaction.h: structures and function for user interaction
 *****************************************************************************
 * Copyright (C) 2005-2006 VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Felix Kühne <fkuehne@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * This structure describes an interaction widget
 * WIDGETS ARE OUTDATED! THIS IS ONLY A STUB TO KEEP WX COMPILING!
 */
struct user_widget_t
{
    int             i_type;             ///< Type identifier;
    char           *psz_text;           ///< Free text

    vlc_value_t     val;
};

/**
 * Possible widget types
 * WIDGETS ARE OUTDATED! THIS IS ONLY A STUB TO KEEP WX COMPILING!
 */
enum
{
    WIDGET_TEXT,                        ///< Text display
    WIDGET_PROGRESS,                    ///< A progress bar
    WIDGET_INPUT_TEXT                   ///< Input (backed up by a variable)
};

/**
 * This structure describes a piece of interaction with the user
 */
struct interaction_dialog_t
{
    int             i_id;               ///< Unique ID
    int             i_type;             ///< Type identifier
    char           *psz_title;          ///< Title
    char           *psz_description;    ///< Descriptor string
    char           *psz_default_button;  ///< default button title (~OK)
    char           *psz_alternate_button;///< alternate button title (~NO)
    /// other button title (optional,~Cancel)
    char           *psz_other_button;

    char           *psz_returned[1];    ///< returned responses from the user

    vlc_value_t     val;                ///< a value coming from core for dialogue
    int             i_timeToGo;         ///< time (in sec) until shown progress is finished
    vlc_bool_t      b_cancelled;        ///< was the dialogue cancelled by the user?

    int             i_widgets;          ///< Number of dialog widgets
    user_widget_t **pp_widgets;         ///< Dialog widgets

    void *          p_private;          ///< Private interface data

    int             i_status;           ///< Dialog status;
    int             i_action;           ///< Action to perform;
    int             i_flags;            ///< Misc flags
    int             i_return;           ///< Return status

    interaction_t  *p_interaction;      ///< Parent interaction object
    vlc_object_t   *p_parent;           ///< The vlc object that asked
                                        //for interaction
};

/**
 * Possible flags . Dialog types
 */
#define DIALOG_GOT_ANSWER           0x01
#define DIALOG_YES_NO_CANCEL        0x02
#define DIALOG_LOGIN_PW_OK_CANCEL   0x04
#define DIALOG_PSZ_INPUT_OK_CANCEL  0x08
#define DIALOG_BLOCKING_ERROR       0x10
#define DIALOG_NONBLOCKING_ERROR    0x20
#define DIALOG_WARNING              0x40
#define DIALOG_USER_PROGRESS        0x80
#define DIALOG_INTF_PROGRESS        0x100

/**
 * Possible return codes
 */
enum
{
    DIALOG_DEFAULT,
    DIALOG_OK_YES,
    DIALOG_NO,
    DIALOG_CANCELLED
};

/**
 * Possible status
 */
enum
{
    NEW_DIALOG,                 ///< Just created
    SENT_DIALOG,                ///< Sent to interface
    UPDATED_DIALOG,             ///< Update to send
    ANSWERED_DIALOG,            ///< Got "answer"
    HIDING_DIALOG,              ///< Hiding requested
    HIDDEN_DIALOG,              ///< Now hidden. Requesting destruction
    DESTROYED_DIALOG,           ///< Interface has destroyed it
};

/**
 * Possible interaction types
 */
enum
{
    INTERACT_DIALOG_ONEWAY,     ///< Dialog box without feedback
    INTERACT_DIALOG_TWOWAY,     ///< Dialog box with feedback
};

/**
 * Predefined reusable dialogs
 */
enum
{
    DIALOG_FIRST,

    DIALOG_LAST_PREDEFINED,
};

/**
 * This structure contains the active interaction dialogs, and is
 * used by the manager
 */
struct interaction_t
{
    VLC_COMMON_MEMBERS

    int                         i_dialogs;      ///< Number of dialogs
    interaction_dialog_t      **pp_dialogs;     ///< Dialogs

    intf_thread_t              *p_intf;         ///< Interface to use

    int                         i_last_id;      ///< Last attributed ID
};
/**
 * Possible actions
 */
enum
{
    INTERACT_NEW,
    INTERACT_UPDATE,
    INTERACT_HIDE,
    INTERACT_DESTROY
};

/***************************************************************************
 * Exported symbols
 ***************************************************************************/

#define intf_UserFatal( a, b, c, d, e... ) __intf_UserFatal( VLC_OBJECT(a),b,c,d, ## e )
VLC_EXPORT( int, __intf_UserFatal,( vlc_object_t*, vlc_bool_t, const char*, const char*, ...) );
#define intf_UserWarn( a, c, d, e... ) __intf_UserWarn( VLC_OBJECT(a),c,d, ## e )
VLC_EXPORT( int, __intf_UserWarn,( vlc_object_t*, const char*, const char*, ...) );
#define intf_UserLoginPassword( a, b, c, d, e... ) __intf_UserLoginPassword( VLC_OBJECT(a),b,c,d,e)
VLC_EXPORT( int, __intf_UserLoginPassword,( vlc_object_t*, const char*, const char*, char **, char **) );
#define intf_UserYesNo( a, b, c, d, e, f ) __intf_UserYesNo( VLC_OBJECT(a),b,c, d, e, f )
VLC_EXPORT( int, __intf_UserYesNo,( vlc_object_t*, const char*, const char*, const char*, const char*, const char*) );
#define intf_UserStringInput( a, b, c, d ) __intf_UserStringInput( VLC_OBJECT(a),b,c,d )
VLC_EXPORT( int, __intf_UserStringInput,(vlc_object_t*, const char*, const char*, char **) );

#define intf_IntfProgress( a, b, c ) __intf_Progress( VLC_OBJECT(a), NULL, b,c, -1 )
#define intf_UserProgress( a, b, c, d, e ) __intf_Progress( VLC_OBJECT(a),b,c,d,e )
VLC_EXPORT( int, __intf_Progress,( vlc_object_t*, const char*, const char*, float, int) );
#define intf_ProgressUpdate( a, b, c, d, e ) __intf_ProgressUpdate( VLC_OBJECT(a),b,c,d,e )
VLC_EXPORT( void, __intf_ProgressUpdate,( vlc_object_t*, int, const char*, float, int) );
#define intf_ProgressIsCancelled( a, b ) __intf_UserProgressIsCancelled( VLC_OBJECT(a),b )
VLC_EXPORT( vlc_bool_t, __intf_UserProgressIsCancelled,( vlc_object_t*, int ) );

#define intf_UserHide( a, b ) __intf_UserHide( VLC_OBJECT(a), b )
VLC_EXPORT( void, __intf_UserHide,( vlc_object_t *, int ));

VLC_EXPORT( void, intf_InteractionManage,( playlist_t *) );
VLC_EXPORT( void, intf_InteractionDestroy,( interaction_t *) );
