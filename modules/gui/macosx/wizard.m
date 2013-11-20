/*****************************************************************************
 * wizard.m: MacOS X Streaming Wizard
 *****************************************************************************
 * Copyright (C) 2005-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>,
 *          Brendon Justin <brendonjustin at gmail.com>
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
 * Note: this code is partially based upon ../wxwidgets/wizard.cpp and
 *         ../wxwidgets/streamdata.h; both written by ClÃ©ment Stenac.
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import "CompatibilityFixes.h"
#import "wizard.h"
#import "intf.h"
#import "playlist.h"
#import <vlc_interface.h>

/*****************************************************************************
 * VLCWizard implementation
 *****************************************************************************/

@implementation VLCWizard

static VLCWizard *_o_sharedInstance = nil;

+ (VLCWizard *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)dealloc
{
    [o_userSelections release];
    [o_videoCodecs release];
    [o_audioCodecs release];
    [o_encapFormats release];
    [super dealloc];
}

- (void)awakeFromNib
{
    if (!OSX_SNOW_LEOPARD)
        [o_wizard_window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    /* some minor cleanup */
    [o_t2_tbl_plst setEnabled:NO];
    o_userSelections = [[NSMutableDictionary alloc] init];
    [o_btn_backward setEnabled:NO];

    /* add audio-bitrates for transcoding */
    NSArray * audioBitratesArray;
    audioBitratesArray = [NSArray arrayWithObjects:@"512", @"256", @"192", @"128", @"64", @"32", @"16", nil];
    [o_t4_pop_audioBitrate removeAllItems];
    [o_t4_pop_audioBitrate addItemsWithTitles: audioBitratesArray];
    [o_t4_pop_audioBitrate selectItemWithTitle: @"192"];

    /* add video-bitrates for transcoding */
    NSArray * videoBitratesArray;
    videoBitratesArray = [NSArray arrayWithObjects:@"3072", @"2048", @"1024", @"768", @"512", @"256", @"192", @"128", @"64", @"32", @"16", nil];
    [o_t4_pop_videoBitrate removeAllItems];
    [o_t4_pop_videoBitrate addItemsWithTitles: videoBitratesArray];
    [o_t4_pop_videoBitrate selectItemWithTitle: @"1024"];

    /* fill 2 global arrays with arrays containing all codec-related information
     * - one array per codec named by its short name to define the encap-compability,
     *     cmd-names, real names, more info in the order: realName, shortName,
     *     moreInfo, encaps */
    NSArray * o_mp1v;
    NSArray * o_mp2v;
    NSArray * o_mp4v;
    NSArray * o_div1;
    NSArray * o_div2;
    NSArray * o_div3;
    NSArray * o_h263;
    NSArray * o_h264;
    NSArray * o_wmv1;
    NSArray * o_wmv2;
    NSArray * o_mjpg;
    NSArray * o_theo;
    NSArray * o_dummyVid;
    o_mp1v = [NSArray arrayWithObjects:@"MPEG-1 Video", @"mp1v",
               _NS("MPEG-1 Video codec (usable with MPEG PS, MPEG TS, MPEG1, OGG "
                   "and RAW)"), @"MUX_PS", @"MUX_TS", @"MUX_MPEG", @"MUX_OGG", @"MUX_RAW",
               @"NO", @"NO", @"NO", @"NO", nil];
    o_mp2v = [NSArray arrayWithObjects:@"MPEG-2 Video", @"mp2v",
               _NS("MPEG-2 Video codec (usable with MPEG PS, MPEG TS, MPEG1, OGG "
                   "and RAW)"), @"MUX_PS", @"MUX_TS", @"MUX_MPEG", @"MUX_OGG", @"MUX_RAW",
               @"NO", @"NO", @"NO", @"NO", nil];
    o_mp4v = [NSArray arrayWithObjects:@"MPEG-4 Video", @"mp4v",
               _NS("MPEG-4 Video codec (useable with MPEG PS, MPEG TS, MPEG1, ASF, "
                   "MP4, OGG and RAW)"), @"MUX_PS", @"MUX_TS", @"MUX_MPEG", @"MUX_ASF",
               @"MUX_MP4", @"MUX_OGG", @"MUX_RAW", @"NO", @"NO", nil];
    o_div1 = [NSArray arrayWithObjects:@"DIVX 1", @"DIV1",
               _NS("DivX first version (useable with MPEG TS, MPEG1, ASF and OGG)"),
               @"MUX_TS", @"MUX_MPEG", @"MUX_ASF", @"MUX_OGG", @"NO", @"NO", @"NO",
               @"NO", @"NO", nil];
    o_div2 = [NSArray arrayWithObjects:@"DIVX 2", @"DIV2",
               _NS("DivX second version (useable with MPEG TS, MPEG1, ASF and OGG)"),
               @"MUX_TS", @"MUX_MPEG", @"MUX_ASF", @"MUX_OGG", @"NO", @"NO", @"NO",
               @"NO", @"NO", nil];
    o_div3 = [NSArray arrayWithObjects:@"DIVX 3", @"DIV3",
               _NS("DivX third version (useable with MPEG TS, MPEG1, ASF and OGG)"),
               @"MUX_TS", @"MUX_MPEG", @"MUX_ASF", @"MUX_OGG", @"NO", @"NO", @"NO",
               @"NO", @"NO", nil];
    o_h263 = [NSArray arrayWithObjects:@"H.263", @"h263",
               _NS("H263 is a video codec optimized for videoconference "
                   "(low rates, useable with MPEG TS)"), @"MUX_TS", @"NO", @"NO", @"NO",
               @"NO", @"NO", @"NO", @"NO", @"NO", nil];
    o_h264 = [NSArray arrayWithObjects:@"H.264", @"h264",
               _NS("H264 is a new video codec (useable with MPEG TS and MP4)"),
               @"MUX_TS", @"MUX_MP4", @"NO", @"NO", @"NO", @"NO", @"NO", @"NO",
               @"NO", nil];
    o_wmv1 = [NSArray arrayWithObjects:@"WMV 1", @"WMV1",
               _NS("WMV (Windows Media Video) 1 (useable with MPEG TS, MPEG1, ASF and "
                   "OGG)"), @"MUX_TS", @"MUX_MPEG", @"MUX_ASF", @"MUX_OGG", @"NO", @"NO",
               @"NO", @"NO", @"NO", nil];
    o_wmv2 = [NSArray arrayWithObjects:@"WMV 2", @"WMV2",
               _NS("WMV (Windows Media Video) 2 (useable with MPEG TS, MPEG1, ASF and "
                   "OGG)"), @"MUX_TS", @"MUX_MPEG", @"MUX_ASF", @"MUX_OGG", @"NO", @"NO",
               @"NO", @"NO", @"NO", nil];
    o_mjpg = [NSArray arrayWithObjects:@"MJPEG", @"MJPG",
               _NS("MJPEG consists of a series of JPEG pictures (useable with MPEG TS,"
                   " MPEG1, ASF and OGG)"), @"MUX_TS", @"MUX_MPEG", @"MUX_ASF", @"MUX_OGG",
               @"NO", @"NO", @"NO", @"NO", @"NO", nil];
    o_theo = [NSArray arrayWithObjects:@"Theora", @"theo",
               _NS("Theora is a free general-purpose codec (useable with MPEG TS "
                   "and OGG)"), @"MUX_TS", @"MUX_OGG", @"NO", @"NO", @"NO", @"NO", @"NO",
               @"NO", @"NO", nil];
    o_dummyVid = [NSArray arrayWithObjects:@"Dummy", @"dummy",
                   _NS("Dummy codec (do not transcode, useable with all encapsulation "
                       "formats)"), @"MUX_PS", @"MUX_TS", @"MUX_MPEG", @"MUX_ASF", @"MUX_MP4",
                   @"MUX_OGG", @"MUX_WAV", @"MUX_RAW", @"MUX_MOV", nil];
    o_videoCodecs = [[NSArray alloc] initWithObjects: o_mp1v, o_mp2v, o_mp4v,
        o_div1, o_div2, o_div3, o_h263, o_h264, o_wmv1, o_wmv2, o_mjpg, o_theo,
        o_dummyVid, nil];


    NSArray * o_mpga;
    NSArray * o_mp3;
    NSArray * o_mp4a;
    NSArray * o_a52;
    NSArray * o_vorb;
    NSArray * o_flac;
    NSArray * o_spx;
    NSArray * o_s16l;
    NSArray * o_fl32;
    NSArray * o_dummyAud;
    o_mpga = [NSArray arrayWithObjects:@"MPEG Audio", @"mpga",
               _NS("The standard MPEG audio (1/2) format (useable with MPEG PS, MPEG TS, "
                   "MPEG1, ASF, OGG and RAW)"), @"MUX_PS", @"MUX_TS", @"MUX_MPEG",
               @"MUX_ASF", @"MUX_OGG", @"MUX_RAW", @"-1", @"-1", @"-1", nil];
    o_mp3 = [NSArray arrayWithObjects:@"MP3", @"mp3",
              _NS("MPEG Audio Layer 3 (useable with MPEG PS, MPEG TS, MPEG1, ASF, OGG "
                  "and RAW)"), @"MUX_PS", @"MUX_TS", @"MUX_MPEG", @"MUX_ASF", @"MUX_OGG",
              @"MUX_RAW", @"-1", @"-1", @"-1", nil];
    o_mp4a = [NSArray arrayWithObjects:@"MPEG 4 Audio", @"mp4a",
               _NS("Audio format for MPEG4 (useable with MPEG TS and MPEG4)"), @"MUX_TS",
               @"MUX_MP4", @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", nil];
    o_a52 = [NSArray arrayWithObjects:@"A/52", @"a52",
              _NS("DVD audio format (useable with MPEG PS, MPEG TS, MPEG1, ASF, OGG "
                  "and RAW)"), @"MUX_PS", @"MUX_TS", @"MUX_MPEG", @"MUX_ASF", @"MUX_OGG",
              @"MUX_RAW", @"-1", @"-1", @"-1", nil];
    o_vorb = [NSArray arrayWithObjects:@"Vorbis", @"vorb",
               _NS("Vorbis is a free audio codec (useable with OGG)"), @"MUX_OGG",
               @"-1",  @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", nil];
    o_flac = [NSArray arrayWithObjects:@"FLAC", @"flac",
               _NS("FLAC is a lossless audio codec (useable with OGG and RAW)"),
               @"MUX_OGG", @"MUX_RAW", @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", nil];
    o_spx = [NSArray arrayWithObjects:@"Speex", @"spx",
              _NS("A free audio codec dedicated to compression of voice (useable "
                  "with OGG)"), @"MUX_OGG", @"-1", @"-1", @"-1", @"-1", @"-1", @"-1",
              @"-1", @"-1", nil];
    o_s16l = [NSArray arrayWithObjects:@"Uncompressed, integer", @"s16l",
               _NS("Uncompressed audio samples (useable with WAV)"), @"MUX_WAV",
               @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", nil];
    o_fl32 = [NSArray arrayWithObjects:@"Uncompressed, floating point", @"fl32",
               _NS("Uncompressed audio samples (useable with WAV)"), @"MUX_WAV",
               @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", @"-1", nil];
    o_dummyAud = [NSArray arrayWithObjects:@"Dummy", @"dummy",
                   _NS("Dummy codec (do not transcode, useable with all encapsulation "
                       "formats)"), @"MUX_PS", @"MUX_TS", @"MUX_MPEG", @"MUX_ASF", @"MUX_MP4",
                   @"MUX_OGG", @"MUX_RAW", @"MUX_MOV", @"MUX_WAV", nil];
    o_audioCodecs = [[NSArray alloc] initWithObjects: o_mpga, o_mp3, o_mp4a,
        o_a52, o_vorb, o_flac, o_spx, o_s16l, o_fl32, o_dummyAud, nil];


    /* fill another global array with all information about the encap-formats
     * note that the order of the formats inside the g. array is the same as on
     * the encap-tab */
    NSArray * o_ps;
    NSArray * o_ts;
    NSArray * o_mpeg;
    NSArray * o_ogg;
    NSArray * o_raw;
    NSArray * o_asf;
    NSArray * o_mp4;
    NSArray * o_mov;
    NSArray * o_wav;
    NSArray * o_asfh;
    o_ps = [NSArray arrayWithObjects:@"ps", @"MPEG PS", _NS("MPEG Program Stream"), @"mpg", nil];
    o_ts = [NSArray arrayWithObjects:@"ts", @"MPEG TS", _NS("MPEG Transport Stream"), nil];
    o_mpeg = [NSArray arrayWithObjects:@"ps", @"MPEG 1", _NS("MPEG 1 Format"), @"mpg", nil];
    o_ogg = [NSArray arrayWithObjects:@"ogg", @"OGG", @"OGG", nil];
    o_raw = [NSArray arrayWithObjects:@"raw", @"RAW", @"RAW", nil];
    o_asf = [NSArray arrayWithObjects:@"asf", @"ASF", @"ASF", nil];
    o_mp4 = [NSArray arrayWithObjects:@"mp4", @"MP4", @"MPEG4", nil];
    o_mov = [NSArray arrayWithObjects:@"mov", @"MOV", @"MOV", nil];
    o_wav = [NSArray arrayWithObjects:@"wav", @"WAV", @"WAV", nil];
    o_asfh = [NSArray arrayWithObjects:@"asfh", @"ASFH", @"ASFH", nil];
    o_encapFormats = [[NSArray alloc] initWithObjects: o_ps, o_ts, o_mpeg,
        o_ogg, o_raw, o_asf, o_mp4, o_mov, o_wav, o_asfh, nil];

    /* yet another array on streaming methods including help texts */
    NSArray * o_http;
    NSArray * o_mms;
    NSArray * o_udp_uni;
    NSArray * o_udp_multi;
    NSArray * o_rtp_uni;
    NSArray * o_rtp_multi;
    o_http = [NSArray arrayWithObjects:@"http", @"HTTP", _NS("Enter the local "
        "addresses you want to listen requests on. Do not enter anything if "
        "you want to listen on all the network interfaces. This is generally "
        "the best thing to do. Other computers can then access the stream at "
        "http://yourip:8080 by default.") , _NS("Use this to stream to several "
        "computers. This method is not the most efficient, as the server needs "\
        "to send the stream several times, but generally the most compatible"), nil];
    o_mms = [NSArray arrayWithObjects:@"mmsh", @"MMS", _NS("Enter the local "
        "addresses you want to listen requests on. Do not enter anything if "
        "you want to listen on all the network interfaces. This is generally "
        "the best thing to do. Other computers can then access the stream at "
        "mms://yourip:8080 by default."), _NS("Use this to stream to several "
        "computers using the Microsoft MMS protocol. This protocol is used as "
        "transport method by many Microsoft's software. Note that only a "
        "small part of the MMS protocol is supported (MMS encapsulated in "
        "HTTP)."), nil];
    o_udp_uni = [NSArray arrayWithObjects:@"udp", @"UDP-Unicast", _NS("Enter "
        "the address of the computer to stream to."), _NS("Use this to stream "
        "to a single computer."), nil];
    o_udp_multi = [NSArray arrayWithObjects:@"udp", @"UDP-Multicast", _NS("Enter "
        "the multicast address to stream to in this field. This must be an IP "
        "address between 224.0.0.0 and 239.255.255.255. For a private use, "
        "enter an address beginning with 239.255."), _NS("Use this to stream "
        "to a dynamic group of computers on a multicast-enabled network. This "
        "is the most efficient method to stream to several computers, but it "
        "won't work over the Internet."), nil];
    o_rtp_uni = [NSArray arrayWithObjects:@"rtp", @"RTP-Unicast", _NS("Enter the "
        "address of the computer to stream to.") , _NS("Use this to stream "
        "to a single computer. RTP headers will be added to the stream"), nil];
    o_rtp_multi = [NSArray arrayWithObjects:@"rtp", @"RTP-Multicast", _NS("Enter "
        "the multicast address to stream to in this field. This must be an IP "
        "address between 224.0.0.0 and 239.255.255.255. For a private use, "
        "enter an address beginning with 239.255."), _NS("Use this to stream "
        "to a dynamic group of computers on a multicast-enabled network. This "
        "is the most efficient method to stream to several computers, but it "
        "won't work over Internet. RTP headers will be added to the stream"), nil];
    o_strmgMthds = [[NSArray alloc] initWithObjects: o_http, o_mms,
        o_udp_uni, o_udp_multi, o_rtp_uni, o_rtp_multi, nil];
}

- (void)showWizard
{
    /* just present the window to the user */
    [o_wizard_window center];
    [o_wizard_window displayIfNeeded];
    [o_wizard_window makeKeyAndOrderFront:nil];
}

- (void)resetWizard
{
    /* go to the front page and clean up a bit */
    [o_userSelections removeAllObjects];
    [o_btn_forward setTitle: _NS("Next")];
    [o_tab_pageHolder selectFirstTabViewItem:self];
}

- (void)initStrings
{
    /* localise all strings to the users lang */
    /* method is called from intf.m (in method showWizard) */

    /* general items */
    [o_btn_backward setTitle: _NS("Back")];
    [o_btn_cancel setTitle: _NS("Cancel")];
    [o_btn_forward setTitle: _NS("Next")];
    [o_wizard_window setTitle: _NS("Streaming/Transcoding Wizard")];

    /* page one ("Hello") */
    [o_t1_txt_title setStringValue: _NS("Streaming/Transcoding Wizard")];
    [o_t1_txt_text setStringValue: _NS("This wizard allows configuring "
        "simple streaming or transcoding setups.")];
    [o_t1_btn_mrInfo_strmg setTitle: _NS("More Info")];
    [o_t1_btn_mrInfo_trnscd setTitle: _NS("More Info")];
    [o_t1_txt_notice setStringValue: _NS("This wizard only gives access to "
        "a small subset of VLC's streaming and transcoding capabilities. "
        "The Open and 'Saving/Streaming' dialogs will give access to more "
        "features.")];
    [[o_t1_matrix_strmgOrTrnscd cellAtRow:0 column:0] setTitle:
        _NS("Stream to network")];
    [[o_t1_matrix_strmgOrTrnscd cellAtRow:1 column:0] setTitle:
        _NS("Transcode/Save to file")];

    /* page two ("Input") */
    [o_t2_title setStringValue: _NS("Choose input")];
    [o_t2_text setStringValue: _NS("Choose here your input stream.")];
    [[o_t2_matrix_inputSourceType cellAtRow:0 column:0] setTitle:
        _NS("Select a stream")];
    [[o_t2_matrix_inputSourceType cellAtRow:1 column:0] setTitle:
        _NS("Existing playlist item")];
    [o_t2_btn_chooseFile setTitle: _NS("Choose...")];
    [[[o_t2_tbl_plst tableColumnWithIdentifier:@"name"] headerCell]
        setStringValue: _NS("Title")];
    [[[o_t2_tbl_plst tableColumnWithIdentifier:@"artist"] headerCell]
        setStringValue: _NS("Author")];
    [[[o_t2_tbl_plst tableColumnWithIdentifier:@"duration"] headerCell]
     setStringValue: _NS("Duration")];
    [o_t2_box_prtExtrct setTitle: _NS("Partial Extract")];
    [o_t2_ckb_enblPartExtrct setTitle: _NS("Enable")];
    [o_t2_ckb_enblPartExtrct setToolTip: _NS("This can be used to read only a "
        "part of the stream. It must be possible to control the incoming "
        "stream (for example, a file or a disc, but not an UDP network stream.) "
        "The starting and ending times can be given in seconds.")];
    [o_t2_txt_prtExtrctFrom setStringValue: _NS("From")];
    [o_t2_txt_prtExtrctTo setStringValue: _NS("To")];

    /* page three ("Streaming 1") */
    [o_t3_txt_title setStringValue: _NS("Streaming")];
    [o_t3_txt_text setStringValue: _NS("This page allows selecting how "
        "the input stream will be sent.")];
    [o_t3_box_dest setTitle: _NS("Destination")];
    [o_t3_box_strmgMthd setTitle: _NS("Streaming method")];
    [o_t3_txt_destInfo setStringValue: _NS("Address of the computer "
        "to stream to.")];
    [[o_t3_matrix_stmgMhd cellAtRow:0 column:0] setTitle: _NS("UDP Unicast")];
    [[o_t3_matrix_stmgMhd cellAtRow:0 column:1] setTitle: _NS("UDP Multicast")];
    [o_t3_txt_strgMthdInfo setStringValue: _NS("Use this to stream to a single "
        "computer.")];

    /* page four ("Transcode 1") */
    [o_t4_title setStringValue: _NS("Transcode")];
    [o_t4_text setStringValue: _NS("This page allows changing the compression "
        "format of the audio or video tracks. To change only "
        "the container format, proceed to next page.")];
    [o_t4_box_audio setTitle: _NS("Audio")];
    [o_t4_box_video setTitle: _NS("Video")];
    [o_t4_ckb_audio setTitle: _NS("Transcode audio")];
    [o_t4_ckb_video setTitle: _NS("Transcode video")];
    [o_t4_txt_videoBitrate setStringValue: _NS("Bitrate (kb/s)")];
    [o_t4_txt_videoCodec setStringValue: _NS("Codec")];
    [o_t4_txt_hintAudio setStringValue: _NS("Enabling this allows transcoding "\
    "the audio track if one is present in the stream.")];
    [o_t4_txt_hintVideo setStringValue: _NS("Enabling this allows transcoding "\
    "the video track if one is present in the stream.")];

    /* page five ("Encap") */
    [o_t5_title setStringValue: _NS("Encapsulation format")];
    [o_t5_text setStringValue: _NS("This page allows selecting how the "
        "stream will be encapsulated. Depending on previously chosen settings "
        "all formats won't be available.")];

    /* page six ("Streaming 2") */
    [o_t6_title setStringValue: _NS("Additional streaming options")];
    [o_t6_text setStringValue: _NS("In this page, a few "
                              "additional streaming parameters can be set.")];
    [o_t6_txt_ttl setStringValue: _NS("Time-To-Live (TTL)")];
    [o_t6_btn_mrInfo_ttl setTitle: _NS("More Info")];
    [o_t6_ckb_sap setTitle: _NS("SAP Announcement")];
    [o_t6_btn_mrInfo_sap setTitle: _NS("More Info")];
    [o_t6_ckb_local setTitle: _NS("Local playback")];
    [o_t6_btn_mrInfo_local setTitle: _NS("More Info")];
    [o_t6_ckb_soverlay setTitle: _NS("Add Subtitles to transcoded video")];

    /* page seven ("Transcode 2") */
    [o_t7_title setStringValue: _NS("Additional transcode options")];
    [o_t7_text setStringValue: _NS("In this page, a few "
                              "additional transcoding parameters can be set.")];
    [o_t7_txt_saveFileTo setStringValue: _NS("Select the file to save to")];
    [o_t7_btn_chooseFile setTitle: _NS("Choose...")];
    [o_t7_ckb_local setTitle: _NS("Local playback")];
    [o_t7_ckb_soverlay setTitle: _NS("Add Subtitles to transcoded video")];
    [o_t7_ckb_soverlay setToolTip: _NS("Adds available subtitles directly to "
                                       "the video. These cannot be disabled "
                                       "by the receiving user as they become "
                                       "part of the image.")];
    [o_t7_btn_mrInfo_local setTitle: _NS("More Info")];

    /* page eight ("Summary") */
    [o_t8_txt_text setStringValue: _NS("This page lists all the settings. "
        "Click \"Finish\" to start streaming or transcoding.")];
    [o_t8_txt_title setStringValue: _NS("Summary")];
    [o_t8_txt_destination setStringValue: [_NS("Destination")
        stringByAppendingString: @":"]];
    [o_t8_txt_encapFormat setStringValue: [_NS("Encap. format")
        stringByAppendingString: @":"]];
    [o_t8_txt_inputStream setStringValue: [_NS("Input stream")
        stringByAppendingString: @":"]];
    [o_t8_txt_partExtract setStringValue: [_NS("Partial Extract")
        stringByAppendingString: @":"]];
    [o_t8_txt_sap setStringValue: [_NS("SAP Announcement")
        stringByAppendingString: @":"]];
    [o_t8_txt_saveFileTo setStringValue: [_NS("Save file to")
        stringByAppendingString: @":"]];
    [o_t8_txt_strmgMthd setStringValue: [_NS("Streaming method")
        stringByAppendingString: @":"]];
    [o_t8_txt_trnscdAudio setStringValue: [_NS("Transcode audio")
        stringByAppendingString: @":"]];
    [o_t8_txt_trnscdVideo setStringValue: [_NS("Transcode video")
        stringByAppendingString: @":"]];
    [o_t8_txt_soverlay setStringValue: [_NS("Include subtitles")
        stringByAppendingString: @":"]];
    [o_t8_txt_local setStringValue: [_NS("Local playback")
        stringByAppendingString: @":"]];
}

- (void)initWithExtractValuesFrom: (NSString *)from
                               to: (NSString *)to
                           ofItem: (NSString *)item
{
    [self resetWizard];
    msg_Dbg(VLCIntf, "wizard was reseted");
    [o_userSelections setObject:@"trnscd" forKey:@"trnscdOrStrmg"];
    [o_btn_backward setEnabled:YES];
    [o_tab_pageHolder selectTabViewItemAtIndex:1];
    [o_t2_fld_prtExtrctFrom setStringValue: from];
    [o_t2_fld_prtExtrctTo setStringValue: to];
    [o_t2_fld_pathToNewStrm setStringValue: item];
    [o_t1_matrix_strmgOrTrnscd selectCellAtRow:1 column:0];
    [[o_t1_matrix_strmgOrTrnscd cellAtRow:0 column:0] setState: NSOffState];
    [o_t2_ckb_enblPartExtrct setState: NSOnState];
    [self t2_enableExtract: nil];
    msg_Dbg(VLCIntf, "wizard interface is set");

    [o_wizard_window center];
    [o_wizard_window display];
    [o_wizard_window makeKeyAndOrderFront:nil];
    msg_Dbg(VLCIntf, "wizard window displayed");
}

- (IBAction)cancelRun:(id)sender
{
    [o_wizard_window close];
}

- (id)playlistWizard
{
    return o_playlist_wizard;
}

- (IBAction)nextTab:(id)sender
{
    NSString * selectedTabViewItemLabel = [[o_tab_pageHolder selectedTabViewItem] label];
    if ([selectedTabViewItemLabel isEqualToString: @"Hello"])
    {
        /* check whether the user wants to stream or just to transcode;
         * store information for later usage */
        NSString *o_mode;
        o_mode = [[o_t1_matrix_strmgOrTrnscd selectedCell] title];
        if ([o_mode isEqualToString: _NS("Stream to network")])
        {
            /* we will be streaming */
            [o_userSelections setObject:@"strmg" forKey:@"trnscdOrStrmg"];
        }
        else
        {
            /* we will just do some transcoding */
            [o_userSelections setObject:@"trnscd" forKey:@"trnscdOrStrmg"];
        }
        [o_btn_backward setEnabled:YES];
        [o_tab_pageHolder selectTabViewItemAtIndex:1];

        /* Fill the playlist with current playlist items */
        [o_playlist_wizard reloadOutlineView];

    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Input"])
    {
        /* check whether partialExtract is enabled and store the values, if needed */
        if ([o_t2_ckb_enblPartExtrct state] == NSOnState)
        {
            [o_userSelections setObject:@"YES" forKey:@"partExtract"];
            [o_userSelections setObject:[o_t2_fld_prtExtrctFrom stringValue]
                forKey:@"partExtractFrom"];
            [o_userSelections setObject:[o_t2_fld_prtExtrctTo stringValue]
                forKey:@"partExtractTo"];
        }else{
            [o_userSelections setObject:@"NO" forKey:@"partExtract"];
        }

        /* check whether we use an existing pl-item or add an new one;
         * store the path or the index and set a flag.
         * complain to the user if s/he didn't provide a path */
        NSString *o_mode;
        BOOL stop;
        stop = NO;
        o_mode = [[o_t2_matrix_inputSourceType selectedCell] title];
        if ([o_mode isEqualToString: _NS("Select a stream")])
        {
            [o_userSelections setObject:@"YES" forKey:@"newStrm"];
            if ([[o_t2_fld_pathToNewStrm stringValue] isEqualToString: @""])
            {
                /* set a flag that no file is selected */
                stop = YES;
            }
            else
            {
                [o_userSelections setObject:[NSArray arrayWithObject:[o_t2_fld_pathToNewStrm stringValue]] forKey:@"pathToStrm"];
            }
        }
        else
        {
            if ([o_t2_tbl_plst numberOfSelectedRows] > 0)
            {
                NSIndexSet * selectedIndexes = [o_t2_tbl_plst selectedRowIndexes];
                NSUInteger count = [selectedIndexes count];
                NSMutableArray * tempArray = [[NSMutableArray alloc] init];
                for( NSUInteger x = 0; x < count; x++)
                {
                    playlist_item_t *p_item = [[o_t2_tbl_plst itemAtRow: [selectedIndexes indexGreaterThanOrEqualToIndex: x]] pointerValue];

                    if (p_item->i_children <= 0)
                    {
                        char *psz_uri = input_item_GetURI( p_item->p_input);
                        [tempArray addObject: [NSString stringWithUTF8String:psz_uri]];
                        free( psz_uri);
                        stop = NO;
                    }
                    else
                        stop = YES;
                }
                [o_userSelections setObject:[NSArray arrayWithArray: tempArray] forKey:@"pathToStrm"];
                [tempArray release];
            }
            else
            {
                /* set a flag that no item is selected */
                stop = YES;
            }
        }

        /* show either "Streaming 1" or "Transcode 1" to the user */
        if (stop == NO)
        {
            if ([[o_userSelections objectForKey:@"trnscdOrStrmg"]
                isEqualToString:@"strmg"])
            {
                /* we are streaming */
                [o_tab_pageHolder selectTabViewItemAtIndex:2];
            }else{
                /* we are just transcoding */

                /* rebuild the menues for the codec-selections */
                [self rebuildCodecMenus];

                [o_tab_pageHolder selectTabViewItemAtIndex:3];
            }
        } else {
            /* show a sheet that the user didn't select a file */
            NSBeginInformationalAlertSheet(_NS("No input selected"),
                _NS("OK"), @"", @"", o_wizard_window, nil, nil, nil, nil, @"%@",
                _NS("No new stream or valid playlist item has been selected.\n\n"
                "Choose one before going to the next page."));
        }
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Streaming 1"])
    {
        /* rebuild the menues for the codec-selections */
        [self rebuildCodecMenus];

        /* check which streaming method is selected and store it */
        int mode;
        mode = [[o_t3_matrix_stmgMhd selectedCell] tag];
        if (mode == 0)
        {
            /* HTTP Streaming */
            [o_userSelections setObject:@"0" forKey:@"stmgMhd"];

            /* disable all codecs which don't support MPEG PS, MPEG TS, MPEG 1,
             * OGG, RAW or ASF */
            [o_t4_pop_audioCodec removeItemWithTitle:@"Uncompressed, integer"];
            [o_t4_pop_audioCodec removeItemWithTitle:@"Uncompressed, floating point"];

        } else if ( mode == 1)
        {
            /* MMS Streaming */
            [o_userSelections setObject:@"1" forKey:@"stmgMhd"];

            /* disable all codecs which don't support ASF / ASFH */
            [o_t4_pop_audioCodec removeItemWithTitle:@"Vorbis"];
            [o_t4_pop_audioCodec removeItemWithTitle:@"FLAC"];
            [o_t4_pop_audioCodec removeItemWithTitle:@"Speex"];
            [o_t4_pop_audioCodec removeItemWithTitle:@"Uncompressed, integer"];
            [o_t4_pop_audioCodec removeItemWithTitle:@"Uncompressed, floating point"];

            [o_t4_pop_videoCodec removeItemWithTitle:@"MPEG-1 Video"];
            [o_t4_pop_videoCodec removeItemWithTitle:@"MPEG-2 Video"];
            [o_t4_pop_videoCodec removeItemWithTitle:@"H.263"];
            [o_t4_pop_videoCodec removeItemWithTitle:@"H.264"];
            [o_t4_pop_videoCodec removeItemWithTitle:@"MJPEG"];
            [o_t4_pop_videoCodec removeItemWithTitle:@"Theora"];
        } else {
            /* RTP/UDP Unicast/Multicast Streaming */
            [o_userSelections setObject: [[NSNumber numberWithInt:mode] stringValue] forKey:@"stmgMhd"];

            /* disable all codecs which don't support MPEG-TS */
            [o_t4_pop_audioCodec removeItemWithTitle:@"Vorbis"];
            [o_t4_pop_audioCodec removeItemWithTitle:@"FLAC"];
            [o_t4_pop_audioCodec removeItemWithTitle:@"Speex"];
            [o_t4_pop_audioCodec removeItemWithTitle:@"Uncompressed, integer"];
            [o_t4_pop_audioCodec removeItemWithTitle:@"Uncompressed, floating point"];
        }

        /* store the destination and check whether is it empty */
        if([[o_userSelections objectForKey:@"stmgMhd"] intValue] >=2)
        {
           /* empty field is valid for HTTP and MMS */
            if ([[o_t3_fld_address stringValue] isEqualToString: @""])
            {
                /* complain to the user that "" is no valid dest. */
                NSBeginInformationalAlertSheet(_NS("No valid destination"),
                    _NS("OK"), @"", @"", o_wizard_window, nil, nil, nil, nil, @"%@",
                    _NS("A valid destination has to be selected "
                    "Enter either a Unicast-IP or a Multicast-IP."
                    "\n\nIf you don't know what this means, have a look at "
                    "the VLC Streaming HOWTO and the help texts in this "
                    "window."));
            } else {
                /* FIXME: check whether the entered IP is really valid */
                [o_userSelections setObject:[o_t3_fld_address stringValue]
                    forKey:@"stmgDest"];
                /* let's go to the transcode-1-tab */
                [o_tab_pageHolder selectTabViewItemAtIndex:3];
            }
        } else {
            [o_userSelections setObject:[o_t3_fld_address stringValue]
                forKey:@"stmgDest"];
            /* let's go to the transcode-1-tab */
            [o_tab_pageHolder selectTabViewItemAtIndex:3];
        }
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Transcode 1"])
    {
        /* check whether the user wants to transcode the video-track and store
         * the related options */
        if ([o_t4_ckb_video state] == NSOnState)
        {
            NSNumber * theNum;
            theNum = [NSNumber numberWithInt:[[o_t4_pop_videoCodec selectedItem]tag]];
            [o_userSelections setObject:@"YES" forKey:@"trnscdVideo"];
            [o_userSelections setObject:[o_t4_pop_videoBitrate titleOfSelectedItem]
                forKey:@"trnscdVideoBitrate"];
            [o_userSelections setObject:theNum forKey:@"trnscdVideoCodec"];
        } else {
            [o_userSelections setObject:@"NO" forKey:@"trnscdVideo"];
        }

        /* check whether the user wants to transcode the audio-track and store
         * the related options */
        if ([o_t4_ckb_audio state] == NSOnState)
        {
            NSNumber * theNum;
            theNum = [NSNumber numberWithInt:[[o_t4_pop_audioCodec selectedItem]tag]];
            [o_userSelections setObject:@"YES" forKey:@"trnscdAudio"];
            [o_userSelections setObject:[o_t4_pop_audioBitrate titleOfSelectedItem]
                forKey:@"trnscdAudioBitrate"];
            [o_userSelections setObject:theNum forKey:@"trnscdAudioCodec"];
        } else {
            [o_userSelections setObject:@"NO" forKey:@"trnscdAudio"];
        }

        /* store the currently selected item for further reference */
        int i_temp = [[o_t5_matrix_encap selectedCell] tag];

        /* disable all encap-formats */
        [[o_t5_matrix_encap cellAtRow:0 column:0] setEnabled:NO];
        [[o_t5_matrix_encap cellAtRow:1 column:0] setEnabled:NO];
        [[o_t5_matrix_encap cellAtRow:2 column:0] setEnabled:NO];
        [[o_t5_matrix_encap cellAtRow:3 column:0] setEnabled:NO];
        [[o_t5_matrix_encap cellAtRow:4 column:0] setEnabled:NO];
        [[o_t5_matrix_encap cellAtRow:5 column:0] setEnabled:NO];
        [[o_t5_matrix_encap cellAtRow:6 column:0] setEnabled:NO];
        [[o_t5_matrix_encap cellAtRow:7 column:0] setEnabled:NO];
        [[o_t5_matrix_encap cellAtRow:8 column:0] setEnabled:NO];
        [[o_t5_matrix_encap cellAtRow:9 column:0] setEnabled:NO];
        [[o_t5_matrix_encap cellAtRow:10 column:0] setEnabled:NO];

        /* re-enable the encap-formats supported by the chosen codecs */
        /* FIXME: the following is a really bad coding-style. feel free to mail
            me ideas how to make this nicer, if you want to -- FK, 7/11/05 */

        if ([[o_userSelections objectForKey:@"trnscdAudio"] isEqualTo: @"YES"])
        {
            NSInteger i_selectedAudioCodec = [[o_userSelections objectForKey:@"trnscdAudioCodec"] intValue];

            if ([[o_userSelections objectForKey:@"trnscdVideo"] isEqualTo: @"YES"])
            {
                NSInteger i_selectedVideoCodec = [[o_userSelections objectForKey:@"trnscdVideoCodec"] intValue];

                /* we are transcoding both audio and video, so we need to check both deps */
                if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_PS"])
                {
                    if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_PS"])
                    {
                        [[o_t5_matrix_encap cellAtRow:0 column:0] setEnabled:YES];
                        [o_t5_matrix_encap selectCellAtRow:0 column:0];
                    }
                }
                if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_TS"])
                {
                    if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_TS"])
                    {
                        [[o_t5_matrix_encap cellAtRow:1 column:0] setEnabled:YES];
                        [o_t5_matrix_encap selectCellAtRow:1 column:0];
                    }
                }
                if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_MPEG"])
                {
                    if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_MPEG"])
                    {
                        [[o_t5_matrix_encap cellAtRow:2 column:0] setEnabled:YES];
                        [o_t5_matrix_encap selectCellAtRow:2 column:0];
                    }
                }
                if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_OGG"])
                {
                    if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_OGG"])
                    {
                        [[o_t5_matrix_encap cellAtRow:3 column:0] setEnabled:YES];
                        [o_t5_matrix_encap selectCellAtRow:3 column:0];
                    }
                }
                if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_RAW"])
                {
                    if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_RAW"])
                    {
                        [[o_t5_matrix_encap cellAtRow:4 column:0] setEnabled:YES];
                        [o_t5_matrix_encap selectCellAtRow:4 column:0];
                    }
                }
                if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_ASF"])
                {
                    if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_ASF"])
                    {
                        [[o_t5_matrix_encap cellAtRow:5 column:0] setEnabled:YES];
                        [o_t5_matrix_encap selectCellAtRow:5 column:0];
                    }
                }
                if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_MP4"])
                {
                    if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_MP4"])
                    {
                        [[o_t5_matrix_encap cellAtRow:6 column:0] setEnabled:YES];
                        [o_t5_matrix_encap selectCellAtRow:6 column:0];
                    }
                }
                if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_MOV"])
                {
                    if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_MOV"])
                    {
                        [[o_t5_matrix_encap cellAtRow:7 column:0] setEnabled:YES];
                        [o_t5_matrix_encap selectCellAtRow:7 column:0];
                    }
                }
                if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_WAV"])
                {
                    if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_WAV"])
                    {
                        [[o_t5_matrix_encap cellAtRow:8 column:0] setEnabled:YES];
                        [o_t5_matrix_encap selectCellAtRow:8 column:0];
                    }
                }

            } else {

                /* we just transcoding the audio */

                /* select formats supported by the audio codec */
                if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_PS"])
                {
                    [[o_t5_matrix_encap cellAtRow:0 column:0] setEnabled:YES];
                    [o_t5_matrix_encap selectCellAtRow:0 column:0];
                }
                if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_TS"])
                {
                    [[o_t5_matrix_encap cellAtRow:1 column:0] setEnabled:YES];
                    [o_t5_matrix_encap selectCellAtRow:1 column:0];
                }
                if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_MPEG"])
                {
                    [[o_t5_matrix_encap cellAtRow:2 column:0] setEnabled:YES];
                    [o_t5_matrix_encap selectCellAtRow:2 column:0];
                }
                if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_OGG"])
                {
                    [[o_t5_matrix_encap cellAtRow:3 column:0] setEnabled:YES];
                    [o_t5_matrix_encap selectCellAtRow:3 column:0];
                }
                if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_RAW"])
                {
                    [[o_t5_matrix_encap cellAtRow:4 column:0] setEnabled:YES];
                    [o_t5_matrix_encap selectCellAtRow:4 column:0];
                }
                if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_ASF"])
                {
                    [[o_t5_matrix_encap cellAtRow:5 column:0] setEnabled:YES];
                    [o_t5_matrix_encap selectCellAtRow:5 column:0];
                }
                if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_MP4"])
                {
                    [[o_t5_matrix_encap cellAtRow:6 column:0] setEnabled:YES];
                    [o_t5_matrix_encap selectCellAtRow:6 column:0];
                }
                if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_MOV"])
                {
                    [[o_t5_matrix_encap cellAtRow:7 column:0] setEnabled:YES];
                    [o_t5_matrix_encap selectCellAtRow:7 column:0];
                }
                if ([[o_audioCodecs objectAtIndex:i_selectedAudioCodec] containsObject: @"MUX_WAV"])
                {
                    [[o_t5_matrix_encap cellAtRow:8 column:0] setEnabled:YES];
                    [o_t5_matrix_encap selectCellAtRow:8 column:0];
                }
            }
        }
        else if ([[o_userSelections objectForKey:@"trnscdVideo"] isEqualTo: @"YES"])
        {
            /* we are just transcoding the video */

            /* select formats supported by the video-codec */
            NSInteger i_selectedVideoCodec = [[o_userSelections objectForKey:@"trnscdVideoCodec"] intValue];

            if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_PS"])
            {
                [[o_t5_matrix_encap cellAtRow:0 column:0] setEnabled:YES];
                [o_t5_matrix_encap selectCellAtRow:0 column:0];
            }
            if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_TS"])
            {
                [[o_t5_matrix_encap cellAtRow:1 column:0] setEnabled:YES];
                [o_t5_matrix_encap selectCellAtRow:1 column:0];
            }
            if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_MPEG"])
            {
                [[o_t5_matrix_encap cellAtRow:2 column:0] setEnabled:YES];
                [o_t5_matrix_encap selectCellAtRow:2 column:0];
            }
            if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_OGG"])
            {
                [[o_t5_matrix_encap cellAtRow:3 column:0] setEnabled:YES];
                [o_t5_matrix_encap selectCellAtRow:3 column:0];
            }
            if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_RAW"])
            {
                [[o_t5_matrix_encap cellAtRow:4 column:0] setEnabled:YES];
                [o_t5_matrix_encap selectCellAtRow:4 column:0];
            }
            if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_ASF"])
            {
                [[o_t5_matrix_encap cellAtRow:5 column:0] setEnabled:YES];
                [o_t5_matrix_encap selectCellAtRow:5 column:0];
            }
            if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_MP4"])
            {
                [[o_t5_matrix_encap cellAtRow:6 column:0] setEnabled:YES];
                [o_t5_matrix_encap selectCellAtRow:6 column:0];
            }
            if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_MOV"])
            {
                [[o_t5_matrix_encap cellAtRow:7 column:0] setEnabled:YES];
                [o_t5_matrix_encap selectCellAtRow:7 column:0];
            }
            if ([[o_videoCodecs objectAtIndex:i_selectedVideoCodec] containsObject: @"MUX_WAV"])
            {
                [[o_t5_matrix_encap cellAtRow:8 column:0] setEnabled:YES];
                [o_t5_matrix_encap selectCellAtRow:8 column:0];
            }
        } else {
            /* we don't do any transcoding
             * -> enabled the encap-formats allowed when streaming content via
             * http plus MP4 since this should work fine in most cases */

            /* FIXME: choose a selection of encap-formats based upon the
             * actually used codecs */

            /* enable MPEG PS, MPEG TS, MPEG 1, OGG, RAW, ASF, MP4 and MOV
             * select MPEG PS */
            [[o_t5_matrix_encap cellAtRow:0 column:0] setEnabled:YES];
            [[o_t5_matrix_encap cellAtRow:1 column:0] setEnabled:YES];
            [[o_t5_matrix_encap cellAtRow:2 column:0] setEnabled:YES];
            [[o_t5_matrix_encap cellAtRow:3 column:0] setEnabled:YES];
            [[o_t5_matrix_encap cellAtRow:4 column:0] setEnabled:YES];
            [[o_t5_matrix_encap cellAtRow:5 column:0] setEnabled:YES];
            [[o_t5_matrix_encap cellAtRow:6 column:0] setEnabled:YES];
            [[o_t5_matrix_encap cellAtRow:7 column:0] setEnabled:YES];
            [[o_t5_matrix_encap cellAtRow:8 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:9 column:0] setEnabled:NO];
            [o_t5_matrix_encap selectCellAtRow:0 column:0];
        }

        NSInteger i_streamingMethod = [[o_userSelections objectForKey:@"stmgMhd"] intValue];
        if ( i_streamingMethod == 1)
        {
            /* if MMS is the streaming protocol, only ASFH is available */
            [[o_t5_matrix_encap cellAtRow:0 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:1 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:2 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:3 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:4 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:5 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:6 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:7 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:8 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:9 column:0] setEnabled:YES];
            [o_t5_matrix_encap selectCellAtRow:9 column:0];
        }
        else if ( i_streamingMethod == 0)
        {
            /* if HTTP is the streaming protocol, disable all unsupported
             * encap-formats, but don't touch the other ones selected above */
            [[o_t5_matrix_encap cellAtRow:6 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:7 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:8 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:9 column:0] setEnabled:NO];
        }
        else if ( i_streamingMethod >= 2)
        {
            /* if UDP/RTP is the streaming protocol, only MPEG-TS is available */
            [[o_t5_matrix_encap cellAtRow:0 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:2 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:3 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:4 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:5 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:6 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:7 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:8 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:9 column:0] setEnabled:NO];
            [[o_t5_matrix_encap cellAtRow:1 column:0] setEnabled:YES];
            [o_t5_matrix_encap selectCellAtRow:1 column:0];
        }

        BOOL anythingEnabled;
        anythingEnabled = NO;
        NSUInteger count = [o_t5_matrix_encap numberOfRows];
        for (NSUInteger x = 0; x < count; x++)
        {
            if ([[o_t5_matrix_encap cellAtRow:x column:0] isEnabled])
                anythingEnabled = YES;
        }

        if (anythingEnabled == YES)
        {
            /* re-select the previously chosen item, if available */
            if ([[o_t5_matrix_encap cellWithTag: i_temp] isEnabled])
                [o_t5_matrix_encap selectCellWithTag: i_temp];

            /* go the encap-tab */
            [o_tab_pageHolder selectTabViewItemAtIndex:4];
        } else {
            /* show a sheet that the selected codecs are not compatible */
            NSBeginInformationalAlertSheet(_NS("Invalid selection"), _NS("OK"),
                @"", @"", o_wizard_window, nil, nil, nil, nil, @"%@", _NS("The "
                "chosen codecs are not compatible with each other. For example: "
                "It is not possible to mix uncompressed audio with any video codec.\n\n"
                "Correct your selection and try again."));
        }

    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Encap"])
    {
        /* get the chosen encap format and store it */
        NSNumber * theNum;
        theNum = [NSNumber numberWithInt:[[o_t5_matrix_encap selectedCell] tag]];
        [o_userSelections setObject:[theNum stringValue] forKey:@"encapFormat"];

        /* show either "Streaming 2" or "Transcode 2" to the user */
        if ([[o_userSelections objectForKey:@"trnscdOrStrmg"] isEqualToString:@"strmg"])
        {
            /* we are streaming */
            [o_tab_pageHolder selectTabViewItemAtIndex:5];
        }else{
            /* we are just transcoding */
            [o_tab_pageHolder selectTabViewItemAtIndex:6];
            /* in case that we are processing multiple items, let the user
             * select a folder instead of a localtion for a single item */
            if ([[o_userSelections objectForKey:@"pathToStrm"] count] > 1)
            {
                [o_t7_txt_saveFileTo setStringValue:
                    _NS("Select the directory to save to")];
            }
            else
            {
                [o_t7_txt_saveFileTo setStringValue:
                    _NS("Select the file to save to")];
            }
        }
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Streaming 2"])
    {
        /* store the chosen TTL */
        [o_userSelections setObject:[o_t6_fld_ttl stringValue] forKey:@"ttl"];

        /* check whether SAP is enabled and store the announce, if needed */
        if ([o_t6_ckb_sap state] == NSOnState)
        {
            [o_userSelections setObject:@"YES" forKey:@"sap"];
            [o_userSelections setObject:[o_t6_fld_sap stringValue] forKey:@"sapText"];
        } else {
            [o_userSelections setObject:@"NO" forKey:@"sap"];
        }

        /* local playback? */
        if ([o_t6_ckb_local state] == NSOnState)
        {
            [o_userSelections setObject:@"YES" forKey:@"localPb"];
        } else {
            [o_userSelections setObject:@"NO" forKey:@"localPb"];
        }

        /* include subtitles? */
        [o_userSelections setObject: [[NSNumber numberWithInt:[o_t6_ckb_soverlay state]] stringValue] forKey: @"soverlay"];

        /* go to "Summary" */
        [self showSummary];
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Transcode 2"])
    {
        /* local playback? */
        if ([o_t7_ckb_local state] == NSOnState)
        {
            [o_userSelections setObject:@"YES" forKey:@"localPb"];
        } else {
            [o_userSelections setObject:@"NO" forKey:@"localPb"];
        }

        /* check whether the path != "" and store it */
        if ([[o_t7_fld_filePath stringValue] isEqualToString: @""])
        {
            /* complain to the user that "" is no valid path for a folder/file */
            if ([[o_userSelections objectForKey:@"pathToStrm"] count] > 1)
                NSBeginInformationalAlertSheet(_NS("No folder selected"),
                    _NS("OK"), @"", @"", o_wizard_window, nil, nil, nil, nil,
                    @"%@\n\n%@", _NS("A directory "
                    "where to save the files has to be selected."),
                    _NS("Enter either a valid path or use the \"Choose...\" "
                    "button to select a location."));
            else
                NSBeginInformationalAlertSheet(_NS("No file selected"),
                    _NS("OK"), @"", @"", o_wizard_window, nil, nil, nil, nil,
                    @"%@\n\n%@", _NS("A file "
                    "where to save the stream has to be selected."),
                    _NS("Enter either a valid path or use the \"Choose\" "
                    "button to select a location."));
        } else {
            /* create a string containing the requested suffix for later usage */
            NSString * theEncapFormat = [[o_encapFormats objectAtIndex:
                [[o_userSelections objectForKey:@"encapFormat"] intValue]]
                objectAtIndex:0];
            if ([theEncapFormat isEqualToString:@"ps"])
                theEncapFormat = @"mpg";

            /* look whether we need to process multiple items or not.
             * choose a faster variant if we just want a single item */
            if ([[o_userSelections objectForKey:@"pathToStrm"] count] > 1)
            {
                NSMutableArray * tempArray = [[NSMutableArray alloc] init];
                int x = 0;
                int y = [[o_userSelections objectForKey:@"pathToStrm"] count];
                NSString * tempString = [[NSString alloc] init];
                while( x != y)
                {
                    NSString * fileNameToUse;
                    /* check whether the extension is hidden or not.
                     * if not, remove it
                     * we need the casting to make GCC4 happy */
                    if ([[[NSFileManager defaultManager] attributesOfItemAtPath:
                        [[o_userSelections objectForKey:@"pathToStrm"]
                         objectAtIndex:x] error:nil] objectForKey:
                        NSFileExtensionHidden])
                        fileNameToUse = [NSString stringWithString:
                            [[NSFileManager defaultManager] displayNameAtPath:
                            [[o_userSelections objectForKey:@"pathToStrm"]
                            objectAtIndex:x]]];
                    else
                    {
                        int z = 0;
                        int count = [[[[NSFileManager defaultManager]
                            displayNameAtPath:
                            [[o_userSelections objectForKey:@"pathToStrm"]
                            objectAtIndex:x]]
                            componentsSeparatedByString: @"."] count];
                        fileNameToUse = @"";
                        while( z < (count - 1))
                        {
                            fileNameToUse = [fileNameToUse stringByAppendingString:
                                [[[[NSFileManager defaultManager]
                                displayNameAtPath:
                                [[o_userSelections objectForKey:@"pathToStrm"]
                                objectAtIndex:x]]
                                componentsSeparatedByString: @"."]
                                objectAtIndex:z]];
                            z += 1;
                        }
                    }
                    tempString = [NSString stringWithFormat: @"%@%@.%@",
                        [o_t7_fld_filePath stringValue],
                        fileNameToUse, theEncapFormat];
                    if ([[NSFileManager defaultManager] fileExistsAtPath:
                        tempString])
                    {
                        /* we don't wanna overwrite existing files, so add an
                         * int to the file-name */
                        int additionalInt = 1;
                        while( additionalInt < 100)
                        {
                            tempString = [NSString stringWithFormat:@"%@%@ %i.%@",
                                [o_t7_fld_filePath stringValue],
                                fileNameToUse, additionalInt, theEncapFormat];
                            if(! [[NSFileManager defaultManager]
                                fileExistsAtPath: tempString])
                                break;
                            additionalInt += 1;
                        }
                        if (additionalInt >= 100)
                            msg_Err( VLCIntf, "Files with the same name are "
                                "already present in the destination directory. "
                                "Delete these files or choose a different directory.");
                    }
                    [tempArray addObject: [tempString retain]];
                    x += 1;
                }
                [o_userSelections setObject: [NSArray arrayWithArray:tempArray]
                    forKey: @"trnscdFilePath"];
                [tempArray release];
                [tempString release];
            }
            else
            {
                /* we don't need to check for existing items because Cocoa
                 * does that already when we are asking the user for a location
                 * to save her file */
                [o_userSelections setObject: [NSArray arrayWithObject:[o_t7_fld_filePath stringValue]] forKey: @"trnscdFilePath"];
            }

            /* include subtitles ? */
            [o_userSelections setObject:[[NSNumber numberWithInt:[o_t7_ckb_soverlay state]] stringValue] forKey: @"soverlay"];

            /* go to "Summary" */
            [self showSummary];
        }
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Summary"])
    {
        intf_thread_t * p_intf = VLCIntf;

        playlist_t * p_playlist = pl_Get( p_intf);

        int x = 0;
        int y = [[o_userSelections objectForKey:@"pathToStrm"] count];
        while( x != y)
        {
            /* we need a temp. variable here to work-around a GCC4-bug */
            NSString *tempString = [NSString stringWithFormat:
                @"%@ (%i/%i)", _NS("Streaming/Transcoding Wizard"),
                ( x + 1), y];
            input_item_t *p_input = input_item_New(
                [[[o_userSelections objectForKey:@"pathToStrm"]
                objectAtIndex:x] UTF8String],
                [tempString UTF8String]);

            /* use the MRL from the text field, in case the user
             * modified it */
            input_item_AddOption( p_input, [[o_t8_fld_mrl stringValue] UTF8String], VLC_INPUT_OPTION_TRUSTED);

            if(! [[o_userSelections objectForKey:@"partExtractFrom"]
                isEqualToString:@""]) {
                NSArray * components = [[o_userSelections objectForKey: @"partExtractFrom"] componentsSeparatedByString:@":"];
                NSUInteger componentCount = [components count];
                NSUInteger time = 0;
                if (componentCount == 1)
                    time = 1000000 * ([[components objectAtIndex:0] intValue]);
                else if (componentCount == 2)
                    time = 1000000 * ([[components objectAtIndex:0] intValue] * 60 + [[components objectAtIndex:1] intValue]);
                else if (componentCount == 3)
                    time = 1000000 * ([[components objectAtIndex:0] intValue] * 3600 + [[components objectAtIndex:1] intValue] * 60 + [[components objectAtIndex:2] intValue]);
                else
                    msg_Err(VLCIntf, "Invalid string format for time");
                input_item_AddOption(p_input, [[NSString stringWithFormat: @"start-time=%lu", time] UTF8String], VLC_INPUT_OPTION_TRUSTED);
            }

            if(! [[o_userSelections objectForKey:@"partExtractTo"]
                isEqualToString:@""]) {
                NSArray * components = [[o_userSelections objectForKey: @"partExtractTo"] componentsSeparatedByString:@":"];
                NSUInteger componentCount = [components count];
                NSUInteger time = 0;
                if (componentCount == 1)
                    time = 1000000 * ([[components objectAtIndex:0] intValue]);
                else if (componentCount == 2)
                    time = 1000000 * ([[components objectAtIndex:0] intValue] * 60 + [[components objectAtIndex:1] intValue]);
                else if (componentCount == 3)
                    time = 1000000 * ([[components objectAtIndex:0] intValue] * 3600 + [[components objectAtIndex:1] intValue] * 60 + [[components objectAtIndex:2] intValue]);
                else
                    msg_Err(VLCIntf, "Invalid string format for time");
                input_item_AddOption(p_input, [[NSString stringWithFormat: @"stop-time=%lu", time] UTF8String], VLC_INPUT_OPTION_TRUSTED);
            }

            input_item_AddOption( p_input, [[NSString stringWithFormat:
                @"ttl=%@", [o_userSelections objectForKey:@"ttl"]]
                UTF8String],
                VLC_INPUT_OPTION_TRUSTED);

            int returnValue = playlist_AddInput( p_playlist, p_input, PLAYLIST_STOP, PLAYLIST_END, true, pl_Unlocked);

            if (x == 0 && returnValue != VLC_SUCCESS)
            {
                /* play the first item and add the others afterwards */
                PL_LOCK;
                playlist_item_t *p_item = playlist_ItemGetByInput( p_playlist, p_input);
                playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, pl_Locked, NULL,
                          p_item);
                PL_UNLOCK;
            }

            vlc_gc_decref( p_input);
            x += 1;
        }

        /* close the window, since we are done */
        [o_wizard_window close];
    }
}

- (void)rebuildCodecMenus
{
    int savePreviousSel = 0;
    savePreviousSel = [o_t4_pop_videoCodec indexOfSelectedItem];
    [o_t4_pop_videoCodec removeAllItems];
    NSUInteger count = [o_videoCodecs count];
    for (NSUInteger x = 0; x < count; x++)
    {
        [o_t4_pop_videoCodec addItemWithTitle:[[o_videoCodecs objectAtIndex:x]
            objectAtIndex:0]];
        [[o_t4_pop_videoCodec lastItem] setTag:x];
    }
    if (savePreviousSel >= 0)
        [o_t4_pop_videoCodec selectItemAtIndex: savePreviousSel];

    savePreviousSel = [o_t4_pop_audioCodec indexOfSelectedItem];
    [o_t4_pop_audioCodec removeAllItems];
    count = [o_audioCodecs count];
    for (NSUInteger x = 0; x < count; x++)
    {
        [o_t4_pop_audioCodec addItemWithTitle:[[o_audioCodecs objectAtIndex:x]
            objectAtIndex:0]];
        [[o_t4_pop_audioCodec lastItem] setTag:x];
    }
    if (savePreviousSel >= 0)
        [o_t4_pop_audioCodec selectItemAtIndex: savePreviousSel];
}

- (void)showSummary
{
    [o_btn_forward setTitle: _NS("Finish")];
    /* if we will transcode multiple items, just give their number; otherwise
     * print the URI of the single item */
    if ([[o_userSelections objectForKey:@"pathToStrm"] count] > 1)
        [o_t8_fld_inptStream setStringValue: [NSString stringWithFormat:
            _NS("%i items"),
            [[o_userSelections objectForKey:@"pathToStrm"] count]]];
    else
        [o_t8_fld_inptStream setStringValue:
            [[o_userSelections objectForKey:@"pathToStrm"] objectAtIndex:0]];

    if ([[o_userSelections objectForKey:@"localPb"] isEqualToString: @"YES"])
    {
        [o_t8_fld_local setStringValue: _NS("yes")];
    } else {
        [o_t8_fld_local setStringValue: _NS("no")];
    }

    if ([[o_userSelections objectForKey:@"partExtract"] isEqualToString: @"YES"])
    {
        [o_t8_fld_partExtract setStringValue: [NSString stringWithFormat:
            _NS("yes: from %@ to %@"),
            [o_userSelections objectForKey:@"partExtractFrom"],
            [o_userSelections objectForKey:@"partExtractTo"]]];
    } else {
        [o_t8_fld_partExtract setStringValue: _NS("no")];
    }

    if ([[o_userSelections objectForKey:@"trnscdVideo"] isEqualToString:@"YES"])
    {
        [o_t8_fld_trnscdVideo setStringValue: [NSString stringWithFormat:
            _NS("yes: %@ @ %@ kb/s"),
            [[o_videoCodecs objectAtIndex:[[o_userSelections objectForKey:
            @"trnscdVideoCodec"] intValue]] objectAtIndex:0],
            [o_userSelections objectForKey:@"trnscdVideoBitrate"]]];
    }
    else
    {
        [o_t8_fld_trnscdVideo setStringValue: _NS("no")];
    }

    if ([[o_userSelections objectForKey:@"soverlay"] isEqualToString:@"1"])
        [o_t8_fld_soverlay setStringValue: _NS("yes")];
    else
        [o_t8_fld_soverlay setStringValue: _NS("no")];

    if ([[o_userSelections objectForKey:@"trnscdAudio"] isEqualToString:@"YES"])
    {
        [o_t8_fld_trnscdAudio setStringValue: [NSString stringWithFormat:
            _NS("yes: %@ @ %@ kb/s"),
            [[o_audioCodecs objectAtIndex:[[o_userSelections objectForKey:
                @"trnscdAudioCodec"] intValue]] objectAtIndex:0],
            [o_userSelections objectForKey:@"trnscdAudioBitrate"]]];
    }
    else
    {
        [o_t8_fld_trnscdAudio setStringValue: _NS("no")];
    }

    if ([[o_userSelections objectForKey:@"trnscdOrStrmg"] isEqualToString:@"strmg"])
    {
        /* we are streaming and perhaps also transcoding */
        [o_t8_fld_saveFileTo setStringValue: @"-"];
        [o_t8_fld_strmgMthd setStringValue: [[o_strmgMthds objectAtIndex:
            [[o_userSelections objectForKey:@"stmgMhd"] intValue]]
            objectAtIndex:1]];
        [o_t8_fld_destination setStringValue: [o_userSelections objectForKey:
            @"stmgDest"]];
        [o_t8_fld_ttl setStringValue: [o_userSelections objectForKey:@"ttl"]];
        if ([[o_userSelections objectForKey:@"sap"] isEqualToString: @"YES"])
        {
            [o_t8_fld_sap setStringValue:
                [_NS("yes") stringByAppendingFormat: @": \"%@\"",
                    [o_userSelections objectForKey:@"sapText"]]];
        }else{
            [o_t8_fld_sap setStringValue: _NS("no")];
        }
    } else {
        /* we are transcoding */
        [o_t8_fld_strmgMthd setStringValue: @"-"];
        [o_t8_fld_destination setStringValue: @"-"];
        [o_t8_fld_ttl setStringValue: @"-"];
        [o_t8_fld_sap setStringValue: @"-"];
        /* do only show the destination of the first item and add a counter, if needed */
        if ([[o_userSelections objectForKey: @"trnscdFilePath"] count] > 1)
            [o_t8_fld_saveFileTo setStringValue:
                [NSString stringWithFormat: @"%@ (+%li)",
                [[o_userSelections objectForKey: @"trnscdFilePath"] objectAtIndex:0],
                ([[o_userSelections objectForKey: @"trnscdFilePath"] count] - 1)]];
        else
            [o_t8_fld_saveFileTo setStringValue:
                [[o_userSelections objectForKey: @"trnscdFilePath"] objectAtIndex:0]];
    }
    [o_t8_fld_encapFormat setStringValue: [[o_encapFormats objectAtIndex:
        [[o_userSelections objectForKey:@"encapFormat"] intValue]] objectAtIndex:1]];

    [self createOpts];
    [o_t8_fld_mrl setStringValue: [[o_userSelections objectForKey:@"opts"]
        objectAtIndex:0]];

    [o_tab_pageHolder selectTabViewItemAtIndex:7];
}

- (void) createOpts
{
    NSMutableString * o_opts_string = [NSMutableString stringWithString:@""];
    NSMutableString *o_trnscdCmd = [NSMutableString stringWithString:@""];
    NSMutableString *o_duplicateCmd = [NSMutableString stringWithString:@""];
    int x = 0;
    int y = [[o_userSelections objectForKey:@"pathToStrm"] count];
    NSMutableArray * tempArray = [[NSMutableArray alloc] init];

    /* loop to create an opt-string for each item we're processing */
    while( x != y)
    {
        /* check whether we transcode the audio and/or the video and compose a
         * string reflecting the settings, if needed */
        if ([[o_userSelections objectForKey:@"trnscdVideo"] isEqualToString:@"YES"])
        {
            [o_trnscdCmd appendString: @"transcode{"];
            [o_trnscdCmd appendFormat: @"vcodec=%@,vb=%i",
                [[o_videoCodecs objectAtIndex:[[o_userSelections objectForKey:@"trnscdVideoCodec"] intValue]] objectAtIndex:1],
                [[o_userSelections objectForKey:@"trnscdVideoBitrate"] intValue]];
            if ([[o_userSelections objectForKey:@"trnscdAudio"] isEqualToString:@"YES"])
            {
                [o_trnscdCmd appendString: @","];
            }
            else
            {
                [o_trnscdCmd appendString: @"}:"];
            }
        }

        /* check whether the user requested local playback. if yes, prepare the
         * string, if not, let it empty */
        if ([[o_userSelections objectForKey:@"localPb"] isEqualToString:@"YES"])
        {
            [o_duplicateCmd appendString: @"duplicate{dst=display,dst=\""];
        }

        if ([[o_userSelections objectForKey:@"trnscdAudio"] isEqualToString:@"YES"])
        {
            if ([[o_userSelections objectForKey:@"trnscdVideo"] isEqualToString:@"NO"])
            {
                /* in case we transcode the audio only, add this */
                [o_trnscdCmd appendString: @"transcode{"];
            }
            [o_trnscdCmd appendFormat: @"acodec=%@,ab=%i}:",
                [[o_audioCodecs objectAtIndex:[[o_userSelections objectForKey:@"trnscdAudioCodec"] intValue]] objectAtIndex:1],
                [[o_userSelections objectForKey:@"trnscdAudioBitrate"] intValue]];
        }

        if ([[o_userSelections objectForKey:@"trnscdOrStrmg"] isEqualToString:@"trnscd"])
        {
            /* we are just transcoding and dumping the stuff to a file */
            [o_opts_string appendFormat:
                @":sout=#%@%@standard{mux=%@,access=file{no-overwrite},dst=%@}",
                o_duplicateCmd,
                o_trnscdCmd,
                [[o_encapFormats objectAtIndex:[[o_userSelections objectForKey:@"encapFormat"] intValue]] objectAtIndex:0],
                [[o_userSelections objectForKey: @"trnscdFilePath"] objectAtIndex:x]];
        }
        else
        {

            /* we are streaming */
            if ([[o_userSelections objectForKey:@"sap"] isEqualToString:@"YES"])
            {
                /* SAP-Announcement is requested */
                NSMutableString *o_sap_option = [NSMutableString stringWithString:@""];
                if([[o_userSelections objectForKey:@"sapText"] isEqualToString:@""])
                {
                    [o_sap_option appendString: @"sap"];
                }
                else
                {
                    [o_sap_option appendFormat: @"sap,name=\"%@\"",
                        [o_userSelections objectForKey:@"sapText"]];
                }
                if ([[[o_strmgMthds objectAtIndex:[[o_userSelections objectForKey: @"stmgMhd"] intValue]] objectAtIndex:0] isEqualToString:@"rtp"])
                {
                    /* RTP is no access out, but a stream out module */
                    [o_opts_string appendFormat:
                                             @":sout=#%@%@rtp{mux=%@,dst=%@,%@}",
                        o_duplicateCmd, o_trnscdCmd,
                        [[o_encapFormats objectAtIndex:[[o_userSelections objectForKey: @"encapFormat"] intValue]] objectAtIndex:0],
                        [o_userSelections objectForKey: @"stmgDest"],
                        o_sap_option];
                }
                else
                {
                    [o_opts_string appendFormat:
                                             @":sout=#%@%@standard{mux=%@,dst=%@,access=%@,%@}",
                        o_duplicateCmd, o_trnscdCmd,
                        [[o_encapFormats objectAtIndex:[[o_userSelections objectForKey: @"encapFormat"] intValue]] objectAtIndex:0],
                        [o_userSelections objectForKey: @"stmgDest"],
                        [[o_strmgMthds objectAtIndex:[[o_userSelections objectForKey: @"stmgMhd"] intValue]] objectAtIndex:0],
                        o_sap_option];
                }
            }
            else
            {
                /* no SAP, just streaming */
                if ([[[o_strmgMthds objectAtIndex:[[o_userSelections objectForKey: @"stmgMhd"] intValue]] objectAtIndex:0] isEqualToString:@"rtp"])
                {
                    /* RTP is different from the other protocols, as it isn't provided through an access out module anymore */
                    [o_opts_string appendFormat:
                                             @":sout=#%@%@rtp{mux=%@,dst=%@}",
                        o_duplicateCmd,
                        o_trnscdCmd,
                        [[o_encapFormats objectAtIndex:[[o_userSelections objectForKey: @"encapFormat"] intValue]] objectAtIndex:0],
                        [o_userSelections objectForKey: @"stmgDest"]];
                }
                else
                {
                    /* all other protocols are cool */
                    [o_opts_string appendFormat:
                                             @":sout=#%@%@standard{mux=%@,dst=%@,access=%@}",
                        o_duplicateCmd,
                        o_trnscdCmd,
                        [[o_encapFormats objectAtIndex:[[o_userSelections objectForKey: @"encapFormat"] intValue]] objectAtIndex:0],
                        [o_userSelections objectForKey: @"stmgDest"],
                        [[o_strmgMthds objectAtIndex:[[o_userSelections objectForKey: @"stmgMhd"] intValue]] objectAtIndex:0]];
                }
            }
        }

        /* check whether the user requested local playback. if yes, close the
         * string with an additional bracket */
        if ([[o_userSelections objectForKey:@"localPb"] isEqualToString:@"YES"])
        {
            [o_opts_string appendString: @"\"}"];
        }

        /* add subtitles to the video if desired */
        if ([[o_userSelections objectForKey:@"soverlay"] intValue] > 0)
            [o_opts_string appendString: @" --sout-transcode-soverlay"];

        [tempArray addObject: o_opts_string];

        o_opts_string = [NSMutableString stringWithString:@""];
        o_trnscdCmd = [NSMutableString stringWithString:@""];
        o_duplicateCmd = [NSMutableString stringWithString:@""];
        x += 1;
    }
    [o_userSelections setObject:[NSArray arrayWithArray: tempArray] forKey:@"opts"];
    [tempArray release];
}

- (IBAction)prevTab:(id)sender
{
    NSString * selectedTabViewItemLabel = [[o_tab_pageHolder selectedTabViewItem] label];

    if ([selectedTabViewItemLabel isEqualToString: @"Summary"])
    {
        /* check whether we are streaming or transcoding and go back */
        if ([[o_userSelections objectForKey:@"trnscdOrStrmg"] isEqualToString:@"strmg"])
        {
            /* show "Streaming 2" */
            [o_tab_pageHolder selectTabViewItemAtIndex:5];
        }else{
            /* show "Transcode 2" */
            [o_tab_pageHolder selectTabViewItemAtIndex:6];
        }
        /* rename the forward-button */
        [o_btn_forward setTitle: _NS("Next")];
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Transcode 2"])
    {
        /* show "Encap" */
        [o_tab_pageHolder selectTabViewItemAtIndex:4];
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Streaming 2"])
    {
        /* show "Encap" */
        [o_tab_pageHolder selectTabViewItemAtIndex:4];
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Encap"])
    {
        /* show "Transcode 1" */
        [o_tab_pageHolder selectTabViewItemAtIndex:3];
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Streaming 1"])
    {
        /* show "Input" */
        [o_tab_pageHolder selectTabViewItemAtIndex:1];
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Transcode 1"])
    {
        if ([[o_userSelections objectForKey:@"trnscdOrStrmg"] isEqualToString:@"strmg"])
        {
            /* show "Streaming 1" */
            [o_tab_pageHolder selectTabViewItemAtIndex:2];
        }else{
            /* show "Input" */
            [o_tab_pageHolder selectTabViewItemAtIndex:1];
        }
    }
    else if ([selectedTabViewItemLabel isEqualToString: @"Input"])
    {
        /* reset the wizard before going backwards. Otherwise, we might get
         * unwanted behaviour in the Encap-Selection */
        [self resetWizard];
        /* show "Hello" */
        [o_tab_pageHolder selectTabViewItemAtIndex:0];
        /* disable backwards-btn */
        [o_btn_backward setEnabled:NO];
    }
}

- (IBAction)t1_mrInfo_streaming:(id)sender
{
    /* show a sheet for the help */
    NSBeginInformationalAlertSheet(_NS("Stream to network"),
        _NS("OK"), @"", @"", o_wizard_window, nil, nil, nil, nil, @"%@",
        _NS("This allows streaming on a network."));
}

- (IBAction)t1_mrInfo_transcode:(id)sender
{
    /* show a sheet for the help */
    NSBeginInformationalAlertSheet(_NS("Transcode/Save to file"),
        _NS("OK"), @"", @"", o_wizard_window, nil, nil, nil, nil, @"%@",
        _NS("This allows saving a stream to a file. The "
        "can be reencoded on the fly. Whatever "
        "VLC can read can be saved.\nPlease note that VLC is not very suited "
        "for file to file transcoding. Its transcoding "
        "features are however useful to save network streams, for example."));
}

- (IBAction)t2_addNewStream:(id)sender
{
    NSOpenPanel * openPanel = [NSOpenPanel openPanel];
    SEL sel = @selector(t2_getNewStreamFromDialog:returnCode:contextInfo:);
    [openPanel beginSheetModalForWindow: o_wizard_window completionHandler: ^(NSInteger returnCode) {
        if (returnCode == NSOKButton)
            [o_t2_fld_pathToNewStrm setStringValue: [[openPanel URL] absoluteString]];
    }];
}

- (IBAction)t2_chooseStreamOrPlst:(id)sender
{
    /* enable and disable the respective items depending on user's choice */
    NSString *o_mode;
    o_mode = [[o_t2_matrix_inputSourceType selectedCell] title];

    if ([o_mode isEqualToString: _NS("Select a stream")])
    {
        [o_t2_btn_chooseFile setEnabled:YES];
        [o_t2_fld_pathToNewStrm setEnabled:YES];
        [o_t2_tbl_plst setEnabled:NO];
    } else {
        [o_t2_btn_chooseFile setEnabled:NO];
        [o_t2_fld_pathToNewStrm setEnabled:NO];
        [o_t2_tbl_plst setEnabled:YES];
    }
}

- (IBAction)t2_enableExtract:(id)sender
{
    /* enable/disable the respective items */
    if([o_t2_ckb_enblPartExtrct state] == NSOnState)
    {
        [o_t2_fld_prtExtrctFrom setEnabled:YES];
        [o_t2_fld_prtExtrctTo setEnabled:YES];
    } else {
        [o_t2_fld_prtExtrctFrom setEnabled:NO];
        [o_t2_fld_prtExtrctTo setEnabled:NO];
        [o_t2_fld_prtExtrctFrom setStringValue:@""];
        [o_t2_fld_prtExtrctTo setStringValue:@""];
    }
}

- (IBAction)t3_strmMthdChanged:(id)sender
{
    /* change the captions of o_t3_txt_destInfo according to the chosen
     * streaming method */
    int mode;
    mode = [[o_t3_matrix_stmgMhd selectedCell] tag];
    if (mode == 0)
    {
        /* HTTP */
        [o_t3_txt_destInfo setStringValue: [[o_strmgMthds objectAtIndex:0]
            objectAtIndex:2]];
        [o_t3_txt_strgMthdInfo setStringValue: [[o_strmgMthds objectAtIndex:0]
            objectAtIndex:3]];
    }
    else if (mode == 1)
    {
        /* MMS */
        [o_t3_txt_destInfo setStringValue: [[o_strmgMthds objectAtIndex:1]
            objectAtIndex:2]];
        [o_t3_txt_strgMthdInfo setStringValue: [[o_strmgMthds objectAtIndex:1]
            objectAtIndex:3]];
    }
    else if (mode == 2)
    {
        /* UDP-Unicast */
        [o_t3_txt_destInfo setStringValue: [[o_strmgMthds objectAtIndex:2]
            objectAtIndex:2]];
        [o_t3_txt_strgMthdInfo setStringValue: [[o_strmgMthds objectAtIndex:2]
        objectAtIndex:3]];
    }
    else if (mode == 3)
    {
        /* UDP-Multicast */
        [o_t3_txt_destInfo setStringValue: [[o_strmgMthds objectAtIndex:3]
            objectAtIndex:2]];
        [o_t3_txt_strgMthdInfo setStringValue: [[o_strmgMthds objectAtIndex:3]
        objectAtIndex:3]];
    }
    else if (mode == 4)
    {
        /* RTP-Unicast */
        [o_t3_txt_destInfo setStringValue: [[o_strmgMthds objectAtIndex:4]
            objectAtIndex:2]];
        [o_t3_txt_strgMthdInfo setStringValue: [[o_strmgMthds objectAtIndex:4]
            objectAtIndex:3]];
    }
    else if (mode == 5)
    {
        /* RTP-Multicast */
        [o_t3_txt_destInfo setStringValue: [[o_strmgMthds objectAtIndex:5]
            objectAtIndex:2]];
        [o_t3_txt_strgMthdInfo setStringValue: [[o_strmgMthds objectAtIndex:5]
        objectAtIndex:3]];
    }
}

- (IBAction)t4_AudCdcChanged:(id)sender
{
    /* update codec info */
    [o_t4_txt_hintAudio setStringValue:[[o_audioCodecs objectAtIndex:
        [[o_t4_pop_audioCodec selectedItem]tag]] objectAtIndex:2]];
}

- (IBAction)t4_enblAudTrnscd:(id)sender
{
    /* enable/disable the respective items */
    if([o_t4_ckb_audio state] == NSOnState)
    {
        [o_t4_pop_audioCodec setEnabled:YES];
        [o_t4_pop_audioBitrate setEnabled:YES];
        [o_t4_txt_hintAudio setStringValue: _NS("Select your audio codec. "
        "Click one to get more information.")];
    } else {
        [o_t4_pop_audioCodec setEnabled:NO];
        [o_t4_pop_audioBitrate setEnabled:NO];
        [o_t4_txt_hintAudio setStringValue: _NS("Enabling this allows transcoding "
        "the audio track if one is present in the stream.")];
    }
}

- (IBAction)t4_enblVidTrnscd:(id)sender
{
    /* enable/disable the respective items */
    if([o_t4_ckb_video state] == NSOnState)
    {
        [o_t4_pop_videoCodec setEnabled:YES];
        [o_t4_pop_videoBitrate setEnabled:YES];
        [o_t4_txt_hintVideo setStringValue: _NS("Select your video codec. "\
        "Click one to get more information.")];
    } else {
        [o_t4_pop_videoCodec setEnabled:NO];
        [o_t4_pop_videoBitrate setEnabled:NO];
        [o_t4_txt_hintVideo setStringValue: _NS("Enabling this allows transcoding "
        "the video track if one is present in the stream.")];

    }
}

- (IBAction)t4_VidCdcChanged:(id)sender
{
    /* update codec info */
    [o_t4_txt_hintVideo setStringValue:[[o_videoCodecs objectAtIndex:
        [[o_t4_pop_videoCodec selectedItem]tag]] objectAtIndex:2]];
}

- (IBAction)t6_enblSapAnnce:(id)sender
{
    /* enable/disable input fld */
    if([o_t6_ckb_sap state] == NSOnState)
    {
        [o_t6_fld_sap setEnabled:YES];
    } else {
        [o_t6_fld_sap setEnabled:NO];
        [o_t6_fld_sap setStringValue:@""];
    }
}

- (IBAction)t6_mrInfo_ttl:(id)sender
{
    /* show a sheet for the help */
    NSBeginInformationalAlertSheet(_NS("Time-To-Live (TTL)"),
        _NS("OK"), @"", @"", o_wizard_window, nil, nil, nil, nil, @"%@",
        _NS("This allows defining the TTL (Time-To-Live) of the stream. "
            "This parameter is the maximum number of routers your stream can "
            "go through. If you don't know what it means, or if you want to "
            "stream on your local network only, leave this setting to 1."));
}

- (IBAction)t6_mrInfo_sap:(id)sender
{
    /* show a sheet for the help */
    NSBeginInformationalAlertSheet(_NS("SAP Announcement"),
        _NS("OK"), @"", @"", o_wizard_window, nil, nil, nil, nil, @"%@",
        _NS("When streaming using UDP, the streams can be "
        "announced using the SAP/SDP announcing protocol. This "
        "way, the clients won't have to type in the multicast address, it "
        "will appear in their playlist if they enable the SAP extra "
        "interface.\nIf you want to give a name to your stream, enter it "
        "here, else, a default name will be used."));
}

- (IBAction)t67_mrInfo_local:(id)sender
{
    /* show a sheet for the help */
    NSBeginInformationalAlertSheet(_NS("Local playback"),
            _NS("OK"), @"", @"", o_wizard_window, nil, nil, nil, nil, @"%@",
            _NS("When this option is enabled, the stream will be both played "
            "and transcoded/streamed.\n\nNote that this requires much more "
            "CPU power than simple transcoding or streaming."));
}

- (IBAction)t7_selectTrnscdDestFile:(id)sender
{
    /* provide a save-to-dialogue, so the user can choose a location for
     * his/her new file. We take a modified NSOpenPanel to select a folder
     * and a plain NSSavePanel to save a single file. */

    if ([[o_userSelections objectForKey:@"pathToStrm"] count] > 1)
    {
        NSOpenPanel * saveFolderPanel = [[NSOpenPanel alloc] init];

        [saveFolderPanel setCanChooseDirectories: YES];
        [saveFolderPanel setCanChooseFiles: NO];
        [saveFolderPanel setCanSelectHiddenExtension: NO];
        [saveFolderPanel setCanCreateDirectories: YES];
        [saveFolderPanel beginSheetModalForWindow: o_wizard_window completionHandler:^(NSInteger returnCode) {
            if (returnCode == NSOKButton)
                [o_t7_fld_filePath setStringValue: [NSString stringWithFormat: @"%@/", [[saveFolderPanel URL] path]]];
        }];
        [saveFolderPanel release];
    }
    else
    {
        NSSavePanel * saveFilePanel = [[NSSavePanel alloc] init];

        /* don't use ".ps" as suffix, since the OSX Finder confuses our
         * creations with PostScript-files and wants to open them with
         * Preview.app */
        NSString * theEncapFormat = [[o_encapFormats objectAtIndex:
        [[o_userSelections objectForKey:@"encapFormat"] intValue]]
        objectAtIndex:0];
        if (![theEncapFormat isEqualToString:@"ps"])
            [saveFilePanel setAllowedFileTypes: [NSArray arrayWithObject:theEncapFormat]];
        else
            [saveFilePanel setAllowedFileTypes: [NSArray arrayWithObject:@"mpg"]];

        [saveFilePanel setCanSelectHiddenExtension: YES];
        [saveFilePanel setCanCreateDirectories: YES];
        [saveFilePanel beginSheetModalForWindow: o_wizard_window completionHandler:^(NSInteger returnCode) {
            if (returnCode == NSOKButton)
                [o_t7_fld_filePath setStringValue:[[saveFilePanel URL] path]];
        }];
        [saveFilePanel release];
    }
}

@end
