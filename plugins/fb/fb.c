/*****************************************************************************
 * fb.c : framebuffer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: fb.c,v 1.14 2002/02/24 20:51:09 gbazin Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <signal.h>                                      /* SIGUSR1, SIGUSR2 */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <fcntl.h>                                                 /* open() */
#include <unistd.h>                                               /* close() */

#include <termios.h>                                       /* struct termios */
#include <sys/ioctl.h>
#include <sys/mman.h>                                              /* mmap() */

#include <linux/fb.h>
#include <linux/vt.h>                                                /* VT_* */
#include <linux/kd.h>                                                 /* KD* */

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list );

static int  vout_Create    ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static void vout_Render    ( vout_thread_t *, picture_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );
static int  vout_Manage    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );

static int  OpenDisplay    ( vout_thread_t * );
static void CloseDisplay   ( vout_thread_t * );
static void SwitchDisplay  ( int i_signal );
static void TextMode       ( int i_tty );
static void GfxMode        ( int i_tty );

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/
#define FB_DEV_VAR "fb_dev"

MODULE_CONFIG_START
ADD_CATEGORY_HINT( "Misc Options", NULL )
ADD_STRING  ( FB_DEV_VAR, "/dev/fb0", NULL,"framebuffer device",NULL )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "Linux console framebuffer module" )
    ADD_CAPABILITY( VOUT, 30 )
    ADD_SHORTCUT( "fb" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * vout_sys_t: video output framebuffer method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the FB specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* System informations */
    int                 i_tty;                          /* tty device handle */
    struct termios      old_termios;

    /* Original configuration informations */
    struct sigaction            sig_usr1;           /* USR1 previous handler */
    struct sigaction            sig_usr2;           /* USR2 previous handler */
    struct vt_mode              vt_mode;                 /* previous VT mode */

    /* Framebuffer information */
    int                         i_fd;                       /* device handle */
    struct fb_var_screeninfo    old_info;      /* original mode informations */
    struct fb_var_screeninfo    var_info;       /* current mode informations */
    boolean_t                   b_pan;     /* does device supports panning ? */
    struct fb_cmap              fb_cmap;                /* original colormap */
    u16                         *p_palette;              /* original palette */

    /* Video information */
    int i_width;
    int i_height;
    int i_bytes_per_pixel;

    /* Video memory */
    byte_t *    p_video;                                      /* base adress */
    size_t      i_page_size;                                    /* page size */

} vout_sys_t;

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_render     = vout_Render;
    p_function_list->functions.vout.pf_display    = vout_Display;
}

/*****************************************************************************
 * vout_Create: allocates FB video thread output method
 *****************************************************************************
 * This function allocates and initializes a FB vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    struct sigaction    sig_tty;                 /* sigaction for tty change */
    struct vt_mode      vt_mode;                          /* vt current mode */
    struct termios      new_termios;

    /* Allocate instance and initialize some members */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        return( 1 );
    };

    /* Set tty and fb devices */
    p_vout->p_sys->i_tty = 0;           /* 0 == /dev/tty0 == current console */

    GfxMode( p_vout->p_sys->i_tty );

    /* Set keyboard settings */
    if (tcgetattr(0, &p_vout->p_sys->old_termios) == -1)
    {
        intf_ErrMsg( "intf error: tcgetattr" );
    }

    if (tcgetattr(0, &new_termios) == -1)
    {
        intf_ErrMsg( "intf error: tcgetattr" );
    }

 /* new_termios.c_lflag &= ~ (ICANON | ISIG);
    new_termios.c_lflag |= (ECHO | ECHOCTL); */
    new_termios.c_lflag &= ~ (ICANON);
    new_termios.c_lflag &= ~(ECHO | ECHOCTL);
    new_termios.c_iflag = 0;
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;

    if (tcsetattr(0, TCSAFLUSH, &new_termios) == -1)
    {
        intf_ErrMsg( "intf error: tcsetattr" );
    }

    ioctl( p_vout->p_sys->i_tty, VT_RELDISP, VT_ACKACQ );

    /* Set-up tty signal handler to be aware of tty changes */
    memset( &sig_tty, 0, sizeof( sig_tty ) );
    sig_tty.sa_handler = SwitchDisplay;
    sigemptyset( &sig_tty.sa_mask );
    if( sigaction( SIGUSR1, &sig_tty, &p_vout->p_sys->sig_usr1 ) ||
        sigaction( SIGUSR2, &sig_tty, &p_vout->p_sys->sig_usr2 ) )
    {
        intf_ErrMsg( "intf error: can't set up signal handler (%s)",
                     strerror(errno) );
        tcsetattr(0, 0, &p_vout->p_sys->old_termios);
        TextMode( p_vout->p_sys->i_tty );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Set-up tty according to new signal handler */
    if( -1 == ioctl( p_vout->p_sys->i_tty,
                     VT_GETMODE, &p_vout->p_sys->vt_mode ) )
    {
        intf_ErrMsg( "intf error: cant get terminal mode (%s)",
                     strerror(errno) );
        sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
        sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );
        tcsetattr(0, 0, &p_vout->p_sys->old_termios);
        TextMode( p_vout->p_sys->i_tty );
        free( p_vout->p_sys );
        return( 1 );
    }
    memcpy( &vt_mode, &p_vout->p_sys->vt_mode, sizeof( vt_mode ) );
    vt_mode.mode   = VT_PROCESS;
    vt_mode.waitv  = 0;
    vt_mode.relsig = SIGUSR1;
    vt_mode.acqsig = SIGUSR2;

    if( -1 == ioctl( p_vout->p_sys->i_tty, VT_SETMODE, &vt_mode ) )
    {
        intf_ErrMsg( "intf error: can't set terminal mode (%s)",
                     strerror(errno) );
        sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
        sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );
        tcsetattr(0, 0, &p_vout->p_sys->old_termios);
        TextMode( p_vout->p_sys->i_tty );
        free( p_vout->p_sys );
        return( 1 );
    }

    if( OpenDisplay( p_vout ) )
    {
        ioctl( p_vout->p_sys->i_tty, VT_SETMODE, &p_vout->p_sys->vt_mode );
        sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
        sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );
        tcsetattr(0, 0, &p_vout->p_sys->old_termios);
        TextMode( p_vout->p_sys->i_tty );
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize framebuffer video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure: RGB with square pixels, whatever
     * the input format is, since it's the only format we know */
    switch( p_vout->p_sys->var_info.bits_per_pixel )
    {
        case 8: /* FIXME: set the palette */
            p_vout->output.i_chroma = FOURCC_BI_RGB; break;
        case 15:
            p_vout->output.i_chroma = FOURCC_RV15; break;
        case 16:
            p_vout->output.i_chroma = FOURCC_RV16; break;
        case 24:
            p_vout->output.i_chroma = FOURCC_BI_BITFIELDS; break;
        case 32:
            p_vout->output.i_chroma = FOURCC_BI_BITFIELDS; break;
        default:
            intf_ErrMsg( "vout error: unknown screen depth" );
            return 0;
    }

    p_vout->output.i_width = p_vout->p_sys->i_width;
    p_vout->output.i_height = p_vout->p_sys->i_height;

    /* Assume we have square pixels */
    p_vout->output.i_aspect = p_vout->p_sys->i_width
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->i_height;

    /* Clear the screen */
    memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size );

    /* Try to initialize 1 direct buffer */
    p_pic = NULL;

    /* Find an empty picture slot */
    for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
    {
        if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
        {
            p_pic = p_vout->p_picture + i_index;
            break;
        }
    }

    /* Allocate the picture */
    if( p_pic == NULL )
    {
        return 0;
    }

    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p->p_pixels = p_vout->p_sys->p_video;
    p_pic->p->i_pixel_bytes = p_vout->p_sys->i_bytes_per_pixel;
    p_pic->p->i_lines = p_vout->p_sys->var_info.yres;

    if( p_vout->p_sys->var_info.xres_virtual )
    {
        p_pic->p->b_margin = 1;
        p_pic->p->b_hidden = 1;
        p_pic->p->i_pitch = p_vout->p_sys->var_info.xres_virtual
                             * p_vout->p_sys->i_bytes_per_pixel;
        p_pic->p->i_visible_bytes = p_vout->p_sys->var_info.xres
                                     * p_vout->p_sys->i_bytes_per_pixel;
    }
    else
    {
        p_pic->p->b_margin = 0;
        p_pic->p->i_pitch = p_vout->p_sys->var_info.xres
                             * p_vout->p_sys->i_bytes_per_pixel;
    }

    /* Only useful for p_vout->p_sys->var_info.bits_per_pixel != 8 */
    p_pic->p->i_red_mask = ( (1 << p_vout->p_sys->var_info.red.length) - 1 )
                             << p_vout->p_sys->var_info.red.offset;
    p_pic->p->i_green_mask = ( (1 << p_vout->p_sys->var_info.green.length) - 1 )
                               << p_vout->p_sys->var_info.green.offset;
    p_pic->p->i_blue_mask = ( (1 << p_vout->p_sys->var_info.blue.length) - 1 )
                              << p_vout->p_sys->var_info.blue.offset;

    p_pic->i_planes = 1;

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

    I_OUTPUTPICTURES++;

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate framebuffer video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    /* Clear the screen */
    memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size );
}

/*****************************************************************************
 * vout_Destroy: destroy FB video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_CreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    CloseDisplay( p_vout );

    /* Reset the terminal */
    ioctl( p_vout->p_sys->i_tty, VT_SETMODE, &p_vout->p_sys->vt_mode );

    /* Remove signal handlers */
    sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
    sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );

    /* Reset the keyboard state */
    tcsetattr( 0, 0, &p_vout->p_sys->old_termios );

    /* Return to text mode */
    TextMode( p_vout->p_sys->i_tty );

    /* Destroy structure */
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle FB events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
#if 0
    u8 buf;

    if ( read(0, &buf, 1) == 1)
    {
        switch( buf )
        {
        case 'q':
            p_main->p_intf->b_die = 1;
            break;

        default:
            break;
        }
    }
#endif

    /*
     * Size change
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        intf_WarnMsg( 3, "vout: reinitializing framebuffer screen" );
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        /* Destroy XImages to change their size */
        vout_End( p_vout );

        /* Recreate XImages. If SysInit failed, the thread can't go on. */
        if( vout_Init( p_vout ) )
        {
            intf_ErrMsg("error: cannot reinit framebuffer screen" );
            return( 1 );
        }

        /* Clear screen */
        memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size );

#if 0
        /* Tell the video output thread that it will need to rebuild YUV
         * tables. This is needed since conversion buffer size may have changed */
        p_vout->i_changes |= VOUT_YUV_CHANGE;
#endif
    }

    return 0;
}

/*****************************************************************************
 * vout_Render: renders previously calculated output
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to FB image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* swap the two Y offsets if the drivers supports panning */
    if( p_vout->p_sys->b_pan )
    {
        p_vout->p_sys->var_info.yoffset = 0;
        //p_vout->p_sys->var_info.yoffset = p_vout->p_sys->var_info.yres;
   
        /* the X offset should be 0, but who knows ...
         * some other app might have played with the framebuffer */
        p_vout->p_sys->var_info.xoffset = 0;

        ioctl( p_vout->p_sys->i_fd,
               FBIOPAN_DISPLAY, &p_vout->p_sys->var_info );
    }
}

#if 0
static void vout_SetPalette( p_vout_thread_t p_vout,
                             u16 *red, u16 *green, u16 *blue, u16 *transp )
{
    struct fb_cmap cmap = { 0, 256, red, green, blue, transp };

    ioctl( p_vout->p_sys->i_fd, FBIOPUTCMAP, &cmap );
}
#endif

/* following functions are local */

/*****************************************************************************
 * OpenDisplay: initialize framebuffer
 *****************************************************************************/
static int OpenDisplay( vout_thread_t *p_vout )
{
    char *psz_device;                             /* framebuffer device path */
    struct fb_fix_screeninfo    fix_info;     /* framebuffer fix information */

    /* Open framebuffer device */
    if( !(psz_device = config_GetPszVariable( FB_DEV_VAR )) )
    {
        intf_ErrMsg( "vout error: don't know which fb device to open" );
        return( 1 );
    }

    p_vout->p_sys->i_fd = open( psz_device, O_RDWR);

    if( p_vout->p_sys->i_fd == -1 )
    {
        intf_ErrMsg("vout error: can't open %s (%s)",
                    psz_device, strerror(errno) );
        free( psz_device );
        return( 1 );
    }
    free( psz_device );

    /* Get framebuffer device informations */
    if( ioctl( p_vout->p_sys->i_fd,
               FBIOGET_VSCREENINFO, &p_vout->p_sys->var_info ) )
    {
        intf_ErrMsg("vout error: can't get fb info (%s)", strerror(errno) );
        close( p_vout->p_sys->i_fd );
        return( 1 );
    }

    if( ioctl( p_vout->p_sys->i_fd,
               FBIOGET_VSCREENINFO, &p_vout->p_sys->old_info ) )
    {
        intf_ErrMsg("vout error: can't get 2nd fb info (%s)", strerror(errno) );
        close( p_vout->p_sys->i_fd );
        return( 1 );
    }

    /* Set some attributes */
    p_vout->p_sys->var_info.activate = FB_ACTIVATE_NXTOPEN;
    p_vout->p_sys->var_info.xoffset =  0;
    p_vout->p_sys->var_info.yoffset =  0;

    if( ioctl( p_vout->p_sys->i_fd,
               FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info ) )
    {
        intf_ErrMsg(" vout error: can't set fb info (%s)", strerror(errno) );
        close( p_vout->p_sys->i_fd );
        return( 1 );
    }

    /* Get some informations again, in the definitive configuration */
    if( ioctl( p_vout->p_sys->i_fd, FBIOGET_FSCREENINFO, &fix_info )
         || ioctl( p_vout->p_sys->i_fd,
                   FBIOGET_VSCREENINFO, &p_vout->p_sys->var_info ) )
    {
        intf_ErrMsg( "vout error: can't get additional fb info (%s)",
                     strerror(errno) );

        /* Restore fb config */
        ioctl( p_vout->p_sys->i_fd,
               FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info );

        close( p_vout->p_sys->i_fd );
        return( 1 );
    }

    /* FIXME: if the image is full-size, it gets cropped on the left
     * because of the xres / xres_virtual slight difference */
    intf_WarnMsg( 3, "vout: %ix%i (virtual %ix%i)",
                  p_vout->p_sys->var_info.xres, p_vout->p_sys->var_info.yres,
                  p_vout->p_sys->var_info.xres_virtual,
                  p_vout->p_sys->var_info.yres_virtual );

    p_vout->p_sys->i_height = p_vout->p_sys->var_info.yres;
    p_vout->p_sys->i_width  = p_vout->p_sys->var_info.xres_virtual
                               ? p_vout->p_sys->var_info.xres_virtual
                               : p_vout->p_sys->var_info.xres;

    p_vout->p_sys->p_palette = NULL;
    p_vout->p_sys->b_pan = ( fix_info.ypanstep || fix_info.ywrapstep );

    switch( p_vout->p_sys->var_info.bits_per_pixel )
    {
    case 8:
        p_vout->p_sys->p_palette = malloc( 8 * 256 * sizeof( u16 ) );
        p_vout->p_sys->fb_cmap.start = 0;
        p_vout->p_sys->fb_cmap.len = 256;
        p_vout->p_sys->fb_cmap.red = p_vout->p_sys->p_palette;
        p_vout->p_sys->fb_cmap.green = p_vout->p_sys->p_palette + 256 * sizeof( u16 );
        p_vout->p_sys->fb_cmap.blue = p_vout->p_sys->p_palette + 2 * 256 * sizeof( u16 );
        p_vout->p_sys->fb_cmap.transp = p_vout->p_sys->p_palette + 3 * 256 * sizeof( u16 );

        /* Save the colormap */
        ioctl( p_vout->p_sys->i_fd, FBIOGETCMAP, &p_vout->p_sys->fb_cmap );

        p_vout->p_sys->i_bytes_per_pixel = 1;
        break;

    case 15:
    case 16:
        p_vout->p_sys->i_bytes_per_pixel = 2;
        break;

    case 24:
        p_vout->p_sys->i_bytes_per_pixel = 3;
        break;

    case 32:
        p_vout->p_sys->i_bytes_per_pixel = 4;
        break;

    default:
        intf_ErrMsg( "vout error: screen depth %d is not supported",
                     p_vout->p_sys->var_info.bits_per_pixel );

        /* Restore fb config */
        ioctl( p_vout->p_sys->i_fd,
               FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info );

        close( p_vout->p_sys->i_fd );
        return 1;
    }

    p_vout->p_sys->i_page_size = p_vout->p_sys->i_width *
                p_vout->p_sys->i_height * p_vout->p_sys->i_bytes_per_pixel;

    /* Map a framebuffer at the beginning */
    p_vout->p_sys->p_video = mmap( 0, p_vout->p_sys->i_page_size,
                                   PROT_READ | PROT_WRITE, MAP_SHARED,
                                   p_vout->p_sys->i_fd, 0 );

    if( p_vout->p_sys->p_video == ((void*)-1) )
    {
        intf_ErrMsg( "vout error: can't map video memory (%s)",
                     strerror(errno) );

        if( p_vout->p_sys->var_info.bits_per_pixel == 8 )
        {
            free( p_vout->p_sys->p_palette );
        }

        /* Restore fb config */
        ioctl( p_vout->p_sys->i_fd,
               FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info );

        close( p_vout->p_sys->i_fd );
        return( 1 );
    }

    intf_WarnMsg( 4, "vout: framebuffer type=%d, visual=%d, ypanstep=%d, "
                     "ywrap=%d, accel=%d", fix_info.type, fix_info.visual,
                     fix_info.ypanstep, fix_info.ywrapstep, fix_info.accel );
    return( 0 );
}

/*****************************************************************************
 * CloseDisplay: terminate FB video thread output method
 *****************************************************************************/
static void CloseDisplay( vout_thread_t *p_vout )
{
    /* Clear display */
    memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size );

    /* Restore palette */
    if( p_vout->p_sys->var_info.bits_per_pixel == 8 );
    {
        ioctl( p_vout->p_sys->i_fd,
               FBIOPUTCMAP, &p_vout->p_sys->fb_cmap );
        free( p_vout->p_sys->p_palette );
    }

    /* Restore fb config */
    ioctl( p_vout->p_sys->i_fd,
           FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info );

    /* Close fb */
    close( p_vout->p_sys->i_fd );
}

/*****************************************************************************
 * SwitchDisplay: VT change signal handler
 *****************************************************************************
 * This function activates or deactivates the output of the thread. It is
 * called by the VT driver, on terminal change.
 *****************************************************************************/
static void SwitchDisplay(int i_signal)
{
#if 0
    vout_thread_t *p_vout;

    vlc_mutex_lock( &p_vout_bank->lock );

    /* XXX: only test the first video output */
    if( p_vout_bank->i_count )
    {
        p_vout = p_vout_bank->pp_vout[0];

        switch( i_signal )
        {
        case SIGUSR1:                                /* vt has been released */
            p_vout->b_active = 0;
            ioctl( p_vout->p_sys->i_tty, VT_RELDISP, 1 );
            break;
        case SIGUSR2:                                /* vt has been acquired */
            p_vout->b_active = 1;
            ioctl( p_vout->p_sys->i_tty, VT_RELDISP, VT_ACTIVATE );
            /* handle blanking */
            vlc_mutex_lock( &p_vout->change_lock );
            p_vout->i_changes |= VOUT_SIZE_CHANGE;
            vlc_mutex_unlock( &p_vout->change_lock );
            break;
        }
    }

    vlc_mutex_unlock( &p_vout_bank->lock );
#endif
}

/*****************************************************************************
 * TextMode and GfxMode : switch tty to text/graphic mode
 *****************************************************************************
 * These functions toggle the tty mode.
 *****************************************************************************/
static void TextMode( int i_tty )
{
    /* return to text mode */
    if( -1 == ioctl(i_tty, KDSETMODE, KD_TEXT) )
    {
        intf_ErrMsg( "intf error: failed ioctl KDSETMODE KD_TEXT" );
    }
}

static void GfxMode( int i_tty )
{
    /* switch to graphic mode */
    if( -1 == ioctl(i_tty, KDSETMODE, KD_GRAPHICS) )
    {
        intf_ErrMsg( "intf error: failed ioctl KDSETMODE KD_GRAPHICS" );
    }
}

