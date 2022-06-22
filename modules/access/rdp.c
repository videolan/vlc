/*****************************************************************************
 * rdp.c: libfreeRDP based Remote Desktop access
 *****************************************************************************
 * Copyright (C) 2013 VideoLAN and VLC Authors
 *****************************************************************************
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_url.h>
#include <vlc_meta.h>

#define boolean bool

/* see MS-RDPBCGR http://msdn.microsoft.com/en-us/library/cc240445.aspx */

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/channels/channels.h>
#include <freerdp/gdi/gdi.h>

#if !defined(FREERDP_INTERFACE_VERSION)
# include <freerdp/version.h>
#endif

#if !defined(FREERDP_VERSION_MAJOR) || \
    (defined(FREERDP_VERSION_MAJOR) && !(FREERDP_VERSION_MAJOR > 1 || (FREERDP_VERSION_MAJOR == 1 && FREERDP_VERSION_MINOR >= 1)))
# define SoftwareGdi sw_gdi
# define Fullscreen fullscreen
# define ServerHostname hostname
# define Username username
# define Password password
# define ServerPort port
# define EncryptionMethods encryption
# define ContextSize context_size
#endif

#include <errno.h>
#ifdef HAVE_POLL_H
# include <poll.h>
#endif

#define USER_TEXT N_("Username")
#define USER_LONGTEXT N_("Username that will be used for the connection, " \
        "if no username is set in the URL.")
#define PASS_TEXT N_("Password")
#define PASS_LONGTEXT N_("Password that will be used for the connection, " \
        "if no username or password are set in URL.")

#define RDP_ENCRYPT N_("Encrypted connection")
#define RDP_FPS N_("Frame rate")
#define RDP_FPS_LONGTEXT N_("Acquisition rate (in fps)")

#define CFG_PREFIX "rdp-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_shortname( N_("RDP") )
    add_shortcut( "rdp" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_description( N_("RDP Remote Desktop") )
    set_capability( "access", 0 )

    add_string( CFG_PREFIX "user", NULL, USER_TEXT, USER_LONGTEXT )
        change_safe()
    add_password(CFG_PREFIX "password", NULL, PASS_TEXT, PASS_LONGTEXT)
        change_safe()
    add_float( CFG_PREFIX "fps", 5, RDP_FPS, RDP_FPS_LONGTEXT )

    add_bool( CFG_PREFIX "encrypt", false, RDP_ENCRYPT, NULL )
        change_safe()

    set_callbacks( Open, Close )
vlc_module_end()

#define RDP_MAX_FD 32

typedef struct
{
    vlc_thread_t thread;
    freerdp *p_instance;
    block_t *p_block;
    int i_framebuffersize;

    float f_fps;
    int i_frame_interval;
    vlc_tick_t i_starttime;

    es_out_id_t *es;

    /* pre-connect params */
    char *psz_hostname;
    int i_port;
} demux_sys_t;

/* context */

struct vlcrdp_context_t
{
    rdpContext rdp_context; /* Extending API's struct */
    demux_t *p_demux;
    rdpSettings* p_settings;
};
typedef struct vlcrdp_context_t vlcrdp_context_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* updates handlers */

static void desktopResizeHandler( rdpContext *p_context )
{
    vlcrdp_context_t * p_vlccontext = (vlcrdp_context_t *) p_context;
    demux_sys_t *p_sys = p_vlccontext->p_demux->p_sys;
    rdpGdi *p_gdi = p_context->gdi;

    if ( p_sys->es )
    {
        es_out_Del( p_vlccontext->p_demux->out, p_sys->es );
        p_sys->es = NULL;
    }

    /* Now init and fill es format */
    vlc_fourcc_t i_chroma;
    switch( p_gdi->bytesPerPixel )
    {
        default:
        case 16:
            i_chroma = VLC_CODEC_RGB16;
            break;
        case 24:
            i_chroma = VLC_CODEC_RGB24;
            break;
        case 32:
            i_chroma = VLC_CODEC_RGB32;
            break;
    }
    es_format_t fmt;
    es_format_Init( &fmt, VIDEO_ES, i_chroma );

    fmt.video.i_chroma = i_chroma;
    fmt.video.i_visible_width =
    fmt.video.i_width = p_gdi->width;
    fmt.video.i_visible_height =
    fmt.video.i_height = p_gdi->height;
    fmt.video.i_frame_rate_base = 1000;
    fmt.video.i_frame_rate = 1000 * p_sys->f_fps;
    p_sys->i_framebuffersize = p_gdi->width * p_gdi->height * p_gdi->bytesPerPixel;

    if ( p_sys->p_block )
        p_sys->p_block = block_Realloc( p_sys->p_block, 0, p_sys->i_framebuffersize );
    else
        p_sys->p_block = block_Alloc( p_sys->i_framebuffersize );

    p_sys->es = es_out_Add( p_vlccontext->p_demux->out, &fmt );
}

static void beginPaintHandler( rdpContext *p_context )
{
    vlcrdp_context_t * p_vlccontext = (vlcrdp_context_t *) p_context;
    demux_sys_t *p_sys = p_vlccontext->p_demux->p_sys;
    rdpGdi *p_gdi = p_context->gdi;
    p_gdi->primary->hdc->hwnd->invalid->null = 1;
    p_gdi->primary->hdc->hwnd->ninvalid = 0;
    if ( ! p_sys->p_block && p_sys->i_framebuffersize )
        p_sys->p_block = block_Alloc( p_sys->i_framebuffersize );
}

static void endPaintHandler( rdpContext *p_context )
{
    vlcrdp_context_t * p_vlccontext = (vlcrdp_context_t *) p_context;
    demux_sys_t *p_sys = p_vlccontext->p_demux->p_sys;
    rdpGdi *p_gdi = p_context->gdi;

    if ( p_sys->p_block )
    {
        p_sys->p_block->i_buffer = p_sys->i_framebuffersize;
        memcpy( p_sys->p_block->p_buffer, p_gdi->primary_buffer, p_sys->p_block->i_buffer );
    }
}

/* instance handlers */

static bool preConnectHandler( freerdp *p_instance )
{
    vlcrdp_context_t * p_vlccontext = (vlcrdp_context_t *) p_instance->context;
    demux_sys_t *p_sys = p_vlccontext->p_demux->p_sys;

    /* Configure connection */
    p_instance->settings->SoftwareGdi = true; /* render in buffer */
    p_instance->settings->Fullscreen = true;
    p_instance->settings->ServerHostname = strdup( p_sys->psz_hostname );
    p_instance->settings->Username =
            var_InheritString( p_vlccontext->p_demux, CFG_PREFIX "user" );
    p_instance->settings->Password =
            var_InheritString( p_vlccontext->p_demux, CFG_PREFIX "password" );
    p_instance->settings->ServerPort = p_sys->i_port;
    p_instance->settings->EncryptionMethods =
            var_InheritBool( p_vlccontext->p_demux, CFG_PREFIX "encrypt" );

    return true;
}

static bool postConnectHandler( freerdp *p_instance )
{
    vlcrdp_context_t * p_vlccontext = (vlcrdp_context_t *) p_instance->context;

    msg_Dbg( p_vlccontext->p_demux, "connected to desktop %dx%d (%d bpp)",
#if defined(FREERDP_VERSION_MAJOR) && (FREERDP_VERSION_MAJOR > 1 || (FREERDP_VERSION_MAJOR == 1 && FREERDP_VERSION_MINOR >= 1))
             p_instance->settings->DesktopWidth,
             p_instance->settings->DesktopHeight,
             p_instance->settings->ColorDepth
#else
             p_instance->settings->width,
             p_instance->settings->height,
             p_instance->settings->color_depth
#endif
             );

    p_instance->update->DesktopResize = desktopResizeHandler;
    p_instance->update->BeginPaint = beginPaintHandler;
    p_instance->update->EndPaint = endPaintHandler;

    gdi_init( p_instance,
                CLRBUF_16BPP |
#if defined(FREERDP_VERSION_MAJOR) && defined(FREERDP_VERSION_MINOR) && \
    !(FREERDP_VERSION_MAJOR > 1 || (FREERDP_VERSION_MAJOR == 1 && FREERDP_VERSION_MINOR >= 2))
                CLRBUF_24BPP |
#endif
                CLRBUF_32BPP, NULL );

    desktopResizeHandler( p_instance->context );
    return true;
}

static bool authenticateHandler( freerdp *p_instance, char** ppsz_username,
                                 char** ppsz_password, char** ppsz_domain )
{
    VLC_UNUSED(ppsz_domain);
    vlcrdp_context_t * p_vlccontext = (vlcrdp_context_t *) p_instance->context;
    *ppsz_username = var_InheritString( p_vlccontext->p_demux, CFG_PREFIX "user" );
    *ppsz_password = var_InheritString( p_vlccontext->p_demux, CFG_PREFIX "password" );
    return true;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool *pb;
    double *p_dbl;
    vlc_meta_t *p_meta;

    switch( i_query )
    {
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_CAN_CONTROL_RATE:
        case DEMUX_HAS_UNSUPPORTED_META:
            pb = va_arg( args, bool * );
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_CAN_RECORD:
            pb = va_arg( args, bool * );
            *pb = true;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS(var_InheritInteger( p_demux, "live-caching" ));
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = vlc_tick_now() - p_sys->i_starttime;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            *va_arg( args, vlc_tick_t * ) = 0;
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
            p_dbl = va_arg( args, double * );
            *p_dbl = p_sys->f_fps;
            return VLC_SUCCESS;

        case DEMUX_GET_META:
            p_meta = va_arg( args, vlc_meta_t * );
            vlc_meta_Set( p_meta, vlc_meta_Title, p_demux->psz_location );
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static void *DemuxThread( void *p_data )
{
    demux_t *p_demux = (demux_t *) p_data;
    demux_sys_t *p_sys = p_demux->p_sys;
    p_sys->i_starttime = vlc_tick_now();
    vlc_tick_t i_next_frame_date = vlc_tick_now() + p_sys->i_frame_interval;
    int i_ret;

    for(;;)
    {
        i_ret = 0;
        int cancel_state = vlc_savecancel();
        if ( freerdp_shall_disconnect( p_sys->p_instance ) )
        {
            vlc_restorecancel( cancel_state );
            msg_Warn( p_demux, "RDP server closed session" );
            es_out_Del( p_demux->out, p_sys->es );
            p_sys->es = NULL;
            return NULL;
        }

        struct
        {
            void* pp_rfds[RDP_MAX_FD]; /* Declared by rdp */
            void* pp_wfds[RDP_MAX_FD];
            int i_nbr;
            int i_nbw;
            struct pollfd ufds[RDP_MAX_FD];
        } fds;

        fds.i_nbr = fds.i_nbw = 0;

        if ( freerdp_get_fds( p_sys->p_instance, fds.pp_rfds, &fds.i_nbr,
                              fds.pp_wfds, &fds.i_nbw ) != true )
        {
            vlc_restorecancel( cancel_state );
            msg_Err( p_demux, "cannot get FDS" );
        }
        else
        if ( (fds.i_nbr + fds.i_nbw) > 0 && p_sys->es )
        {
            vlc_restorecancel( cancel_state );
            int i_count = 0;

            for( int i = 0; i < fds.i_nbr; i++ )
            {
                fds.ufds[ i_count ].fd = (long) fds.pp_rfds[ i ];
                fds.ufds[ i_count ].events = POLLIN ;
                fds.ufds[ i_count++ ].revents = 0;
            }
            for( int i = 0; i < fds.i_nbw && i_count < RDP_MAX_FD; i++ )
            {   /* may be useless */
                fds.ufds[ i_count ].fd = (long) fds.pp_wfds[ i ];
                fds.ufds[ i_count ].events = POLLOUT;
                fds.ufds[ i_count++ ].revents = 0;
            }
            i_ret = poll( fds.ufds, i_count, p_sys->i_frame_interval * 1000/2 );
        } else {
            vlc_restorecancel( cancel_state );
        }

        vlc_tick_wait( i_next_frame_date );
        i_next_frame_date += p_sys->i_frame_interval;

        if ( i_ret >= 0 )
        {
            /* Do the rendering */
            cancel_state = vlc_savecancel();
            freerdp_check_fds( p_sys->p_instance );
            vlc_restorecancel( cancel_state );
            block_t *p_block = block_Duplicate( p_sys->p_block );
            if (likely( p_block && p_sys->p_block ))
            {
                p_sys->p_block->i_dts = p_sys->p_block->i_pts = vlc_tick_now() - p_sys->i_starttime;
                es_out_SetPCR( p_demux->out, p_sys->p_block->i_pts );
                es_out_Send( p_demux->out, p_sys->es, p_sys->p_block );
                p_sys->p_block = p_block;
            }
        }
    }
    return NULL;
}

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t      *p_demux = (demux_t*)p_this;
    demux_sys_t  *p_sys;

    if (p_demux->out == NULL)
        return VLC_EGENERIC;

    p_sys = vlc_obj_calloc( p_this, 1, sizeof(demux_sys_t) );
    if( !p_sys ) return VLC_ENOMEM;

    p_sys->f_fps = var_InheritFloat( p_demux, CFG_PREFIX "fps" );
    if ( p_sys->f_fps <= 0 ) p_sys->f_fps = 1.0;
    p_sys->i_frame_interval = CLOCK_FREQ / p_sys->f_fps;

#if FREERDP_VERSION_MAJOR == 1 && FREERDP_VERSION_MINOR < 2
    freerdp_channels_global_init();
#endif

    p_sys->p_instance = freerdp_new();
    if ( !p_sys->p_instance )
    {
        msg_Err( p_demux, "rdp instantiation error" );
        return VLC_EGENERIC;
    }

    p_demux->p_sys = p_sys;
    p_sys->p_instance->PreConnect = preConnectHandler;
    p_sys->p_instance->PostConnect = postConnectHandler;
    p_sys->p_instance->Authenticate = authenticateHandler;

    /* Set up context handlers and let it be allocated */
    p_sys->p_instance->ContextSize = sizeof( vlcrdp_context_t );
    freerdp_context_new( p_sys->p_instance );

    vlcrdp_context_t * p_vlccontext = (vlcrdp_context_t *) p_sys->p_instance->context;
    p_vlccontext->p_demux = p_demux;

    /* Parse uri params for pre-connect */
    vlc_url_t url;
    vlc_UrlParse( &url, p_demux->psz_url );

    if ( !EMPTY_STR(url.psz_host) )
        p_sys->psz_hostname = strdup( url.psz_host );
    else
        p_sys->psz_hostname = strdup( "localhost" );

    p_sys->i_port = ( url.i_port > 0 ) ? url.i_port : 3389;

    vlc_UrlClean( &url );

    if ( ! freerdp_connect( p_sys->p_instance ) )
    {
        msg_Err( p_demux, "can't connect to rdp server" );
        goto error;
    }

    if ( vlc_clone( &p_sys->thread, DemuxThread, p_demux, VLC_THREAD_PRIORITY_INPUT ) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "can't spawn thread" );
        freerdp_disconnect( p_sys->p_instance );
        goto error;
    }

    p_demux->pf_demux = NULL;
    p_demux->pf_control = Control;

    return VLC_SUCCESS;

error:
    freerdp_free( p_sys->p_instance );
    free( p_sys->psz_hostname );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    if ( p_sys->es )
        es_out_Del( p_demux->out, p_sys->es );

    freerdp_disconnect( p_sys->p_instance );
    freerdp_free( p_sys->p_instance );
#if FREERDP_VERSION_MAJOR == 1 && FREERDP_VERSION_MINOR < 2
    freerdp_channels_global_uninit();
#endif

    if ( p_sys->p_block )
        block_Release( p_sys->p_block );

    free( p_sys->psz_hostname );
}
