/*******************************************************************************
 * vout_x11.c: X11 video output display method
 * (c)1998 VideoLAN
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "video.h"
#include "video_output.h"
#include "video_sys.h"
#include "intf_msg.h"

/*******************************************************************************
 * vout_sys_t: video output X11 method descriptor
 *******************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the X11 specific properties of an output thread. X11 video 
 * output is performed through regular resizable windows. Windows can be
 * dynamically resized to adapt to the size of the streams.
 *******************************************************************************/
typedef struct vout_sys_s
{
    /* User settings */
    boolean_t           b_shm;                 /* shared memory extension flag */

    /* Internal settings and properties */
    Display *           p_display;                          /* display pointer */
    int                 i_screen;                             /* screen number */
    Window              root_window;                            /* root window */
    Window              window;                     /* window instance handler */
    GC                  gc;                /* graphic context instance handler */    

    /* Font information */
    int                 i_char_bytes_per_line;      /* character width (bytes) */
    int                 i_char_height;             /* character height (lines) */
    int                 i_char_interspacing; /* space between centers (pixels) */
    byte_t *            pi_font;                       /* pointer to font data */

    /* Display buffers and shared memory information */
    int                 i_buffer_index;                        /* buffer index */
    XImage *            p_ximage[2];                         /* XImage pointer */   
    XShmSegmentInfo     shm_info[2];         /* shared memory zone information */
} vout_sys_t;

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/
static int  X11OpenDisplay      ( vout_thread_t *p_vout, char *psz_display, Window root_window );
static void X11CloseDisplay     ( vout_thread_t *p_vout );
static int  X11GetFont          ( vout_thread_t *p_vout );
static int  X11CreateWindow     ( vout_thread_t *p_vout );
static void X11DestroyWindow    ( vout_thread_t *p_vout );
static int  X11CreateImage      ( vout_thread_t *p_vout, XImage **pp_ximage );
static void X11DestroyImage     ( XImage *p_ximage );
static int  X11CreateShmImage   ( vout_thread_t *p_vout, XImage **pp_ximage, 
                                  XShmSegmentInfo *p_shm_info );
static void X11DestroyShmImage  ( vout_thread_t *p_vout, XImage *p_ximage, 
                                  XShmSegmentInfo *p_shm_info );


/*******************************************************************************
 * vout_SysCreate: allocate X11 video thread output method
 *******************************************************************************
 * This function allocate and initialize a X11 vout method. It uses some of the
 * vout properties to choose the window size, and change them according to the
 * actual properties of the display.
 *******************************************************************************/
int vout_SysCreate( vout_thread_t *p_vout, char *psz_display, int i_root_window )
{
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );    
    if( p_vout->p_sys == NULL )
    {   
        intf_ErrMsg("error: %s\n", strerror(ENOMEM) );        
        return( 1 );        
    }    

    /* Open and initialize device. This function issues its own error messages.
     * Since XLib is usually not thread-safe, we can't use the same display
     * pointer than the interface or another thread. However, the root window
     * id is still valid. */
    if( X11OpenDisplay( p_vout, psz_display, i_root_window ) )
    {
        intf_ErrMsg("error: can't initialize X11 display\n" );
        free( p_vout->p_sys );
        return( 1 );               
    }

    return( 0 );
}

/*******************************************************************************
 * vout_SysInit: initialize X11 video thread output method
 *******************************************************************************
 * This function create the XImages needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *******************************************************************************/
int vout_SysInit( vout_thread_t *p_vout )
{ 
    int i_err;

    /* Create XImages using XShm extension - on failure, fall back to regular 
     * way (and destroy the first image if it was created successfully) */
    if( p_vout->p_sys->b_shm )
    {
        /* Create first image */
        i_err = X11CreateShmImage( p_vout, &p_vout->p_sys->p_ximage[0], 
                                   &p_vout->p_sys->shm_info[0] );
        if( !i_err )                           /* first image has been created */
        {
            /* Create second image */
            if( X11CreateShmImage( p_vout, &p_vout->p_sys->p_ximage[1], 
                                   &p_vout->p_sys->shm_info[1] ) )
            {                               /* error creating the second image */
                X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[0], 
                                    &p_vout->p_sys->shm_info[0] );
                i_err = 1;
            }
        }
        if( i_err )                                        /* an error occured */
        {                        
            intf_Msg("Video: XShm extension desactivated\n" );
            p_vout->p_sys->b_shm = 0;
        }
    }

    /* Create XImages without XShm extension */
    if( !p_vout->p_sys->b_shm )
    {
        if( X11CreateImage( p_vout, &p_vout->p_sys->p_ximage[0] ) )
        {
            intf_ErrMsg("error: can't create images\n");
            p_vout->p_sys->p_ximage[0] = NULL;
            p_vout->p_sys->p_ximage[1] = NULL;
            return( 1 );
        }
        if( X11CreateImage( p_vout, &p_vout->p_sys->p_ximage[1] ) )
        {
            intf_ErrMsg("error: can't create images\n");
            X11DestroyImage( p_vout->p_sys->p_ximage[0] );
            p_vout->p_sys->p_ximage[0] = NULL;
            p_vout->p_sys->p_ximage[1] = NULL;
            return( 1 );
        }
    }

    /* Set bytes per line */
    p_vout->i_bytes_per_line = p_vout->p_sys->p_ximage[0]->bytes_per_line;    

    /* Set buffer index to 0 */
    p_vout->p_sys->i_buffer_index = 0;
    return( 0 );
}

/*******************************************************************************
 * vout_SysEnd: terminate X11 video thread output method
 *******************************************************************************
 * Destroy the X11 XImages created by vout_SysInit. It is called at the end of 
 * the thread, but also each time the window is resized.
 *******************************************************************************/
void vout_SysEnd( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_shm )                              /* Shm XImages... */
    {
        X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[0], 
                            &p_vout->p_sys->shm_info[0] );
        X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[1], 
                            &p_vout->p_sys->shm_info[1] );
    }
    else                                          /* ...or regular XImages */
    {
        X11DestroyImage( p_vout->p_sys->p_ximage[0] );
        X11DestroyImage( p_vout->p_sys->p_ximage[1] );
    }
}

/*******************************************************************************
 * vout_SysDestroy: destroy X11 video thread output method
 *******************************************************************************
 * Terminate an output method created by vout_X11CreateOutputMethod
 *******************************************************************************/
void vout_SysDestroy( vout_thread_t *p_vout )
{
    X11CloseDisplay( p_vout );
    free( p_vout->p_sys );
}

/*******************************************************************************
 * vout_SysManage: handle X11 events
 *******************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a non null value on 
 * error.
 *******************************************************************************/
int vout_SysManage( vout_thread_t *p_vout )
{
    if( p_vout->i_changes & VOUT_SIZE_CHANGE ) 
    {        
        intf_DbgMsg("resizing window\n");      
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;        

        /* Resize window */
        XResizeWindow( p_vout->p_sys->p_display, p_vout->p_sys->window, 
                       p_vout->i_width, p_vout->i_height );

        /* Destroy XImages to change their size */
        vout_SysEnd( p_vout );

        /* Recreate XImages. If SysInit failed, the thread can't go on. */
        if( vout_SysInit( p_vout ) )
        {
            intf_ErrMsg("error: can't resize display\n");
            return( 1 );            
        }
        intf_Msg("Video: display resized to %dx%d\n", p_vout->i_width, p_vout->i_height);            
    }
    
    return 0;
}

/*******************************************************************************
 * vout_SysDisplay: displays previously rendered output
 *******************************************************************************
 * This function send the currently rendered image to X11 server, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *******************************************************************************/
void vout_SysDisplay( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_shm)                                  /* XShm is used */
    {
        /* Display rendered image using shared memory extension */
        XShmPutImage(p_vout->p_sys->p_display, p_vout->p_sys->window, p_vout->p_sys->gc, 
                     p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ], 
                     0, 0, 0, 0,  
                     p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ]->width,  
                     p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ]->height, True);

        /* Send the order to the X server */
        XFlush(p_vout->p_sys->p_display);
    }
    else                                  /* regular X11 capabilities are used */
    {
        XPutImage(p_vout->p_sys->p_display, p_vout->p_sys->window, p_vout->p_sys->gc, 
                  p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ], 
                  0, 0, 0, 0,  
                  p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ]->width,  
                  p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ]->height);

        /* Send the order to the X server */
        XFlush(p_vout->p_sys->p_display);
    }

    /* Swap buffers */
    p_vout->p_sys->i_buffer_index = ++p_vout->p_sys->i_buffer_index & 1;
}

/*******************************************************************************
 * vout_SysGetPicture: get current display buffer informations
 *******************************************************************************
 * This function returns the address of the current display buffer.
 *******************************************************************************/
void * vout_SysGetPicture( vout_thread_t *p_vout )
{
    return( p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ]->data );        
}

/*******************************************************************************
 * vout_SysPrint: print simple text on a picture
 *******************************************************************************
 * This function will print a simple text on the picture. It is designed to
 * print debugging or general informations, not to render subtitles.
 * Since there is no way to print text on an Ximage directly, this function
 * copy directly the pixels from a font.
 *******************************************************************************/
void vout_SysPrint( vout_thread_t *p_vout, int i_x, int i_y, int i_halign, 
                    int i_valign, unsigned char *psz_text )
{
    int                 i_line;                    /* line in character matrix */
    int                 i_byte;               /* byte offset in character line */    
    int                 i_height;                          /* character height */    
    int                 i_char_bytes_per_line;         /* total bytes per line */
    int                 i_text_width;                      /* total text width */
    byte_t *            pi_pic;                                /* picture data */
    byte_t *            pi_char;                             /* character data */

    /* Update upper left coordinates according to alignment */
    i_text_width = p_vout->p_sys->i_char_interspacing * strlen( psz_text );    
    switch( i_halign )
    {
    case 0:                                                        /* centered */
        i_x -= i_text_width / 2;
        break;        
    case 1:                                                   /* right aligned */
        i_x -= i_text_width;
        break;                
    }
    switch( i_valign )
    {
    case 0:                                                        /* centered */
        i_y -= p_vout->p_sys->i_char_height / 2;
        break;        
    case 1:                                                   /* bottom aligned */
        i_y -= p_vout->p_sys->i_char_height;
        break;                
    }

    /* Copy used variables to local */
    i_height =                  p_vout->p_sys->i_char_height;
    i_char_bytes_per_line =     p_vout->p_sys->i_char_bytes_per_line;    

    /* Check that the text is in the screen vertically and horizontally */
    if( (i_y < 0) || (i_y + i_height > p_vout->i_height) || (i_x < 0) ||
        (i_x + i_text_width > p_vout->i_width) )
    {
        intf_DbgMsg("text '%s' would print outside the screen\n", psz_text);        
        return;        
    }    

    /* Print text */
    for( ; *psz_text != '\0'; psz_text++ )
    {
        /* Check that the character is valid and in the screen horizontally */
        if( (*psz_text >= VOUT_MIN_CHAR) && (*psz_text < VOUT_MAX_CHAR) )
        {       
            /* Select character */
            pi_char =   p_vout->p_sys->pi_font + (*psz_text - VOUT_MIN_CHAR) * 
                i_height * i_char_bytes_per_line;
            pi_pic =    p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ]->data +
                i_y * p_vout->i_bytes_per_line + i_x * p_vout->i_bytes_per_pixel;

            /* Copy character */
            for( i_line = 0; i_line < i_height; i_line++ )
            {
                /* Copy line */
                for( i_byte = 0; i_byte < i_char_bytes_per_line; i_byte++ )
                {
                    pi_pic[ i_byte  ] = *pi_char++;                                
                }
                
                /* Go to next line */
                pi_pic += p_vout->i_bytes_per_line;
            }
        }

        /* Jump to next character */
        i_x += p_vout->p_sys->i_char_interspacing;
    }
}

/* following functions are local */

/*******************************************************************************
 * X11OpenDisplay: open and initialize X11 device 
 *******************************************************************************
 * Create a window according to video output given size, and set other 
 * properties according to the display properties.
 *******************************************************************************/
static int X11OpenDisplay( vout_thread_t *p_vout, char *psz_display, Window root_window )
{
    /* Open display */
    p_vout->p_sys->p_display = XOpenDisplay( psz_display );
    if( p_vout->p_sys->p_display == NULL )
    {
        intf_ErrMsg("error: can't open display %s\n", psz_display );        
        return( 1 );        
    }

    /* Initialize structure */
    p_vout->p_sys->root_window  = root_window;
    p_vout->p_sys->b_shm        = (XShmQueryExtension(p_vout->p_sys->p_display) == True);
    p_vout->p_sys->i_screen     = DefaultScreen( p_vout->p_sys->p_display );
    if( !p_vout->p_sys->b_shm )
    {        
        intf_Msg("Video: XShm extension is not available\n");    
    }    

    /* Get the screen depth */
    p_vout->i_screen_depth = DefaultDepth( p_vout->p_sys->p_display, 
                                           p_vout->p_sys->i_screen );
    switch( p_vout->i_screen_depth )
    {
    case 15:                        /* 15 bpp (16bpp with a missing green bit) */
    case 16:                                          /* 16 bpp (65536 colors) */
        p_vout->i_bytes_per_pixel = 2;
        break;
    case 24:                                    /* 24 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 3;
        break;
    case 32:                                    /* 32 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 4;
        break;
    default:                                       /* unsupported screen depth */
        intf_ErrMsg("error: screen depth %d is not supported\n", 
                    p_vout->i_screen_depth);    
        XCloseDisplay( p_vout->p_sys->p_display );        
        return( 1  );
        break;
    }    

    /* Create a window */
    if( X11CreateWindow( p_vout ) )
    {
        intf_ErrMsg("error: can't open a window\n");        
        XCloseDisplay( p_vout->p_sys->p_display );        
        return( 1 );
    }

    /* Get font information */
    if( X11GetFont( p_vout ) )
    {
        intf_ErrMsg("error: can't read default font\n");
        X11DestroyWindow( p_vout );
        XCloseDisplay( p_vout->p_sys->p_display );
        return( 1 );        
    }

    return( 0 );    
}

/*******************************************************************************
 * X11CloseDisplay: close X11 device 
 *******************************************************************************
 * Returns all resources allocated by X11OpenDisplay and restore the original
 * state of the display.
 *******************************************************************************/
static void X11CloseDisplay( vout_thread_t *p_vout )
{
    // Free font info
    free( p_vout->p_sys->pi_font );    

    // Destroy window and close display
    X11DestroyWindow( p_vout );
    XCloseDisplay( p_vout->p_sys->p_display );    
}

/*******************************************************************************
 * X11GetFont: get default font bitmap informations
 *******************************************************************************
 * This function will convert a font into a bitmap for later use by the 
 * vout_SysPrint function.
 *******************************************************************************/
static int X11GetFont( vout_thread_t *p_vout )
{
    XFontStruct *       p_font_info;             /* font information structure */
    Pixmap              pixmap;              /* pixmap used to draw characters */
    GC                  gc;                                 /* graphic context */        
    XGCValues           gc_values;               /* graphic context properties */    
    XImage *            p_ximage;                      /* ximage for character */    
    unsigned char       i_char;                             /* character index */    
    int                 i_char_width;              /* character width (pixels) */
    int                 i_char_bytes;                  /* total character size */        
    
    /* Load font */
    p_font_info = XLoadQueryFont( p_vout->p_sys->p_display, "fixed" );
    if( p_font_info == NULL )
    {
        intf_ErrMsg("error: can't load 'fixed' font\n");
        return( 1 );        
    }
    
    /* Get character size */
    i_char_width =                              p_font_info->max_bounds.lbearing + 
        p_font_info->max_bounds.rbearing;
    p_vout->p_sys->i_char_bytes_per_line =      i_char_width * p_vout->i_bytes_per_pixel;    
    p_vout->p_sys->i_char_height =              p_font_info->max_bounds.ascent + 
        p_font_info->max_bounds.descent;
    i_char_bytes =                              p_vout->p_sys->i_char_bytes_per_line *
        p_vout->p_sys->i_char_height;    
    p_vout->p_sys->i_char_interspacing =        p_font_info->max_bounds.width;    

    /* Allocate font descriptor */
    p_vout->p_sys->pi_font = malloc( i_char_bytes * ( VOUT_MAX_CHAR - VOUT_MIN_CHAR ) );
    if( p_vout->p_sys->pi_font == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror( ENOMEM ) );
        XFreeFont( p_vout->p_sys->p_display, p_font_info );
        return( 1 );        
    }   

    /* Create drawable and graphic context */
    gc_values.foreground =      XBlackPixel( p_vout->p_sys->p_display, 
                                             p_vout->p_sys->i_screen );
    gc_values.background =      XBlackPixel( p_vout->p_sys->p_display, 
                                             p_vout->p_sys->i_screen );
    gc_values.font =            p_font_info->fid;    
    pixmap = XCreatePixmap( p_vout->p_sys->p_display, p_vout->p_sys->window,
                            i_char_width,
                            p_vout->p_sys->i_char_height *(VOUT_MAX_CHAR-VOUT_MIN_CHAR),
                            p_vout->i_screen_depth );    
    gc = XCreateGC( p_vout->p_sys->p_display, pixmap, 
                    GCForeground | GCBackground | GCFont, &gc_values );

    /* Clear pixmap and invert graphic context */
    XFillRectangle( p_vout->p_sys->p_display, pixmap, gc, 0, 0, i_char_width, 
                    p_vout->p_sys->i_char_height*(VOUT_MAX_CHAR-VOUT_MIN_CHAR) );    
    XSetForeground( p_vout->p_sys->p_display, gc, 
                    XWhitePixel( p_vout->p_sys->p_display, p_vout->p_sys->i_screen ) );
    XSetBackground( p_vout->p_sys->p_display, gc, 
                    XBlackPixel( p_vout->p_sys->p_display, p_vout->p_sys->i_screen ) );

    /* Copy characters bitmaps to font descriptor */
    for( i_char = VOUT_MIN_CHAR; i_char < VOUT_MAX_CHAR; i_char++ )
    {    
        XDrawString( p_vout->p_sys->p_display, pixmap, gc, 0,
                     p_font_info->max_bounds.ascent + 
                     (i_char-VOUT_MIN_CHAR) * p_vout->p_sys->i_char_height,
                     &i_char, 1 );
    }
    p_ximage = XGetImage( p_vout->p_sys->p_display, pixmap, 0, 0, i_char_width,
                          p_vout->p_sys->i_char_height*(VOUT_MAX_CHAR-VOUT_MIN_CHAR),
                          -1, ZPixmap );        
    memcpy( p_vout->p_sys->pi_font, p_ximage->data, 
            i_char_bytes*(VOUT_MAX_CHAR-VOUT_MIN_CHAR));        

    /* Free resources, unload font and return */        
    XDestroyImage( p_ximage ); 
    XFreeGC( p_vout->p_sys->p_display, gc );
    XFreePixmap( p_vout->p_sys->p_display, pixmap );
    XFreeFont( p_vout->p_sys->p_display, p_font_info );
    return( 0 );    
}

/*******************************************************************************
 * X11CreateWindow: create X11 vout window
 *******************************************************************************
 * The video output window will be created. Normally, this window is wether 
 * full screen or part of a parent window. Therefore, it does not need a 
 * title or other hints. Thery are still supplied in case the window would be
 * spawned as a standalone one by the interface.
 *******************************************************************************/
static int X11CreateWindow( vout_thread_t *p_vout )
{
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    XEvent                  xevent;
    boolean_t               b_expose;
    boolean_t               b_map_notify;    

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;         /* save the hidden part */
 
    /* Create the window and set hints */
    p_vout->p_sys->window = XCreateSimpleWindow( p_vout->p_sys->p_display,
						 p_vout->p_sys->root_window,
						 0, 0, 
						 p_vout->i_width, p_vout->i_height,
						 0, 0, 0);
    XSelectInput( p_vout->p_sys->p_display, p_vout->p_sys->window, 
                  ExposureMask | StructureNotifyMask );
    XChangeWindowAttributes( p_vout->p_sys->p_display, p_vout->p_sys->window, 
                             CWBackingStore, &xwindow_attributes);

    /* Creation of a graphic context that doesn't generate a GraphicsExpose event
       when using functions like XCopyArea */
    xgcvalues.graphics_exposures = False;    
    p_vout->p_sys->gc =  XCreateGC( p_vout->p_sys->p_display, p_vout->p_sys->window,
                                    GCGraphicsExposures, &xgcvalues);

    /* Send orders to server, and wait until window is displayed - two events
     * must be received: a MapNotify event, an Expose event allowing drawing in the
     * window */
    b_expose = 0;
    b_map_notify = 0;
    XMapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window);
    do
    {
        XNextEvent( p_vout->p_sys->p_display, &xevent);
        if( (xevent.type == Expose) 
            && (xevent.xexpose.window == p_vout->p_sys->window) )
        {
            b_expose = 1;
        }
        else if( (xevent.type == MapNotify) 
                 && (xevent.xmap.window == p_vout->p_sys->window) )
        {
            b_map_notify = 1;
        }
    }
    while( !( b_expose && b_map_notify ) );
    XSelectInput( p_vout->p_sys->p_display, p_vout->p_sys->window, 0 );

    /* At this stage, the window is openned, displayed, and ready to receive 
     * data */
    return( 0 );
}

/*******************************************************************************
 * X11DestroyWindow: destroy X11 window
 *******************************************************************************
 * Destroy an X11 window created by vout_X11CreateWindow
 *******************************************************************************/
static void X11DestroyWindow( vout_thread_t *p_vout )
{
    XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
    XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
}

/*******************************************************************************
 * X11CreateImage: create an XImage                                      
 *******************************************************************************
 * Create a simple XImage used as a buffer.
 *******************************************************************************/
static int X11CreateImage( vout_thread_t *p_vout, XImage **pp_ximage )
{
    byte_t *    pb_data;                            /* image data storage zone */
    int         i_quantum;                       /* XImage quantum (see below) */
  
    /* Allocate memory for image */
    p_vout->i_bytes_per_line = p_vout->i_width * p_vout->i_bytes_per_pixel;    
    pb_data = (byte_t *) malloc( p_vout->i_bytes_per_line * p_vout->i_height );
    if( !pb_data )                                                    /* error */
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        return( 1 );   
    }

    /* Optimize the quantum of a scanline regarding its size - the quantum is
       a diviser of the number of bits between the start of two scanlines. */
    if( !(( p_vout->i_bytes_per_line ) % 32) )
    {
        i_quantum = 32;
    }
    else    
    {
        if( !(( p_vout->i_bytes_per_line ) % 16) )
        {
            i_quantum = 16;
        }
        else
        {
            i_quantum = 8;
        }
    }
    
    /* Create XImage */
    *pp_ximage = XCreateImage( p_vout->p_sys->p_display, 
                               DefaultVisual(p_vout->p_sys->p_display, p_vout->p_sys->i_screen),
                               p_vout->i_screen_depth, ZPixmap, 0, pb_data, 
                               p_vout->i_width, p_vout->i_height, i_quantum, 0);
    if(! *pp_ximage )                                                 /* error */
    {
        intf_ErrMsg( "error: XCreateImage() failed\n" );
        free( pb_data );
        return( 1 );
    }

    return 0;
}

/*******************************************************************************
 * X11CreateShmImage: create an XImage using shared memory extension
 *******************************************************************************
 * Prepare an XImage for DisplayX11ShmImage function.
 * The order of the operations respects the recommandations of the mit-shm 
 * document by J.Corbet and K.Packard. Most of the parameters were copied from 
 * there.
 *******************************************************************************/
static int X11CreateShmImage( vout_thread_t *p_vout, XImage **pp_ximage, 
                              XShmSegmentInfo *p_shm_info)
{
    /* Create XImage */
    *pp_ximage = XShmCreateImage( p_vout->p_sys->p_display, 
                                  DefaultVisual(p_vout->p_sys->p_display, p_vout->p_sys->i_screen),
                                  p_vout->i_screen_depth, ZPixmap, 0, 
                                  p_shm_info, p_vout->i_width, p_vout->i_height );
    if(! *pp_ximage )                                                 /* error */
    {
        intf_ErrMsg("error: XShmCreateImage() failed\n");
        return( 1 );
    }

    /* Allocate shared memory segment - 0777 set the access permission
     * rights (like umask), they are not yet supported by X servers */
    p_shm_info->shmid = shmget( IPC_PRIVATE, 
                                (*pp_ximage)->bytes_per_line * (*pp_ximage)->height, 
                                IPC_CREAT | 0777);
    if( p_shm_info->shmid < 0)                                        /* error */
    {
        intf_ErrMsg("error: can't allocate shared image data (%s)\n",
                    strerror(errno));
        XDestroyImage( *pp_ximage );
        return( 1 );
    }

    /* Attach shared memory segment to process (read/write) */
    p_shm_info->shmaddr = (*pp_ximage)->data = shmat(p_shm_info->shmid, 0, 0);
    if(! p_shm_info->shmaddr )
    {                                                                 /* error */
        intf_ErrMsg("error: can't attach shared memory (%s)\n",
                    strerror(errno));
        shmctl( p_shm_info->shmid, IPC_RMID, 0 );        /* free shared memory */
        XDestroyImage( *pp_ximage );
        return( 1 );
    }

    /* Mark the shm segment to be removed when there will be no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm_info->shmid, IPC_RMID, 0 );

    /* Attach shared memory segment to X server (read only) */
    p_shm_info->readOnly = True;
    if( XShmAttach( p_vout->p_sys->p_display, p_shm_info ) == False )    /* error */
    {
        intf_ErrMsg("error: can't attach shared memory to X11 server\n");
        shmdt( p_shm_info->shmaddr );     /* detach shared memory from process
                                           * and automatic free                */
        XDestroyImage( *pp_ximage );
        return( 1 );
    }

    /* Send image to X server. This instruction is required, since having 
     * built a Shm XImage and not using it causes an error on XCloseDisplay */
    XFlush( p_vout->p_sys->p_display );    
    return( 0 );
}

/*******************************************************************************
 * X11DestroyImage: destroy an XImage                                  
 *******************************************************************************
 * Destroy XImage AND associated data. If pointer is NULL, the image won't be
 * destroyed (see vout_X11ManageOutputMethod())
 *******************************************************************************/
static void X11DestroyImage( XImage *p_ximage )
{
    if( p_ximage != NULL )
    {
        XDestroyImage( p_ximage );                       /* no free() required */
    }
}

/*******************************************************************************
 * X11DestroyShmImage                                                    
 *******************************************************************************
 * Destroy XImage AND associated data. Detach shared memory segment from
 * server and process, then free it. If pointer is NULL, the image won't be
 * destroyed (see vout_X11ManageOutputMethod()) 
 *******************************************************************************/
static void X11DestroyShmImage( vout_thread_t *p_vout, XImage *p_ximage, 
                                XShmSegmentInfo *p_shm_info )
{
    /* If pointer is NULL, do nothing */
    if( p_ximage == NULL )
    {
        return;
    }

    XShmDetach( p_vout->p_sys->p_display, p_shm_info );     /* detach from server */
    XDestroyImage( p_ximage );
    if( shmdt( p_shm_info->shmaddr ) )    /* detach shared memory from process */
    {                                     /* also automatic freeing...         */
        intf_ErrMsg("error: can't detach shared memory (%s)\n", 
                    strerror(errno));
    }
}

