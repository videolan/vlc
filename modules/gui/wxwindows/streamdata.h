/*****************************************************************************
 * streamdata.h: streaming/transcoding data
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
 * $Id: wizard.cpp 7826 2004-05-30 14:43:12Z zorglub $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/


#define MUX_PS          0
#define MUX_TS          1
#define MUX_MPEG        2
#define MUX_OGG         3
#define MUX_RAW         4
#define MUX_ASF         5
#define MUX_AVI         6
#define MUX_MP4         7
#define MUX_MOV         8

/* Muxer / Codecs / Access_out compatibility tables */


struct codec {
    char *psz_display;
    char *psz_codec;
    char *psz_descr;
    int muxers[9];
};

static struct codec vcodecs_array[] =
{
    { "MPEG-1 Video" , "mp1v" , "MPEG-1 Video codec",
       {MUX_PS, MUX_TS, MUX_MPEG, MUX_OGG, MUX_AVI, MUX_RAW, -1,-1,-1 } },
    { "MPEG-2 Video" , "mp2v" , "MPEG-2 Video codec",
       {MUX_PS, MUX_TS, MUX_MPEG, MUX_OGG, MUX_AVI, MUX_RAW, -1,-1,-1 } },
    { "MPEG-4 Video" , "mp4v" , "MPEG-4 Video codec",
       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_MP4,MUX_OGG,MUX_AVI,MUX_RAW, -1} },
    { "DIVX 1" ,"DIV1","Divx first version" ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , MUX_AVI , -1,-1,-1,-1 } },
    { "DIVX 2" ,"DIV2","Divx second version" ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , MUX_AVI , -1,-1,-1,-1 } },
    { "DIVX 3" ,"DIV3","Divx third version" ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , MUX_AVI , -1,-1,-1,-1 } },
    { "H 263" , "H263" , "H263 is ..." ,
       { MUX_TS, MUX_AVI, -1,-1,-1,-1,-1,-1,-1 } },
    { "I 263", "I263", "I263 is ..." ,
       { MUX_TS, MUX_AVI, -1,-1,-1,-1,-1,-1,-1 } },
    { "WMV 1" , "WMV1", "First version of WMV" ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , MUX_AVI , -1,-1,-1,-1 } },
    { "WMV 2" , "WMV2", "2 version of WMV" ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , MUX_AVI , -1,-1,-1,-1 } },
    { "MJPEG" , "MJPG", "MJPEG consists of a series of JPEG pictures" ,
       {MUX_TS , MUX_MPEG , MUX_ASF , MUX_OGG , MUX_AVI , -1,-1,-1,-1 } },
    { "Theora" , "theo", "Experimental free codec",
       {MUX_TS, -1,-1,-1,-1,-1,-1,-1,-1} },
    { "Dummy", "dummy", "Dummy codec (do not transcode)" ,
      {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_MP4,MUX_OGG,MUX_AVI,MUX_RAW,MUX_MOV}},
    { NULL,NULL,NULL , {-1,-1,-1,-1,-1,-1,-1,-1,-1}} /* Do not remove me */
};

static struct codec acodecs_array[] =
{
    { "MPEG Audio" , "mpga" , "The standard MPEG audio (1/2) format" ,
       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_OGG,MUX_AVI,MUX_RAW, -1,-1} },
    { "MP3" , "mp3" , "MPEG Audio Layer 3" ,
       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_OGG,MUX_AVI,MUX_RAW, -1,-1} },
    { "MPEG 4 Audio" , "mp4a" , "Audio format for MPEG4" ,
       {MUX_TS, MUX_MP4, -1,-1,-1,-1,-1,-1,-1 } },
    { "A/52" , "a52" , "DVD audio format" ,
       {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_OGG,MUX_AVI,MUX_RAW, -1,-1} },
    { "Vorbis" , "vorb" , "This is a free audio codec" ,
       {MUX_OGG, -1,-1,-1,-1,-1,-1,-1,-1} },
    { "FLAC" , "flac" , "This is an audio codec" ,
       {MUX_OGG , MUX_RAW, -1,-1,-1,-1,-1,-1,-1} },
    { "Speex" , "spx" , "An audio codec dedicated to compression of voice" ,
       {MUX_OGG, -1,-1,-1,-1,-1,-1,-1,-1} },
    { "Dummy", "dummy", "Dummy codec (do not transcode)" ,
     {MUX_PS,MUX_TS,MUX_MPEG,MUX_ASF,MUX_MP4,MUX_OGG,MUX_AVI,MUX_RAW,MUX_MOV}},
    { NULL,NULL,NULL , {-1,-1,-1,-1,-1,-1,-1,-1,-1}} /* Do not remove me */
};

struct method {
    char *psz_access;
    char *psz_method;
    char *psz_descr;
    char *psz_address;
    int   muxers[9];
};

static struct method methods_array[] =
{
    {"udp:","UDP Unicast", "Use this to stream to a single computer",
     "Enter the address of the computer to stream to",
     { MUX_TS, -1,-1,-1,-1,-1,-1,-1,-1 } },
    {"udp:","UDP Multicast",
     "Use this to stream to a dynamic group of computers on a "
     "multicast-enabled network. This is the most efficient method "
     "to stream to several computers, but it does not work over Internet.",
     "Enter the multicast address to stream to in this field. "
     "This must be an IP address between 224.0.0.0 an 239.255.255.255 "
     "For a private use, enter an address beginning with 239.255.",
     { MUX_TS, -1,-1,-1,-1,-1,-1,-1,-1 } },
    {"http://","HTTP",
            "Use this to stream to several computers. This method is "
     "less efficient, as the server needs to send several times the "
     "stream.",
     "Enter the local addresses you want to listen to. Do not enter "
     "anything if you want to listen to all adresses or if you don't "
     "understand. This is generally the best thing to do. Other computers "
     "can then access the stream at http://yourip:8080 by default",
     { MUX_TS, MUX_PS, MUX_MPEG, MUX_OGG, MUX_RAW, MUX_ASF, -1,-1,-1} },
    { NULL, NULL,NULL,NULL , {-1,-1,-1,-1,-1,-1,-1,-1,-1}} /* Do not remove me */
};

struct encap {
    int   id;
    char *psz_mux;
    char *psz_encap;
    char *psz_descr;
};

static struct encap encaps_array[] =
{
    { MUX_PS, "ps","MPEG PS", "MPEG Program Stream" },
    { MUX_TS, "ts","MPEG TS", "MPEG Transport Stream" },
    { MUX_MPEG, "ps", "MPEG 1", "MPEG 1 Format" },
    { MUX_OGG, "ogg", "OGG", "OGG" },
    { MUX_RAW, "raw", "RAW", "RAW" },
    { MUX_ASF, "asf","ASF", "ASF" },
    { MUX_AVI, "avi","AVI", "AVI" },
    { MUX_MP4, "mp4","MP4", "MPEG4" },
    { MUX_MOV, "mov","MOV", "MOV" },
    { -1 , NULL,NULL , NULL } /* Do not remove me */
};


/* Bitrates arrays */
    static const wxString vbitrates_array[] =
    {
        wxT("3072"),
        wxT("2048"),
        wxT("1024"),
        wxT("768"),
        wxT("512"),
        wxT("384"),
        wxT("256"),
        wxT("192"),
        wxT("128"),
        wxT("96"),
        wxT("64"),
        wxT("32"),
        wxT("16")
    };
    static const wxString abitrates_array[] =
    {
        wxT("512"),
        wxT("256"),
        wxT("192"),
        wxT("128"),
        wxT("96"),
        wxT("64"),
        wxT("32"),
        wxT("16")
    };

