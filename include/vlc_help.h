/*****************************************************************************
 * vlc_help.h: Help strings
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: vlc_help.h,v 1.1 2003/09/22 14:40:10 zorglub Exp $
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

#define GENERAL_HELP N_( \
    "VLC Preferences. \nConfigure some global options in General Settings" \
     "\n and configure each VLC plugin in the Plugins section.\n" \
     "Click on 'Advanced Options' to see every options." )
#define PLUGIN_HELP N_( \
    "In this tree, you can set options for every plugin used by VLC.\n" \
    "Plugins are sorted by type.\nHave fun tuning VLC !" )
    
/*
 *  Then, help for each module capabilities.
 */

#define ACCESS_HELP N_( \
    "Access modules settings\n" \
    "Settings related to the various access methods used by VLC\n" \
    "Common settings you may want to alter are http proxy or\n" \
    "caching settings" )

#define AUDIO_FILTER_HELP N_("Audio filters settings\n" \
    "Audio filters can be set in the Audio section, and configured\n" \
    "here.")

#define AOUT_HELP N_("Audio output modules settings")

#define CHROMA_HELP N_("Chroma modules settings")

#define DECODER_HELP N_( \
    "Decoder modules settings\n" \
    "In the Subsdec section you may want to set your preferred subtitles\n" \
    "text encoding\n")

#define DEMUX_HELP N_( \
    "Demuxer settings")

#define INTERFACE_HELP  N_( \
    "Interface plugins settings\n" \
    "Interface plugins can be enabled in the Interface section and\n" \
    "configured here.")

#define SOUT_HELP N_( \
    "Stream output access modules settings\n" \
    "In this section you can set the caching value for the UDP stream\n" \
    "output access module")

#define SUBTITLE_DEMUX_HELP N_( \
    "Subtitle demuxer settings\n" \
    "In this section you can force the behaviour of the subtitle demuxer,\n" \
    "for example by setting the subtitles type or file name.")

#define TEXT_HELP N_( \
    "Text renderer settings\n" \
    "Use these settings to choose the font you want VLC to use for text\n" \
    "rendering (to display subtitles for example)")

#define VOUT_HELP N_( \
    "Video output modules settings\n" \
    "Choose your preferred video output in the Video section, \n" \
    "and configure it here." )

#define VIDEO_FILTER_HELP N_( \
    "Video filters settings\n" \
    "Video filters can be enabled in the Video section and configured" \
    "here. Configure the \"adjust\" filter to modify \n" \
    "contrast/hue/saturation settings.")

/*
 *  A little help for modules with unknown capabilities
 */

#define UNKNOWN_HELP N_("No help available")


#endif /* VLC_HELP_H */
