/*******************************************************************************
 * xconsole.h: X11 console for interface
 * (c)1998 VideoLAN
 *******************************************************************************
 * The X11 console is a simple way to get interactive input from the user. It
 * does not disturbs the standard terminal output. In theory, multiple consoles
 * could be openned on different displays.
 *?? will probably evolve
 *******************************************************************************/

/*******************************************************************************
 * xconsole_t: X11 console descriptor
 *******************************************************************************
 * The display pointer is specific to this structure since in theory, multiple
 * console could be openned on different displays. A console is divided in two
 * sections. The lower one is a single line edit control. Above, a multi-line
 * output zone allow to send messages.
 *******************************************************************************/
typedef struct
{
    /* Initialization fields - those fields should be initialized before 
     * calling intf_OpenX11Console(). */
    char *                  psz_display;                       /* display name */
    char *                  psz_geometry;                   /* window geometry */    

    /* following fields are internal */
    
    /* Settings and display properties */
    Display *               p_display;                      /* display pointer */
    int                     i_screen;                         /* screen number */
    XFontStruct *           p_font;                               /* used font */    
    Window                  window;                 /* window instance handler */

    /* Graphic contexts */
    GC                      default_gc;    /* graphic context for default text */

    /* Pixmaps */
    Pixmap                  background_pixmap;            /* window background */

    /* Window properties */
    int                     i_width, i_height;            /* window dimensions */ 
    int                     i_text_offset;  /* text zone placement from bottom */
    int                     i_text_line_height;/* height of a single text line */
    int                     i_edit_height;           /* total edit zone height */
    int                     i_edit_offset;  /* edit zone placement from bottom */

    /* Text array */
    char *                  psz_text[INTF_XCONSOLE_MAX_LINES];         /* text */    
    int                     i_text_index;                   /* last line index */

    /* Edit lines properties. The line has one more character than
     * maximum width to allow adding a terminal '\0' when it is sent to 
     * execution or text zone. The size must stay between 0 (included) and 
     * INTF_X11_CONSOLE_MAX_LINE_WIDTH (included). The cursor position (index)
     * can be between 0 (included) and size (included). */
    char                    sz_edit[INTF_XCONSOLE_MAX_LINE_WIDTH + 1]; 
    int                     i_edit_index;                   /* cursor position */
    int                     i_edit_size;            /* total size of edit text */

    /* History. The history array (composed of asciiz strings) has a base, 
     * marking the *next* registered line, and an index, marking the actual
     * line browsed. When an history browse is started, the current line is
     * stored at base (but base isn't increased), and index is modified.
     * When a command is executed, it is registered at base and base is
     * increased. */
    char *                  psz_history[INTF_XCONSOLE_HISTORY_SIZE + 1]; 
    int                     i_history_index;               /* index in history */
    int                     i_history_base;                    /* history base */
} xconsole_t;

/*******************************************************************************
 * Prototypes
 *******************************************************************************/
int  intf_OpenXConsole      ( xconsole_t *p_console );
void intf_CloseXConsole     ( xconsole_t *p_console );

void intf_ManageXConsole    ( xconsole_t *p_console );
void intf_ClearXConsole     ( xconsole_t *p_console );
void intf_PrintXConsole     ( xconsole_t *p_console, char *psz_str );
