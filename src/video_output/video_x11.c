/*******************************************************************************
 * vout_x11.c: X11 video output display method
 * (c)1998 VideoLAN
 *******************************************************************************
 * The X11 method for video output thread. It's properties (and the vout_x11_t
 * type) are defined in vout.h. The functions declared here should not be
 * needed by any other module than vout.c.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/shm.h>
#include <sys/soundcard.h>
#include <sys/uio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "xutils.h"

#include "input.h"
#include "input_vlan.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"
#include "video_x11.h"

#include "xconsole.h"
#include "interface.h"
#include "intf_msg.h"

#include "pgm_data.h"

/*
 * Local prototypes
 */
static int  X11CheckConfiguration   ( video_cfg_t *p_cfg );

static int  X11OpenDisplay          ( vout_thread_t *p_vout );
static int  X11CreateWindow         ( vout_thread_t *p_vout );
static int  X11CreateImages         ( vout_thread_t *p_vout );
static void X11DestroyImages        ( vout_thread_t *p_vout );
static void X11DestroyWindow        ( vout_thread_t *p_vout );
static void X11CloseDisplay         ( vout_thread_t *p_vout );
static int  X11CreateImage          ( vout_thread_t *p_vout, XImage **pp_ximage );
static void X11DestroyImage         ( XImage *p_ximage );
static int  X11CreateShmImage       ( vout_thread_t *p_vout, XImage **pp_ximage, 
                                      XShmSegmentInfo *p_shm_info );
static void X11DestroyShmImage      ( vout_thread_t *p_vout, XImage *p_ximage, 
                                      XShmSegmentInfo *p_shm_info );

static vout_render_blank_t  X11RenderRGBBlank;
static vout_render_blank_t  X11RenderPixelBlank8bpp;
static vout_render_blank_t  X11RenderPixelBlank16bpp;
static vout_render_blank_t  X11RenderPixelBlank24bpp;
static vout_render_blank_t  X11RenderPixelBlank32bpp;
static vout_render_line_t   X11RenderRGBLine8bpp;
static vout_render_line_t   X11RenderRGBLine16bpp;
static vout_render_line_t   X11RenderRGBLine24bpp;
static vout_render_line_t   X11RenderRGBLine32bpp;
static vout_render_line_t   X11RenderPixelLine8bpp;
static vout_render_line_t   X11RenderPixelLine16bpp;
static vout_render_line_t   X11RenderPixelLine24bpp;
static vout_render_line_t   X11RenderPixelLine32bpp;
static vout_render_line_t   X11RenderRGBMaskLine;
static vout_render_line_t   X11RenderPixelMaskLine8bpp;
static vout_render_line_t   X11RenderPixelMaskLine16bpp;
static vout_render_line_t   X11RenderPixelMaskLine24bpp;
static vout_render_line_t   X11RenderPixelMaskLine32bpp;
/* ?? YUV types */

/*******************************************************************************
 * vout_X11AllocOutputMethod: allocate X11 video thread output method
 *******************************************************************************
 * This function creates a X11 output method descriptor in the vout_thread_t
 * desriptor and initialize it.
 * Following configuration properties are used:
 *  VIDEO_CFG_DISPLAY    display used
 *  VIDEO_CFG_TITLE      window title
 *  VIDEO_CFG_SHM_EXT    try to use XShm extension
 *******************************************************************************/
int vout_X11AllocOutputMethod( vout_thread_t *p_vout, video_cfg_t *p_cfg )
{
    /* Check configuration */
    if( X11CheckConfiguration(p_cfg) )
    {
        return( 1 );
    }

    /* Allocate descriptor */
    p_vout->p_x11 = (vout_x11_t *) malloc( sizeof( vout_x11_t ) );
    if( p_vout->p_x11 == NULL )
    {
        intf_ErrMsg("vout error 101-1: cannot allocate X11 method descriptor: %s\n",
                    strerror(errno));        
        return( 1 );        
    }   

    /* Initialize fields - string passed in configuration structure are copied
     * since they can be destroyed at any moment - remember that NULL is a valid
     * value for psz_display */
    p_vout->p_x11->psz_title = (char *) malloc( strlen(p_cfg->psz_title) + 1);
    if( p_vout->p_x11->psz_title == NULL )
    {
        free( p_vout->p_x11 );            
        return( 1 );
    }        
    strcpy( p_vout->p_x11->psz_title, p_cfg->psz_title );    
    if( p_cfg->psz_display != NULL ) 
    {
        p_vout->p_x11->psz_display = (char *) malloc( strlen(p_cfg->psz_display) + 1);
        if( p_vout->p_x11->psz_display == NULL )
        {
            free( p_vout->p_x11->psz_title );            
            free( p_vout->p_x11 );            
            return( 1 );
        }        
        strcpy( p_vout->p_x11->psz_display, p_cfg->psz_display );    
    }
    else
    {
        p_vout->p_x11->psz_display = NULL;        
    }
    p_vout->p_x11->b_shm_ext = p_cfg->b_shm_ext;
    return( 0 );    
}

/*******************************************************************************
 * vout_X11FreeOutputMethod: free X11 video thread output method
 *******************************************************************************
 * Free an X11 video thread output method allocated by 
 * vout_X11AllocOutputMethod()
 *******************************************************************************/
void vout_X11FreeOutputMethod( vout_thread_t *p_vout )
{
    if( p_vout->p_x11->psz_display != NULL )
    {
        free( p_vout->p_x11->psz_display );        
    }
    free( p_vout->p_x11->psz_title );        
    free( p_vout->p_x11 );    
}

/*******************************************************************************
 * vout_X11CreateOutputMethod: create X11 video thread output method
 *******************************************************************************
 * This function opens a display and create a X11 window according to the user
 * configuration.
 *******************************************************************************/
int vout_X11CreateOutputMethod( vout_thread_t *p_vout )
{
    if( X11OpenDisplay( p_vout ) )                             /* open display */
    {
        free( p_vout->p_x11 );        
        return( 1 );
    }
    if( X11CreateWindow( p_vout ) )                           /* create window */
    {
        X11CloseDisplay( p_vout );
        free( p_vout->p_x11 );        
        return( 1 );
    }
    if( X11CreateImages( p_vout ) )                           /* create images */
    {
        X11DestroyWindow( p_vout );
        X11CloseDisplay( p_vout );
        free( p_vout->p_x11 );
        return( 1 );        
    }
    intf_DbgMsg("%p -> success, depth=%d bpp, XShm=%d\n", 
                p_vout, p_vout->i_screen_depth, p_vout->p_x11->b_shm_ext);
    return( 0 );
}

/*******************************************************************************
 * vout_X11DestroyOutputMethod: destroy X11 video thread output method
 *******************************************************************************
 * Destroys an output method created by vout_X11CreateOutputMethod
 *******************************************************************************
 * Messages type: vout, major code: 102
 *******************************************************************************/
void vout_X11DestroyOutputMethod( vout_thread_t *p_vout )
{
    X11DestroyImages( p_vout );
    X11DestroyWindow( p_vout );
	X11CloseDisplay( p_vout );
    intf_DbgMsg("%p\n", p_vout );
}

/*******************************************************************************
 * vout_X11ManageOutputMethod: handle X11 events
 *******************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a negative value if 
 * something happened which does not allow the thread to continue, and a 
 * positive one if the thread can go on, but the images have been modified and
 * therefore it is useless to display them.
 *******************************************************************************
 * Messages type: vout, major code: 103
 *******************************************************************************/
int vout_X11ManageOutputMethod( vout_thread_t *p_vout )
{
    XEvent      xevent;                                           /* X11 event */
    boolean_t   b_resized;                          /* window has been resized */
    
    /* Handle X11 events: ConfigureNotify events are parsed to know if the
     * output window's size changed, MapNotify and UnmapNotify to know if the
     * window is mapped (and if the display is usefull), and ClientMessages
     * to intercept window destruction requests */
    b_resized = 0;
    while( XCheckWindowEvent( p_vout->p_x11->p_display, p_vout->p_x11->window,
                              StructureNotifyMask, &xevent ) == True )
    {      
        /* ConfigureNotify event: prepare  */
        if( (xevent.type == ConfigureNotify)  
            && ((xevent.xconfigure.width != p_vout->i_width)
                || (xevent.xconfigure.height != p_vout->i_height)) )
        {
            /* Update dimensions */
            b_resized = 1;
            p_vout->i_width = xevent.xconfigure.width;
            p_vout->i_height = xevent.xconfigure.height;
        }
        /* MapNotify event: change window status and disable screen saver */
        else if( (xevent.type == MapNotify) && !p_vout->b_active )
        {
            XDisableScreenSaver( p_vout->p_x11->p_display );       
            p_vout->b_active = 1;
        }
        /* UnmapNotify event: change window status and enable screen saver */
        else if( (xevent.type == UnmapNotify) && p_vout->b_active )
        {
            XEnableScreenSaver( p_vout->p_x11->p_display );       
            p_vout->b_active = 0;            
        }        
        /* ClientMessage event - only WM_PROTOCOLS with WM_DELETE_WINDOW data
         * are handled - according to the man pages, the format is always 32 
         * in this case */
        else if( (xevent.type == ClientMessage)
/*                 && (xevent.xclient.message_type == p_vout->p_x11->wm_protocols) 
                 && (xevent.xclient.data.l[0] == p_vout->p_x11->wm_delete_window )*/ )
        {
            intf_DbgMsg("******* ClientMessage ******\n");
            
            /* ?? this never happens :( */
            return( -1 );
        }
#ifdef DEBUG
        /* Other event */
        else
        {            
            intf_DbgMsg("%p -> unhandled event type %d received\n", p_vout, xevent.type );
        }        
#endif
    }

    /* If window has been resized, re-create images */
    if( b_resized )
    {
        intf_DbgMsg("%p -> resizing window\n", p_vout);
        X11DestroyImages( p_vout );
        if( X11CreateImages( p_vout ) )
        {
            /* A fatal error occured: images could not be re-created. Note
             * that in this case, the images pointers will be NULL, so the
             * image destructor will know it does not need to destroy them. */
            return( -1 );
        }
        return( 1 );        
    }

    return( 0 );
}

/*******************************************************************************
 * vout_X11DisplayOutput: displays previously rendered output
 *******************************************************************************
 * This function send the currently rendered image to X11 server, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *******************************************************************************
 * Messages type: vout, major code: 105
 *******************************************************************************/
void vout_X11DisplayOutput( vout_thread_t *p_vout )
{
    if( p_vout->p_x11->b_shm_ext)                                 /* XShm is used */
    {
        /* Display rendered image using shared memory extension */
        XShmPutImage(p_vout->p_x11->p_display, p_vout->p_x11->window, p_vout->p_x11->gc, 
                     p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ], 
                     0, 0, 0, 0,  
                     p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->width,  
                     p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->height, True);

        /* Send the order to the X server */
        XFlush(p_vout->p_x11->p_display);
        
        /* ?? wait until effective display ? */
/*        do XNextEvent(Display_Ptr, &xev);
        while(xev.type!=CompletionType);*/
    }
    else                                  /* regular X11 capabilities are used */
    {
        XPutImage(p_vout->p_x11->p_display, p_vout->p_x11->window, p_vout->p_x11->gc, 
                  p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ], 
                  0, 0, 0, 0,  
                  p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->width,  
                  p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->height);
        /* Send the order to the X server */
        XFlush(p_vout->p_x11->p_display);   /* ?? not needed ? */
    }

    /* Swap buffers */
    p_vout->p_x11->i_buffer_index = ++p_vout->p_x11->i_buffer_index & 1;
}

/* following functions are local */

/*******************************************************************************
 * X11CheckConfiguration: check configuration.
 *******************************************************************************
 * Set default parameters where required. In DEBUG mode, check if configuration
 * is valid.
 *******************************************************************************
 * Messages type: vout, major code: 116
 *******************************************************************************/
static int  X11CheckConfiguration( video_cfg_t *p_cfg )
{
    /* Window dimensions */
    if( !( p_cfg->i_properties & VIDEO_CFG_WIDTH ) )
    {
        p_cfg->i_width = VOUT_WIDTH;
    }
    if( !( p_cfg->i_properties & VIDEO_CFG_HEIGHT ) )
    {
        p_cfg->i_height = VOUT_HEIGHT;
    }
 
    /* Display */
    if( !( p_cfg->i_properties & VIDEO_CFG_DISPLAY ) )
    {
        p_cfg->psz_display = NULL;
    }

    /* Window title */
    if( !( p_cfg->i_properties & VIDEO_CFG_TITLE ) )
    {
        p_cfg->psz_title = VOUT_TITLE;
    }

    /* Use of XShm extension */
    if( !( p_cfg->i_properties & VIDEO_CFG_SHM_EXT ) )
    {
        p_cfg->b_shm_ext = VOUT_SHM_EXT;
    }

    return( 0 );
}

/*******************************************************************************
 * X11OpenDisplay: open X11 display
 *******************************************************************************
 * Opens an X11 display and set up pictures rendering functions according to
 * screen depth.
 * Following configuration properties are used:
 *  VIDEO_CFG_DISPLAY    display used
 *  VIDEO_CFG_SHM_EXT    try to use XShm extension
 *******************************************************************************
 * Messages type: vout, major code: 106
 *******************************************************************************/
static int X11OpenDisplay( vout_thread_t *p_vout )
{
    char *psz_display;

    /* Open display, using display name provided or default one */
    psz_display = XDisplayName( p_vout->p_x11->psz_display );
    p_vout->p_x11->p_display = XOpenDisplay( psz_display );
    if( !p_vout->p_x11->p_display )                                      /* error */
    {
        intf_ErrMsg("vout error 106-1: can't open display %s\n", psz_display );
       return( 1 );               
    }

    /* Check if XShm extension is wished and supported */
    /* ?? in old client, we checked if display was local or not - is it really
     * needed ? */
    if( p_vout->p_x11->b_shm_ext )
    {
        p_vout->p_x11->b_shm_ext = (XShmQueryExtension(p_vout->p_x11->p_display) == True);
    }
    else
    {
        p_vout->p_x11->b_shm_ext = 0;
    }

    /* Get the screen number and depth (bpp) - select functions depending 
     * of this value. i_bytes_per_pixel is required since on some hardware,
     * depth as 15bpp are used, which can cause problems with later memory
     * allocation. */
    p_vout->p_x11->i_screen = DefaultScreen( p_vout->p_x11->p_display );
    p_vout->i_screen_depth = DefaultDepth( p_vout->p_x11->p_display, 
                                           p_vout->p_x11->i_screen );
    switch( p_vout->i_screen_depth )
    {
    case 8:                                              /* 8 bpp (256 colors) */        
        p_vout->i_bytes_per_pixel = 1;
        p_vout->RenderRGBBlank = X11RenderRGBBlank;
        p_vout->RenderPixelBlank = X11RenderPixelBlank8bpp;
        p_vout->RenderRGBLine = X11RenderRGBLine8bpp;
        p_vout->RenderPixelLine = X11RenderPixelLine8bpp;
        p_vout->RenderRGBMaskLine = X11RenderRGBMaskLine;
        p_vout->RenderPixelMaskLine = X11RenderPixelMaskLine8bpp;                
        /*
        Process_Frame=Dither_Frame;
        Process_Top_Field=Dither_Top_Field;
        Process_Bottom_Field=Dither_Bottom_Field;
        Process_Top_Field420=Dither_Top_Field420;
        Process_Bottom_Field420=Dither_Bottom_Field420; ?? */
        break;

    case 15:                        /* 15 bpp (16bpp with a missing green bit) */
        p_vout->i_bytes_per_pixel = 2;              
        /*
         ?? */
        p_vout->RenderRGBBlank = X11RenderRGBBlank;
        p_vout->RenderPixelBlank = X11RenderPixelBlank16bpp;
        p_vout->RenderRGBLine = X11RenderRGBLine16bpp;
        p_vout->RenderPixelLine = X11RenderPixelLine16bpp;
        p_vout->RenderRGBMaskLine = X11RenderRGBMaskLine;
        p_vout->RenderPixelMaskLine = X11RenderPixelMaskLine16bpp;        
        /* ?? probably a 16bpp with differenr convertion functions - just map
         * functions, then switch to 16bpp */
        break;

    case 16:                                          /* 16 bpp (65536 colors) */
        p_vout->i_bytes_per_pixel = 2;
        p_vout->RenderRGBBlank = X11RenderRGBBlank;
        p_vout->RenderPixelBlank = X11RenderPixelBlank16bpp;
        p_vout->RenderRGBLine = X11RenderRGBLine16bpp;
        p_vout->RenderPixelLine = X11RenderPixelLine16bpp;
        p_vout->RenderRGBMaskLine = X11RenderRGBMaskLine;
        p_vout->RenderPixelMaskLine = X11RenderPixelMaskLine16bpp;               
       /*
        Process_Frame=Translate_Frame;
        Process_Top_Field=Translate_Top_Field;
        Process_Bottom_Field=Translate_Bottom_Field;
        Process_Top_Field420=Translate_Top_Field420;
        Process_Bottom_Field420=Translate_Bottom_Field420; ?? */
        break;

    case 24:                                    /* 24 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 3;
        p_vout->RenderRGBBlank = X11RenderRGBBlank;
        p_vout->RenderPixelBlank = X11RenderPixelBlank24bpp;
        p_vout->RenderRGBLine = X11RenderRGBLine24bpp;
        p_vout->RenderPixelLine = X11RenderPixelLine24bpp;
        p_vout->RenderRGBMaskLine = X11RenderRGBMaskLine;
        p_vout->RenderPixelMaskLine = X11RenderPixelMaskLine24bpp;        
        /*
        Process_Frame=Translate_Frame;
        Process_Top_Field=Translate_Top_Field;
        Process_Bottom_Field=Translate_Bottom_Field;
        Process_Top_Field420=Translate_Top_Field420;
        Process_Bottom_Field420=Translate_Bottom_Field420; ?? */
        break;

    case 32:                                    /* 32 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 4;
        p_vout->RenderRGBBlank = X11RenderRGBBlank;
        p_vout->RenderPixelBlank = X11RenderPixelBlank32bpp;
        p_vout->RenderRGBLine = X11RenderRGBLine32bpp;
        p_vout->RenderPixelLine = X11RenderPixelLine32bpp;
        p_vout->RenderRGBMaskLine = X11RenderRGBMaskLine;
        p_vout->RenderPixelMaskLine = X11RenderPixelMaskLine32bpp;
        /*
        Process_Frame=Translate_Frame;
        Process_Top_Field=Translate_Top_Field;
        Process_Bottom_Field=Translate_Bottom_Field;
        Process_Top_Field420=Translate_Top_Field420;
        Process_Bottom_Field420=Translate_Bottom_Field420; ?? */
        break;

    default:                                       /* unsupported screen depth */
        intf_ErrMsg("vout error 106-2: screen depth %i is not supported\n", 
                    p_vout->i_screen_depth);
        XCloseDisplay( p_vout->p_x11->p_display );
        return( 1  );
        break;
    }
    return( 0 ); 
}

/*******************************************************************************
 * X11CreateWindow: create X11 window
 *******************************************************************************
 * Create and set-up the output window.
 * Following configuration properties are used:
 *  VIDEO_CFG_WIDTH      window width
 *  VIDEO_CFG_HEIGHT     window height 
 *  VIDEO_CFG_TITLE      window title
 *******************************************************************************
 * Messages type: vout, major code: 107
 *******************************************************************************/
static int X11CreateWindow( vout_thread_t *p_vout )
{
    XSizeHints              xsize_hints;
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    XEvent                  xevent;
    boolean_t               b_expose;
    boolean_t               b_configure_notify;
    boolean_t               b_map_notify;    

    /* Prepare window manager hints and properties */
    xsize_hints.base_width =            p_vout->i_width;
    xsize_hints.base_height =           p_vout->i_height;
    xsize_hints.flags =                 PSize;
    p_vout->p_x11->wm_protocols =       XInternAtom( p_vout->p_x11->p_display, "WM_PROTOCOLS", True );    
    p_vout->p_x11->wm_delete_window =   XInternAtom( p_vout->p_x11->p_display, "WM_DELETE_WINDOW", True );
    /* ?? add icons and placement hints ? */

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;         /* save the hidden part */  
 
    /* Create the window and set hints - the window must receive ConfigureNotify
     * events, and, until it is displayed, Expose and MapNotify events. */
    p_vout->p_x11->window = XCreateSimpleWindow( p_vout->p_x11->p_display,
                                              DefaultRootWindow( p_vout->p_x11->p_display ),
                                              0, 0, 
                                              p_vout->i_width, p_vout->i_height,
                                              0, 0, 0);
    XSelectInput( p_vout->p_x11->p_display, p_vout->p_x11->window, 
                  ExposureMask | StructureNotifyMask );
    XChangeWindowAttributes( p_vout->p_x11->p_display, p_vout->p_x11->window, 
                             CWBackingStore, &xwindow_attributes);

    /* Set window manager hints and properties: size hints, command, window's name,
     * and accepted protocols */
    XSetWMNormalHints( p_vout->p_x11->p_display, p_vout->p_x11->window, &xsize_hints );
    XSetCommand( p_vout->p_x11->p_display, p_vout->p_x11->window, 
                 p_program_data->ppsz_argv, p_program_data->i_argc );    
    XStoreName( p_vout->p_x11->p_display, p_vout->p_x11->window, p_vout->p_x11->psz_title );
    if( (p_vout->p_x11->wm_protocols == None)          /* use WM_DELETE_WINDOW */
        || (p_vout->p_x11->wm_delete_window == None)
        || !XSetWMProtocols( p_vout->p_x11->p_display, p_vout->p_x11->window, 
                             &p_vout->p_x11->wm_delete_window, 1 ) )
    {
        /* WM_DELETE_WINDOW is not supported by window manager */
        intf_Msg("vout: missing or bad window manager - please exit program kindly.\n");
    }
    
    /* Creation of a graphic context that doesn't generate a GraphicsExpose event
       when using functions like XCopyArea */
    xgcvalues.graphics_exposures = False;    
    p_vout->p_x11->gc =  XCreateGC( p_vout->p_x11->p_display, p_vout->p_x11->window,
                                    GCGraphicsExposures, &xgcvalues);

    /* Create color system */
    /*?? if( CreateX11Colors( p_vout ) )
    {
        intf_ErrMsg("vout error 107-1: can't initialize color system\n");
        XCloseDisplay( p_vout->p_x11->p_display );
        return( - 1 );
    }*/
   
    /* Send orders to server, and wait until window is displayed - three events
     * must be received: a MapNotify event, an Expose event allowing drawing in the
     * window, and a ConfigureNotify to get the window dimensions. Once those events
     * have been received, only ConfigureNotify events need to be received. */   
    b_expose = 0;
    b_configure_notify = 0;
    b_map_notify = 0;
    XMapWindow( p_vout->p_x11->p_display, p_vout->p_x11->window);
    do
    {
        XNextEvent( p_vout->p_x11->p_display, &xevent);
        if( (xevent.type == Expose) 
            && (xevent.xexpose.window == p_vout->p_x11->window) )
        {
            b_expose = 1;
        }
        else if( (xevent.type == MapNotify) 
                 && (xevent.xmap.window == p_vout->p_x11->window) )
        {
            b_map_notify = 1;
        }
        else if( (xevent.type == ConfigureNotify) 
                 && (xevent.xconfigure.window == p_vout->p_x11->window) )
        {
            b_configure_notify = 1;
            p_vout->i_width = xevent.xconfigure.width;
            p_vout->i_height = xevent.xconfigure.height;       
        }
    }
    while( !( b_expose && b_configure_notify && b_map_notify ) );
    XSelectInput( p_vout->p_x11->p_display, p_vout->p_x11->window, StructureNotifyMask );

    /* At this stage, the window is openned, displayed, and ready to receive data */
    p_vout->b_active = 1;    
    return( 0 );
}

/*******************************************************************************
 * X11CreateImages: create X11 rendering buffers
 *******************************************************************************
 * Create two XImages which will be used as rendering buffers. On error, non 0
 * will be returned and the images pointer will be set to NULL (see 
 * vout_X11ManageOutputMethod()).
 *******************************************************************************
 * Messages type: vout, major code: 108
 *******************************************************************************/
static int X11CreateImages( vout_thread_t *p_vout )
{
    int i_err;

    /* Create XImages using XShm extension - on failure, fall back to regular 
     * way (and destroy the first image if it was created successfully) */
    if( p_vout->p_x11->b_shm_ext )
    {
        /* Create first image */
        i_err = X11CreateShmImage( p_vout, &p_vout->p_x11->p_ximage[0], 
                                   &p_vout->p_x11->shm_info[0] );
        if( !i_err )                           /* first image has been created */
        {
            /* Create second image */
            if( X11CreateShmImage( p_vout, &p_vout->p_x11->p_ximage[1], 
                                   &p_vout->p_x11->shm_info[1] ) )
            {                               /* error creating the second image */
                X11DestroyShmImage( p_vout, p_vout->p_x11->p_ximage[0], 
                                    &p_vout->p_x11->shm_info[0] );
                i_err = 1;
            }
        }
        if( i_err )                                        /* an error occured */
        {                        
            intf_Msg("vout: XShm extension desactivated\n" );
            p_vout->p_x11->b_shm_ext = 0;
        }
    }

    /* Create XImages without XShm extension */
    if( !p_vout->p_x11->b_shm_ext )
    {
        if( X11CreateImage( p_vout, &p_vout->p_x11->p_ximage[0] ) )
        {
            intf_Msg("vout error 108-1: can't create images\n");
            p_vout->p_x11->p_ximage[0] = NULL;
            p_vout->p_x11->p_ximage[1] = NULL;
            return( -1 );
        }
        if( X11CreateImage( p_vout, &p_vout->p_x11->p_ximage[1] ) )
        {
            intf_Msg("vout error 108-2: can't create images\n");
            X11DestroyImage( p_vout->p_x11->p_ximage[0] );
            p_vout->p_x11->p_ximage[0] = NULL;
            p_vout->p_x11->p_ximage[1] = NULL;
            return( -1 );
        }
    }

    /* Set buffer index to 0 */
    p_vout->p_x11->i_buffer_index = 0;

    return( 0 );
}

/*******************************************************************************
 * X11DestroyImages: destroy X11 rendering buffers
 *******************************************************************************
 * Destroy buffers created by vout_X11CreateImages().
 *******************************************************************************
 * Messages type: vout, major code: 109
 *******************************************************************************/
static void X11DestroyImages( vout_thread_t *p_vout )
{
    if( p_vout->p_x11->b_shm_ext )                              /* Shm XImages... */
    {
        X11DestroyShmImage( p_vout, p_vout->p_x11->p_ximage[0], 
                            &p_vout->p_x11->shm_info[0] );
		X11DestroyShmImage( p_vout, p_vout->p_x11->p_ximage[1], 
                            &p_vout->p_x11->shm_info[1] );
	}
	else                                              /* ...or regular XImages */
	{
		X11DestroyImage( p_vout->p_x11->p_ximage[0] );
		X11DestroyImage( p_vout->p_x11->p_ximage[1] );
	}
}

/*******************************************************************************
 * X11DestroyWindow: destroy X11 window
 *******************************************************************************
 * Destroy an X11 window created by vout_X11CreateWindow
 *******************************************************************************
 * Messages type: vout, major code: 110
 *******************************************************************************/
static void X11DestroyWindow( vout_thread_t *p_vout )
{
    XUnmapWindow( p_vout->p_x11->p_display, p_vout->p_x11->window );
/*  ??  DestroyX11Colors( p_vout );  */         
    /* ?? no more valid: colormap */
/*    if( p_vout->p_x11->b_private_colormap )
    {   
        XFreeColormap( p_vout->p_x11->p_display, p_vout->p_x11->private_colormap);
    }    */
	XFreeGC( p_vout->p_x11->p_display, p_vout->p_x11->gc );
    XDestroyWindow( p_vout->p_x11->p_display, p_vout->p_x11->window );
}

/*******************************************************************************
 * X11CloseDisplay: close X11 display
 *******************************************************************************
 * Close an X11 display openned by vout_X11OpenDisplay().
 *******************************************************************************
 * Messages type: vout, major code: 111
 *******************************************************************************/
static void X11CloseDisplay( vout_thread_t *p_vout )
{
    XCloseDisplay( p_vout->p_x11->p_display );                   /* close display */
}

/*******************************************************************************
 * X11CreateImage: create an XImage                                      
 *******************************************************************************
 * Messages type: vout, major code 112
 *******************************************************************************/
static int X11CreateImage( vout_thread_t *p_vout, XImage **pp_ximage )
{
    byte_t *    pb_data;                            /* image data storage zone */
    int         i_quantum;                       /* XImage quantum (see below) */
  
    /* Allocate memory for image */
    pb_data = (byte_t *) malloc( p_vout->i_bytes_per_pixel
                                 * p_vout->i_width 
                                 * p_vout->i_height );
    if( !pb_data )                                                    /* error */
    {
        intf_ErrMsg("vout error 112-1: %s\n", strerror(ENOMEM));
        return( -1 );   
    }

    /* Optimize the quantum of a scanline regarding its size - the quantum is
       a diviser of the number of bits between the start of two scanlines. */
    if( !(( p_vout->i_width * p_vout->i_bytes_per_pixel ) % 32) )
    {
        i_quantum = 32;
    }
    else    
    {
        if( !(( p_vout->i_width * p_vout->i_bytes_per_pixel ) % 16) )
        {
            i_quantum = 16;
        }
        else
        {
            i_quantum = 8;
        }
    }
    
    /* Create XImage */
    *pp_ximage = XCreateImage( p_vout->p_x11->p_display, 
                               DefaultVisual(p_vout->p_x11->p_display, p_vout->p_x11->i_screen),
                               p_vout->i_screen_depth, ZPixmap, 0, pb_data, 
                               p_vout->i_width, p_vout->i_height, i_quantum, 0);
    if(! *pp_ximage )                                                 /* error */
    {
        intf_ErrMsg( "vout error 112-2: XCreateImage() failed\n" );
        free( pb_data );
        return( -1 );
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
 * ?? error on failure:
 * X Error of failed request:  BadAccess (attempt to access private resource denied)
 *  Major opcode of failed request:  129 (MIT-SHM)
 *  Minor opcode of failed request:  1 (X_ShmAttach)
 *  Serial number of failed request:  17
 *  Current serial number in output stream:  18         
 *******************************************************************************
 * Messages type: vout, major code 113
 *******************************************************************************/
static int X11CreateShmImage( vout_thread_t *p_vout, XImage **pp_ximage, 
                              XShmSegmentInfo *p_shm_info)
{
    /* Create XImage */
    *pp_ximage = XShmCreateImage( p_vout->p_x11->p_display, 
                                  DefaultVisual(p_vout->p_x11->p_display, p_vout->p_x11->i_screen),
                                  p_vout->i_screen_depth, ZPixmap, 0, 
                                  p_shm_info, p_vout->i_width, p_vout->i_height );
    if(! *pp_ximage )                                                 /* error */
    {
        intf_ErrMsg("vout error 113-1: XShmCreateImage() failed\n");
        return( -1 );
    }

    /* Allocate shared memory segment - 0777 set the access permission
     * rights (like umask), they are not yet supported by X servers */
    p_shm_info->shmid = shmget( IPC_PRIVATE, 
                                (*pp_ximage)->bytes_per_line * (*pp_ximage)->height, 
                                IPC_CREAT | 0777);
    if( p_shm_info->shmid < 0)                                        /* error */
    {
        intf_ErrMsg("vout error 113-2: can't allocate shared image data (%s)\n",
                    strerror(errno));
        XDestroyImage( *pp_ximage );
        return( -1 );
    }

    /* Attach shared memory segment to process (read/write) */
    p_shm_info->shmaddr = (*pp_ximage)->data = shmat(p_shm_info->shmid, 0, 0);
    if(! p_shm_info->shmaddr )
    {                                                                 /* error */
        intf_ErrMsg("vout error 113-3: can't attach shared memory (%s)\n",
                    strerror(errno));
        shmctl( p_shm_info->shmid, IPC_RMID, 0 );        /* free shared memory */
        XDestroyImage( *pp_ximage );
        return( -1 );
    }

    /* Mark the shm segment to be removed when there will be no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm_info->shmid, IPC_RMID, 0 );

    /* Attach shared memory segment to X server (read only) */
    p_shm_info->readOnly = True;
    if( XShmAttach( p_vout->p_x11->p_display, p_shm_info ) == False )    /* error */
    {
        intf_ErrMsg("vout error 113-4: can't attach shared memory to server\n");
        shmdt( p_shm_info->shmaddr );     /* detach shared memory from process
                                           * and automatic free                */
        XDestroyImage( *pp_ximage );
        return( -1 );
    }

    /* ?? don't know what it is. Function XShmGetEventBase prototype is defined
     * in mit-shm document, but does not appears in any header. */
    p_vout->p_x11->i_completion_type = XShmGetEventBase(p_vout->p_x11->p_display) + ShmCompletion;

    return( 0 );
}

/*******************************************************************************
 * X11DestroyImage: destroy an XImage                                  
 *******************************************************************************
 * Destroy XImage AND associated data. If pointer is NULL, the image won't be
 * destroyed (see vout_X11ManageOutputMethod())
 *******************************************************************************
 * Messages type: vout, major code 114
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
 *******************************************************************************
 * Messages type: vout, major code 115
 *******************************************************************************/
static void X11DestroyShmImage( vout_thread_t *p_vout, XImage *p_ximage, 
                                XShmSegmentInfo *p_shm_info )
{
    /* If pointer is NULL, do nothing */
    if( p_ximage == NULL )
    {
        return;
    }

    XShmDetach( p_vout->p_x11->p_display, p_shm_info );     /* detach from server */
    XDestroyImage( p_ximage );
    if( shmdt( p_shm_info->shmaddr ) )    /* detach shared memory from process */
    {                                     /* also automatic freeing...         */
        intf_ErrMsg("vout error 115-1: can't detach shared memory (%s)\n", 
                    strerror(errno));
    }
}

/* following functions are local rendering functions */

/*******************************************************************************
 * X11RenderRGBBlank: RGB blank picture rendering function
 *******************************************************************************
 * Render a blank picture. Opposed to other rendering function, this one is
 * picture-based and not line-based. Dimensions sent as parameters are effective
 * dimensions of the rectangle to draw.
 *******************************************************************************
 * Messages type: vout, major code: ???
 *******************************************************************************/
static void X11RenderRGBBlank( vout_thread_t *p_vout, pixel_t pixel, 
                               int i_x, int i_y, int i_width, int i_height )
{
    /* ?? convert rgb->pixel */
    /* ?? call p_vout->RenderPixelBlank */
}

/*******************************************************************************
 * X11RenderPixelBlank*: pixel blank picture rendering functions
 *******************************************************************************
 * Render a blank picture. Opposed to other rendering function, this one is
 * picture-based and not line-based. Dimensions sent as parameters are effective
 * dimensions of the rectangle to draw.
 *******************************************************************************
 * Messages type: vout, major code: 117
 *******************************************************************************/
static void X11RenderPixelBlank8bpp( vout_thread_t *p_vout, pixel_t pixel, 
                                     int i_x, int i_y, int i_width, int i_height )
{
    int         i_line;                                        /* current line */
    int         i_bytes_per_line;                     /* XImage bytes per line */
    byte_t *    p_data;                                         /* XImage data */
    
    i_bytes_per_line = p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->bytes_per_line;
    p_data = p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->data + i_bytes_per_line * i_y;
    
    for( i_line = i_y; i_line < i_y + i_height; i_line++, p_data += i_bytes_per_line )
    {
        memset( p_data + i_x, pixel, i_width - i_x );
    }
}

static void X11RenderPixelBlank16bpp( vout_thread_t *p_vout, pixel_t pixel, 
                                      int i_x, int i_y, int i_width, int i_height )
{
    int         i_line;                                        /* current line */
    int         i_pixel;                                       /* pixel offset */
    int         i_bytes_per_line;                     /* XImage bytes per line */
    byte_t *    p_data;                                         /* XImage data */
    
    i_bytes_per_line = p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->bytes_per_line;
    p_data = p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->data + i_bytes_per_line * i_y;
    
    for( i_line = i_y; i_line < i_y + i_height; i_line++, p_data += i_bytes_per_line )
    {
        for( i_pixel = 0; i_pixel < i_width; i_pixel++ )
        {
            ((u16 *)p_data)[ i_x + i_pixel ] = pixel;
        }
        break;
    }
}

static void X11RenderPixelBlank24bpp( vout_thread_t *p_vout, pixel_t pixel, 
                                      int i_x, int i_y, int i_width, int i_height )
{
    int         i_line;                                        /* current line */
    int         i_pixel;                                       /* pixel offset */
    int         i_bytes_per_line;                     /* XImage bytes per line */
    byte_t *    p_data;                                         /* XImage data */
    
    i_bytes_per_line = p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->bytes_per_line;
    p_data = p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->data + i_bytes_per_line * i_y;
    
    for( i_line = i_y; i_line < i_y + i_height; i_line++, p_data += i_bytes_per_line )
    {
        for( i_pixel = 0; i_pixel < i_width; i_pixel++ )
        {
            *(u32 *)(p_data + (i_x + i_pixel) * 3) |= pixel;
        }
    }    
}

static void X11RenderPixelBlank32bpp( vout_thread_t *p_vout, pixel_t pixel, 
                                      int i_x, int i_y, int i_width, int i_height )
{
    int         i_line;                                        /* current line */
    int         i_pixel;                                       /* pixel offset */
    int         i_bytes_per_line;                     /* XImage bytes per line */
    byte_t *    p_data;                                         /* XImage data */
    
    i_bytes_per_line = p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->bytes_per_line;
    p_data = p_vout->p_x11->p_ximage[ p_vout->p_x11->i_buffer_index ]->data + i_bytes_per_line * i_y;
    
    for( i_line = i_y; i_line < i_y + i_height; i_line++, p_data += i_bytes_per_line )
    {
        for( i_pixel = 0; i_pixel < i_width; i_pixel++ )
        {
            ((u32 *)p_data)[ i_x + i_pixel ] = pixel;
        }
        break;
    }
}

/*******************************************************************************
 * X11RenderRGBLine*: RGB picture rendering functions
 *******************************************************************************
 * Render a 24bpp RGB-encoded picture line.
 *******************************************************************************
 * Messages type: vout, major code: 118
 *******************************************************************************/
static void X11RenderRGBLine8bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                  int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                  int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

static void X11RenderRGBLine16bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                   int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                   int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

static void X11RenderRGBLine24bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                   int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                   int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

static void X11RenderRGBLine32bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                   int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                   int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

/*******************************************************************************
 * X11RenderPixelLine*: pixel picture rendering functions
 *******************************************************************************
 * Render a pixel-encoded picture line.
 *******************************************************************************
 * Messages type: vout, major code: 119
 *******************************************************************************/
static void X11RenderPixelLine8bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                    int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                    int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

static void X11RenderPixelLine16bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                     int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                     int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

static void X11RenderPixelLine24bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                     int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                     int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

static void X11RenderPixelLine32bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                     int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                     int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

/*******************************************************************************
 * X11RenderRGBMaskLine: mask picture rendering function
 *******************************************************************************
 * Render a 1bpp RGB mask-encoded picture line.
 *******************************************************************************
 * Messages type: vout, major code: 120
 *******************************************************************************/
static void X11RenderRGBMaskLine( vout_thread_t *p_vout, picture_t *p_pic,
                                  int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                  int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

/*******************************************************************************
 * X11RenderPixelMaskLine: mask picture rendering functions
 *******************************************************************************
 * Render a 1bpp pixel mask-encoded picture line.
 *******************************************************************************
 * Messages type: vout, major code: 121
 *******************************************************************************/
static void X11RenderPixelMaskLine8bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                        int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                        int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

static void X11RenderPixelMaskLine16bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                         int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                         int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

static void X11RenderPixelMaskLine24bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                         int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                         int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}

static void X11RenderPixelMaskLine32bpp( vout_thread_t *p_vout, picture_t *p_pic,
                                         int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                         int i_width, int i_line_width, int i_ratio )
{
    /* ?? */
}
