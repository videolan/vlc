/*******************************************************************************
 * xutils.h: X11 utilities
 * (c)1999 VideoLAN
 *******************************************************************************
 * This library includes many usefull functions to perform simple operations
 * using Xlib.
 *******************************************************************************
 * Required headers:
 *  <X11/Xlib.h>
 *******************************************************************************/

/*******************************************************************************
 * Prototypes
 *******************************************************************************/
int     XTryLoadFont( Display *p_display, char *psz_font, XFontStruct **p_font );
void    XEnableScreenSaver( Display *p_display );
void    XDisableScreenSaver( Display *p_display );
