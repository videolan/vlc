/*****************************************************************************
 * vlc_help.h: Help strings
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Anil Daoud <anil@videolan.org>
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

#ifndef _VLC_HELP_H
#define _VLC_HELP_H 1

/*
 *  First, we need help strings for the General Settings and for the
 *  Plugins screen
 */
#define MAIN_TITLE N_( "VLC preferences" )
#define MAIN_HELP N_( \
    "Select \"Advanced Options\" to see all options." )

#define GENERAL_TITLE N_("General")

/* Interface */
#define INTF_TITLE N_("Interface")
#define INTF_HELP  N_( "Settings for VLC's interfaces" )

#define INTF_GENERAL_HELP N_( "General interface setttings" )

#define INTF_MAIN_TITLE  N_( "Main interfaces" )
#define INTF_MAIN_HELP N_( "Settings for the main interface" )

#define INTF_CONTROL_TITLE N_( "Control interfaces" )
#define INTF_CONTROL_HELP N_( "Settings for VLC's control interfaces" )

#define INTF_HOTKEYS_TITLE N_( "Hotkeys settings" )
#define INTF_HOTKEYS_HELP N_( "Hotkeys settings" )

/* Audio */
#define AUDIO_TITLE N_( "Audio" )
#define AUDIO_HELP N_( "Audio settings" )

#define AUDIO_GENERAL_TITLE N_( "General audio settings" )
#define AUDIO_GENERAL_HELP N_("General audio settings")

#define AFILTER_TITLE N_("Filters")
#define AFILTER_HELP N_( \
    "Audio filters are used to postprocess the audio stream." )

#define AVISUAL_TITLE N_("Visualizations")
#define AVISUAL_HELP N_( \
    "Audio visualizations" )

#define AOUT_TITLE N_( "Output modules" )
#define AOUT_HELP N_("These are general settings for audio output modules.")

#define AMISC_TITLE N_("Miscellaneous")
#define AMISC_HELP N_( "Miscellaneous audio settings and modules." )

/* Video */
#define VIDEO_TITLE N_("Video")
#define VIDEO_HELP N_("Video settings")

#define VIDEO_GENERAL_TITLE N_( "General video settings")
#define VIDEO_GENERAL_HELP N_( "General video settings" )

#define _VOUT_TITLE N_("Output modules" )
#define VOUT_HELP N_( \
    "Choose your preferred video output and configure it here." )

#define VFILTER_TITLE N_("Filters" )
#define VFILTER_HELP N_( \
    "Video filters are used to postprocess the video stream." )

#define SUBPIC_TITLE N_( "Subtitles/OSD")
#define SUBPIC_HELP N_( "Miscellaneous settings related to On-Screen-Display,"\
        " subtitles and \"overlay subpictures\".")
/*
#define TEXT_TITLE N_("Text rendering")
#define TEXT_HELP N_( \
    "Use the settings of the \"freetype\" module to choose the font you " \
    "want VLC to use for text rendering (to display subtitles for example).")
*/
/* Input */
#define INPUT_TITLE N_( "Input / Codecs" )
#define INPUT_HELP N_( "These are the settings for the input, demultiplexing " \
         "and decoding parts of VLC. Encoder settings can also be found here." )

#define ACCESS_TITLE N_( "Access modules" )
#define ACCESS_HELP N_( \
    "Settings related to the various access methods used by VLC. " \
    "Common settings you may want to alter are HTTP proxy or " \
    "caching settings." )

#define ACCESS_FILTER_TITLE N_( "Access filters" )
#define ACCESS_FILTER_HELP N_( \
    "Access filters are special modules that allow advanced operations on " \
    "the input side of VLC. You should not touch anything here unless you " \
    "know what you are doing." )

#define DEMUX_TITLE N_("Demuxers")
#define DEMUX_HELP N_( "Demuxers are used to separate audio and video streams." )

#define VDEC_TITLE  N_( "Video codecs" )
#define VDEC_HELP N_( "Settings for the video-only decoders and encoders." )

#define ADEC_TITLE  N_( "Audio codecs" )
#define ADEC_HELP N_( "Settings for the audio-only decoders and encoders." )

#define SDEC_TITLE N_( "Other codecs")
#define SDEC_HELP N_( "Settings for audio+video and miscellaneous decoders and encoders." )

#define ADVANCED_TITLE N_("General")
#define ADVANCED_HELP N_( "General input settings. Use with care." )

/* Sout */
#define SOUT_TITLE N_( "Stream output" )
#define SOUT_HELP N_( \
      "Stream output is what allows VLC to act as a streaming server " \
      "or to save incoming streams.\n" \
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

#define SOUT_SAP_TITLE N_( "SAP" )
#define SOUT_SAP_HELP N_( \
                 "SAP is a way to publically announce streams that are being "\
                 "sent using multicast UDP or RTP." )

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
#define AADVANCED_HELP N_( "Advanced settings. Use with care.")

#define CPU_TITLE N_( "CPU features" )
#define CPU_HELP N_( "You can choose to disable some CPU accelerations " \
        "here. You should probably not change these settings." )

#define MISC_TITLE N_( "Advanced settings" )
#define MISC_HELP N_( "Other advanced settings")

#define NETWORK_TITLE N_( "Network" )
#define NETWORK_HELP N_( "These modules provide network functions to all " \
                "other parts of VLC." )

/* OLD */

#define CHROMA_TITLE N_("Chroma modules settings")
#define CHROMA_HELP N_("These settings affect chroma transformation modules.")

#define PACKETIZER_TITLE  N_("Packetizer modules settings" )
#define PACKETIZER_HELP "These are general settings for the "\
        "packetizers used in VLC's stream output subsystem."

#define ENCODER_TITLE N_("Encoders settings")
#define ENCODER_HELP N_( \
    "These are general settings for video/audio/subtitles encoding modules.")


#define DIALOGS_TITLE N_("Dialog providers settings")
#define DIALOGS_HELP  N_( \
    "Dialog providers can be configured here.")

#define SUBTITLE_DEMUX_TITLE N_("Subtitle demuxer settings")
#define SUBTITLE_DEMUX_HELP N_( \
    "In this section you can force the behavior of the subtitle demuxer, " \
    "for example by setting the subtitles type or file name.")

#define VIDEO_FILTER2_TITLE N_("Video filters settings")
#define VIDEO_FILTER2_HELP " "

/*
 *  A little help for modules with unknown capabilities
 */

#define UNKNOWN_TITLE N_("No help available" )
#define UNKNOWN_HELP N_("There is no help available for these modules.")

/*****************************************************************************
 * GetCapabilityHelp: Display the help for one capability.
 *****************************************************************************/
static inline char * GetCapabilityHelp( char *psz_capability, int i_type)
{
/*
    if( psz_capability == NULL) return " ";

    if( !strcasecmp(psz_capability,"access_demux") )
        return i_type == 1 ? ACCESS_TITLE : ACCESS_HELP;
    if( !strcasecmp(psz_capability,"access2") )
        return i_type == 1 ? ACCESS_TITLE : ACCESS_HELP;
    if( !strcasecmp(psz_capability,"audio filter") )
        return i_type == 1 ? AUDIO_FILTER_TITLE : AUDIO_FILTER_HELP;
    if( !strcasecmp(psz_capability,"audio filter2") )
        return i_type == 1 ? AUDIO_FILTER2_TITLE : AUDIO_FILTER2_HELP;
    if( !strcasecmp(psz_capability,"audio output") )
        return i_type == 1 ? AOUT_TITLE : AOUT_HELP;
    if( !strcasecmp(psz_capability,"chroma") )
        return i_type == 1 ? CHROMA_TITLE : CHROMA_HELP;
    if( !strcasecmp(psz_capability,"decoder") )
        return i_type == 1 ? DECODER_TITLE : DECODER_HELP;
    if( !strcasecmp(psz_capability,"packetizer") )
        return i_type == 1 ? PACKETIZER_TITLE : PACKETIZER_HELP;
    if( !strcasecmp(psz_capability,"encoder") )
        return i_type == 1 ? ENCODER_TITLE : ENCODER_HELP;
    if( !strcasecmp(psz_capability,"demux2") )
        return i_type == 1 ? DEMUX_TITLE : DEMUX_HELP;
    if( !strcasecmp(psz_capability,"interface") )
        return i_type == 1 ? INTERFACE_TITLE : INTERFACE_HELP;
    if( !strcasecmp(psz_capability,"dialogs provider") )
        return i_type == 1 ? DIALOGS_TITLE : DIALOGS_HELP;
    if( !strcasecmp(psz_capability,"network") )
        return i_type == 1 ? NETWORK_TITLE : NETWORK_HELP;
    if( !strcasecmp(psz_capability,"sout access") )
        return i_type == 1 ? SOUT_ACCESS_TITLE : SOUT_ACCESS_HELP;
    if( !strcasecmp(psz_capability,"sout mux") )
        return i_type == 1 ? SOUT_MUX_TITLE : SOUT_MUX_HELP;
    if( !strcasecmp(psz_capability,"sout stream") )
        return i_type == 1 ? SOUT_STREAM_TITLE : SOUT_STREAM_HELP;
    if( !strcasecmp(psz_capability,"subtitle demux") )
        return i_type == 1 ? SUBTITLE_DEMUX_TITLE : SUBTITLE_DEMUX_HELP;
    if( !strcasecmp(psz_capability,"text renderer") )
        return i_type == 1 ? TEXT_TITLE : TEXT_HELP;
    if( !strcasecmp(psz_capability,"video output") )
        return i_type == 1 ? _VOUT_TITLE : VOUT_HELP;
    if( !strcasecmp(psz_capability,"video filter") )
        return i_type == 1 ? VIDEO_FILTER_TITLE : VIDEO_FILTER_HELP;
    if( !strcasecmp(psz_capability,"video filter2") )
        return i_type == 1 ? VIDEO_FILTER2_TITLE : VIDEO_FILTER2_HELP;

    */
    return " ";
}


static struct config_category_t categories_array[] =
{
    /* Interface */
    { CAT_INTERFACE, INTF_TITLE, INTF_HELP },
    { SUBCAT_INTERFACE_GENERAL, GENERAL_TITLE, INTF_GENERAL_HELP },
    { SUBCAT_INTERFACE_MAIN, INTF_MAIN_TITLE, INTF_MAIN_HELP },
    { SUBCAT_INTERFACE_CONTROL, INTF_CONTROL_TITLE, INTF_CONTROL_HELP },
    { SUBCAT_INTERFACE_HOTKEYS, INTF_HOTKEYS_TITLE, INTF_HOTKEYS_HELP },

    { CAT_AUDIO, AUDIO_TITLE, AUDIO_HELP },
    { SUBCAT_AUDIO_GENERAL, AUDIO_GENERAL_TITLE, AUDIO_GENERAL_HELP },
    { SUBCAT_AUDIO_AOUT, AOUT_TITLE, AOUT_HELP },
    { SUBCAT_AUDIO_AFILTER, AFILTER_TITLE, AFILTER_HELP },
    { SUBCAT_AUDIO_VISUAL, AVISUAL_TITLE, AVISUAL_HELP },
    { SUBCAT_AUDIO_MISC, AMISC_TITLE, AMISC_HELP },

    { CAT_VIDEO, VIDEO_TITLE, VIDEO_HELP },
    { SUBCAT_VIDEO_GENERAL, VIDEO_GENERAL_TITLE, VIDEO_GENERAL_HELP },
    { SUBCAT_VIDEO_VOUT, _VOUT_TITLE, VOUT_HELP },
    { SUBCAT_VIDEO_VFILTER, VFILTER_TITLE, VFILTER_HELP },
    { SUBCAT_VIDEO_SUBPIC, SUBPIC_TITLE, SUBPIC_HELP },

    { CAT_INPUT, INPUT_TITLE, INPUT_HELP },
    { SUBCAT_INPUT_GENERAL, ADVANCED_TITLE, ADVANCED_HELP },
    { SUBCAT_INPUT_ACCESS, ACCESS_TITLE, ACCESS_HELP },
    { SUBCAT_INPUT_ACCESS_FILTER, ACCESS_FILTER_TITLE, ACCESS_FILTER_HELP },
    { SUBCAT_INPUT_DEMUX, DEMUX_TITLE, DEMUX_HELP },
    { SUBCAT_INPUT_VCODEC, VDEC_TITLE, VDEC_HELP },
    { SUBCAT_INPUT_ACODEC, ADEC_TITLE, ADEC_HELP },
    { SUBCAT_INPUT_SCODEC, SDEC_TITLE, SDEC_HELP },

    { CAT_SOUT, SOUT_TITLE, SOUT_HELP },
    { SUBCAT_SOUT_GENERAL, GENERAL_TITLE, SOUT_GENERAL_HELP },
    { SUBCAT_SOUT_STREAM, SOUT_STREAM_TITLE, SOUT_STREAM_HELP },
    { SUBCAT_SOUT_MUX, SOUT_MUX_TITLE, SOUT_MUX_HELP },
    { SUBCAT_SOUT_ACO, SOUT_ACO_TITLE, SOUT_ACO_HELP },
    { SUBCAT_SOUT_PACKETIZER, SOUT_PACKET_TITLE, SOUT_PACKET_HELP },
    { SUBCAT_SOUT_SAP, SOUT_SAP_TITLE, SOUT_SAP_HELP },
    { SUBCAT_SOUT_VOD, SOUT_VOD_TITLE, SOUT_VOD_HELP },

    { CAT_PLAYLIST, PLAYLIST_TITLE , PLAYLIST_HELP },
    { SUBCAT_PLAYLIST_GENERAL, GENERAL_TITLE, PGENERAL_HELP },
    { SUBCAT_PLAYLIST_SD, SD_TITLE, SD_HELP },

    { CAT_ADVANCED, AADVANCED_TITLE, AADVANCED_HELP },
    { SUBCAT_ADVANCED_CPU, CPU_TITLE, CPU_HELP },
    { SUBCAT_ADVANCED_MISC, MISC_TITLE, MISC_HELP },

    { -1, NULL, NULL }
};


inline char *config_CategoryNameGet( int i_value )
{
    int i = 0 ;
    while( categories_array[i].psz_name != NULL )
    {
        if( categories_array[i].i_id == i_value )
        {
            return categories_array[i].psz_name;
        }
        i++;
    }
    return NULL;
}

inline char *config_CategoryHelpGet( int i_value )
{
    int i = 0 ;
    while( categories_array[i].psz_help != NULL )
    {
        if( categories_array[i].i_id == i_value )
        {
            return categories_array[i].psz_help;
        }
        i++;
    }
    return NULL;
}

#endif /* VLC_HELP_H */
