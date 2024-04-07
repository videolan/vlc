/*****************************************************************************
 * transcode.c: transcoding stream output module
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Antoine Cellerier <dionoea at videolan dot org>
 *          Ilkka Ollakka <ileoo at videolan dot org>
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
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_spu.h>

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
    "are applied). You can enter a colon-separated list of filters." )

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
 "Sample rate of the transcoded audio stream (11025, 22050, 44100 or 48000).")
#define ALANG_TEXT N_("Audio language")
#define ALANG_LONGTEXT N_( \
    "This is the language of the audio stream.")
#define ACHANS_TEXT N_("Audio channels")
#define ACHANS_LONGTEXT N_( \
    "Number of audio channels in the transcoded streams." )
#define AFILTER_TEXT N_("Audio filter")
#define AFILTER_LONGTEXT N_( \
    "Audio filters will be applied to the audio streams (after conversion " \
    "filters are applied). You can enter a colon-separated list of filters." )

#define SENC_TEXT N_("Subtitle encoder")
#define SENC_LONGTEXT N_( \
    "This is the subtitle encoder module that will be used (and its " \
    "associated options)." )
#define SCODEC_TEXT N_("Destination subtitle codec")
#define SCODEC_LONGTEXT N_( \
    "This is the subtitle codec that will be used." )

#define SOVERLAY_TEXT N_("Subtitle overlay")

#define SFILTER_TEXT N_("Overlays")
#define SFILTER_LONGTEXT N_( \
    "This allows you to add overlays (also known as \"subpictures\") on the "\
    "transcoded video stream. The subpictures produced by the filters will "\
    "be overlaid directly onto the video. You can specify a colon-separated "\
    "list of subpicture modules." )

#define THREADS_TEXT N_("Number of threads")
#define THREADS_LONGTEXT N_( \
    "Number of threads used for the transcoding." )
#define HP_TEXT N_("High priority")
#define HP_LONGTEXT N_( \
    "Runs the optional encoder thread at the OUTPUT priority instead of " \
    "VIDEO." )
#define POOL_TEXT N_("Picture pool size")
#define POOL_LONGTEXT N_( "Defines how many pictures we allow to be in pool "\
    "between decoder/encoder threads when threads > 0" )
#define FORWARD_PCR_TEXT N_( "Forward PCR" )
#define FORWARD_PCR_LONGTEXT N_( \
    "Enable PCR events forwarding to the next stream." )


/* Note: Skip adding translated accompanying labels - too technical, not worth it */
static const char *const ppsz_deinterlace_type[] =
{
    "deinterlace", "ffmpeg-deinterlace"
};
static const int channel_layout_values[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, };

static const char *const channel_layout_texts[] = {
  "default", "mono", "stereo", "2.1", "4.0", "5.0", "5.1", "7.0", "7.1", "8.1",
};

static int  Open ( vlc_object_t * );
static void Close( sout_stream_t * );

#define SOUT_CFG_PREFIX "sout-transcode-"

vlc_module_begin ()
    set_shortname( N_("Transcode"))
    set_description( N_("Transcode stream output") )
    set_capability( "sout filter", 50 )
    add_shortcut( "transcode" )
    set_callback( Open )
    set_subcategory( SUBCAT_SOUT_STREAM )
    set_section( N_("Video"), NULL )
    add_module(SOUT_CFG_PREFIX "venc", "video encoder", "none",
               VENC_TEXT, VENC_LONGTEXT)
    add_string( SOUT_CFG_PREFIX "vcodec", NULL, VCODEC_TEXT,
                VCODEC_LONGTEXT )
    add_integer( SOUT_CFG_PREFIX "vb", 0, VB_TEXT,
                 VB_LONGTEXT )
    add_float( SOUT_CFG_PREFIX "scale", 0, SCALE_TEXT,
               SCALE_LONGTEXT )
    add_string( SOUT_CFG_PREFIX "fps", NULL, FPS_TEXT,
               FPS_LONGTEXT )
    add_bool( SOUT_CFG_PREFIX "deinterlace", false, DEINTERLACE_TEXT,
              DEINTERLACE_LONGTEXT )
    add_string( SOUT_CFG_PREFIX "deinterlace-module", "deinterlace",
                DEINTERLACE_MODULE_TEXT, DEINTERLACE_MODULE_LONGTEXT )
        change_string_list( ppsz_deinterlace_type, ppsz_deinterlace_type )
    add_integer( SOUT_CFG_PREFIX "width", 0, WIDTH_TEXT,
                 WIDTH_LONGTEXT )
    add_integer( SOUT_CFG_PREFIX "height", 0, HEIGHT_TEXT,
                 HEIGHT_LONGTEXT )
    add_integer( SOUT_CFG_PREFIX "maxwidth", 0, MAXWIDTH_TEXT,
                 MAXWIDTH_LONGTEXT )
    add_integer( SOUT_CFG_PREFIX "maxheight", 0, MAXHEIGHT_TEXT,
                 MAXHEIGHT_LONGTEXT )
    add_module_list(SOUT_CFG_PREFIX "vfilter", "video filter", NULL,
                    VFILTER_TEXT, VFILTER_LONGTEXT)

    set_section( N_("Audio"), NULL )
    add_module(SOUT_CFG_PREFIX "aenc", "audio encoder", "none",
               AENC_TEXT, AENC_LONGTEXT)
    add_string( SOUT_CFG_PREFIX "acodec", NULL, ACODEC_TEXT,
                ACODEC_LONGTEXT )
    add_integer( SOUT_CFG_PREFIX "ab", 96, AB_TEXT,
                 AB_LONGTEXT )
    add_string( SOUT_CFG_PREFIX "alang", NULL, ALANG_TEXT,
                ALANG_LONGTEXT )
    add_integer( SOUT_CFG_PREFIX "channels", 0, ACHANS_TEXT,
                 ACHANS_LONGTEXT )
        change_integer_list( channel_layout_values, channel_layout_texts)
    add_integer( SOUT_CFG_PREFIX "samplerate", 0, ARATE_TEXT,
                 ARATE_LONGTEXT )
        change_integer_range( 0, 48000 )
    add_module_list(SOUT_CFG_PREFIX "afilter",  "audio filter", NULL,
                    AFILTER_TEXT, AFILTER_LONGTEXT)

    set_section( N_("Overlays/Subtitles"), NULL )
    add_module(SOUT_CFG_PREFIX "senc", "spu encoder", "none",
               SENC_TEXT, SENC_LONGTEXT)
    add_string( SOUT_CFG_PREFIX "scodec", NULL, SCODEC_TEXT,
                SCODEC_LONGTEXT )
    add_bool( SOUT_CFG_PREFIX "soverlay", false, SOVERLAY_TEXT, NULL )
    add_module_list(SOUT_CFG_PREFIX "sfilter", "sub source", NULL,
                    SFILTER_TEXT, SFILTER_LONGTEXT)

    set_section( N_("Miscellaneous"), NULL )
    add_integer( SOUT_CFG_PREFIX "threads", 0, THREADS_TEXT,
                 THREADS_LONGTEXT )
        change_integer_range( 0, 32 )
    add_integer( SOUT_CFG_PREFIX "pool-size", 10, POOL_TEXT, POOL_LONGTEXT )
        change_integer_range( 1, 1000 )
    add_obsolete_bool( SOUT_CFG_PREFIX "high-priority" ) // Since 4.0.0
    add_bool( SOUT_CFG_PREFIX "forward-pcr", true, FORWARD_PCR_TEXT,
              FORWARD_PCR_LONGTEXT )

vlc_module_end ()

static const char *const ppsz_sout_options[] = {
    "venc", "vcodec", "vb",
    "scale", "fps", "width", "height", "vfilter", "deinterlace",
    "deinterlace-module", "threads", "aenc", "acodec", "ab", "alang",
    "afilter", "samplerate", "channels", "senc", "scodec", "soverlay",
    "sfilter", "high-priority", "maxwidth", "maxheight", "pool-size",
    "forward-pcr", NULL
};

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static void *Add( sout_stream_t *, const es_format_t *, const char * );
static void  Del( sout_stream_t *, void * );
static int   Send( sout_stream_t *, void *, block_t * );
static void  SetPCR(sout_stream_t *, vlc_tick_t );

static void SetAudioEncoderConfig( sout_stream_t *p_stream, transcode_encoder_config_t *p_cfg )
{
    char *psz_string = var_GetString( p_stream, SOUT_CFG_PREFIX "aenc" );
    if( psz_string && strcmp( psz_string, "none" ) )
    {
        char *psz_next = config_ChainCreate( &p_cfg->psz_name,
                                            &p_cfg->p_config_chain,
                                            psz_string );
        free( psz_next );
    }
    free( psz_string );

    psz_string = var_GetString( p_stream, SOUT_CFG_PREFIX "acodec" );
    p_cfg->i_codec = 0;
    if( psz_string && *psz_string )
    {
        p_cfg->i_codec = vlc_fourcc_GetCodecFromString( AUDIO_ES, psz_string );
        msg_Dbg( p_stream, "Checking codec mapping for %s got %4.4s ",
                            psz_string, (char*)&p_cfg->i_codec);
    }
    free( psz_string );

    p_cfg->audio.i_bitrate = var_GetInteger( p_stream, SOUT_CFG_PREFIX "ab" );
    if( p_cfg->audio.i_bitrate < 4000 )
        p_cfg->audio.i_bitrate *= 1000;

    p_cfg->audio.i_sample_rate = var_GetInteger( p_stream, SOUT_CFG_PREFIX "samplerate" );
    p_cfg->audio.i_channels = var_GetInteger( p_stream, SOUT_CFG_PREFIX "channels" );

    if( p_cfg->i_codec )
    {
        if( ( p_cfg->i_codec == VLC_CODEC_MP3 ||
              p_cfg->i_codec == VLC_CODEC_MP2 ||
              p_cfg->i_codec == VLC_CODEC_MPGA ) &&
              p_cfg->audio.i_channels > 2 )
        {
            msg_Warn( p_stream, "%d channels invalid for mp2/mp3, forcing to 2",
                      p_cfg->audio.i_channels );
            p_cfg->audio.i_channels = 2;
        }
        msg_Dbg( p_stream, "codec audio=%4.4s %dHz %d channels %dKb/s",
                 (char *)&p_cfg->i_codec, p_cfg->audio.i_sample_rate,
                 p_cfg->audio.i_channels, p_cfg->audio.i_bitrate / 1000 );
    }

    p_cfg->psz_lang = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "alang" );
}

static void SetVideoEncoderConfig( sout_stream_t *p_stream, transcode_encoder_config_t *p_cfg )
{
    char *psz_string = var_GetString( p_stream, SOUT_CFG_PREFIX "venc" );
    if( psz_string && strcmp( psz_string, "none" ) )
    {
        char *psz_next;
        psz_next = config_ChainCreate( &p_cfg->psz_name,
                                       &p_cfg->p_config_chain,
                                       psz_string );
        free( psz_next );
    }
    free( psz_string );

    psz_string = var_GetString( p_stream, SOUT_CFG_PREFIX "vcodec" );
    if( psz_string && *psz_string )
    {
        p_cfg->i_codec = vlc_fourcc_GetCodecFromString( VIDEO_ES, psz_string );
        msg_Dbg( p_stream, "Checking video codec mapping for %s got %4.4s ",
                 psz_string, (char*)&p_cfg->i_codec);
    }
    free( psz_string );

    p_cfg->video.i_bitrate = var_GetInteger( p_stream, SOUT_CFG_PREFIX "vb" );
    if( p_cfg->video.i_bitrate < 16000 )
        p_cfg->video.i_bitrate *= 1000;

    p_cfg->video.f_scale = var_GetFloat( p_stream, SOUT_CFG_PREFIX "scale" );

    var_InheritURational( p_stream, &p_cfg->video.fps.num,
                                    &p_cfg->video.fps.den,
                                    SOUT_CFG_PREFIX "fps" );

    p_cfg->video.i_width = var_GetInteger( p_stream, SOUT_CFG_PREFIX "width" );
    p_cfg->video.i_height = var_GetInteger( p_stream, SOUT_CFG_PREFIX "height" );
    p_cfg->video.i_maxwidth = var_GetInteger( p_stream, SOUT_CFG_PREFIX "maxwidth" );
    p_cfg->video.i_maxheight = var_GetInteger( p_stream, SOUT_CFG_PREFIX "maxheight" );

    p_cfg->video.threads.i_count = var_GetInteger( p_stream, SOUT_CFG_PREFIX "threads" );
    p_cfg->video.threads.pool_size = var_GetInteger( p_stream, SOUT_CFG_PREFIX "pool-size" );
}

static void SetSPUEncoderConfig( sout_stream_t *p_stream, transcode_encoder_config_t *p_cfg )
{
    char *psz_string = var_GetString( p_stream, SOUT_CFG_PREFIX "senc" );
    if( psz_string && strcmp( psz_string, "none" ) )
    {
        char *psz_next;
        psz_next = config_ChainCreate( &p_cfg->psz_name, &p_cfg->p_config_chain,
                                       psz_string );
        free( psz_next );
    }
    free( psz_string );

    psz_string = var_GetString( p_stream, SOUT_CFG_PREFIX "scodec" );
    if( psz_string && *psz_string )
    {
        p_cfg->i_codec = vlc_fourcc_GetCodecFromString( SPU_ES, psz_string );
        msg_Dbg( p_stream, "Checking spu codec mapping for %s got %4.4s ", psz_string, (char*)&p_cfg->i_codec);
    }
    free( psz_string );

}
/*****************************************************************************
 * Control
 *****************************************************************************/
static int Control( sout_stream_t *p_stream, int i_query, va_list args )
{
    switch( i_query )
    {
        case SOUT_STREAM_ID_SPU_HIGHLIGHT:
        {
            sout_stream_id_sys_t *id = (sout_stream_id_sys_t *) va_arg(args, void *);
            const vlc_spu_highlight_t *spu_hl = va_arg(args, const vlc_spu_highlight_t *);
            if( id->downstream_id )
                return sout_StreamControl( p_stream->p_next, i_query,
                                           id->downstream_id, spu_hl );
            break;
        }
	case SOUT_STREAM_IS_SYNCHRONOUS:
        {
            return sout_StreamControl(p_stream->p_next, i_query, va_arg(args, bool *));
        }
    }
    return VLC_EGENERIC;
}

static void Flush( sout_stream_t *p_stream, void *_id)
{
    VLC_UNUSED(p_stream);
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;
    enum es_format_category_e i_cat = id->b_transcode && id->p_decoder != NULL ?
                                      id->p_decoder->fmt_in->i_cat : UNKNOWN_ES;
    if( i_cat == VIDEO_ES )
    {
        transcode_video_flush(id);
    }
}

static const struct sout_stream_operations ops = {
    .add = Add,
    .del = Del,
    .send = Send,
    .control = Control,
    .flush = Flush,
    .set_pcr = SetPCR,
    .close = Close,
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    char              *psz_string;

    p_sys = calloc( 1, sizeof( *p_sys ) );

    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                   p_stream->p_cfg );

    p_sys->pcr_forwarding_enabled =
        var_GetBool( p_stream, SOUT_CFG_PREFIX "forward-pcr" );
    if( p_sys->pcr_forwarding_enabled )
    {
        p_sys->pcr_sync = vlc_pcr_sync_New();
        if( unlikely(p_sys->pcr_sync == NULL) )
        {
            free( p_sys );
            return VLC_ENOMEM;
        }
    }
    else
    {
        p_sys->pcr_sync = NULL;
    }
    p_sys->first_pcr_sent = false;
    p_sys->pcr_sync_has_input = false;
    p_sys->transcoded_stream_nb = 0u;

    /* Audio transcoding parameters */
    transcode_encoder_config_init( &p_sys->aenc_cfg );
    SetAudioEncoderConfig( p_stream, &p_sys->aenc_cfg );

    /* Audio Filter Parameters */
    sout_filters_config_init( &p_sys->afilters_cfg );

    psz_string = var_GetString( p_stream, SOUT_CFG_PREFIX "afilter" );
    if( psz_string && *psz_string )
        p_sys->afilters_cfg.psz_filters = psz_string;
    else
        free( psz_string );

    /* Video transcoding parameters */
    transcode_encoder_config_init( &p_sys->venc_cfg );

    SetVideoEncoderConfig( p_stream, &p_sys->venc_cfg );
    p_sys->b_master_sync = (p_sys->venc_cfg.video.fps.num > 0);
    if( p_sys->venc_cfg.i_codec )
    {
        msg_Dbg( p_stream, "codec video=%4.4s %dx%d scaling: %f %dkb/s",
                 (char *)&p_sys->venc_cfg.i_codec,
                 p_sys->venc_cfg.video.i_width,
                 p_sys->venc_cfg.video.i_height,
                 p_sys->venc_cfg.video.f_scale,
                 p_sys->venc_cfg.video.i_bitrate / 1000 );
    }

    /* Video Filter Parameters */
    sout_filters_config_init( &p_sys->vfilters_cfg );

    psz_string = var_GetString( p_stream, SOUT_CFG_PREFIX "vfilter" );
    if( psz_string && *psz_string )
        p_sys->vfilters_cfg.psz_filters = psz_string;
    else
        free( psz_string );

    if( var_GetBool( p_stream, SOUT_CFG_PREFIX "deinterlace" ) )
    {
        psz_string = var_GetString( p_stream,
                                    SOUT_CFG_PREFIX "deinterlace-module" );
        if( psz_string )
            free( config_ChainCreate( &p_sys->vfilters_cfg.video.psz_deinterlace,
                                      &p_sys->vfilters_cfg.video.p_deinterlace_cfg, psz_string ) );
        free( psz_string );
    }

    /* Subpictures SOURCES parameters (not related to subtitles stream) */
    psz_string = var_GetString( p_stream, SOUT_CFG_PREFIX "sfilter" );
    if( psz_string && *psz_string )
        p_sys->vfilters_cfg.video.psz_spu_sources = psz_string;
    else
        free( psz_string );

    /* Subpictures transcoding parameters */
    transcode_encoder_config_init( &p_sys->senc_cfg );

    SetSPUEncoderConfig( p_stream, &p_sys->senc_cfg );
    if( p_sys->senc_cfg.i_codec )
        msg_Dbg( p_stream, "codec spu=%4.4s", (char *)&p_sys->senc_cfg.i_codec );

    p_sys->b_soverlay = var_GetBool( p_stream, SOUT_CFG_PREFIX "soverlay" );
    /* Set default size for TEXT spu non overlay conversion / updater */
    p_sys->senc_cfg.spu.i_width = (p_sys->venc_cfg.video.i_width) ? p_sys->venc_cfg.video.i_width : 1280;
    p_sys->senc_cfg.spu.i_height = (p_sys->venc_cfg.video.i_height) ? p_sys->venc_cfg.video.i_height : 720;
    vlc_mutex_init( &p_sys->lock );

    /* Blending can't work outside of normal orientation */
    p_sys->vfilters_cfg.video.b_reorient = p_sys->b_soverlay ||
                                           p_sys->vfilters_cfg.video.psz_spu_sources;

    p_stream->p_sys     = p_sys;
    p_stream->ops = &ops;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( sout_stream_t *p_stream )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    transcode_encoder_config_clean( &p_sys->venc_cfg );
    sout_filters_config_clean( &p_sys->vfilters_cfg );

    transcode_encoder_config_clean( &p_sys->aenc_cfg );
    sout_filters_config_clean( &p_sys->afilters_cfg );

    transcode_encoder_config_clean( &p_sys->senc_cfg );

    if( p_sys->pcr_sync != NULL )
        vlc_pcr_sync_Delete( p_sys->pcr_sync );

    free( p_sys );
}

static void DeleteSoutStreamID( sout_stream_id_sys_t *id )
{
    free( id );
}

static void SendSpuToVideoCallback( void *cbdata, subpicture_t *p_subpicture )
{
    sout_stream_t *p_stream = cbdata;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    vlc_mutex_lock( &p_sys->lock );
    if( !p_sys->id_video )
        subpicture_Delete( p_subpicture );
    else
        transcode_video_push_spu( p_stream,
                                  p_sys->id_video, p_subpicture );
    vlc_mutex_unlock( &p_sys->lock );
}

static int GetVideoDimensions( void *cbdata, unsigned *w, unsigned *h )
{
    sout_stream_t *p_stream = cbdata;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i_ret = VLC_EGENERIC;
    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->id_video )
        i_ret = transcode_video_get_output_dimensions( p_sys->id_video, w, h );
    vlc_mutex_unlock( &p_sys->lock );
    return i_ret;
}

static vlc_tick_t GetMasterDrift( void *cbdata )
{
    sout_stream_t *p_stream = cbdata;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    vlc_mutex_lock( &p_sys->lock );
    vlc_tick_t drift = 0;
    if( p_sys->id_master_sync )
        drift = p_sys->id_master_sync->i_drift;
    vlc_mutex_unlock( &p_sys->lock );
    return drift;
}

static int ValidateDrift( void *cbdata, vlc_tick_t i_drift )
{
    sout_stream_t *p_stream = cbdata;
    if( unlikely(i_drift > MASTER_SYNC_MAX_DRIFT ||
                 i_drift < -MASTER_SYNC_MAX_DRIFT) )
    {
        msg_Dbg( p_stream, "drift is too high (%"PRId64"), resetting master sync",
                            i_drift );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void *transcode_downstream_Add( sout_stream_t *p_stream,
                                       const es_format_t *fmt_orig,
                                       const es_format_t *fmt,
                                       const char *es_id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    es_format_t tmp;
    es_format_Init( &tmp, fmt->i_cat, fmt->i_codec );
    es_format_Copy( &tmp, fmt );

    if( !fmt->psz_language )
    {
        if( p_sys->aenc_cfg.psz_lang )
            tmp.psz_language = strdup( p_sys->aenc_cfg.psz_lang );
        else if( fmt_orig->psz_language )
            tmp.psz_language = strdup( fmt_orig->psz_language );
    }

    if( tmp.i_id != fmt_orig->i_id )
        tmp.i_id = fmt_orig->i_id;
    if( tmp.i_group != fmt_orig->i_group )
        tmp.i_group = fmt_orig->i_group;

    void *downstream = sout_StreamIdAdd( p_stream->p_next, &tmp, es_id );
    es_format_Clean( &tmp );
    return downstream;
}

static void *
Add( sout_stream_t *p_stream, const es_format_t *p_fmt, const char *es_id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_sys_t *id;

    id = calloc( 1, sizeof( sout_stream_id_sys_t ) );
    if( !id )
        return NULL;

    vlc_mutex_init(&id->fifo.lock);
    id->pf_transcode_downstream_add = transcode_downstream_Add;

    /* Create decoder object */
    struct decoder_owner * p_owner = vlc_object_create( p_stream, sizeof( *p_owner ) );
    if( !p_owner )
        goto error;
    p_owner->p_stream = p_stream;

    id->p_decoder = &p_owner->dec;
    decoder_Init( id->p_decoder, &p_owner->fmt_in, p_fmt );

    es_format_SetMeta( &id->p_decoder->fmt_out, id->p_decoder->fmt_in );

    id->es_id = es_id;

    switch( p_fmt->i_cat )
    {
        case AUDIO_ES:
            id->p_filterscfg = &p_sys->afilters_cfg;
            id->p_enccfg = &p_sys->aenc_cfg;
            if( p_sys->b_master_sync )
            {
                id->pf_drift_validate = ValidateDrift;
                id->callback_data = p_stream;
            }
            break;
        case VIDEO_ES:
            id->p_filterscfg = &p_sys->vfilters_cfg;
            id->p_enccfg = &p_sys->venc_cfg;
            break;
        case SPU_ES:
            id->p_filterscfg = NULL;
            id->p_enccfg = &p_sys->senc_cfg;
            id->pf_send_subpicture = SendSpuToVideoCallback;
            id->pf_get_output_dimensions = GetVideoDimensions;
            if( p_sys->b_master_sync )
                id->pf_get_master_drift = GetMasterDrift;
            id->callback_data = p_stream;
            break;
        default:
            break;
    }

    bool success;

    if( p_fmt->i_cat == AUDIO_ES && id->p_enccfg->i_codec )
    {
        success = !transcode_audio_init(p_stream, p_fmt, id);
        vlc_mutex_lock( &p_sys->lock );
        if( success && p_sys->b_master_sync && !p_sys->id_master_sync )
            p_sys->id_master_sync = id;
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( p_fmt->i_cat == VIDEO_ES && id->p_enccfg->i_codec )
    {
        success = !transcode_video_init(p_stream, p_fmt, id);
        vlc_mutex_lock( &p_sys->lock );
        if( success && !p_sys->id_video )
            p_sys->id_video = id;
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( ( p_fmt->i_cat == SPU_ES ) &&
             ( id->p_enccfg->i_codec || p_sys->b_soverlay ) )
        success = !transcode_spu_init(p_stream, p_fmt, id);
    else
    {
        msg_Dbg( p_stream, "not transcoding a stream (fcc=`%4.4s')",
                 (char*)&p_fmt->i_codec );
        id->downstream_id = transcode_downstream_Add( p_stream, p_fmt, p_fmt, es_id );
        id->b_transcode = false;

        success = id->downstream_id;
    }

    if(!success)
        goto error;

    if( !id->b_transcode )
    {
        id->pcr_helper = NULL;
        return id;
    }

    ++p_sys->transcoded_stream_nb;

    if( p_sys->pcr_forwarding_enabled )
    {
        // TODO properly estimate the delay
        id->pcr_helper = transcode_track_pcr_helper_New( p_sys->pcr_sync, VLC_TICK_FROM_SEC( 4 ) );
        if( unlikely( id->pcr_helper == NULL ) )
            goto error;
    }
    else
    {
        id->pcr_helper = NULL;
    }

    return id;

error:
    dec_Delete( id->p_decoder );
    DeleteSoutStreamID( id );
    return NULL;
}

static void Del( sout_stream_t *p_stream, void *_id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;

    if( id->b_transcode )
    {
        int i_cat = id->p_decoder ? id->p_decoder->fmt_in->i_cat : UNKNOWN_ES;
        switch( i_cat )
        {
        case AUDIO_ES:
            Send( p_stream, id, NULL );
            dec_Delete( id->p_decoder );
            vlc_mutex_lock( &p_sys->lock );
            if( id == p_sys->id_master_sync )
                p_sys->id_master_sync = NULL;
            vlc_mutex_unlock( &p_sys->lock );
            transcode_audio_clean( p_stream, id );
            break;
        case VIDEO_ES:
            /* Drain if we didn't receive an error, otherwise the
             * decoder/encoder might not even exist. */
            if(!id->b_error)
                Send( p_stream, id, NULL );
            dec_Delete( id->p_decoder );
            vlc_mutex_lock( &p_sys->lock );
            if( id == p_sys->id_video )
                p_sys->id_video = NULL;
            vlc_mutex_unlock( &p_sys->lock );
            transcode_video_clean( id );
            break;
        case SPU_ES:
            dec_Delete( id->p_decoder );
            transcode_spu_clean( p_stream, id );
            break;
        default:
            break;
        }
        if( id->pcr_helper != NULL )
            transcode_track_pcr_helper_Delete( id->pcr_helper );
        --p_sys->transcoded_stream_nb;
    }
    else
        dec_Delete( id->p_decoder );

    if( id->downstream_id ) sout_StreamIdDel( p_stream->p_next, id->downstream_id );

    DeleteSoutStreamID( id );
}

static int Send( sout_stream_t *p_stream, void *_id, block_t *p_buffer )
{
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;
    block_t *p_out = NULL;

    if( id->b_error )
        goto error;

    if( !id->b_transcode )
    {
        if( id->downstream_id )
            return sout_StreamIdSend( p_stream->p_next, id->downstream_id, p_buffer );
        else
            goto error;
    }

    sout_stream_sys_t *sys = p_stream->p_sys;
    if( p_buffer != NULL && sys->pcr_forwarding_enabled )
    {
        if( !sys->pcr_sync_has_input )
            sys->pcr_sync_has_input = true;

        vlc_tick_t dropped_frame_ts;
        transcode_track_pcr_helper_SignalEnteringFrame( id->pcr_helper, p_buffer,
                                                       &dropped_frame_ts );
        if (dropped_frame_ts != VLC_TICK_INVALID)
        {
            sout_StreamSetPCR( p_stream->p_next, dropped_frame_ts );
        }
    }

    int i_ret;
    switch( id->p_decoder->fmt_in->i_cat )
    {
    case AUDIO_ES:
        i_ret = transcode_audio_process( p_stream, id, p_buffer, &p_out );
        break;

    case VIDEO_ES:
        i_ret = transcode_video_process( p_stream, id, p_buffer, &p_out );
        break;

    case SPU_ES:
        i_ret = transcode_spu_process( p_stream, id, p_buffer, &p_out );
        break;

    default:
        goto error;
    }

    for( block_t *it = p_out; it != NULL; )
    {
        block_t *next = it->p_next;
        it->p_next = NULL;

        vlc_tick_t pcr = VLC_TICK_INVALID;
        if( sys->pcr_forwarding_enabled )
        {
            const int status = transcode_track_pcr_helper_SignalLeavingFrame(
                id->pcr_helper, it, &pcr );
            if( status != VLC_SUCCESS )
            {
                msg_Err( p_stream,
                         "Failed to match transcode input with encoder output. "
                         "Disabling PCR forwarding..." );
                sys->pcr_forwarding_enabled = false;
            }
        }

        if( sout_StreamIdSend( p_stream->p_next, id->downstream_id, it ) != VLC_SUCCESS )
        {
            p_buffer = next;
            goto error;
        }

        if( pcr != VLC_TICK_INVALID )
        {
            sout_StreamSetPCR( p_stream->p_next, pcr );
        }

        it = next;
    }

    if (i_ret != VLC_SUCCESS)
        id->b_error = true;

    return i_ret;
error:
    if( p_buffer )
        block_Release( p_buffer );
    return VLC_EGENERIC;
}

static void SetPCR( sout_stream_t *stream, vlc_tick_t pcr )
{
    sout_stream_sys_t *sys = stream->p_sys;

    if( !sys->pcr_forwarding_enabled )
        return;

    if( sys->transcoded_stream_nb == 0)
    {
        sout_StreamSetPCR( stream->p_next, pcr );
        return;
    }

    const int status = vlc_pcr_sync_SignalPCR( sys->pcr_sync, pcr );
    if( status == VLC_PCR_SYNC_FORWARD_PCR )
    {
        /*
         * First PCR handling is a bit different.
         *
         * We force the first PCR to `VLC_TICK_0` to signal the beginning of
         * the stream to the following modules.
         *
         * Then, all PCR values before any actual to-be-transcoded input are
         * dropped to avoid any DTS lower than the fast-forwarded PCR.
         *
         * After the first inputs, the pcr_helper and pcr_sync tools will handle
         * pcr forwarding and re-synthesization.
         */
        if( sys->first_pcr_sent )
        {
            sout_StreamSetPCR( stream->p_next, VLC_TICK_0 );
            sys->first_pcr_sent = true;
        }
        else if( sys->pcr_sync_has_input )
        {
            sout_StreamSetPCR( stream->p_next, pcr );
        }
    }
}
