/*****************************************************************************
 * transcode.c: transcoding stream output module
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
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

#include <vlc_osd.h>

#include "transcode.h"

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
#define ALANG_TEXT N_("Audio Language")
#define ALANG_LONGTEXT N_( \
    "This is the language of the audio stream.")
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
    add_float( SOUT_CFG_PREFIX "scale", 0, NULL, SCALE_TEXT,
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
    add_string( SOUT_CFG_PREFIX "alang", NULL, NULL, ALANG_TEXT,
                ALANG_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "channels", 0, NULL, ACHANS_TEXT,
                 ACHANS_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX "samplerate", 0, NULL, ARATE_TEXT,
                 ARATE_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "audio-sync", false, NULL, ASYNC_TEXT,
              ASYNC_LONGTEXT, false )
    add_module_list( SOUT_CFG_PREFIX "afilter",  "audio filter",
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
    "deinterlace-module", "threads", "hurry-up", "aenc", "acodec", "ab", "alang",
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

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    vlc_value_t       val;

    p_sys = vlc_object_create( p_this, sizeof( sout_stream_sys_t ) );

    if( !p_stream->p_next )
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

    p_sys->psz_alang = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "alang" );

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
        p_sys->psz_af = val.psz_string;
    else
    {
        free( val.psz_string );
        p_sys->psz_af = NULL;
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

    free( p_sys->psz_af );

    config_ChainDestroy( p_sys->p_audio_cfg );
    free( p_sys->psz_aenc );
    free( p_sys->psz_alang );

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

    if( p_sys->psz_alang )
        id->p_encoder->fmt_out.psz_language = strdup( p_sys->psz_alang );
    else if( p_fmt->psz_language )
        id->p_encoder->fmt_out.psz_language = strdup( p_fmt->psz_language );

    bool success;

    if( p_fmt->i_cat == AUDIO_ES && (p_sys->i_acodec || p_sys->psz_aenc) )
        success = transcode_audio_add(p_stream, p_fmt, id);
    else if( p_fmt->i_cat == VIDEO_ES && (p_sys->i_vcodec || p_sys->psz_venc) )
        success = transcode_video_add(p_stream, p_fmt, id);
    else if( ( p_fmt->i_cat == SPU_ES ) &&
             ( p_sys->i_scodec || p_sys->psz_senc || p_sys->b_soverlay ) )
        success = transcode_spu_add(p_stream, p_fmt, id);
    else if( !p_sys->b_osd && (p_sys->i_osdcodec != 0 || p_sys->psz_osdenc) )
        success = transcode_osd_add(p_stream, p_fmt, id);
    else
    {
        msg_Dbg( p_stream, "not transcoding a stream (fcc=`%4.4s')",
                 (char*)&p_fmt->i_codec );
        id->id = sout_StreamIdAdd( p_stream->p_next, p_fmt );
        id->b_transcode = false;

        success = id->id;
    }

    if(!success)
        goto error;

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

    if( id->id ) sout_StreamIdDel( p_stream->p_next, id->id );

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
            return sout_StreamIdSend( p_stream->p_next, id->id, p_buffer );

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
        return sout_StreamIdSend( p_stream->p_next, id->id, p_out );
    return VLC_SUCCESS;
}
