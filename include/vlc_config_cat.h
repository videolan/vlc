/*****************************************************************************
 * vlc_config_cat.h : Definition of configuration categories
 *****************************************************************************
 * Copyright (C) 2003 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Anil Daoud <anil@videolan.org>
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

#ifndef VLC_HELP_H
#define VLC_HELP_H 1
# include <vlc_plugin.h>

/*
 *  First, we need help strings for the General Settings and for the
 *  Plugins screen
 */
#define MAIN_TITLE N_( "VLC preferences" )
#define MAIN_HELP N_( \
    "Select \"Advanced Options\" to see all options." )

/* Interface */
#define INTF_TITLE N_("Interface")
#define INTF_HELP  N_( "Settings for VLC's interfaces" )

#define INTF_GENERAL_HELP N_( "Main interfaces settings" )

#define INTF_MAIN_TITLE  N_( "Main interfaces" )
#define INTF_MAIN_HELP N_( "Settings for the main interface" )

#define INTF_CONTROL_TITLE N_( "Control interfaces" )
#define INTF_CONTROL_HELP N_( "Settings for VLC's control interfaces" )

#define INTF_HOTKEYS_TITLE N_( "Hotkeys settings" )
#define INTF_HOTKEYS_HELP N_( "Hotkeys settings" )

/* Audio */
#define AUDIO_TITLE N_( "Audio" )
#define AUDIO_HELP N_( "Audio settings" )

#define AUDIO_GENERAL_HELP N_("General audio settings")

#define AFILTER_TITLE N_("Filters")
#define AFILTER_HELP N_( "Audio filters are used to process the audio stream." )

#define ARESAMPLER_TITLE N_("Audio resampler")

#define AVISUAL_TITLE N_("Visualizations")
#define AVISUAL_HELP N_( "Audio visualizations" )

#define AOUT_TITLE N_( "Output modules" )
#define AOUT_HELP N_("General settings for audio output modules.")

#define AMISC_TITLE N_("Miscellaneous")
#define AMISC_HELP N_( "Miscellaneous audio settings and modules." )

/* Video */
#define VIDEO_TITLE N_("Video")
#define VIDEO_HELP N_("Video settings")

#define VIDEO_GENERAL_HELP N_( "General video settings" )

#define _VOUT_TITLE N_("Output modules" )
#define VOUT_HELP N_("General settings for video output modules.")

#define VFILTER_TITLE N_("Filters" )
#define VFILTER_HELP N_("Video filters are used to process the video stream." )

#define SUBPIC_TITLE N_( "Subtitles / OSD")
#define SUBPIC_HELP N_( "Settings related to On-Screen-Display,"\
        " subtitles and \"overlay subpictures\"")

#define SPLITTER_TITLE N_("Splitters")
#define SPLITTER_HELP N_("Video splitters separate the stream into multiple videos.")

/*
#define TEXT_HELP N_( \
    "Use the settings of the \"freetype\" module to choose the font you " \
    "want VLC to use for text rendering (to display subtitles for example).")
*/
/* Input */
#define INPUT_TITLE N_( "Input / Codecs" )
#define INPUT_HELP N_( "Settings for input, demultiplexing, " \
         "decoding and encoding")

#define ACCESS_TITLE N_( "Access modules" )
#define ACCESS_HELP N_( \
    "Settings related to the various access methods. " \
    "Common settings you may want to alter are HTTP proxy or " \
    "caching settings." )

#define STREAM_FILTER_TITLE N_( "Stream filters" )
#define STREAM_FILTER_HELP N_( \
    "Stream filters are special modules that allow advanced operations on " \
    "the input side of VLC. Use with care..." )

#define DEMUX_TITLE N_("Demuxers")
#define DEMUX_HELP N_( "Demuxers are used to separate audio and video streams." )

#define VDEC_TITLE  N_( "Video codecs" )
#define VDEC_HELP N_( "Settings for the video, images or video+audio decoders and encoders." )

#define ADEC_TITLE  N_( "Audio codecs" )
#define ADEC_HELP N_( "Settings for the audio-only decoders and encoders." )

#define SDEC_TITLE N_( "Subtitle codecs")
#define SDEC_HELP N_( "Settings for subtitle, teletext and CC decoders and encoders." )

#define ADVANCED_HELP N_( "General input settings. Use with care..." )

/* Sout */
#define SOUT_TITLE N_( "Stream output" )
#define SOUT_HELP N_( \
      "Stream output settings are used when acting as a streaming server " \
      "or when saving incoming streams.\n" \
      "Streams are first muxed and then sent through an \"access output\" "\
      "module that can either save the stream to a file, or stream " \
      "it (UDP, HTTP, RTP/RTSP).\n" \
      "Sout streams modules allow advanced stream processing (transcoding, "\
      "duplicating...).")

#define SOUT_GENERAL_HELP N_( "General stream output settings")

#define SOUT_MUX_TITLE N_( "Muxers" )
#define SOUT_MUX_HELP N_( \
       "Muxers create the encapsulation formats that are used to " \
       "put all the elementary streams (video, audio, ...) " \
       "together. This setting allows you to always force a specific muxer. " \
       "You should probably not do that.\n" \
       "You can also set default parameters for each muxer." )

#define SOUT_ACO_TITLE N_( "Access output" )
#define SOUT_ACO_HELP N_( \
   "Access output modules control the ways the muxed streams are sent. " \
   "This setting allows you to always force a specific access output method. " \
   "You should probably not do that.\n" \
   "You can also set default parameters for each access output.")

#define SOUT_PACKET_TITLE N_( "Packetizers" )
#define SOUT_PACKET_HELP N_( \
        "Packetizers are used to \"preprocess\" the elementary "\
        "streams before muxing. " \
        "This setting allows you to always force a packetizer. " \
        "You should probably not do that.\n" \
        "You can also set default parameters for each packetizer." )

#define SOUT_STREAM_TITLE N_("Sout stream")
#define SOUT_STREAM_HELP N_( "Sout stream modules allow to build a sout " \
                "processing chain. Please refer to the Streaming Howto for " \
                "more information. You can configure default options for " \
                "each sout stream module here.")

#define SOUT_VOD_TITLE N_( "VOD" )
#define SOUT_VOD_HELP N_( "VLC's implementation of Video On Demand" )


/* Playlist */
#define PLAYLIST_TITLE N_( "Playlist" )
#define PLAYLIST_HELP N_( "Settings related to playlist behaviour " \
        "(e.g. playback mode) and to modules that automatically add "\
        "items to the playlist (\"service discovery\" modules).")

#define PGENERAL_HELP N_( "General playlist behaviour")
#define SD_TITLE N_("Services discovery")
#define SD_HELP N_("Services discovery modules are facilities "\
        "that automatically add items to playlist.")

/* Advanced */
#define AADVANCED_TITLE N_( "Advanced" )
#define AADVANCED_HELP N_( "Advanced settings. Use with care...")

#define MISC_TITLE N_( "Advanced settings" )

/* This function is deprecated and is kept only for compatibility */
static const struct config_category_t categories_array[] =
{
    /* Interface */
    { CAT_INTERFACE, INTF_TITLE, INTF_HELP },
    { SUBCAT_INTERFACE_GENERAL, INTF_TITLE, INTF_GENERAL_HELP },
    { SUBCAT_INTERFACE_MAIN, INTF_MAIN_TITLE, INTF_MAIN_HELP },
    { SUBCAT_INTERFACE_CONTROL, INTF_CONTROL_TITLE, INTF_CONTROL_HELP },
    { SUBCAT_INTERFACE_HOTKEYS, INTF_HOTKEYS_TITLE, INTF_HOTKEYS_HELP },

    { CAT_AUDIO, AUDIO_TITLE, AUDIO_HELP },
    { SUBCAT_AUDIO_GENERAL, AUDIO_TITLE, AUDIO_GENERAL_HELP },
    { SUBCAT_AUDIO_AOUT, AOUT_TITLE, AOUT_HELP },
    { SUBCAT_AUDIO_AFILTER, AFILTER_TITLE, AFILTER_HELP },
    { SUBCAT_AUDIO_RESAMPLER, ARESAMPLER_TITLE, AFILTER_HELP },
    { SUBCAT_AUDIO_VISUAL, AVISUAL_TITLE, AVISUAL_HELP },
    { SUBCAT_AUDIO_MISC, AMISC_TITLE, AMISC_HELP },

    { CAT_VIDEO, VIDEO_TITLE, VIDEO_HELP },
    { SUBCAT_VIDEO_GENERAL, VIDEO_TITLE, VIDEO_GENERAL_HELP },
    { SUBCAT_VIDEO_VOUT, _VOUT_TITLE, VOUT_HELP },
    { SUBCAT_VIDEO_VFILTER, VFILTER_TITLE, VFILTER_HELP },
    { SUBCAT_VIDEO_SUBPIC, SUBPIC_TITLE, SUBPIC_HELP },
    { SUBCAT_VIDEO_SPLITTER, SPLITTER_TITLE, SPLITTER_HELP },

    { CAT_INPUT, INPUT_TITLE, INPUT_HELP },
    { SUBCAT_INPUT_GENERAL, INPUT_TITLE, INPUT_HELP },
    { SUBCAT_INPUT_ACCESS, ACCESS_TITLE, ACCESS_HELP },
    { SUBCAT_INPUT_DEMUX, DEMUX_TITLE, DEMUX_HELP },
    { SUBCAT_INPUT_VCODEC, VDEC_TITLE, VDEC_HELP },
    { SUBCAT_INPUT_ACODEC, ADEC_TITLE, ADEC_HELP },
    { SUBCAT_INPUT_SCODEC, SDEC_TITLE, SDEC_HELP },
    { SUBCAT_INPUT_STREAM_FILTER, STREAM_FILTER_TITLE, STREAM_FILTER_HELP },

    { CAT_SOUT, SOUT_TITLE, SOUT_HELP },
    { SUBCAT_SOUT_GENERAL, SOUT_TITLE, SOUT_GENERAL_HELP },
    { SUBCAT_SOUT_STREAM, SOUT_STREAM_TITLE, SOUT_STREAM_HELP },
    { SUBCAT_SOUT_MUX, SOUT_MUX_TITLE, SOUT_MUX_HELP },
    { SUBCAT_SOUT_ACO, SOUT_ACO_TITLE, SOUT_ACO_HELP },
    { SUBCAT_SOUT_PACKETIZER, SOUT_PACKET_TITLE, SOUT_PACKET_HELP },
    { SUBCAT_SOUT_VOD, SOUT_VOD_TITLE, SOUT_VOD_HELP },

    { CAT_PLAYLIST, PLAYLIST_TITLE , PLAYLIST_HELP },
    { SUBCAT_PLAYLIST_GENERAL, PLAYLIST_TITLE, PGENERAL_HELP },
    { SUBCAT_PLAYLIST_SD, SD_TITLE, SD_HELP },

    { CAT_ADVANCED, AADVANCED_TITLE, AADVANCED_HELP },
    { SUBCAT_ADVANCED_MISC, MISC_TITLE, AADVANCED_HELP },

    { -1, NULL, NULL }
};

VLC_USED
static inline const char *config_CategoryNameGet( int i_value )
{
    int i = 0;
    while( categories_array[i].psz_name != NULL )
    {
        if( categories_array[i].i_id == i_value )
        {
            return vlc_gettext(categories_array[i].psz_name);
        }
        i++;
    }
    return NULL;
}

VLC_USED
static inline const char *config_CategoryHelpGet( int i_value )
{
    int i = 0;
    while( categories_array[i].psz_help != NULL )
    {
        if( categories_array[i].i_id == i_value )
        {
            return vlc_gettext(categories_array[i].psz_help);
        }
        i++;
    }
    return NULL;
}

#endif /* VLC_HELP_H */
