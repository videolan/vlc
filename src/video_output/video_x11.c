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
#include <stdlib.h>
#include <string.h>
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

    /* Color maps and translation tables - some of those tables are shifted,
     * see x11.c for more informations. */
    u8 *                trans_16bpp_red;           /* red (16 bpp) (SHIFTED !) */
    u8 *                trans_16bpp_green;       /* green (16 bpp) (SHIFTED !) */
    u8 *                trans_16bpp_blue;         /* blue (16 bpp) (SHIFTED !) */

    /* Display buffers and shared memory information */
    int                 i_buffer_index;                        /* buffer index */
    XImage *            p_ximage[2];                         /* XImage pointer */   
    XShmSegmentInfo     shm_info[2];         /* shared memory zone information */

    int                 i_completion_type;                               /* ?? */
} vout_sys_t;

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/
static int  X11GetProperties        ( vout_thread_t *p_vout );
static int  X11CreateWindow         ( vout_thread_t *p_vout );
static int  X11CreateImages         ( vout_thread_t *p_vout );
static void X11DestroyImages        ( vout_thread_t *p_vout );
static void X11DestroyWindow        ( vout_thread_t *p_vout );
static int  X11CreateImage          ( vout_thread_t *p_vout, XImage **pp_ximage );
static void X11DestroyImage         ( XImage *p_ximage );
static int  X11CreateShmImage       ( vout_thread_t *p_vout, XImage **pp_ximage, 
                                      XShmSegmentInfo *p_shm_info );
static void X11DestroyShmImage      ( vout_thread_t *p_vout, XImage *p_ximage, 
                                      XShmSegmentInfo *p_shm_info );


/*******************************************************************************
 * vout_SysCreate: allocate X11 video thread output method
 *******************************************************************************
 * This function allocate and initialize a X11 vout method.
 *******************************************************************************/
int vout_SysCreate( vout_thread_t *p_vout, char *psz_display, Window root_window )
{
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );    
    if( p_vout->p_sys == NULL )
    {   
        return( 1 );
        
    }    

    /* Since XLib is usually not thread-safe, we can't use the same display
     * pointer than the interface or another thread. However, the window
     * id is still valid */
    p_vout->p_sys->p_display = XOpenDisplay( psz_display );
    if( !p_vout->p_sys->p_display )                               /* error */
    {
        intf_ErrMsg("vout error: can't open display %s\n", psz_display );
        free( p_vout->p_sys );
        return( 1 );               
    }
        
    p_vout->p_sys->root_window = root_window;
    return( 0 );
}

/*******************************************************************************
 * vout_SysInit: initialize X11 video thread output method
 *******************************************************************************
 * This function create a X11 window according to the user configuration.
 *******************************************************************************/
int vout_SysInit( vout_thread_t *p_vout )
{
    if( X11GetProperties( p_vout ) )                      /* get display properties */
    {
	return( 1 );
    }
    if( X11CreateWindow( p_vout ) )                           /* create window */
    {
        return( 1 );
    }
    if( X11CreateImages( p_vout ) )                           /* create images */
    {
        X11DestroyWindow( p_vout );
        return( 1 );        
    }
    intf_DbgMsg("%p -> success, depth=%d bpp, Shm=%d\n", 
                p_vout, p_vout->i_screen_depth, p_vout->p_sys->b_shm );
    return( 0 );
}

/*******************************************************************************
 * vout_SysEnd: terminate X11 video thread output method
 *******************************************************************************
 * Terminate an output method created by vout_X11CreateOutputMethod
 *******************************************************************************/
void vout_SysEnd( vout_thread_t *p_vout )
{
    X11DestroyImages( p_vout );
    X11DestroyWindow( p_vout );
    intf_DbgMsg("%p\n", p_vout );
}

/*******************************************************************************
 * vout_SysDestroy: destroy X11 video thread output method
 *******************************************************************************
 * Terminate an output method created by vout_X11CreateOutputMethod
 *******************************************************************************/
void vout_SysDestroy( vout_thread_t *p_vout )
{
    XCloseDisplay( p_vout->p_sys->p_display );    
    free( p_vout->p_sys );
}

/*******************************************************************************
 * vout_SysManage: handle X11 events
 *******************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a negative value if 
 * something happened which does not allow the thread to continue, and a 
 * positive one if the thread can go on, but the images have been modified and
 * therefore it is useless to display them.
 *******************************************************************************
 * Messages type: vout, major code: 103
 *******************************************************************************/
int vout_SysManage( vout_thread_t *p_vout )
{
    boolean_t b_resized;
    //??

    /* ?? this function should not receive any usefull X11 messages, since they
     * have tobe treated by the main interface window - check it. */
    return 0; //??


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
 * vout_SysDisplay: displays previously rendered output
 *******************************************************************************
 * This function send the currently rendered image to X11 server, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *******************************************************************************/
void vout_SysDisplay( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_shm)                                     /* XShm is used */
    {
        /* Display rendered image using shared memory extension */
        XShmPutImage(p_vout->p_sys->p_display, p_vout->p_sys->window, p_vout->p_sys->gc, 
                     p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ], 
                     0, 0, 0, 0,  
                     p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ]->width,  
                     p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ]->height, True);

        /* Send the order to the X server */
        XFlush(p_vout->p_sys->p_display);
        
        /* ?? wait until effective display ? */
/*        do XNextEvent(Display_Ptr, &xev);
        while(xev.type!=CompletionType);*/
    }
    else                                  /* regular X11 capabilities are used */
    {
        XPutImage(p_vout->p_sys->p_display, p_vout->p_sys->window, p_vout->p_sys->gc, 
                  p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ], 
                  0, 0, 0, 0,  
                  p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ]->width,  
                  p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ]->height);

        /* Send the order to the X server */
        XFlush(p_vout->p_sys->p_display);       /* ?? not needed ? */
    }

    /* Swap buffers */
    p_vout->p_sys->i_buffer_index = ++p_vout->p_sys->i_buffer_index & 1;
}

/* following functions are local */

/*******************************************************************************
 * X11GetProperties: get properties of a given display
 *******************************************************************************
 * Opens an X11 display and try to detect usable X extensions.
 *******************************************************************************/
static int X11GetProperties( vout_thread_t *p_vout )
{
    /* Check if extensions are supported */
    p_vout->p_sys->b_shm = VOUT_XSHM && (XShmQueryExtension(p_vout->p_sys->p_display) == True);

    /* Get the screen number and depth (bpp) - select functions depending 
     * of this value. i_bytes_per_pixel is required since on some hardware,
     * depth as 15bpp are used, which can cause problems with later memory
     * allocation. */
    p_vout->p_sys->i_screen =   DefaultScreen( p_vout->p_sys->p_display );
    p_vout->i_screen_depth = DefaultDepth( p_vout->p_sys->p_display, 
                                           p_vout->p_sys->i_screen );
    switch( p_vout->i_screen_depth )
    {
    case 15:                        /* 15 bpp (16bpp with a missing green bit) */
        p_vout->i_bytes_per_pixel = 2;              
        /*
         ?? */
        break;

    case 16:                                          /* 16 bpp (65536 colors) */
        p_vout->i_bytes_per_pixel = 2;
       /*
        Process_Frame=Translate_Frame;
        Process_Top_Field=Translate_Top_Field;
        Process_Bottom_Field=Translate_Bottom_Field;
        Process_Top_Field420=Translate_Top_Field420;
        Process_Bottom_Field420=Translate_Bottom_Field420; ?? */
        break;

    case 24:                                    /* 24 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 3;

        /*
        Process_Frame=Translate_Frame;
        Process_Top_Field=Translate_Top_Field;
        Process_Bottom_Field=Translate_Bottom_Field;
        Process_Top_Field420=Translate_Top_Field420;
        Process_Bottom_Field420=Translate_Bottom_Field420; ?? */
        break;

    case 32:                                    /* 32 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 4;
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
        return( 1  );
        break;
    }
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
 
    /* Create the window and set hints - the window must receive ConfigureNotify
     * events, and, until it is displayed, Expose and MapNotify events. */
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
            intf_Msg("vout: XShm extension desactivated\n" );
            p_vout->p_sys->b_shm = 0;
        }
    }

    /* Create XImages without XShm extension */
    if( !p_vout->p_sys->b_shm )
    {
        if( X11CreateImage( p_vout, &p_vout->p_sys->p_ximage[0] ) )
        {
            intf_Msg("vout error 108-1: can't create images\n");
            p_vout->p_sys->p_ximage[0] = NULL;
            p_vout->p_sys->p_ximage[1] = NULL;
            return( -1 );
        }
        if( X11CreateImage( p_vout, &p_vout->p_sys->p_ximage[1] ) )
        {
            intf_Msg("vout error 108-2: can't create images\n");
            X11DestroyImage( p_vout->p_sys->p_ximage[0] );
            p_vout->p_sys->p_ximage[0] = NULL;
            p_vout->p_sys->p_ximage[1] = NULL;
            return( -1 );
        }
    }

    /* Set buffer index to 0 */
    p_vout->p_sys->i_buffer_index = 0;

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
    if( p_vout->p_sys->b_shm )                              /* Shm XImages... */
    {
        X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[0], 
                            &p_vout->p_sys->shm_info[0] );
		X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[1], 
                            &p_vout->p_sys->shm_info[1] );
	}
	else                                              /* ...or regular XImages */
	{
		X11DestroyImage( p_vout->p_sys->p_ximage[0] );
		X11DestroyImage( p_vout->p_sys->p_ximage[1] );
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
    intf_DbgMsg("vout window: 0x%x\n", p_vout->p_sys->window );
    XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
    XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
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
    *pp_ximage = XCreateImage( p_vout->p_sys->p_display, 
                               DefaultVisual(p_vout->p_sys->p_display, p_vout->p_sys->i_screen),
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
    *pp_ximage = XShmCreateImage( p_vout->p_sys->p_display, 
                                  DefaultVisual(p_vout->p_sys->p_display, p_vout->p_sys->i_screen),
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
    if( XShmAttach( p_vout->p_sys->p_display, p_shm_info ) == False )    /* error */
    {
        intf_ErrMsg("vout error 113-4: can't attach shared memory to server\n");
        shmdt( p_shm_info->shmaddr );     /* detach shared memory from process
                                           * and automatic free                */
        XDestroyImage( *pp_ximage );
        return( -1 );
    }

    /* ?? don't know what it is. Function XShmGetEventBase prototype is defined
     * in mit-shm document, but does not appears in any header. */
    p_vout->p_sys->i_completion_type = XShmGetEventBase(p_vout->p_sys->p_display) + ShmCompletion;

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

    XShmDetach( p_vout->p_sys->p_display, p_shm_info );     /* detach from server */
    XDestroyImage( p_ximage );
    if( shmdt( p_shm_info->shmaddr ) )    /* detach shared memory from process */
    {                                     /* also automatic freeing...         */
        intf_ErrMsg("vout error 115-1: can't detach shared memory (%s)\n", 
                    strerror(errno));
    }
}

/* following functions are local rendering functions */

