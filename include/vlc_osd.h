/*****************************************************************************
 * vlc_osd.h - OSD menu and subpictures definitions and function prototypes
 *****************************************************************************
 * Copyright (C) 1999-2006 the VideoLAN team
 * Copyright (C) 2004-2005 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Gildas Bazin <gbazin@videolan.org>
 *
 * Added code from include/osd.h written by:
 * Copyright (C) 2003-2005 the VideoLAN team
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

#ifndef VLC_OSD_H
#define VLC_OSD_H 1

#include "vlc_vout.h"

# ifdef __cplusplus
extern "C" {
# endif

/**
 * \file
 * This file defines SPU subpicture and OSD functions and object types.
 */

/**********************************************************************
 * Base SPU structures
 **********************************************************************/
/**
 * \defgroup spu Subpicture Unit
 * This module describes the programming interface for the subpicture unit.
 * It includes functions allowing to create/destroy an spu, create/destroy
 * subpictures and render them.
 * @{
 */

#include <vlc_vout.h>

typedef struct spu_private_t spu_private_t;

/* Default subpicture channel ID */
#define DEFAULT_CHAN           1

/**
 * Subpicture unit descriptor
 */
struct spu_t
{
    VLC_COMMON_MEMBERS

    int (*pf_control)( spu_t *, int, va_list );

    spu_private_t *p;
};

enum spu_query_e
{
    SPU_CHANNEL_REGISTER,         /* arg1= int *   res=    */
    SPU_CHANNEL_CLEAR             /* arg1= int     res=    */
};

static inline int spu_vaControl( spu_t *p_spu, int i_query, va_list args )
{
    if( p_spu->pf_control )
        return p_spu->pf_control( p_spu, i_query, args );
    else
        return VLC_EGENERIC;
}

static inline int spu_Control( spu_t *p_spu, int i_query, ... )
{
    va_list args;
    int i_result;

    va_start( args, i_query );
    i_result = spu_vaControl( p_spu, i_query, args );
    va_end( args );
    return i_result;
}

VLC_EXPORT( spu_t *, spu_Create, ( vlc_object_t * ) );
#define spu_Create(a) spu_Create(VLC_OBJECT(a))
VLC_EXPORT( int, spu_Init, ( spu_t * ) );
VLC_EXPORT( void, spu_Destroy, ( spu_t * ) );
void spu_Attach( spu_t *, vlc_object_t *, bool );

/**
 * This function sends a subpicture to the spu_t core.
 * 
 * You cannot use the provided subpicture anymore. The spu_t core
 * will destroy it at its convenience.
 */
VLC_EXPORT( void, spu_DisplaySubpicture, ( spu_t *, subpicture_t * ) );

/**
 * This function asks the spu_t core a list of subpictures to display.
 *
 * The returned list can only be used by spu_RenderSubpictures.
 */
VLC_EXPORT( subpicture_t *, spu_SortSubpictures, ( spu_t *, mtime_t render_subtitle_date, bool b_subtitle_only ) );

/**
 * This function renders a list of subpicture_t on the provided picture.
 *
 * \param p_fmt_dst is the format of the destination picture.
 * \param p_fmt_src is the format of the original(source) video.
 */
VLC_EXPORT( void, spu_RenderSubpictures, ( spu_t *,  picture_t *, const video_format_t *p_fmt_dst, subpicture_t *p_list, const video_format_t *p_fmt_src, mtime_t render_subtitle_date ) );

/** @}*/

/**********************************************************************
 * OSD Menu
 **********************************************************************/
/**
 * \defgroup osdmenu OSD Menu
 * The OSD menu core creates the OSD menu structure in memory. It parses a
 * configuration file that defines all elements that are part of the menu. The
 * core also handles all actions and menu structure updates on behalf of video
 * subpicture filters.
 *
 * The file modules/video_filters/osdmenu.c implements a subpicture filter that
 * specifies the final information on positioning of the current state image.
 * A subpicture filter is called each time a video picture has to be rendered,
 * it also gives a start and end date to the subpicture. The subpicture can be
 * streamed if used inside a transcoding command. For example:
 *
 *  vlc dvdsimple:///dev/dvd --extraintf rc
 *  --sout='#transcode{osd}:std{access=udp,mux=ts,dst=dest_ipaddr}'
 *  --osdmenu-file=share/osdmenu/dvd.cfg
 *
 * An example for local usage of the OSD menu is:
 *
 *  vlc dvdsimple:///dev/dvd --extraintf rc
 *  --sub-filter osdmenu
 *  --osdmenu-file=share/osdmenu/dvd.cfg
 *
 * Each OSD menu element, called "action", defines a hotkey action. Each action
 * can have several states (unselect, select, pressed). Each state has an image
 * that represents the state visually. The commands "menu right", "menu left",
 * "menu up" and "menu down" are used to navigate through the OSD menu structure.
 * The commands "menu on" or "menu show" and "menu off" or "menu hide" respectively
 * show and hide the OSD menu subpictures.
 *
 * There is one special element called "range". A range is an arbritary range
 * of state images that can be browsed using "menu up" and "menu down" commands
 * on the rc interface.
 *
 * The OSD menu configuration file uses a very simple syntax and basic parser.
 * A configuration file has the ".cfg".
 * An example is "share/osdmenu/dvd256.cfg".
 * @{
 */

/**
 * \brief The OSD Menu configuration file format.
 *
 * The configuration file syntax is very basic and so is its parser. See the
 * BNF formal representation below:
 *
 * The keywords FILENAME and PATHNAME represent the filename and pathname
 * specification that is valid for the Operating System VLC is compiled for.
 *
 * The hotkey actions that are supported by VLC are documented in the file
 * src/libvlc. The file include/vlc_keys.h defines some hotkey internals.
 *
 * CONFIG_FILE = FILENAME '.cfg'
 * WS = [ ' ' | '\t' ]+
 * OSDMENU_PATH = PATHNAME
 * DIR = 'dir' WS OSDMENU_PATH '\n'
 * STYLE = 'style' [ 'default' | 'concat' ] '\n'
 * STATE = [ 'unselect' | 'select' | 'pressed' ]
 * HOTKEY_ACTION = 'key-' [ 'a' .. 'z', 'A' .. 'Z', '-' ]+
 *
 * ACTION_TYPE        = 'type' 'volume' '\n'
 * ACTION_BLOCK_START = 'action' WS HOTKEY_ACTION WS '('POS','POS')' '\n'
 * ACTION_BLOCK_END   = 'end' '\n'
 * ACTION_STATE       = STATE WS FILENAME '\n'
 * ACTION_RANGE_START = 'range' WS HOTKEY_ACTION WS DEFAULT_INDEX '\n'
 * ACTION_RANGE_END   = 'end' '\n'
 * ACTION_RANGE_STATE = FILENAME '\n'
 *
 * ACTION_BLOCK_RANGE = ACTION_RANGE_START [WS ACTION_RANGE_STATE]+ WS ACTION_RANGE_END
 * ACTION_BLOCK = ACTION_BLOCK_START [WS ACTION_TYPE*] [ [WS ACTION_STATE]+3 | [WS ACTION_BLOCK_RANGE]+1 ] ACTION_BLOCK_END
 * CONFIG_FILE_CONTENTS = DIR [ACTION_BLOCK]+
 *
 */

/**
 * OSD menu position and picture type defines
 */

#define OSD_ALIGN_LEFT 0x1
#define OSD_ALIGN_RIGHT 0x2
#define OSD_ALIGN_TOP 0x4
#define OSD_ALIGN_BOTTOM 0x8

#define OSD_HOR_SLIDER 1
#define OSD_VERT_SLIDER 2

#define OSD_PLAY_ICON 1
#define OSD_PAUSE_ICON 2
#define OSD_SPEAKER_ICON 3
#define OSD_MUTE_ICON 4

/**
 * Text style
 *
 * A text style is used to specify the formatting of text.
 * A font renderer can use the supplied information to render the
 * text specified.
 */
struct text_style_t
{
    char *     psz_fontname;      /**< The name of the font */
    int        i_font_size;       /**< The font size in pixels */
    int        i_font_color;      /**< The color of the text 0xRRGGBB
                                       (native endianness) */
    int        i_font_alpha;      /**< The transparency of the text.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
    int        i_style_flags;     /**< Formatting style flags */
    int        i_outline_color;   /**< The color of the outline 0xRRGGBB */
    int        i_outline_alpha;   /**< The transparency of the outline.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
    int        i_shadow_color;    /**< The color of the shadow 0xRRGGBB */
    int        i_shadow_alpha;    /**< The transparency of the shadow.
                                        0x00 is fully opaque,
                                        0xFF fully transparent */
    int        i_background_color;/**< The color of the background 0xRRGGBB */
    int        i_background_alpha;/**< The transparency of the background.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
    int        i_karaoke_background_color;/**< Background color for karaoke 0xRRGGBB */
    int        i_karaoke_background_alpha;/**< The transparency of the karaoke bg.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
    int        i_outline_width;   /**< The width of the outline in pixels */
    int        i_shadow_width;    /**< The width of the shadow in pixels */
    int        i_spacing;         /**< The spaceing between glyphs in pixels */
};

/* Style flags for \ref text_style_t */
#define STYLE_BOLD        1
#define STYLE_ITALIC      2
#define STYLE_OUTLINE     4
#define STYLE_SHADOW      8
#define STYLE_BACKGROUND  16
#define STYLE_UNDERLINE   32
#define STYLE_STRIKEOUT   64

/**
 * Create a default text style
 */
VLC_EXPORT( text_style_t *, text_style_New, ( void ) );

/**
 * Copy a text style into another
 */
VLC_EXPORT( text_style_t *, text_style_Copy, ( text_style_t *, const text_style_t * ) );

/**
 * Duplicate a text style
 */
VLC_EXPORT( text_style_t *, text_style_Duplicate, ( const text_style_t * ) );

/**
 * Delete a text style created by text_style_New or text_style_Duplicate
 */
VLC_EXPORT( void, text_style_Delete, ( text_style_t * ) );

/**
 * OSD menu button states
 *
 * Every button has three states, either it is unselected, selected or pressed.
 * An OSD menu skin can associate images with each state.
 *
 *  OSD_BUTTON_UNSELECT 0
 *  OSD_BUTTON_SELECT   1
 *  OSD_BUTTON_PRESSED  2
 */
#define OSD_BUTTON_UNSELECT 0
#define OSD_BUTTON_SELECT   1
#define OSD_BUTTON_PRESSED  2

/**
 * OSD State object
 *
 * The OSD state object holds the state and associated images for a
 * particular state on the screen. The picture is displayed when this
 * state is the active state.
 */
struct osd_state_t
{
    osd_state_t *p_next;    /*< pointer to next state */
    osd_state_t *p_prev;    /*< pointer to previous state */
    picture_t   *p_pic;     /*< picture of state */

    char        *psz_state; /*< state name */
    int          i_state;   /*< state index */

    int     i_x;            /*< x-position of button state image */
    int     i_y;            /*< y-position of button state image */
    int     i_width;        /*< width of button state image */
    int     i_height;       /*< height of button state image */
};

/**
 * OSD Button object
 *
 * An OSD Button has different states. Each state has an image for display.
 */
struct osd_button_t
{
    osd_button_t *p_next;   /*< pointer to next button */
    osd_button_t *p_prev;   /*< pointer to previous button */
    osd_button_t *p_up;     /*< pointer to up button */
    osd_button_t *p_down;   /*< pointer to down button */

    osd_state_t *p_current_state; /*< pointer to current state image */
    osd_state_t *p_states; /*< doubly linked list of states */
    picture_t   *p_feedback; /*< feedback picture */

    char    *psz_name;     /*< name of button */

    /* These member should probably be a struct hotkey */
    char    *psz_action;      /*< hotkey action name on button*/
    char    *psz_action_down; /*< hotkey action name on range buttons
                                  for command "menu down" */
    /* end of hotkey specifics */

    int     i_x;            /*< x-position of button visible state image */
    int     i_y;            /*< y-position of button visible state image */
    int     i_width;        /*< width of button visible state image */
    int     i_height;       /*< height of button visible state image */

    /* range style button */
    bool   b_range;    /*< button should be interpreted as range */
    int          i_ranges;   /*< number of states */
};

/**
 * OSD Menu Style
 *
 * The images that make up an OSD menu can be created in such away that
 * they contain all buttons in the same picture, with the selected one
 * highlighted or being a concatenation of all the seperate images. The
 * first case is the default.
 *
 * To change the default style the keyword 'style' should be set to 'concat'.
 */

#define OSD_MENU_STYLE_SIMPLE 0x0
#define OSD_MENU_STYLE_CONCAT 0x1

/**
 * OSD Menu State object
 *
 * Represents the current state as displayed.
 */
/* Represent the menu state */
struct osd_menu_state_t
{
    int     i_x;        /*< x position of spu region */
    int     i_y;        /*< y position of spu region */
    int     i_width;    /*< width of spu region */
    int     i_height;   /*< height of spu region */

    picture_t    *p_pic;  /*< pointer to picture to display */
    osd_button_t *p_visible; /*< shortcut to visible button */

    bool b_menu_visible; /*< menu currently visible? */
    bool b_update;       /*< update OSD Menu when true */

    /* quick hack to volume state. */
    osd_button_t *p_volume; /*< pointer to volume range object. */
};

/**
 * OSD Menu object
 *
 * The main OSD Menu object, which holds a linked list to all buttons
 * and images that defines the menu. The p_state variable represents the
 * current state of the OSD Menu.
 */
struct osd_menu_t
{
    VLC_COMMON_MEMBERS

    int     i_x;        /*< x-position of OSD Menu on the video screen */
    int     i_y;        /*< y-position of OSD Menu on the video screen */
    int     i_width;    /*< width of OSD Menu on the video screen */
    int     i_height;   /*< height of OSD Menu on the video screen */
    int     i_style;    /*< style of spu region generation */
    int     i_position; /*< display position */

    char             *psz_path;  /*< directory where OSD menu images are stored */
    osd_button_t     *p_button;  /*< doubly linked list of buttons */
    osd_menu_state_t *p_state;   /*< current state of OSD menu */

    /* quick link in the linked list. */
    osd_button_t  *p_last_button; /*< pointer to last button in the list */

    /* misc parser */
    module_t        *p_parser;  /*< pointer to parser module */
    char            *psz_file;  /*< Config file name */
    image_handler_t *p_image;   /*< handler to image loading and conversion libraries */
};

/**
 * Initialize an osd_menu_t object
 *
 * This functions has to be called before any call to other osd_menu_t*
 * functions. It creates the osd_menu object and holds a pointer to it
 * during its lifetime.
 */
VLC_EXPORT( osd_menu_t *, osd_MenuCreate, ( vlc_object_t *, const char * ) );

/**
 * Delete the osd_menu_t object
 *
 * This functions has to be called to release the associated module and
 * memory for the osdmenu. After return of this function the pointer to
 * osd_menu_t* is invalid.
 */
VLC_EXPORT( void, osd_MenuDelete, ( vlc_object_t *, osd_menu_t * ) );

#define osd_MenuCreate(object,file) osd_MenuCreate( VLC_OBJECT(object), file )
#define osd_MenuDelete(object,osd)  osd_MenuDelete( VLC_OBJECT(object), osd )

/**
 * Find OSD Menu button at position x,y
 */
VLC_EXPORT( osd_button_t *, osd_ButtonFind, ( vlc_object_t *p_this,
     int, int, int, int, int, int ) );

#define osd_ButtonFind(object,x,y,h,w,sh,sw)  osd_ButtonFind(object,x,y,h,w,sh,sw)

/**
 * Select the button provided as the new active button
 */
VLC_EXPORT( void, osd_ButtonSelect, ( vlc_object_t *, osd_button_t *) );

#define osd_ButtonSelect(object,button) osd_ButtonSelect(object,button)

/**
 * Show the OSD menu.
 *
 * Show the OSD menu on the video output or mux it into the stream.
 * Every change to the OSD menu will now be visible in the output. An output
 * can be a video output window or a stream (\see stream output)
 */
VLC_EXPORT( void, osd_MenuShow, ( vlc_object_t * ) );

/**
 * Hide the OSD menu.
 *
 * Stop showing the OSD menu on the video output or mux it into the stream.
 */
VLC_EXPORT( void, osd_MenuHide, ( vlc_object_t * ) );

/**
 * Activate the action of this OSD menu item.
 *
 * The rc interface command "menu select" triggers the sending of an
 * hotkey action to the hotkey interface. The hotkey that belongs to
 * the current highlighted OSD menu item will be used.
 */
VLC_EXPORT( void, osd_MenuActivate,   ( vlc_object_t * ) );

#define osd_MenuShow(object) osd_MenuShow( VLC_OBJECT(object) )
#define osd_MenuHide(object) osd_MenuHide( VLC_OBJECT(object) )
#define osd_MenuActivate(object)   osd_MenuActivate( VLC_OBJECT(object) )

/**
 * Next OSD menu item
 *
 * Select the next OSD menu item to be highlighted.
 * Note: The actual position on screen of the menu item is determined by
 * the OSD menu configuration file.
 */
VLC_EXPORT( void, osd_MenuNext, ( vlc_object_t * ) );

/**
 * Previous OSD menu item
 *
 * Select the previous OSD menu item to be highlighted.
 * Note: The actual position on screen of the menu item is determined by
 * the OSD menu configuration file.
 */
VLC_EXPORT( void, osd_MenuPrev, ( vlc_object_t * ) );

/**
 * OSD menu item above
 *
 * Select the OSD menu item above the current item to be highlighted.
 * Note: The actual position on screen of the menu item is determined by
 * the OSD menu configuration file.
 */
VLC_EXPORT( void, osd_MenuUp,   ( vlc_object_t * ) );

/**
 * OSD menu item below
 *
 * Select the next OSD menu item below the current item to be highlighted.
 * Note: The actual position on screen of the menu item is determined by
 * the OSD menu configuration file.
 */
VLC_EXPORT( void, osd_MenuDown, ( vlc_object_t * ) );

#define osd_MenuNext(object) osd_MenuNext( VLC_OBJECT(object) )
#define osd_MenuPrev(object) osd_MenuPrev( VLC_OBJECT(object) )
#define osd_MenuUp(object)   osd_MenuUp( VLC_OBJECT(object) )
#define osd_MenuDown(object) osd_MenuDown( VLC_OBJECT(object) )

/**
 * Display the audio volume bitmap.
 *
 * Display the correct audio volume bitmap that corresponds to the
 * current Audio Volume setting.
 */
VLC_EXPORT( void, osd_Volume, ( vlc_object_t * ) );

#define osd_Volume(object)     osd_Volume( VLC_OBJECT(object) )

/**
 * Retrieve a non modifyable pointer to the OSD Menu state
 *
 */
static inline const osd_menu_state_t *osd_GetMenuState( osd_menu_t *p_osd )
{
    return( p_osd->p_state );
}

/**
 * Get the last key press received by the OSD Menu
 *
 * Returns 0 when no key has been pressed or the value of the key pressed.
 */
static inline bool osd_GetKeyPressed( osd_menu_t *p_osd )
{
    return( p_osd->p_state->b_update );
}

/**
 * Set the key pressed to a value.
 *
 * Assign a new key value to the last key pressed on the OSD Menu.
 */
static inline void osd_SetKeyPressed( vlc_object_t *p_this, int i_value )
{
    vlc_value_t val;

    val.i_int = i_value;
    var_Set( p_this, "key-pressed", val );
}

/**
 * Update the OSD Menu visibility flag.
 *
 * true means OSD Menu should be shown. false means OSD Menu
 * should not be shown.
 */
static inline void osd_SetMenuVisible( osd_menu_t *p_osd, bool b_value )
{
    vlc_value_t val;

    val.b_bool = p_osd->p_state->b_menu_visible = b_value;
    var_Set( p_osd, "osd-menu-visible", val );
}

/**
 * Update the OSD Menu update flag
 *
 * If the OSD Menu should be updated then set the update flag to
 * true, else to false.
 */
static inline void osd_SetMenuUpdate( osd_menu_t *p_osd, bool b_value )
{
    vlc_value_t val;

    val.b_bool = p_osd->p_state->b_update = b_value;
    var_Set( p_osd, "osd-menu-update", val );
}

/**
 * Textual feedback
 *
 * Functions that provide the textual feedback on the OSD. They are shown
 * on hotkey commands. The feedback is also part of the osd_button_t
 * object. The types are declared in the include file include/vlc_osd.h
 * @see vlc_osd.h
 */
VLC_EXPORT( int, osd_ShowTextRelative, ( spu_t *, int, const char *, const text_style_t *, int, int, int, mtime_t ) );
VLC_EXPORT( int, osd_ShowTextAbsolute, ( spu_t *, int, const char *, const text_style_t *, int, int, int, mtime_t, mtime_t ) );
VLC_EXPORT( void, osd_Message, ( spu_t *, int, char *, ... ) LIBVLC_FORMAT( 3, 4 ) );

/**
 * Default feedback images
 *
 * Functions that provide the default OSD feedback images on hotkey
 * commands. These feedback images are also part of the osd_button_t
 * object. The types are declared in the include file include/vlc_osd.h
 * @see vlc_osd.h
 */
VLC_EXPORT( int, osd_Slider, ( vlc_object_t *, spu_t *, int, int, int, int, int, int, short ) );
VLC_EXPORT( int, osd_Icon, ( vlc_object_t *, spu_t *, int, int, int, int, int, short ) );

/** @} */

/**********************************************************************
 * Vout text and widget overlays
 **********************************************************************/

VLC_EXPORT( int, vout_ShowTextRelative, ( vout_thread_t *, int, char *, const text_style_t *, int, int, int, mtime_t ) );

VLC_EXPORT( int, vout_ShowTextAbsolute, ( vout_thread_t *, int, const char *, const text_style_t *, int, int, int, mtime_t, mtime_t ) );

VLC_EXPORT( int, vout_OSDEpg, ( vout_thread_t *, input_item_t * ) );

/**
 * Write an informative message at the default location,
 * for the default duration and only if the OSD option is enabled.
 * \param p_caller The object that called the function.
 * \param i_channel Subpicture channel
 * \param psz_format printf style formatting
 **/
VLC_EXPORT( void,  vout_OSDMessage, ( vlc_object_t *, int, const char *, ... ) LIBVLC_FORMAT( 3, 4 ) );

#define vout_OSDMessage( obj, chan, ...) \
        vout_OSDMessage( VLC_OBJECT(obj), chan, __VA_ARGS__ )

/**
 * Display a slider on the video output.
 * \param p_this    The object that called the function.
 * \param i_channel Subpicture channel
 * \param i_postion Current position in the slider
 * \param i_type    Types are: OSD_HOR_SLIDER and OSD_VERT_SLIDER.
 * @see vlc_osd.h
 */
VLC_EXPORT( void, vout_OSDSlider, ( vlc_object_t *, int, int , short ) );

/**
 * Display an Icon on the video output.
 * \param p_this    The object that called the function.
 * \param i_channel Subpicture channel
 * \param i_type    Types are: OSD_PLAY_ICON, OSD_PAUSE_ICON, OSD_SPEAKER_ICON, OSD_MUTE_ICON
 * @see vlc_osd.h
 */
VLC_EXPORT( void, vout_OSDIcon, ( vlc_object_t *, int, short ) );

# ifdef __cplusplus
}
# endif

#endif /* _VLC_OSD_H */
