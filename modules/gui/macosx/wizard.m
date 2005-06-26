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
 * Note: this code is based upon ../wxwindows/wizard.cpp,
 *       written by Clément Stenac.
 *****************************************************************************/ 

/* TODO:
    - implementation of the logic, i.e. handling of the collected values, respective manipulation of the GUI, start of the stream
    - move some arrays to an external header file
    - some GUI things (e.g. radio buttons on page 2, etc. - see FIXMEs)
    - l10n string fixes (both in OSX and WX) */

 
/*****************************************************************************
 * Preamble
 *****************************************************************************/ 
#import "wizard.h"
#import "intf.h"


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

- (void)awakeFromNib
{
    /* some minor cleanup */
    [o_t2_tbl_plst setEnabled:NO];
	[o_wizardhelp_window setExcludedFromWindowsMenu:YES];
	

    /* FIXME: make the both arrays global */

    /* add audio-codecs for transcoding */
    NSArray * audioBitratesArray;
    audioBitratesArray = [NSArray arrayWithObjects: @"512", @"256", @"192", @"128", @"64", @"32", @"16", nil ];
    [o_t4_pop_audioBitrate removeAllItems];
    [o_t4_pop_audioBitrate addItemsWithTitles: audioBitratesArray];
    [o_t4_pop_audioBitrate selectItemWithTitle: @"192"];
    
    /* add video-codecs for transcoding */
    NSArray * videoBitratesArray;
    videoBitratesArray = [NSArray arrayWithObjects: @"3072", @"2048", @"1024", @"768", @"512", @"256", @"192", @"128", @"64", @"32", @"16", nil ];
    [o_t4_pop_videoBitrate removeAllItems];
    [o_t4_pop_videoBitrate addItemsWithTitles: videoBitratesArray];
    [o_t4_pop_videoBitrate selectItemWithTitle: @"1024"];
    
    /* FIXME: fill the codec-popups as well */
}

- (void)showWizard
{
    /* just present the window to the user */
    /* we might need a method to reset the window first */
    [o_tab_pageHolder selectFirstTabViewItem:self];
    
    [o_wizard_window center];
    [o_wizard_window displayIfNeeded];
    [o_wizard_window makeKeyAndOrderFront:nil];
}

- (void)initStrings
{
    /* localise all strings to the users lang */
    /* method is called from intf.m (in method openWizard) */
    
    /* general items */
    [o_btn_backward setTitle: _NS("< Back")];
    [o_btn_cancel setTitle: _NS("Cancel")];
    [o_btn_forward setTitle: _NS("Next >")];
    [o_wizard_window setTitle: _NS("Streaming/Transcoding Wizard")];
    
    /* page one ("Hello") */
    [o_t1_txt_title setStringValue: _NS("Streaming/Transcoding Wizard")];
    [o_t1_txt_text setStringValue: _NS("This wizard helps you to stream, transcode or save a stream")];
    [o_t1_btn_mrInfo_strmg setTitle: _NS("More Info")];
    [o_t1_btn_mrInfo_trnscd setTitle: _NS("More Info")];
    [o_t1_txt_notice setStringValue: _NS("This wizard only gives access to a small subset of VLC's streaming and transcoding capabilities. Use the Open and Stream Output dialogs to get all of them")];
    [o_t1_rdo_streaming setTitle: _NS("Stream to network")];
    [o_t1_rdo_transcode setTitle: _NS("Transcode/Save to file")];
    
    /* page two ("Input") */
    [o_t2_title setStringValue: _NS("Choose input")];
    [o_t2_text setStringValue: _NS("Choose here your input stream")];
    [o_t2_rdo_newStrm setTitle: _NS("Select a stream")];
    [o_t2_rdo_exstPlstItm setTitle: _NS("Existing playlist item")];
    [o_t2_btn_chooseFile setTitle: _NS("Choose...")];
    [[[o_t2_tbl_plst tableColumnWithIdentifier:@"name"] headerCell] setStringValue: _NS("Name")];
    [o_t2_box_prtExtrct setTitle: _NS("Partial Extract")];
    [o_t2_ckb_enblPartExtrct setTitle: _NS("Enable")];
    [o_t2_txt_prtExtrctFrom setStringValue: _NS("From")];
    [o_t2_txt_prtExtrctTo setStringValue: _NS("To")];
    
    /* page three ("Streaming 1") */
    [o_t3_txt_title setStringValue: _NS("Streaming")];
    [o_t3_txt_text setStringValue: _NS("In this page, you will select how your input stream will be sent.")];
    [o_t3_box_dest setTitle: _NS("Destination")];
    [o_t3_box_strmgMthd setTitle: _NS("Streaming method")];
    [o_t3_txt_destInfo setStringValue: _NS("Enter the address of the computer to stream to")];
    [[o_t3_matrix_stmgMhd cellAtRow:1 column:0] setTitle: _NS("UDP Unicast")];
    [[o_t3_matrix_stmgMhd cellAtRow:1 column:1] setTitle: _NS("UDP Multicast")];
    
    /* page four ("Transcode 1") */
    [o_t4_title setStringValue: _NS("Transcode")];
    [o_t4_text setStringValue: _NS("If you want to change the compression format of the audio or video tracks, fill in this page. (If you only want to change the container format, proceed to next page).")];
    [o_t4_box_audio setTitle: _NS("Audio")];
    [o_t4_box_video setTitle: _NS("Video")];
    [o_t4_ckb_audio setTitle: _NS("Transcode audio")];
    [o_t4_ckb_video setTitle: _NS("Transcode video")];
    [o_t4_txt_videoBitrate setStringValue: _NS("Bitrate (kb/s)")];
    [o_t4_txt_videoCodec setStringValue: _NS("Codec")];
    [o_t4_txt_hintAudio setStringValue: _NS("If your stream has audio and you want to " \
                         "transcode it, enable this")];
    [o_t4_txt_hintVideo setStringValue: _NS("If your stream has video and you want to " \
                         "transcode it, enable this")];
    
    /* page five ("Encap") */
    [o_t5_title setStringValue: _NS("Encapsulation format")];
    [o_t5_text setStringValue: _NS("In this page, you will select how the stream will be "\
                     "encapsulated. Depending on the choices you made, all "\
                     "formats won't be available.")];
    
    /* page six ("Streaming 2") */
    [o_t6_title setStringValue: _NS("Additional streaming options")];
    [o_t6_text setStringValue: _NS("In this page, you will define a few " \
                              "additional parameters for your stream.")];
    [o_t6_txt_ttl setStringValue: _NS("Time-To-Live (TTL)")];
    [o_t6_btn_mrInfo_ttl setTitle: _NS("More Info")];
    [o_t6_ckb_sap setTitle: _NS("SAP Announce")];
    [o_t6_btn_mrInfo_sap setTitle: _NS("More Info")];
     
    /* page seven ("Transcode 2") */
    [o_t7_title setStringValue: _NS("Additional transcode options")];
    [o_t7_text setStringValue: _NS("In this page, you will define a few " \
                              "additionnal parameters for your transcoding.")];
    [o_t7_txt_saveFileTo setStringValue: _NS("Select the file to save to")];
    [o_t7_btn_chooseFile setTitle: _NS("Choose...")];
	
	/* wizard help window */
	[o_wh_btn_okay setTitle: _NS("OK")];
}

- (IBAction)cancelRun:(id)sender
{
    [o_wizard_window close];
}

- (IBAction)nextTab:(id)sender
{
    /* only a stub atm; needs to be implemented correctly later on */
    [o_tab_pageHolder selectNextTabViewItem:self];
}

- (IBAction)prevTab:(id)sender
{
    /* only a stub atm; needs to be implemented correctly later on */
    [o_tab_pageHolder selectPreviousTabViewItem:self];
}

- (IBAction)t1_mrInfo_streaming:(id)sender
{
    /* show a sheet for the help */
	/* since NSAlert does not exist on OSX < 10.3, we use our own implementation */
	[o_wh_txt_title setStringValue: _NS("Stream to network")];
	[o_wh_txt_text setStringValue: _NS("Use this to stream on a network.")];
	[NSApp beginSheet: o_wizardhelp_window
            modalForWindow: o_wizard_window
            modalDelegate: o_wizardhelp_window
            didEndSelector: nil
            contextInfo: nil];
}

- (IBAction)t1_mrInfo_transcode:(id)sender
{
    /* show a sheet for the help */
	[o_wh_txt_title setStringValue: _NS("Transcode/Save to file")];
	[o_wh_txt_text setStringValue: _NS("Use this to save a stream to a file. You "\
			"have the possibility to reencode the stream. You can save whatever "\
			"VLC can read.\nPlease notice that VLC is not very suited " \
			"for file to file transcoding. You should use its transcoding " \
            "features to save network streams, for example.")];
	[NSApp beginSheet: o_wizardhelp_window
            modalForWindow: o_wizard_window
            modalDelegate: o_wizardhelp_window
            didEndSelector: nil
            contextInfo: nil];
}

- (IBAction)t2_addNewStream:(id)sender
{
    NSOpenPanel * openPanel = [NSOpenPanel openPanel];
    SEL sel = @selector(t2_getNewStreamFromDialog:returnCode:contextInfo:);
    [openPanel beginSheetForDirectory:@"~" file:nil types:nil modalForWindow:o_wizard_window modalDelegate:self didEndSelector:sel contextInfo:nil];
}

- (void)t2_getNewStreamFromDialog: (NSOpenPanel *)sheet returnCode: (int)returnCode contextInfo: (void *)contextInfo
{
    if (returnCode == NSOKButton)
    {
        [o_t2_fld_pathToNewStrm setStringValue:[sheet filename]];
        /* FIXME: store path in a global variable */
    }
}

- (IBAction)t2_chooseStreamOrPlst:(id)sender
{
    /* enable and disable the respective items depending on user's choice */
    /* TODO */
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
    }
}

- (IBAction)t3_addressEntered:(id)sender
{
    /* check whether the entered address is valid */
}

- (IBAction)t4_AudCdcChanged:(id)sender
{
    /* update codec info */
}

- (IBAction)t4_enblAudTrnscd:(id)sender
{
    /* enable/disable the respective items */
    if([o_t4_ckb_audio state] == NSOnState)
    {
        [o_t4_pop_audioCodec setEnabled:YES];
        
        [o_t4_pop_audioBitrate setEnabled:YES];
    } else {
        [o_t4_pop_audioCodec setEnabled:NO];
        [o_t4_pop_audioBitrate setEnabled:NO];
    }
}

- (IBAction)t4_enblVidTrnscd:(id)sender
{
    /* enable/disable the respective items */
    if([o_t4_ckb_video state] == NSOnState)
    {
        [o_t4_pop_videoCodec setEnabled:YES];
        [o_t4_pop_videoBitrate setEnabled:YES];
    } else {
        [o_t4_pop_videoCodec setEnabled:NO];
        [o_t4_pop_videoBitrate setEnabled:NO];
    }
}

- (IBAction)t4_VidCdcChanged:(id)sender
{
    /* update codec info */
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
	[o_wh_txt_title setStringValue: _NS("Time-To-Live (TTL)")];
	[o_wh_txt_text setStringValue: _NS("Define the TTL (Time-To-Live) of the stream. "\
			"This parameter is the maximum number of routers your stream can go "
			"through. If you don't know what it means, or if you want to stream on " \
			"your local network only, leave this setting to 1.")];
	[NSApp beginSheet: o_wizardhelp_window
            modalForWindow: o_wizard_window
            modalDelegate: o_wizardhelp_window
            didEndSelector: nil
            contextInfo: nil];
}

- (IBAction)t6_mrInfo_sap:(id)sender
{
    /* show a sheet for the help */
	[o_wh_txt_title setStringValue: _NS("SAP Announce")];
	[o_wh_txt_text setStringValue: _NS("When streaming using UDP, you can " \
		"announce your streams using the SAP/SDP announcing protocol. This " \
		"way, the clients won't have to type in the multicast address, it " \
		"will appear in their playlist if they enable the SAP extra interface.\n" \
		"If you want to give a name to your stream, enter it here, " \
		"else, a default name will be used.")];
	[NSApp beginSheet: o_wizardhelp_window
            modalForWindow: o_wizard_window
            modalDelegate: o_wizardhelp_window
            didEndSelector: nil
            contextInfo: nil];
}

- (IBAction)t7_selectTrnscdDestFile:(id)sender
{
    /* provide a save-to-dialogue, so the user can choose a location for his/her new file */
    NSSavePanel * savePanel = [NSSavePanel savePanel];
    SEL sel = @selector(t7_getTrnscdDestFile:returnCode:contextInfo:);
    [savePanel beginSheetForDirectory:@"~" file:nil modalForWindow:o_wizard_window modalDelegate:self didEndSelector:sel contextInfo:nil];
    /* FIXME: insert a suffix in file depending on the chosen encap-format */
}

- (void)t7_getTrnscdDestFile: (NSSavePanel *)sheet returnCode: (int)returnCode contextInfo: (void *)contextInfo
{
    if (returnCode == NSOKButton)
    {
        [o_t7_fld_filePath setStringValue:[sheet filename]];
        /* FIXME: store path in a global variable and add a suffix depending on the chosen encap-format, if needed */
    }
}

- (IBAction)wh_closeSheet:(id)sender
{
	/* close the help sheet */
	[NSApp endSheet:o_wizardhelp_window];
	[o_wizardhelp_window close];
}

@end
