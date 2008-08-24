/*****************************************************************************
 * pvr.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Eric Petit <titer@videolan.org>
 *          Paul Corke <paulc@datatote.co.uk>
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
#include <vlc_access.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#ifdef HAVE_NEW_LINUX_VIDEODEV2_H
#   ifdef VIDEODEV2_H_FILE
#   include VIDEODEV2_H_FILE
#   else
#   include <linux/videodev2.h>
#   endif
#else
#include "videodev2.h"
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Default caching value for PVR streams. This " \
    "value should be set in milliseconds." )

#define DEVICE_TEXT N_( "Device" )
#define DEVICE_LONGTEXT N_( "PVR video device" )

#define RADIO_DEVICE_TEXT N_( "Radio device" )
#define RADIO_DEVICE_LONGTEXT N_( "PVR radio device" )

#define NORM_TEXT N_( "Norm" )
#define NORM_LONGTEXT N_( "Norm of the stream " \
    "(Automatic, SECAM, PAL, or NTSC)." )

#define WIDTH_TEXT N_( "Width" )
#define WIDTH_LONGTEXT N_( "Width of the stream to capture " \
    "(-1 for autodetection)." )

#define HEIGHT_TEXT N_( "Height" )
#define HEIGHT_LONGTEXT N_( "Height of the stream to capture " \
    "(-1 for autodetection)." )

#define FREQUENCY_TEXT N_( "Frequency" )
#define FREQUENCY_LONGTEXT N_( "Frequency to capture (in kHz), if applicable." )

#define FRAMERATE_TEXT N_( "Framerate" )
#define FRAMERATE_LONGTEXT N_( "Framerate to capture, if applicable " \
    "(-1 for autodetect)." )

#define KEYINT_TEXT N_( "Key interval" )
#define KEYINT_LONGTEXT N_( "Interval between keyframes (-1 for autodetect)." )

#define BFRAMES_TEXT N_( "B Frames" )
#define BFRAMES_LONGTEXT N_("If this option is set, B-Frames will be used. " \
    "Use this option to set the number of B-Frames.")

#define BITRATE_TEXT N_( "Bitrate" )
#define BITRATE_LONGTEXT N_( "Bitrate to use (-1 for default)." )

#define BITRATE_PEAK_TEXT N_( "Bitrate peak" )
#define BITRATE_PEAK_LONGTEXT N_( "Peak bitrate in VBR mode." )

#define BITRATE_MODE_TEXT N_( "Bitrate mode" )
#define BITRATE_MODE_LONGTEXT N_( "Bitrate mode to use (VBR or CBR)." )

#define BITMASK_TEXT N_( "Audio bitmask" )
#define BITMASK_LONGTEXT N_("Bitmask that will "\
    "get used by the audio part of the card." )

#define VOLUME_TEXT N_( "Volume" )
#define VOLUME_LONGTEXT N_("Audio volume (0-65535)." )

#define CHAN_TEXT N_( "Channel" )
#define CHAN_LONGTEXT N_( "Channel of the card to use (Usually, 0 = tuner, " \
    "1 = composite, 2 = svideo)" )

static const int i_norm_list[] =
    { V4L2_STD_UNKNOWN, V4L2_STD_SECAM, V4L2_STD_PAL, V4L2_STD_NTSC };
static const char *const psz_norm_list_text[] =
    { N_("Automatic"), N_("SECAM"), N_("PAL"),  N_("NTSC") };

static const int i_bitrates[] = { 0, 1 };
static const char *const psz_bitrates_list_text[] = { N_("vbr"), N_("cbr") };

static const int pi_radio_range[2] = { 65000, 108000 };

vlc_module_begin();
    set_shortname( N_("PVR") );
    set_description( N_("IVTV MPEG Encoding cards input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    set_capability( "access", 0 );
    add_shortcut( "pvr" );

    add_integer( "pvr-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT,
                 CACHING_LONGTEXT, true );
    add_string( "pvr-device", "/dev/video0", NULL, DEVICE_TEXT,
                 DEVICE_LONGTEXT, false );
    add_string( "pvr-radio-device", "/dev/radio0", NULL, RADIO_DEVICE_TEXT,
                 RADIO_DEVICE_LONGTEXT, false );
    add_integer( "pvr-norm", V4L2_STD_UNKNOWN , NULL, NORM_TEXT,
                 NORM_LONGTEXT, false );
       change_integer_list( i_norm_list, psz_norm_list_text, NULL );
    add_integer( "pvr-width", -1, NULL, WIDTH_TEXT, WIDTH_LONGTEXT, true );
    add_integer( "pvr-height", -1, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT,
                 true );
    add_integer( "pvr-frequency", -1, NULL, FREQUENCY_TEXT, FREQUENCY_LONGTEXT,
                 false );
    add_integer( "pvr-framerate", -1, NULL, FRAMERATE_TEXT, FRAMERATE_LONGTEXT,
                 true );
    add_integer( "pvr-keyint", -1, NULL, KEYINT_TEXT, KEYINT_LONGTEXT,
                 true );
    add_integer( "pvr-bframes", -1, NULL, FRAMERATE_TEXT, FRAMERATE_LONGTEXT,
                 true );
    add_integer( "pvr-bitrate", -1, NULL, BITRATE_TEXT, BITRATE_LONGTEXT,
                 false );
    add_integer( "pvr-bitrate-peak", -1, NULL, BITRATE_PEAK_TEXT,
                 BITRATE_PEAK_LONGTEXT, true );
    add_integer( "pvr-bitrate-mode", -1, NULL, BITRATE_MODE_TEXT,
                 BITRATE_MODE_LONGTEXT, true );
        change_integer_list( i_bitrates, psz_bitrates_list_text, NULL );
    add_integer( "pvr-audio-bitmask", -1, NULL, BITMASK_TEXT,
                 BITMASK_LONGTEXT, true );
    add_integer( "pvr-audio-volume", -1, NULL, VOLUME_TEXT,
                 VOLUME_LONGTEXT, true );
    add_integer( "pvr-channel", -1, NULL, CHAN_TEXT, CHAN_LONGTEXT, true );

    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
static ssize_t Read   ( access_t *, uint8_t *, size_t );
static int Control( access_t *, int, va_list );

/* ivtv specific ioctls */
#define IVTV_IOC_G_CODEC    0xFFEE7703
#define IVTV_IOC_S_CODEC    0xFFEE7704

/* for use with IVTV_IOC_G_CODEC and IVTV_IOC_S_CODEC */

struct ivtv_ioctl_codec {
    uint32_t aspect;
    uint32_t audio_bitmask;
    uint32_t bframes;
    uint32_t bitrate_mode;
    uint32_t bitrate;
    uint32_t bitrate_peak;
    uint32_t dnr_mode;
    uint32_t dnr_spatial;
    uint32_t dnr_temporal;
    uint32_t dnr_type;
    uint32_t framerate;
    uint32_t framespergop;
    uint32_t gop_closure;
    uint32_t pulldown;
    uint32_t stream_type;
};

struct access_sys_t
{
    /* file descriptor */
    int i_fd;
    int i_radio_fd;

    char *psz_videodev;
    char *psz_radiodev;

    /* options */
    int i_standard;
    int i_width;
    int i_height;
    int i_frequency;
    int i_framerate;
    int i_keyint;
    int i_bframes;
    int i_bitrate;
    int i_bitrate_peak;
    int i_bitrate_mode;
    int i_audio_bitmask;
    int i_input;
    int i_volume;

    /* driver version */
    bool b_v4l2_api;
};

/*****************************************************************************
 * ConfigureIVTV: set up codec parameters using the old ivtv api
 *****************************************************************************/
static int ConfigureIVTV( access_t * p_access )
{
    access_sys_t *p_sys = (access_sys_t *) p_access->p_sys;
    struct ivtv_ioctl_codec codec;
    int result;

    memset( &codec, 0, sizeof(struct ivtv_ioctl_codec) );

    result = ioctl( p_sys->i_fd, IVTV_IOC_G_CODEC, &codec );
    if( result < 0 )
    {
        msg_Err( p_access, "Failed to read current capture card settings." );
        return VLC_EGENERIC;
    }

    if( p_sys->i_framerate != -1 )
    {
        switch( p_sys->i_framerate )
        {
            case 30:
                codec.framerate = 0;
                break;

            case 25:
                codec.framerate = 1;
                break;

            default:
                msg_Warn( p_access, "Invalid framerate, reverting to 25." );
                codec.framerate = 1;
                break;
        }
    }

    if( p_sys->i_bitrate != -1 )
    {
        codec.bitrate = p_sys->i_bitrate;
    }

    if( p_sys->i_bitrate_peak != -1 )
    {
        codec.bitrate_peak = p_sys->i_bitrate_peak;
    }

    if( p_sys->i_bitrate_mode != -1 )
    {
        codec.bitrate_mode = p_sys->i_bitrate_mode;
    }

    if( p_sys->i_audio_bitmask != -1 )
    {
        codec.audio_bitmask = p_sys->i_audio_bitmask;
    }

    if( p_sys->i_keyint != -1 )
    {
        codec.framespergop = p_sys->i_keyint;
    }

    if( p_sys->i_bframes != -1 )
    {
        codec.bframes = p_sys->i_bframes;
    }

    result = ioctl( p_sys->i_fd, IVTV_IOC_S_CODEC, &codec );
    if( result  < 0 )
    {
        msg_Err( p_access, "Failed to write new capture card settings." );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "Setting codec parameters to:  framerate: "
                        "%d, bitrate: %d/%d/%d",
                        codec.framerate, codec.bitrate,
                        codec.bitrate_peak, codec.bitrate_mode );
    return VLC_SUCCESS;
}

#ifdef HAVE_NEW_LINUX_VIDEODEV2_H

#define MAX_V4L2_CTRLS (6)
/*****************************************************************************
 * AddV4L2Ctrl: adds a control to the v4l2 controls list
 *****************************************************************************/
static void AddV4L2Ctrl( access_t * p_access,
                         struct v4l2_ext_controls * p_controls,
                         uint32_t i_id, uint32_t i_value )
{
    if( p_controls->count >= MAX_V4L2_CTRLS )
    {
        msg_Err( p_access, "Tried to set too many v4l2 controls at once." );
        return;
    }

    p_controls->controls[p_controls->count].id    = i_id;
    p_controls->controls[p_controls->count].value = i_value;
    p_controls->count++;
}

/*****************************************************************************
 * V4L2SampleRate: calculate v4l2 sample rate from pvr-audio-bitmask
 *****************************************************************************/
static uint32_t V4L2SampleRate( uint32_t i_bitmask )
{
    switch( i_bitmask & 0x0003 )
    {
        case 0x0001: return V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000;
        case 0x0002: return V4L2_MPEG_AUDIO_SAMPLING_FREQ_32000;
    }
    return V4L2_MPEG_AUDIO_SAMPLING_FREQ_44100;
}

/*****************************************************************************
 * V4L2AudioEncoding: calculate v4l2 audio encoding level from pvr-audio-bitmask
 *****************************************************************************/
static uint32_t V4L2AudioEncoding( uint32_t i_bitmask )
{
    switch( i_bitmask & 0x000c )
    {
        case 0x0004: return V4L2_MPEG_AUDIO_ENCODING_LAYER_1;
        case 0x0008: return V4L2_MPEG_AUDIO_ENCODING_LAYER_2;
    }
    return 0xffffffff;
}

/*****************************************************************************
 * V4L2AudioL1Bitrate: calculate v4l2 audio bitrate for layer-1 audio from pvr-audio-bitmask
 *****************************************************************************/
static uint32_t V4L2AudioL1Bitrate( uint32_t i_bitmask )
{
    switch( i_bitmask & 0x00f0 )
    {
        case 0x0010: return V4L2_MPEG_AUDIO_L1_BITRATE_32K;
        case 0x0020: return V4L2_MPEG_AUDIO_L1_BITRATE_64K;
        case 0x0030: return V4L2_MPEG_AUDIO_L1_BITRATE_96K;
        case 0x0040: return V4L2_MPEG_AUDIO_L1_BITRATE_128K;
        case 0x0050: return V4L2_MPEG_AUDIO_L1_BITRATE_160K;
        case 0x0060: return V4L2_MPEG_AUDIO_L1_BITRATE_192K;
        case 0x0070: return V4L2_MPEG_AUDIO_L1_BITRATE_224K;
        case 0x0080: return V4L2_MPEG_AUDIO_L1_BITRATE_256K;
        case 0x0090: return V4L2_MPEG_AUDIO_L1_BITRATE_288K;
        case 0x00a0: return V4L2_MPEG_AUDIO_L1_BITRATE_320K;
        case 0x00b0: return V4L2_MPEG_AUDIO_L1_BITRATE_352K;
        case 0x00c0: return V4L2_MPEG_AUDIO_L1_BITRATE_384K;
        case 0x00d0: return V4L2_MPEG_AUDIO_L1_BITRATE_416K;
        case 0x00e0: return V4L2_MPEG_AUDIO_L1_BITRATE_448K;
    }
    return V4L2_MPEG_AUDIO_L1_BITRATE_320K;
}

/*****************************************************************************
 * V4L2AudioL2Bitrate: calculate v4l2 audio bitrate for layer-1 audio from pvr-audio-bitmask
 *****************************************************************************/
static uint32_t V4L2AudioL2Bitrate( uint32_t i_bitmask )
{
    switch( i_bitmask & 0x00f0 )
    {
        case 0x0010: return V4L2_MPEG_AUDIO_L2_BITRATE_32K;
        case 0x0020: return V4L2_MPEG_AUDIO_L2_BITRATE_48K;
        case 0x0030: return V4L2_MPEG_AUDIO_L2_BITRATE_56K;
        case 0x0040: return V4L2_MPEG_AUDIO_L2_BITRATE_64K;
        case 0x0050: return V4L2_MPEG_AUDIO_L2_BITRATE_80K;
        case 0x0060: return V4L2_MPEG_AUDIO_L2_BITRATE_96K;
        case 0x0070: return V4L2_MPEG_AUDIO_L2_BITRATE_112K;
        case 0x0080: return V4L2_MPEG_AUDIO_L2_BITRATE_128K;
        case 0x0090: return V4L2_MPEG_AUDIO_L2_BITRATE_160K;
        case 0x00a0: return V4L2_MPEG_AUDIO_L2_BITRATE_192K;
        case 0x00b0: return V4L2_MPEG_AUDIO_L2_BITRATE_224K;
        case 0x00c0: return V4L2_MPEG_AUDIO_L2_BITRATE_256K;
        case 0x00d0: return V4L2_MPEG_AUDIO_L2_BITRATE_320K;
        case 0x00e0: return V4L2_MPEG_AUDIO_L2_BITRATE_384K;
    }
    return V4L2_MPEG_AUDIO_L2_BITRATE_192K;
}

/*****************************************************************************
 * V4L2AudioMode: calculate v4l2 audio mode from pvr-audio-bitmask
 *****************************************************************************/
static uint32_t V4L2AudioMode( uint32_t i_bitmask )
{
    switch( i_bitmask & 0x0300 )
    {
        case 0x0100: return V4L2_MPEG_AUDIO_MODE_JOINT_STEREO;
        case 0x0200: return V4L2_MPEG_AUDIO_MODE_DUAL;
        case 0x0300: return V4L2_MPEG_AUDIO_MODE_MONO;
    }
    return V4L2_MPEG_AUDIO_MODE_STEREO;
}

/*****************************************************************************
 * ConfigureV4L2: set up codec parameters using the new v4l2 api
 *****************************************************************************/
static int ConfigureV4L2( access_t * p_access )
{
    access_sys_t *p_sys = (access_sys_t *) p_access->p_sys;
    struct v4l2_ext_controls controls;
    int result;

    memset( &controls, 0, sizeof(struct v4l2_ext_controls) );
    controls.ctrl_class  = V4L2_CTRL_CLASS_MPEG;
    controls.error_idx   = 0;
    controls.reserved[0] = 0;
    controls.reserved[1] = 0;
    controls.count       = 0;
    controls.controls    = calloc( sizeof( struct v4l2_ext_control ),
                                   MAX_V4L2_CTRLS );

    if( controls.controls == NULL )
        return VLC_ENOMEM;

    /* Note: Ignore frame rate.  Doesn't look like it can be changed. */
    if( p_sys->i_bitrate != -1 )
    {
        AddV4L2Ctrl( p_access, &controls, V4L2_CID_MPEG_VIDEO_BITRATE,
                     p_sys->i_bitrate );
        msg_Dbg( p_access, "Setting [%u] bitrate = %u",
                 controls.count - 1, p_sys->i_bitrate );
    }

    if( p_sys->i_bitrate_peak != -1 )
    {
        AddV4L2Ctrl( p_access, &controls, V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
                     p_sys->i_bitrate_peak );
        msg_Dbg( p_access, "Setting [%u] bitrate_peak = %u",
                 controls.count - 1, p_sys->i_bitrate_peak );
    }

    if( p_sys->i_bitrate_mode != -1 )
    {
        AddV4L2Ctrl( p_access, &controls, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
                     p_sys->i_bitrate_mode );
        msg_Dbg( p_access, "Setting [%u] bitrate_mode = %u",
                 controls.count - 1, p_sys->i_bitrate_mode );
    }

    if( p_sys->i_audio_bitmask != -1 )
    {
        /* Sample rate */
        AddV4L2Ctrl( p_access, &controls, V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ,
                    V4L2SampleRate( p_sys->i_audio_bitmask ) );

        /* Encoding layer and bitrate */
        switch( V4L2AudioEncoding( p_sys->i_audio_bitmask ) )
        {
            case V4L2_MPEG_AUDIO_ENCODING_LAYER_1:
                 AddV4L2Ctrl( p_access, &controls,
                              V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ,
                              V4L2_MPEG_AUDIO_ENCODING_LAYER_1 );
                 AddV4L2Ctrl( p_access, &controls,
                              V4L2_CID_MPEG_AUDIO_L1_BITRATE,
                              V4L2AudioL1Bitrate( p_sys->i_audio_bitmask ) );
                 break;

            case V4L2_MPEG_AUDIO_ENCODING_LAYER_2:
                 AddV4L2Ctrl( p_access, &controls,
                              V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ,
                              V4L2_MPEG_AUDIO_ENCODING_LAYER_2 );
                 AddV4L2Ctrl( p_access, &controls,
                              V4L2_CID_MPEG_AUDIO_L2_BITRATE,
                              V4L2AudioL2Bitrate( p_sys->i_audio_bitmask ) );
                 break;
        }

        /* Audio mode - stereo or mono */
        AddV4L2Ctrl( p_access, &controls, V4L2_CID_MPEG_AUDIO_MODE,
                     V4L2AudioMode( p_sys->i_audio_bitmask ) );

        /* See if the user wants any other audio feature */
        if( ( p_sys->i_audio_bitmask & 0x1ff00 ) != 0 )
        {
            /* It would be possible to support the bits that represent:
             *   V4L2_CID_MPEG_AUDIO_MODE_EXTENSION
             *   V4L2_CID_MPEG_AUDIO_EMPHASIS
             *   V4L2_CID_MPEG_AUDIO_CRC
             * but they are not currently used.  Tell the user.
             */
            msg_Err( p_access, "There were bits in pvr-audio-bitmask that were not used.");
        }
        msg_Dbg( p_access, "Setting audio controls");
    }

    if( p_sys->i_keyint != -1 )
    {
        AddV4L2Ctrl( p_access, &controls, V4L2_CID_MPEG_VIDEO_GOP_SIZE,
                     p_sys->i_keyint );
        msg_Dbg( p_access, "Setting [%u] keyint = %u",
                 controls.count - 1, p_sys->i_keyint );
    }

    if( p_sys->i_bframes != -1 )
    {
        AddV4L2Ctrl( p_access, &controls, V4L2_CID_MPEG_VIDEO_B_FRAMES,
                     p_sys->i_bframes );
        msg_Dbg( p_access, "Setting [%u] bframes = %u",
                 controls.count - 1, p_sys->i_bframes );
    }

    result = ioctl( p_sys->i_fd, VIDIOC_S_EXT_CTRLS, &controls );
    if( result < 0 )
    {
        msg_Err( p_access, "Failed to write %u new capture card settings.",
                            controls.error_idx );
    }
    free( controls.controls );
    return VLC_SUCCESS;
}

#endif /* HAVE_NEW_LINUX_VIDEODEV2_H */

/*****************************************************************************
 * Open: open the device
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    access_t *p_access = (access_t*) p_this;
    access_sys_t * p_sys;
    char * psz_tofree;
    char * psz_parser;
    char * psz_device = NULL;
    vlc_value_t val;
    struct v4l2_capability device_capability;
    int result;

    memset( &device_capability, 0, sizeof(struct v4l2_capability) );

    p_access->pf_read = Read;
    p_access->pf_block = NULL;
    p_access->pf_seek = NULL;
    p_access->pf_control = Control;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = false;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;

    /* create private access data */
    p_sys = calloc( sizeof( access_sys_t ), 1 );
    if( !p_sys )
        return VLC_ENOMEM;

    p_access->p_sys = p_sys;

    /* defaults values */
    var_Create( p_access, "pvr-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_access, "pvr-device", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-device" , &val);
    p_sys->psz_videodev = val.psz_string;

    var_Create( p_access, "pvr-radio-device", VLC_VAR_STRING |
                                              VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-radio-device" , &val);
    p_sys->psz_radiodev = val.psz_string;

    var_Create( p_access, "pvr-norm", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-norm" , &val);
    p_sys->i_standard = val.i_int;

    var_Create( p_access, "pvr-width", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-width" , &val);
    p_sys->i_width = val.i_int;

    var_Create( p_access, "pvr-height", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-height" , &val);
    p_sys->i_height = val.i_int;

    var_Create( p_access, "pvr-frequency", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-frequency" , &val);
    p_sys->i_frequency = val.i_int;

    var_Create( p_access, "pvr-framerate", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-framerate" , &val);
    p_sys->i_framerate = val.i_int;

    var_Create( p_access, "pvr-keyint", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-keyint" , &val);
    p_sys->i_keyint = val.i_int;

    var_Create( p_access, "pvr-bframes", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-bframes" , &val);
    p_sys->i_bframes = val.b_bool;

    var_Create( p_access, "pvr-bitrate", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-bitrate" , &val);
    p_sys->i_bitrate = val.i_int;

    var_Create( p_access, "pvr-bitrate-peak", VLC_VAR_INTEGER |
                                              VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-bitrate-peak" , &val);
    p_sys->i_bitrate_peak = val.i_int;

    var_Create( p_access, "pvr-bitrate-mode", VLC_VAR_INTEGER |
                                              VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-bitrate-mode" , &val);
    p_sys->i_bitrate_mode = val.i_int;

    var_Create( p_access, "pvr-audio-bitmask", VLC_VAR_INTEGER |
                                              VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-audio-bitmask" , &val);
    p_sys->i_audio_bitmask = val.i_int;

    var_Create( p_access, "pvr-audio-volume", VLC_VAR_INTEGER |
                                              VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-audio-volume" , &val);
    p_sys->i_volume = val.i_int;

    var_Create( p_access, "pvr-channel", VLC_VAR_INTEGER |
                                              VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-channel" , &val);
    p_sys->i_input = val.i_int;

    /* parse command line options */
    psz_tofree = strdup( p_access->psz_path );
    if( !psz_tofree )
        return VLC_ENOMEM;

    psz_parser = psz_tofree;
    if( *psz_parser )
    {
        for( ;; )
        {
            if ( !strncmp( psz_parser, "norm=", strlen( "norm=" ) ) )
            {
                char *psz_parser_init;
                psz_parser += strlen( "norm=" );
                psz_parser_init = psz_parser;
                while ( (*psz_parser != ':')
                        && (*psz_parser != ',')
                        && (*psz_parser != '\0') )
                {
                    psz_parser++;
                }

                if ( !strncmp( psz_parser_init, "secam" ,
                               psz_parser - psz_parser_init ) )
                {
                    p_sys->i_standard = V4L2_STD_SECAM;
                }
                else if ( !strncmp( psz_parser_init, "pal" ,
                                    psz_parser - psz_parser_init ) )
                {
                    p_sys->i_standard = V4L2_STD_PAL;
                }
                else if ( !strncmp( psz_parser_init, "ntsc" ,
                                    psz_parser - psz_parser_init ) )
                {
                    p_sys->i_standard = V4L2_STD_NTSC;
                }
                else
                {
                    p_sys->i_standard = strtol( psz_parser_init ,
                                                &psz_parser, 0 );
                }
            }
            else if( !strncmp( psz_parser, "channel=",
                               strlen( "channel=" ) ) )
            {
                p_sys->i_input =
                    strtol( psz_parser + strlen( "channel=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "device=", strlen( "device=" ) ) )
            {
                int i_len = strlen( "/dev/videox" );
                psz_device = calloc( i_len  + 1, 1 );
                if( !psz_device )
                    return VLC_ENOMEM;

                snprintf( psz_device, i_len, "/dev/video%ld",
                            strtol( psz_parser + strlen( "device=" ),
                            &psz_parser, 0 ) );
            }
            else if( !strncmp( psz_parser, "frequency=",
                               strlen( "frequency=" ) ) )
            {
                p_sys->i_frequency =
                    strtol( psz_parser + strlen( "frequency=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "framerate=",
                               strlen( "framerate=" ) ) )
            {
                p_sys->i_framerate =
                    strtol( psz_parser + strlen( "framerate=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "keyint=",
                               strlen( "keyint=" ) ) )
            {
                p_sys->i_keyint =
                    strtol( psz_parser + strlen( "keyint=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "bframes=",
                               strlen( "bframes=" ) ) )
            {
                p_sys->i_bframes =
                    strtol( psz_parser + strlen( "bframes=" ),
                            &psz_parser, 0 );
            }

            else if( !strncmp( psz_parser, "width=",
                               strlen( "width=" ) ) )
            {
                p_sys->i_width =
                    strtol( psz_parser + strlen( "width=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "height=",
                               strlen( "height=" ) ) )
            {
                p_sys->i_height =
                    strtol( psz_parser + strlen( "height=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "audio=",
                               strlen( "audio=" ) ) )
            {
                p_sys->i_audio_bitmask =
                    strtol( psz_parser + strlen( "audio=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "bitrate=",
                               strlen( "bitrate=" ) ) )
            {
                p_sys->i_bitrate =
                    strtol( psz_parser + strlen( "bitrate=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "maxbitrate=",
                               strlen( "maxbitrate=" ) ) )
            {
                p_sys->i_bitrate_peak =
                    strtol( psz_parser + strlen( "maxbitrate=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "bitratemode=",
                               strlen( "bitratemode=" ) ) )
            {
                char *psz_parser_init;
                psz_parser += strlen( "bitratemode=" );
                psz_parser_init = psz_parser;
                while ( (*psz_parser != ':')
                        && (*psz_parser != ',')
                        && (*psz_parser != '\0') )
                {
                    psz_parser++;
                }

                if ( !strncmp( psz_parser_init, "vbr" ,
                               psz_parser - psz_parser_init ) )
                {
                     p_sys->i_bitrate_mode = 0;
                }
                else if ( !strncmp( psz_parser_init, "cbr" ,
                                    psz_parser - psz_parser_init ) )
                {
                    p_sys->i_bitrate_mode = 1;
                }
            }
            else if( !strncmp( psz_parser, "size=",
                               strlen( "size=" ) ) )
            {
                p_sys->i_width =
                    strtol( psz_parser + strlen( "size=" ),
                            &psz_parser, 0 );
                p_sys->i_height =
                    strtol( psz_parser + 1 ,
                            &psz_parser, 0 );
            }
            else
            {
                char *psz_parser_init;
                psz_parser_init = psz_parser;
                while ( (*psz_parser != ':') &&
                        (*psz_parser != ',') &&
                        (*psz_parser != '\0') )
                {
                    psz_parser++;
                }
                psz_device = calloc( psz_parser - psz_parser_init + 1, 1 );
                if( !psz_device )
                    return VLC_ENOMEM;

                strncpy( psz_device, psz_parser_init,
                         psz_parser - psz_parser_init );
            }
            if( *psz_parser )
                psz_parser++;
            else
                break;
        }
    }
    free( psz_tofree );

    if( psz_device )
    {
        free( p_sys->psz_videodev );
        p_sys->psz_videodev = psz_device;
    }

    /* open the device */
    p_sys->i_fd = open( p_sys->psz_videodev, O_RDWR );
    if( p_sys->i_fd < 0 )
    {
        msg_Err( p_access, "Cannot open device (%m)." );
        Close( VLC_OBJECT(p_access) );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_access, "Using video device: %s.", p_sys->psz_videodev);

    /* See what version of ivtvdriver is running */
    result = ioctl( p_sys->i_fd, VIDIOC_QUERYCAP, &device_capability );
    if( result < 0 )
    {
        msg_Err( p_access, "unknown ivtv/pvr driver version in use" );
        Close( VLC_OBJECT(p_access) );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "%s driver (%s on %s) version %02x.%02x.%02x",
              device_capability.driver,
              device_capability.card,
              device_capability.bus_info,
            ( device_capability.version >> 16 ) & 0xff,
            ( device_capability.version >>  8 ) & 0xff,
            ( device_capability.version       ) & 0xff);

    if ( strncmp( (char *) device_capability.driver, "ivtv", 4 )
           || device_capability.version >= 0x000800 )
    {
        /* Drivers > 0.8.0 use v4l2 API instead of IVTV ioctls */
        msg_Dbg( p_access, "this driver uses the v4l2 API" );
        p_sys->b_v4l2_api = true;
    }
    else
    {
        p_sys->b_v4l2_api = false;
    }

    /* set the input */
    if ( p_sys->i_input != -1 )
    {
        result = ioctl( p_sys->i_fd, VIDIOC_S_INPUT, &p_sys->i_input );
        if ( result < 0 )
            msg_Warn( p_access, "Failed to select the requested input pin." );
        else
            msg_Dbg( p_access, "input set to: %d", p_sys->i_input );
    }

    /* set the video standard */
    if ( p_sys->i_standard != V4L2_STD_UNKNOWN )
    {
        result = ioctl( p_sys->i_fd, VIDIOC_S_STD, &p_sys->i_standard );
        if ( result  < 0 )
            msg_Warn( p_access, "Failed to set the requested video standard." );
        else
            msg_Dbg( p_access, "video standard set to: %x",
                     p_sys->i_standard);
    }

    /* set the picture size */
    if ( (p_sys->i_width != -1) || (p_sys->i_height != -1) )
    {
        struct v4l2_format vfmt;

        memset( &vfmt, 0, sizeof(struct v4l2_format) );
        vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        result = ioctl( p_sys->i_fd, VIDIOC_G_FMT, &vfmt );
        if ( result < 0 )
        {
            msg_Warn( p_access, "Failed to read current picture size." );
        }
        else
        {
            if ( p_sys->i_width != -1 )
            {
                vfmt.fmt.pix.width = p_sys->i_width;
            }

            if ( p_sys->i_height != -1 )
            {
                vfmt.fmt.pix.height = p_sys->i_height;
            }

            result = ioctl( p_sys->i_fd, VIDIOC_S_FMT, &vfmt );
            if ( result < 0 )
            {
                msg_Warn( p_access, "Failed to set requested picture size." );
            }
            else
            {
                msg_Dbg( p_access, "picture size set to: %dx%d",
                         vfmt.fmt.pix.width, vfmt.fmt.pix.height );
            }
        }
    }

    /* set the frequency */
    if ( p_sys->i_frequency != -1 )
    {
        int i_fd;
        struct v4l2_tuner vt;

         /* TODO: let the user choose the tuner */
        memset( &vt, 0, sizeof(struct v4l2_tuner) );

        if ( (p_sys->i_frequency >= pi_radio_range[0])
              && (p_sys->i_frequency <= pi_radio_range[1]) )
        {
            p_sys->i_radio_fd = open( p_sys->psz_radiodev, O_RDWR );
            if( p_sys->i_radio_fd < 0 )
            {
                msg_Err( p_access, "Cannot open radio device (%m)." );
                Close( VLC_OBJECT(p_access) );
                return VLC_EGENERIC;
            }
            msg_Dbg( p_access, "using radio device: %s",
                     p_sys->psz_radiodev );
            i_fd = p_sys->i_radio_fd;
        }
        else
        {
            i_fd = p_sys->i_fd;
            p_sys->i_radio_fd = -1;
        }

        result = ioctl( i_fd, VIDIOC_G_TUNER, &vt );
        if ( result < 0 )
        {
            msg_Warn( p_access, "Failed to read tuner information (%m)." );
        }
        else
        {
            struct v4l2_frequency vf;

            memset( &vf, 0, sizeof(struct v4l2_frequency) );
            vf.tuner = vt.index;

            result = ioctl( i_fd, VIDIOC_G_FREQUENCY, &vf );
            if ( result < 0 )
            {
                msg_Warn( p_access, "Failed to read tuner frequency (%m)." );
            }
            else
            {
                if( vt.capability & V4L2_TUNER_CAP_LOW )
                    vf.frequency = p_sys->i_frequency * 16;
                else
                    vf.frequency = (p_sys->i_frequency * 16 + 500) / 1000;

                result = ioctl( i_fd, VIDIOC_S_FREQUENCY, &vf );
                if( result < 0 )
                {
                    msg_Warn( p_access, "Failed to set tuner frequency (%m)." );
                }
                else
                {
                    msg_Dbg( p_access, "tuner frequency set to: %d",
                             p_sys->i_frequency );
                }
            }
        }
    }

    /* control parameters */
    if ( p_sys->i_volume != -1 )
    {
        struct v4l2_control ctrl;

        memset( &ctrl, 0, sizeof(struct v4l2_control) );
        ctrl.id = V4L2_CID_AUDIO_VOLUME;
        ctrl.value = p_sys->i_volume;

        result = ioctl( p_sys->i_fd, VIDIOC_S_CTRL, &ctrl );
        if ( result < 0 )
        {
            msg_Warn( p_access, "Failed to set the volume." );
        }
    }

    /* codec parameters */
    if ( (p_sys->i_framerate != -1)
            || (p_sys->i_bitrate_mode != -1)
            || (p_sys->i_bitrate_peak != -1)
            || (p_sys->i_keyint != -1)
            || (p_sys->i_bframes != -1)
            || (p_sys->i_bitrate != -1)
            || (p_sys->i_audio_bitmask != -1) )
    {
        if( p_sys->b_v4l2_api )
        {
#ifdef HAVE_NEW_LINUX_VIDEODEV2_H
            result = ConfigureV4L2( p_access );
            if( result != VLC_SUCCESS )
            {
                Close( VLC_OBJECT(p_access) );
                return result;
            }
#else
            msg_Warn( p_access, "You have new ivtvdrivers, "
                      "but this vlc was built against an old v4l2 version." );
#endif
        }
        else
        {
            result = ConfigureIVTV( p_access );
            if( result != VLC_SUCCESS )
            {
                Close( VLC_OBJECT(p_access) );
                return result;
            }
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the device
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    access_t *p_access = (access_t*) p_this;
    access_sys_t *p_sys = (access_sys_t *) p_access->p_sys;

    if ( p_sys->i_fd != -1 )
        close( p_sys->i_fd );
    if ( p_sys->i_radio_fd != -1 )
        close( p_sys->i_radio_fd );
    free( p_sys->psz_videodev );
    free( p_sys->psz_radiodev );
    free( p_sys );
}

/*****************************************************************************
 * Read
 *****************************************************************************/
static ssize_t Read( access_t * p_access, uint8_t * p_buffer, size_t i_len )
{
    access_sys_t *p_sys = (access_sys_t *) p_access->p_sys;
    struct pollfd ufd;
    int i_ret;

    ufd.fd = p_sys->i_fd;
    ufd.events = POLLIN;

    if( p_access->info.b_eof )
        return 0;

    do
    {
        if( !vlc_object_alive (p_access) )
            return 0;

        ufd.revents = 0;
    }
    while( ( i_ret = poll( &ufd, 1, 500 ) ) == 0 );

    if( i_ret < 0 )
    {
        msg_Err( p_access, "Polling error (%m)." );
        return -1;
    }

    i_ret = read( p_sys->i_fd, p_buffer, i_len );
    if( i_ret == 0 )
    {
        p_access->info.b_eof = true;
    }
    else if( i_ret > 0 )
    {
        p_access->info.i_pos += i_ret;
    }

    return i_ret;
}

/*****************************************************************************
 * Control
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    bool   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;
        case ACCESS_CAN_PAUSE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = (int64_t)var_GetInteger( p_access, "pvr-caching" ) * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "Unimplemented query in control." );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}
