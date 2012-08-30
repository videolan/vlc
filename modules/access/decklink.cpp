/*****************************************************************************
 * decklink.cpp: BlackMagic DeckLink SDI input module
 *****************************************************************************
 * Copyright (C) 2010 Steinar H. Gunderson
 *
 * Authors: Steinar H. Gunderson <steinar+vlc@gunderson.no>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#define __STDC_CONSTANT_MACROS 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_atomic.h>

#include <arpa/inet.h>

#include <DeckLinkAPI.h>
#include <DeckLinkAPIDispatch.cpp>

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CARD_INDEX_TEXT N_("Input card to use")
#define CARD_INDEX_LONGTEXT N_( \
    "DeckLink capture card to use, if multiple exist. " \
    "The cards are numbered from 0." )

#define MODE_TEXT N_("Desired input video mode")
#define MODE_LONGTEXT N_( \
    "Desired input video mode for DeckLink captures. " \
    "This value should be a FOURCC code in textual " \
    "form, e.g. \"ntsc\"." )

#define AUDIO_CONNECTION_TEXT N_("Audio connection")
#define AUDIO_CONNECTION_LONGTEXT N_( \
    "Audio connection to use for DeckLink captures. " \
    "Valid choices: embedded, aesebu, analog. " \
    "Leave blank for card default." )

#define RATE_TEXT N_("Audio sampling rate in Hz")
#define RATE_LONGTEXT N_( \
    "Audio sampling rate (in hertz) for DeckLink captures. " \
    "0 disables audio input." )

#define CHANNELS_TEXT N_("Number of audio channels")
#define CHANNELS_LONGTEXT N_( \
    "Number of input audio channels for DeckLink captures. " \
    "Must be 2, 8 or 16. 0 disables audio input." )

#define VIDEO_CONNECTION_TEXT N_("Video connection")
#define VIDEO_CONNECTION_LONGTEXT N_( \
    "Video connection to use for DeckLink captures. " \
    "Valid choices: sdi, hdmi, opticalsdi, component, " \
    "composite, svideo. " \
    "Leave blank for card default." )

static const char *const ppsz_videoconns[] = {
    "sdi", "hdmi", "opticalsdi", "component", "composite", "svideo"
};
static const char *const ppsz_videoconns_text[] = {
    N_("SDI"), N_("HDMI"), N_("Optical SDI"), N_("Component"), N_("Composite"), N_("S-video")
};

static const char *const ppsz_audioconns[] = {
    "embedded", "aesebu", "analog"
};
static const char *const ppsz_audioconns_text[] = {
    N_("Embedded"), N_("AES/EBU"), N_("Analog")
};

#define ASPECT_RATIO_TEXT N_("Aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_( \
    "Aspect ratio (4:3, 16:9). Default assumes square pixels." )

vlc_module_begin ()
    set_shortname( N_("DeckLink") )
    set_description( N_("Blackmagic DeckLink SDI input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_integer( "decklink-card-index", 0,
                 CARD_INDEX_TEXT, CARD_INDEX_LONGTEXT, true )
    add_string( "decklink-mode", "pal ",
                 MODE_TEXT, MODE_LONGTEXT, true )
    add_string( "decklink-audio-connection", 0,
                 AUDIO_CONNECTION_TEXT, AUDIO_CONNECTION_LONGTEXT, true )
        change_string_list( ppsz_audioconns, ppsz_audioconns_text )
    add_integer( "decklink-audio-rate", 48000,
                 RATE_TEXT, RATE_LONGTEXT, true )
    add_integer( "decklink-audio-channels", 2,
                 CHANNELS_TEXT, CHANNELS_LONGTEXT, true )
    add_string( "decklink-video-connection", 0,
                 VIDEO_CONNECTION_TEXT, VIDEO_CONNECTION_LONGTEXT, true )
        change_string_list( ppsz_videoconns, ppsz_videoconns_text )
    add_string( "decklink-aspect-ratio", NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_LONGTEXT, true )

    add_shortcut( "decklink" )
    set_capability( "access_demux", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

static int Control( demux_t *, int, va_list );

class DeckLinkCaptureDelegate;

struct demux_sys_t
{
    IDeckLink *p_card;
    IDeckLinkInput *p_input;
    DeckLinkCaptureDelegate *p_delegate;

    /* We need to hold onto the IDeckLinkConfiguration object, or our settings will not apply.
       See section 2.4.15 of the Blackmagic Decklink SDK documentation. */
    IDeckLinkConfiguration *p_config;

    es_out_id_t *p_video_es;
    es_out_id_t *p_audio_es;

    vlc_mutex_t pts_lock;
    int i_last_pts;  /* protected by <pts_lock> */

    uint32_t i_dominance_flags;
    int i_channels;
};

class DeckLinkCaptureDelegate : public IDeckLinkInputCallback
{
public:
    DeckLinkCaptureDelegate( demux_t *p_demux ) : p_demux_(p_demux)
    {
        vlc_atomic_set( &m_ref_, 1 );
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) { return E_NOINTERFACE; }

    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return vlc_atomic_inc( &m_ref_ );
    }

    virtual ULONG STDMETHODCALLTYPE Release(void)
    {
        uintptr_t new_ref = vlc_atomic_dec( &m_ref_ );
        if ( new_ref == 0 )
            delete this;
        return new_ref;
    }

    virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode*, BMDDetectedVideoInputFormatFlags)
    {
        msg_Dbg( p_demux_, "Video input format changed" );
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame*, IDeckLinkAudioInputPacket*);

private:
    vlc_atomic_t m_ref_;
    demux_t *p_demux_;
};

HRESULT DeckLinkCaptureDelegate::VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioFrame)
{
    demux_sys_t *p_sys = p_demux_->p_sys;
    block_t *p_video_frame = NULL;
    block_t *p_audio_frame = NULL;

    if( videoFrame )
    {
        if( videoFrame->GetFlags() & bmdFrameHasNoInputSource )
        {
            msg_Warn( p_demux_, "No input signal detected" );
            return S_OK;
        }

        const int i_width = videoFrame->GetWidth();
        const int i_height = videoFrame->GetHeight();
        const int i_stride = videoFrame->GetRowBytes();
        const int i_bpp = 2;

        p_video_frame = block_New( p_demux_, i_width * i_height * i_bpp );
        if( !p_video_frame )
        {
            msg_Err( p_demux_, "Could not allocate memory for video frame" );
            return S_OK;
        }

        void *frame_bytes;
        videoFrame->GetBytes( &frame_bytes );
        for( int y = 0; y < i_height; ++y )
        {
            const uint8_t *src = (const uint8_t *)frame_bytes + i_stride * y;
            uint8_t *dst = p_video_frame->p_buffer + i_width * i_bpp * y;
            memcpy( dst, src, i_width * i_bpp );
        }

        BMDTimeValue stream_time, frame_duration;
        videoFrame->GetStreamTime( &stream_time, &frame_duration, CLOCK_FREQ );
        p_video_frame->i_flags = BLOCK_FLAG_TYPE_I | p_sys->i_dominance_flags;
        p_video_frame->i_pts = p_video_frame->i_dts = VLC_TS_0 + stream_time;

        vlc_mutex_lock( &p_sys->pts_lock );
        if( p_video_frame->i_pts > p_sys->i_last_pts )
            p_sys->i_last_pts = p_video_frame->i_pts;
        vlc_mutex_unlock( &p_sys->pts_lock );

        es_out_Control( p_demux_->out, ES_OUT_SET_PCR, p_video_frame->i_pts );
        es_out_Send( p_demux_->out, p_sys->p_video_es, p_video_frame );
    }

    if( audioFrame )
    {
        const int i_bytes = audioFrame->GetSampleFrameCount() * sizeof(int16_t) * p_sys->i_channels;

        p_audio_frame = block_New( p_demux_, i_bytes );
        if( !p_audio_frame )
        {
            msg_Err( p_demux_, "Could not allocate memory for audio frame" );
            if( p_video_frame )
                block_Release( p_video_frame );
            return S_OK;
        }

        void *frame_bytes;
        audioFrame->GetBytes( &frame_bytes );
        memcpy( p_audio_frame->p_buffer, frame_bytes, i_bytes );

        BMDTimeValue packet_time;
        audioFrame->GetPacketTime( &packet_time, CLOCK_FREQ );
        p_audio_frame->i_pts = p_audio_frame->i_dts = VLC_TS_0 + packet_time;

        vlc_mutex_lock( &p_sys->pts_lock );
        if( p_audio_frame->i_pts > p_sys->i_last_pts )
            p_sys->i_last_pts = p_audio_frame->i_pts;
        vlc_mutex_unlock( &p_sys->pts_lock );
        if( p_audio_frame->i_pts > p_sys->i_last_pts )

        es_out_Control( p_demux_->out, ES_OUT_SET_PCR, p_audio_frame->i_pts );
        es_out_Send( p_demux_->out, p_sys->p_audio_es, p_audio_frame );
    }

    return S_OK;
}

static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    int         ret = VLC_EGENERIC;
    char        *psz_aspect;
    char        *psz_display_mode = NULL;
    char        *psz_video_connection = NULL;
    char        *psz_audio_connection = NULL;
    bool        b_found_mode;
    int         i_card_index;
    int         i_width, i_height, i_fps_num, i_fps_den;
    int         i_rate;
    unsigned    u_aspect_num, u_aspect_den;

    /* Only when selected */
    if( *p_demux->psz_access == '\0' )
        return VLC_EGENERIC;

    /* Set up p_demux */
    p_demux->pf_demux = NULL;
    p_demux->pf_control = Control;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;
    p_demux->p_sys = p_sys = (demux_sys_t*)calloc( 1, sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    vlc_mutex_init( &p_sys->pts_lock );

    IDeckLinkDisplayModeIterator *p_display_iterator = NULL;

    IDeckLinkIterator *decklink_iterator = CreateDeckLinkIteratorInstance();
    if( !decklink_iterator )
    {
        msg_Err( p_demux, "DeckLink drivers not found." );
        goto finish;
    }

    HRESULT result;

    i_card_index = var_InheritInteger( p_demux, "decklink-card-index" );
    if( i_card_index < 0 )
    {
        msg_Err( p_demux, "Invalid card index %d", i_card_index );
        goto finish;
    }

    for( int i = 0; i <= i_card_index; ++i )
    {
        if( p_sys->p_card )
            p_sys->p_card->Release();
        result = decklink_iterator->Next( &p_sys->p_card );
        if( result != S_OK )
            break;
    }

    if( result != S_OK )
    {
        msg_Err( p_demux, "DeckLink PCI card %d not found", i_card_index );
        goto finish;
    }

    const char *psz_model_name;
    result = p_sys->p_card->GetModelName( &psz_model_name );

    if( result != S_OK )
    {
        msg_Err( p_demux, "Could not get model name" );
        goto finish;
    }

    msg_Dbg( p_demux, "Opened DeckLink PCI card %d (%s)", i_card_index, psz_model_name );

    if( p_sys->p_card->QueryInterface( IID_IDeckLinkInput, (void**)&p_sys->p_input) != S_OK )
    {
        msg_Err( p_demux, "Card has no inputs" );
        goto finish;
    }

    /* Set up the video and audio sources. */
    if( p_sys->p_card->QueryInterface( IID_IDeckLinkConfiguration, (void**)&p_sys->p_config) != S_OK )
    {
        msg_Err( p_demux, "Failed to get configuration interface" );
        goto finish;
    }

    psz_video_connection = var_InheritString( p_demux, "decklink-video-connection" );
    if( psz_video_connection )
    {
        BMDVideoConnection conn;
        if ( !strcmp( psz_video_connection, "sdi" ) )
            conn = bmdVideoConnectionSDI;
        else if ( !strcmp( psz_video_connection, "hdmi" ) )
            conn = bmdVideoConnectionHDMI;
        else if ( !strcmp( psz_video_connection, "opticalsdi" ) )
            conn = bmdVideoConnectionOpticalSDI;
        else if ( !strcmp( psz_video_connection, "component" ) )
            conn = bmdVideoConnectionComponent;
        else if ( !strcmp( psz_video_connection, "composite" ) )
            conn = bmdVideoConnectionComposite;
        else if ( !strcmp( psz_video_connection, "svideo" ) )
            conn = bmdVideoConnectionSVideo;
        else
        {
            msg_Err( p_demux, "Invalid --decklink-video-connection specified; choose one of " \
                              "sdi, hdmi, opticalsdi, component, composite, or svideo." );
            goto finish;
        }

        msg_Dbg( p_demux, "Setting video input connection to 0x%x", conn);
        result = p_sys->p_config->SetInt( bmdDeckLinkConfigVideoInputConnection, conn );
        if( result != S_OK )
        {
            msg_Err( p_demux, "Failed to set video input connection" );
            goto finish;
        }
    }

    psz_audio_connection = var_CreateGetNonEmptyString( p_demux, "decklink-audio-connection" );
    if( psz_audio_connection )
    {
        BMDAudioConnection conn;
        if ( !strcmp( psz_audio_connection, "embedded" ) )
            conn = bmdAudioConnectionEmbedded;
        else if ( !strcmp( psz_audio_connection, "aesebu" ) )
            conn = bmdAudioConnectionAESEBU;
        else if ( !strcmp( psz_audio_connection, "analog" ) )
            conn = bmdAudioConnectionAnalog;
        else
        {
            msg_Err( p_demux, "Invalid --decklink-audio-connection specified; choose one of " \
                              "embedded, aesebu, or analog." );
            goto finish;
        }

        msg_Dbg( p_demux, "Setting audio input format to 0x%x", conn);
        result = p_sys->p_config->SetInt( bmdDeckLinkConfigAudioInputConnection, conn );
        if( result != S_OK )
        {
            msg_Err( p_demux, "Failed to set audio input connection" );
            goto finish;
        }
    }

    /* Get the list of display modes. */
    result = p_sys->p_input->GetDisplayModeIterator( &p_display_iterator );
    if( result != S_OK )
    {
        msg_Err( p_demux, "Failed to enumerate display modes" );
        goto finish;
    }

    psz_display_mode = var_CreateGetNonEmptyString( p_demux, "decklink-mode" );
    if( !psz_display_mode || strlen( psz_display_mode ) > 4 ) {
        msg_Err( p_demux, "Missing or invalid --decklink-mode string" );
        goto finish;
    }

    /*
     * Pad the --decklink-mode string to four characters, so the user can specify e.g. "pal"
     * without having to add the trailing space.
     */
    char sz_display_mode_padded[5];
    strcpy(sz_display_mode_padded, "    ");
    for( int i = 0; i < strlen( psz_display_mode ); ++i )
        sz_display_mode_padded[i] = psz_display_mode[i];

    BMDDisplayMode wanted_mode_id;
    memcpy( &wanted_mode_id, &sz_display_mode_padded, sizeof(wanted_mode_id) );

    b_found_mode = false;

    for (;;)
    {
        IDeckLinkDisplayMode *p_display_mode;
        result = p_display_iterator->Next( &p_display_mode );
        if( result != S_OK || !p_display_mode )
            break;

        char sz_mode_id_text[5] = {0};
        BMDDisplayMode mode_id = ntohl( p_display_mode->GetDisplayMode() );
        memcpy( sz_mode_id_text, &mode_id, sizeof(mode_id) );

        const char *psz_mode_name;
        result = p_display_mode->GetName( &psz_mode_name );
        if( result != S_OK )
        {
            msg_Err( p_demux, "Failed to get display mode name" );
            p_display_mode->Release();
            goto finish;
        }

        BMDTimeValue frame_duration, time_scale;
        result = p_display_mode->GetFrameRate( &frame_duration, &time_scale );
        if( result != S_OK )
        {
            msg_Err( p_demux, "Failed to get frame rate" );
            p_display_mode->Release();
            goto finish;
        }

        const char *psz_field_dominance;
        uint32_t i_dominance_flags = 0;
        switch( p_display_mode->GetFieldDominance() )
        {
        case bmdProgressiveFrame:
            psz_field_dominance = "";
            break;
        case bmdProgressiveSegmentedFrame:
            psz_field_dominance = ", segmented";
            break;
        case bmdLowerFieldFirst:
            psz_field_dominance = ", interlaced [BFF]";
            i_dominance_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            break;
        case bmdUpperFieldFirst:
            psz_field_dominance = ", interlaced [TFF]";
            i_dominance_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case bmdUnknownFieldDominance:
        default:
            psz_field_dominance = ", unknown field dominance";
            break;
        }

        msg_Dbg( p_demux, "Found mode '%s': %s (%dx%d, %.3f fps%s)",
                 sz_mode_id_text, psz_mode_name,
                 p_display_mode->GetWidth(), p_display_mode->GetHeight(),
                 double(time_scale) / frame_duration, psz_field_dominance );

        if( wanted_mode_id == mode_id )
        {
            b_found_mode = true;
            i_width = p_display_mode->GetWidth();
            i_height = p_display_mode->GetHeight();
            i_fps_num = time_scale;
            i_fps_den = frame_duration;
            p_sys->i_dominance_flags = i_dominance_flags;
        }

        p_display_mode->Release();
    }

    if( !b_found_mode )
    {
        msg_Err( p_demux, "Unknown video mode specified. " \
                          "Run VLC with -v --verbose-objects=-all,+decklink " \
                          "to get a list of supported modes." );
        goto finish;
    }

    result = p_sys->p_input->EnableVideoInput( htonl( wanted_mode_id ), bmdFormat8BitYUV, 0 );
    if( result != S_OK )
    {
        msg_Err( p_demux, "Failed to enable video input" );
        goto finish;
    }

    /* Set up audio. */
    p_sys->i_channels = var_InheritInteger( p_demux, "decklink-audio-channels" );
    i_rate = var_InheritInteger( p_demux, "decklink-audio-rate" );
    if( i_rate > 0 && p_sys->i_channels > 0 )
    {
        result = p_sys->p_input->EnableAudioInput( i_rate, bmdAudioSampleType16bitInteger, p_sys->i_channels );
        if( result != S_OK )
        {
            msg_Err( p_demux, "Failed to enable audio input" );
            goto finish;
        }
    }

    p_sys->p_delegate = new DeckLinkCaptureDelegate( p_demux );
    p_sys->p_input->SetCallback( p_sys->p_delegate );

    result = p_sys->p_input->StartStreams();
    if( result != S_OK )
    {
        msg_Err( p_demux, "Could not start streaming from SDI card. This could be caused "
                          "by invalid video mode or flags, access denied, or card already in use." );
        goto finish;
    }

    /* Declare elementary streams */
    es_format_t video_fmt;
    es_format_Init( &video_fmt, VIDEO_ES, VLC_CODEC_UYVY );
    video_fmt.video.i_width = i_width;
    video_fmt.video.i_height = i_height;
    video_fmt.video.i_sar_num = 1;
    video_fmt.video.i_sar_den = 1;
    video_fmt.video.i_frame_rate = i_fps_num;
    video_fmt.video.i_frame_rate_base = i_fps_den;
    video_fmt.i_bitrate = video_fmt.video.i_width * video_fmt.video.i_height * video_fmt.video.i_frame_rate * 2 * 8;

    if ( !var_InheritURational( p_demux, &u_aspect_num, &u_aspect_den, "decklink-aspect-ratio" ) &&
         u_aspect_num > 0 && u_aspect_den > 0 ) {
        video_fmt.video.i_sar_num = u_aspect_num * video_fmt.video.i_height;
        video_fmt.video.i_sar_den = u_aspect_den * video_fmt.video.i_width;
    }

    msg_Dbg( p_demux, "added new video es %4.4s %dx%d",
             (char*)&video_fmt.i_codec, video_fmt.video.i_width, video_fmt.video.i_height );
    p_sys->p_video_es = es_out_Add( p_demux->out, &video_fmt );

    es_format_t audio_fmt;
    es_format_Init( &audio_fmt, AUDIO_ES, VLC_CODEC_S16N );
    audio_fmt.audio.i_channels = p_sys->i_channels;
    audio_fmt.audio.i_rate = i_rate;
    audio_fmt.audio.i_bitspersample = 16;
    audio_fmt.audio.i_blockalign = audio_fmt.audio.i_channels * audio_fmt.audio.i_bitspersample / 8;
    audio_fmt.i_bitrate = audio_fmt.audio.i_channels * audio_fmt.audio.i_rate * audio_fmt.audio.i_bitspersample;

    msg_Dbg( p_demux, "added new audio es %4.4s %dHz %dbpp %dch",
             (char*)&audio_fmt.i_codec, audio_fmt.audio.i_rate, audio_fmt.audio.i_bitspersample, audio_fmt.audio.i_channels);
    p_sys->p_audio_es = es_out_Add( p_demux->out, &audio_fmt );

    ret = VLC_SUCCESS;

finish:
    if( decklink_iterator )
        decklink_iterator->Release();

    free( psz_video_connection );
    free( psz_audio_connection );
    free( psz_display_mode );

    if( p_display_iterator )
        p_display_iterator->Release();

    if( ret != VLC_SUCCESS )
        Close( p_this );

    return ret;
}

static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    if( p_sys->p_config )
        p_sys->p_config->Release();

    if( p_sys->p_input )
    {
        p_sys->p_input->StopStreams();
        p_sys->p_input->Release();
    }

    if( p_sys->p_card )
        p_sys->p_card->Release();

    if( p_sys->p_delegate )
        p_sys->p_delegate->Release();

    vlc_mutex_destroy( &p_sys->pts_lock );
    free( p_sys );
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool *pb;
    int64_t    *pi64;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            pb = (bool*)va_arg( args, bool * );
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 =
                INT64_C(1000) * var_InheritInteger( p_demux, "live-caching" );
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            vlc_mutex_lock( &p_sys->pts_lock );
            *pi64 = p_sys->i_last_pts;
            vlc_mutex_unlock( &p_sys->pts_lock );
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}
