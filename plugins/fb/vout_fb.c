/*****************************************************************************
 * vout_fb.c: framebuffer video output display method
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000, 2001 VideoLAN
 * $Id: vout_fb.c,v 1.10 2001/03/21 13:42:33 sam Exp $
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

#define MODULE_NAME fb
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

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

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "modules.h"

#include "video.h"
#include "video_output.h"

#include "intf_msg.h"
#include "main.h"

/*****************************************************************************
 * vout_sys_t: video output framebuffer method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the FB specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* System informations */
    int                 i_tty_dev;                      /* tty device handle */
    struct termios      old_termios;

    /* Original configuration informations */
    struct sigaction            sig_usr1;           /* USR1 previous handler */
    struct sigaction            sig_usr2;           /* USR2 previous handler */
    struct vt_mode              vt_mode;                 /* previous VT mode */

    /* Framebuffer information */
    int                         i_fb_dev;                   /* device handle */
    struct fb_var_screeninfo    old_info;      /* original mode informations */
    struct fb_var_screeninfo    var_info;       /* current mode informations */
    boolean_t                   b_pan;     /* does device supports panning ? */
    struct fb_cmap              fb_cmap;                /* original colormap */
    u16                         *fb_palette;             /* original palette */

    /* Video memory */
    byte_t *                    p_video;                      /* base adress */
    size_t                      i_page_size;                    /* page size */

} vout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s * );
static void vout_SetPalette( p_vout_thread_t p_vout, u16 *red, u16 *green,
                             u16 *blue, u16 *transp );

static int  FBOpenDisplay  ( struct vout_thread_s * );
static void FBCloseDisplay ( struct vout_thread_s * );
static void FBSwitchDisplay( int i_signal );
static void FBTextMode     ( int i_tty_dev );
static void FBGfxMode      ( int i_tty_dev );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = vout_SetPalette;
}

/*****************************************************************************
 * vout_Probe: probe the video driver and return a score
 *****************************************************************************
 * This function tries to open the framebuffer and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    int i_fd;

    if( TestMethod( VOUT_METHOD_VAR, "fb" ) )
    {
        return( 999 );
    }

    i_fd = open( main_GetPszVariable( VOUT_FB_DEV_VAR,
                                      VOUT_FB_DEV_DEFAULT ), O_RDWR );
    if( i_fd == -1 )
    {
        return( 0 );
    }
    close( i_fd );

    return( 30 );
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
    p_vout->p_sys->i_tty_dev = 0;       /* 0 == /dev/tty0 == current console */

    FBGfxMode( p_vout->p_sys->i_tty_dev );

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

    ioctl(p_vout->p_sys->i_tty_dev, VT_RELDISP, VT_ACKACQ);

    /* Set-up tty signal handler to be aware of tty changes */
    memset( &sig_tty, 0, sizeof( sig_tty ) );
    sig_tty.sa_handler = FBSwitchDisplay;
    sigemptyset( &sig_tty.sa_mask );
    if( sigaction( SIGUSR1, &sig_tty, &p_vout->p_sys->sig_usr1 ) ||
        sigaction( SIGUSR2, &sig_tty, &p_vout->p_sys->sig_usr2 ) )
    {
        intf_ErrMsg( "intf error: can't set up signal handler (%s)",
                     strerror(errno) );
        tcsetattr(0, 0, &p_vout->p_sys->old_termios);
        FBTextMode( p_vout->p_sys->i_tty_dev );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Set-up tty according to new signal handler */
    if( ioctl(p_vout->p_sys->i_tty_dev, VT_GETMODE, &p_vout->p_sys->vt_mode)
        == -1 )
    {
        intf_ErrMsg( "intf error: cant get terminal mode (%s)",
                     strerror(errno) );
        sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
        sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );
        tcsetattr(0, 0, &p_vout->p_sys->old_termios);
        FBTextMode( p_vout->p_sys->i_tty_dev );
        free( p_vout->p_sys );
        return( 1 );
    }
    memcpy( &vt_mode, &p_vout->p_sys->vt_mode, sizeof( vt_mode ) );
    vt_mode.mode   = VT_PROCESS;
    vt_mode.waitv  = 0;
    vt_mode.relsig = SIGUSR1;
    vt_mode.acqsig = SIGUSR2;

    if( ioctl(p_vout->p_sys->i_tty_dev, VT_SETMODE, &vt_mode) == -1 )
    {
        intf_ErrMsg( "intf error: can't set terminal mode (%s)",
                     strerror(errno) );
        sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
        sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );
        tcsetattr(0, 0, &p_vout->p_sys->old_termios);
        FBTextMode( p_vout->p_sys->i_tty_dev );
        free( p_vout->p_sys );
        return( 1 );
    }

    if( FBOpenDisplay( p_vout ) )
    {
        ioctl(p_vout->p_sys->i_tty_dev, VT_SETMODE, &p_vout->p_sys->vt_mode);
        sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
        sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );
        tcsetattr(0, 0, &p_vout->p_sys->old_termios);
        FBTextMode( p_vout->p_sys->i_tty_dev );
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
    /* Clear the screen */
    memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size * 2 );

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate framebuffer video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    /* Clear the screen */
    memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size * 2 );
}

/*****************************************************************************
 * vout_Destroy: destroy FB video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_CreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    FBCloseDisplay( p_vout );

    /* Reset the terminal */
    ioctl(p_vout->p_sys->i_tty_dev, VT_SETMODE, &p_vout->p_sys->vt_mode);

    /* Remove signal handlers */
    sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
    sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );

    /* Reset the keyboard state */
    tcsetattr( 0, 0, &p_vout->p_sys->old_termios );

    /* Return to text mode */
    FBTextMode( p_vout->p_sys->i_tty_dev );

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
        intf_WarnMsg( 1, "vout: reinitializing framebuffer screen" );
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
        memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size * 2 );

#if 1
        /* Tell the video output thread that it will need to rebuild YUV
         * tables. This is needed since conversion buffer size may have changed */
        p_vout->i_changes |= VOUT_YUV_CHANGE;
#endif
    }

    return 0;
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to FB image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout )
{
    /* swap the two Y offsets if the drivers supports panning */
    if( p_vout->p_sys->b_pan )
    {
        p_vout->p_sys->var_info.yoffset =
            p_vout->i_buffer_index ? p_vout->p_sys->var_info.yres : 0;
   
        /* the X offset should be 0, but who knows ...
         * some other app might have played with the framebuffer */
        p_vout->p_sys->var_info.xoffset = 0;

        ioctl( p_vout->p_sys->i_fb_dev,
               FBIOPAN_DISPLAY, &p_vout->p_sys->var_info );
    }
}

/*****************************************************************************
 * vout_SetPalette: sets an 8 bpp palette
 *****************************************************************************
 * This function sets the palette given as an argument. It does not return
 * anything, but could later send information on which colors it was unable
 * to set.
 *****************************************************************************/
static void vout_SetPalette( p_vout_thread_t p_vout,
                             u16 *red, u16 *green, u16 *blue, u16 *transp )
{
    struct fb_cmap cmap = { 0, 256, red, green, blue, transp };

    ioctl( p_vout->p_sys->i_fb_dev, FBIOPUTCMAP, &cmap );
}

/* following functions are local */

/*****************************************************************************
 * FBOpenDisplay: initialize framebuffer
 *****************************************************************************/
static int FBOpenDisplay( vout_thread_t *p_vout )
{
    char *psz_device;                             /* framebuffer device path */
    struct fb_fix_screeninfo    fix_info;     /* framebuffer fix information */

    /* Open framebuffer device */
    psz_device = main_GetPszVariable( VOUT_FB_DEV_VAR, VOUT_FB_DEV_DEFAULT );
    p_vout->p_sys->i_fb_dev = open( psz_device, O_RDWR);
    if( p_vout->p_sys->i_fb_dev == -1 )
    {
        intf_ErrMsg("vout error: can't open %s (%s)", psz_device, strerror(errno) );
        return( 1 );
    }

    /* Get framebuffer device informations */
    if( ioctl( p_vout->p_sys->i_fb_dev, FBIOGET_VSCREENINFO, &p_vout->p_sys->var_info ) )
    {
        intf_ErrMsg("vout error: can't get fb info (%s)", strerror(errno) );
        close( p_vout->p_sys->i_fb_dev );
        return( 1 );
    }

    if( ioctl( p_vout->p_sys->i_fb_dev, FBIOGET_VSCREENINFO, &p_vout->p_sys->old_info ) )
    {
        intf_ErrMsg("vout error: can't get 2nd fb info (%s)", strerror(errno) );
        close( p_vout->p_sys->i_fb_dev );
        return( 1 );
    }

    /* Set some attributes */
    p_vout->p_sys->var_info.activate = FB_ACTIVATE_NXTOPEN;
    p_vout->p_sys->var_info.xoffset =  0;
    p_vout->p_sys->var_info.yoffset =  0;

    if( ioctl( p_vout->p_sys->i_fb_dev, FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info ) )
    {
        intf_ErrMsg(" vout error: can't set fb info (%s)", strerror(errno) );
        close( p_vout->p_sys->i_fb_dev );
        return( 1 );
    }

    /* Get some informations again, in the definitive configuration */
    if( ioctl( p_vout->p_sys->i_fb_dev, FBIOGET_FSCREENINFO, &fix_info ) ||
        ioctl( p_vout->p_sys->i_fb_dev, FBIOGET_VSCREENINFO, &p_vout->p_sys->var_info ) )
    {
        intf_ErrMsg(" vout error: can't get additional fb info (%s)", strerror(errno) );

        /* Restore fb config */
        ioctl( p_vout->p_sys->i_fb_dev, FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info );

        close( p_vout->p_sys->i_fb_dev );
        return( 1 );
    }

    /* FIXME: if the image is full-size, it gets cropped on the left
     * because of the xres / xres_virtual slight difference */
    intf_WarnMsg( 1, "vout: %ix%i (virtual %ix%i)",
                  p_vout->p_sys->var_info.xres,
                  p_vout->p_sys->var_info.yres,
                  p_vout->p_sys->var_info.xres_virtual,
                  p_vout->p_sys->var_info.yres_virtual );

    p_vout->i_height = p_vout->p_sys->var_info.yres;
    p_vout->i_width  = p_vout->p_sys->var_info.xres_virtual ?
                           p_vout->p_sys->var_info.xres_virtual
                           : p_vout->p_sys->var_info.xres;

    p_vout->i_screen_depth = p_vout->p_sys->var_info.bits_per_pixel;

    p_vout->p_sys->fb_palette = NULL;
    p_vout->p_sys->b_pan = ( fix_info.ypanstep || fix_info.ywrapstep );

    switch( p_vout->i_screen_depth )
    {
    case 8:                                                         /* 8 bpp */
        p_vout->p_sys->fb_palette = malloc( 8 * 256 * sizeof( u16 ) );
        p_vout->p_sys->fb_cmap.start = 0;
        p_vout->p_sys->fb_cmap.len = 256;
        p_vout->p_sys->fb_cmap.red = p_vout->p_sys->fb_palette;
        p_vout->p_sys->fb_cmap.green = p_vout->p_sys->fb_palette + 256 * sizeof( u16 );
        p_vout->p_sys->fb_cmap.blue = p_vout->p_sys->fb_palette + 2 * 256 * sizeof( u16 );
        p_vout->p_sys->fb_cmap.transp = p_vout->p_sys->fb_palette + 3 * 256 * sizeof( u16 );

        /* Save the colormap */
        ioctl( p_vout->p_sys->i_fb_dev, FBIOGETCMAP, &p_vout->p_sys->fb_cmap );

        p_vout->i_bytes_per_pixel = 1;
        p_vout->i_bytes_per_line = p_vout->i_width;
        break;

    case 15:                      /* 15 bpp (16bpp with a missing green bit) */
    case 16:                                        /* 16 bpp (65536 colors) */
        p_vout->i_bytes_per_pixel = 2;
        p_vout->i_bytes_per_line = p_vout->i_width * 2;
        break;

    case 24:                                  /* 24 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 3;
        p_vout->i_bytes_per_line = p_vout->i_width * 3;
        break;

    case 32:                                  /* 32 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 4;
        p_vout->i_bytes_per_line = p_vout->i_width * 4;
        break;

    default:                                     /* unsupported screen depth */
        intf_ErrMsg( "vout error: screen depth %d is not supported",
                     p_vout->i_screen_depth);

        /* Restore fb config */
        ioctl( p_vout->p_sys->i_fb_dev, FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info );

        close( p_vout->p_sys->i_fb_dev );
        return( 1 );
        break;
    }

    switch( p_vout->i_screen_depth )
    {
    case 15:
    case 16:
    case 24:
    case 32:
        p_vout->i_red_mask =    ( (1 << p_vout->p_sys->var_info.red.length) - 1 )
                                    << p_vout->p_sys->var_info.red.offset;
        p_vout->i_green_mask =    ( (1 << p_vout->p_sys->var_info.green.length) - 1 )
                                    << p_vout->p_sys->var_info.green.offset;
        p_vout->i_blue_mask =    ( (1 << p_vout->p_sys->var_info.blue.length) - 1 )
                                    << p_vout->p_sys->var_info.blue.offset;
    }

    p_vout->p_sys->i_page_size = p_vout->i_width *
                p_vout->i_height * p_vout->i_bytes_per_pixel;

    /* Map two framebuffers a the very beginning of the fb */
    p_vout->p_sys->p_video = mmap( 0, p_vout->p_sys->i_page_size * 2,
                                   PROT_READ | PROT_WRITE, MAP_SHARED,
                                   p_vout->p_sys->i_fb_dev, 0 );

    if( (int)p_vout->p_sys->p_video == -1 ) /* according to man, it is -1.
                                               What about NULL ? */
    {
        intf_ErrMsg("vout error: can't map video memory (%s)", strerror(errno) );
        /* FIXME: restore fb config ?? */
        if( p_vout->i_screen_depth == 8 )
        {
            free( p_vout->p_sys->fb_palette );
        }

        /* Restore fb config */
        ioctl( p_vout->p_sys->i_fb_dev, FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info );

        close( p_vout->p_sys->i_fb_dev );
        return( 1 );
    }

    /* Set and initialize buffers */
    if( p_vout->p_sys->b_pan )
    {
        vout_SetBuffers( p_vout, p_vout->p_sys->p_video,
                                 p_vout->p_sys->p_video
                                  + p_vout->p_sys->i_page_size );
    }
    else
    {
        vout_SetBuffers( p_vout, p_vout->p_sys->p_video,
                                 p_vout->p_sys->p_video );
    }
    
    intf_WarnMsg( 1, "framebuffer type=%d, visual=%d, ypanstep=%d, ywrap=%d, accel=%d",
                  fix_info.type, fix_info.visual, fix_info.ypanstep, fix_info.ywrapstep, fix_info.accel );
    return( 0 );
}

/*****************************************************************************
 * FBCloseDisplay: terminate FB video thread output method
 *****************************************************************************/
static void FBCloseDisplay( vout_thread_t *p_vout )
{
    /* Clear display */
    memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size * 2 );

    /* Restore palette */
    if( p_vout->i_screen_depth == 8 );
    {
        ioctl( p_vout->p_sys->i_fb_dev, FBIOPUTCMAP, &p_vout->p_sys->fb_cmap );
        free( p_vout->p_sys->fb_palette );
    }

    /* Restore fb config */
    ioctl( p_vout->p_sys->i_fb_dev, FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info );

    /* Close fb */
    close( p_vout->p_sys->i_fb_dev );
}

/*****************************************************************************
 * FBSwitchDisplay: VT change signal handler
 *****************************************************************************
 * This function activates or deactivates the output of the thread. It is
 * called by the VT driver, on terminal change.
 *****************************************************************************/
static void FBSwitchDisplay(int i_signal)
{
    if( p_main->p_vout != NULL )
    {
        switch( i_signal )
        {
        case SIGUSR1:                                /* vt has been released */
            p_main->p_vout->b_active = 0;
            ioctl( ((vout_sys_t *)p_main->p_vout->p_sys)->i_tty_dev,
                   VT_RELDISP, 1 );
            break;
        case SIGUSR2:                                /* vt has been acquired */
            p_main->p_vout->b_active = 1;
            ioctl( ((vout_sys_t *)p_main->p_vout->p_sys)->i_tty_dev,
                   VT_RELDISP, VT_ACTIVATE );
            /* handle blanking */
            vlc_mutex_lock( &p_main->p_vout->change_lock );
            p_main->p_vout->i_changes |= VOUT_SIZE_CHANGE;
            vlc_mutex_unlock( &p_main->p_vout->change_lock );
            break;
        }
    }
}

/*****************************************************************************
 * FBTextMode and FBGfxMode : switch tty to text/graphic mode
 *****************************************************************************
 * These functions toggle the tty mode.
 *****************************************************************************/
static void FBTextMode( int i_tty_dev )
{
    /* return to text mode */
    if (-1 == ioctl(i_tty_dev, KDSETMODE, KD_TEXT))
    {
        intf_ErrMsg( "intf error: failed ioctl KDSETMODE KD_TEXT" );
    }
}

static void FBGfxMode( int i_tty_dev )
{
    /* switch to graphic mode */
    if (-1 == ioctl(i_tty_dev, KDSETMODE, KD_GRAPHICS))
    {
        intf_ErrMsg( "intf error: failed ioctl KDSETMODE KD_GRAPHICS" );
    }
}

