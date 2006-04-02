/*****************************************************************************
 * streamdata.cpp: streaming/transcoding data
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include <vlc/vlc.h>
#include <wx/string.h>
#include "streamdata.h"


const struct codec vcodecs_array[] =
{
    { "MPEG-1 Video" , "mp1v" , N_("MPEG-1 Video codec (useable with MPEG PS, " \
        "MPEG TS, MPEG1, OGG and RAW)"),
//       {MUX_PS, MUX_TS, MUX_MPEG, MUX_OGG, MUX_AVI, MUX_RAW, -1,-1,-1 } },
       {MUX_PS, MUX_TS, MUX_MPEG, MUX_OGG, MUX_RAW, -1,-1,-1,-1 } },
    { "MPEG-2 Video" , "mp2v" , N_("MPEG-2 Video codec (useable with MPEG PS, " \
        "MPEG TS, MPEG1, OGG and RAW)"),
//       {MUX_PS, MUX_TS, MUX_MPEG, MUX_OGG, MUX_AVI, MUX_RAW, -1,-1,-1 } },
       {MUX_PS, MUX_TS, MUX_MPEG, MUX_OGG, MUX_RAW, -1,-1,-1,-1 } },
    { "MPEG-4 Video" , "mp4v" , N_("MPEG-4 Video codec (useable with MPEG PS, " \
        "MPEG TS, MPEG1, ASF, MPEG4, OGG and RAW)"),
//       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_MP4,MUX_OGG,MUX_AVI,MUX_RAW, -1} },
       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_MP4,MUX_OGG,MUX_RAW, -1,-1} },
    { "DIVX 1" ,"DIV1",N_("DivX first version (useable with MPEG TS, MPEG1, ASF" \
        " and OGG)") ,
//       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , MUX_AVI , -1,-1,-1,-1 } },
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , -1 , -1,-1,-1,-1 } },
    { "DIVX 2" ,"DIV2",N_("DivX second version (useable with MPEG TS, MPEG1, ASF" \
        " and OGG)") ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , -1 , -1,-1,-1,-1 } },
    { "DIVX 3" ,"DIV3",N_("DivX third version (useable with MPEG TS, MPEG1, ASF" \
        " and OGG)") ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , -1 , -1,-1,-1,-1 } },
    { "H 263" , "h263" , N_("H263 is a video codec optimized for videoconference " \
        "(low rates, useable with MPEG TS)") ,
       { MUX_TS, -1, -1,-1,-1,-1,-1,-1,-1 } },
    { "H 264" , "h264" , N_("H264 is a new video codec (useable with MPEG TS " \
        "and MPEG4)") ,
       { MUX_TS, MUX_MP4, MUX_ASF,-1,-1,-1,-1,-1,-1 } },
    { "WMV 1" , "WMV1", N_("WMV (Windows Media Video) 7 (useable with MPEG TS, " \
        "MPEG1, ASF and OGG)") ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , -1 , -1,-1,-1,-1 } },
    { "WMV 2" , "WMV2", N_("WMV (Windows Media Video) 8 (useable with MPEG TS, " \
        "MPEG1, ASF and OGG)") ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , -1 , -1,-1,-1,-1 } },
#ifdef WIN32
    { "WMV 3" , "WMV3", N_("WMV (Windows Media Video) 9 (useable with MPEG TS, " \
        "MPEG1, ASF and OGG)") ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , -1 , -1,-1,-1,-1 } },
#endif
    { "MJPEG" , "MJPG", N_("MJPEG consists of a series of JPEG pictures " \
        "(useable with MPEG TS, MPEG1, ASF and OGG)") ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , -1 , -1,-1,-1,-1 } },
    { "Theora" , "theo", N_("Theora is a free general-purpose codec (useable " \
        "with MPEG TS)"),
       {MUX_TS, MUX_OGG,-1,-1,-1,-1,-1,-1,-1} },
    { "Dummy", "dummy", N_("Dummy codec (do not transcode, useable with all " \
        "encapsulation formats)") ,
      {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_MP4,MUX_OGG,MUX_WAV,MUX_RAW,MUX_MOV}},
    { NULL,NULL,NULL , {-1,-1,-1,-1,-1,-1,-1,-1,-1}} /* Do not remove me */
};

const struct codec acodecs_array[] =
{
    { "MPEG Audio" , "mpga" , N_("The standard MPEG audio (1/2) format " \
        "(useable with MPEG PS, MPEG TS, MPEG1, ASF, OGG and RAW)") ,
//       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_OGG,MUX_AVI,MUX_RAW, -1,-1} },
       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_OGG,MUX_RAW, -1,-1,-1} },
    { "MP3" , "mp3" , N_("MPEG Audio Layer 3 (useable with MPEG PS, MPEG TS, " \
        "MPEG1, ASF, OGG and RAW)") ,
//       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_OGG,MUX_AVI,MUX_RAW, -1,-1} },
       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_OGG,MUX_RAW, -1,-1, -1} },
    { "MPEG 4 Audio" , "mp4a" , N_("Audio format for MPEG4 (useable with " \
        "MPEG TS and MPEG4)") ,
       {MUX_TS, MUX_MP4, -1,-1,-1,-1,-1,-1,-1 } },
    { "A/52" , "a52" , N_("DVD audio format (useable with MPEG PS, MPEG TS, " \
        "MPEG1, ASF, OGG and RAW)") ,
//       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_OGG,MUX_AVI,MUX_RAW, -1,-1} },
       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_OGG,MUX_RAW, -1,-1,-1} },
    { "Vorbis" , "vorb" , N_("Vorbis is a free audio codec (useable with OGG)") ,
       {MUX_OGG, -1,-1,-1,-1,-1,-1,-1,-1} },
    { "FLAC" , "flac" , N_("FLAC is a lossless audio codec (useable with OGG " \
        "and RAW)") ,
       {MUX_OGG , MUX_RAW, -1,-1,-1,-1,-1,-1,-1} },
    { "Speex" , "spx" , N_("A free audio codec dedicated to compression of " \
        "voice (useable with OGG)") ,
       {MUX_OGG, -1,-1,-1,-1,-1,-1,-1,-1} },
    { "Uncompressed, integer" , "s16l" , N_("Uncompressed audio samples " \
        "(useable with WAV)"),
       {MUX_WAV, -1,-1,-1,-1,-1,-1,-1,-1} },
    { "Uncompressed, floating" , "fl32" , N_("Uncompressed audio samples " \
        "(useable with WAV)"),
       {MUX_WAV, -1,-1,-1,-1,-1,-1,-1,-1} },
    { "Dummy", "dummy", N_("Dummy codec (do not transcode, useable with all " \
        "encapsulation formats)") ,
//     {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_MP4,MUX_OGG,MUX_AVI,MUX_RAW,MUX_MOV}},
     {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_MP4,MUX_OGG,MUX_RAW,MUX_MOV,MUX_WAV}},
    { NULL,NULL,NULL , {-1,-1,-1,-1,-1,-1,-1,-1,-1}} /* Do not remove me */
};

const struct method methods_array[] =
{
    {"rtp",N_("RTP Unicast"), N_("Stream to a single computer."),
     N_("Enter the address of the computer to stream to."),
     { MUX_TS, -1,-1,-1,-1,-1,-1,-1,-1 } },
    {"rtp",N_("RTP Multicast"),
     N_("Stream to a dynamic group of computers on a "
     "multicast-enabled network. This is the most efficient method "
     "to stream to several computers, but it does not work over the Internet."),
     N_("Enter the multicast address to stream to. "
     "This must be an IP address between 224.0.0.0 an 239.255.255.255. "
     "For private use, enter an address beginning with 239.255."),
     { MUX_TS, -1,-1,-1,-1,-1,-1,-1,-1 } },
    {"http","HTTP",
     N_("Stream to several computers. This method is "
     "less efficient, as the server needs to send the "
     "stream several times."),
     N_("Enter the local addresses you want to listen to. Do not enter "
     "anything if you want to listen to all adresses or if you don't "
     "understand. This is generally the best thing to do. Other computers "
     "can then access the stream at http://yourip:8080 by default."),
     { MUX_TS, MUX_PS, MUX_MPEG, MUX_OGG, MUX_RAW, MUX_ASF, -1,-1,-1} },
    { NULL, NULL,NULL,NULL , {-1,-1,-1,-1,-1,-1,-1,-1,-1}} /* Do not remove me */
};


const struct encap encaps_array[] =
{
    { MUX_PS, "ps","MPEG PS", N_("MPEG Program Stream") },
    { MUX_TS, "ts","MPEG TS", N_("MPEG Transport Stream") },
    { MUX_MPEG, "ps", "MPEG 1", N_("MPEG 1 Format") },
    { MUX_OGG, "ogg", "OGG", "OGG" },
    { MUX_RAW, "raw", "RAW", "RAW" },
    { MUX_ASF, "asf","ASF", "ASF" },
//    { MUX_AVI, "avi","AVI", "AVI" },
    { MUX_MP4, "mp4","MP4", "MPEG4" },
    { MUX_MOV, "mov","MOV", "MOV" },
    { MUX_WAV, "wav","WAV", "WAV" },
    { -1 , NULL,NULL , NULL } /* Do not remove me */
};
