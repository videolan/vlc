/*******************************************************************************
 * xconsole.c: X11 console for interface
 * (c)1998 VideoLAN
 *******************************************************************************
 * The X11 console is a simple way to get interactive input from the user. It
 * does not disturbs the standard terminal output. In theory, multiple consoles
 * could be opened on different displays.
 * The console has 2 parts: a multi-line display on top, able to display
 * messages, and an input line at bottom. 
 *******************************************************************************/

/* ?? todo: background color, color lines, pixmaps in front of line, 
 * multiple fonts, control-like implementation, mouse management (copy/paste)... 
 * add a scroller to text zone
 * It will probably be better to use something esier to use than plain xlib.
 *
 * There is no prompt yet since a pixmap would be sexier as a text prompt :) 
 * issue beeps (using audio_output, of course) on 'errors' (eol, end of history
 * browse, ...) 
 * remove dups from history */

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h>
#include <keysym.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/xpm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "xutils.h"

#include "xconsole.h"
#include "intf_msg.h"
#include "intf_cmd.h"

/*
 * Local prototypes
 */
static int  CreateXConsoleWindow        ( xconsole_t *p_console );
static void DestroyXConsoleWindow       ( xconsole_t *p_console );
static void PrintXConsoleLine           ( xconsole_t *p_console, int i_line,
                                          char *psz_str );
static void RedrawXConsoleText          ( xconsole_t *p_console );
static void RedrawXConsoleEdit          ( xconsole_t *p_console );
static void RegisterXConsoleHistory     ( xconsole_t *p_console );
static void RecallXConsoleHistory       ( xconsole_t *p_console, int i_index );

/*******************************************************************************
 * intf_OpenXConsole: open console
 *******************************************************************************
 * This function try to open an X11 display and create a simple text window.
 * It returns 0 on success.
 *******************************************************************************/
int intf_OpenXConsole( xconsole_t *p_console )
{
    int i_index;                                         /* all purposes index */

    /* Open display */
    p_console->psz_display = XDisplayName( p_console->psz_display );
    p_console->p_display = XOpenDisplay( p_console->psz_display ); 
    if( !p_console->p_display )                                       /* error */
    {
        intf_ErrMsg("intf error: can't open display %s\n", 
                    p_console->psz_display );
        return( -1 );               
    }

    /* Get the screen number */
    p_console->i_screen = DefaultScreen( p_console->p_display );    
   
    /* Create window and set console attributes */
    if( CreateXConsoleWindow( p_console ) )                           /* error */
    {
        XFreeFont( p_console->p_display, p_console->p_font );     /* free font */
        XCloseDisplay( p_console->p_display );                /* close display */
        return( 1 );                                      /* return error code */
    }

    /* Set all text as empty */
    for( i_index = 0; i_index < INTF_XCONSOLE_MAX_LINES; i_index++ )
    {
        p_console->psz_text[i_index] = 0;                    /* clear the line */
    }
    p_console->i_text_index = 0;

    /* Clear edit line and history */
    for( i_index = 0; i_index < INTF_XCONSOLE_HISTORY_SIZE; i_index++ )
    {
        p_console->psz_history[i_index] = 0;                  /* clear the line */
    }
    p_console->i_history_index = 0;
    p_console->i_history_base = 0;
    p_console->i_edit_index = 0;
    p_console->i_edit_size = 0;

    /* Print welcome message */
    intf_PrintXConsole( p_console, INTF_XCONSOLE_WELCOME_MSG );

    intf_DbgMsg("intf debug: X11 console %p opened\n", p_console );
    return( 0 );
}

/*******************************************************************************
 * intf_CloseXConsole: close console                                     (ok ?)
 *******************************************************************************
 * Destroy a console window and close the display. This function should be
 * called when the interface thread ends, and any further access to this console
 * is vorbidden.
 *******************************************************************************/
void intf_CloseXConsole( xconsole_t *p_console )
{
    int i_index;                                         /* all purposes index */   

    /* Destroy history */
    for( i_index = 0; i_index < INTF_XCONSOLE_HISTORY_SIZE; i_index++ )
    {
        if( p_console->psz_history[i_index] )      /* history line isn't empty */
        {
            free( p_console->psz_history[i_index] );          /* free the line */
        }
    }

    /* Destroy text */
    for( i_index = 0; i_index < INTF_XCONSOLE_MAX_LINES; i_index++ )
    {
        if( p_console->psz_text[i_index] )                 /* line isn't empty */
        {
            free( p_console->psz_text[i_index] );             /* free the line */
        }
    }

    /* Destroy window and resources */
    DestroyXConsoleWindow( p_console ); 

    /* Close display */
    XSetErrorHandler( NULL );              /* ?? reset non-fatal error handler */
    XSetIOErrorHandler( NULL );                   /* ?? reset IO error handler */
    XCloseDisplay( p_console->p_display );                    /* close display */

    intf_DbgMsg("intf debug: X11 console %p closed\n", p_console );    
}

/*******************************************************************************
 * intf_ManageXConsole: manage X11 console events
 *******************************************************************************
 * This function manage events received in X11 console such as key pressed,
 * messages received, and so on... It also run commands typed in the console.
 *******************************************************************************/
void intf_ManageXConsole( xconsole_t *p_console )
{
    XEvent  x_event;                                          /* generic event */
    char    x_char;                                /* keyboard event character */
    KeySym  keysym;                                                  /* keysym */
    int     i_index;                                    /* multi purpose index */
    int         i_next_index;                            /* next history index */   

    /* If window has been resized, update attributes and redraw */
    while( XCheckTypedEvent( p_console->p_display, ConfigureNotify, &x_event ) )
    {
        /* ?? check if move, expose or resize !!! */
        p_console->i_width = ((XConfigureEvent *) &x_event)->width;
        p_console->i_height = ((XConfigureEvent *) &x_event)->height;
        RedrawXConsoleText( p_console );        
        RedrawXConsoleEdit( p_console );
        XFlush( p_console->p_display );
    }

    /* Check keyboard events */
    while( XCheckTypedEvent( p_console->p_display, KeyPress, &x_event ) )
    {
        /* If XLookupString returns a single character, the key is probably an
         * ascii character and is added to the edit line, except if it is a
         * control character (ascii < 32) or DEL (ascii = 127) */
        if( (XLookupString( (XKeyEvent *) &x_event, &x_char, 1, &keysym, 0 ) == 1) 
            && ( x_char >= 32 ) && ( x_char != 127 ) )
        {
            /* Increase line size, except if maximal size has been reached */
            if( ++p_console->i_edit_size > INTF_XCONSOLE_MAX_LINE_WIDTH )
            {
                /* Maximal size reached */
                p_console->i_edit_size = INTF_XCONSOLE_MAX_LINE_WIDTH;
            }

            /* Shift right line from cursor position to the end of line */
            for( i_index = p_console->i_edit_size - 1; i_index > p_console->i_edit_index; i_index-- )
            {
                p_console->sz_edit[i_index] = p_console->sz_edit[i_index - 1];                
            }

            /* Insert character at the cursor position and increase cursor position, 
             * except if the cursor reached the right of the line */
            if( p_console->i_edit_index < INTF_XCONSOLE_MAX_LINE_WIDTH )
            {
                p_console->sz_edit[p_console->i_edit_index++] = x_char;
            }
        }
        /* Else, the key is a control key, and keysym is used to process it. */
        else
        {
            switch( keysym )
            {
            /* Cursor position modification */
            case XK_Delete:                                          /* Delete */ 
                /* Delete has effect only if cursor is not at the end of the line */
                if( p_console->i_edit_index < p_console->i_edit_size )
                {
                    /* Shift left line from cursor position (excluded) to the end of line */
                    for( i_index = p_console->i_edit_index + 1; i_index < p_console->i_edit_size; i_index++ )
                    {
                        p_console->sz_edit[i_index - 1] = p_console->sz_edit[i_index];
                    }
                    /* Decrease line size */
                    p_console->i_edit_size--;
                }
                break;             
            case XK_BackSpace:                                    /* BackSpace */
                 /* BackSpace has effect only if cursor is not at the beginning of the line */
                if( p_console->i_edit_index > 0 )
                {
                    /* Shift left line from cursor position (included) to the end of line */
                    for( i_index = p_console->i_edit_index; i_index < p_console->i_edit_size; i_index++ )
                    {
                        p_console->sz_edit[i_index - 1] = p_console->sz_edit[i_index];
                    }
                    /* Decrease line size and cursor position */
                    p_console->i_edit_size--;
                    p_console->i_edit_index--;
                }
                break;
            case XK_Left:                                              /* Left */
                /* Move left */
                if( --p_console->i_edit_index < 0 )
                {
                    p_console->i_edit_index = 0;
                }
                break;  
            case XK_Right:                                            /* Right */
                /* Move right */
                if( ++p_console->i_edit_index > p_console->i_edit_size )
                {
                    p_console->i_edit_index = p_console->i_edit_size;
                }
                break;
            case XK_Home:                                              /* Home */
                /* Go to beginning of line */
                p_console->i_edit_index = 0;
                break;
            case XK_End:                                                /* End */
                /* Go to end of line */
                p_console->i_edit_index = p_console->i_edit_size;
                break;

            /* Clear */
            case XK_Escape:
                /* Clear line and reset history index and record */
                p_console->i_edit_index = 0;
                p_console->i_edit_size = 0;
                p_console->i_history_index = p_console->i_history_base;
                break;

            /* History */
            case XK_Up:
                /* If history index is history base, this marks the beginning of
                 * an history browse, and the current edited line needs to be
                 * registered */
                if( p_console->i_history_index == p_console->i_history_base )
                {
                    RegisterXConsoleHistory( p_console );
                }

                /* Calculate next index */
                if( p_console->i_history_index )
                {
                    i_next_index = p_console->i_history_index - 1;
                }
                else                        
                {
                    i_next_index = INTF_XCONSOLE_HISTORY_SIZE - 1;
                }

                /* Go to previous record, if it not blank and a loop hasn't been made */
                if( (i_next_index != p_console->i_history_base) && p_console->psz_history[i_next_index] )
                {
                    RecallXConsoleHistory( p_console, i_next_index );
                }
                break;                
            case XK_Down:
                /* If history index is not history base, Calculate next index */
                if( p_console->i_history_index != p_console->i_history_base )
                {
                    /* Calculate next index */
                    if( p_console->i_history_index == INTF_XCONSOLE_HISTORY_SIZE - 1 )
                    {
                        i_next_index = 0;
                    }
                    else
                    {
                        i_next_index = p_console->i_history_index + 1;
                    }

                    /* Go to previous record */
                    RecallXConsoleHistory( p_console, i_next_index ); 
                }
                break;            

            /* Command execution */
            case XK_Return:
                /* Command is executed only if line is not empty */
                if( p_console->i_edit_size )
                {                    
                    /* Add current line to history and increase history base. The
                     * called function also add a terminal '\0' to sz_edit, which
                     * allow to send it without furhter modification to execution. */
                    RegisterXConsoleHistory( p_console );
                    if( ++p_console->i_history_base > INTF_XCONSOLE_HISTORY_SIZE )
                    {
                        p_console->i_history_base = 0;
                    }

                    /* Execute command */
                    intf_ExecCommand( p_console->sz_edit );

                    /* Clear line and reset history index */
                    p_console->i_edit_index = 0;
                    p_console->i_edit_size = 0;
                    p_console->i_history_index = p_console->i_history_base;
                }
                break;
            }
        }

        /* Redraw edit zone */
        RedrawXConsoleEdit( p_console );
        XFlush( p_console->p_display );
    }
}

/*******************************************************************************
 * intf_ClearXConsole: clear all lines in an X11 console               (ok ?)
 *******************************************************************************
 * This function clears a X11 console, destroying all its text. The input
 * line is not reset.
 *******************************************************************************/
void intf_ClearXConsole( xconsole_t *p_console )
{
    int i_index;                                         /* all purposes index */   

    /* Clear all lines - once a line has been cleared, it's data pointer
     * must be set to 0 to avoid clearing it again. */
    for( i_index = 0; i_index < INTF_XCONSOLE_MAX_LINES; i_index++ )
    {
        if( p_console->psz_text[i_index] )          
        {
            free( p_console->psz_text[i_index] );
            p_console->psz_text[i_index] = 0;
        }
    }

    /* Redraw the window */
    RedrawXConsoleText( p_console );
    XFlush( p_console->p_display );
}

/*******************************************************************************
 * intf_PrintXConsole: print a message in a X11 console                (ok ?)
 *******************************************************************************
 * This function print a message in an X11 console window. On error, it prints
 * an error message and exit. The message is processed and copied and can be 
 * freed once the function has been called. During processing, the string can
 * be splitted in several lines if '\n' are encountered or if a line is larger
 * than INTF_XCONSOLE_MAX_LINE_WIDTH. A trailing '\n' will be ignored.
 *******************************************************************************/
void intf_PrintXConsole( xconsole_t *p_console, char *psz_str )
{
    char        sz_line_buffer[INTF_XCONSOLE_MAX_LINE_WIDTH + 1];    /* buffer */
    char *      psz_index;                                     /* string index */
    int         i_char_index;                            /* char index in line */
    boolean_t   b_eol;                                     /* end of line flag */  

    /* Split the string in different lines. A line */
    i_char_index = 0;                                           /* character 0 */
    b_eol = 0;
    for( psz_index = psz_str; *psz_index; psz_index++ )
    {
        /* Check if we reached an end of line: if the current character is
         * really an end of line or if the next one marks the end of the
         * string. An end of line at then end of a string will be only
         * processed once. */
        if( (*psz_index == '\n') || ( psz_index[1] == '\0' ) )
        {
            b_eol = 1;            
        }
        /* Else, the current character is added at the end of the line buffer */
        {
            sz_line_buffer[ i_char_index++ ] = *psz_index;
            /* If we reached the maximal size for the line buffer, we also need
             * to treat the end of line */
            if( i_char_index == INTF_XCONSOLE_MAX_LINE_WIDTH )
            {
                b_eol = 1;
            }
        }

        /* Check if an end of line has been detected */
        if( b_eol )
        {
            /* Add a '\0' character at the end of the line buffer - note that
             * i_char_index is now the line size + 1. */
            sz_line_buffer[ i_char_index++ ] = '\0';

            /* Increase line index */
            if( p_console->i_text_index++                              /* loop */
                == INTF_XCONSOLE_MAX_LINES )
            {
                p_console->i_text_index = 0;
            }
            /* If there was a line in the current location, it needs to be 
             * erased. It will disappear from the console messages history. */
            if( p_console->psz_text[ p_console->i_text_index] )  
            {
                free( p_console->psz_text[ p_console->i_text_index] );
            }

            /* Allocate memory in lines array, and copy string - a memcpy is used
             * since the size of the string is already known. On error during the 
             * allocation (ENOMEM), the line is set as blank and an error message 
             * is issued. */
            p_console->psz_text[ p_console->i_text_index ]
                = (char *) malloc( sizeof(char) * i_char_index );
            if( !p_console->psz_text[ p_console->i_text_index] )      /* error */
            {
                intf_ErrMsg("intf error: can't copy `%s' (%s)\n", 
                            sz_line_buffer, strerror(ENOMEM) );
                p_console->psz_text[ p_console->i_text_index] = 0;
            }
            else                                                    /* success */
            {
                memcpy( p_console->psz_text[ p_console->i_text_index ], 
                        sz_line_buffer, i_char_index );
            }

            /* Reset end of line indicator and char index */
            b_eol = 0;
            i_char_index = 0;
        }
    }

    /* Display lines */
    /* ?? now: redraw... would be better to scroll and draw only the good one */
    RedrawXConsoleText( p_console );
    XFlush( p_console->p_display );
}

/* following functions are local */

/*******************************************************************************
 * RegisterXConsoleHistory: send current edit line to history          (ok ?)
 *******************************************************************************
 * This function add a '\0' termination character at the end of the editing
 * buffer, and copy it to the actual history base. If there is already a
 * command registered at this place, it is removed.
 * Note that the base index isn't updated.
 * If there is not enough memory to allocate history record, the previous
 * record is still cleared, and the new one is left blank. Note that this will
 * probably cause problems when the history will be browsed, since the line
 * won't be able to be recovered.
 *******************************************************************************/
static void RegisterXConsoleHistory( xconsole_t *p_console )
{
    /* Add a null character to mark the end of the edit line */
    p_console->sz_edit[p_console->i_edit_size] = '\0';

    /* Free current history record if required */
    if(  p_console->psz_history[p_console->i_history_base] )
    {
        free( p_console->psz_history[p_console->i_history_base] );
    }

    /* Allocate memory */
    p_console->psz_history[p_console->i_history_base] 
        = (char *) malloc( sizeof(char) * ( p_console->i_edit_size + 1 ) );

    /* On error, an error message is issued and the line is left blank */
    if(  !p_console->psz_history[p_console->i_history_base] )         /* error */
    {
        intf_ErrMsg("intf error: can't register `%s' in history\n", p_console->sz_edit );
        p_console->psz_history[p_console->i_history_base] = 0;            
    }
    /* If the allocation succeeded, the line is copied. Memcpy is used rather than
     * strcpy because the line size is already known. */
    else
    {
        memcpy( p_console->psz_history[p_console->i_history_base], 
                p_console->sz_edit, p_console->i_edit_size + 1 );                
    }
}

/*******************************************************************************
 * RecallXConsoleHistory: copy an historic record to edit line         (ok ?)
 *******************************************************************************
 * This function copy an historic record to edit line and update historic 
 * index.
 *******************************************************************************/
static void RecallXConsoleHistory( xconsole_t *p_console, int i_index )
{
    /* Calculate line size and copy it to edit line */
    for( p_console->i_edit_size = 0; 
         p_console->psz_history[i_index][p_console->i_edit_size];
         p_console->i_edit_size++ )
    {
        p_console->sz_edit[p_console->i_edit_size] = p_console->psz_history[i_index][p_console->i_edit_size];
    }

    /* Update indexes */
    p_console->i_history_index = i_index;
    p_console->i_edit_index = p_console->i_edit_size;
}


/*******************************************************************************
 * CreateXConsoleWindow: open and set-up X11 console window            (ok ?)
 *******************************************************************************
 * This function tries to create and set up a console window. This window is
 * resizeable.
  *******************************************************************************/
static int CreateXConsoleWindow( xconsole_t *p_console )
{
    XSizeHints              xsize_hints;
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    int                     i_dummy;                    /* dummy for geometry */ 

    /* Load first (edit) font - some of its properties are required to adapt
     * window properties */
    if( XTryLoadFont( p_console->p_display, INTF_XCONSOLE_FONT, &p_console->p_font ) )
    {
        return( 1 );
    }  

    /* Set size related console properties */
    p_console->i_edit_height =      p_console->p_font->ascent + p_console->p_font->descent + 5;
    p_console->i_edit_offset =      p_console->p_font->descent + 2;
    p_console->i_text_offset =      p_console->i_edit_height + p_console->p_font->descent;
    p_console->i_text_line_height = p_console->p_font->ascent + p_console->p_font->descent;

    /* Read window geometry and prepare size hints */
    XParseGeometry( p_console->psz_geometry,
                    &i_dummy, &i_dummy, &p_console->i_width, &p_console->i_height );
    xsize_hints.base_width = p_console->i_height;
    xsize_hints.base_height = p_console->i_width;
    xsize_hints.min_width =  p_console->i_text_line_height;
    xsize_hints.min_height =  p_console->i_text_line_height;
    xsize_hints.flags = PSize | PMinSize;

    /* Prepare the window attributes */
    xwindow_attributes.backing_store =      Always;
    xwindow_attributes.background_pixel =   WhitePixel( p_console->p_display, p_console->i_screen );
    xwindow_attributes.event_mask =         KeyPressMask | StructureNotifyMask;

    /* Create the window */
    p_console->window = XCreateWindow( p_console->p_display,
                                       DefaultRootWindow( p_console->p_display ),
                                       0, 0, p_console->i_width, p_console->i_height, 0,
                                       CopyFromParent, InputOutput, CopyFromParent,
                                       CWBackingStore | CWBackPixel | CWEventMask,
                                       &xwindow_attributes );
    XSetWMNormalHints( p_console->p_display, p_console->window, &xsize_hints );
    XStoreName(p_console->p_display, p_console->window, INTF_XCONSOLE_TITLE);
    
    /* Creation of a graphic contexts */
    xgcvalues.background = WhitePixel( p_console->p_display, p_console->i_screen );
    xgcvalues.foreground = BlackPixel( p_console->p_display, p_console->i_screen );
    p_console->default_gc = XCreateGC( p_console->p_display, p_console->window,
                                       GCForeground | GCBackground, 
                                       &xgcvalues);

/* ?? pixmap is always freed, etc... -> to be fixed ! */
#ifdef INTF_XCONSOLE_BACKGROUND_PIXMAP
    /* Load background pixmap */
    if( XpmReadFileToPixmap( p_console->p_display,                    /* error */
                             p_console->window, INTF_XCONSOLE_BACKGROUND_PIXMAP,
                             &p_console->background_pixmap, NULL, NULL) )
    {
        intf_ErrMsg("intf error: can't load background pixmap `%s'\n", 
                    INTF_XCONSOLE_BACKGROUND_PIXMAP );
    }
    else                                                            /* success */
    {
        /* Set window background */
        XSetWindowBackgroundPixmap(p_console->p_display, p_console->window, 
                                   p_console->background_pixmap );  
    }
#endif

    /* Map the window */
    XMapWindow( p_console->p_display, p_console->window );              /* map */
    XFlush( p_console->p_display );

    return( 0 );
}

/*******************************************************************************
 * DestroyXConsoleWindow: destroy console window                       (ok ?)
 *******************************************************************************
 * Destroy a console window opened by CreateXConsoleWindow.
 *******************************************************************************/
static void DestroyXConsoleWindow( xconsole_t *p_console )
{
    /* Unmap window */
    XUnmapWindow( p_console->p_display, p_console->window );    

    /* Destroy graphic contexts */
    XFreeGC( p_console->p_display, p_console->default_gc );

    /* Free pixmaps */
    XFreePixmap( p_console->p_display, p_console->background_pixmap );

    /* Free font */
    XFreeFont( p_console->p_display, p_console->p_font );

    /* Destroy window */
    XDestroyWindow( p_console->p_display, p_console->window );
}

/*******************************************************************************
 * PrintXConsoleLine: print a message in console                       (ok ?)
 *******************************************************************************
 * Print a message in console. Line 0 is just above the edit line, and line
 * number increase while going up. The string is scanned for control
 * characters.
 *******************************************************************************/
static void PrintXConsoleLine( xconsole_t *p_console, int i_line, char *psz_str )
{
    XTextItem   textitem[INTF_XCONSOLE_MAX_LINE_WIDTH];                /* text */
    int         i_nb_textitems;                   /* total number of textitems */

    int         i_x;                                    /* horizontal position */
    int         i_y;                                 /* base vertical position */
   
    /* If string is a null pointer, return */
    if( !psz_str )
    {
        return;
    }

    /* Prepare array of textitems */
    for( i_nb_textitems = 0; 
         *psz_str && (i_nb_textitems < INTF_XCONSOLE_MAX_LINE_WIDTH);  
         i_nb_textitems++ )
    {
        /* Check if first character is a control character - if a control
         * character has been successfully processed, process next one */
        while( *psz_str == '\033' )
        {
            psz_str++;
            switch( *psz_str )
            {
            case 'g':                                                  /* bell */
                /* ?? ring bell */
                psz_str++;
                break; 

            case '\0':                                 /* error: end of string */
                /* Print error message */
                intf_ErrMsg("intf error: unauthorized control character `\\0'\n");
                break;

            default:                       /* error: unknown control character */
                /* Print error message */
                intf_ErrMsg("intf error: unknown control character `%c'\n", *psz_str );
                break;
            } 
        }

        /* Prepare textitem */
        textitem[i_nb_textitems].chars = psz_str;
        textitem[i_nb_textitems].nchars = 0;
        textitem[i_nb_textitems].delta = 0;
        textitem[i_nb_textitems].font = p_console->p_font->fid;

        /* Reach next control character or end of string */
        do
        {
            psz_str++;
            textitem[i_nb_textitems].nchars++; 
        }while( *psz_str && (*psz_str != '\033') );
    }

    /* Calulcate base position */
    i_x = 0;
    i_y = p_console->i_height - p_console->i_text_offset - i_line * p_console->i_text_line_height;

    /* Print text */
    XDrawText( p_console->p_display, p_console->window, p_console->default_gc, i_x, i_y,
               textitem, i_nb_textitems );   
}

/*******************************************************************************
 * RedrawXConsoleEdit: redraw console edit zone                        (ok ?)
 *******************************************************************************
 * This function redraw just the edit zone of the console. It has to be called
 * when the window is resized.
 *******************************************************************************/
static void RedrawXConsoleEdit( xconsole_t *p_console )
{
    XTextItem   textitem;                                              /* text */
    int         i_x;                                        /* cursor position */

    /* Clear window edit zone */
    XClearArea( p_console->p_display, p_console->window, 0,
                p_console->i_height - p_console->i_edit_height, 
                p_console->i_width, p_console->i_edit_height, False );

    /* Draw separation line */
    XDrawLine( p_console->p_display, p_console->window, p_console->default_gc,
               0, p_console->i_height - p_console->i_edit_height,
               p_console->i_width, p_console->i_height - p_console->i_edit_height );

    /* Prepare text to be printed */
    textitem.chars = p_console->sz_edit;
    textitem.nchars = p_console->i_edit_size;
    textitem.delta = 0;
    textitem.font = p_console->p_font->fid;

    /* Print text */
    XDrawText( p_console->p_display, p_console->window, p_console->default_gc, 
               0, p_console->i_height - p_console->i_edit_offset,
               &textitem, 1 );

    /* Get cursor position */
    i_x = XTextWidth( p_console->p_font, p_console->sz_edit, p_console->i_edit_index );

    /* Print cursor. Cursor is placed at the beginning of the edited character,
     * and it's size is calculated from maximal character size in the font. */
    XDrawLine( p_console->p_display, p_console->window, p_console->default_gc,
               i_x, p_console->i_height - p_console->i_edit_offset - p_console->p_font->ascent,
               i_x, p_console->i_height - p_console->i_edit_offset + p_console->p_font->descent );
}

/*******************************************************************************
 * RedrawXConsoleText: redraw console text zone                        (ok ?)
 *******************************************************************************
 * This function redraw all lines in the console. It has to be called when
 * the window is resized.
 *******************************************************************************/
static void RedrawXConsoleText( xconsole_t *p_console )
{
    int i_line;                                                /* current line */
    int i_index;                                       /* multi-purposes index */
    int i_maxline;                                      /* maximum line number */
        
    /* Clear window text zone */
    XClearArea( p_console->p_display, p_console->window, 0, 0, p_console->i_width, 
                p_console->i_height - p_console->i_edit_height, False );

    /* Get maximum line number from window height */
    i_maxline = (p_console->i_height - p_console->i_text_offset) 
        / p_console->i_text_line_height;
    
    /* Re-print messages zone: start from first message, line 0 and print
     * messages until top of window is reached or all lines have been printed,
     * which happens when i_index is the start index again */
    i_line = 0;
    i_index = p_console->i_text_index;
    do
    {
        /* Print line */
        PrintXConsoleLine( p_console, i_line, p_console->psz_text[i_index] );

        /* Decrease i_index. Note that lines must be printed in reverse order
         * since they are added in order. */
        if( !i_index-- )                                               /* loop */
        {
            i_index = INTF_XCONSOLE_MAX_LINES - 1;
        }
    }
    while( (i_line++ < i_maxline) && (i_index != p_console->i_text_index) );
}
