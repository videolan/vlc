/*****************************************************************************
 * transcode.c: transcoding stream output module
 *****************************************************************************
 * Copyright (C) 2003-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Antoine Cellerier <dionoea at videolan dot org>
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
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_sout.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_codec.h>
#include <vlc_meta.h>
#include <vlc_block.h>
#include <vlc_filter.h>
#include <vlc_osd.h>

#include <math.h>

#define MASTER_SYNC_MAX_DRIFT 100000

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define VENC_TEXT N_("Video encoder")
#define VENC_LONGTEXT N_( \
    "This is the video encoder module that will be used (and its associated "\
    "options).")
#define VCODEC_TEXT N_("Destination video codec")
#define VCODEC_LONGTEXT N_( \
    "This is the video codec that will be used.")
#define VB_TEXT N_("Video bitrate")
#define VB_LONGTEXT N_( \
    "Target bitrate of the transcoded video stream." )
#define SCALE_TEXT N_("Video scaling")
#define SCALE_LONGTEXT N_( \
    "Scale factor to apply to the video while transcoding (eg: 0.25)")
#define FPS_TEXT N_("Video frame-rate")
#define FPS_LONGTEXT N_( \
    "Target output frame rate for the video stream." )
#define DEINTERLACE_TEXT N_("Deinterlace video")
#define DEINTERLACE_LONGTEXT N_( \
    "Deinterlace the video before encoding." )
#define DEINTERLACE_MODULE_TEXT N_("Deinterlace module")
#define DEINTERLACE_MODULE_LONGTEXT N_( \
    "Specify the deinterlace module to use." )
#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_( \
    "Output video width." )
#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_( \
    "Output video height." )
#define MAXWIDTH_TEXT N_("Maximum video width")
#define MAXWIDTH_LONGTEXT N_( \
    "Maximum output video width." )
#define MAXHEIGHT_TEXT N_("Maximum video height")
#define MAXHEIGHT_LONGTEXT N_( \
    "Maximum output video height." )
#define VFILTER_TEXT N_("Video filter")
#define VFILTER_LONGTEXT N_( \
    "Video filters will be applied to the video streams (after overlays " \
    "are applied). You must enter a comma-separated list of filters." )

#define AENC_TEXT N_("Audio encoder")
#define AENC_LONGTEXT N_( \
    "This is the audio encoder module that will be used (and its associated "\
    "options).")
#define ACODEC_TEXT N_("Destination audio codec")
#define ACODEC_LONGTEXT N_( \
    "This is the audio codec that will be used.")
#define AB_TEXT N_("Audio bitrate")
#define AB_LONGTEXT N_( \
    "Target bitrate of the transcoded audio stream." )
#define ARATE_TEXT N_("Audio sample rate")
#define ARATE_LONGTEXT N_( \
 "Sample rate of the transcoded audio stream (11250, 22500, 44100 or 48000).")
#define ACHANS_TEXT N_("Audio channels")
#define ACHANS_LONGTEXT N_( \
    "Number of audio channels in the transcoded streams." )
#define AFILTER_TEXT N_("Audio filter")
#define AFILTER_LONGTEXT N_( \
    "Audio filters will be applied to the audio streams (after conversion " \
    "filters are applied). You must enter a comma-separated list of filters." )

#define SENC_TEXT N_("Subtitles encoder")
#define SENC_LONGTEXT N_( \
    "This is the subtitles encoder module that will be used (and its " \
    "associated options)." )
#define SCODEC_TEXT N_("Destination subtitles codec")
#define SCODEC_LONGTEXT N_( \
    "This is the subtitles codec that will be used." )

#define SFILTER_TEXT N_("Overlays")
#define SFILTER_LONGTEXT N_( \
    "This allows you to add overlays (also known as \"subpictures\" on the "\
    "transcoded video stream. The subpictures produced by the filters will "\
    "be overlayed directly onto the video. You must specify a comma-separated "\
    "list of subpicture modules" )

#define OSD_TEXT N_("OSD menu")
#define OSD_LONGTEXT N_(\
    "Stream the On Screen Display menu (using the osdmenu subpicture module)." )

#define THREADS_TEXT N_("Number of threads")
#define THREADS_LONGTEXT N_( \
    "Number of threads used for the transcoding." )
#define HP_TEXT N_("High priority")
#define HP_LONGTEXT N_( \
    "Runs the optional encoder thread at the OUTPUT priority instead of " \
    "VIDEO." )

#define ASYNC_TEXT N_("Synchronise on audio track")
#define ASYNC_LONGTEXT N_( \
    "This option will drop/duplicate video frames to synchronise the video " \
    "track on the audio track." )

#define HURRYUP_TEXT N_( "Hurry up" )
#define HURRYUP_LONGTEXT N_( "The transcoder will drop frames if your CPU " \
                "can't keep up with the encoding rate." )

static const char *const ppsz_deinterlace_type[] =
{
    "deinterlace", "ffmpeg-deinterlace"
};

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-transcode-"

vlc_module_begin ()
    set_shortname( N_("Transcode"))
    set_description( N_("Transcode stream output") )
    set_capability( "sout stream", 50 )
    add_shortcut( "transcode" )
    set_callbacks( Open, Close )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )
    set_section( N_("Video"), NULL )
    add_module( SOUT_CFG_PREFIX "venc", "encoder", NULL, NULL, VENC_TEXT,
                VENC_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "vcodec", NULL, NULL, VCODEC_TEXT,
                VCODEC_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX "vb", 0, NULL, VB_TEXT,
                 VB_LONGTEXT, false )
    add_float( SOUT_CFG_PREFIX "scale", 1, NULL, SCALE_TEXT,
               SCALE_LONGTEXT, false )
    add_float( SOUT_CFG_PREFIX "fps", 0, NULL, FPS_TEXT,
               FPS_LONGTEXT, false )
    add_bool( SOUT_CFG_PREFIX "hurry-up", true, NULL, HURRYUP_TEXT,
               HURRYUP_LONGTEXT, false )
    add_bool( SOUT_CFG_PREFIX "deinterlace", false, NULL, DEINTERLACE_TEXT,
              DEINTERLACE_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "deinterlace-module", "deinterlace", NULL,
                DEINTERLACE_MODULE_TEXT, DEINTERLACE_MODULE_LONGTEXT,
                false )
        change_string_list( ppsz_deinterlace_type, 0, 0 )
    add_integer( SOUT_CFG_PREFIX "width", 0, NULL, WIDTH_TEXT,
                 WIDTH_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "height", 0, NULL, HEIGHT_TEXT,
                 HEIGHT_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "maxwidth", 0, NULL, MAXWIDTH_TEXT,
                 MAXWIDTH_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "maxheight", 0, NULL, MAXHEIGHT_TEXT,
                 MAXHEIGHT_LONGTEXT, true )
    add_module_list( SOUT_CFG_PREFIX "vfilter", "video filter2",
                     NULL, NULL,
                     VFILTER_TEXT, VFILTER_LONGTEXT, false )

    set_section( N_("Audio"), NULL )
    add_module( SOUT_CFG_PREFIX "aenc", "encoder", NULL, NULL, AENC_TEXT,
                AENC_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "acodec", NULL, NULL, ACODEC_TEXT,
                ACODEC_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX "ab", 0, NULL, AB_TEXT,
                 AB_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX "channels", 0, NULL, ACHANS_TEXT,
                 ACHANS_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX "samplerate", 0, NULL, ARATE_TEXT,
                 ARATE_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "audio-sync", false, NULL, ASYNC_TEXT,
              ASYNC_LONGTEXT, false )
    add_module_list( SOUT_CFG_PREFIX "afilter",  "audio filter2",
                     NULL, NULL,
                     AFILTER_TEXT, AFILTER_LONGTEXT, false )

    set_section( N_("Overlays/Subtitles"), NULL )
    add_module( SOUT_CFG_PREFIX "senc", "encoder", NULL, NULL, SENC_TEXT,
                SENC_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "scodec", NULL, NULL, SCODEC_TEXT,
                SCODEC_LONGTEXT, false )
    add_bool( SOUT_CFG_PREFIX "soverlay", false, NULL, SCODEC_TEXT,
               SCODEC_LONGTEXT, false )
    add_module_list( SOUT_CFG_PREFIX "sfilter", "video filter",
                     NULL, NULL,
                     SFILTER_TEXT, SFILTER_LONGTEXT, false )

    set_section( N_("On Screen Display"), NULL )
    add_bool( SOUT_CFG_PREFIX "osd", false, NULL, OSD_TEXT,
              OSD_LONGTEXT, false )

    set_section( N_("Miscellaneous"), NULL )
    add_integer( SOUT_CFG_PREFIX "threads", 0, NULL, THREADS_TEXT,
                 THREADS_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "high-priority", false, NULL, HP_TEXT, HP_LONGTEXT,
              true )

vlc_module_end ()

static const char *const ppsz_sout_options[] = {
    "venc", "vcodec", "vb",
    "scale", "fps", "width", "height", "vfilter", "deinterlace",
    "deinterlace-module", "threads", "hurry-up", "aenc", "acodec", "ab",
    "afilter", "samplerate", "channels", "senc", "scodec", "soverlay",
    "sfilter", "osd", "audio-sync", "high-priority", "maxwidth", "maxheight",
    NULL
};

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, block_t* );

static int  transcode_audio_new    ( sout_stream_t *, sout_stream_id_t * );
static void transcode_audio_close  ( sout_stream_id_t * );
static int  transcode_audio_process( sout_stream_t *, sout_stream_id_t *,
                                     block_t *, block_t ** );

static aout_buffer_t *audio_new_buffer( decoder_t *, int );
static void audio_del_buffer( decoder_t *, aout_buffer_t * );

static int  transcode_video_new    ( sout_stream_t *, sout_stream_id_t * );
static void transcode_video_close  ( sout_stream_t *, sout_stream_id_t * );
static void transcode_video_encoder_init( sout_stream_t *, sout_stream_id_t *);
static int  transcode_video_encoder_open( sout_stream_t *, sout_stream_id_t *);
static int  transcode_video_process( sout_stream_t *, sout_stream_id_t *,
                                     block_t *, block_t ** );

static picture_t *video_new_buffer_decoder( decoder_t * );
static void video_del_buffer_decoder( decoder_t *, picture_t * );
static void video_link_picture_decoder( decoder_t *, picture_t * );
static void video_unlink_picture_decoder( decoder_t *, picture_t * );

static int  transcode_spu_new    ( sout_stream_t *, sout_stream_id_t * );
static void transcode_spu_close  ( sout_stream_id_t * );
static int  transcode_spu_process( sout_stream_t *, sout_stream_id_t *,
                                   block_t *, block_t ** );

static int  transcode_osd_new    ( sout_stream_t *, sout_stream_id_t * );
static void transcode_osd_close  ( sout_stream_t *, sout_stream_id_t * );
static int  transcode_osd_process( sout_stream_t *, sout_stream_id_t *,
                                   block_t *, block_t ** );

static void* EncoderThread( vlc_object_t * p_this );

static const int pi_channels_maps[6] =
{
    0,
    AOUT_CHAN_CENTER,   AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
};

#define PICTURE_RING_SIZE 64
#define SUBPICTURE_RING_SIZE 20

#define ENC_FRAMERATE (25 * 1000 + .5)
#define ENC_FRAMERATE_BASE 1000

struct sout_stream_sys_t
{
    VLC_COMMON_MEMBERS

    sout_stream_t   *p_out;
    sout_stream_id_t *id_video;
    block_t         *p_buffers;
    vlc_mutex_t     lock_out;
    vlc_cond_t      cond;
    picture_t *     pp_pics[PICTURE_RING_SIZE];
    int             i_first_pic, i_last_pic;

    /* Audio */
    vlc_fourcc_t    i_acodec;   /* codec audio (0 if not transcode) */
    char            *psz_aenc;
    config_chain_t  *p_audio_cfg;
    uint32_t        i_sample_rate;
    uint32_t        i_channels;
    int             i_abitrate;

    char            *psz_af2;

    /* Video */
    vlc_fourcc_t    i_vcodec;   /* codec video (0 if not transcode) */
    char            *psz_venc;
    config_chain_t  *p_video_cfg;
    int             i_vbitrate;
    double          f_scale;
    double          f_fps;
    unsigned int    i_width, i_maxwidth;
    unsigned int    i_height, i_maxheight;
    bool            b_deinterlace;
    char            *psz_deinterlace;
    config_chain_t  *p_deinterlace_cfg;
    int             i_threads;
    bool            b_high_priority;
    bool            b_hurry_up;

    char            *psz_vf2;

    /* SPU */
    vlc_fourcc_t    i_scodec;   /* codec spu (0 if not transcode) */
    char            *psz_senc;
    bool            b_soverlay;
    config_chain_t  *p_spu_cfg;
    spu_t           *p_spu;

    /* OSD Menu */
    vlc_fourcc_t    i_osdcodec; /* codec osd menu (0 if not transcode) */
    char            *psz_osdenc;
    config_chain_t  *p_osd_cfg;
    bool            b_osd;   /* true when osd es is registered */

    /* Sync */
    bool            b_master_sync;
    mtime_t         i_master_drift;
};

struct decoder_owner_sys_t
{
    sout_stream_sys_t *p_sys;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    vlc_value_t       val;

    p_sys = vlc_object_create( p_this, sizeof( sout_stream_sys_t ) );

    p_sys->p_out = sout_StreamNew( p_stream->p_sout, p_stream->psz_next );
    if( !p_sys->p_out )
    {
        msg_Err( p_stream, "cannot create chain" );
        vlc_object_release( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->i_master_drift = 0;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                   p_stream->p_cfg );

    /* Audio transcoding parameters */
    var_Get( p_stream, SOUT_CFG_PREFIX "aenc", &val );
    p_sys->psz_aenc = NULL;
    p_sys->p_audio_cfg = NULL;
    if( val.psz_string && *val.psz_string )
    {
        char *psz_next;
        psz_next = config_ChainCreate( &p_sys->psz_aenc, &p_sys->p_audio_cfg,
                                       val.psz_string );
        free( psz_next );
    }
    free( val.psz_string );

    var_Get( p_stream, SOUT_CFG_PREFIX "acodec", &val );
    p_sys->i_acodec = 0;
    if( val.psz_string && *val.psz_string )
    {
        char fcc[4] = "    ";
        memcpy( fcc, val.psz_string, __MIN( strlen( val.psz_string ), 4 ) );
        p_sys->i_acodec = VLC_FOURCC( fcc[0], fcc[1], fcc[2], fcc[3] );
    }
    free( val.psz_string );

    var_Get( p_stream, SOUT_CFG_PREFIX "ab", &val );
    p_sys->i_abitrate = val.i_int;
    if( p_sys->i_abitrate < 4000 ) p_sys->i_abitrate *= 1000;

    var_Get( p_stream, SOUT_CFG_PREFIX "samplerate", &val );
    p_sys->i_sample_rate = val.i_int;

    var_Get( p_stream, SOUT_CFG_PREFIX "channels", &val );
    p_sys->i_channels = val.i_int;

    if( p_sys->i_acodec )
    {
        if( ( p_sys->i_acodec == VLC_CODEC_MP3 ||
              p_sys->i_acodec == VLC_CODEC_MPGA ) && p_sys->i_channels > 2 )
        {
            msg_Warn( p_stream, "%d channels invalid for mp3, forcing to 2",
                      p_sys->i_channels );
            p_sys->i_channels = 2;
        }
        msg_Dbg( p_stream, "codec audio=%4.4s %dHz %d channels %dKb/s",
                 (char *)&p_sys->i_acodec, p_sys->i_sample_rate,
                 p_sys->i_channels, p_sys->i_abitrate / 1000 );
    }

    var_Get( p_stream, SOUT_CFG_PREFIX "afilter", &val );
    if( val.psz_string && *val.psz_string )
        p_sys->psz_af2 = val.psz_string;
    else
    {
        free( val.psz_string );
        p_sys->psz_af2 = NULL;
    }

    /* Video transcoding parameters */
    var_Get( p_stream, SOUT_CFG_PREFIX "venc", &val );
    p_sys->psz_venc = NULL;
    p_sys->p_video_cfg = NULL;
    if( val.psz_string && *val.psz_string )
    {
        char *psz_next;
        psz_next = config_ChainCreate( &p_sys->psz_venc, &p_sys->p_video_cfg,
                                   val.psz_string );
        free( psz_next );
    }
    free( val.psz_string );

    var_Get( p_stream, SOUT_CFG_PREFIX "vcodec", &val );
    p_sys->i_vcodec = 0;
    if( val.psz_string && *val.psz_string )
    {
        char fcc[4] = "    ";
        memcpy( fcc, val.psz_string, __MIN( strlen( val.psz_string ), 4 ) );
        p_sys->i_vcodec = VLC_FOURCC( fcc[0], fcc[1], fcc[2], fcc[3] );
    }
    free( val.psz_string );

    var_Get( p_stream, SOUT_CFG_PREFIX "vb", &val );
    p_sys->i_vbitrate = val.i_int;
    if( p_sys->i_vbitrate < 16000 ) p_sys->i_vbitrate *= 1000;

    var_Get( p_stream, SOUT_CFG_PREFIX "scale", &val );
    p_sys->f_scale = val.f_float;

    var_Get( p_stream, SOUT_CFG_PREFIX "fps", &val );
    p_sys->f_fps = val.f_float;

    var_Get( p_stream, SOUT_CFG_PREFIX "hurry-up", &val );
    p_sys->b_hurry_up = val.b_bool;

    var_Get( p_stream, SOUT_CFG_PREFIX "width", &val );
    p_sys->i_width = val.i_int;

    var_Get( p_stream, SOUT_CFG_PREFIX "height", &val );
    p_sys->i_height = val.i_int;

    var_Get( p_stream, SOUT_CFG_PREFIX "maxwidth", &val );
    p_sys->i_maxwidth = val.i_int;

    var_Get( p_stream, SOUT_CFG_PREFIX "maxheight", &val );
    p_sys->i_maxheight = val.i_int;

    var_Get( p_stream, SOUT_CFG_PREFIX "vfilter", &val );
    if( val.psz_string && *val.psz_string )
        p_sys->psz_vf2 = val.psz_string;
    else
    {
        free( val.psz_string );
        p_sys->psz_vf2 = NULL;
    }

    var_Get( p_stream, SOUT_CFG_PREFIX "deinterlace", &val );
    p_sys->b_deinterlace = val.b_bool;

    var_Get( p_stream, SOUT_CFG_PREFIX "deinterlace-module", &val );
    p_sys->psz_deinterlace = NULL;
    p_sys->p_deinterlace_cfg = NULL;
    if( val.psz_string && *val.psz_string )
    {
        char *psz_next;
        psz_next = config_ChainCreate( &p_sys->psz_deinterlace,
                                   &p_sys->p_deinterlace_cfg,
                                   val.psz_string );
        free( psz_next );
    }
    free( val.psz_string );

    var_Get( p_stream, SOUT_CFG_PREFIX "threads", &val );
    p_sys->i_threads = val.i_int;
    var_Get( p_stream, SOUT_CFG_PREFIX "high-priority", &val );
    p_sys->b_high_priority = val.b_bool;

    if( p_sys->i_vcodec )
    {
        msg_Dbg( p_stream, "codec video=%4.4s %dx%d scaling: %f %dkb/s",
                 (char *)&p_sys->i_vcodec, p_sys->i_width, p_sys->i_height,
                 p_sys->f_scale, p_sys->i_vbitrate / 1000 );
    }

    /* Subpictures transcoding parameters */
    p_sys->p_spu = NULL;
    p_sys->psz_senc = NULL;
    p_sys->p_spu_cfg = NULL;
    p_sys->i_scodec = 0;

    var_Get( p_stream, SOUT_CFG_PREFIX "senc", &val );
    if( val.psz_string && *val.psz_string )
    {
        char *psz_next;
        psz_next = config_ChainCreate( &p_sys->psz_senc, &p_sys->p_spu_cfg,
                                   val.psz_string );
        free( psz_next );
    }
    free( val.psz_string );

    var_Get( p_stream, SOUT_CFG_PREFIX "scodec", &val );
    if( val.psz_string && *val.psz_string )
    {
        char fcc[4] = "    ";
        memcpy( fcc, val.psz_string, __MIN( strlen( val.psz_string ), 4 ) );
        p_sys->i_scodec = VLC_FOURCC( fcc[0], fcc[1], fcc[2], fcc[3] );
    }
    free( val.psz_string );

    if( p_sys->i_scodec )
    {
        msg_Dbg( p_stream, "codec spu=%4.4s", (char *)&p_sys->i_scodec );
    }

    var_Get( p_stream, SOUT_CFG_PREFIX "soverlay", &val );
    p_sys->b_soverlay = val.b_bool;

    var_Get( p_stream, SOUT_CFG_PREFIX "sfilter", &val );
    if( val.psz_string && *val.psz_string )
    {
        p_sys->p_spu = spu_Create( p_stream );
        var_Create( p_sys->p_spu, "sub-filter", VLC_VAR_STRING );
        var_Set( p_sys->p_spu, "sub-filter", val );
        spu_Init( p_sys->p_spu );
    }
    free( val.psz_string );

    /* OSD menu transcoding parameters */
    p_sys->psz_osdenc = NULL;
    p_sys->p_osd_cfg  = NULL;
    p_sys->i_osdcodec = 0;
    p_sys->b_osd   = false;

    var_Get( p_stream, SOUT_CFG_PREFIX "osd", &val );
    if( val.b_bool )
    {
        vlc_value_t osd_val;
        char *psz_next;

        psz_next = config_ChainCreate( &p_sys->psz_osdenc,
                                   &p_sys->p_osd_cfg, strdup( "dvbsub") );
        free( psz_next );

        p_sys->i_osdcodec = VLC_CODEC_YUVP;

        msg_Dbg( p_stream, "codec osd=%4.4s", (char *)&p_sys->i_osdcodec );

        if( !p_sys->p_spu )
        {
            osd_val.psz_string = strdup("osdmenu");
            p_sys->p_spu = spu_Create( p_stream );
            var_Create( p_sys->p_spu, "sub-filter", VLC_VAR_STRING );
            var_Set( p_sys->p_spu, "sub-filter", osd_val );
            spu_Init( p_sys->p_spu );
            free( osd_val.psz_string );
        }
        else
        {
            osd_val.psz_string = strdup("osdmenu");
            var_Set( p_sys->p_spu, "sub-filter", osd_val );
            free( osd_val.psz_string );
        }
    }

    /* Audio settings */
    var_Get( p_stream, SOUT_CFG_PREFIX "audio-sync", &val );
    p_sys->b_master_sync = val.b_bool;
    if( p_sys->f_fps > 0 ) p_sys->b_master_sync = true;

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;
    p_stream->p_sys     = p_sys;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    sout_StreamDelete( p_sys->p_out );

    free( p_sys->psz_af2 );

    config_ChainDestroy( p_sys->p_audio_cfg );
    free( p_sys->psz_aenc );

    free( p_sys->psz_vf2 );

    config_ChainDestroy( p_sys->p_video_cfg );
    free( p_sys->psz_venc );

    config_ChainDestroy( p_sys->p_deinterlace_cfg );
    free( p_sys->psz_deinterlace );

    config_ChainDestroy( p_sys->p_spu_cfg );
    free( p_sys->psz_senc );

    if( p_sys->p_spu ) spu_Destroy( p_sys->p_spu );

    config_ChainDestroy( p_sys->p_osd_cfg );
    free( p_sys->psz_osdenc );

    vlc_object_release( p_sys );
}

struct sout_stream_id_t
{
    bool            b_transcode;

    /* id of the out stream */
    void *id;

    /* Decoder */
    decoder_t       *p_decoder;

    /* Filters */
    filter_chain_t  *p_f_chain;
    /* User specified filters */
    filter_chain_t  *p_uf_chain;

    /* Encoder */
    encoder_t       *p_encoder;

    /* Sync */
    date_t          interpolated_pts;
};

static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t *id;

    id = calloc( 1, sizeof( sout_stream_id_t ) );
    if( !id )
        goto error;

    id->id = NULL;
    id->p_decoder = NULL;
    id->p_encoder = NULL;

    /* Create decoder object */
    id->p_decoder = vlc_object_create( p_stream, VLC_OBJECT_DECODER );
    if( !id->p_decoder )
        goto error;
    vlc_object_attach( id->p_decoder, p_stream );
    id->p_decoder->p_module = NULL;
    id->p_decoder->fmt_in = *p_fmt;
    id->p_decoder->b_pace_control = true;

    /* Create encoder object */
    id->p_encoder = sout_EncoderCreate( p_stream );
    if( !id->p_encoder )
        goto error;
    vlc_object_attach( id->p_encoder, p_stream );
    id->p_encoder->p_module = NULL;

    /* Create destination format */
    es_format_Init( &id->p_encoder->fmt_out, p_fmt->i_cat, 0 );
    id->p_encoder->fmt_out.i_id    = p_fmt->i_id;
    id->p_encoder->fmt_out.i_group = p_fmt->i_group;
    if( p_fmt->psz_language )
        id->p_encoder->fmt_out.psz_language = strdup( p_fmt->psz_language );

    if( p_fmt->i_cat == AUDIO_ES && (p_sys->i_acodec || p_sys->psz_aenc) )
    {
        msg_Dbg( p_stream,
                 "creating audio transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
                 (char*)&p_fmt->i_codec, (char*)&p_sys->i_acodec );

        /* Complete destination format */
        id->p_encoder->fmt_out.i_codec = p_sys->i_acodec;
        id->p_encoder->fmt_out.audio.i_rate = p_sys->i_sample_rate > 0 ?
            p_sys->i_sample_rate : p_fmt->audio.i_rate;
        id->p_encoder->fmt_out.i_bitrate = p_sys->i_abitrate;
        id->p_encoder->fmt_out.audio.i_bitspersample =
            p_fmt->audio.i_bitspersample;
        id->p_encoder->fmt_out.audio.i_channels = p_sys->i_channels > 0 ?
            p_sys->i_channels : p_fmt->audio.i_channels;
        /* Sanity check for audio channels */
        id->p_encoder->fmt_out.audio.i_channels =
            __MIN( id->p_encoder->fmt_out.audio.i_channels,
                   id->p_decoder->fmt_in.audio.i_channels );
        id->p_encoder->fmt_out.audio.i_original_channels =
            id->p_decoder->fmt_in.audio.i_physical_channels;
        if( id->p_decoder->fmt_in.audio.i_channels ==
            id->p_encoder->fmt_out.audio.i_channels )
        {
            id->p_encoder->fmt_out.audio.i_physical_channels =
                id->p_decoder->fmt_in.audio.i_physical_channels;
        }
        else
        {
            id->p_encoder->fmt_out.audio.i_physical_channels =
                pi_channels_maps[id->p_encoder->fmt_out.audio.i_channels];
        }

        /* Build decoder -> filter -> encoder chain */
        if( transcode_audio_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create audio chain" );
            goto error;
        }

        /* Open output stream */
        id->id = sout_StreamIdAdd( p_sys->p_out, &id->p_encoder->fmt_out );
        id->b_transcode = true;

        if( !id->id )
        {
            transcode_audio_close( id );
            goto error;
        }

        date_Init( &id->interpolated_pts, p_fmt->audio.i_rate, 1 );
    }
    else if( p_fmt->i_cat == VIDEO_ES &&
             (p_sys->i_vcodec != 0 || p_sys->psz_venc) )
    {
        msg_Dbg( p_stream,
                 "creating video transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
                 (char*)&p_fmt->i_codec, (char*)&p_sys->i_vcodec );

        /* Complete destination format */
        id->p_encoder->fmt_out.i_codec = p_sys->i_vcodec;
        id->p_encoder->fmt_out.video.i_width  = p_sys->i_width & ~1;
        id->p_encoder->fmt_out.video.i_height = p_sys->i_height & ~1;
        id->p_encoder->fmt_out.i_bitrate = p_sys->i_vbitrate;

        /* Build decoder -> filter -> encoder chain */
        if( transcode_video_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create video chain" );
            goto error;
        }

        /* Stream will be added later on because we don't know
         * all the characteristics of the decoded stream yet */
        id->b_transcode = true;

        if( p_sys->f_fps > 0 )
        {
            id->p_encoder->fmt_out.video.i_frame_rate =
                (p_sys->f_fps * 1000) + 0.5;
            id->p_encoder->fmt_out.video.i_frame_rate_base =
                ENC_FRAMERATE_BASE;
        }
    }
    else if( ( p_fmt->i_cat == SPU_ES ) &&
             ( p_sys->i_scodec || p_sys->psz_senc ) )
    {
        msg_Dbg( p_stream, "creating subtitles transcoding from fcc=`%4.4s' "
                 "to fcc=`%4.4s'", (char*)&p_fmt->i_codec,
                 (char*)&p_sys->i_scodec );

        /* Complete destination format */
        id->p_encoder->fmt_out.i_codec = p_sys->i_scodec;

        /* build decoder -> filter -> encoder */
        if( transcode_spu_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create subtitles chain" );
            goto error;
        }

        /* open output stream */
        id->id = sout_StreamIdAdd( p_sys->p_out, &id->p_encoder->fmt_out );
        id->b_transcode = true;

        if( !id->id )
        {
            transcode_spu_close( id );
            goto error;
        }
    }
    else if( p_fmt->i_cat == SPU_ES && p_sys->b_soverlay )
    {
        msg_Dbg( p_stream, "subtitles (fcc=`%4.4s') overlaying",
                 (char*)&p_fmt->i_codec );

        id->b_transcode = true;

        /* Build decoder -> filter -> overlaying chain */
        if( transcode_spu_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create subtitles chain" );
            goto error;
        }
    }
    else if( !p_sys->b_osd && (p_sys->i_osdcodec != 0 || p_sys->psz_osdenc) )
    {
        msg_Dbg( p_stream, "creating osd transcoding from fcc=`%4.4s' "
                 "to fcc=`%4.4s'", (char*)&p_fmt->i_codec,
                 (char*)&p_sys->i_scodec );

        id->b_transcode = true;

        /* Create a fake OSD menu elementary stream */
        if( transcode_osd_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create osd chain" );
            goto error;
        }
        p_sys->b_osd = true;
    }
    else
    {
        msg_Dbg( p_stream, "not transcoding a stream (fcc=`%4.4s')",
                 (char*)&p_fmt->i_codec );
        id->id = sout_StreamIdAdd( p_sys->p_out, p_fmt );
        id->b_transcode = false;

        if( !id->id ) goto error;
    }

    return id;

error:
    if( id )
    {
        if( id->p_decoder )
        {
            vlc_object_detach( id->p_decoder );
            vlc_object_release( id->p_decoder );
            id->p_decoder = NULL;
        }

        if( id->p_encoder )
        {
            vlc_object_detach( id->p_encoder );
            es_format_Clean( &id->p_encoder->fmt_out );
            vlc_object_release( id->p_encoder );
            id->p_encoder = NULL;
        }

        free( id );
    }
    return NULL;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if( id->b_transcode )
    {
        switch( id->p_decoder->fmt_in.i_cat )
        {
        case AUDIO_ES:
            transcode_audio_close( id );
            break;
        case VIDEO_ES:
            transcode_video_close( p_stream, id );
            break;
        case SPU_ES:
            if( p_sys->b_osd )
                transcode_osd_close( p_stream, id );
            else
                transcode_spu_close( id );
            break;
        }
    }

    if( id->id ) sout_StreamIdDel( p_sys->p_out, id->id );

    if( id->p_decoder )
    {
        vlc_object_detach( id->p_decoder );
        vlc_object_release( id->p_decoder );
        id->p_decoder = NULL;
    }

    if( id->p_encoder )
    {
        vlc_object_detach( id->p_encoder );
        es_format_Clean( &id->p_encoder->fmt_out );
        vlc_object_release( id->p_encoder );
        id->p_encoder = NULL;
    }
    free( id );

    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    block_t *p_out = NULL;

    if( !id->b_transcode )
    {
        if( id->id )
            return sout_StreamIdSend( p_sys->p_out, id->id, p_buffer );

        block_Release( p_buffer );
        return VLC_EGENERIC;
    }

    switch( id->p_decoder->fmt_in.i_cat )
    {
    case AUDIO_ES:
        transcode_audio_process( p_stream, id, p_buffer, &p_out );
        break;

    case VIDEO_ES:
        if( transcode_video_process( p_stream, id, p_buffer, &p_out )
            != VLC_SUCCESS )
        {
            return VLC_EGENERIC;
        }
        break;

    case SPU_ES:
        /* Transcode OSD menu pictures. */
        if( p_sys->b_osd )
        {
            if( transcode_osd_process( p_stream, id, p_buffer, &p_out ) !=
                VLC_SUCCESS )
            {
                return VLC_EGENERIC;
            }
        }
        else if ( transcode_spu_process( p_stream, id, p_buffer, &p_out ) !=
            VLC_SUCCESS )
        {
            return VLC_EGENERIC;
        }
        break;

    default:
        p_out = NULL;
        block_Release( p_buffer );
        break;
    }

    if( p_out )
        return sout_StreamIdSend( p_sys->p_out, id->id, p_out );
    return VLC_SUCCESS;
}

/****************************************************************************
 * decoder helper
 ****************************************************************************/
static inline void video_timer_start( encoder_t * p_encoder )
{
    stats_TimerStart( p_encoder, "encoding video frame",
                      STATS_TIMER_VIDEO_FRAME_ENCODING );
}

static inline void video_timer_stop( encoder_t * p_encoder )
{
    stats_TimerStop( p_encoder, STATS_TIMER_VIDEO_FRAME_ENCODING );
}

static inline void video_timer_close( encoder_t * p_encoder )
{
    stats_TimerDump(  p_encoder, STATS_TIMER_VIDEO_FRAME_ENCODING );
    stats_TimerClean( p_encoder, STATS_TIMER_VIDEO_FRAME_ENCODING );
}

static inline void audio_timer_start( encoder_t * p_encoder )
{
    stats_TimerStart( p_encoder, "encoding audio frame",
                      STATS_TIMER_AUDIO_FRAME_ENCODING );
}

static inline void audio_timer_stop( encoder_t * p_encoder )
{
    stats_TimerStop( p_encoder, STATS_TIMER_AUDIO_FRAME_ENCODING );
}

static inline void audio_timer_close( encoder_t * p_encoder )
{
    stats_TimerDump(  p_encoder, STATS_TIMER_AUDIO_FRAME_ENCODING );
    stats_TimerClean( p_encoder, STATS_TIMER_AUDIO_FRAME_ENCODING );
}

/****************************************************************************
 * decoder reencoder part
 ****************************************************************************/

static block_t *transcode_audio_alloc( filter_t *p_filter, int size )
{
    VLC_UNUSED( p_filter );
    return block_Alloc( size );
}

static int transcode_audio_filter_allocation_init( filter_t *p_filter,
                                                   void *data )
{
    VLC_UNUSED(data);
    p_filter->pf_audio_buffer_new = transcode_audio_alloc;
    return VLC_SUCCESS;
}

static bool transcode_audio_filter_needed( const es_format_t *p_fmt1, const es_format_t *p_fmt2 )
{
    if( p_fmt1->i_codec != p_fmt2->i_codec ||
        p_fmt1->audio.i_channels != p_fmt2->audio.i_channels ||
        p_fmt1->audio.i_rate != p_fmt2->audio.i_rate )
        return true;
    return false;
}
static int transcode_audio_filter_chain_build( sout_stream_t *p_stream, filter_chain_t *p_chain,
                                               const es_format_t *p_dst, const es_format_t *p_src )
{
    if( !transcode_audio_filter_needed( p_dst, p_src ) )
        return VLC_SUCCESS;

    es_format_t current = *p_src;

    msg_Dbg( p_stream, "Looking for filter "
             "(%4.4s->%4.4s, channels %d->%d, rate %d->%d)",
         (const char *)&p_src->i_codec,
         (const char *)&p_dst->i_codec,
         p_src->audio.i_channels,
         p_dst->audio.i_channels,
         p_src->audio.i_rate,
         p_dst->audio.i_rate );

    /* If any filter is needed, convert to fl32 */
    if( current.i_codec != VLC_CODEC_FL32 )
    {
        /* First step, convert to fl32 */
        current.i_codec =
        current.audio.i_format = VLC_CODEC_FL32;

        if( !filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, &current ) )
        {
            msg_Err( p_stream, "Failed to find conversion filter to fl32" );
            return VLC_EGENERIC;
        }
        current = *filter_chain_GetFmtOut( p_chain );
    }

    /* Fix sample rate */
    if( current.audio.i_rate != p_dst->audio.i_rate )
    {
        current.audio.i_rate = p_dst->audio.i_rate;
        if( !filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, &current ) )
        {
            msg_Err( p_stream, "Failed to find conversion filter for resampling" );
            return VLC_EGENERIC;
        }
        current = *filter_chain_GetFmtOut( p_chain );
    }

    /* Fix channels */
    if( current.audio.i_channels != p_dst->audio.i_channels )
    {
        current.audio.i_channels = p_dst->audio.i_channels;
        current.audio.i_physical_channels = p_dst->audio.i_physical_channels;
        current.audio.i_original_channels = p_dst->audio.i_original_channels;

        if( ( !current.audio.i_physical_channels || !current.audio.i_original_channels ) &&
            current.audio.i_channels < 6 )
            current.audio.i_physical_channels =
            current.audio.i_original_channels = pi_channels_maps[current.audio.i_channels];

        if( !filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, &current ) )
        {
            msg_Err( p_stream, "Failed to find conversion filter for channel mixing" );
            return VLC_EGENERIC;
        }
        current = *filter_chain_GetFmtOut( p_chain );
    }

    /* And last step, convert to the requested codec */
    if( current.i_codec != p_dst->i_codec )
    {
        current.i_codec = p_dst->i_codec;
        if( !filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, &current ) )
        {
            msg_Err( p_stream, "Failed to find conversion filter to %4.4s",
                     (const char*)&p_dst->i_codec);
            return VLC_EGENERIC;
        }
        current = *filter_chain_GetFmtOut( p_chain );
    }

    if( transcode_audio_filter_needed( p_dst, &current ) )
    {
        /* Weird case, a filter has side effects, doomed */
        msg_Err( p_stream, "Failed to create a valid audio filter chain" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_stream, "Got complete audio filter chain" );
    return VLC_SUCCESS;
}

static int transcode_audio_new( sout_stream_t *p_stream,
                                sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    es_format_t fmt_last;

    /*
     * Open decoder
     */

    /* Initialization of decoder structures */
    id->p_decoder->fmt_out = id->p_decoder->fmt_in;
    id->p_decoder->fmt_out.i_extra = 0;
    id->p_decoder->fmt_out.p_extra = 0;
    id->p_decoder->pf_decode_audio = NULL;
    id->p_decoder->pf_aout_buffer_new = audio_new_buffer;
    id->p_decoder->pf_aout_buffer_del = audio_del_buffer;
    /* id->p_decoder->p_cfg = p_sys->p_audio_cfg; */

    id->p_decoder->p_module =
        module_need( id->p_decoder, "decoder", "$codec", false );
    if( !id->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find audio decoder" );
        return VLC_EGENERIC;
    }
    id->p_decoder->fmt_out.audio.i_bitspersample =
        aout_BitsPerSample( id->p_decoder->fmt_out.i_codec );
    fmt_last = id->p_decoder->fmt_out;
    /* Fix AAC SBR changing number of channels and sampling rate */
    if( !(id->p_decoder->fmt_in.i_codec == VLC_CODEC_MP4A &&
        fmt_last.audio.i_rate != id->p_encoder->fmt_in.audio.i_rate &&
        fmt_last.audio.i_channels != id->p_encoder->fmt_in.audio.i_channels) )
        fmt_last.audio.i_rate = id->p_decoder->fmt_in.audio.i_rate;

    /*
     * Open encoder
     */

    /* Initialization of encoder format structures */
    es_format_Init( &id->p_encoder->fmt_in, id->p_decoder->fmt_in.i_cat,
                    id->p_decoder->fmt_out.i_codec );
    id->p_encoder->fmt_in.audio.i_format = id->p_decoder->fmt_out.i_codec;

    id->p_encoder->fmt_in.audio.i_rate = id->p_encoder->fmt_out.audio.i_rate;
    id->p_encoder->fmt_in.audio.i_physical_channels =
        id->p_encoder->fmt_out.audio.i_physical_channels;
    id->p_encoder->fmt_in.audio.i_original_channels =
        id->p_encoder->fmt_out.audio.i_original_channels;
    id->p_encoder->fmt_in.audio.i_channels =
        id->p_encoder->fmt_out.audio.i_channels;
    id->p_encoder->fmt_in.audio.i_bitspersample =
        aout_BitsPerSample( id->p_encoder->fmt_in.i_codec );

    id->p_encoder->p_cfg = p_stream->p_sys->p_audio_cfg;
    id->p_encoder->p_module =
        module_need( id->p_encoder, "encoder", p_sys->psz_aenc, true );
    if( !id->p_encoder->p_module )
    {
        msg_Err( p_stream, "cannot find audio encoder (module:%s fourcc:%4.4s)",
                 p_sys->psz_aenc ? p_sys->psz_aenc : "any",
                 (char *)&p_sys->i_acodec );
        module_unneed( id->p_decoder, id->p_decoder->p_module );
        id->p_decoder->p_module = NULL;
        return VLC_EGENERIC;
    }
    id->p_encoder->fmt_in.audio.i_format = id->p_encoder->fmt_in.i_codec;
    id->p_encoder->fmt_in.audio.i_bitspersample =
        aout_BitsPerSample( id->p_encoder->fmt_in.i_codec );

    /* Load user specified audio filters */
    if( p_sys->psz_af2 )
    {
        es_format_t fmt_fl32 = fmt_last;
        fmt_fl32.i_codec =
        fmt_fl32.audio.i_format = VLC_CODEC_FL32;
        if( transcode_audio_filter_chain_build( p_stream, id->p_uf_chain,
                                                &fmt_fl32, &fmt_last ) )
        {
            transcode_audio_close( id );
            return VLC_EGENERIC;
        }
        fmt_last = fmt_fl32;

        id->p_uf_chain = filter_chain_New( p_stream, "audio filter2", false,
                                           transcode_audio_filter_allocation_init, NULL, NULL );
        filter_chain_Reset( id->p_uf_chain, &fmt_last, &fmt_fl32 );
        if( filter_chain_AppendFromString( id->p_uf_chain, p_sys->psz_af2 ) > 0 )
            fmt_last = *filter_chain_GetFmtOut( id->p_uf_chain );
    }

    /* Load conversion filters */
    id->p_f_chain = filter_chain_New( p_stream, "audio filter2", true,
                    transcode_audio_filter_allocation_init, NULL, NULL );
    filter_chain_Reset( id->p_f_chain, &fmt_last, &id->p_encoder->fmt_in );

    if( transcode_audio_filter_chain_build( p_stream, id->p_f_chain,
                                            &id->p_encoder->fmt_in, &fmt_last ) )
    {
        transcode_audio_close( id );
        return VLC_EGENERIC;
    }
    fmt_last = id->p_encoder->fmt_in;

    /* */
    id->p_encoder->fmt_out.i_codec =
        vlc_fourcc_GetCodec( AUDIO_ES, id->p_encoder->fmt_out.i_codec );

    return VLC_SUCCESS;
}

static void transcode_audio_close( sout_stream_id_t *id )
{
    audio_timer_close( id->p_encoder );

    /* Close decoder */
    if( id->p_decoder->p_module )
        module_unneed( id->p_decoder, id->p_decoder->p_module );
    id->p_decoder->p_module = NULL;

    if( id->p_decoder->p_description )
        vlc_meta_Delete( id->p_decoder->p_description );
    id->p_decoder->p_description = NULL;

    /* Close encoder */
    if( id->p_encoder->p_module )
        module_unneed( id->p_encoder, id->p_encoder->p_module );
    id->p_encoder->p_module = NULL;

    /* Close filters */
    if( id->p_uf_chain )
        filter_chain_Delete( id->p_uf_chain );
    if( id->p_f_chain )
        filter_chain_Delete( id->p_f_chain );
}

static int transcode_audio_process( sout_stream_t *p_stream,
                                    sout_stream_id_t *id,
                                    block_t *in, block_t **out )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    aout_buffer_t *p_audio_buf;
    block_t *p_block, *p_audio_block;
    *out = NULL;

    while( (p_audio_buf = id->p_decoder->pf_decode_audio( id->p_decoder,
                                                          &in )) )
    {
        sout_UpdateStatistic( p_stream->p_sout, SOUT_STATISTIC_DECODED_AUDIO, 1 );
        if( p_sys->b_master_sync )
        {
            mtime_t i_dts = date_Get( &id->interpolated_pts ) + 1;
            if ( p_audio_buf->i_pts - i_dts > MASTER_SYNC_MAX_DRIFT
                  || p_audio_buf->i_pts - i_dts < -MASTER_SYNC_MAX_DRIFT )
            {
                msg_Dbg( p_stream, "drift is too high, resetting master sync" );
                date_Set( &id->interpolated_pts, p_audio_buf->i_pts );
                i_dts = p_audio_buf->i_pts + 1;
            }
            p_sys->i_master_drift = p_audio_buf->i_pts - i_dts;
            date_Increment( &id->interpolated_pts, p_audio_buf->i_nb_samples );
            p_audio_buf->i_pts -= p_sys->i_master_drift;
            p_audio_buf->end_date -= p_sys->i_master_drift;
        }

        p_audio_block = p_audio_buf->p_sys;
        p_audio_block->i_buffer = p_audio_buf->i_nb_bytes;
        p_audio_block->i_dts = p_audio_block->i_pts =
            p_audio_buf->i_pts;
        p_audio_block->i_length = p_audio_buf->end_date -
            p_audio_buf->i_pts;
        p_audio_block->i_nb_samples = p_audio_buf->i_nb_samples;

        /* Run filter chain */
        if( id->p_uf_chain )
        {
            p_audio_block = filter_chain_AudioFilter( id->p_uf_chain, p_audio_block );
            assert( p_audio_block );
        }

        p_audio_block = filter_chain_AudioFilter( id->p_f_chain, p_audio_block );
        assert( p_audio_block );

        p_audio_buf->p_buffer = p_audio_block->p_buffer;
        p_audio_buf->i_nb_bytes = p_audio_block->i_buffer;
        p_audio_buf->i_nb_samples = p_audio_block->i_nb_samples;
        p_audio_buf->i_pts = p_audio_block->i_dts;
        p_audio_buf->end_date = p_audio_block->i_dts + p_audio_block->i_length;

        audio_timer_start( id->p_encoder );
        p_block = id->p_encoder->pf_encode_audio( id->p_encoder, p_audio_buf );
        audio_timer_stop( id->p_encoder );

        block_ChainAppend( out, p_block );
        block_Release( p_audio_block );
        free( p_audio_buf );
    }

    return VLC_SUCCESS;
}

static aout_buffer_t *audio_new_buffer( decoder_t *p_dec, int i_samples )
{
    aout_buffer_t *p_buffer;
    block_t *p_block;
    int i_size;

    if( p_dec->fmt_out.audio.i_bitspersample )
    {
        i_size = i_samples * p_dec->fmt_out.audio.i_bitspersample / 8 *
            p_dec->fmt_out.audio.i_channels;
    }
    else if( p_dec->fmt_out.audio.i_bytes_per_frame &&
             p_dec->fmt_out.audio.i_frame_length )
    {
        i_size = i_samples * p_dec->fmt_out.audio.i_bytes_per_frame /
            p_dec->fmt_out.audio.i_frame_length;
    }
    else
    {
        i_size = i_samples * 4 * p_dec->fmt_out.audio.i_channels;
    }

    p_buffer = malloc( sizeof(aout_buffer_t) );
    if( !p_buffer ) return NULL;
    p_buffer->i_flags = 0;
    p_buffer->p_sys = p_block = block_New( p_dec, i_size );

    p_buffer->p_buffer = p_block->p_buffer;
    p_buffer->i_size = p_buffer->i_nb_bytes = p_block->i_buffer;
    p_buffer->i_nb_samples = i_samples;
    p_block->i_nb_samples = i_samples;

    return p_buffer;
}

static void audio_del_buffer( decoder_t *p_dec, aout_buffer_t *p_buffer )
{
    VLC_UNUSED(p_dec);
    if( p_buffer && p_buffer->p_sys ) block_Release( p_buffer->p_sys );
    free( p_buffer );
}

/*
 * video
 */

static picture_t *transcode_video_filter_buffer_new( filter_t *p_filter )
{
    p_filter->fmt_out.video.i_chroma = p_filter->fmt_out.i_codec;
    return picture_New( p_filter->fmt_out.video.i_chroma,
                        p_filter->fmt_out.video.i_width,
                        p_filter->fmt_out.video.i_height,
                        p_filter->fmt_out.video.i_aspect );
}
static void transcode_video_filter_buffer_del( filter_t *p_filter, picture_t *p_pic )
{
    VLC_UNUSED(p_filter);
    picture_Release( p_pic );
}

static int transcode_video_filter_allocation_init( filter_t *p_filter,
                                                   void *p_data )
{
    VLC_UNUSED(p_data);
    p_filter->pf_vout_buffer_new = transcode_video_filter_buffer_new;
    p_filter->pf_vout_buffer_del = transcode_video_filter_buffer_del;
    return VLC_SUCCESS;
}

static void transcode_video_filter_allocation_clear( filter_t *p_filter )
{
    VLC_UNUSED(p_filter);
}

static int transcode_video_new( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* Open decoder
     * Initialization of decoder structures
     */
    id->p_decoder->fmt_out = id->p_decoder->fmt_in;
    id->p_decoder->fmt_out.i_extra = 0;
    id->p_decoder->fmt_out.p_extra = 0;
    id->p_decoder->pf_decode_video = NULL;
    id->p_decoder->pf_get_cc = NULL;
    id->p_decoder->pf_get_cc = 0;
    id->p_decoder->pf_vout_buffer_new = video_new_buffer_decoder;
    id->p_decoder->pf_vout_buffer_del = video_del_buffer_decoder;
    id->p_decoder->pf_picture_link    = video_link_picture_decoder;
    id->p_decoder->pf_picture_unlink  = video_unlink_picture_decoder;
    id->p_decoder->p_owner = malloc( sizeof(decoder_owner_sys_t) );
    if( !id->p_decoder->p_owner )
        return VLC_EGENERIC;

    id->p_decoder->p_owner->p_sys = p_sys;
    /* id->p_decoder->p_cfg = p_sys->p_video_cfg; */

    id->p_decoder->p_module =
        module_need( id->p_decoder, "decoder", "$codec", false );

    if( !id->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find video decoder" );
        free( id->p_decoder->p_owner );
        return VLC_EGENERIC;
    }

    /*
     * Open encoder.
     * Because some info about the decoded input will only be available
     * once the first frame is decoded, we actually only test the availability
     * of the encoder here.
     */

    /* Initialization of encoder format structures */
    es_format_Init( &id->p_encoder->fmt_in, id->p_decoder->fmt_in.i_cat,
                    id->p_decoder->fmt_out.i_codec );
    id->p_encoder->fmt_in.video.i_chroma = id->p_decoder->fmt_out.i_codec;

    /* The dimensions will be set properly later on.
     * Just put sensible values so we can test an encoder is available. */
    id->p_encoder->fmt_in.video.i_width =
        id->p_encoder->fmt_out.video.i_width
          ? id->p_encoder->fmt_out.video.i_width
          : id->p_decoder->fmt_in.video.i_width
            ? id->p_decoder->fmt_in.video.i_width : 16;
    id->p_encoder->fmt_in.video.i_height =
        id->p_encoder->fmt_out.video.i_height
          ? id->p_encoder->fmt_out.video.i_height
          : id->p_decoder->fmt_in.video.i_height
            ? id->p_decoder->fmt_in.video.i_height : 16;
    id->p_encoder->fmt_in.video.i_frame_rate = ENC_FRAMERATE;
    id->p_encoder->fmt_in.video.i_frame_rate_base = ENC_FRAMERATE_BASE;

    id->p_encoder->i_threads = p_sys->i_threads;
    id->p_encoder->p_cfg = p_sys->p_video_cfg;

    id->p_encoder->p_module =
        module_need( id->p_encoder, "encoder", p_sys->psz_venc, true );
    if( !id->p_encoder->p_module )
    {
        msg_Err( p_stream, "cannot find video encoder (module:%s fourcc:%4.4s)",
                 p_sys->psz_venc ? p_sys->psz_venc : "any",
                 (char *)&p_sys->i_vcodec );
        module_unneed( id->p_decoder, id->p_decoder->p_module );
        id->p_decoder->p_module = 0;
        free( id->p_decoder->p_owner );
        return VLC_EGENERIC;
    }

    /* Close the encoder.
     * We'll open it only when we have the first frame. */
    module_unneed( id->p_encoder, id->p_encoder->p_module );
    if( id->p_encoder->fmt_out.p_extra )
    {
        free( id->p_encoder->fmt_out.p_extra );
        id->p_encoder->fmt_out.p_extra = NULL;
        id->p_encoder->fmt_out.i_extra = 0;
    }
    id->p_encoder->p_module = NULL;

    if( p_sys->i_threads >= 1 )
    {
        int i_priority = p_sys->b_high_priority ? VLC_THREAD_PRIORITY_OUTPUT :
                           VLC_THREAD_PRIORITY_VIDEO;
        p_sys->id_video = id;
        vlc_mutex_init( &p_sys->lock_out );
        vlc_cond_init( &p_sys->cond );
        memset( p_sys->pp_pics, 0, sizeof(p_sys->pp_pics) );
        p_sys->i_first_pic = 0;
        p_sys->i_last_pic = 0;
        p_sys->p_buffers = NULL;
        p_sys->b_die = p_sys->b_error = 0;
        if( vlc_thread_create( p_sys, "encoder", EncoderThread, i_priority ) )
        {
            msg_Err( p_stream, "cannot spawn encoder thread" );
            module_unneed( id->p_decoder, id->p_decoder->p_module );
            id->p_decoder->p_module = 0;
            free( id->p_decoder->p_owner );
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static void transcode_video_encoder_init( sout_stream_t *p_stream,
                                          sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* Calculate scaling
     * width/height of source */
    int i_src_width = id->p_decoder->fmt_out.video.i_width;
    int i_src_height = id->p_decoder->fmt_out.video.i_height;

    /* with/height scaling */
    float f_scale_width = 1;
    float f_scale_height = 1;

    /* width/height of output stream */
    int i_dst_width;
    int i_dst_height;

    /* aspect ratio */
    float f_aspect = (float)id->p_decoder->fmt_out.video.i_aspect /
                            VOUT_ASPECT_FACTOR;

    msg_Dbg( p_stream, "decoder aspect is %i:%i",
                 id->p_decoder->fmt_out.video.i_aspect, VOUT_ASPECT_FACTOR );

    /* Change f_aspect from source frame to source pixel */
    f_aspect = f_aspect * i_src_height / i_src_width;
    msg_Dbg( p_stream, "source pixel aspect is %f:1", f_aspect );

    /* Calculate scaling factor for specified parameters */
    if( id->p_encoder->fmt_out.video.i_width <= 0 &&
        id->p_encoder->fmt_out.video.i_height <= 0 && p_sys->f_scale )
    {
        /* Global scaling. Make sure width will remain a factor of 16 */
        float f_real_scale;
        int  i_new_height;
        int i_new_width = i_src_width * p_sys->f_scale;

        if( i_new_width % 16 <= 7 && i_new_width >= 16 )
            i_new_width -= i_new_width % 16;
        else
            i_new_width += 16 - i_new_width % 16;

        f_real_scale = (float)( i_new_width ) / (float) i_src_width;

        i_new_height = __MAX( 16, i_src_height * (float)f_real_scale );

        f_scale_width = f_real_scale;
        f_scale_height = (float) i_new_height / (float) i_src_height;
    }
    else if( id->p_encoder->fmt_out.video.i_width > 0 &&
             id->p_encoder->fmt_out.video.i_height <= 0 )
    {
        /* Only width specified */
        f_scale_width = (float)id->p_encoder->fmt_out.video.i_width/i_src_width;
        f_scale_height = f_scale_width;
    }
    else if( id->p_encoder->fmt_out.video.i_width <= 0 &&
             id->p_encoder->fmt_out.video.i_height > 0 )
    {
         /* Only height specified */
         f_scale_height = (float)id->p_encoder->fmt_out.video.i_height/i_src_height;
         f_scale_width = f_scale_height;
     }
     else if( id->p_encoder->fmt_out.video.i_width > 0 &&
              id->p_encoder->fmt_out.video.i_height > 0 )
     {
         /* Width and height specified */
         f_scale_width = (float)id->p_encoder->fmt_out.video.i_width/i_src_width;
         f_scale_height = (float)id->p_encoder->fmt_out.video.i_height/i_src_height;
     }

     /* check maxwidth and maxheight
      */
     if( p_sys->i_maxwidth && f_scale_width > (float)p_sys->i_maxwidth /
                                                     i_src_width )
     {
         f_scale_width = (float)p_sys->i_maxwidth / i_src_width;
     }

     if( p_sys->i_maxheight && f_scale_height > (float)p_sys->i_maxheight /
                                                       i_src_height )
     {
         f_scale_height = (float)p_sys->i_maxheight / i_src_height;
     }


     /* Change aspect ratio from source pixel to scaled pixel */
     f_aspect = f_aspect * f_scale_height / f_scale_width;
     msg_Dbg( p_stream, "scaled pixel aspect is %f:1", f_aspect );

     /* f_scale_width and f_scale_height are now final */
     /* Calculate width, height from scaling
      * Make sure its multiple of 2
      */
     i_dst_width =  2 * (int)(f_scale_width*i_src_width/2+0.5);
     i_dst_height = 2 * (int)(f_scale_height*i_src_height/2+0.5);

     /* Change aspect ratio from scaled pixel to output frame */
     f_aspect = f_aspect * i_dst_width / i_dst_height;

     /* Store calculated values */
     id->p_encoder->fmt_out.video.i_width =
     id->p_encoder->fmt_out.video.i_visible_width = i_dst_width;
     id->p_encoder->fmt_out.video.i_height =
     id->p_encoder->fmt_out.video.i_visible_height = i_dst_height;

     id->p_encoder->fmt_in.video.i_width =
     id->p_encoder->fmt_in.video.i_visible_width = i_dst_width;
     id->p_encoder->fmt_in.video.i_height =
     id->p_encoder->fmt_in.video.i_visible_height = i_dst_height;

     msg_Dbg( p_stream, "source %ix%i, destination %ix%i",
         i_src_width, i_src_height,
         i_dst_width, i_dst_height
     );

    /* Handle frame rate conversion */
    if( !id->p_encoder->fmt_out.video.i_frame_rate ||
        !id->p_encoder->fmt_out.video.i_frame_rate_base )
    {
        if( id->p_decoder->fmt_out.video.i_frame_rate &&
            id->p_decoder->fmt_out.video.i_frame_rate_base )
        {
            id->p_encoder->fmt_out.video.i_frame_rate =
                id->p_decoder->fmt_out.video.i_frame_rate;
            id->p_encoder->fmt_out.video.i_frame_rate_base =
                id->p_decoder->fmt_out.video.i_frame_rate_base;
        }
        else
        {
            /* Pick a sensible default value */
            id->p_encoder->fmt_out.video.i_frame_rate = ENC_FRAMERATE;
            id->p_encoder->fmt_out.video.i_frame_rate_base = ENC_FRAMERATE_BASE;
        }
    }

    id->p_encoder->fmt_in.video.i_frame_rate =
        id->p_encoder->fmt_out.video.i_frame_rate;
    id->p_encoder->fmt_in.video.i_frame_rate_base =
        id->p_encoder->fmt_out.video.i_frame_rate_base;

    date_Init( &id->interpolated_pts,
               id->p_encoder->fmt_out.video.i_frame_rate,
               id->p_encoder->fmt_out.video.i_frame_rate_base );

    /* Check whether a particular aspect ratio was requested */
    if( !id->p_encoder->fmt_out.video.i_aspect )
    {
        id->p_encoder->fmt_out.video.i_aspect =
                (int)( f_aspect * VOUT_ASPECT_FACTOR + 0.5 );
    }
    id->p_encoder->fmt_in.video.i_aspect =
        id->p_encoder->fmt_out.video.i_aspect;

    msg_Dbg( p_stream, "encoder aspect is %i:%i",
             id->p_encoder->fmt_out.video.i_aspect, VOUT_ASPECT_FACTOR );

    id->p_encoder->fmt_in.video.i_chroma = id->p_encoder->fmt_in.i_codec;
}

static int transcode_video_encoder_open( sout_stream_t *p_stream,
                                         sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;


    msg_Dbg( p_stream, "destination (after video filters) %ix%i",
             id->p_encoder->fmt_in.video.i_width,
             id->p_encoder->fmt_in.video.i_height );

    id->p_encoder->p_module =
        module_need( id->p_encoder, "encoder", p_sys->psz_venc, true );
    if( !id->p_encoder->p_module )
    {
        msg_Err( p_stream, "cannot find video encoder (module:%s fourcc:%4.4s)",
                 p_sys->psz_venc ? p_sys->psz_venc : "any",
                 (char *)&p_sys->i_vcodec );
        return VLC_EGENERIC;
    }

    id->p_encoder->fmt_in.video.i_chroma = id->p_encoder->fmt_in.i_codec;

    /*  */
    id->p_encoder->fmt_out.i_codec =
        vlc_fourcc_GetCodec( VIDEO_ES, id->p_encoder->fmt_out.i_codec );

    id->id = sout_StreamIdAdd( p_stream->p_sys->p_out,
                               &id->p_encoder->fmt_out );
    if( !id->id )
    {
        msg_Err( p_stream, "cannot add this stream" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void transcode_video_close( sout_stream_t *p_stream,
                                   sout_stream_id_t *id )
{
    if( p_stream->p_sys->i_threads >= 1 )
    {
        vlc_mutex_lock( &p_stream->p_sys->lock_out );
        vlc_object_kill( p_stream->p_sys );
        vlc_cond_signal( &p_stream->p_sys->cond );
        vlc_mutex_unlock( &p_stream->p_sys->lock_out );
        vlc_thread_join( p_stream->p_sys );
        vlc_mutex_destroy( &p_stream->p_sys->lock_out );
        vlc_cond_destroy( &p_stream->p_sys->cond );
    }

    video_timer_close( id->p_encoder );

    /* Close decoder */
    if( id->p_decoder->p_module )
        module_unneed( id->p_decoder, id->p_decoder->p_module );
    if( id->p_decoder->p_description )
        vlc_meta_Delete( id->p_decoder->p_description );

    free( id->p_decoder->p_owner );

    /* Close encoder */
    if( id->p_encoder->p_module )
        module_unneed( id->p_encoder, id->p_encoder->p_module );

    /* Close filters */
    if( id->p_f_chain )
        filter_chain_Delete( id->p_f_chain );
    if( id->p_uf_chain )
        filter_chain_Delete( id->p_uf_chain );
}

static int transcode_video_process( sout_stream_t *p_stream,
                                    sout_stream_id_t *id,
                                    block_t *in, block_t **out )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i_duplicate = 1;
    picture_t *p_pic, *p_pic2 = NULL;
    *out = NULL;

    while( (p_pic = id->p_decoder->pf_decode_video( id->p_decoder, &in )) )
    {
        subpicture_t *p_subpic = NULL;

        sout_UpdateStatistic( p_stream->p_sout, SOUT_STATISTIC_DECODED_VIDEO, 1 );

        if( p_stream->p_sout->i_out_pace_nocontrol && p_sys->b_hurry_up )
        {
            mtime_t current_date = mdate();
            if( current_date + 50000 > p_pic->date )
            {
                msg_Dbg( p_stream, "late picture skipped (%"PRId64")",
                         current_date + 50000 - p_pic->date );
                picture_Release( p_pic );
                continue;
            }
        }

        if( p_sys->b_master_sync )
        {
            mtime_t i_video_drift;
            mtime_t i_master_drift = p_sys->i_master_drift;
            mtime_t i_pts;

            i_pts = date_Get( &id->interpolated_pts ) + 1;
            if ( p_pic->date - i_pts > MASTER_SYNC_MAX_DRIFT
                  || p_pic->date - i_pts < -MASTER_SYNC_MAX_DRIFT )
            {
                msg_Dbg( p_stream, "drift is too high, resetting master sync" );
                date_Set( &id->interpolated_pts, p_pic->date );
                i_pts = p_pic->date + 1;
            }
            i_video_drift = p_pic->date - i_pts;
            i_duplicate = 1;

            /* Set the pts of the frame being encoded */
            p_pic->date = i_pts;

            if( i_video_drift < (i_master_drift - 50000) )
            {
#if 0
                msg_Dbg( p_stream, "dropping frame (%i)",
                         (int)(i_video_drift - i_master_drift) );
#endif
                picture_Release( p_pic );
                continue;
            }
            else if( i_video_drift > (i_master_drift + 50000) )
            {
#if 0
                msg_Dbg( p_stream, "adding frame (%i)",
                         (int)(i_video_drift - i_master_drift) );
#endif
                i_duplicate = 2;
            }
        }

        if( !id->p_encoder->p_module )
        {
            transcode_video_encoder_init( p_stream, id );

            id->p_f_chain = filter_chain_New( p_stream, "video filter2",
                                              false,
                               transcode_video_filter_allocation_init,
                               transcode_video_filter_allocation_clear,
                               p_stream->p_sys );

            /* Deinterlace */
            if( p_stream->p_sys->b_deinterlace )
            {
                filter_chain_AppendFilter( id->p_f_chain,
                                           p_sys->psz_deinterlace,
                                           p_sys->p_deinterlace_cfg,
                                           &id->p_decoder->fmt_out,
                                           &id->p_decoder->fmt_out );
            }

            /* Take care of the scaling and chroma conversions */
            if( ( id->p_decoder->fmt_out.video.i_chroma !=
                  id->p_encoder->fmt_in.video.i_chroma ) ||
                ( id->p_decoder->fmt_out.video.i_width !=
                  id->p_encoder->fmt_in.video.i_width ) ||
                ( id->p_decoder->fmt_out.video.i_height !=
                  id->p_encoder->fmt_in.video.i_height ) )
            {
                filter_chain_AppendFilter( id->p_f_chain,
                                           NULL, NULL,
                                           &id->p_decoder->fmt_out,
                                           &id->p_encoder->fmt_in );
            }

            if( p_sys->psz_vf2 )
            {
                const es_format_t *p_fmt_out;
                id->p_uf_chain = filter_chain_New( p_stream, "video filter2",
                                                   true,
                                   transcode_video_filter_allocation_init,
                                   transcode_video_filter_allocation_clear,
                                   p_stream->p_sys );
                filter_chain_Reset( id->p_uf_chain, &id->p_encoder->fmt_in,
                                    &id->p_encoder->fmt_in );
                filter_chain_AppendFromString( id->p_uf_chain, p_sys->psz_vf2 );
                p_fmt_out = filter_chain_GetFmtOut( id->p_uf_chain );
                es_format_Copy( &id->p_encoder->fmt_in, p_fmt_out );
                id->p_encoder->fmt_out.video.i_width =
                    id->p_encoder->fmt_in.video.i_width;
                id->p_encoder->fmt_out.video.i_height =
                    id->p_encoder->fmt_in.video.i_height;
                id->p_encoder->fmt_out.video.i_aspect =
                    id->p_encoder->fmt_in.video.i_aspect;
            }

            if( transcode_video_encoder_open( p_stream, id ) != VLC_SUCCESS )
            {
                picture_Release( p_pic );
                transcode_video_close( p_stream, id );
                id->b_transcode = false;
                return VLC_EGENERIC;
            }
        }

        /* Run filter chain */
        if( id->p_f_chain )
            p_pic = filter_chain_VideoFilter( id->p_f_chain, p_pic );

        /*
         * Encoding
         */

        /* Check if we have a subpicture to overlay */
        if( p_sys->p_spu )
        {
            p_subpic = spu_SortSubpictures( p_sys->p_spu, p_pic->date, false );
            /* TODO: get another pic */
        }

        /* Overlay subpicture */
        if( p_subpic )
        {
            video_format_t fmt;

            if( picture_IsReferenced( p_pic ) && !filter_chain_GetLength( id->p_f_chain ) )
            {
                /* We can't modify the picture, we need to duplicate it */
                picture_t *p_tmp = video_new_buffer_decoder( id->p_decoder );
                if( p_tmp )
                {
                    picture_Copy( p_tmp, p_pic );
                    picture_Release( p_pic );
                    p_pic = p_tmp;
                }
            }

            if( filter_chain_GetLength( id->p_f_chain ) > 0 )
                fmt = filter_chain_GetFmtOut( id->p_f_chain )->video;
            else
                fmt = id->p_decoder->fmt_out.video;

            /* FIXME (shouldn't have to be done here) */
            fmt.i_sar_num = fmt.i_aspect * fmt.i_height / fmt.i_width;
            fmt.i_sar_den = VOUT_ASPECT_FACTOR;

            /* FIXME the mdate() seems highly suspicious */
            spu_RenderSubpictures( p_sys->p_spu, p_pic, &fmt,
                                   p_subpic, &id->p_decoder->fmt_out.video, mdate() );
        }

        /* Run user specified filter chain */
        if( id->p_uf_chain )
            p_pic = filter_chain_VideoFilter( id->p_uf_chain, p_pic );

        if( p_sys->i_threads == 0 )
        {
            block_t *p_block;

            video_timer_start( id->p_encoder );
            p_block = id->p_encoder->pf_encode_video( id->p_encoder, p_pic );
            video_timer_stop( id->p_encoder );

            block_ChainAppend( out, p_block );
        }

        if( p_sys->b_master_sync )
        {
            mtime_t i_pts = date_Get( &id->interpolated_pts ) + 1;
            if ( p_pic->date - i_pts > MASTER_SYNC_MAX_DRIFT
                  || p_pic->date - i_pts < -MASTER_SYNC_MAX_DRIFT )
            {
                msg_Dbg( p_stream, "drift is too high, resetting master sync" );
                date_Set( &id->interpolated_pts, p_pic->date );
                i_pts = p_pic->date + 1;
            }
            date_Increment( &id->interpolated_pts, 1 );
        }

        if( p_sys->b_master_sync && i_duplicate > 1 )
        {
            mtime_t i_pts = date_Get( &id->interpolated_pts ) + 1;
            if( (p_pic->date - i_pts > MASTER_SYNC_MAX_DRIFT)
                 || ((p_pic->date - i_pts) < -MASTER_SYNC_MAX_DRIFT) )
            {
                msg_Dbg( p_stream, "drift is too high, resetting master sync" );
                date_Set( &id->interpolated_pts, p_pic->date );
                i_pts = p_pic->date + 1;
            }
            date_Increment( &id->interpolated_pts, 1 );

            if( p_sys->i_threads >= 1 )
            {
                /* We can't modify the picture, we need to duplicate it */
                p_pic2 = video_new_buffer_decoder( id->p_decoder );
                if( p_pic2 != NULL )
                {
                    picture_Copy( p_pic2, p_pic );
                    p_pic2->date = i_pts;
                }
            }
            else
            {
                block_t *p_block;
                p_pic->date = i_pts;
                video_timer_start( id->p_encoder );
                p_block = id->p_encoder->pf_encode_video(id->p_encoder, p_pic);
                video_timer_stop( id->p_encoder );
                block_ChainAppend( out, p_block );
            }
        }

        if( p_sys->i_threads == 0 )
        {
            picture_Release( p_pic );
        }
        else
        {
            vlc_mutex_lock( &p_sys->lock_out );
            p_sys->pp_pics[p_sys->i_last_pic++] = p_pic;
            p_sys->i_last_pic %= PICTURE_RING_SIZE;
            *out = p_sys->p_buffers;
            p_sys->p_buffers = NULL;
            if( p_pic2 != NULL )
            {
                p_sys->pp_pics[p_sys->i_last_pic++] = p_pic2;
                p_sys->i_last_pic %= PICTURE_RING_SIZE;
            }
            vlc_cond_signal( &p_sys->cond );
            vlc_mutex_unlock( &p_sys->lock_out );
        }
    }

    return VLC_SUCCESS;
}

static void* EncoderThread( vlc_object_t* p_this )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t*)p_this;
    sout_stream_id_t *id = p_sys->id_video;
    picture_t *p_pic;
    int canc = vlc_savecancel ();

    while( vlc_object_alive (p_sys) && !p_sys->b_error )
    {
        block_t *p_block;

        vlc_mutex_lock( &p_sys->lock_out );
        while( p_sys->i_last_pic == p_sys->i_first_pic )
        {
            vlc_cond_wait( &p_sys->cond, &p_sys->lock_out );
            if( !vlc_object_alive (p_sys) || p_sys->b_error ) break;
        }
        if( !vlc_object_alive (p_sys) || p_sys->b_error )
        {
            vlc_mutex_unlock( &p_sys->lock_out );
            break;
        }

        p_pic = p_sys->pp_pics[p_sys->i_first_pic++];
        p_sys->i_first_pic %= PICTURE_RING_SIZE;
        vlc_mutex_unlock( &p_sys->lock_out );

        video_timer_start( id->p_encoder );
        p_block = id->p_encoder->pf_encode_video( id->p_encoder, p_pic );
        video_timer_stop( id->p_encoder );

        vlc_mutex_lock( &p_sys->lock_out );
        block_ChainAppend( &p_sys->p_buffers, p_block );

        vlc_mutex_unlock( &p_sys->lock_out );
        picture_Release( p_pic );
    }

    while( p_sys->i_last_pic != p_sys->i_first_pic )
    {
        p_pic = p_sys->pp_pics[p_sys->i_first_pic++];
        p_sys->i_first_pic %= PICTURE_RING_SIZE;
        picture_Release( p_pic );
    }
    block_ChainRelease( p_sys->p_buffers );

    vlc_restorecancel (canc);
    return NULL;
}

static picture_t *video_new_buffer_decoder( decoder_t *p_dec )
{
    sout_stream_sys_t *p_ssys = p_dec->p_owner->p_sys;
    if( p_ssys->i_threads >= 1 )
    {
        int i_first_pic = p_ssys->i_first_pic;

        if( p_ssys->i_first_pic != p_ssys->i_last_pic )
        {
            /* Encoder still has stuff to encode, wait to clear-up the list */
            while( p_ssys->i_first_pic == i_first_pic )
            {
#warning THERE IS A DEFINITELY BUG! LOCKING IS INSUFFICIENT!
                msleep( 10000 );
                barrier ();
            }
        }
    }

    p_dec->fmt_out.video.i_chroma = p_dec->fmt_out.i_codec;
    return picture_New( p_dec->fmt_out.video.i_chroma,
                        p_dec->fmt_out.video.i_width,
                        p_dec->fmt_out.video.i_height,
                        p_dec->fmt_out.video.i_aspect );
}

static void video_del_buffer_decoder( decoder_t *p_decoder, picture_t *p_pic )
{
    VLC_UNUSED(p_decoder);
    picture_Release( p_pic );
}

static void video_link_picture_decoder( decoder_t *p_dec, picture_t *p_pic )
{
    VLC_UNUSED(p_dec);
    picture_Hold( p_pic );
}

static void video_unlink_picture_decoder( decoder_t *p_dec, picture_t *p_pic )
{
    VLC_UNUSED(p_dec);
    picture_Release( p_pic );
}

/*
 * SPU
 */
static subpicture_t *spu_new_buffer( decoder_t * );
static void spu_del_buffer( decoder_t *, subpicture_t * );

static int transcode_spu_new( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /*
     * Open decoder
     */

    /* Initialization of decoder structures */
    id->p_decoder->pf_decode_sub = NULL;
    id->p_decoder->pf_spu_buffer_new = spu_new_buffer;
    id->p_decoder->pf_spu_buffer_del = spu_del_buffer;
    id->p_decoder->p_owner = (decoder_owner_sys_t *)p_stream;
    /* id->p_decoder->p_cfg = p_sys->p_spu_cfg; */

    id->p_decoder->p_module =
        module_need( id->p_decoder, "decoder", "$codec", false );

    if( !id->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find spu decoder" );
        return VLC_EGENERIC;
    }

    if( !p_sys->b_soverlay )
    {
        /* Open encoder */
        /* Initialization of encoder format structures */
        es_format_Init( &id->p_encoder->fmt_in, id->p_decoder->fmt_in.i_cat,
                        id->p_decoder->fmt_in.i_codec );

        id->p_encoder->p_cfg = p_sys->p_spu_cfg;

        id->p_encoder->p_module =
            module_need( id->p_encoder, "encoder", p_sys->psz_senc, true );

        if( !id->p_encoder->p_module )
        {
            module_unneed( id->p_decoder, id->p_decoder->p_module );
            msg_Err( p_stream, "cannot find spu encoder (%s)", p_sys->psz_senc );
            return VLC_EGENERIC;
        }
    }

    if( !p_sys->p_spu )
    {
        p_sys->p_spu = spu_Create( p_stream );
        spu_Init( p_sys->p_spu );
    }

    return VLC_SUCCESS;
}

static void transcode_spu_close( sout_stream_id_t *id)
{
    /* Close decoder */
    if( id->p_decoder->p_module )
        module_unneed( id->p_decoder, id->p_decoder->p_module );
    if( id->p_decoder->p_description )
        vlc_meta_Delete( id->p_decoder->p_description );

    /* Close encoder */
    if( id->p_encoder->p_module )
        module_unneed( id->p_encoder, id->p_encoder->p_module );
}

static int transcode_spu_process( sout_stream_t *p_stream,
                                  sout_stream_id_t *id,
                                  block_t *in, block_t **out )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    subpicture_t *p_subpic;
    *out = NULL;

    p_subpic = id->p_decoder->pf_decode_sub( id->p_decoder, &in );
    if( !p_subpic )
        return VLC_EGENERIC;

    sout_UpdateStatistic( p_stream->p_sout, SOUT_STATISTIC_DECODED_SUBTITLE, 1 );

    if( p_sys->b_master_sync && p_sys->i_master_drift )
    {
        p_subpic->i_start -= p_sys->i_master_drift;
        if( p_subpic->i_stop ) p_subpic->i_stop -= p_sys->i_master_drift;
    }

    if( p_sys->b_soverlay )
    {
        spu_DisplaySubpicture( p_sys->p_spu, p_subpic );
    }
    else
    {
        block_t *p_block;

        p_block = id->p_encoder->pf_encode_sub( id->p_encoder, p_subpic );
        spu_del_buffer( id->p_decoder, p_subpic );
        if( p_block )
        {
            block_ChainAppend( out, p_block );
            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
}

static subpicture_t *spu_new_buffer( decoder_t *p_dec )
{
    VLC_UNUSED( p_dec );
    return subpicture_New();
}

static void spu_del_buffer( decoder_t *p_dec, subpicture_t *p_subpic )
{
    VLC_UNUSED( p_dec );
    subpicture_Delete( p_subpic );
}

/*
 * OSD menu
 */
static int transcode_osd_new( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    id->p_decoder->fmt_in.i_cat = SPU_ES;
    id->p_encoder->fmt_out.psz_language = strdup( "osd" );

    if( p_sys->i_osdcodec != 0 || p_sys->psz_osdenc )
    {
        msg_Dbg( p_stream, "creating osdmenu transcoding from fcc=`%4.4s' "
                 "to fcc=`%4.4s'", (char*)&id->p_encoder->fmt_out.i_codec,
                 (char*)&p_sys->i_osdcodec );

        /* Complete destination format */
        id->p_encoder->fmt_out.i_codec = p_sys->i_osdcodec;

        /* Open encoder */
        es_format_Init( &id->p_encoder->fmt_in, id->p_decoder->fmt_in.i_cat,
                        VLC_CODEC_YUVA );
        id->p_encoder->fmt_in.psz_language = strdup( "osd" );

        id->p_encoder->p_cfg = p_sys->p_osd_cfg;

        id->p_encoder->p_module =
            module_need( id->p_encoder, "encoder", p_sys->psz_osdenc, true );

        if( !id->p_encoder->p_module )
        {
            msg_Err( p_stream, "cannot find spu encoder (%s)", p_sys->psz_osdenc );
            goto error;
        }

        /* open output stream */
        id->id = sout_StreamIdAdd( p_sys->p_out, &id->p_encoder->fmt_out );
        id->b_transcode = true;

        if( !id->id ) goto error;
    }
    else
    {
        msg_Dbg( p_stream, "not transcoding a stream (fcc=`%4.4s')",
                 (char*)&id->p_decoder->fmt_out.i_codec );
        id->id = sout_StreamIdAdd( p_sys->p_out, &id->p_decoder->fmt_out );
        id->b_transcode = false;

        if( !id->id ) goto error;
    }

    if( !p_sys->p_spu )
    {
        p_sys->p_spu = spu_Create( p_stream );
        spu_Init( p_sys->p_spu );
    }

    return VLC_SUCCESS;

 error:
    msg_Err( p_stream, "starting osd encoding thread failed" );
    if( id->p_encoder->p_module )
            module_unneed( id->p_encoder, id->p_encoder->p_module );
    p_sys->b_osd = false;
    return VLC_EGENERIC;
}

static void transcode_osd_close( sout_stream_t *p_stream, sout_stream_id_t *id)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* Close encoder */
    if( id )
    {
        if( id->p_encoder->p_module )
            module_unneed( id->p_encoder, id->p_encoder->p_module );
    }
    p_sys->b_osd = false;
}

static int transcode_osd_process( sout_stream_t *p_stream,
                                  sout_stream_id_t *id,
                                  block_t *in, block_t **out )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    subpicture_t *p_subpic = NULL;

    /* Check if we have a subpicture to send */
    if( p_sys->p_spu && in->i_dts > 0)
    {
        p_subpic = spu_SortSubpictures( p_sys->p_spu, in->i_dts, false );
    }
    else
    {
        msg_Warn( p_stream, "spu channel not initialized, doing it now" );
        if( !p_sys->p_spu )
        {
            p_sys->p_spu = spu_Create( p_stream );
            spu_Init( p_sys->p_spu );
        }
    }

    if( p_subpic )
    {
        block_t *p_block = NULL;

        if( p_sys->b_master_sync && p_sys->i_master_drift )
        {
            p_subpic->i_start -= p_sys->i_master_drift;
            if( p_subpic->i_stop ) p_subpic->i_stop -= p_sys->i_master_drift;
        }

        p_block = id->p_encoder->pf_encode_sub( id->p_encoder, p_subpic );
        subpicture_Delete( p_subpic );
        if( p_block )
        {
            p_block->i_dts = p_block->i_pts = in->i_dts;
            block_ChainAppend( out, p_block );
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}
