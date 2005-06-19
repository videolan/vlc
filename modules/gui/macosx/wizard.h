/*****************************************************************************
 * wizard.h: MacOS X Streaming Wizard
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id$
 *
 * Authors: Felix Kühne <fkuehne@users.sf.net> 
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

/*****************************************************************************
 * VLCWizard 
 *****************************************************************************/
 
#import <Cocoa/Cocoa.h>

@interface VLCWizard : NSObject
{
    IBOutlet id o_btn_backward;
    IBOutlet id o_btn_cancel;
    IBOutlet id o_btn_forward;
    IBOutlet id o_t1_btn_mrInfo_strmg;
    IBOutlet id o_t1_btn_mrInfo_trnscd;
    IBOutlet id o_t1_rdo_streaming;
    IBOutlet id o_t1_rdo_transcode;
    IBOutlet id o_t1_txt_notice;
    IBOutlet id o_t1_txt_text;
    IBOutlet id o_t1_txt_title;
    IBOutlet id o_t2_box_prtExtrct;
    IBOutlet id o_t2_ckb_enblPartExtrct;
    IBOutlet id o_t2_btn_chooseFile;
    IBOutlet id o_t2_fld_pathToNewStrm;
    IBOutlet id o_t2_fld_prtExtrctFrom;
    IBOutlet id o_t2_fld_prtExtrctTo;
    IBOutlet id o_t2_rdo_exstPlstItm;
    IBOutlet id o_t2_rdo_newStrm;
    IBOutlet id o_t2_tbl_plst;
    IBOutlet id o_t2_text;
    IBOutlet id o_t2_title;
    IBOutlet id o_t2_txt_prtExtrctFrom;
    IBOutlet id o_t2_txt_prtExtrctTo;
    IBOutlet id o_t3_box_dest;
    IBOutlet id o_t3_box_strmgMthd;
    IBOutlet id o_t3_fld_address;
    IBOutlet id o_t3_matrix_stmgMhd;
    IBOutlet id o_t3_txt_destInfo;
    IBOutlet id o_t3_txt_text;
    IBOutlet id o_t3_txt_title;
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
    IBOutlet id o_t5_matrix_encap;
    IBOutlet id o_t5_text;
    IBOutlet id o_t5_title;
    IBOutlet id o_t6_ckb_sap;
    IBOutlet id o_t6_fld_sap;
    IBOutlet id o_t6_fld_ttl;
    IBOutlet id o_t6_text;
    IBOutlet id o_t6_title;
    IBOutlet id o_t6_txt_ttl;
    IBOutlet id o_t6_btn_mrInfo_ttl;
    IBOutlet id o_t6_btn_mrInfo_sap;
    IBOutlet id o_t7_btn_chooseFile;
    IBOutlet id o_t7_fld_filePath;
    IBOutlet id o_t7_text;
    IBOutlet id o_t7_title;
    IBOutlet id o_t7_txt_saveFileTo;
    IBOutlet id o_tab_pageHolder;
    IBOutlet id o_wizard_window;
}
- (IBAction)cancelRun:(id)sender;
- (IBAction)nextTab:(id)sender;
- (IBAction)prevTab:(id)sender;
- (IBAction)t1_mrInfo_streaming:(id)sender;
- (IBAction)t1_mrInfo_transcode:(id)sender;
- (IBAction)t2_addNewStream:(id)sender;
- (IBAction)t2_chooseStreamOrPlst:(id)sender;
- (IBAction)t2_enableExtract:(id)sender;
- (IBAction)t3_addressEntered:(id)sender;
- (IBAction)t4_AudCdcChanged:(id)sender;
- (IBAction)t4_enblAudTrnscd:(id)sender;
- (IBAction)t4_enblVidTrnscd:(id)sender;
- (IBAction)t4_VidCdcChanged:(id)sender;
- (IBAction)t6_enblSapAnnce:(id)sender;
- (IBAction)t6_mrInfo_ttl:(id)sender;
- (IBAction)t6_mrInfo_sap:(id)sender;
- (IBAction)t7_selectTrnscdDestFile:(id)sender;

+ (VLCWizard *)sharedInstance;
- (void)showWizard;
@end
