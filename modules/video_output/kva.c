/*****************************************************************************
 * kva.c: KVA video output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2010, 2011, 2012 VLC authors and VideoLAN
 *
 * Authors: KO Myung-Hun <komh@chollian.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

#include <ctype.h>
#include <float.h>
#include <assert.h>

#include <fourcc.h>

#include <kva.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define KVA_FIXT23_TEXT N_( \
    "Enable a workaround for T23" )
#define KVA_FIXT23_LONGTEXT N_( \
    "Enable this option if the diagonal stripes are displayed " \
    "when the window size is equal to or smaller than the movie size." )
#define KVA_VIDEO_MODE_TEXT N_( \
    "Video mode" )
#define KVA_VIDEO_MODE_LONGTEXT N_( \
    "Select a proper video mode to be used by KVA." )

static const char *const ppsz_kva_video_mode[] = {
    "auto", "snap", "wo", "vman", "dive" };
static const char *const ppsz_kva_video_mode_text[] = {
    N_("Auto"), N_("SNAP"), N_("WarpOverlay!"), N_("VMAN"), N_("DIVE") };

vlc_module_begin ()
    set_shortname( "KVA" )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    add_string( "kva-video-mode", ppsz_kva_video_mode[0], KVA_VIDEO_MODE_TEXT,
                KVA_VIDEO_MODE_LONGTEXT, false )
        change_string_list( ppsz_kva_video_mode, ppsz_kva_video_mode_text )
    add_bool( "kva-fixt23", false, KVA_FIXT23_TEXT, KVA_FIXT23_LONGTEXT, true )
    set_description( N_("K Video Acceleration video output") )
    set_capability( "vout display", 100 )
    add_shortcut( "kva" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * vout_display_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the module specific properties of an output thread.
 *****************************************************************************/
struct vout_display_sys_t
{
    TID                tid;
    HEV                ack_event;
    int                i_result;
    HAB                hab;
    HMQ                hmq;
    HWND               frame;
    HWND               client;
    KVASETUP           kvas;
    KVACAPS            kvac;
    LONG               i_screen_width;
    LONG               i_screen_height;
    bool               b_fixt23;
    PFNWP              p_old_frame;
    RECTL              client_rect;
    vout_window_t     *parent_window;
    HWND               parent;
    picture_pool_t    *pool;
    unsigned           button_pressed;
    bool               is_mouse_hidden;
    bool               is_on_top;
};

struct picture_sys_t
{
    int i_chroma_shift;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_pool_t *Pool   (vout_display_t *, unsigned);
static void            Display(vout_display_t *, picture_t *, subpicture_t * );
static int             Control(vout_display_t *, int, va_list);
static void            Manage (vout_display_t *);

static int  OpenDisplay ( vout_display_t *, video_format_t * );
static void CloseDisplay( vout_display_t * );

static int  KVALock( picture_t * );
static void KVAUnlock( picture_t * );

static void             MorphToPM     ( void );
static int              ConvertKey    ( USHORT );
static MRESULT EXPENTRY MyFrameWndProc( HWND, ULONG, MPARAM, MPARAM );
static MRESULT EXPENTRY WndProc       ( HWND, ULONG, MPARAM, MPARAM );

#define WC_VLC_KVA "WC_VLC_KVA"

#define COLOR_KEY 0x0F0F0F

#define WM_VLC_MANAGE               ( WM_USER + 1 )
#define WM_VLC_FULLSCREEN_CHANGE    ( WM_USER + 2 )
#define WM_VLC_SIZE_CHANGE          ( WM_USER + 3 )

static const char *psz_video_mode[ 4 ] = {"DIVE", "WarpOverlay!", "SNAP",
                                          "VMAN"};

static void PMThread( void *arg )
{
    vout_display_t *vd = ( vout_display_t * )arg;
    vout_display_sys_t * sys = vd->sys;
    ULONG i_frame_flags;
    QMSG qm;
    char *psz_mode;
    ULONG i_kva_mode;

    /* */
    video_format_t fmt;
    video_format_ApplyRotation(&fmt, &vd->fmt);

    /* */
    vout_display_info_t info = vd->info;
    info.is_slow = false;
    info.has_double_click = true;
    info.needs_hide_mouse = true;
    info.has_pictures_invalid = false;

    MorphToPM();

    sys->hab = WinInitialize( 0 );
    sys->hmq = WinCreateMsgQueue( sys->hab, 0);

    WinRegisterClass( sys->hab,
                      WC_VLC_KVA,
                      WndProc,
                      CS_SIZEREDRAW | CS_MOVENOTIFY,
                      sizeof( PVOID ));

    sys->b_fixt23 = var_CreateGetBool( vd, "kva-fixt23");

    if( !sys->b_fixt23 )
        /* If an external window was specified, we'll draw in it. */
        sys->parent_window =
            vout_display_NewWindow( vd, VOUT_WINDOW_TYPE_HWND );

    if( sys->parent_window )
    {
        sys->parent = ( HWND )sys->parent_window->handle.hwnd;

        ULONG i_style = WinQueryWindowULong( sys->parent, QWL_STYLE );
        WinSetWindowULong( sys->parent, QWL_STYLE,
                           i_style | WS_CLIPCHILDREN );

        i_frame_flags = FCF_TITLEBAR;
    }
    else
    {
        sys->parent = HWND_DESKTOP;

        i_frame_flags = FCF_SYSMENU    | FCF_TITLEBAR | FCF_MINMAX |
                        FCF_SIZEBORDER | FCF_TASKLIST;
    }

    sys->frame =
        WinCreateStdWindow( sys->parent,      /* parent window handle */
                            WS_VISIBLE,       /* frame window style */
                            &i_frame_flags,   /* window style */
                            WC_VLC_KVA,       /* class name */
                            "",               /* window title */
                            0L,               /* default client style */
                            NULLHANDLE,       /* resource in exe file */
                            1,                /* frame window id */
                            &sys->client );   /* client window handle */

    if( sys->frame == NULLHANDLE )
    {
        msg_Err( vd, "cannot create a frame window");

        goto exit_frame;
    }

    WinSetWindowPtr( sys->client, 0, vd );

    if( !sys->parent_window )
    {
        WinSetWindowPtr( sys->frame, 0, vd );
        sys->p_old_frame = WinSubclassWindow( sys->frame, MyFrameWndProc );
    }

    psz_mode = var_CreateGetString( vd, "kva-video-mode" );

    i_kva_mode = KVAM_AUTO;
    if( strcmp( psz_mode, "snap" ) == 0 )
        i_kva_mode = KVAM_SNAP;
    else if( strcmp( psz_mode, "wo" ) == 0 )
        i_kva_mode = KVAM_WO;
    else if( strcmp( psz_mode, "vman" ) == 0 )
        i_kva_mode = KVAM_VMAN;
    else if( strcmp( psz_mode, "dive" ) == 0 )
        i_kva_mode = KVAM_DIVE;

    free( psz_mode );

    if( kvaInit( i_kva_mode, sys->client, COLOR_KEY ))
    {
        msg_Err( vd, "cannot initialize KVA");

        goto exit_kva_init;
    }

    kvaCaps( &sys->kvac );

    msg_Dbg( vd, "selected video mode = %s",
             psz_video_mode[ sys->kvac.ulMode - 1 ]);

    if( OpenDisplay( vd, &fmt ) )
    {
        msg_Err( vd, "cannot open display");

        goto exit_open_display;
    }

    if( vd->cfg->is_fullscreen && !sys->parent_window )
        WinPostMsg( sys->client, WM_VLC_FULLSCREEN_CHANGE,
                    MPFROMLONG( true ), 0 );

    kvaDisableScreenSaver();

    /* Setup vout_display now that everything is fine */
    vd->fmt     = fmt;
    vd->info    = info;

    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = Manage;

    /* Prevent SIG_FPE */
    _control87(MCW_EM, MCW_EM);

    sys->i_result = VLC_SUCCESS;
    DosPostEventSem( sys->ack_event );

    if( !sys->parent_window )
        WinSetVisibleRegionNotify( sys->frame, TRUE );

    while( WinGetMsg( sys->hab, &qm, NULLHANDLE, 0, 0 ))
        WinDispatchMsg( sys->hab, &qm );

    if( !sys->parent_window )
        WinSetVisibleRegionNotify( sys->frame, FALSE );

    kvaEnableScreenSaver();

    CloseDisplay( vd );

    /* fall through */

exit_open_display :
    kvaDone();

exit_kva_init :
    if( !sys->parent_window )
        WinSubclassWindow( sys->frame, sys->p_old_frame );

    WinDestroyWindow( sys->frame );

exit_frame :
    vout_display_DeleteWindow( vd, sys->parent_window );

    if( sys->is_mouse_hidden )
        WinShowPointer( HWND_DESKTOP, TRUE );

    WinDestroyMsgQueue( sys->hmq );
    WinTerminate( sys->hab );

    sys->i_result = VLC_EGENERIC;
    DosPostEventSem( sys->ack_event );
}

/**
 * This function initializes KVA vout method.
 */
static int Open ( vlc_object_t *object )
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    vd->sys = sys = calloc( 1, sizeof( *sys ));
    if( !sys )
        return VLC_ENOMEM;

    DosCreateEventSem( NULL, &sys->ack_event, 0, FALSE );

    sys->tid = _beginthread( PMThread, NULL, 1024 * 1024, vd );
    DosWaitEventSem( sys->ack_event, SEM_INDEFINITE_WAIT );

    if( sys->i_result != VLC_SUCCESS )
    {
        DosCloseEventSem( sys->ack_event );

        free( sys );

        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy KVA video thread output method
 *****************************************************************************
 * Terminate an output method created by Open
 *****************************************************************************/
static void Close ( vlc_object_t *object )
{
    vout_display_t * vd = (vout_display_t *)object;
    vout_display_sys_t * sys = vd->sys;

    WinPostQueueMsg( sys->hmq, WM_QUIT, 0, 0 );

    DosWaitThread( &sys->tid, DCWW_WAIT );

    if( sys->pool )
        picture_pool_Release( sys->pool );

    DosCloseEventSem( sys->ack_event );

    free( sys );
}

/**
 * Return a pool of direct buffers
 */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(count);

    return sys->pool;
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to the display.
 *****************************************************************************/
static void Display( vout_display_t *vd, picture_t *picture,
                     subpicture_t *subpicture )
{
    VLC_UNUSED( vd );
    VLC_UNUSED( subpicture );

    picture_Release( picture );
}

/*****************************************************************************
 * Manage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by video output thread. It returns
 * a non null value if an error occurred.
 *****************************************************************************/
static void Manage( vout_display_t *vd )
{
    vout_display_sys_t * sys = vd->sys;

    /* Let a window procedure manage instead because if resizing a frame window
     * here, WM_SIZE is not sent to its child window.
     * Maybe, is this due to the different threads ? */
    WinPostMsg( sys->client, WM_VLC_MANAGE, 0, 0 );
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_display_t *vd, int query, va_list args )
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
    case VOUT_DISPLAY_HIDE_MOUSE:
    {
        POINTL ptl;

        WinQueryPointerPos( HWND_DESKTOP, &ptl );
        if( !sys->is_mouse_hidden &&
            WinWindowFromPoint( HWND_DESKTOP, &ptl, TRUE ) == sys->client )
        {
            WinShowPointer( HWND_DESKTOP, FALSE );
            sys->is_mouse_hidden = true;
        }

        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_CHANGE_FULLSCREEN:
    {
        bool fs = va_arg(args, int);

        WinPostMsg( sys->client, WM_VLC_FULLSCREEN_CHANGE, MPFROMLONG(fs), 0 );
        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
    {
        const unsigned state = va_arg( args, unsigned );
        const bool is_on_top = (state & VOUT_WINDOW_STATE_ABOVE) != 0;

        if( is_on_top )
            WinSetWindowPos( sys->frame, HWND_TOP, 0, 0, 0, 0, SWP_ZORDER );

        sys->is_on_top = is_on_top;

        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_ZOOM:
    {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);

        WinPostMsg( sys->client, WM_VLC_SIZE_CHANGE,
                    MPFROMLONG( cfg->display.width ),
                    MPFROMLONG( cfg->display.height ));
        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    {
        if( query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT )
        {
            vout_display_place_t place;
            vout_display_PlacePicture(&place, &vd->source, vd->cfg, false);

            sys->kvas.ulAspectWidth  = place.width;
            sys->kvas.ulAspectHeight = place.height;
        }
        else
        {
            video_format_t src_rot;
            video_format_ApplyRotation(&src_rot, &vd->source);

            sys->kvas.rclSrcRect.xLeft   = src_rot.i_x_offset;
            sys->kvas.rclSrcRect.yTop    = src_rot.i_y_offset;
            sys->kvas.rclSrcRect.xRight  = src_rot.i_x_offset +
                                           src_rot.i_visible_width;
            sys->kvas.rclSrcRect.yBottom = src_rot.i_y_offset +
                                           src_rot.i_visible_height;
        }

        kvaSetup( &sys->kvas );

        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_RESET_PICTURES:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        /* TODO */
        break;
    }

    msg_Err(vd, "Unsupported query(=%d) in vout display KVA", query);
    return VLC_EGENERIC;
}

/* following functions are local */

/*****************************************************************************
 * OpenDisplay: open and initialize KVA device
 *****************************************************************************
 * Open and initialize display according to preferences specified in the vout
 * thread fields.
 *****************************************************************************/
static int OpenDisplay( vout_display_t *vd, video_format_t *fmt )
{
    vout_display_sys_t * sys = vd->sys;
    const vlc_fourcc_t *fallback;
    bool b_hw_accel = 0;
    FOURCC i_kva_fourcc;
    int i_chroma_shift;
    char sz_title[ 256 ];
    RECTL rcl;
    int w, h;

    msg_Dbg( vd, "render chroma = %4.4s", ( const char * )&fmt->i_chroma );

    for( int pass = 0; pass < 2 && !b_hw_accel; pass++ )
    {
        fallback = ( pass == 0 ) ? vlc_fourcc_GetYUVFallback( fmt->i_chroma ) :
                                   vlc_fourcc_GetRGBFallback( fmt->i_chroma );

        for( int i = 0; fallback[ i ]; i++ )
        {
            switch( fallback[ i ])
            {
                case VLC_CODEC_YV12:
                    b_hw_accel = sys->kvac.ulInputFormatFlags & KVAF_YV12;
                    i_kva_fourcc = FOURCC_YV12;
                    i_chroma_shift = 1;
                    break;

                case VLC_CODEC_YUYV:
                    b_hw_accel = sys->kvac.ulInputFormatFlags & KVAF_YUY2;
                    i_kva_fourcc = FOURCC_Y422;
                    i_chroma_shift = 0;
                    break;

                case VLC_CODEC_YV9:
                    b_hw_accel = sys->kvac.ulInputFormatFlags & KVAF_YVU9;
                    i_kva_fourcc = FOURCC_YVU9;
                    i_chroma_shift = 2;
                    break;

                case VLC_CODEC_RGB32:
                    b_hw_accel = sys->kvac.ulInputFormatFlags & KVAF_BGR32;
                    i_kva_fourcc = FOURCC_BGR4;
                    i_chroma_shift = 0;
                    break;

                case VLC_CODEC_RGB24:
                    b_hw_accel = sys->kvac.ulInputFormatFlags & KVAF_BGR24;
                    i_kva_fourcc = FOURCC_BGR3;
                    i_chroma_shift = 0;
                    break;

                case VLC_CODEC_RGB16:
                    b_hw_accel = sys->kvac.ulInputFormatFlags & KVAF_BGR16;
                    i_kva_fourcc = FOURCC_R565;
                    i_chroma_shift = 0;
                    break;

                case VLC_CODEC_RGB15:
                    b_hw_accel = sys->kvac.ulInputFormatFlags & KVAF_BGR15;
                    i_kva_fourcc = FOURCC_R555;
                    i_chroma_shift = 0;
                    break;
            }

            if( b_hw_accel )
            {
                fmt->i_chroma = fallback[ i ];
                break;
            }
        }
    }

    if( !b_hw_accel )
    {
        msg_Err( vd, "Ooops. There is no fourcc supported by KVA at all.");

        return VLC_EGENERIC;
    }

    /* Set the RGB masks */
    fmt->i_rmask = sys->kvac.ulRMask;
    fmt->i_gmask = sys->kvac.ulGMask;
    fmt->i_bmask = sys->kvac.ulBMask;

    msg_Dbg( vd, "output chroma = %4.4s", ( const char * )&fmt->i_chroma );
    msg_Dbg( vd, "KVA chroma = %4.4s", ( const char * )&i_kva_fourcc );

    w = fmt->i_width;
    h = fmt->i_height;

    sys->kvas.ulLength           = sizeof( KVASETUP );
    sys->kvas.szlSrcSize.cx      = w;
    sys->kvas.szlSrcSize.cy      = h;
    sys->kvas.rclSrcRect.xLeft   = fmt->i_x_offset;
    sys->kvas.rclSrcRect.yTop    = fmt->i_y_offset;
    sys->kvas.rclSrcRect.xRight  = fmt->i_x_offset + fmt->i_visible_width;
    sys->kvas.rclSrcRect.yBottom = fmt->i_y_offset + fmt->i_visible_height;
    sys->kvas.ulRatio            = KVAR_FORCEANY;
    sys->kvas.ulAspectWidth      = w;
    sys->kvas.ulAspectHeight     = h;
    sys->kvas.fccSrcColor        = i_kva_fourcc;
    sys->kvas.fDither            = TRUE;

    if( kvaSetup( &sys->kvas ))
    {
        msg_Err( vd, "cannot set up KVA");

        return VLC_EGENERIC;
    }

    /* Create the associated picture */
    picture_sys_t *picsys = malloc( sizeof( *picsys ) );
    if( picsys == NULL )
        return VLC_ENOMEM;
    picsys->i_chroma_shift = i_chroma_shift;

    picture_resource_t resource = { .p_sys = picsys };
    picture_t *picture = picture_NewFromResource( fmt, &resource );
    if( !picture )
    {
        free( picsys );
        return VLC_ENOMEM;
    }

    /* Wrap it into a picture pool */
    picture_pool_configuration_t pool_cfg;
    memset( &pool_cfg, 0, sizeof( pool_cfg ));
    pool_cfg.picture_count = 1;
    pool_cfg.picture       = &picture;
    pool_cfg.lock          = KVALock;
    pool_cfg.unlock        = KVAUnlock;

    sys->pool = picture_pool_NewExtended( &pool_cfg );
    if( !sys->pool )
    {
        picture_Release( picture );
        return VLC_ENOMEM;
    }

    if (vd->cfg->display.title)
        snprintf( sz_title, sizeof( sz_title ), "%s", vd->cfg->display.title );
    else
        snprintf( sz_title, sizeof( sz_title ),
                  "%s (%4.4s to %4.4s - %s mode KVA output)",
                  VOUT_TITLE,
                  ( char * )&vd->fmt.i_chroma,
                  ( char * )&sys->kvas.fccSrcColor,
                  psz_video_mode[ sys->kvac.ulMode - 1 ]);
    WinSetWindowText( sys->frame, sz_title );

    sys->i_screen_width  = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
    sys->i_screen_height = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );

    if( sys->parent_window )
        WinQueryWindowRect( sys->parent, &sys->client_rect );
    else
    {
        sys->client_rect.xLeft   = ( sys->i_screen_width  - w ) / 2;
        sys->client_rect.yBottom = ( sys->i_screen_height - h ) / 2 ;
        sys->client_rect.xRight  = sys->client_rect.xLeft   + w;
        sys->client_rect.yTop    = sys->client_rect.yBottom + h;
    }

    rcl = sys->client_rect;

    WinCalcFrameRect( sys->frame, &rcl, FALSE);

    WinSetWindowPos( sys->frame, HWND_TOP,
                     rcl.xLeft, rcl.yBottom,
                     rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                     SWP_MOVE | SWP_SIZE | SWP_ZORDER | SWP_SHOW |
                     SWP_ACTIVATE );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDisplay: close and reset KVA device
 *****************************************************************************
 * This function returns all resources allocated by OpenDisplay and restore
 * the original state of the device.
 *****************************************************************************/
static void CloseDisplay( vout_display_t *vd )
{
    VLC_UNUSED( vd );
}

static int KVALock( picture_t *picture )
{
    picture_sys_t *picsys = picture->p_sys;
    PVOID kva_buffer;
    ULONG kva_bpl;

    if( kvaLockBuffer( &kva_buffer, &kva_bpl ))
        return VLC_EGENERIC;

    /* Packed or Y plane */
    picture->p->p_pixels = ( uint8_t * )kva_buffer;
    picture->p->i_pitch  = kva_bpl;
    picture->p->i_lines  = picture->format.i_height;

    /* Other planes */
    for( int n = 1; n < picture->i_planes; n++ )
    {
        const plane_t *o = &picture->p[n-1];
        plane_t *p = &picture->p[n];

        p->p_pixels = o->p_pixels + o->i_lines * o->i_pitch;
        p->i_pitch  = kva_bpl >> picsys->i_chroma_shift;
        p->i_lines  = picture->format.i_height >> picsys->i_chroma_shift;
    }

    return VLC_SUCCESS;
}

static void KVAUnlock( picture_t *picture )
{
    VLC_UNUSED( picture );

    kvaUnlockBuffer();
}

static void MorphToPM( void )
{
    PPIB pib;

    DosGetInfoBlocks(NULL, &pib);

    /* Change flag from VIO to PM */
    if (pib->pib_ultype == 2)
        pib->pib_ultype = 3;
}

/*****************************************************************************
 * Key events handling
 *****************************************************************************/
static const struct
{
    USHORT i_pmkey;
    int    i_vlckey;
} pmkeys_to_vlckeys[] =
{
    { VK_LEFT, KEY_LEFT },
    { VK_RIGHT, KEY_RIGHT },
    { VK_UP, KEY_UP },
    { VK_DOWN, KEY_DOWN },
    { VK_SPACE, ' ' },
    { VK_NEWLINE, KEY_ENTER },
    { VK_ENTER, KEY_ENTER },
    { VK_F1, KEY_F1 },
    { VK_F2, KEY_F2 },
    { VK_F3, KEY_F3 },
    { VK_F4, KEY_F4 },
    { VK_F5, KEY_F5 },
    { VK_F6, KEY_F6 },
    { VK_F7, KEY_F7 },
    { VK_F8, KEY_F8 },
    { VK_F9, KEY_F9 },
    { VK_F10, KEY_F10 },
    { VK_F11, KEY_F11 },
    { VK_F12, KEY_F12 },
    { VK_HOME, KEY_HOME },
    { VK_END, KEY_END },
    { VK_INSERT, KEY_INSERT },
    { VK_DELETE, KEY_DELETE },
/*
    Not supported
    {, KEY_MENU },
*/
    { VK_ESC, KEY_ESC },
    { VK_PAGEUP, KEY_PAGEUP },
    { VK_PAGEDOWN, KEY_PAGEDOWN },
    { VK_TAB, KEY_TAB },
    { VK_BACKSPACE, KEY_BACKSPACE },
/*
    Not supported
    {, KEY_MOUSEWHEELUP },
    {, KEY_MOUSEWHEELDOWN },
    {, KEY_MOUSEWHEELLEFT },
    {, KEY_MOUSEWHEELRIGHT },

    {, KEY_BROWSER_BACK },
    {, KEY_BROWSER_FORWARD },
    {, KEY_BROWSER_REFRESH },
    {, KEY_BROWSER_STOP },
    {, KEY_BROWSER_SEARCH },
    {, KEY_BROWSER_FAVORITES },
    {, KEY_BROWSER_HOME },
    {, KEY_VOLUME_MUTE },
    {, KEY_VOLUME_DOWN },
    {, KEY_VOLUME_UP },
    {, KEY_MEDIA_NEXT_TRACK },
    {, KEY_MEDIA_PREV_TRACK },
    {, KEY_MEDIA_STOP },
    {, KEY_MEDIA_PLAY_PAUSE },
*/

    { 0, 0 }
};

static int ConvertKey( USHORT i_pmkey )
{
    int i;
    for( i = 0; pmkeys_to_vlckeys[ i ].i_pmkey != 0; i++ )
    {
        if( pmkeys_to_vlckeys[ i ].i_pmkey == i_pmkey )
            return pmkeys_to_vlckeys[ i ].i_vlckey;
    }
    return 0;
}

static MRESULT EXPENTRY MyFrameWndProc( HWND hwnd, ULONG msg, MPARAM mp1,
                                        MPARAM mp2 )
{
    vout_display_t *vd = WinQueryWindowPtr( hwnd, 0 );
    vout_display_sys_t *sys = vd->sys;

    switch( msg )
    {
        case WM_QUERYTRACKINFO :
        {
            MRESULT mr;

            mr = sys->p_old_frame( hwnd, msg, mp1, mp2 );
            if( !sys->b_fixt23 )
                return mr;

            PTRACKINFO pti = ( PTRACKINFO )mp2;
            RECTL      rcl;

            pti->rclBoundary.xLeft   = 0;
            pti->rclBoundary.yBottom = 0;
            pti->rclBoundary.xRight  = sys->i_screen_width;
            pti->rclBoundary.yTop    = sys->i_screen_height;

            WinCalcFrameRect( hwnd, &pti->rclBoundary, FALSE );

            pti->ptlMaxTrackSize.x = pti->rclBoundary.xRight -
                                     pti->rclBoundary.xLeft;
            pti->ptlMaxTrackSize.y = pti->rclBoundary.yTop -
                                     pti->rclBoundary.yBottom;

            rcl.xLeft   = 0;
            rcl.yBottom = 0;
            rcl.xRight  = sys->kvas.szlSrcSize.cx + 1;
            rcl.yTop    = sys->kvas.szlSrcSize.cy + 1;

            WinCalcFrameRect( hwnd, &rcl, FALSE );

            pti->ptlMinTrackSize.x = rcl.xRight - rcl.xLeft;
            pti->ptlMinTrackSize.y = rcl.yTop - rcl.yBottom;

            return MRFROMLONG( TRUE );
        }

        case WM_ADJUSTWINDOWPOS :
        {
            if( !sys->b_fixt23 )
                break;

            PSWP  pswp = ( PSWP )mp1;

            if( pswp->fl & SWP_SIZE )
            {
                RECTL rcl;

                rcl.xLeft   = pswp->x;
                rcl.yBottom = pswp->y;
                rcl.xRight  = rcl.xLeft + pswp->cx;
                rcl.yTop    = rcl.yBottom + pswp->cy;

                WinCalcFrameRect( hwnd, &rcl, TRUE );

                if( rcl.xRight - rcl.xLeft <= sys->kvas.szlSrcSize.cx )
                    rcl.xRight = rcl.xLeft + ( sys->kvas.szlSrcSize.cx + 1 );

                if( rcl.yTop - rcl.yBottom <= sys->kvas.szlSrcSize.cy )
                    rcl.yTop = rcl.yBottom + ( sys->kvas.szlSrcSize.cy + 1 );

                if( rcl.xRight - rcl.xLeft > sys->i_screen_width )
                {
                    rcl.xLeft  = 0;
                    rcl.xRight = sys->i_screen_width;
                }

                if( rcl.yTop - rcl.yBottom > sys->i_screen_height )
                {
                    rcl.yBottom = 0;
                    rcl.yTop    = sys->i_screen_height;
                }

                WinCalcFrameRect( hwnd, &rcl, FALSE );

                if( pswp->x != rcl.xLeft || pswp->y != rcl.yBottom )
                    pswp->fl |= SWP_MOVE;

                pswp->x = rcl.xLeft;
                pswp->y = rcl.yBottom;

                pswp->cx = rcl.xRight - rcl.xLeft;
                pswp->cy = rcl.yTop - rcl.yBottom;
            }

            break;
        }

        //case WM_VRNDISABLED :
        case WM_VRNENABLED :
            if( !vd->cfg->is_fullscreen && sys->is_on_top )
                WinSetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_ZORDER );
            break;
    }

    return sys->p_old_frame( hwnd, msg, mp1, mp2 );
}

static void MousePressed( vout_display_t *vd, HWND hwnd, unsigned button )
{
    if( WinQueryFocus( HWND_DESKTOP ) != hwnd )
        WinSetFocus( HWND_DESKTOP, hwnd );

    if( !vd->sys->button_pressed )
        WinSetCapture( HWND_DESKTOP, hwnd );

    vd->sys->button_pressed |= 1 << button;

    vout_display_SendEventMousePressed( vd, button );
}

static void MouseReleased( vout_display_t *vd, unsigned button )
{
    vd->sys->button_pressed &= ~(1 << button);
    if( !vd->sys->button_pressed )
        WinSetCapture( HWND_DESKTOP, NULLHANDLE );

    vout_display_SendEventMouseReleased( vd, button );
}

#define WM_MOUSELEAVE   0x41F

static MRESULT EXPENTRY WndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    vout_display_t * vd = WinQueryWindowPtr( hwnd, 0 );
    MRESULT result = ( MRESULT )TRUE;

    if ( !vd )
        return WinDefWindowProc( hwnd, msg, mp1, mp2 );

    vout_display_sys_t * sys = vd->sys;
    RECTL rcl;
    SWP   swp;

    if ( sys->is_mouse_hidden &&
         ((msg >= WM_MOUSEFIRST    && msg <= WM_MOUSELAST) ||
          (msg >= WM_EXTMOUSEFIRST && msg <= WM_EXTMOUSELAST) ||
           msg == WM_MOUSELEAVE))
    {
        WinShowPointer(HWND_DESKTOP, TRUE);
        sys->is_mouse_hidden = false;
    }

    switch( msg )
    {
        /* the user wants to close the window */
        case WM_CLOSE:
            vout_display_SendEventClose(vd);
            result = 0;
            break;

        case WM_MOUSEMOVE :
        {
            SHORT i_mouse_x = SHORT1FROMMP( mp1 );
            SHORT i_mouse_y = SHORT2FROMMP( mp1 );
            RECTL movie_rect;
            int   i_movie_width, i_movie_height;
            int   i_src_width, i_src_height;

            /* Get a current movie area */
            kvaAdjustDstRect( &sys->kvas.rclSrcRect, &movie_rect );
            i_movie_width = movie_rect.xRight - movie_rect.xLeft;
            i_movie_height = movie_rect.yTop - movie_rect.yBottom;

            i_src_width =  sys->kvas.rclSrcRect.xRight -
                           sys->kvas.rclSrcRect.xLeft;
            i_src_height = sys->kvas.rclSrcRect.yBottom -
                           sys->kvas.rclSrcRect.yTop;

            int x = ( i_mouse_x - movie_rect.xLeft ) *
                    i_src_width / i_movie_width +
                    sys->kvas.rclSrcRect.xLeft;
            int y = ( i_mouse_y - movie_rect.yBottom ) *
                    i_src_height / i_movie_height;

            /* Invert Y coordinate and add y offset */
            y = ( i_src_height - y ) + sys->kvas.rclSrcRect.yTop;;

            vout_display_SendEventMouseMoved(vd, x, y);

            result = WinDefWindowProc( hwnd, msg, mp1,mp2 );
            break;
        }

        case WM_BUTTON1DOWN :
            MousePressed( vd, hwnd, MOUSE_BUTTON_LEFT );
            break;

        case WM_BUTTON2DOWN :
            MousePressed( vd, hwnd, MOUSE_BUTTON_RIGHT );
            break;

        case WM_BUTTON3DOWN :
            MousePressed( vd, hwnd, MOUSE_BUTTON_CENTER );
            break;

        case WM_BUTTON1UP :
            MouseReleased( vd, MOUSE_BUTTON_LEFT );
            break;

        case WM_BUTTON2UP :
            MouseReleased( vd, MOUSE_BUTTON_RIGHT );
            break;

        case WM_BUTTON3UP :
            MouseReleased( vd, MOUSE_BUTTON_CENTER );
            break;

        case WM_BUTTON1DBLCLK :
            vout_display_SendEventMouseDoubleClick(vd);
            break;

        case WM_TRANSLATEACCEL :
            /* We have no accelerator table at all */
            result = ( MRESULT )FALSE;
            break;

        case WM_CHAR :
        {
            USHORT i_flags = SHORT1FROMMP( mp1 );
            USHORT i_ch    = SHORT1FROMMP( mp2 );
            USHORT i_vk    = SHORT2FROMMP( mp2 );
            int    i_key   = 0;

            /* If embedded window, let the parent process keys */
            if( sys->parent_window )
            {
                WinPostMsg( sys->parent, msg, mp1, mp2 );
                break;
            }

            if( !( i_flags & KC_KEYUP ))
            {
                if( i_flags & KC_VIRTUALKEY )
                    /* convert the key if possible */
                    i_key = ConvertKey( i_vk );
                else if(( i_flags & KC_CHAR ) && !HIBYTE( i_ch ))
                    i_key = tolower( i_ch );

                if( i_key )
                {
                    if( i_flags & KC_SHIFT )
                       i_key |= KEY_MODIFIER_SHIFT;

                    if( i_flags & KC_CTRL )
                        i_key |= KEY_MODIFIER_CTRL;

                    if( i_flags & KC_ALT )
                        i_key |= KEY_MODIFIER_ALT;

                    vout_display_SendEventKey(vd, i_key);
                }
            }
            break;
        }

        /* Process Manage() call */
        case WM_VLC_MANAGE :
            break;

        /* Fullscreen change */
        case WM_VLC_FULLSCREEN_CHANGE :
            if( LONGFROMMP( mp1 ))
            {
                WinQueryWindowPos( sys->frame, &swp );
                sys->client_rect.xLeft   = swp.x;
                sys->client_rect.yBottom = swp.y;
                sys->client_rect.xRight  = sys->client_rect.xLeft   + swp.cx;
                sys->client_rect.yTop    = sys->client_rect.yBottom + swp.cy;
                WinCalcFrameRect( sys->frame, &sys->client_rect, TRUE );

                rcl.xLeft   = 0;
                rcl.yBottom = 0;
                rcl.xRight  = sys->i_screen_width;
                rcl.yTop    = sys->i_screen_height;
            }
            else
                rcl = sys->client_rect;

            WinCalcFrameRect( sys->frame, &rcl, FALSE );

            WinSetWindowPos( sys->frame, HWND_TOP,
                             rcl.xLeft, rcl.yBottom,
                             rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                             SWP_MOVE | SWP_SIZE | SWP_ZORDER | SWP_SHOW |
                             SWP_ACTIVATE );
            break;

        /* Size change */
        case WM_VLC_SIZE_CHANGE :
            rcl.xLeft   = 0;
            rcl.yBottom = 0;
            rcl.xRight  = LONGFROMMP( mp1 );
            rcl.yTop    = LONGFROMMP( mp2 );
            WinCalcFrameRect( sys->frame, &rcl, FALSE );

            WinSetWindowPos( sys->frame, NULLHANDLE,
                             0, 0,
                             rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                             SWP_SIZE );

            WinQueryWindowPos( sys->frame, &swp );
            sys->client_rect.xLeft   = swp.x;
            sys->client_rect.yBottom = swp.y;
            sys->client_rect.xRight  = sys->client_rect.xLeft   + swp.cx;
            sys->client_rect.yTop    = sys->client_rect.yBottom + swp.cy;
            WinCalcFrameRect( sys->frame, &sys->client_rect, TRUE );
            break;

        default :
            return WinDefWindowProc( hwnd, msg, mp1, mp2 );
    }

    /* If embedded window, we need to change our window size according to a
     * parent window size */
    if( sys->parent_window )
    {
        RECTL rect;

        WinQueryWindowRect( sys->parent, &rcl );
        WinQueryWindowRect( sys->client, &rect);

        if( rcl.xLeft   != rect.xLeft   ||
            rcl.yBottom != rect.yBottom ||
            rcl.xRight  != rect.xRight  ||
            rcl.yTop    != rect.yTop)
        {
            WinCalcFrameRect( sys->frame, &rcl, FALSE );

            WinSetWindowPos( sys->frame, NULLHANDLE,
                             rcl.xLeft, rcl.yBottom,
                             rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                             SWP_SIZE | SWP_MOVE );
        }
    }

    return result;
}
