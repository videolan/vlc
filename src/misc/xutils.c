/*******************************************************************************
 * xutils.c: X11 utilities
 * (c)1998 VideoLAN
 *******************************************************************************
 * This library includes many usefull functions to perform simple operations
 * using Xlib.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "intf_msg.h"



/*******************************************************************************
 * XTryLoadFont: try to load a font and a fallback font
 *******************************************************************************
 * This function try to load a font, and, on failure, a default fallback font
 * (usually 'fixed'). This function returns 0 on success, >0 if it failed but
 * the default font 
 *******************************************************************************/
int XTryLoadFont( Display *p_display, char *psz_font, XFontStruct **p_font )
{
    *p_font = XLoadQueryFont( p_display, psz_font );
    if( *p_font )                                                   /* success */
    {
        return( 0 );
    }

    intf_DbgMsg("X11 error: can't load font `%s', using `%s' instead\n", p_font, X11_DEFAULT_FONT );
    *p_font = XLoadQueryFont( p_display, X11_DEFAULT_FONT );
    if( *p_font )                                                   /* success */
    {
        return( 1 );
    }

    /* Fatal error */
    intf_ErrMsg("X11 error: can't load fallback font `%s'\n", X11_DEFAULT_FONT );
    return( -1 );
}

/*******************************************************************************
 * XEnableScreenSaver: enable screen saver
 *******************************************************************************
 * This function enable the screen saver on a display after it had been
 * disabled by XDisableScreenSaver. Both functions use a counter mechanism to 
 * know wether the screen saver can be activated or not: if n successive calls
 * are made to XDisableScreenSaver, n successive calls to XEnableScreenSaver
 * will be required before the screen saver could effectively be activated.
 *******************************************************************************/
int XEnableScreenSaver( Display *p_display )
{
    /* ?? */
}

/*******************************************************************************
 * XDisableScreenSaver: disable screen saver
 *******************************************************************************
 * See XEnableScreenSaver
 *******************************************************************************/
int XDisableScreenSaver( Display *p_display )
{
    /* ?? */
}

