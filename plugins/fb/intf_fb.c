/*****************************************************************************
 * intf_fb.c: Linux framebuffer interface plugin
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 *
 * Authors:
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
#include "defs.h"

#include <errno.h>                                                  /* errno */
#include <signal.h>                                      /* SIGUSR1, SIGUSR2 */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                                /* read() */
#include <sys/ioctl.h>                                            /* ioctl() */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */

#include <termios.h>                                       /* struct termios */
#include <linux/vt.h>                                                /* VT_* */
#include <linux/kd.h>                                                 /* KD* */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "input.h"
#include "video.h"
#include "video_output.h"

#include "intf_msg.h"
#include "interface.h"

#include "main.h"

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    /* System informations */
    int                         i_tty_dev;              /* tty device handle */

    /* Original configuration informations */
    struct sigaction            sig_usr1;           /* USR1 previous handler */
    struct sigaction            sig_usr2;           /* USR2 previous handler */
    struct vt_mode              vt_mode;                 /* previous VT mode */

    int                 i_width;                     /* width of main window */
    int                 i_height;                   /* height of main window */

    struct termios      old_termios;
    struct termios      new_termios;

} intf_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void    FBSwitchDisplay            ( int i_signal );
static void    FBTextMode                 ( int i_tty_dev );
static void    FBGfxMode                  ( int i_tty_dev );

/*****************************************************************************
 * intf_FBCreate: initialize and create window
 *****************************************************************************/
int intf_FBCreate( intf_thread_t *p_intf )
{
    struct sigaction            sig_tty;         /* sigaction for tty change */
    struct vt_mode              vt_mode;                  /* vt current mode */

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };
    intf_DbgMsg("0x%x\n", p_intf );

    /* Set tty and fb devices */
    p_intf->p_sys->i_tty_dev = 0;       /* 0 == /dev/tty0 == current console */

    FBGfxMode( p_intf->p_sys->i_tty_dev );

    /* set keyboard settings */
    if (tcgetattr(0, &p_intf->p_sys->old_termios) == -1)
    {
        intf_ErrMsg( "intf error: tcgetattr" );
    }

    if (tcgetattr(0, &p_intf->p_sys->new_termios) == -1)
    {
        intf_ErrMsg( "intf error: tcgetattr" );
    }

    p_intf->p_sys->new_termios.c_lflag &= ~ (ICANON | ISIG);
    p_intf->p_sys->new_termios.c_lflag |= (ECHO | ECHOCTL);
    p_intf->p_sys->new_termios.c_iflag = 0;
    p_intf->p_sys->new_termios.c_cc[VMIN] = 1;
    p_intf->p_sys->new_termios.c_cc[VTIME] = 0;

    if (tcsetattr(0, TCSAFLUSH, &p_intf->p_sys->new_termios) == -1)
    {
        intf_ErrMsg( "intf error: tcsetattr" );
    }

    ioctl(p_intf->p_sys->i_tty_dev, VT_RELDISP, VT_ACKACQ);

    /* Set-up tty signal handler to be aware of tty changes */
    memset( &sig_tty, 0, sizeof( sig_tty ) );
    sig_tty.sa_handler = FBSwitchDisplay;
    sigemptyset( &sig_tty.sa_mask );
    if( sigaction( SIGUSR1, &sig_tty, &p_intf->p_sys->sig_usr1 ) ||
        sigaction( SIGUSR2, &sig_tty, &p_intf->p_sys->sig_usr2 ) )
    {
        intf_ErrMsg( "intf error: can't set up signal handler (%s)\n",
                     strerror(errno) );
        tcsetattr(0, 0, &p_intf->p_sys->old_termios);
        FBTextMode( p_intf->p_sys->i_tty_dev );
        return( 1 );
    }

    /* Set-up tty according to new signal handler */
    if( ioctl(p_intf->p_sys->i_tty_dev, VT_GETMODE, &p_intf->p_sys->vt_mode)
        == -1 )
    {
        intf_ErrMsg( "intf error: cant get terminal mode (%s)\n",
                     strerror(errno) );
        sigaction( SIGUSR1, &p_intf->p_sys->sig_usr1, NULL );
        sigaction( SIGUSR2, &p_intf->p_sys->sig_usr2, NULL );
        tcsetattr(0, 0, &p_intf->p_sys->old_termios);
        FBTextMode( p_intf->p_sys->i_tty_dev );
        return( 1 );
    }
    memcpy( &vt_mode, &p_intf->p_sys->vt_mode, sizeof( vt_mode ) );
    vt_mode.mode   = VT_PROCESS;
    vt_mode.waitv  = 0;
    vt_mode.relsig = SIGUSR1;
    vt_mode.acqsig = SIGUSR2;

    if( ioctl(p_intf->p_sys->i_tty_dev, VT_SETMODE, &vt_mode) == -1 )
    {
        intf_ErrMsg( "intf error: can't set terminal mode (%s)\n",
                     strerror(errno) );
        sigaction( SIGUSR1, &p_intf->p_sys->sig_usr1, NULL );
        sigaction( SIGUSR2, &p_intf->p_sys->sig_usr2, NULL );
        tcsetattr(0, 0, &p_intf->p_sys->old_termios);
        FBTextMode( p_intf->p_sys->i_tty_dev );
        return( 1 );
    }

    /* Spawn video output thread */
    if( p_main->b_video )
    {
        p_intf->p_vout = vout_CreateThread( NULL, 0,
                                            p_intf->p_sys->i_width,
                                            p_intf->p_sys->i_height,
                                            NULL, 0, NULL );
        if( p_intf->p_vout == NULL )                          /* XXX?? error */
        {
            intf_ErrMsg("intf error: can't create output thread\n" );
            ioctl( p_intf->p_sys->i_tty_dev, VT_SETMODE,
                   &p_intf->p_sys->vt_mode );
            sigaction( SIGUSR1, &p_intf->p_sys->sig_usr1, NULL );
            sigaction( SIGUSR2, &p_intf->p_sys->sig_usr2, NULL );
            free( p_intf->p_sys );
            tcsetattr(0, 0, &p_intf->p_sys->old_termios);
            FBTextMode( p_intf->p_sys->i_tty_dev );
            return( 1 );
        }
    }

    return( 0 );
}

/*****************************************************************************
 * intf_FBDestroy: destroy interface window
 *****************************************************************************/
void intf_FBDestroy( intf_thread_t *p_intf )
{
    /* resets the keyboard state */
    tcsetattr(0, 0, &p_intf->p_sys->old_termios);

    /* return to text mode */
    FBTextMode( p_intf->p_sys->i_tty_dev );

    /* Close input thread, if any (blocking) */
    if( p_intf->p_input )
    {
        input_DestroyThread( p_intf->p_input, NULL );
    }

    /* Close video output thread, if any (blocking) */
    if( p_intf->p_vout )
    {
        vout_DestroyThread( p_intf->p_vout, NULL );
    }

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * intf_FBManage: event loop
 *****************************************************************************/
void intf_FBManage( intf_thread_t *p_intf )
{
    unsigned char buf[16];

    //while ( read(0, buf, 1) == 1)
    if ( read(0, buf, 1) == 1)
    {
        if( intf_ProcessKey(p_intf, (int)buf[0]) )
        {
            intf_ErrMsg("unhandled key '%c' (%i)\n", (char) buf[0], buf[0] );
        }
    }
}

/*****************************************************************************
 * FBSwitchDisplay: VT change signal handler
 *****************************************************************************
 * This function activates or deactivates the output of the thread. It is
 * called by the VT driver, on terminal change.
 *****************************************************************************/
static void FBSwitchDisplay(int i_signal)
{
    if( p_main->p_intf->p_vout != NULL )
    {
        switch( i_signal )
        {
        case SIGUSR1:                                /* vt has been released */
            p_main->p_intf->p_vout->b_active = 0;
            ioctl( ((intf_sys_t *)p_main->p_intf->p_sys)->i_tty_dev,
                   VT_RELDISP, 1 );
            break;
        case SIGUSR2:                                /* vt has been acquired */
            p_main->p_intf->p_vout->b_active = 1;
            ioctl( ((intf_sys_t *)p_main->p_intf->p_sys)->i_tty_dev,
                   VT_RELDISP, VT_ACTIVATE );
            /* handle blanking */
            p_main->p_intf->p_vout->i_changes |= VOUT_SIZE_CHANGE;
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
        intf_ErrMsg("intf error: ioctl KDSETMODE\n");
    }
}

static void FBGfxMode( int i_tty_dev )
{
    /* switch to graphic mode */
    if (-1 == ioctl(i_tty_dev, KDSETMODE, KD_GRAPHICS))
    {
        intf_ErrMsg("intf error: ioctl KDSETMODE\n");
    }
}

/*****************************************************************************
 * vout_SysPrint: print simple text on a picture
 *****************************************************************************
 * This function will print a simple text on the picture. It is designed to
 * print debugging or general informations, not to render subtitles.
 *****************************************************************************/
void vout_SysPrint( vout_thread_t *p_vout, int i_x, int i_y, int i_halign,
                    int i_valign, unsigned char *psz_text )
{

}

