/*****************************************************************************
 * pvr.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Eric Petit <titer@videolan.org>
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
#include <vlc/vlc.h>
#include <vlc/input.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include "videodev2.h"

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

#define BITRATE_MODE_TEXT N_( "Bitrate mode)" )
#define BITRATE_MODE_LONGTEXT N_( "Bitrate mode to use (VBR or CBR)." )

#define BITMASK_TEXT N_( "Audio bitmask" )
#define BITMASK_LONGTEXT N_("Bitmask that will "\
    "get used by the audio part of the card." )

#define VOLUME_TEXT N_( "Volume" )
#define VOLUME_LONGTEXT N_("Audio volume (0-65535)." )

#define CHAN_TEXT N_( "Channel" )
#define CHAN_LONGTEXT N_( "Channel of the card to use (Usually, 0 = tuner, " \
    "1 = composite, 2 = svideo)" )

static int i_norm_list[] =
    { V4L2_STD_UNKNOWN, V4L2_STD_SECAM, V4L2_STD_PAL, V4L2_STD_NTSC };
static char *psz_norm_list_text[] =
    { N_("Automatic"), N_("SECAM"), N_("PAL"),  N_("NTSC") };

static int i_bitrates[] = { 0, 1 };
static char *psz_bitrates_list_text[] = { N_("vbr"), N_("cbr") };

static int pi_radio_range[2] = { 65000, 108000 };

vlc_module_begin();
    set_shortname( _("PVR") );
    set_description( _("IVTV MPEG Encoding cards input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    set_capability( "access2", 0 );
    add_shortcut( "pvr" );

    add_integer( "pvr-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_string( "pvr-device", "/dev/video0", NULL, DEVICE_TEXT,
                 DEVICE_LONGTEXT, VLC_FALSE );
    add_string( "pvr-radio-device", "/dev/radio0", NULL, RADIO_DEVICE_TEXT,
                 RADIO_DEVICE_LONGTEXT, VLC_FALSE );
    add_integer( "pvr-norm", V4L2_STD_UNKNOWN , NULL, NORM_TEXT,
                 NORM_LONGTEXT, VLC_FALSE );
       change_integer_list( i_norm_list, psz_norm_list_text, 0 );
    add_integer( "pvr-width", -1, NULL, WIDTH_TEXT, WIDTH_LONGTEXT, VLC_TRUE );
    add_integer( "pvr-height", -1, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT,
                 VLC_TRUE );
    add_integer( "pvr-frequency", -1, NULL, FREQUENCY_TEXT, FREQUENCY_LONGTEXT,
                 VLC_FALSE );
    add_integer( "pvr-framerate", -1, NULL, FRAMERATE_TEXT, FRAMERATE_LONGTEXT,
                 VLC_TRUE );
    add_integer( "pvr-keyint", -1, NULL, KEYINT_TEXT, KEYINT_LONGTEXT,
                 VLC_TRUE );
    add_integer( "pvr-bframes", -1, NULL, FRAMERATE_TEXT, FRAMERATE_LONGTEXT,
                 VLC_TRUE );
    add_integer( "pvr-bitrate", -1, NULL, BITRATE_TEXT, BITRATE_LONGTEXT,
                 VLC_FALSE );
    add_integer( "pvr-bitrate-peak", -1, NULL, BITRATE_PEAK_TEXT,
                 BITRATE_PEAK_LONGTEXT, VLC_TRUE );
    add_integer( "pvr-bitrate-mode", -1, NULL, BITRATE_MODE_TEXT,
                 BITRATE_MODE_LONGTEXT, VLC_TRUE );
        change_integer_list( i_bitrates, psz_bitrates_list_text, 0 );
    add_integer( "pvr-audio-bitmask", -1, NULL, BITMASK_TEXT,
                 BITMASK_LONGTEXT, VLC_TRUE );
    add_integer( "pvr-audio-volume", -1, NULL, VOLUME_TEXT,
                 VOLUME_LONGTEXT, VLC_TRUE );
    add_integer( "pvr-channel", -1, NULL, CHAN_TEXT, CHAN_LONGTEXT, VLC_TRUE );

    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
static int Read   ( access_t *, uint8_t *, int );
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
};

/*****************************************************************************
 * Open: open the device
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    access_t *p_access = (access_t*) p_this;
    access_sys_t * p_sys;
    char * psz_tofree, * psz_parser, * psz_device, * psz_radio_device;
    vlc_value_t val;

    //psz_device = calloc( strlen( "/dev/videox" ) + 1, 1 );

    p_access->pf_read = Read;
    p_access->pf_block = NULL;
    p_access->pf_seek = NULL;
    p_access->pf_control = Control;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;

    /* create private access data */
    p_sys = calloc( sizeof( access_sys_t ), 1 );
    p_access->p_sys = p_sys;

    /* defaults values */
    var_Create( p_access, "pvr-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_access, "pvr-device", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-device" , &val);
    psz_device = val.psz_string;

    var_Create( p_access, "pvr-radio-device", VLC_VAR_STRING |
                                              VLC_VAR_DOINHERIT );
    var_Get( p_access, "pvr-radio-device" , &val);
    psz_radio_device = val.psz_string;

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
                while ( *psz_parser != ':' && *psz_parser != ','
                                                    && *psz_parser != '\0' )
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
                psz_device = calloc( strlen( "/dev/videox" ) + 1, 1 );
                sprintf( psz_device, "/dev/video%ld",
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
                while ( *psz_parser != ':' && *psz_parser != ','
                         && *psz_parser != '\0' )
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
                while ( *psz_parser != ':' && *psz_parser != ',' && *psz_parser != '\0' )
                {
                    psz_parser++;
                }
                psz_device = calloc( psz_parser - psz_parser_init + 1, 1 );
                strncpy( psz_device, psz_parser_init,
                         psz_parser - psz_parser_init );
            }
            if( *psz_parser )
                psz_parser++;
            else
                break;
        }
    }

    //give a default value to psz_device if none has been specified

    if ( psz_device == NULL )
    {
        psz_device = calloc( strlen( "/dev/videox" ) + 1, 1 );
        strcpy( psz_device, "/dev/video0" );
    }

    free( psz_tofree );

    /* open the device */
    if( ( p_sys->i_fd = open( psz_device, O_RDWR ) ) < 0 )
    {
        msg_Err( p_access, "cannot open device (%s)", strerror( errno ) );
        free( p_sys );
        return VLC_EGENERIC;
    }
    else
    {
        msg_Dbg( p_access, "using video device: %s",psz_device);
    }

    free( psz_device );

    /* set the input */
    if ( p_sys->i_input != -1 )
    {
        if ( ioctl( p_sys->i_fd, VIDIOC_S_INPUT, &p_sys->i_input ) < 0 )
        {
            msg_Warn( p_access, "VIDIOC_S_INPUT failed" );
        }
        else
        {
            msg_Dbg( p_access, "input set to :%d", p_sys->i_input);
        }
    }

    /* set the video standard */
    if ( p_sys->i_standard != V4L2_STD_UNKNOWN )
    {
        if ( ioctl( p_sys->i_fd, VIDIOC_S_STD, &p_sys->i_standard ) < 0 )
        {
            msg_Warn( p_access, "VIDIOC_S_STD failed" );
        }
        else
        {
            msg_Dbg( p_access, "video standard set to :%x", p_sys->i_standard);
        }
    }

    /* set the picture size */
    if ( p_sys->i_width != -1 || p_sys->i_height != -1 )
    {
        struct v4l2_format vfmt;

        vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ( ioctl( p_sys->i_fd, VIDIOC_G_FMT, &vfmt ) < 0 )
        {
            msg_Warn( p_access, "VIDIOC_G_FMT failed" );
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

            if ( ioctl( p_sys->i_fd, VIDIOC_S_FMT, &vfmt ) < 0 )
            {
                msg_Warn( p_access, "VIDIOC_S_FMT failed" );
            }
            else
            {
                msg_Dbg( p_access, "picture size set to :%dx%d",
                         vfmt.fmt.pix.width, vfmt.fmt.pix.height );
            }
        }
    }

    /* set the frequency */
    if ( p_sys->i_frequency != -1 )
    {
        int i_fd;
        struct v4l2_frequency vf;
        vf.tuner = 0; /* TODO: let the user choose the tuner */

        if ( p_sys->i_frequency >= pi_radio_range[0]
              && p_sys->i_frequency <= pi_radio_range[1] )
        {
            if( ( p_sys->i_radio_fd = open( psz_radio_device, O_RDWR ) ) < 0 )
            {
                msg_Err( p_access, "cannot open radio device (%s)",
                         strerror( errno ) );
                close( p_sys->i_fd );
                free( p_sys );
                return VLC_EGENERIC;
            }
            else
            {
                msg_Dbg( p_access, "using radio device: %s", psz_radio_device );
            }
            i_fd = p_sys->i_radio_fd;
        }
        else
        {
            i_fd = p_sys->i_fd;
            p_sys->i_radio_fd = -1;
        }

        if ( ioctl( i_fd, VIDIOC_G_FREQUENCY, &vf ) < 0 )
        {
            msg_Warn( p_access, "VIDIOC_G_FREQUENCY failed (%s)",
                      strerror( errno ) );
        }
        else
        {
            vf.frequency = (p_sys->i_frequency * 16 + 500) / 1000;
            if( ioctl( i_fd, VIDIOC_S_FREQUENCY, &vf ) < 0 )
            {
                msg_Warn( p_access, "VIDIOC_S_FREQUENCY failed (%s)",
                          strerror( errno ) );
            }
            else
            {
                msg_Dbg( p_access, "tuner frequency set to :%d",
                         p_sys->i_frequency );
            }
        }
    }

    /* control parameters */
    if ( p_sys->i_volume != -1 )
    {
        struct v4l2_control ctrl;

        ctrl.id = V4L2_CID_AUDIO_VOLUME;
        ctrl.value = p_sys->i_volume;

        if ( ioctl( p_sys->i_fd, VIDIOC_S_CTRL, &ctrl ) < 0 )
        {
            msg_Warn( p_access, "VIDIOC_S_CTRL failed" );
        }
    }

    /* codec parameters */
    if ( p_sys->i_framerate != -1
            || p_sys->i_bitrate_mode != -1
            || p_sys->i_bitrate_peak != -1
            || p_sys->i_keyint != -1
            || p_sys->i_bframes != -1
            || p_sys->i_bitrate != -1
            || p_sys->i_audio_bitmask != -1 )
    {
        struct ivtv_ioctl_codec codec;

        if ( ioctl( p_sys->i_fd, IVTV_IOC_G_CODEC, &codec ) < 0 )
        {
            msg_Warn( p_access, "IVTV_IOC_G_CODEC failed" );
        }
        else
        {
            if ( p_sys->i_framerate != -1 )
            {
                switch ( p_sys->i_framerate )
                {
                    case 30:
                        codec.framerate = 0;
                        break;

                    case 25:
                        codec.framerate = 1;
                        break;

                    default:
                        msg_Warn( p_access, "invalid framerate, reverting to 25" );
                        codec.framerate = 1;
                        break;
                }
            }

            if ( p_sys->i_bitrate != -1 )
            {
                codec.bitrate = p_sys->i_bitrate;
            }

            if ( p_sys->i_bitrate_peak != -1 )
            {
                codec.bitrate_peak = p_sys->i_bitrate_peak;
            }

            if ( p_sys->i_bitrate_mode != -1 )
            {
                codec.bitrate_mode = p_sys->i_bitrate_mode;
            }

            if ( p_sys->i_audio_bitmask != -1 )
            {
                codec.audio_bitmask = p_sys->i_audio_bitmask;
            }
            if ( p_sys->i_keyint != -1 )
            {
                codec.framespergop = p_sys->i_keyint;
            }

            if ( p_sys->i_bframes != -1 )
            {
                codec.bframes = p_sys->i_bframes;
            }
            if( ioctl( p_sys->i_fd, IVTV_IOC_S_CODEC, &codec ) < 0 )
            {
                msg_Warn( p_access, "IVTV_IOC_S_CODEC failed" );
            }
            else
            {
                msg_Dbg( p_access, "Setting codec parameters to:  framerate: %d, bitrate: %d/%d/%d",
               codec.framerate, codec.bitrate, codec.bitrate_peak, codec.bitrate_mode );
            }
        }
    }

    /* do a quick read */
#if 0
    if ( p_sys->i_fd )
    {
        if ( read( p_sys->i_fd, psz_tmp, 1 ) )
        {
            msg_Dbg(p_input, "Could read byte from device");
        }
        else
        {
            msg_Warn(p_input, "Could not read byte from device");
        }
    }
#endif

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the device
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    access_t *p_access = (access_t*) p_this;
    access_sys_t * p_sys = p_access->p_sys;

    close( p_sys->i_fd );
    if ( p_sys->i_radio_fd != -1 )
        close( p_sys->i_radio_fd );
    free( p_sys );
}

/*****************************************************************************
 * Read
 *****************************************************************************/
static int Read( access_t * p_access, uint8_t * p_buffer, int i_len )
{
    access_sys_t * p_sys = p_access->p_sys;

    int i_ret;

    struct timeval timeout;
    fd_set fds;

    FD_ZERO( &fds );
    FD_SET( p_sys->i_fd, &fds );
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    if( p_access->info.b_eof )
        return 0;

    while( !( i_ret = select( p_sys->i_fd + 1, &fds, NULL, NULL, &timeout) ) )
    {
        FD_ZERO( &fds );
        FD_SET( p_sys->i_fd, &fds );
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        if( p_access->b_die )
            return 0;
    }

    if( i_ret < 0 )
    {
        msg_Err( p_access, "select error (%s)", strerror( errno ) );
        return -1;
    }

    i_ret = read( p_sys->i_fd, p_buffer, i_len );
    if( i_ret == 0 )
    {
        p_access->info.b_eof = VLC_TRUE;
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
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;
        case ACCESS_CAN_PAUSE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
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
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}
