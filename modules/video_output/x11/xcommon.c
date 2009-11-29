/*****************************************************************************
 * xcommon.c: Functions common to the X11 and XVideo plugins
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Sam Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>
#include <vlc_vout_window.h>
#include <vlc_keys.h>

#ifdef HAVE_MACHINE_PARAM_H
    /* BSD */
#   include <machine/param.h>
#   include <sys/types.h>                                  /* typedef ushort */
#   include <sys/ipc.h>
#endif

#ifdef HAVE_XSP
#include <X11/extensions/Xsp.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#if defined (HAVE_SYS_SHM_H) && !defined (MODULE_NAME_IS_glx)
#   include <sys/shm.h>                                /* shmget(), shmctl() */
#   include <X11/extensions/XShm.h>
#endif
#ifdef DPMSINFO_IN_DPMS_H
#   include <X11/extensions/dpms.h>
#endif

#ifdef MODULE_NAME_IS_glx
#   include <GL/glx.h>
#endif

#ifdef MODULE_NAME_IS_xvmc
#   include <X11/extensions/Xv.h>
#   include <X11/extensions/Xvlib.h>
#   include <X11/extensions/vldXvMC.h>
#   include "../../codec/xvmc/accel_xvmc.h"
#endif

#include "xcommon.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  Activate   ( vlc_object_t * );
void Deactivate ( vlc_object_t * );

#ifndef MODULE_NAME_IS_glx
static int  InitVideo      ( vout_thread_t * );
static void EndVideo       ( vout_thread_t * );
static void DisplayVideo   ( vout_thread_t *, picture_t * );
static int  InitDisplay    ( vout_thread_t * );
#endif
static int  ManageVideo    ( vout_thread_t * );
static int  Control        ( vout_thread_t *, int, va_list );

static int  CreateWindow   ( vout_thread_t *, x11_window_t * );
static void DestroyWindow  ( vout_thread_t *, x11_window_t * );

#ifndef MODULE_NAME_IS_glx
static int  NewPicture     ( vout_thread_t *, picture_t * );
static void FreePicture    ( vout_thread_t *, picture_t * );
# ifdef HAVE_SYS_SHM_H
static int i_shm_major = 0;
# endif
#endif

static void ToggleFullScreen      ( vout_thread_t * );

static void EnableXScreenSaver    ( vout_thread_t * );
static void DisableXScreenSaver   ( vout_thread_t * );

static void CreateCursor   ( vout_thread_t * );
static void DestroyCursor  ( vout_thread_t * );
static void ToggleCursor   ( vout_thread_t * );

#if defined(MODULE_NAME_IS_xvmc)
static int  XVideoGetPort    ( vout_thread_t *, vlc_fourcc_t, picture_heap_t * );
static void RenderVideo    ( vout_thread_t *, picture_t * );
//static int  xvmc_check_yv12( Display *display, XvPortID port );
//static void xvmc_update_XV_DOUBLE_BUFFER( vout_thread_t *p_vout );
#endif

static int X11ErrorHandler( Display *, XErrorEvent * );

#ifdef HAVE_XSP
static void EnablePixelDoubling( vout_thread_t *p_vout );
static void DisablePixelDoubling( vout_thread_t *p_vout );
#endif



/*****************************************************************************
 * Activate: allocate X11 video thread output method
 *****************************************************************************
 * This function allocate and initialize a X11 vout method. It uses some of the
 * vout properties to choose the window size, and change them according to the
 * actual properties of the display.
 *****************************************************************************/
int Activate ( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *        psz_display;
#if defined(MODULE_NAME_IS_xvmc)
    char *psz_value;
    char *       psz_chroma;
    vlc_fourcc_t i_chroma = 0;
    bool   b_chroma = 0;
#endif

#ifndef MODULE_NAME_IS_glx
    p_vout->pf_init = InitVideo;
    p_vout->pf_end = EndVideo;
    p_vout->pf_display = DisplayVideo;
#endif
#ifdef MODULE_NAME_IS_xvmc
    p_vout->pf_render = RenderVideo;
#else
    p_vout->pf_render = NULL;
#endif
    p_vout->pf_manage = ManageVideo;
    p_vout->pf_control = Control;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    /* Open display, using the "display" config variable or the DISPLAY
     * environment variable */
    psz_display = config_GetPsz( p_vout, MODULE_STRING "-display" );

    p_vout->p_sys->p_display = XOpenDisplay( psz_display );

    if( p_vout->p_sys->p_display == NULL )                         /* error */
    {
        msg_Err( p_vout, "cannot open display %s",
                         XDisplayName( psz_display ) );
        free( p_vout->p_sys );
        free( psz_display );
        return VLC_EGENERIC;
    }
    free( psz_display );

    /* Replace error handler so we can intercept some non-fatal errors */
    XSetErrorHandler( X11ErrorHandler );

    /* Get a screen ID matching the XOpenDisplay return value */
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

#if defined(MODULE_NAME_IS_xvmc)
    psz_chroma = config_GetPsz( p_vout, "xvideo-chroma" );
    if( psz_chroma )
    {
        if( strlen( psz_chroma ) >= 4 )
        {
            /* Do not use direct assignment because we are not sure of the
             * alignment. */
            memcpy(&i_chroma, psz_chroma, 4);
            b_chroma = 1;
        }

        free( psz_chroma );
    }

    if( b_chroma )
    {
        msg_Dbg( p_vout, "forcing chroma 0x%.8x (%4.4s)",
                 i_chroma, (char*)&i_chroma );
    }
    else
    {
        i_chroma = p_vout->render.i_chroma;
    }

    /* Check that we have access to an XVideo port providing this chroma */
    p_vout->p_sys->i_xvport = XVideoGetPort( p_vout, VLC2X11_FOURCC(i_chroma),
                                             &p_vout->output );
    if( p_vout->p_sys->i_xvport < 0 )
    {
        /* If a specific chroma format was requested, then we don't try to
         * be cleverer than the user. He knew pretty well what he wanted. */
        if( b_chroma )
        {
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }

        /* It failed, but it's not completely lost ! We try to open an
         * XVideo port for an YUY2 picture. We'll need to do an YUV
         * conversion, but at least it has got scaling. */
        p_vout->p_sys->i_xvport =
                        XVideoGetPort( p_vout, X11_FOURCC('Y','U','Y','2'),
                                               &p_vout->output );
        if( p_vout->p_sys->i_xvport < 0 )
        {
            /* It failed, but it's not completely lost ! We try to open an
             * XVideo port for a simple 16bpp RGB picture. We'll need to do
             * an YUV conversion, but at least it has got scaling. */
            p_vout->p_sys->i_xvport =
                            XVideoGetPort( p_vout, X11_FOURCC('R','V','1','6'),
                                                   &p_vout->output );
            if( p_vout->p_sys->i_xvport < 0 )
            {
                XCloseDisplay( p_vout->p_sys->p_display );
                free( p_vout->p_sys );
                return VLC_EGENERIC;
            }
        }
    }
    p_vout->output.i_chroma = vlc_fourcc_GetCodec( VIDEO_ES, X112VLC_FOURCC(p_vout->output.i_chroma) );
#elif defined(MODULE_NAME_IS_glx)
    {
        int i_opcode, i_evt, i_err = 0;
        int i_maj, i_min = 0;

        /* Check for GLX extension */
        if( !XQueryExtension( p_vout->p_sys->p_display, "GLX",
                              &i_opcode, &i_evt, &i_err ) )
        {
            msg_Err( p_this, "GLX extension not supported" );
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }
        if( !glXQueryExtension( p_vout->p_sys->p_display, &i_err, &i_evt ) )
        {
            msg_Err( p_this, "glXQueryExtension failed" );
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }

        /* Check GLX version */
        if (!glXQueryVersion( p_vout->p_sys->p_display, &i_maj, &i_min ) )
        {
            msg_Err( p_this, "glXQueryVersion failed" );
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }
        if( i_maj <= 0 || ((i_maj == 1) && (i_min < 3)) )
        {
            p_vout->p_sys->b_glx13 = false;
            msg_Dbg( p_this, "using GLX 1.2 API" );
        }
        else
        {
            p_vout->p_sys->b_glx13 = true;
            msg_Dbg( p_this, "using GLX 1.3 API" );
        }
    }
#endif

    /* Create blank cursor (for mouse cursor autohiding) */
    p_vout->p_sys->i_time_mouse_last_moved = mdate();
    p_vout->p_sys->i_mouse_hide_timeout =
        var_GetInteger(p_vout, "mouse-hide-timeout") * 1000;
    p_vout->p_sys->b_mouse_pointer_visible = 1;
    CreateCursor( p_vout );

    /* Set main window's size */
    p_vout->p_sys->window.i_x      = 0;
    p_vout->p_sys->window.i_y      = 0;
    p_vout->p_sys->window.i_width  = p_vout->i_window_width;
    p_vout->p_sys->window.i_height = p_vout->i_window_height;
    var_Create( p_vout, "video-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */
    if( CreateWindow( p_vout, &p_vout->p_sys->window ) )
    {
        msg_Err( p_vout, "cannot create X11 window" );
        DestroyCursor( p_vout );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

#ifndef MODULE_NAME_IS_glx
    /* Open and initialize device. */
    if( InitDisplay( p_vout ) )
    {
        msg_Err( p_vout, "cannot initialize X11 display" );
        DestroyCursor( p_vout );
        DestroyWindow( p_vout, &p_vout->p_sys->window );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }
#endif

    /* Disable screen saver */
    DisableXScreenSaver( p_vout );

    /* Misc init */
    p_vout->p_sys->i_time_button_last_pressed = 0;

#ifdef MODULE_NAME_IS_xvmc
    p_vout->p_sys->p_last_subtitle_save = NULL;
    psz_value = config_GetPsz( p_vout, "xvmc-deinterlace-mode" );

    /* Look what method was requested */
    //var_Create( p_vout, "xvmc-deinterlace-mode", VLC_VAR_STRING );
    //var_Change( p_vout, "xvmc-deinterlace-mode", VLC_VAR_INHERITVALUE, &val, NULL );
    if( psz_value )
    {
        if( (strcmp(psz_value, "bob") == 0) ||
            (strcmp(psz_value, "blend") == 0) )
           p_vout->p_sys->xvmc_deinterlace_method = 2;
        else if (strcmp(psz_value, "discard") == 0)
           p_vout->p_sys->xvmc_deinterlace_method = 1;
        else
           p_vout->p_sys->xvmc_deinterlace_method = 0;
        free(psz_value );
    }
    else
        p_vout->p_sys->xvmc_deinterlace_method = 0;

    /* Look what method was requested */
    //var_Create( p_vout, "xvmc-crop-style", VLC_VAR_STRING );
    //var_Change( p_vout, "xvmc-crop-style", VLC_VAR_INHERITVALUE, &val, NULL );
    psz_value = config_GetPsz( p_vout, "xvmc-crop-style" );

    if( psz_value )
    {
        if( strncmp( psz_value, "eq", 2 ) == 0 )
           p_vout->p_sys->xvmc_crop_style = 1;
        else if( strncmp( psz_value, "4-16", 4 ) == 0)
           p_vout->p_sys->xvmc_crop_style = 2;
        else if( strncmp( psz_value, "16-4", 4 ) == 0)
           p_vout->p_sys->xvmc_crop_style = 3;
        else
           p_vout->p_sys->xvmc_crop_style = 0;
        free( psz_value );
    }
    else
        p_vout->p_sys->xvmc_crop_style = 0;

    msg_Dbg(p_vout, "Deinterlace = %d", p_vout->p_sys->xvmc_deinterlace_method);
    msg_Dbg(p_vout, "Crop = %d", p_vout->p_sys->xvmc_crop_style);

    if( checkXvMCCap( p_vout ) == VLC_EGENERIC )
    {
        msg_Err( p_vout, "no XVMC capability found" );
        Deactivate( p_this );
        return VLC_EGENERIC;
    }
    subpicture_t sub_pic;
    sub_pic.p_sys = NULL;
    p_vout->p_sys->last_date = 0;
#endif

#ifdef HAVE_XSP
    p_vout->p_sys->i_hw_scale = 1;
#endif

    /* Variable to indicate if the window should be on top of others */
    /* Trigger a callback right now */
    var_TriggerCallback( p_vout, "video-on-top" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: destroy X11 video thread output method
 *****************************************************************************
 * Terminate an output method created by Open
 *****************************************************************************/
void Deactivate ( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Restore cursor if it was blanked */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
    }

#if defined(MODULE_NAME_IS_xvmc)
    if( p_vout->p_sys->xvmc_cap )
    {
        xvmc_context_writer_lock( &p_vout->p_sys->xvmc_lock );
        xxmc_dispose_context( p_vout );
        if( p_vout->p_sys->old_subpic )
        {
            xxmc_xvmc_free_subpicture( p_vout, p_vout->p_sys->old_subpic );
            p_vout->p_sys->old_subpic = NULL;
        }
        if( p_vout->p_sys->new_subpic )
        {
            xxmc_xvmc_free_subpicture( p_vout, p_vout->p_sys->new_subpic );
            p_vout->p_sys->new_subpic = NULL;
        }
        free( p_vout->p_sys->xvmc_cap );
        xvmc_context_writer_unlock( &p_vout->p_sys->xvmc_lock );
    }
#endif

#ifdef HAVE_XSP
    DisablePixelDoubling(p_vout);
#endif

    DestroyCursor( p_vout );
    EnableXScreenSaver( p_vout );
    DestroyWindow( p_vout, &p_vout->p_sys->window );
    XCloseDisplay( p_vout->p_sys->p_display );

    /* Destroy structure */
#ifdef MODULE_NAME_IS_xvmc
    free_context_lock( &p_vout->p_sys->xvmc_lock );
#endif

    free( p_vout->p_sys );
}

#ifdef MODULE_NAME_IS_xvmc

#define XINE_IMGFMT_YV12 (('2'<<24)|('1'<<16)|('V'<<8)|'Y')

/* called xlocked */
#if 0
static int xvmc_check_yv12( Display *display, XvPortID port )
{
    XvImageFormatValues *formatValues;
    int                  formats;
    int                  i;

    formatValues = XvListImageFormats( display, port, &formats );

    for( i = 0; i < formats; i++ )
    {
        if( ( formatValues[i].id == XINE_IMGFMT_YV12 ) &&
            ( !( strncmp( formatValues[i].guid, "YV12", 4 ) ) ) )
        {
            XFree (formatValues);
            return 0;
        }
    }

    XFree (formatValues);
    return 1;
}
#endif

#if 0
static void xvmc_sync_surface( vout_thread_t *p_vout, XvMCSurface * srf )
{
    XvMCSyncSurface( p_vout->p_sys->p_display, srf );
}
#endif

#if 0
static void xvmc_update_XV_DOUBLE_BUFFER( vout_thread_t *p_vout )
{
    Atom         atom;
    int          xv_double_buffer;

    xv_double_buffer = 1;

    XLockDisplay( p_vout->p_sys->p_display );
    atom = XInternAtom( p_vout->p_sys->p_display, "XV_DOUBLE_BUFFER", False );
#if 0
    XvSetPortAttribute (p_vout->p_sys->p_display, p_vout->p_sys->i_xvport, atom, xv_double_buffer);
#endif
    XvMCSetAttribute( p_vout->p_sys->p_display, &p_vout->p_sys->context, atom, xv_double_buffer );
    XUnlockDisplay( p_vout->p_sys->p_display );

    //xprintf(this->xine, XINE_VERBOSITY_DEBUG,
    //    "video_out_xxmc: double buffering mode = %d\n", xv_double_buffer);
}
#endif

static void RenderVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_xxmc_t *xxmc = NULL;

    xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );

    xxmc = &p_pic->p_sys->xxmc_data;
    if( (!xxmc->decoded ||
        !xxmc_xvmc_surface_valid( p_vout, p_pic->p_sys->xvmc_surf )) )
    {
        xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
        return;
    }

#if 0
    vlc_mutex_lock( &p_vout->lastsubtitle_lock );
    if (p_vout->p_sys->p_last_subtitle != NULL)
    {
        if( p_vout->p_sys->p_last_subtitle_save != p_vout->p_sys->p_last_subtitle )
        {
            p_vout->p_sys->new_subpic =
                xxmc_xvmc_alloc_subpicture( p_vout, &p_vout->p_sys->context,
                    p_vout->p_sys->xvmc_width,
                    p_vout->p_sys->xvmc_height,
                    p_vout->p_sys->xvmc_cap[p_vout->p_sys->xvmc_cur_cap].subPicType.id );

            if (p_vout->p_sys->new_subpic)
            {
                XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
                XvMCClearSubpicture( p_vout->p_sys->p_display,
                        p_vout->p_sys->new_subpic,
                        0,
                        0,
                        p_vout->p_sys->xvmc_width,
                        p_vout->p_sys->xvmc_height,
                        0x00 );
                XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
                clear_xx44_palette( &p_vout->p_sys->palette );

                if( sub_pic.p_sys == NULL )
                {
                    sub_pic.p_sys = malloc( sizeof( picture_sys_t ) );
                    if( sub_pic.p_sys != NULL )
                    {
                        sub_pic.p_sys->p_vout = p_vout;
                        sub_pic.p_sys->xvmc_surf = NULL;
                        sub_pic.p_sys->p_image = p_vout->p_sys->subImage;
                    }
                }
                sub_pic.p_sys->p_image = p_vout->p_sys->subImage;
                sub_pic.p->p_pixels = sub_pic.p_sys->p_image->data;
                sub_pic.p->i_pitch = p_vout->output.i_width;

                memset( p_vout->p_sys->subImage->data, 0,
                        (p_vout->p_sys->subImage->width * p_vout->p_sys->subImage->height) );

                if (p_vout->p_last_subtitle != NULL)
                {
                    blend_xx44( p_vout->p_sys->subImage->data,
                                p_vout->p_last_subtitle,
                                p_vout->p_sys->subImage->width,
                                p_vout->p_sys->subImage->height,
                                p_vout->p_sys->subImage->width,
                                &p_vout->p_sys->palette,
                                (p_vout->p_sys->subImage->id == FOURCC_IA44) );
                }

                XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
                XvMCCompositeSubpicture( p_vout->p_sys->p_display,
                                         p_vout->p_sys->new_subpic,
                                         p_vout->p_sys->subImage,
                                         0, /* overlay->x */
                                         0, /* overlay->y */
                                         p_vout->output.i_width, /* overlay->width, */
                                         p_vout->output.i_height, /* overlay->height */
                                         0, /* overlay->x */
                                         0 ); /*overlay->y */
                XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
                if (p_vout->p_sys->old_subpic)
                {
                    xxmc_xvmc_free_subpicture( p_vout,
                                               p_vout->p_sys->old_subpic);
                    p_vout->p_sys->old_subpic = NULL;
                }
                if (p_vout->p_sys->new_subpic)
                {
                    p_vout->p_sys->old_subpic = p_vout->p_sys->new_subpic;
                    p_vout->p_sys->new_subpic = NULL;
                    xx44_to_xvmc_palette( &p_vout->p_sys->palette,
                            p_vout->p_sys->xvmc_palette,
                            0,
                            p_vout->p_sys->old_subpic->num_palette_entries,
                            p_vout->p_sys->old_subpic->entry_bytes,
                            p_vout->p_sys->old_subpic->component_order );
                    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
                    XvMCSetSubpicturePalette( p_vout->p_sys->p_display,
                                              p_vout->p_sys->old_subpic,
                                              p_vout->p_sys->xvmc_palette );
                    XvMCFlushSubpicture( p_vout->p_sys->p_display,
                                         p_vout->p_sys->old_subpic);
                    XvMCSyncSubpicture( p_vout->p_sys->p_display,
                                        p_vout->p_sys->old_subpic );
                    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
                }

                XVMCLOCKDISPLAY( p_vout->p_sys->p_display);
                if (p_vout->p_sys->xvmc_backend_subpic )
                {
                    XvMCBlendSubpicture( p_vout->p_sys->p_display,
                                         p_pic->p_sys->xvmc_surf,
                                         p_vout->p_sys->old_subpic,
                                         0,
                                         0,
                                         p_vout->p_sys->xvmc_width,
                                         p_vout->p_sys->xvmc_height,
                                         0,
                                         0,
                                         p_vout->p_sys->xvmc_width,
                                         p_vout->p_sys->xvmc_height );
                }
                else
                {
                    XvMCBlendSubpicture2( p_vout->p_sys->p_display,
                                          p_pic->p_sys->xvmc_surf,
                                          p_pic->p_sys->xvmc_surf,
                                          p_vout->p_sys->old_subpic,
                                          0,
                                          0,
                                          p_vout->p_sys->xvmc_width,
                                          p_vout->p_sys->xvmc_height,
                                          0,
                                          0,
                                          p_vout->p_sys->xvmc_width,
                                          p_vout->p_sys->xvmc_height );
               }
               XVMCUNLOCKDISPLAY(p_vout->p_sys->p_display);
            }
        }
        else
        {
            XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
            if( p_vout->p_sys->xvmc_backend_subpic )
            {
                XvMCBlendSubpicture( p_vout->p_sys->p_display,
                                     p_pic->p_sys->xvmc_surf,
                                     p_vout->p_sys->old_subpic,
                                     0, 0,
                                     p_vout->p_sys->xvmc_width,
                                     p_vout->p_sys->xvmc_height,
                                     0, 0,
                                     p_vout->p_sys->xvmc_width,
                                     p_vout->p_sys->xvmc_height );
            }
            else
            {
                XvMCBlendSubpicture2( p_vout->p_sys->p_display,
                                      p_pic->p_sys->xvmc_surf,
                                      p_pic->p_sys->xvmc_surf,
                                      p_vout->p_sys->old_subpic,
                                      0, 0,
                                      p_vout->p_sys->xvmc_width,
                                      p_vout->p_sys->xvmc_height,
                                      0, 0,
                                      p_vout->p_sys->xvmc_width,
                                      p_vout->p_sys->xvmc_height );
            }
            XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
        }
    }
    p_vout->p_sys->p_last_subtitle_save = p_vout->p_last_subtitle;

    vlc_mutex_unlock( &p_vout->lastsubtitle_lock );
#endif
    xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
}
#endif

#ifdef HAVE_XSP
/*****************************************************************************
 * EnablePixelDoubling: Enables pixel doubling
 *****************************************************************************
 * Checks if the double size image fits in current window, and enables pixel
 * doubling accordingly. The i_hw_scale is the integer scaling factor.
 *****************************************************************************/
static void EnablePixelDoubling( vout_thread_t *p_vout )
{
    int i_hor_scale = ( p_vout->p_sys->window.i_width ) / p_vout->render.i_width;
    int i_vert_scale =  ( p_vout->p_sys->window.i_height ) / p_vout->render.i_height;
    if ( ( i_hor_scale > 1 ) && ( i_vert_scale > 1 ) ) {
        p_vout->p_sys->i_hw_scale = 2;
        msg_Dbg( p_vout, "Enabling pixel doubling, scaling factor %d", p_vout->p_sys->i_hw_scale );
        XSPSetPixelDoubling( p_vout->p_sys->p_display, 0, 1 );
    }
}

/*****************************************************************************
 * DisablePixelDoubling: Disables pixel doubling
 *****************************************************************************
 * The scaling factor i_hw_scale is reset to the no-scaling value 1.
 *****************************************************************************/
static void DisablePixelDoubling( vout_thread_t *p_vout )
{
    if ( p_vout->p_sys->i_hw_scale > 1 ) {
        msg_Dbg( p_vout, "Disabling pixel doubling" );
        XSPSetPixelDoubling( p_vout->p_sys->p_display, 0, 0 );
        p_vout->p_sys->i_hw_scale = 1;
    }
}
#endif


#if !defined(MODULE_NAME_IS_glx)
/*****************************************************************************
 * InitVideo: initialize X11 video thread output method
 *****************************************************************************
 * This function create the XImages needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int InitVideo( vout_thread_t *p_vout )
{
    unsigned int i_index = 0;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

#if defined(MODULE_NAME_IS_xvmc)
    /* Initialize the output structure; we already found an XVideo port,
     * and the corresponding chroma we will be using. Since we can
     * arbitrary scale, stick to the coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_vout->fmt_out = p_vout->fmt_in;
    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;

#if XvVersion < 2 || ( XvVersion == 2 && XvRevision < 2 )
    switch( p_vout->output.i_chroma )
    {
        case VLC_CODEC_RGB16:
#if defined( WORDS_BIGENDIAN )
            p_vout->output.i_rmask = 0xf800;
            p_vout->output.i_gmask = 0x07e0;
            p_vout->output.i_bmask = 0x001f;
#else
            p_vout->output.i_rmask = 0x001f;
            p_vout->output.i_gmask = 0x07e0;
            p_vout->output.i_bmask = 0xf800;
#endif
            break;
        case VLC_CODEC_RGB15:
#if defined( WORDS_BIGENDIAN )
            p_vout->output.i_rmask = 0x7c00;
            p_vout->output.i_gmask = 0x03e0;
            p_vout->output.i_bmask = 0x001f;
#else
            p_vout->output.i_rmask = 0x001f;
            p_vout->output.i_gmask = 0x03e0;
            p_vout->output.i_bmask = 0x7c00;
#endif
            break;
    }
#endif
#endif

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

    if( p_vout->output.i_chroma == VLC_CODEC_YV12 )
    {
        /* U and V inverted compared to I420
         * Fixme: this should be handled by the vout core */
        p_vout->output.i_chroma = VLC_CODEC_I420;
        p_vout->fmt_out.i_chroma = VLC_CODEC_I420;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DisplayVideo: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to X11 server.
 * (The Xv extension takes care of "double-buffering".)
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    unsigned int i_width, i_height, i_x, i_y;

    vout_PlacePicture( p_vout, p_vout->p_sys->window.i_width,
                       p_vout->p_sys->window.i_height,
                       &i_x, &i_y, &i_width, &i_height );

#ifdef MODULE_NAME_IS_xvmc
    xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );

    vlc_xxmc_t *xxmc = &p_pic->p_sys->xxmc_data;
    if( !xxmc->decoded ||
        !xxmc_xvmc_surface_valid( p_vout, p_pic->p_sys->xvmc_surf ) )
    {
      msg_Dbg( p_vout, "DisplayVideo decoded=%d\tsurfacevalid=%d",
               xxmc->decoded,
               xxmc_xvmc_surface_valid( p_vout, p_pic->p_sys->xvmc_surf ) );
      xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
      return;
    }

    int src_width = p_vout->output.i_width;
    int src_height = p_vout->output.i_height;
    int src_x, src_y;

    if( p_vout->p_sys->xvmc_crop_style == 1 )
    {
        src_x = 20;
        src_y = 20;
        src_width -= 40;
        src_height -= 40;
    }
    else if( p_vout->p_sys->xvmc_crop_style == 2 )
    {
        src_x = 20;
        src_y = 40;
        src_width -= 40;
        src_height -= 80;
    }
    else if( p_vout->p_sys->xvmc_crop_style == 3 )
    {
        src_x = 40;
        src_y = 20;
        src_width -= 80;
        src_height -= 40;
    }
    else
    {
        src_x = 0;
        src_y = 0;
    }

    int first_field;
    if( p_vout->p_sys->xvmc_deinterlace_method > 0 )
    {   /* BOB DEINTERLACE */
        if( (p_pic->p_sys->nb_display == 0) ||
            (p_vout->p_sys->xvmc_deinterlace_method == 1) )
        {
            first_field = (p_pic->b_top_field_first) ?
                                XVMC_BOTTOM_FIELD : XVMC_TOP_FIELD;
        }
        else
        {
            first_field = (p_pic->b_top_field_first) ?
                                XVMC_TOP_FIELD : XVMC_BOTTOM_FIELD;
        }
    }
    else
    {
        first_field = XVMC_FRAME_PICTURE;
     }

    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
    XvMCFlushSurface( p_vout->p_sys->p_display, p_pic->p_sys->xvmc_surf );
    /* XvMCSyncSurface(p_vout->p_sys->p_display, p_picture->p_sys->xvmc_surf); */
    XvMCPutSurface( p_vout->p_sys->p_display,
                    p_pic->p_sys->xvmc_surf,
                    p_vout->p_sys->window.video_window,
                    src_x,
                    src_y,
                    src_width,
                    src_height,
                    0 /*dest_x*/,
                    0 /*dest_y*/,
                    i_width,
                    i_height,
                    first_field);

    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
    if( p_vout->p_sys->xvmc_deinterlace_method == 2 )
    {   /* BOB DEINTERLACE */
        if( p_pic->p_sys->nb_display == 0 )/* && ((t2-t1) < 15000)) */
        {
            mtime_t last_date = p_pic->date;

            vlc_mutex_lock( &p_vout->picture_lock );
            if( !p_vout->p_sys->last_date )
            {
                p_pic->date += 20000;
            }
            else
            {
                p_pic->date = ((3 * p_pic->date -
                                    p_vout->p_sys->last_date) / 2 );
            }
            p_vout->p_sys->last_date = last_date;
            p_pic->b_force = 1;
            p_pic->p_sys->nb_display = 1;
            vlc_mutex_unlock( &p_vout->picture_lock );
        }
        else
        {
            p_pic->p_sys->nb_display = 0;
            p_pic->b_force = 0;
        }
    }
    xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
#endif

#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->i_shm_opcode )
    {
        /* Display rendered image using shared memory extension */
        XvShmPutImage( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport,
                       p_vout->p_sys->window.video_window,
                       p_vout->p_sys->window.gc, p_pic->p_sys->p_image,
                       p_vout->fmt_out.i_x_offset,
                       p_vout->fmt_out.i_y_offset,
                       p_vout->fmt_out.i_visible_width,
                       p_vout->fmt_out.i_visible_height,
                       0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height,
                       False /* Don't put True here or you'll waste your CPU */ );
    }
    else
#endif /* HAVE_SYS_SHM_H */
    {
        /* Use standard XPutImage -- this is gonna be slow ! */
        XvPutImage( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport,
                    p_vout->p_sys->window.video_window,
                    p_vout->p_sys->window.gc, p_pic->p_sys->p_image,
                    p_vout->fmt_out.i_x_offset,
                    p_vout->fmt_out.i_y_offset,
                    p_vout->fmt_out.i_visible_width,
                    p_vout->fmt_out.i_visible_height,
                    0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height );
    }

    /* Make sure the command is sent now - do NOT use XFlush !*/
    XSync( p_vout->p_sys->p_display, False );
}
#endif

/*****************************************************************************
 * ManageVideo: handle X11 events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a non null value on
 * error.
 *****************************************************************************/
static int ManageVideo( vout_thread_t *p_vout )
{
    XEvent      xevent;                                         /* X11 event */
    vlc_value_t val;

#ifdef MODULE_NAME_IS_xvmc
    xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );
#endif

    /* Handle events from the owner window */
    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->window.owner_window->handle.xid,
                              StructureNotifyMask, &xevent ) == True )
    {
        /* ConfigureNotify event: prepare  */
        if( xevent.type == ConfigureNotify )
            /* Update dimensions */
            XResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->window.base_window,
                           xevent.xconfigure.width,
                           xevent.xconfigure.height );
    }

    /* Handle X11 events: ConfigureNotify events are parsed to know if the
     * output window's size changed, MapNotify and UnmapNotify to know if the
     * window is mapped (and if the display is useful), and ClientMessages
     * to intercept window destruction requests */

    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->window.base_window,
                              StructureNotifyMask |
                              ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask | Button1MotionMask , &xevent )
           == True )
    {
        /* ConfigureNotify event: prepare  */
        if( xevent.type == ConfigureNotify )
        {
            if( (unsigned int)xevent.xconfigure.width
                   != p_vout->p_sys->window.i_width
              || (unsigned int)xevent.xconfigure.height
                    != p_vout->p_sys->window.i_height )
            {
                /* Update dimensions */
                p_vout->i_changes |= VOUT_SIZE_CHANGE;
                p_vout->p_sys->window.i_width = xevent.xconfigure.width;
                p_vout->p_sys->window.i_height = xevent.xconfigure.height;
            }
        }
        /* Mouse click */
        else if( xevent.type == ButtonPress )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 1;
                    var_Set( p_vout, "mouse-button-down", val );

                    var_SetBool( p_vout->p_libvlc, "intf-popupmenu", false );

                    /* detect double-clicks */
                    if( ( ((XButtonEvent *)&xevent)->time -
                          p_vout->p_sys->i_time_button_last_pressed ) < 300 )
                    {
                        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                    }

                    p_vout->p_sys->i_time_button_last_pressed =
                        ((XButtonEvent *)&xevent)->time;
                    break;
                case Button2:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 2;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button3:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 4;
                    var_Set( p_vout, "mouse-button-down", val );
                    var_SetBool( p_vout->p_libvlc, "intf-popupmenu", true );
                    break;

                case Button4:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 8;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button5:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 16;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;
            }
        }
        /* Mouse release */
        else if( xevent.type == ButtonRelease )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    {
                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~1;
                        var_Set( p_vout, "mouse-button-down", val );

                        var_SetBool( p_vout, "mouse-clicked", true );
                    }
                    break;

                case Button2:
                    {
                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~2;
                        var_Set( p_vout, "mouse-button-down", val );

                        var_ToggleBool( p_vout->p_libvlc, "intf-show" );
                    }
                    break;

                case Button3:
                    {
                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~4;
                        var_Set( p_vout, "mouse-button-down", val );

                    }
                    break;

                case Button4:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~8;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button5:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~16;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

            }
        }
        /* Mouse move */
        else if( xevent.type == MotionNotify )
        {
            unsigned int i_width, i_height, i_x, i_y;

            /* somewhat different use for vout_PlacePicture:
             * here the values are needed to give to mouse coordinates
             * in the original picture space */
            vout_PlacePicture( p_vout, p_vout->p_sys->window.i_width,
                               p_vout->p_sys->window.i_height,
                               &i_x, &i_y, &i_width, &i_height );

            /* Compute the x coordinate and check if the value is
               in [0,p_vout->fmt_in.i_visible_width] */
            val.i_int = ( xevent.xmotion.x - i_x ) *
                p_vout->fmt_in.i_visible_width / i_width +
                p_vout->fmt_in.i_x_offset;

            if( (int)(xevent.xmotion.x - i_x) < 0 )
                val.i_int = 0;
            else if( (unsigned int)val.i_int > p_vout->fmt_in.i_visible_width )
                val.i_int = p_vout->fmt_in.i_visible_width;

            var_Set( p_vout, "mouse-x", val );

            /* compute the y coordinate and check if the value is
               in [0,p_vout->fmt_in.i_visible_height] */
            val.i_int = ( xevent.xmotion.y - i_y ) *
                p_vout->fmt_in.i_visible_height / i_height +
                p_vout->fmt_in.i_y_offset;

            if( (int)(xevent.xmotion.y - i_y) < 0 )
                val.i_int = 0;
            else if( (unsigned int)val.i_int > p_vout->fmt_in.i_visible_height )
                val.i_int = p_vout->fmt_in.i_visible_height;

            var_Set( p_vout, "mouse-y", val );

            var_SetBool( p_vout, "mouse-moved", true );

            p_vout->p_sys->i_time_mouse_last_moved = mdate();
            if( ! p_vout->p_sys->b_mouse_pointer_visible )
            {
                ToggleCursor( p_vout );
            }
        }
        else if( xevent.type == ReparentNotify /* XXX: why do we get this? */
                  || xevent.type == MapNotify
                  || xevent.type == UnmapNotify )
        {
            /* Ignore these events */
        }
        else /* Other events */
        {
            msg_Warn( p_vout, "unhandled event %d received", xevent.type );
        }
    }

    /* Handle events for video output sub-window */
    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->window.video_window,
                              ExposureMask, &xevent ) == True );

    /* ClientMessage event - only WM_PROTOCOLS with WM_DELETE_WINDOW data
     * are handled - according to the man pages, the format is always 32
     * in this case */
    while( XCheckTypedEvent( p_vout->p_sys->p_display,
                             ClientMessage, &xevent ) )
    {
        if( (xevent.xclient.message_type == p_vout->p_sys->window.wm_protocols)
               && ((Atom)xevent.xclient.data.l[0]
                     == p_vout->p_sys->window.wm_delete_window ) )
        {
            /* the user wants to close the window */
            playlist_t * p_playlist = pl_Hold( p_vout );
            if( p_playlist != NULL )
            {
                playlist_Stop( p_playlist );
                pl_Release( p_vout );
            }
        }
    }

    /*
     * Fullscreen Change
     */
    if ( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        /* Update the object variable and trigger callback */
        var_SetBool( p_vout, "fullscreen", !p_vout->b_fullscreen );

        ToggleFullScreen( p_vout );
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    /* autoscale toggle */
    if( p_vout->i_changes & VOUT_SCALE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SCALE_CHANGE;

        p_vout->b_autoscale = var_GetBool( p_vout, "autoscale" );
        p_vout->i_zoom = ZOOM_FP_FACTOR;

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /* scaling factor */
    if( p_vout->i_changes & VOUT_ZOOM_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_ZOOM_CHANGE;

        p_vout->b_autoscale = false;
        p_vout->i_zoom =
            (int)( ZOOM_FP_FACTOR * var_GetFloat( p_vout, "scale" ) );

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    if( p_vout->i_changes & VOUT_CROP_CHANGE ||
        p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;

        p_vout->fmt_out.i_x_offset = p_vout->fmt_in.i_x_offset;
        p_vout->fmt_out.i_y_offset = p_vout->fmt_in.i_y_offset;
        p_vout->fmt_out.i_visible_width = p_vout->fmt_in.i_visible_width;
        p_vout->fmt_out.i_visible_height = p_vout->fmt_in.i_visible_height;
        p_vout->fmt_out.i_aspect = p_vout->fmt_in.i_aspect;
        p_vout->fmt_out.i_sar_num = p_vout->fmt_in.i_sar_num;
        p_vout->fmt_out.i_sar_den = p_vout->fmt_in.i_sar_den;
        p_vout->output.i_aspect = p_vout->fmt_in.i_aspect;

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /*
     * Size change
     *
     * (Needs to be placed after VOUT_FULLSREEN_CHANGE because we can activate
     *  the size flag inside the fullscreen routine)
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        unsigned int i_width, i_height, i_x, i_y;

        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        vout_PlacePicture( p_vout, p_vout->p_sys->window.i_width,
                           p_vout->p_sys->window.i_height,
                           &i_x, &i_y, &i_width, &i_height );

        XMoveResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->window.video_window,
                           i_x, i_y, i_width, i_height );
    }

    /* Autohide Cursour */
    if( mdate() - p_vout->p_sys->i_time_mouse_last_moved >
        p_vout->p_sys->i_mouse_hide_timeout )
    {
        /* Hide the mouse automatically */
        if( p_vout->p_sys->b_mouse_pointer_visible )
        {
            ToggleCursor( p_vout );
        }
    }

#ifdef MODULE_NAME_IS_xvmc
    xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
#endif

    return 0;
}

#if !defined( MODULE_NAME_IS_glx )
/*****************************************************************************
 * EndVideo: terminate X11 video thread output method
 *****************************************************************************
 * Destroy the X11 XImages created by Init. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void EndVideo( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the direct buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        FreePicture( p_vout, PP_OUTPUTPICTURE[ i_index ] );
    }
}
#endif

/* following functions are local */

/*****************************************************************************
 * CreateWindow: open and set-up X11 main window
 *****************************************************************************/
static int CreateWindow( vout_thread_t *p_vout, x11_window_t *p_win )
{
    XSizeHints              xsize_hints;
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    XEvent                  xevent;

    bool              b_map_notify = false;

    /* Prepare window manager hints and properties */
    p_win->wm_protocols =
             XInternAtom( p_vout->p_sys->p_display, "WM_PROTOCOLS", True );
    p_win->wm_delete_window =
             XInternAtom( p_vout->p_sys->p_display, "WM_DELETE_WINDOW", True );

    /* Never have a 0-pixel-wide window */
    xsize_hints.min_width = 2;
    xsize_hints.min_height = 1;

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;       /* save the hidden part */
    xwindow_attributes.background_pixel = BlackPixel(p_vout->p_sys->p_display,
                                                     p_vout->p_sys->i_screen);
    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask;

    {
        vout_window_cfg_t wnd_cfg;
        memset( &wnd_cfg, 0, sizeof(wnd_cfg) );
        wnd_cfg.type   = VOUT_WINDOW_TYPE_XID;
        wnd_cfg.x      = p_win->i_x;
        wnd_cfg.y      = p_win->i_y;
        wnd_cfg.width  = p_win->i_width;
        wnd_cfg.height = p_win->i_height;

        p_win->owner_window = vout_window_New( VLC_OBJECT(p_vout), NULL, &wnd_cfg );
        if( !p_win->owner_window )
            return VLC_EGENERIC;
        xsize_hints.base_width  = xsize_hints.width = p_win->i_width;
        xsize_hints.base_height = xsize_hints.height = p_win->i_height;
        xsize_hints.flags       = PSize | PMinSize;

        if( p_win->i_x >=0 || p_win->i_y >= 0 )
        {
            xsize_hints.x = p_win->i_x;
            xsize_hints.y = p_win->i_y;
            xsize_hints.flags |= PPosition;
        }

        /* Select events we are interested in. */
        XSelectInput( p_vout->p_sys->p_display,
                      p_win->owner_window->handle.xid, StructureNotifyMask );

        /* Get the parent window's geometry information */
        XGetGeometry( p_vout->p_sys->p_display,
                      p_win->owner_window->handle.xid,
                      &(Window){ 0 }, &(int){ 0 }, &(int){ 0 },
                      &p_win->i_width,
                      &p_win->i_height,
                      &(unsigned){ 0 }, &(unsigned){ 0 } );

        /* From man XSelectInput: only one client at a time can select a
         * ButtonPress event, so we need to open a new window anyway. */
        p_win->base_window =
            XCreateWindow( p_vout->p_sys->p_display,
                           p_win->owner_window->handle.xid,
                           0, 0,
                           p_win->i_width, p_win->i_height,
                           0,
                           0, CopyFromParent, 0,
                           CWBackingStore | CWBackPixel | CWEventMask,
                           &xwindow_attributes );
    }

    if( (p_win->wm_protocols == None)        /* use WM_DELETE_WINDOW */
        || (p_win->wm_delete_window == None)
        || !XSetWMProtocols( p_vout->p_sys->p_display, p_win->base_window,
                             &p_win->wm_delete_window, 1 ) )
    {
        /* WM_DELETE_WINDOW is not supported by window manager */
        msg_Warn( p_vout, "missing or bad window manager" );
    }

    /* Creation of a graphic context that doesn't generate a GraphicsExpose
     * event when using functions like XCopyArea */
    xgcvalues.graphics_exposures = False;
    p_win->gc = XCreateGC( p_vout->p_sys->p_display,
                           p_win->base_window,
                           GCGraphicsExposures, &xgcvalues );

    /* Wait till the window is mapped */
    XMapWindow( p_vout->p_sys->p_display, p_win->base_window );
    do
    {
        XWindowEvent( p_vout->p_sys->p_display, p_win->base_window,
                      SubstructureNotifyMask | StructureNotifyMask, &xevent);
        if( (xevent.type == MapNotify)
                 && (xevent.xmap.window == p_win->base_window) )
        {
            b_map_notify = true;
        }
        else if( (xevent.type == ConfigureNotify)
                 && (xevent.xconfigure.window == p_win->base_window) )
        {
            p_win->i_width = xevent.xconfigure.width;
            p_win->i_height = xevent.xconfigure.height;
        }
    } while( !b_map_notify );

    long mask = StructureNotifyMask | PointerMotionMask;
    if( var_CreateGetBool( p_vout, "mouse-events" ) )
        mask |= ButtonPressMask | ButtonReleaseMask;
    XSelectInput( p_vout->p_sys->p_display, p_win->base_window, mask );

    /* Create video output sub-window. */
    p_win->video_window =  XCreateSimpleWindow(
                                      p_vout->p_sys->p_display,
                                      p_win->base_window, 0, 0,
                                      p_win->i_width, p_win->i_height,
                                      0,
                                      BlackPixel( p_vout->p_sys->p_display,
                                                  p_vout->p_sys->i_screen ),
                                      WhitePixel( p_vout->p_sys->p_display,
                                                  p_vout->p_sys->i_screen ) );

    XSetWindowBackground( p_vout->p_sys->p_display, p_win->video_window,
                          BlackPixel( p_vout->p_sys->p_display,
                                      p_vout->p_sys->i_screen ) );

    XMapWindow( p_vout->p_sys->p_display, p_win->video_window );
    XSelectInput( p_vout->p_sys->p_display, p_win->video_window,
                  ExposureMask );

    /* make sure the video window will be centered in the next ManageVideo() */
    p_vout->i_changes |= VOUT_SIZE_CHANGE;

    /* If the cursor was formerly blank than blank it again */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
        ToggleCursor( p_vout );
    }

    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DestroyWindow: destroy the window
 *****************************************************************************
 *
 *****************************************************************************/
static void DestroyWindow( vout_thread_t *p_vout, x11_window_t *p_win )
{
    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    if( p_win->video_window != None )
        XDestroyWindow( p_vout->p_sys->p_display, p_win->video_window );

    XFreeGC( p_vout->p_sys->p_display, p_win->gc );

    XUnmapWindow( p_vout->p_sys->p_display, p_win->base_window );
    XDestroyWindow( p_vout->p_sys->p_display, p_win->base_window );

    /* make sure base window is destroyed before proceeding further */
    bool b_destroy_notify = false;
    do
    {
        XEvent      xevent;
        XWindowEvent( p_vout->p_sys->p_display, p_win->base_window,
                      SubstructureNotifyMask | StructureNotifyMask, &xevent);
        if( (xevent.type == DestroyNotify)
                 && (xevent.xmap.window == p_win->base_window) )
        {
            b_destroy_notify = true;
        }
    } while( !b_destroy_notify );

    vout_window_Delete( p_win->owner_window );
}

/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
#if !defined(MODULE_NAME_IS_glx)
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_plane;

    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

#ifdef MODULE_NAME_IS_xvmc
    p_pic->p_sys->p_vout = p_vout;
    p_pic->p_sys->xvmc_surf = NULL;
    p_pic->p_sys->xxmc_data.decoded = 0;
    p_pic->p_sys->xxmc_data.proc_xxmc_update_frame = xxmc_do_update_frame;
    //    p_pic->p_accel_data = &p_pic->p_sys->xxmc_data;
    p_pic->p_sys->nb_display = 0;
#endif

    /* Fill in picture_t fields */
    if( picture_Setup( p_pic, p_vout->output.i_chroma,
                       p_vout->output.i_width, p_vout->output.i_height,
                       p_vout->output.i_aspect ) )
        return -1;

#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->i_shm_opcode )
    {
        /* Create image using XShm extension */
        p_pic->p_sys->p_image =
            CreateShmImage( p_vout, p_vout->p_sys->p_display,
                            p_vout->p_sys->i_xvport,
                            VLC2X11_FOURCC(p_vout->output.i_chroma),
                            &p_pic->p_sys->shminfo,
                            p_vout->output.i_width, p_vout->output.i_height );
    }

    if( !p_vout->p_sys->i_shm_opcode || !p_pic->p_sys->p_image )
#endif /* HAVE_SYS_SHM_H */
    {
        /* Create image without XShm extension */
        p_pic->p_sys->p_image =
            CreateImage( p_vout, p_vout->p_sys->p_display,
                         p_vout->p_sys->i_xvport,
                         VLC2X11_FOURCC(p_vout->output.i_chroma),
                         p_pic->format.i_bits_per_pixel,
                         p_vout->output.i_width, p_vout->output.i_height );

#ifdef HAVE_SYS_SHM_H
        if( p_pic->p_sys->p_image && p_vout->p_sys->i_shm_opcode )
        {
            msg_Warn( p_vout, "couldn't create SHM image, disabling SHM" );
            p_vout->p_sys->i_shm_opcode = 0;
        }
#endif /* HAVE_SYS_SHM_H */
    }

    if( p_pic->p_sys->p_image == NULL )
    {
        free( p_pic->p_sys );
        return -1;
    }

    switch( p_vout->output.i_chroma )
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_YV12:
        case VLC_CODEC_Y211:
        case VLC_CODEC_YUYV:
        case VLC_CODEC_UYVY:
        case VLC_CODEC_RGB15:
        case VLC_CODEC_RGB16:
        case VLC_CODEC_RGB24: /* Fixme: pixel pitch == 4 ? */
        case VLC_CODEC_RGB32:

            for( i_plane = 0; i_plane < p_pic->p_sys->p_image->num_planes;
                 i_plane++ )
            {
                p_pic->p[i_plane].p_pixels = (uint8_t*)p_pic->p_sys->p_image->data
                    + p_pic->p_sys->p_image->offsets[i_plane];
                p_pic->p[i_plane].i_pitch =
                    p_pic->p_sys->p_image->pitches[i_plane];
            }
            if( p_vout->output.i_chroma == VLC_CODEC_YV12 )
            {
                /* U and V inverted compared to I420
                 * Fixme: this should be handled by the vout core */
                p_pic->U_PIXELS = (uint8_t*)p_pic->p_sys->p_image->data
                    + p_pic->p_sys->p_image->offsets[2];
                p_pic->V_PIXELS = (uint8_t*)p_pic->p_sys->p_image->data
                    + p_pic->p_sys->p_image->offsets[1];
            }

            break;

        default:
            /* Unknown chroma, tell the guy to get lost */
            IMAGE_FREE( p_pic->p_sys->p_image );
            free( p_pic->p_sys );
            msg_Err( p_vout, "never heard of chroma 0x%.8x (%4.4s)",
                     p_vout->output.i_chroma, (char*)&p_vout->output.i_chroma );
            p_pic->i_planes = 0;
            return -1;
    }
    return 0;
}

/*****************************************************************************
 * FreePicture: destroy a picture allocated with NewPicture
 *****************************************************************************
 * Destroy XImage AND associated data. If using Shm, detach shared memory
 * segment from server and process, then free it. The XDestroyImage manpage
 * says that both the image structure _and_ the data pointed to by the
 * image structure are freed, so no need to free p_image->data.
 *****************************************************************************/
static void FreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* The order of operations is correct */
#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->i_shm_opcode )
    {
        XShmDetach( p_vout->p_sys->p_display, &p_pic->p_sys->shminfo );
        IMAGE_FREE( p_pic->p_sys->p_image );

        shmctl( p_pic->p_sys->shminfo.shmid, IPC_RMID, 0 );
        if( shmdt( p_pic->p_sys->shminfo.shmaddr ) )
        {
            msg_Err( p_vout, "cannot detach shared memory (%m)" );
        }
    }
    else
#endif
    {
        IMAGE_FREE( p_pic->p_sys->p_image );
    }

#ifdef MODULE_NAME_IS_xvmc
    if( p_pic->p_sys->xvmc_surf != NULL )
    {
        xxmc_xvmc_free_surface(p_vout , p_pic->p_sys->xvmc_surf);
        p_pic->p_sys->xvmc_surf = NULL;
    }
#endif

    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    free( p_pic->p_sys );
}
#endif /* !MODULE_NAME_IS_glx */

/*****************************************************************************
 * ToggleFullScreen: Enable or disable full screen mode
 *****************************************************************************
 * This function will switch between fullscreen and window mode.
 *****************************************************************************/
static void ToggleFullScreen ( vout_thread_t *p_vout )
{
    p_vout->b_fullscreen = !p_vout->b_fullscreen;
    vout_window_SetFullScreen( p_vout->p_sys->window.owner_window,
                               p_vout->b_fullscreen );

#ifdef HAVE_XSP
    if( p_vout->b_fullscreen )
        EnablePixelDoubling( p_vout );
    else
        DisablePixelDoubling( p_vout );
#endif
}

/*****************************************************************************
 * EnableXScreenSaver: enable screen saver
 *****************************************************************************
 * This function enables the screen saver on a display after it has been
 * disabled by XDisableScreenSaver.
 * FIXME: what happens if multiple vlc sessions are running at the same
 *        time ???
 *****************************************************************************/
static void EnableXScreenSaver( vout_thread_t *p_vout )
{
#ifdef DPMSINFO_IN_DPMS_H
    int dummy;
#endif

    if( p_vout->p_sys->i_ss_timeout )
    {
        XSetScreenSaver( p_vout->p_sys->p_display, p_vout->p_sys->i_ss_timeout,
                         p_vout->p_sys->i_ss_interval,
                         p_vout->p_sys->i_ss_blanking,
                         p_vout->p_sys->i_ss_exposure );
    }

    /* Restore DPMS settings */
#ifdef DPMSINFO_IN_DPMS_H
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        if( p_vout->p_sys->b_ss_dpms )
        {
            DPMSEnable( p_vout->p_sys->p_display );
        }
    }
#endif
}

/*****************************************************************************
 * DisableXScreenSaver: disable screen saver
 *****************************************************************************
 * See XEnableXScreenSaver
 *****************************************************************************/
static void DisableXScreenSaver( vout_thread_t *p_vout )
{
#ifdef DPMSINFO_IN_DPMS_H
    int dummy;
#endif

    /* Save screen saver information */
    XGetScreenSaver( p_vout->p_sys->p_display, &p_vout->p_sys->i_ss_timeout,
                     &p_vout->p_sys->i_ss_interval,
                     &p_vout->p_sys->i_ss_blanking,
                     &p_vout->p_sys->i_ss_exposure );

    /* Disable screen saver */
    if( p_vout->p_sys->i_ss_timeout )
    {
        XSetScreenSaver( p_vout->p_sys->p_display, 0,
                         p_vout->p_sys->i_ss_interval,
                         p_vout->p_sys->i_ss_blanking,
                         p_vout->p_sys->i_ss_exposure );
    }

    /* Disable DPMS */
#ifdef DPMSINFO_IN_DPMS_H
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        CARD16 unused;
        /* Save DPMS current state */
        DPMSInfo( p_vout->p_sys->p_display, &unused,
                  &p_vout->p_sys->b_ss_dpms );
        DPMSDisable( p_vout->p_sys->p_display );
   }
#endif
}

/*****************************************************************************
 * CreateCursor: create a blank mouse pointer
 *****************************************************************************/
static void CreateCursor( vout_thread_t *p_vout )
{
    XColor cursor_color;

    p_vout->p_sys->cursor_pixmap =
        XCreatePixmap( p_vout->p_sys->p_display,
                       DefaultRootWindow( p_vout->p_sys->p_display ),
                       1, 1, 1 );

    XParseColor( p_vout->p_sys->p_display,
                 XCreateColormap( p_vout->p_sys->p_display,
                                  DefaultRootWindow(
                                                    p_vout->p_sys->p_display ),
                                  DefaultVisual(
                                                p_vout->p_sys->p_display,
                                                p_vout->p_sys->i_screen ),
                                  AllocNone ),
                 "black", &cursor_color );

    p_vout->p_sys->blank_cursor =
        XCreatePixmapCursor( p_vout->p_sys->p_display,
                             p_vout->p_sys->cursor_pixmap,
                             p_vout->p_sys->cursor_pixmap,
                             &cursor_color, &cursor_color, 1, 1 );
}

/*****************************************************************************
 * DestroyCursor: destroy the blank mouse pointer
 *****************************************************************************/
static void DestroyCursor( vout_thread_t *p_vout )
{
    XFreePixmap( p_vout->p_sys->p_display, p_vout->p_sys->cursor_pixmap );
}

/*****************************************************************************
 * ToggleCursor: hide or show the mouse pointer
 *****************************************************************************
 * This function hides the X pointer if it is visible by setting the pointer
 * sprite to a blank one. To show it again, we disable the sprite.
 *****************************************************************************/
static void ToggleCursor( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_mouse_pointer_visible )
    {
        XDefineCursor( p_vout->p_sys->p_display,
                       p_vout->p_sys->window.base_window,
                       p_vout->p_sys->blank_cursor );
        p_vout->p_sys->b_mouse_pointer_visible = 0;
    }
    else
    {
        XUndefineCursor( p_vout->p_sys->p_display,
                         p_vout->p_sys->window.base_window );
        p_vout->p_sys->b_mouse_pointer_visible = 1;
    }
}

#if defined(MODULE_NAME_IS_xvmc)
/*****************************************************************************
 * XVideoGetPort: get YUV12 port
 *****************************************************************************/
static int XVideoGetPort( vout_thread_t *p_vout,
                          vlc_fourcc_t i_chroma, picture_heap_t *p_heap )
{
    XvAdaptorInfo *p_adaptor;
    unsigned int i;
    unsigned int i_adaptor, i_num_adaptors;
    int i_requested_adaptor;
    int i_selected_port;

    switch( XvQueryExtension( p_vout->p_sys->p_display, &i, &i, &i, &i, &i ) )
    {
        case Success:
            break;

        case XvBadExtension:
            msg_Warn( p_vout, "XvBadExtension" );
            return -1;

        case XvBadAlloc:
            msg_Warn( p_vout, "XvBadAlloc" );
            return -1;

        default:
            msg_Warn( p_vout, "XvQueryExtension failed" );
            return -1;
    }

    switch( XvQueryAdaptors( p_vout->p_sys->p_display,
                             DefaultRootWindow( p_vout->p_sys->p_display ),
                             &i_num_adaptors, &p_adaptor ) )
    {
        case Success:
            break;

        case XvBadExtension:
            msg_Warn( p_vout, "XvBadExtension for XvQueryAdaptors" );
            return -1;

        case XvBadAlloc:
            msg_Warn( p_vout, "XvBadAlloc for XvQueryAdaptors" );
            return -1;

        default:
            msg_Warn( p_vout, "XvQueryAdaptors failed" );
            return -1;
    }

    i_selected_port = -1;
    i_requested_adaptor = config_GetInt( p_vout, "xvmc-adaptor" );
    for( i_adaptor = 0; i_adaptor < i_num_adaptors; ++i_adaptor )
    {
        XvImageFormatValues *p_formats;
        int i_format, i_num_formats;
        int i_port;

        /* If we requested an adaptor and it's not this one, we aren't
         * interested */
        if( i_requested_adaptor != -1 && ((int)i_adaptor != i_requested_adaptor) )
        {
            continue;
        }

        /* If the adaptor doesn't have the required properties, skip it */
        if( !( p_adaptor[ i_adaptor ].type & XvInputMask ) ||
            !( p_adaptor[ i_adaptor ].type & XvImageMask ) )
        {
            continue;
        }

        /* Check that adaptor supports our requested format... */
        p_formats = XvListImageFormats( p_vout->p_sys->p_display,
                                        p_adaptor[i_adaptor].base_id,
                                        &i_num_formats );

        for( i_format = 0;
             i_format < i_num_formats && ( i_selected_port == -1 );
             i_format++ )
        {
            XvAttribute     *p_attr;
            int             i_attr, i_num_attributes;
            Atom            autopaint = None, colorkey = None;

            /* If this is not the format we want, or at least a
             * similar one, forget it */
            if( !vout_ChromaCmp( p_formats[ i_format ].id, i_chroma ) )
            {
                continue;
            }

            /* Look for the first available port supporting this format */
            for( i_port = p_adaptor[i_adaptor].base_id;
                 ( i_port < (int)(p_adaptor[i_adaptor].base_id
                                   + p_adaptor[i_adaptor].num_ports) )
                   && ( i_selected_port == -1 );
                 i_port++ )
            {
                if( XvGrabPort( p_vout->p_sys->p_display, i_port, CurrentTime )
                     == Success )
                {
                    i_selected_port = i_port;
                    p_heap->i_chroma = p_formats[ i_format ].id;
#if XvVersion > 2 || ( XvVersion == 2 && XvRevision >= 2 )
                    p_heap->i_rmask = p_formats[ i_format ].red_mask;
                    p_heap->i_gmask = p_formats[ i_format ].green_mask;
                    p_heap->i_bmask = p_formats[ i_format ].blue_mask;
#endif
                }
            }

            /* If no free port was found, forget it */
            if( i_selected_port == -1 )
            {
                continue;
            }

            /* If we found a port, print information about it */
            msg_Dbg( p_vout, "adaptor %i, port %i, format 0x%x (%4.4s) %s",
                     i_adaptor, i_selected_port, p_formats[ i_format ].id,
                     (char *)&p_formats[ i_format ].id,
                     ( p_formats[ i_format ].format == XvPacked ) ?
                         "packed" : "planar" );

            /* Use XV_AUTOPAINT_COLORKEY if supported, otherwise we will
             * manually paint the colour key */
            p_attr = XvQueryPortAttributes( p_vout->p_sys->p_display,
                                            i_selected_port,
                                            &i_num_attributes );

            for( i_attr = 0; i_attr < i_num_attributes; i_attr++ )
            {
                if( !strcmp( p_attr[i_attr].name, "XV_AUTOPAINT_COLORKEY" ) )
                {
                    autopaint = XInternAtom( p_vout->p_sys->p_display,
                                             "XV_AUTOPAINT_COLORKEY", False );
                    XvSetPortAttribute( p_vout->p_sys->p_display,
                                        i_selected_port, autopaint, 1 );
                }
                if( !strcmp( p_attr[i_attr].name, "XV_COLORKEY" ) )
                {
                    /* Find out the default colour key */
                    colorkey = XInternAtom( p_vout->p_sys->p_display,
                                            "XV_COLORKEY", False );
                    XvGetPortAttribute( p_vout->p_sys->p_display,
                                        i_selected_port, colorkey,
                                        &p_vout->p_sys->i_colourkey );
                }
            }
            p_vout->p_sys->b_paint_colourkey =
                autopaint == None && colorkey != None;

            if( p_attr != NULL )
            {
                XFree( p_attr );
            }
        }

        if( p_formats != NULL )
        {
            XFree( p_formats );
        }

    }

    if( i_num_adaptors > 0 )
    {
        XvFreeAdaptorInfo( p_adaptor );
    }

    if( i_selected_port == -1 )
    {
        int i_chroma_tmp = X112VLC_FOURCC( i_chroma );
        if( i_requested_adaptor == -1 )
        {
            msg_Warn( p_vout, "no free XVideo port found for format "
                      "0x%.8x (%4.4s)", i_chroma_tmp, (char*)&i_chroma_tmp );
        }
        else
        {
            msg_Warn( p_vout, "XVideo adaptor %i does not have a free "
                      "XVideo port for format 0x%.8x (%4.4s)",
                      i_requested_adaptor, i_chroma_tmp, (char*)&i_chroma_tmp );
        }
    }

    return i_selected_port;
}
#endif

#ifndef MODULE_NAME_IS_glx
/*****************************************************************************
 * InitDisplay: open and initialize X11 device
 *****************************************************************************
 * Create a window according to video output given size, and set other
 * properties according to the display properties.
 *****************************************************************************/
static int InitDisplay( vout_thread_t *p_vout )
{
#ifdef HAVE_SYS_SHM_H
    p_vout->p_sys->i_shm_opcode = 0;

    if( config_GetInt( p_vout, MODULE_STRING "-shm" ) > 0 )
    {
        int major, evt, err;

        if( XQueryExtension( p_vout->p_sys->p_display, "MIT-SHM", &major,
                             &evt, &err )
         && XShmQueryExtension( p_vout->p_sys->p_display ) )
            p_vout->p_sys->i_shm_opcode = major;

        if( p_vout->p_sys->i_shm_opcode )
        {
            int minor;
            Bool pixmaps;

            XShmQueryVersion( p_vout->p_sys->p_display, &major, &minor,
                              &pixmaps );
            msg_Dbg( p_vout, "XShm video extension v%d.%d "
                     "(with%s pixmaps, opcode: %d)",
                     major, minor, pixmaps ? "" : "out",
                     p_vout->p_sys->i_shm_opcode );
        }
        else
            msg_Warn( p_vout, "XShm video extension not available" );
    }
    else
        msg_Dbg( p_vout, "XShm video extension disabled" );
#endif

    return VLC_SUCCESS;
}

#ifdef HAVE_SYS_SHM_H
/*****************************************************************************
 * CreateShmImage: create an XImage or XvImage using shared memory extension
 *****************************************************************************
 * Prepare an XImage or XvImage for display function.
 * The order of the operations respects the recommandations of the mit-shm
 * document by J.Corbet and K.Packard. Most of the parameters were copied from
 * there. See http://ftp.xfree86.org/pub/XFree86/4.0/doc/mit-shm.TXT
 *****************************************************************************/
IMAGE_TYPE * CreateShmImage( vout_thread_t *p_vout,
                                    Display* p_display, EXTRA_ARGS_SHM,
                                    int i_width, int i_height )
{
    IMAGE_TYPE *p_image;
    Status result;

    /* Create XImage / XvImage */
#if defined(MODULE_NAME_IS_xvmc)
    p_image = XvShmCreateImage( p_display, i_xvport, i_chroma, 0,
                                i_width, i_height, p_shm );
#endif
    if( p_image == NULL )
    {
        msg_Err( p_vout, "image creation failed" );
        return NULL;
    }

    /* For too big image, the buffer returned is sometimes too small, prevent
     * VLC to segfault because of it
     * FIXME is it normal ? Is there a way to detect it
     * before (XvQueryBestSize did not) ? */
    if( p_image->width < i_width || p_image->height < i_height )
    {
        msg_Err( p_vout, "cannot allocate shared image data with the right size "
                         "(%dx%d instead of %dx%d)",
                         p_image->width, p_image->height,
                         i_width, i_height );
        IMAGE_FREE( p_image );
        return NULL;
    }

    /* Allocate shared memory segment. */
    p_shm->shmid = shmget( IPC_PRIVATE, DATA_SIZE(p_image), IPC_CREAT | 0600 );
    if( p_shm->shmid < 0 )
    {
        msg_Err( p_vout, "cannot allocate shared image data (%m)" );
        IMAGE_FREE( p_image );
        return NULL;
    }

    /* Attach shared memory segment to process (read/write) */
    p_shm->shmaddr = p_image->data = shmat( p_shm->shmid, 0, 0 );
    if(! p_shm->shmaddr )
    {
        msg_Err( p_vout, "cannot attach shared memory (%m)" );
        IMAGE_FREE( p_image );
        shmctl( p_shm->shmid, IPC_RMID, 0 );
        return NULL;
    }

    /* Read-only data. We won't be using XShmGetImage */
    p_shm->readOnly = True;

    /* Attach shared memory segment to X server */
    XSynchronize( p_display, True );
    i_shm_major = p_vout->p_sys->i_shm_opcode;
    result = XShmAttach( p_display, p_shm );
    if( result == False || !i_shm_major )
    {
        msg_Err( p_vout, "cannot attach shared memory to X server" );
        IMAGE_FREE( p_image );
        shmctl( p_shm->shmid, IPC_RMID, 0 );
        shmdt( p_shm->shmaddr );
        return NULL;
    }
    XSynchronize( p_display, False );

    /* Send image to X server. This instruction is required, since having
     * built a Shm XImage and not using it causes an error on XCloseDisplay,
     * and remember NOT to use XFlush ! */
    XSync( p_display, False );

#if 0
    /* Mark the shm segment to be removed when there are no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm->shmid, IPC_RMID, 0 );
#endif

    return p_image;
}
#endif

#endif
/*****************************************************************************
 * X11ErrorHandler: replace error handler so we can intercept some of them
 *****************************************************************************/
static int X11ErrorHandler( Display * display, XErrorEvent * event )
{
    char txt[1024];

    XGetErrorText( display, event->error_code, txt, sizeof( txt ) );
    fprintf( stderr,
             "[????????] x11 video output error: X11 request %u.%u failed "
              "with error code %u:\n %s\n",
             event->request_code, event->minor_code, event->error_code, txt );

    switch( event->request_code )
    {
    case X_SetInputFocus:
        /* Ignore errors on XSetInputFocus()
         * (they happen when a window is not yet mapped) */
        return 0;
    }

#if defined (HAVE_SYS_SHM_H) && !defined (MODULE_NAME_IS_glx)
    if( event->request_code == i_shm_major ) /* MIT-SHM */
    {
        fprintf( stderr,
                 "[????????] x11 video output notice:"
                 " buggy X11 server claims shared memory\n"
                 "[????????] x11 video output notice:"
                 " support though it does not work (OpenSSH?)\n" );
        return i_shm_major = 0;
    }
#endif

    XSetErrorHandler(NULL);
    return (XSetErrorHandler(X11ErrorHandler))( display, event );
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    unsigned int i_width, i_height;

    switch( i_query )
    {
        case VOUT_SET_SIZE:
            i_width  = va_arg( args, unsigned int );
            i_height = va_arg( args, unsigned int );
            if( !i_width ) i_width = p_vout->i_window_width;
            if( !i_height ) i_height = p_vout->i_window_height;

            return vout_window_SetSize( p_vout->p_sys->window.owner_window,
                                        i_width, i_height);

        case VOUT_SET_STAY_ON_TOP:
            return vout_window_SetOnTop( p_vout->p_sys->window.owner_window,
                                         va_arg( args, int ) );

       default:
            return VLC_EGENERIC;
    }
}
