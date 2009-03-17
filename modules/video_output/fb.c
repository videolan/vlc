/*****************************************************************************
 * fb.c : framebuffer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Jean-Paul Saman
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>                                                 /* ENOMEM */
#include <signal.h>                                      /* SIGUSR1, SIGUSR2 */
#include <fcntl.h>                                                 /* open() */
#include <unistd.h>                                               /* close() */

#include <termios.h>                                       /* struct termios */
#include <sys/ioctl.h>
#include <sys/mman.h>                                              /* mmap() */

#include <linux/fb.h>
#include <linux/vt.h>                                                /* VT_* */
#include <linux/kd.h>                                                 /* KD* */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_interface.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void Display   ( vout_thread_t *, picture_t * );
static int  Control   ( vout_thread_t *, int, va_list );

static int  NewPicture     ( vout_thread_t *, picture_t * );
static void FreePicture    ( vout_thread_t *, picture_t * );

static int  OpenDisplay    ( vout_thread_t * );
static void CloseDisplay   ( vout_thread_t * );
static void SwitchDisplay  ( int i_signal );
static void TextMode       ( int i_tty );
static void GfxMode        ( int i_tty );

#define MAX_DIRECTBUFFERS 1

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FB_DEV_VAR "fbdev"

#define DEVICE_TEXT N_("Framebuffer device")
#define DEVICE_LONGTEXT N_( \
    "Framebuffer device to use for rendering (usually /dev/fb0).")

#define TTY_TEXT N_("Run fb on current tty.")
#define TTY_LONGTEXT N_( \
    "Run framebuffer on current TTY device (default enabled). " \
    "(disable tty handling with caution)" )

#define CHROMA_TEXT N_("Chroma used.")
#define CHROMA_LONGTEXT N_( \
    "Force use of a specific chroma for output. Default is I420." )

#define ASPECT_RATIO_TEXT N_("Video aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_( \
    "Aspect ratio of the video image (4:3, 16:9). Default is square pixels." )

#define FB_MODE_TEXT N_("Framebuffer resolution to use.")
#define FB_MODE_LONGTEXT N_( \
    "Select the resolution for the framebuffer. Currently it supports " \
    "the values 0=QCIF 1=CIF 2=NTSC 3=PAL, 4=auto (default 4=auto)" )

#define HW_ACCEL_TEXT N_("Framebuffer uses hw acceleration.")
#define HW_ACCEL_LONGTEXT N_( \
    "If your framebuffer supports hardware acceleration or does double buffering " \
    "in hardware then you must disable this option. It then does double buffering " \
    "in software." )

vlc_module_begin ()
    set_shortname( "Framebuffer" )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    add_file( FB_DEV_VAR, "/dev/fb0", NULL, DEVICE_TEXT, DEVICE_LONGTEXT,
              false )
    add_bool( "fb-tty", 1, NULL, TTY_TEXT, TTY_LONGTEXT, true )
    add_string( "fb-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                true )
    add_string( "fb-aspect-ratio", NULL, NULL, ASPECT_RATIO_TEXT,
                ASPECT_RATIO_LONGTEXT, true )
    add_integer( "fb-mode", 4, NULL, FB_MODE_TEXT, FB_MODE_LONGTEXT,
                 true )
    add_bool( "fb-hw-accel", true, NULL, HW_ACCEL_TEXT, HW_ACCEL_LONGTEXT,
              true )
    set_description( N_("GNU/Linux framebuffer video output") )
    set_capability( "video output", 30 )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * vout_sys_t: video output framebuffer method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the FB specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    /* System information */
    int                 i_tty;                          /* tty device handle */
    bool                b_tty;
    struct termios      old_termios;

    /* Original configuration information */
    struct sigaction            sig_usr1;           /* USR1 previous handler */
    struct sigaction            sig_usr2;           /* USR2 previous handler */
    struct vt_mode              vt_mode;                 /* previous VT mode */

    /* Framebuffer information */
    int                         i_fd;                       /* device handle */
    struct fb_var_screeninfo    old_info;       /* original mode information */
    struct fb_var_screeninfo    var_info;        /* current mode information */
    bool                        b_pan;     /* does device supports panning ? */
    struct fb_cmap              fb_cmap;                /* original colormap */
    uint16_t                    *p_palette;              /* original palette */
    bool                        b_hw_accel;          /* has hardware support */

    /* Video information */
    uint32_t i_width;
    uint32_t i_height;
    int      i_aspect;
    int      i_bytes_per_pixel;
    bool     b_auto;       /* Automatically adjust video size to fb size */
    vlc_fourcc_t i_chroma;

    /* Video memory */
    uint8_t    *p_video;                                      /* base adress */
    size_t      i_page_size;                                    /* page size */
};

struct picture_sys_t
{
    uint8_t *    p_data;                                      /* base adress */
};

/*****************************************************************************
 * Create: allocates FB video thread output method
 *****************************************************************************
 * This function allocates and initializes a FB vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t    *p_sys;
    char          *psz_chroma;
    char          *psz_aspect;
    int           i_mode;
    struct sigaction    sig_tty;                 /* sigaction for tty change */
    struct vt_mode      vt_mode;                          /* vt current mode */
    struct termios      new_termios;

    /* Allocate instance and initialize some members */
    p_vout->p_sys = p_sys = calloc( 1, sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->p_video = MAP_FAILED;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;
    p_vout->pf_control = Control;

    /* Does the framebuffer uses hw acceleration? */
    p_sys->b_hw_accel = var_CreateGetBool( p_vout, "fb-hw-accel" );

    /* Set tty and fb devices */
    p_sys->i_tty = 0; /* 0 == /dev/tty0 == current console */
    p_sys->b_tty = var_CreateGetBool( p_vout, "fb-tty" );
#ifndef WIN32
#if defined(HAVE_ISATTY)
    /* Check that stdin is a TTY */
    if( p_sys->b_tty && !isatty( 0 ) )
    {
        msg_Warn( p_vout, "fd 0 is not a TTY" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    else
    {
        msg_Warn( p_vout, "disabling tty handling, use with caution because "
                          "there is no way to return to the tty." );
    }
#endif
#endif

    psz_chroma = var_CreateGetNonEmptyString( p_vout, "fb-chroma" );
    if( psz_chroma )
    {
        if( strlen( psz_chroma ) == 4 )
        {
            p_sys->i_chroma = VLC_FOURCC( psz_chroma[0],
                                   psz_chroma[1],
                                   psz_chroma[2],
                                   psz_chroma[3] );
            msg_Dbg( p_vout, "forcing chroma '%s'", psz_chroma );
        }
        else
        {
            msg_Warn( p_vout, "invalid chroma (%s), using defaults.",
                      psz_chroma );
        }
        free( psz_chroma );
    }

    p_sys->i_aspect = -1;
    psz_aspect = var_CreateGetNonEmptyString( p_vout, "fb-aspect-ratio" );
    if( psz_aspect )
    {
        char *psz_parser = strchr( psz_aspect, ':' );

        if( psz_parser )
        {
            *psz_parser++ = '\0';
            p_sys->i_aspect = ( atoi( psz_aspect )
                              * VOUT_ASPECT_FACTOR ) / atoi( psz_parser );
        }
        msg_Dbg( p_vout, "using aspect ratio %d:%d",
                  atoi( psz_aspect ), atoi( psz_parser ) );

        free( psz_aspect );
    }

    p_sys->b_auto = false;
    i_mode = var_CreateGetInteger( p_vout, "fb-mode" );
    switch( i_mode )
    {
        case 0: /* QCIF */
            p_sys->i_width  = 176;
            p_sys->i_height = 144;
            break;
        case 1: /* CIF */
            p_sys->i_width  = 352;
            p_sys->i_height = 288;
            break;
        case 2: /* NTSC */
            p_sys->i_width  = 640;
            p_sys->i_height = 480;
            break;
        case 3: /* PAL */
            p_sys->i_width  = 704;
            p_sys->i_height = 576;
            break;
        case 4:
        default:
            p_sys->b_auto = true;
     }

    /* tty handling */
    if( p_sys->b_tty )
    {
        GfxMode( p_sys->i_tty );

        /* Set keyboard settings */
        if( tcgetattr(0, &p_vout->p_sys->old_termios) == -1 )
        {
            msg_Err( p_vout, "tcgetattr failed" );
        }

        if( tcgetattr(0, &new_termios) == -1 )
        {
            msg_Err( p_vout, "tcgetattr failed" );
        }

        /* new_termios.c_lflag &= ~ (ICANON | ISIG);
        new_termios.c_lflag |= (ECHO | ECHOCTL); */
        new_termios.c_lflag &= ~ (ICANON);
        new_termios.c_lflag &= ~(ECHO | ECHOCTL);
        new_termios.c_iflag = 0;
        new_termios.c_cc[VMIN] = 1;
        new_termios.c_cc[VTIME] = 0;

        if( tcsetattr(0, TCSAFLUSH, &new_termios) == -1 )
        {
            msg_Err( p_vout, "tcsetattr failed" );
        }

        ioctl( p_sys->i_tty, VT_RELDISP, VT_ACKACQ );

        /* Set-up tty signal handler to be aware of tty changes */
        memset( &sig_tty, 0, sizeof( sig_tty ) );
        sig_tty.sa_handler = SwitchDisplay;
        sigemptyset( &sig_tty.sa_mask );
        if( sigaction( SIGUSR1, &sig_tty, &p_vout->p_sys->sig_usr1 ) ||
            sigaction( SIGUSR2, &sig_tty, &p_vout->p_sys->sig_usr2 ) )
        {
            msg_Err( p_vout, "cannot set signal handler (%m)" );
            tcsetattr(0, 0, &p_vout->p_sys->old_termios);
            TextMode( p_sys->i_tty );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }

        /* Set-up tty according to new signal handler */
        if( -1 == ioctl( p_sys->i_tty, VT_GETMODE, &p_vout->p_sys->vt_mode ) )
        {
            msg_Err( p_vout, "cannot get terminal mode (%m)" );
            sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
            sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );
            tcsetattr(0, 0, &p_vout->p_sys->old_termios);
            TextMode( p_sys->i_tty );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }
        memcpy( &vt_mode, &p_vout->p_sys->vt_mode, sizeof( vt_mode ) );
        vt_mode.mode   = VT_PROCESS;
        vt_mode.waitv  = 0;
        vt_mode.relsig = SIGUSR1;
        vt_mode.acqsig = SIGUSR2;

        if( -1 == ioctl( p_sys->i_tty, VT_SETMODE, &vt_mode ) )
        {
            msg_Err( p_vout, "cannot set terminal mode (%m)" );
            sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
            sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );
            tcsetattr(0, 0, &p_vout->p_sys->old_termios);
            TextMode( p_sys->i_tty );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }
    }

    if( OpenDisplay( p_vout ) )
    {
        Destroy( VLC_OBJECT(p_vout) );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Destroy: destroy FB video thread output method
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    CloseDisplay( p_vout );

    if( p_vout->p_sys->b_tty )
    {
        /* Reset the terminal */
        ioctl( p_vout->p_sys->i_tty, VT_SETMODE, &p_vout->p_sys->vt_mode );

        /* Remove signal handlers */
        sigaction( SIGUSR1, &p_vout->p_sys->sig_usr1, NULL );
        sigaction( SIGUSR2, &p_vout->p_sys->sig_usr2, NULL );

        /* Reset the keyboard state */
        tcsetattr( 0, 0, &p_vout->p_sys->old_termios );

        /* Return to text mode */
        TextMode( p_vout->p_sys->i_tty );
    }

    /* Destroy structure */
    free( p_vout->p_sys );
}

/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );
    if( p_pic->p_sys == NULL )
    {
        return VLC_ENOMEM;
    }

    /* Fill in picture_t fields */
    vout_InitPicture( VLC_OBJECT(p_vout), p_pic, p_vout->output.i_chroma,
                      p_vout->output.i_width, p_vout->output.i_height,
                      p_vout->output.i_aspect );

    p_pic->p_sys->p_data = malloc( p_vout->p_sys->i_page_size );
    if( !p_pic->p_sys->p_data )
    {
        free( p_pic->p_sys );
        p_pic->p_sys = NULL;
        return VLC_ENOMEM;
    }

    p_pic->p->p_pixels = (uint8_t*) p_pic->p_sys->p_data;

    p_pic->p->i_pixel_pitch = p_vout->p_sys->i_bytes_per_pixel;
    p_pic->p->i_lines = p_vout->p_sys->var_info.yres;
    p_pic->p->i_visible_lines = p_vout->p_sys->var_info.yres;

    if( p_vout->p_sys->var_info.xres_virtual )
    {
        p_pic->p->i_pitch = p_vout->p_sys->var_info.xres_virtual
                             * p_vout->p_sys->i_bytes_per_pixel;
    }
    else
    {
        p_pic->p->i_pitch = p_vout->p_sys->var_info.xres
                             * p_vout->p_sys->i_bytes_per_pixel;
    }

    p_pic->p->i_visible_pitch = p_vout->p_sys->var_info.xres
                                 * p_vout->p_sys->i_bytes_per_pixel;
    p_pic->i_planes = 1;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * FreePicture: destroy a picture allocated with NewPicture
 *****************************************************************************
 * Destroy Image AND associated data.
 *****************************************************************************/
static void FreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    VLC_UNUSED(p_vout);
    free( p_pic->p_sys->p_data );
    free( p_pic->p_sys );
    p_pic->p_sys = NULL;
}

/*****************************************************************************
 * Init: initialize framebuffer video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_index;
    picture_t *p_pic = NULL;

    I_OUTPUTPICTURES = 0;

    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_vout->fmt_out = p_vout->fmt_in;
    if( p_sys->i_chroma == 0 )
    {
        /* Initialize the output structure: RGB with square pixels, whatever
         * the input format is, since it's the only format we know */
        switch( p_sys->var_info.bits_per_pixel )
        {
        case 8: /* FIXME: set the palette */
            p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2'); break;
        case 15:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','1','5'); break;
        case 16:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','1','6'); break;
        case 24:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','2','4'); break;
        case 32:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2'); break;
        default:
            msg_Err( p_vout, "unknown screen depth %i",
                     p_vout->p_sys->var_info.bits_per_pixel );
            return VLC_EGENERIC;
        }

        if( p_sys->var_info.bits_per_pixel != 8 )
        {
            p_vout->output.i_rmask = ( (1 << p_sys->var_info.red.length) - 1 )
                                 << p_sys->var_info.red.offset;
            p_vout->output.i_gmask = ( (1 << p_sys->var_info.green.length) - 1 )
                                 << p_sys->var_info.green.offset;
            p_vout->output.i_bmask = ( (1 << p_sys->var_info.blue.length) - 1 )
                                 << p_sys->var_info.blue.offset;
        }
    }
    else
    {
        p_vout->output.i_chroma = p_sys->i_chroma;
    }
    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;

    p_vout->output.i_width =
    p_vout->fmt_out.i_width =
    p_vout->fmt_out.i_visible_width = p_sys->i_width;
    p_vout->output.i_height =
    p_vout->fmt_out.i_height =
    p_vout->fmt_out.i_visible_height = p_sys->i_height;

    /* Assume we have square pixels */
    if( p_sys->i_aspect < 0 )
    {
        p_vout->output.i_aspect = ( p_sys->i_width
                                  * VOUT_ASPECT_FACTOR ) / p_sys->i_height;
    }
    else p_vout->output.i_aspect = p_sys->i_aspect;

    p_vout->fmt_out.i_sar_num = p_vout->fmt_out.i_sar_den = 1;
    p_vout->fmt_out.i_aspect  = p_vout->render.i_aspect = p_vout->output.i_aspect;
    p_vout->fmt_out.i_x_offset= p_vout->fmt_out.i_y_offset = 0;

    /* Clear the screen */
    memset( p_sys->p_video, 0, p_sys->i_page_size );

    if( !p_sys->b_hw_accel )
    {
        /* Try to initialize up to MAX_DIRECTBUFFERS direct buffers */
        while( I_OUTPUTPICTURES < MAX_DIRECTBUFFERS )
        {
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
            if( p_pic == NULL || NewPicture( p_vout, p_pic ) )
            {
                break;
            }

            p_pic->i_status = DESTROYED_PICTURE;
            p_pic->i_type   = DIRECT_PICTURE;

            PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

            I_OUTPUTPICTURES++;
        }
    }
    else
    {
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
            return VLC_EGENERIC;
        }

        /* We know the chroma, allocate a buffer which will be used
        * directly by the decoder */
        p_pic->p->p_pixels = p_vout->p_sys->p_video;
        p_pic->p->i_pixel_pitch = p_vout->p_sys->i_bytes_per_pixel;
        p_pic->p->i_lines = p_vout->p_sys->var_info.yres;
        p_pic->p->i_visible_lines = p_vout->p_sys->var_info.yres;

        if( p_vout->p_sys->var_info.xres_virtual )
        {
            p_pic->p->i_pitch = p_vout->p_sys->var_info.xres_virtual
                                * p_vout->p_sys->i_bytes_per_pixel;
        }
        else
        {
            p_pic->p->i_pitch = p_vout->p_sys->var_info.xres
                                * p_vout->p_sys->i_bytes_per_pixel;
        }

        p_pic->p->i_visible_pitch = p_vout->p_sys->var_info.xres
                                    * p_vout->p_sys->i_bytes_per_pixel;
        p_pic->i_planes = 1;
        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate framebuffer video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    if( !p_vout->p_sys->b_hw_accel )
    {
        int i_index;

        /* Free the direct buffers we allocated */
        for( i_index = I_OUTPUTPICTURES ; i_index ; )
        {
            i_index--;
            FreePicture( p_vout, PP_OUTPUTPICTURE[ i_index ] );
        }

    }
    /* Clear the screen */
    memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size );
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    (void) p_vout; (void) i_query; (void) args;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Manage: handle FB events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
#if 0
    uint8_t buf;

    if ( read(0, &buf, 1) == 1)
    {
        switch( buf )
        {
        case 'q':
            libvlc_Quit( p_vout->p_libvlc );
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
        msg_Dbg( p_vout, "reinitializing framebuffer screen" );
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        /* Destroy XImages to change their size */
        End( p_vout );

        /* Recreate XImages. If SysInit failed, the thread can't go on. */
        if( Init( p_vout ) )
        {
            msg_Err( p_vout, "cannot reinit framebuffer screen" );
            return VLC_EGENERIC;
        }

        /* Clear screen */
        memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size );

#if 0
        /* Tell the video output thread that it will need to rebuild YUV
         * tables. This is needed since conversion buffer size may have changed */
        p_vout->i_changes |= VOUT_YUV_CHANGE;
#endif
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to FB image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
static int panned=0;
    /* swap the two Y offsets if the drivers supports panning */
    if( p_vout->p_sys->b_pan )
    {
        p_vout->p_sys->var_info.yoffset = 0;
        /*p_vout->p_sys->var_info.yoffset = p_vout->p_sys->var_info.yres; */

        /* the X offset should be 0, but who knows ...
         * some other app might have played with the framebuffer */
        p_vout->p_sys->var_info.xoffset = 0;

        if( panned < 0 )
        {
            ioctl( p_vout->p_sys->i_fd,
                   FBIOPAN_DISPLAY, &p_vout->p_sys->var_info );
            panned++;
        }
    }

    if( !p_vout->p_sys->b_hw_accel )
    {
        vlc_memcpy( p_vout->p_sys->p_video, p_pic->p->p_pixels,
                    p_vout->p_sys->i_page_size );
    }
}

#if 0
static void SetPalette( vout_thread_t *p_vout, uint16_t *red, uint16_t *green,
                                               uint16_t *blue, uint16_t *transp )
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
    vout_sys_t *p_sys = (vout_sys_t *) p_vout->p_sys;
    char *psz_device;                             /* framebuffer device path */
    struct fb_fix_screeninfo    fix_info;     /* framebuffer fix information */

    /* Open framebuffer device */
    if( !(psz_device = config_GetPsz( p_vout, FB_DEV_VAR )) )
    {
        msg_Err( p_vout, "don't know which fb device to open" );
        return VLC_EGENERIC;
    }

    p_sys->i_fd = open( psz_device, O_RDWR);
    if( p_sys->i_fd == -1 )
    {
        msg_Err( p_vout, "cannot open %s (%m)", psz_device );
        free( psz_device );
        return VLC_EGENERIC;
    }
    free( psz_device );

    /* Get framebuffer device information */
    if( ioctl( p_sys->i_fd, FBIOGET_VSCREENINFO, &p_sys->var_info ) )
    {
        msg_Err( p_vout, "cannot get fb info (%m)" );
        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    memcpy( &p_sys->old_info, &p_sys->var_info,
            sizeof( struct fb_var_screeninfo ) );

    /* Get some info on the framebuffer itself */
    if( !p_sys->b_auto )
    {
        p_sys->var_info.xres = p_sys->var_info.xres_virtual = p_sys->i_width;
        p_sys->var_info.yres = p_sys->var_info.yres_virtual = p_sys->i_height;
        p_vout->fmt_out.i_width = p_sys->i_width;
        p_vout->fmt_out.i_height = p_sys->i_height;
    }

    /* Set some attributes */
    p_sys->var_info.activate = p_sys->b_tty
                               ? FB_ACTIVATE_NXTOPEN
                               : FB_ACTIVATE_NOW;
    p_sys->var_info.xoffset =  0;
    p_sys->var_info.yoffset =  0;

    if( ioctl( p_sys->i_fd, FBIOPUT_VSCREENINFO, &p_sys->var_info ) )
    {
        msg_Err( p_vout, "cannot set fb info (%m)" );
        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    /* Get some information again, in the definitive configuration */
    if( ioctl( p_sys->i_fd, FBIOGET_FSCREENINFO, &fix_info )
         || ioctl( p_sys->i_fd, FBIOGET_VSCREENINFO, &p_sys->var_info ) )
    {
        msg_Err( p_vout, "cannot get additional fb info (%m)" );

        /* Restore fb config */
        ioctl( p_sys->i_fd, FBIOPUT_VSCREENINFO, &p_sys->old_info );

        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    /* If the fb has limitations on mode change,
     * then keep the resolution of the fb */
    if( (p_sys->i_height != p_sys->var_info.yres) ||
        (p_sys->i_width != p_sys->var_info.xres) )
    {
        p_sys->b_auto = true;
        msg_Warn( p_vout,
                  "using framebuffer native resolution instead of requested (%ix%i)",
                  p_sys->i_width, p_sys->i_height );
    }
    p_sys->i_height = p_sys->var_info.yres;
    p_sys->i_width  = p_sys->var_info.xres_virtual
                               ? p_sys->var_info.xres_virtual
                               : p_sys->var_info.xres;

    /* FIXME: if the image is full-size, it gets cropped on the left
     * because of the xres / xres_virtual slight difference */
    msg_Dbg( p_vout, "%ix%i (virtual %ix%i) (request %ix%i)",
             p_sys->var_info.xres, p_sys->var_info.yres,
             p_sys->var_info.xres_virtual,
             p_sys->var_info.yres_virtual,
             p_sys->i_width, p_sys->i_height );

    p_sys->p_palette = NULL;
    p_sys->b_pan = ( fix_info.ypanstep || fix_info.ywrapstep );

    switch( p_sys->var_info.bits_per_pixel )
    {
    case 8:
        p_sys->p_palette = malloc( 8 * 256 * sizeof( uint16_t ) );
        if( !p_sys->p_palette )
        {
            /* Restore fb config */
            ioctl( p_sys->i_fd, FBIOPUT_VSCREENINFO, &p_sys->old_info );

            close( p_sys->i_fd );
            return VLC_ENOMEM;
        }
        p_sys->fb_cmap.start = 0;
        p_sys->fb_cmap.len = 256;
        p_sys->fb_cmap.red = p_sys->p_palette;
        p_sys->fb_cmap.green = p_sys->p_palette + 256 * sizeof( uint16_t );
        p_sys->fb_cmap.blue = p_sys->p_palette + 2 * 256 * sizeof( uint16_t );
        p_sys->fb_cmap.transp = p_sys->p_palette + 3 * 256 * sizeof( uint16_t );

        /* Save the colormap */
        ioctl( p_sys->i_fd, FBIOGETCMAP, &p_sys->fb_cmap );

        p_sys->i_bytes_per_pixel = 1;
        break;

    case 15:
    case 16:
        p_sys->i_bytes_per_pixel = 2;
        break;

    case 24:
        p_sys->i_bytes_per_pixel = 3;
        break;

    case 32:
        p_sys->i_bytes_per_pixel = 4;
        break;

    default:
        msg_Err( p_vout, "screen depth %d is not supported",
                         p_sys->var_info.bits_per_pixel );

        /* Restore fb config */
        ioctl( p_sys->i_fd, FBIOPUT_VSCREENINFO, &p_sys->old_info );

        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    p_sys->i_page_size = p_sys->i_width * p_sys->i_height *
                         p_sys->i_bytes_per_pixel;

    /* Map a framebuffer at the beginning */
    p_sys->p_video = mmap( NULL, p_sys->i_page_size,
                              PROT_READ | PROT_WRITE, MAP_SHARED,
                              p_sys->i_fd, 0 );

    if( p_sys->p_video == MAP_FAILED )
    {
        msg_Err( p_vout, "cannot map video memory (%m)" );

        if( p_sys->var_info.bits_per_pixel == 8 )
        {
            free( p_sys->p_palette );
            p_sys->p_palette = NULL;
        }

        /* Restore fb config */
        ioctl( p_sys->i_fd, FBIOPUT_VSCREENINFO, &p_vout->p_sys->old_info );

        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_vout, "framebuffer type=%d, visual=%d, ypanstep=%d, "
             "ywrap=%d, accel=%d", fix_info.type, fix_info.visual,
             fix_info.ypanstep, fix_info.ywrapstep, fix_info.accel );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDisplay: terminate FB video thread output method
 *****************************************************************************/
static void CloseDisplay( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->p_video != MAP_FAILED )
    {
        /* Clear display */
        memset( p_vout->p_sys->p_video, 0, p_vout->p_sys->i_page_size );
        munmap( p_vout->p_sys->p_video, p_vout->p_sys->i_page_size );
    }

    if( p_vout->p_sys->i_fd >= 0 )
    {
        /* Restore palette */
        if( p_vout->p_sys->var_info.bits_per_pixel == 8 )
        {
            ioctl( p_vout->p_sys->i_fd,
                   FBIOPUTCMAP, &p_vout->p_sys->fb_cmap );
            free( p_vout->p_sys->p_palette );
            p_vout->p_sys->p_palette = NULL;
        }

        /* Restore fb config */
        ioctl( p_vout->p_sys->i_fd,
               FBIOPUT_VSCREENINFO, &p_vout->p_sys->old_info );

        /* Close fb */
        close( p_vout->p_sys->i_fd );
    }
}

/*****************************************************************************
 * SwitchDisplay: VT change signal handler
 *****************************************************************************
 * This function activates or deactivates the output of the thread. It is
 * called by the VT driver, on terminal change.
 *****************************************************************************/
static void SwitchDisplay( int i_signal )
{
    VLC_UNUSED( i_signal );
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
            ioctl( p_sys->i_tty, VT_RELDISP, 1 );
            break;
        case SIGUSR2:                                /* vt has been acquired */
            p_vout->b_active = 1;
            ioctl( p_sys->i_tty, VT_RELDISP, VT_ACTIVATE );
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
        /*msg_Err( p_vout, "failed ioctl KDSETMODE KD_TEXT" );*/
    }
}

static void GfxMode( int i_tty )
{
    /* switch to graphic mode */
    if( -1 == ioctl(i_tty, KDSETMODE, KD_GRAPHICS) )
    {
        /*msg_Err( p_vout, "failed ioctl KDSETMODE KD_GRAPHICS" );*/
    }
}
