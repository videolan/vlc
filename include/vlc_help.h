/*****************************************************************************
 * vlc_help.h: Help strings
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _VLC_HELP_H
#define _VLC_HELP_H 1

/*
 *  First, we need help strings for the General Settings and for the
 *  Plugins screen
 */
#define GENERAL_TITLE N_( "VLC preferences" )
#define GENERAL_HELP N_( \
    "Configure the global options in General Settings " \
    "and configure each VLC module in the Modules section.\n" \
    "Click on \"Advanced Options\" to see all options." )

#define PLUGIN_TITLE N_( "VLC modules preferences" )
#define PLUGIN_HELP N_( \
    "In this tree, you can set options for every module used by VLC.\n" \
    "Modules are sorted by type." )

/*
 *  Then, help for each module capabilities.
 */

#define ACCESS_TITLE N_( "Access modules settings" )
#define ACCESS_HELP N_( \
    "Settings related to the various access methods used by VLC.\n" \
    "Common settings you may want to alter are HTTP proxy or " \
    "caching settings." )

#define AUDIO_FILTER_TITLE N_("Audio filters settings")
#define AUDIO_FILTER_HELP N_( \
    "Audio filters can be set in the Audio section, and configured " \
    "here.")

#define AUDIO_FILTER2_TITLE N_("Audio filters settings")
#define AUDIO_FILTER2_HELP N_(" ")

#define AOUT_TITLE N_("Audio output modules settings")
#define AOUT_HELP N_("These are general settings for audio output modules.")

#define CHROMA_TITLE N_("Chroma modules settings")
#define CHROMA_HELP N_("These settings affect chroma transformation modules.")

#define DECODER_TITLE  N_("Decoder modules settings" )
#define DECODER_HELP N_( \
    "In the Subsdec section you may want to set the text encoding of your " \
    "preferred subtitles.")

#define PACKETIZER_TITLE  N_("Packetizer modules settings" )
#define PACKETIZER_HELP N_(" ")

#define ENCODER_TITLE N_("Encoders settings")
#define ENCODER_HELP N_( \
    "These are general settings for video/audio/subtitles encoding modules.")

#define DEMUX_TITLE N_("Demuxers settings")
#define DEMUX_HELP N_( "These settings affect demuxer modules.")

#define INTERFACE_TITLE N_("Interface plugins settings")
#define INTERFACE_HELP  N_( \
    "Interface plugins can be enabled in the Interface section and " \
    "configured here.")

#define DIALOGS_TITLE N_("Dialog providers settings")
#define DIALOGS_HELP  N_( \
    "Dialog providers can be configured here.")

#define NETWORK_TITLE N_("Network modules settings")
#define NETWORK_HELP N_(" ")

#define SOUT_ACCESS_TITLE N_("Stream output access modules settings")
#define SOUT_ACCESS_HELP N_( \
    "In this section you can set the caching value for the UDP stream " \
    "output access module.")

#define SOUT_MUX_TITLE N_("Stream output muxer modules settings")
#define SOUT_MUX_HELP N_(" ")

#define SOUT_STREAM_TITLE N_("Stream output modules settings")
#define SOUT_STREAM_HELP N_(" ")

#define SUBTITLE_DEMUX_TITLE N_("Subtitle demuxer settings")
#define SUBTITLE_DEMUX_HELP N_( \
    "In this section you can force the behavior of the subtitle demuxer, " \
    "for example by setting the subtitles type or file name.")

#define TEXT_TITLE N_("Text renderer settings")
#define TEXT_HELP N_( \
    "Use these settings to choose the font you want VLC to use for text " \
    "rendering (to display subtitles for example).")

#define _VOUT_TITLE N_("Video output modules settings")
#define VOUT_HELP N_( \
    "Choose your preferred video output in the Video section, " \
    "and configure it here." )

#define VIDEO_FILTER_TITLE N_("Video filters settings")
#define VIDEO_FILTER_HELP N_( \
    "Video filters can be enabled in the Video section and configured " \
    "here.\n" \
    "Configure the \"adjust\" filter to modify contrast/hue/saturation " \
    "settings.")

#define VIDEO_FILTER2_TITLE N_("Video filters settings")
#define VIDEO_FILTER2_HELP N_(" ")

/*
 *  A little help for modules with unknown capabilities
 */

#define UNKNOWN_TITLE N_("No help available" )
#define UNKNOWN_HELP N_("No help is available for these modules")

/*****************************************************************************
 * GetCapabilityHelp: Display the help for one capability.
 *****************************************************************************/
static inline char * GetCapabilityHelp( char *psz_capability, int i_type)
{
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

    return " ";
}

#endif /* VLC_HELP_H */
