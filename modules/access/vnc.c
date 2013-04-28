/*****************************************************************************
 * vnc.c: libVNC access
 *****************************************************************************
 * Copyright (C) 2013 VideoLAN Authors
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * NOTA BENE: this module requires the linking against a library which is
 * known to require licensing under the GNU General Public License version 2
 * (or later). Therefore, the result of compiling this module will normally
 * be subject to the terms of that later license.
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
#include <vlc_fourcc.h>

#include <rfb/rfbclient.h>

#define RFB_USER N_("Username")
#define RFB_PASSWORD N_("Password")
#define RFB_CA_TEXT N_("X.509 Certificate Authority")
#define RFB_CA_LONGTEXT N_("Certificate of the Authority to verify server's against")
#define RFB_CRL_TEXT N_("X.509 Certificate Revocation List")
#define RFB_CRL_LONGTEXT N_("List of revoked servers certificates")
#define RFB_CERT_TEXT N_("X.509 Client certificate")
#define RFB_CERT_LONGTEXT N_("Certificate for client authentification")
#define RFB_KEY_TEXT N_("X.509 Client private key")
#define RFB_KEY_LONGTEXT N_("Private key for authentification by certificate")

#define RFB_CHROMA N_("Frame buffer depth")
#define RFB_CHROMA_LONGTEXT N_("RGB chroma (RV32, RV24, RV16, RGB2)")
#define RFB_FPS N_("Frame rate")
#define RFB_FPS_LONGTEXT N_("How many times the screen content should be refreshed per second.")
#define RFB_COMPRESS N_("Compression level")
#define RFB_COMPRESS_LONGTEXT N_("Transfer compression level from 0 (none) to 9 (max)")
#define RFB_QUALITY N_("Image quality")
#define RFB_QUALITY_LONGTEXT N_("Image quality 1 to 9 (max)")

#define CFG_PREFIX "rfb-"

const char *const rgb_chromas[] = { N_("32 bits"), N_("24 bits"), N_("16 bits"), N_("8 bits") };
const char *const rgb_chromas_v[] = { "RV32", "RV24", "RV16", "RGB8" };

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_shortname( N_("VNC") )
    add_shortcut( "vnc" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_description( N_("VNC client access") )
    set_capability( "access_demux", 0 )

    add_string( CFG_PREFIX "user", NULL, RFB_USER, RFB_USER, false )
        change_safe()
    add_password( CFG_PREFIX "password", NULL, RFB_PASSWORD, RFB_PASSWORD, false )
        change_safe()
    add_loadfile( CFG_PREFIX "x509-ca", NULL, RFB_CA_TEXT, RFB_CA_LONGTEXT, true )
        change_safe()
    add_loadfile( CFG_PREFIX "x509-crl", NULL, RFB_CRL_TEXT, RFB_CRL_LONGTEXT, true )
        change_safe()
    add_loadfile( CFG_PREFIX "x509-client-cert", NULL, RFB_CERT_TEXT, RFB_CERT_LONGTEXT, true )
        change_safe()
    add_loadfile( CFG_PREFIX "x509-client-key", NULL, RFB_KEY_TEXT, RFB_KEY_LONGTEXT, true )
        change_safe()

    add_float( CFG_PREFIX "fps", 5, RFB_FPS, RFB_FPS_LONGTEXT, true )
    add_string( CFG_PREFIX "chroma", rgb_chromas_v[0], RFB_CHROMA, RFB_CHROMA_LONGTEXT, false )
        change_string_list (rgb_chromas_v, rgb_chromas)
        change_safe()
    add_integer( CFG_PREFIX "compress-level", 0, RFB_COMPRESS, RFB_COMPRESS_LONGTEXT, true )
        change_integer_range (0, 9)
        change_safe()
    add_integer( CFG_PREFIX "quality-level", 9, RFB_QUALITY, RFB_QUALITY_LONGTEXT, true )
        change_integer_range (1, 9)
        change_safe()

    set_callbacks( Open, Close )
vlc_module_end()

struct demux_sys_t
{
    vlc_thread_t thread;
    int i_cancel_state;

    rfbClient* p_client;
    int i_framebuffersize;
    block_t *p_block;

    float f_fps;
    int i_frame_interval;
    mtime_t i_starttime;

    es_out_id_t *es;
};

static void *DemuxThread( void *p_data );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static rfbBool mallocFrameBufferHandler( rfbClient* p_client )
{
    vlc_fourcc_t i_chroma;
    demux_t *p_demux = (demux_t *) rfbClientGetClientData( p_client, DemuxThread );
    demux_sys_t *p_sys = p_demux->p_sys;

    if ( p_sys->es ) /* Source has changed resolution */
    {
        es_out_Del( p_demux->out, p_sys->es );
        p_sys->es = NULL;
    }

    int i_width = p_client->width;
    int i_height = p_client->height;
    int i_depth = p_client->format.bitsPerPixel;

    switch( i_depth )
    {
        case 8:
            i_chroma = VLC_CODEC_RGB8;
            break;
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

    if ( i_chroma != VLC_CODEC_RGB8 ) /* Palette based, no mask */
    {
        video_format_t videofmt;
        memset( &videofmt, 0, sizeof(video_format_t) );
        videofmt.i_chroma = i_chroma;
        video_format_FixRgb( &videofmt );

        p_client->format.redShift = videofmt.i_lrshift;
        p_client->format.greenShift = videofmt.i_lgshift;
        p_client->format.blueShift = videofmt.i_lbshift;
        p_client->format.redMax = videofmt.i_rmask >> videofmt.i_lrshift;
        p_client->format.greenMax = videofmt.i_gmask >> videofmt.i_lgshift;
        p_client->format.blueMax = videofmt.i_bmask >> videofmt.i_lbshift;
    }

    /* Set up framebuffer */
    p_sys->i_framebuffersize = i_width * i_height * i_depth / 8;

    /* Reuse unsent block */
    if ( p_sys->p_block )
        p_sys->p_block = block_Realloc( p_sys->p_block, 0, p_sys->i_framebuffersize );
    else
        p_sys->p_block = block_Alloc( p_sys->i_framebuffersize );

    if ( p_sys->p_block )
        p_sys->p_block->i_buffer = p_sys->i_framebuffersize;
    else
        return FALSE;

    /* Push our VNC config */
    SetFormatAndEncodings( p_client );

    /* Now init and fill es format */
    es_format_t fmt;
    es_format_Init( &fmt, VIDEO_ES, i_chroma );

    /* Fill input format */
    fmt.video.i_chroma = i_chroma;
    fmt.video.i_visible_width =
            fmt.video.i_width = i_width;

    fmt.video.i_visible_height =
            fmt.video.i_height = i_height;

    fmt.video.i_frame_rate_base = 1000;
    fmt.video.i_frame_rate = 1000 * p_sys->f_fps;

    fmt.video.i_bits_per_pixel = i_depth;
    fmt.video.i_rmask = p_client->format.redMax << p_client->format.redShift;
    fmt.video.i_gmask = p_client->format.greenMax << p_client->format.greenShift;
    fmt.video.i_bmask = p_client->format.blueMax << p_client->format.blueShift;

    fmt.video.i_sar_num = fmt.video.i_sar_den = 1;

    /* declare the new es */
    p_sys->es = es_out_Add( p_demux->out, &fmt );

    return TRUE;
}

/* Auth */
static char *getPasswordHandler( rfbClient *p_client )
{
    demux_t *p_demux = (demux_t *) rfbClientGetClientData( p_client, DemuxThread );
    /* freed by libvnc */
    return var_InheritString( p_demux, CFG_PREFIX "password" );
}

static rfbCredential* getCredentialHandler( rfbClient *p_client, int i_credentialType )
{
    demux_t *p_demux = (demux_t *) rfbClientGetClientData( p_client, DemuxThread );

    rfbCredential *credential = calloc( 1, sizeof(rfbCredential) );
    if ( !credential ) return NULL;

    switch( i_credentialType )
    {
        case rfbCredentialTypeX509:
            /* X509None, X509Vnc, X509Plain */
            credential->x509Credential.x509CACertFile =
                    var_InheritString( p_demux, CFG_PREFIX "x509-ca" );
            credential->x509Credential.x509CACrlFile =
                    var_InheritString( p_demux, CFG_PREFIX "x509-crl" );
            /* client auth by certificate */
            credential->x509Credential.x509ClientCertFile =
                    var_InheritString( p_demux, CFG_PREFIX "x509-client-cert" );
            credential->x509Credential.x509ClientKeyFile =
                    var_InheritString( p_demux, CFG_PREFIX "x509-client-key" );
            break;

        case rfbCredentialTypeUser:
            credential->userCredential.username =
                    var_InheritString( p_demux, CFG_PREFIX "user" );
            credential->userCredential.password =
                    var_InheritString( p_demux, CFG_PREFIX "password" );
            break;

        default:
            free( credential );
            return NULL; /* Unsupported Auth */
    }
    /* freed by libvnc */
    return credential;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    bool *pb;
    int64_t *pi64;
    double *p_dbl;
    vlc_meta_t *p_meta;

    switch( i_query )
    {
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_CAN_CONTROL_RATE:
        case DEMUX_HAS_UNSUPPORTED_META:
            pb = (bool*)va_arg( args, bool * );
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_CAN_RECORD:
            pb = (bool*)va_arg( args, bool * );
            *pb = true;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = INT64_C(1000)
                  * var_InheritInteger( p_demux, "network-caching" );
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = mdate() - p_demux->p_sys->i_starttime;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = 0;
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
            p_dbl = (double*)va_arg( args, double * );
            *p_dbl = p_demux->p_sys->f_fps;
            return VLC_SUCCESS;

        case DEMUX_GET_META:
            p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t* );
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
    demux_sys_t  *p_sys = p_demux->p_sys;
    mtime_t i_next_frame_date = mdate() + p_sys->i_frame_interval;
    int i_status;

    for(;;)
    {
        p_sys->i_cancel_state = vlc_savecancel();
        i_status = WaitForMessage( p_sys->p_client, p_sys->i_frame_interval );
        vlc_restorecancel( p_sys->i_cancel_state );

        /* Ensure we're not building frames too fast */
        /* as WaitForMessage takes only a maximum wait */
        mwait( i_next_frame_date );
        i_next_frame_date += p_sys->i_frame_interval;

        if ( i_status > 0 )
        {
            p_sys->p_client->frameBuffer = p_sys->p_block->p_buffer;
            p_sys->i_cancel_state = vlc_savecancel();
            i_status = HandleRFBServerMessage( p_sys->p_client );
            vlc_restorecancel( p_sys->i_cancel_state );
            if ( ! i_status )
            {
                msg_Warn( p_demux, "Cannot get announced data. Server closed ?" );
                es_out_Del( p_demux->out, p_sys->es );
                p_sys->es = NULL;
                return NULL;
            }
            else
            {
                block_t *p_block = block_Duplicate( p_sys->p_block );
                if ( p_block ) /* drop frame/content if no next block */
                {
                    p_sys->p_block->i_dts = p_sys->p_block->i_pts = mdate();
                    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->p_block->i_pts );
                    es_out_Send( p_demux->out, p_sys->es, p_sys->p_block );
                    p_sys->p_block = p_block;
                }
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

    p_sys = calloc( 1, sizeof(demux_sys_t) );
    if( !p_sys ) return VLC_ENOMEM;

    p_sys->f_fps = var_InheritFloat( p_demux, CFG_PREFIX "fps" );
    if ( p_sys->f_fps <= 0 ) p_sys->f_fps = 1.0;
    p_sys->i_frame_interval = 1000000 / p_sys->f_fps ;

    char *psz_chroma = var_InheritString( p_demux, CFG_PREFIX "chroma" );
    vlc_fourcc_t i_chroma = vlc_fourcc_GetCodecFromString( VIDEO_ES, psz_chroma );
    free( psz_chroma );
    if ( !i_chroma || vlc_fourcc_IsYUV( i_chroma ) )
    {
        msg_Err( p_demux, "Only RGB chroma are supported" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    const vlc_chroma_description_t *p_chroma_desc = vlc_fourcc_GetChromaDescription( i_chroma );
    if ( !p_chroma_desc )
    {
        msg_Err( p_demux, "Unable to get RGB chroma description" );
        free( p_sys );
        return VLC_EGENERIC;
    }

#ifdef NDEBUG
    rfbEnableClientLogging = FALSE;
#endif

    p_sys->p_client = rfbGetClient( p_chroma_desc->pixel_bits / 3, // bitsPerSample
                                    3, // samplesPerPixel
                                    p_chroma_desc->pixel_size ); // bytesPerPixel
    if ( ! p_sys->p_client )
    {
        msg_Dbg( p_demux, "Unable to set up client for %s",
                 vlc_fourcc_GetDescription( VIDEO_ES, i_chroma ) );
        free( p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "set up client for %s %d %d %d",
             vlc_fourcc_GetDescription( VIDEO_ES, i_chroma ),
             p_chroma_desc->pixel_bits / 3, 3, p_chroma_desc->pixel_size );

    p_sys->p_client->MallocFrameBuffer = mallocFrameBufferHandler;
    p_sys->p_client->canHandleNewFBSize = TRUE;
    p_sys->p_client->GetCredential = getCredentialHandler;
    p_sys->p_client->GetPassword = getPasswordHandler; /* VNC simple auth */

    /* Set compression and quality levels */
    p_sys->p_client->appData.compressLevel =
            var_InheritInteger( p_demux, CFG_PREFIX "compress-level" );
    p_sys->p_client->appData.qualityLevel =
            var_InheritInteger( p_demux, CFG_PREFIX "quality-level" );

    /* Parse uri params */
    vlc_url_t url;
    vlc_UrlParse( &url, p_demux->psz_location, 0 );

    if ( !EMPTY_STR(url.psz_host) )
        p_sys->p_client->serverHost = strdup( url.psz_host );
    else
        p_sys->p_client->serverHost = strdup( "localhost" );

    p_sys->p_client->appData.viewOnly = TRUE;
    p_sys->p_client->serverPort = ( url.i_port > 0 ) ? url.i_port : 5900;

    msg_Dbg( p_demux, "VNC init %s host=%s pass=%s port=%d",
             p_demux->psz_location,
             p_sys->p_client->serverHost,
             url.psz_password,
             p_sys->p_client->serverPort );

    vlc_UrlClean( &url );

    /* make demux available for callback handlers */
    rfbClientSetClientData( p_sys->p_client, DemuxThread, p_demux );
    p_demux->p_sys = p_sys;

    if( !rfbInitClient( p_sys->p_client, NULL, NULL ) )
    {
        msg_Err( p_demux, "can't connect to RFB server" );
        goto error;
    }

    p_sys->i_starttime = mdate();

    if ( vlc_clone( &p_sys->thread, DemuxThread, p_demux, VLC_THREAD_PRIORITY_INPUT ) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "can't spawn thread" );
        goto error;
    }

    p_demux->pf_demux = NULL;
    p_demux->pf_control = Control;

    return VLC_SUCCESS;

error:
    free( p_sys );
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

    rfbClientCleanup( p_sys->p_client );

    if ( p_sys->p_block )
        block_Release( p_sys->p_block );

    free( p_sys );
}
