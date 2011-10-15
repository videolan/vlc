/*****************************************************************************
 * wizard.h: MacOS X Streaming Wizard
 *****************************************************************************
 * Copyright (C) 2005-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Kühne <fkuehne at videolan dot org>
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
 * VLCWizard
 *****************************************************************************/

#import <Cocoa/Cocoa.h>

@interface VLCWizard : NSObject
{
    /* general items */
    IBOutlet id o_btn_backward;
    IBOutlet id o_btn_cancel;
    IBOutlet id o_btn_forward;
    IBOutlet id o_tab_pageHolder;
    IBOutlet id o_wizard_window;
    IBOutlet id o_playlist_wizard;

    /* page one ("Hello") */
    IBOutlet id o_t1_btn_mrInfo_strmg;
    IBOutlet id o_t1_btn_mrInfo_trnscd;
    IBOutlet id o_t1_matrix_strmgOrTrnscd;
    IBOutlet id o_t1_txt_notice;
    IBOutlet id o_t1_txt_text;
    IBOutlet id o_t1_txt_title;

    /* page two ("Input") */
    IBOutlet id o_t2_box_prtExtrct;
    IBOutlet id o_t2_ckb_enblPartExtrct;
    IBOutlet id o_t2_btn_chooseFile;
    IBOutlet id o_t2_fld_pathToNewStrm;
    IBOutlet id o_t2_fld_prtExtrctFrom;
    IBOutlet id o_t2_fld_prtExtrctTo;
    IBOutlet id o_t2_matrix_inputSourceType;
    IBOutlet id o_t2_tbl_plst;
    IBOutlet id o_t2_text;
    IBOutlet id o_t2_title;
    IBOutlet id o_t2_txt_prtExtrctFrom;
    IBOutlet id o_t2_txt_prtExtrctTo;

    /* page one ("Streaming 1") */
    IBOutlet id o_t3_box_dest;
    IBOutlet id o_t3_box_strmgMthd;
    IBOutlet id o_t3_fld_address;
    IBOutlet id o_t3_matrix_stmgMhd;
    IBOutlet id o_t3_txt_destInfo;
    IBOutlet id o_t3_txt_text;
    IBOutlet id o_t3_txt_title;
    IBOutlet id o_t3_txt_strgMthdInfo;

    /* page four ("Transcode 1") */
    IBOutlet id o_t4_box_audio;
    IBOutlet id o_t4_box_video;
    IBOutlet id o_t4_ckb_audio;
    IBOutlet id o_t4_ckb_video;
    IBOutlet id o_t4_pop_audioBitrate;
    IBOutlet id o_t4_pop_audioCodec;
    IBOutlet id o_t4_pop_videoBitrate;
    IBOutlet id o_t4_pop_videoCodec;
    IBOutlet id o_t4_text;
    IBOutlet id o_t4_title;
    IBOutlet id o_t4_txt_audioBitrate;
    IBOutlet id o_t4_txt_videoBitrate;
    IBOutlet id o_t4_txt_audioCodec;
    IBOutlet id o_t4_txt_videoCodec;
    IBOutlet id o_t4_txt_hintAudio;
    IBOutlet id o_t4_txt_hintVideo;

    /* page five ("Encap") */
    IBOutlet id o_t5_matrix_encap;
    IBOutlet id o_t5_text;
    IBOutlet id o_t5_title;

    /* page six ("Streaming 2") */
    IBOutlet id o_t6_ckb_sap;
    IBOutlet id o_t6_fld_sap;
    IBOutlet id o_t6_fld_ttl;
    IBOutlet id o_t6_text;
    IBOutlet id o_t6_title;
    IBOutlet id o_t6_txt_ttl;
    IBOutlet id o_t6_btn_mrInfo_ttl;
    IBOutlet id o_t6_btn_mrInfo_sap;
    IBOutlet id o_t6_btn_mrInfo_local;
    IBOutlet id o_t6_ckb_soverlay;
    IBOutlet id o_t6_ckb_local;

    /* page seven ("Transcode 2") */
    IBOutlet id o_t7_btn_chooseFile;
    IBOutlet id o_t7_fld_filePath;
    IBOutlet id o_t7_text;
    IBOutlet id o_t7_title;
    IBOutlet id o_t7_txt_saveFileTo;
    IBOutlet id o_t7_btn_mrInfo_local;
    IBOutlet id o_t7_ckb_soverlay;
    IBOutlet id o_t7_ckb_local;

    /* page eight ("Summary") */
    IBOutlet id o_t8_fld_destination;
    IBOutlet id o_t8_fld_encapFormat;
    IBOutlet id o_t8_fld_inptStream;
    IBOutlet id o_t8_fld_partExtract;
    IBOutlet id o_t8_fld_sap;
    IBOutlet id o_t8_fld_saveFileTo;
    IBOutlet id o_t8_fld_strmgMthd;
    IBOutlet id o_t8_fld_trnscdAudio;
    IBOutlet id o_t8_fld_trnscdVideo;
    IBOutlet id o_t8_fld_soverlay;
    IBOutlet id o_t8_fld_ttl;
    IBOutlet id o_t8_fld_mrl;
    IBOutlet id o_t8_fld_local;
    IBOutlet id o_t8_txt_destination;
    IBOutlet id o_t8_txt_encapFormat;
    IBOutlet id o_t8_txt_inputStream;
    IBOutlet id o_t8_txt_partExtract;
    IBOutlet id o_t8_txt_sap;
    IBOutlet id o_t8_txt_saveFileTo;
    IBOutlet id o_t8_txt_strmgMthd;
    IBOutlet id o_t8_txt_text;
    IBOutlet id o_t8_txt_title;
    IBOutlet id o_t8_txt_trnscdAudio;
    IBOutlet id o_t8_txt_trnscdVideo;
    IBOutlet id o_t8_txt_soverlay;
    IBOutlet id o_t8_txt_ttl;
    IBOutlet id o_t8_txt_mrl;
    IBOutlet id o_t8_txt_local;

    NSMutableDictionary * o_userSelections;
    NSArray * o_videoCodecs;
    NSArray * o_audioCodecs;
    NSArray * o_encapFormats;
    NSArray * o_strmgMthds;
    NSString * o_opts;
}
- (IBAction)cancelRun:(id)sender;
- (IBAction)nextTab:(id)sender;
- (IBAction)prevTab:(id)sender;
- (IBAction)t1_mrInfo_streaming:(id)sender;
- (IBAction)t1_mrInfo_transcode:(id)sender;
- (IBAction)t2_addNewStream:(id)sender;
- (IBAction)t2_chooseStreamOrPlst:(id)sender;
- (IBAction)t2_enableExtract:(id)sender;
- (IBAction)t3_strmMthdChanged:(id)sender;
- (IBAction)t4_AudCdcChanged:(id)sender;
- (IBAction)t4_enblAudTrnscd:(id)sender;
- (IBAction)t4_enblVidTrnscd:(id)sender;
- (IBAction)t4_VidCdcChanged:(id)sender;
- (IBAction)t6_enblSapAnnce:(id)sender;
- (IBAction)t6_mrInfo_ttl:(id)sender;
- (IBAction)t6_mrInfo_sap:(id)sender;
- (IBAction)t67_mrInfo_local:(id)sender;
- (IBAction)t7_selectTrnscdDestFile:(id)sender;

+ (VLCWizard *)sharedInstance;

- (void)showWizard;
- (void)showSummary;
- (void)resetWizard;
- (void)createOpts;
- (void)rebuildCodecMenus;
- (id)playlistWizard;
- (void)initWithExtractValuesFrom: (NSString *)from to: (NSString *)to ofItem: (NSString *)item;

@end
