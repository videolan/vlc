/*****************************************************************************
 * vlc_extensions.h: Extensions (meta data, web information, ...)
 *****************************************************************************
 * Copyright (C) 2009-2010 VideoLAN and authors
 * $Id$
 *
 * Authors: Jean-Philippe André < jpeg # videolan.org >
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_EXTENSIONS_H
#define VLC_EXTENSIONS_H

#include "vlc_common.h"
#include "vlc_arrays.h"

/* Structures */
typedef struct extensions_manager_sys_t extensions_manager_sys_t;
typedef struct extensions_manager_t extensions_manager_t;
typedef struct extension_sys_t extension_sys_t;

/** Extension descriptor: name, title, author, ... */
typedef struct extension_t {
    /* Below, (ro) means read-only for the GUI */
    char *psz_name;           /**< Real name of the extension (ro) */

    char *psz_title;          /**< Display title (ro) */
    char *psz_author;         /**< Author of the extension (ro) */
    char *psz_version;        /**< Version (ro) */
    char *psz_url;            /**< A URL to the official page (ro) */
    char *psz_description;    /**< Full description (ro) */
    char *psz_shortdescription; /**< Short description (eg. 1 line)  (ro) */
    char *p_icondata;         /**< Embedded data for the icon (ro) */
    int   i_icondata_size;    /**< Size of that data */

    extension_sys_t *p_sys;   /**< Reserved for the manager module */
} extension_t;

/** Extensions manager object */
struct extensions_manager_t
{
    VLC_COMMON_MEMBERS

    module_t *p_module;                /**< Extensions manager module */
    extensions_manager_sys_t *p_sys;   /**< Reserved for the module */

    DECL_ARRAY(extension_t*) extensions; /**< Array of extension descriptors */
    vlc_mutex_t lock;                  /**< A lock for the extensions array */

    /** Control, see extension_Control */
    int ( *pf_control ) ( extensions_manager_t*, int, va_list );
};

/* Control commands */
enum
{
    /* Control extensions */
    EXTENSION_ACTIVATE,       /**< arg1: extension_t* */
    EXTENSION_DEACTIVATE,     /**< arg1: extension_t* */
    EXTENSION_IS_ACTIVATED,   /**< arg1: extension_t*, arg2: bool* */
    EXTENSION_HAS_MENU,       /**< arg1: extension_t* */
    EXTENSION_GET_MENU,       /**< arg1: extension_t*, arg2: char***, arg3: uint16_t** */
    EXTENSION_TRIGGER_ONLY,   /**< arg1: extension_t*, arg2: bool* */
    EXTENSION_TRIGGER,        /**< arg1: extension_t* */
    EXTENSION_TRIGGER_MENU,   /**< arg1: extension_t*, int (uint16_t) */
    EXTENSION_SET_INPUT,      /**< arg1: extension_t*, arg2 (input_thread_t*) */
    EXTENSION_PLAYING_CHANGED, /**< arg1: extension_t*, arg2 int( playing status ) */
    EXTENSION_META_CHANGED,   /**< arg1: extension_t*, arg2 (input_item_t*) */
};

/**
 * Control function for extensions.
 * Every GUI -> extension command will go through this function.
 **/
static inline int extension_Control( extensions_manager_t *p_mgr,
                                     int i_control, ... )
{
    va_list args;
    va_start( args, i_control );
    int i_ret = p_mgr->pf_control( p_mgr, i_control, args );
    va_end( args );
    return i_ret;
}

/**
 * Helper for extension_HasMenu, extension_IsActivated...
 * Do not use.
 **/
static inline bool __extension_GetBool( extensions_manager_t *p_mgr,
                                        extension_t *p_ext,
                                        int i_flag,
                                        bool b_default )
{
    bool b = b_default;
    int i_ret = extension_Control( p_mgr, i_flag, p_ext, &b );
    if( i_ret != VLC_SUCCESS )
        return b_default;
    else
        return b;
}

/** Activate or trigger an extension */
#define extension_Activate( mgr, ext ) \
        extension_Control( mgr, EXTENSION_ACTIVATE, ext )

/** Trigger the extension. Attention: NOT multithreaded! */
#define extension_Trigger( mgr, ext ) \
        extension_Control( mgr, EXTENSION_TRIGGER, ext )

/** Deactivate an extension */
#define extension_Deactivate( mgr, ext ) \
        extension_Control( mgr, EXTENSION_DEACTIVATE, ext )

/** Is this extension activated? */
#define extension_IsActivated( mgr, ext ) \
        __extension_GetBool( mgr, ext, EXTENSION_IS_ACTIVATED, false )

/** Does this extension have a sub-menu? */
#define extension_HasMenu( mgr, ext ) \
        __extension_GetBool( mgr, ext, EXTENSION_HAS_MENU, false )

/** Get this extension's sub-menu */
static inline int extension_GetMenu( extensions_manager_t *p_mgr,
                                     extension_t *p_ext,
                                     char ***pppsz,
                                     uint16_t **ppi )
{
    return extension_Control( p_mgr, EXTENSION_GET_MENU, p_ext, pppsz, ppi );
}

/** Trigger an entry of the extension menu */
static inline int extension_TriggerMenu( extensions_manager_t *p_mgr,
                                         extension_t *p_ext,
                                         uint16_t i )
{
    return extension_Control( p_mgr, EXTENSION_TRIGGER_MENU, p_ext, i );
}

/** Trigger an entry of the extension menu */
static inline int extension_SetInput( extensions_manager_t *p_mgr,
                                        extension_t *p_ext,
                                        struct input_thread_t *p_input )
{
    return extension_Control( p_mgr, EXTENSION_SET_INPUT, p_ext, p_input );
}

static inline int extension_PlayingChanged( extensions_manager_t *p_mgr,
                                            extension_t *p_ext,
                                            int state )
{
    return extension_Control( p_mgr, EXTENSION_PLAYING_CHANGED, p_ext, state );
}

static inline int extension_MetaChanged( extensions_manager_t *p_mgr,
                                         extension_t *p_ext )
{
    return extension_Control( p_mgr, EXTENSION_META_CHANGED, p_ext );
}

/** Can this extension only be triggered but not activated?
    Not compatible with HasMenu */
#define extension_TriggerOnly( mgr, ext ) \
        __extension_GetBool( mgr, ext, EXTENSION_TRIGGER_ONLY, false )


/*****************************************************************************
 * Extension dialogs
 *****************************************************************************/

typedef struct extension_dialog_t extension_dialog_t;
typedef struct extension_widget_t extension_widget_t;

/// User interface event types
typedef enum
{
    EXTENSION_EVENT_CLICK,       ///< Click on a widget: data = widget
    EXTENSION_EVENT_CLOSE,       ///< Close the dialog: no data
    // EXTENSION_EVENT_SELECTION_CHANGED,
    // EXTENSION_EVENT_TEXT_CHANGED,
} extension_dialog_event_e;

/// Command to pass to the extension dialog owner
typedef struct
{
    extension_dialog_t *p_dlg;      ///< Destination dialog
    extension_dialog_event_e event; ///< Event, @see extension_dialog_event_e
    void *p_data;                   ///< Opaque data to send
} extension_dialog_command_t;


/// Dialog descriptor for extensions
struct extension_dialog_t
{
    vlc_object_t *p_object;      ///< Owner object (callback on "dialog-event")

    char *psz_title;             ///< Title for the Dialog (in TitleBar)
    int i_width;                 ///< Width hint in pixels (may be discarded)
    int i_height;                ///< Height hint in pixels (may be discarded)

    DECL_ARRAY(extension_widget_t*) widgets; ///< Widgets owned by the dialog

    bool b_hide;                 ///< Hide this dialog (!b_hide shows)
    bool b_kill;                 ///< Kill this dialog

    void *p_sys;                 ///< Dialog private pointer
    void *p_sys_intf;            ///< GUI private pointer
    vlc_mutex_t lock;            ///< Dialog mutex
    vlc_cond_t cond;             ///< Signaled == UI is done working on the dialog
};

/** Send a command to an Extension dialog
 * @param p_dialog The dialog
 * @param event @see extension_dialog_event_e for a list of possible events
 * @param data Optional opaque data,  @see extension_dialog_event_e
 * @return VLC error code
 **/
static inline int extension_DialogCommand( extension_dialog_t* p_dialog,
                                           extension_dialog_event_e event,
                                           void *data )
{
    extension_dialog_command_t command;
    command.p_dlg = p_dialog;
    command.event = event;
    command.p_data = data;
    var_SetAddress( p_dialog->p_object, "dialog-event", &command );
    return VLC_SUCCESS;
}

/** Close the dialog
 * @param dlg The dialog
 **/
#define extension_DialogClosed( dlg ) \
        extension_DialogCommand( dlg, EXTENSION_EVENT_CLOSE, NULL )

/** Forward a click on a widget
 * @param dlg The dialog
 * @param wdg The widget (button, ...)
 **/
#define extension_WidgetClicked( dlg, wdg ) \
        extension_DialogCommand( dlg, EXTENSION_EVENT_CLICK, wdg )

/// Widget types
typedef enum
{
    EXTENSION_WIDGET_LABEL,      ///< Text label
    EXTENSION_WIDGET_BUTTON,     ///< Clickable button
    EXTENSION_WIDGET_IMAGE,      ///< Image label (psz_text is local URI)
    EXTENSION_WIDGET_HTML,       ///< HTML or rich text area (non editable)
    EXTENSION_WIDGET_TEXT_FIELD, ///< Editable text line for user input
    EXTENSION_WIDGET_PASSWORD,   ///< Editable password input (******)
    EXTENSION_WIDGET_DROPDOWN,   ///< Drop-down box
    EXTENSION_WIDGET_LIST,       ///< Vertical list box (of strings)
    EXTENSION_WIDGET_CHECK_BOX,  ///< Checkable box with label
    EXTENSION_WIDGET_SPIN_ICON,  ///< A "loading..." spinning icon
} extension_widget_type_e;

/// Widget descriptor for extensions
struct extension_widget_t
{
    /* All widgets */
    extension_widget_type_e type; ///< Type of the widget
    char *psz_text;               ///< Text. May be NULL or modified by the UI

    /* Drop-down & List widgets */
    struct extension_widget_value_t {
        int i_id;          ///< Identifier for the extension module
                           // (weird behavior may occur if not unique)
        char *psz_text;    ///< String value
        bool b_selected;   ///< True if this item is selected
        struct extension_widget_value_t *p_next; ///< Next value or NULL
    } *p_values;                  ///< Chained list of values (Drop-down/List)

    /* Check-box */
    bool b_checked;               ///< Is this entry checked

    /* Layout */
    int i_row;                    ///< Row in the grid
    int i_column;                 ///< Column in the grid
    int i_horiz_span;             ///< Horizontal size of the object
    int i_vert_span;              ///< Vertical size of the object
    int i_width;                  ///< Width hint
    int i_height;                 ///< Height hint
    bool b_hide;                  ///< Hide this widget (make it invisible)

    /* Spinning icon */
    int i_spin_loops;             ///< Number of loops to play (-1 = infinite,
                                  // 0 = stop animation)

    /* Orders */
    bool b_kill;                  ///< Destroy this widget
    bool b_update;                ///< Update this widget

    /* Misc */
    void *p_sys;                  ///< Reserved for the extension manager
    void *p_sys_intf;             ///< Reserved for the UI, but:
                                  // NULL means the UI has destroyed the widget
                                  // or has not created it yet
    extension_dialog_t *p_dialog; ///< Parent dialog
};

VLC_API int dialog_ExtensionUpdate(vlc_object_t*, extension_dialog_t *);
#define dialog_ExtensionUpdate(o, d) dialog_ExtensionUpdate(VLC_OBJECT(o), d)

#endif /* VLC_EXTENSIONS_H */

