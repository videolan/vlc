/*****************************************************************************
 * vlc_help.h: Help strings
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: vlc_help.h,v 1.5 2003/12/03 12:33:21 sam Exp $
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
#define GENERAL_TITLE N_( "VLC Preferences" )
#define GENERAL_HELP N_( \
    "Configure some global options in General Settings " \
     "and configure each VLC plugin in the Plugins section.\n" \
     "Click on \"Advanced Options\" to see all options." )

#define PLUGIN_TITLE N_( "VLC Plugins Preferences" )
#define PLUGIN_HELP N_( \
    "In this tree, you can set options for every plugin used by VLC.\n" \
    "Plugins are sorted by type.\nHave fun tuning VLC!" )

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

#define AOUT_TITLE N_("Audio output modules settings")
#define AOUT_HELP N_("These are general settings for audio output modules.")

#define AOUT_ENC_TITLE N_("Audio encoders settings")
#define AOUT_ENC_HELP N_("These are general settings for audio encoding modules.")

#define CHROMA_TITLE N_("Chroma modules settings")
#define CHROMA_HELP N_(" ")

#define DECODER_TITLE  N_("Decoder modules settings" )
#define DECODER_HELP N_( \
    "In the Subsdec section you may want to set the text encoding of your " \
    "preferred subtitles.")

#define DEMUX_TITLE N_("Demuxers settings")
#define DEMUX_HELP N_( " ")

#define INTERFACE_TITLE N_("Interface plugins settings")
#define INTERFACE_HELP  N_( \
    "Interface plugins can be enabled in the Interface section and " \
    "configured here.")

#define SOUT_TITLE N_("Stream output access modules settings")
#define SOUT_HELP N_( \
    "In this section you can set the caching value for the UDP stream" \
    "output access module.")

#define SUBTITLE_DEMUX_TITLE N_("Subtitle demuxer settings")
#define SUBTITLE_DEMUX_HELP N_( \
    "In this section you can force the behavior of the subtitle demuxer, " \
    "for example by setting the subtitles type or file name.")

#define TEXT_TITLE N_("Text renderer settings")
#define TEXT_HELP N_( \
    "Use these settings to choose the font you want VLC to use for text " \
    "rendering (to display subtitles for example).")

#define VOUT__TITLE N_("Video output modules settings")
#define VOUT_HELP N_( \
    "Choose your preferred video output in the Video section, " \
    "and configure it here." )

#define VIDEO_FILTER_TITLE N_("Video filters settings")
#define VIDEO_FILTER_HELP N_( \
    "Video filters can be enabled in the Video section and configured " \
    "here.\n" \
    "Configure the \"adjust\" filter to modify contrast/hue/saturation " \
    " settings.")

/*
 *  A little help for modules with unknown capabilities
 */

#define UNKNOWN_TITLE N_("No help available" )
#define UNKNOWN_HELP N_("No help is available for these modules")

#endif /* VLC_HELP_H */
