/*****************************************************************************
 * open.m: Open dialogues for VLC's MacOS X port
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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
 * Preamble
 *****************************************************************************/
#import <stdlib.h>                                      /* malloc(), free() */
#import <sys/param.h>                                    /* for MAXPATHLEN */

#import "CompatibilityFixes.h"

#import <paths.h>
#import <IOKit/IOBSD.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOCDMedia.h>
#import <IOKit/storage/IODVDMedia.h>
#import <IOKit/storage/IOBDMedia.h>
#import <Cocoa/Cocoa.h>
#import <QTKit/QTKit.h>

#import "intf.h"
#import "playlist.h"
#import "open.h"
#import "output.h"
#import "eyetv.h"
#import "misc.h"

#import <vlc_url.h>

NSArray *qtkvideoDevices;
NSArray *qtkaudioDevices;
#define setEyeTVUnconnected \
[o_capture_lbl setStringValue: _NS("No device is selected")]; \
[o_capture_long_lbl setStringValue: _NS("No device is selected.\n\nChoose available device in above pull-down menu.\n")]; \
[o_capture_lbl displayIfNeeded]; \
[o_capture_long_lbl displayIfNeeded]; \
[self showCaptureView: o_capture_label_view]

struct display_info_t
{
    CGRect rect;
    CGDirectDisplayID id;
};

/*****************************************************************************
 * VLCOpen implementation
 *****************************************************************************/
@implementation VLCOpen

@synthesize fileSubDelay, fileSubFps;

#pragma mark -
#pragma mark Init

static VLCOpen *_o_sharedMainInstance = nil;

+ (VLCOpen *)sharedInstance
{
    return _o_sharedMainInstance ? _o_sharedMainInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedMainInstance)
        [self dealloc];
    else
        _o_sharedMainInstance = [super init];

    return _o_sharedMainInstance;
}

- (void)dealloc
{
    [o_allMediaDevices release];
    [o_specialMediaFolders release];
    if (o_opticalDevices)
        [o_opticalDevices release];
    if (o_file_slave_path)
        [o_file_slave_path release];
    [o_mrl release];
    if (o_sub_path)
        [o_sub_path release];
    [o_currentOpticalMediaIconView release];
    [o_currentOpticalMediaView release];
    for (int i = 0; i < [o_displayInfos count]; i ++) {
        NSValue *v = [o_displayInfos objectAtIndex:i];
        free([v pointerValue]);
    }
    [o_displayInfos removeAllObjects];
    [o_displayInfos release];

    [super dealloc];
}

- (void)awakeFromNib
{
    if (!OSX_SNOW_LEOPARD)
        [o_panel setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [o_panel setTitle: _NS("Open Source")];
    [o_mrl_lbl setStringValue: _NS("Media Resource Locator (MRL)")];

    [o_btn_ok setTitle: _NS("Open")];
    [o_btn_cancel setTitle: _NS("Cancel")];

    [[o_tabview tabViewItemAtIndex: 0] setLabel: _NS("File")];
    [o_tabview accessibilitySetOverrideValue:_NS("4 Tabs to choose between media input. Select 'File' for files, 'Disc' for optical media such as DVDs, Audio CDs or BRs, 'Network' for network streams or 'Capture' for Input Devices such as microphones or cameras, the current screen or TV streams if the EyeTV application is installed.") forAttribute:NSAccessibilityDescriptionAttribute];
    [[o_tabview tabViewItemAtIndex: 1] setLabel: _NS("Disc")];
    [[o_tabview tabViewItemAtIndex: 2] setLabel: _NS("Network")];
    [[o_tabview tabViewItemAtIndex: 3] setLabel: _NS("Capture")];
    [o_file_name setStringValue: @""];
    [o_file_name_stub setStringValue: _NS("Choose a file")];
    [o_file_icon_well setImage: [NSImage imageNamed:@"generic"]];
    [o_file_btn_browse setTitle: _NS("Browse...")];
    [[o_file_btn_browse cell] accessibilitySetOverrideValue:_NS("Click to select a file for playback") forAttribute:NSAccessibilityDescriptionAttribute];
    [o_file_stream setTitle: _NS("Treat as a pipe rather than as a file")];
    [o_file_stream setHidden: NO];
    [o_file_slave_ckbox setTitle: _NS("Play another media synchronously")];
    [o_file_slave_select_btn setTitle: _NS("Choose...")];
    [[o_file_btn_browse cell] accessibilitySetOverrideValue:_NS("Click to select a another file to play it in sync with the previously selected file.") forAttribute:NSAccessibilityDescriptionAttribute];
    [o_file_slave_filename_lbl setStringValue: @""];
    [o_file_slave_icon_well setImage: NULL];
    [o_file_subtitles_filename_lbl setStringValue: @""];
    [o_file_subtitles_icon_well setImage: NULL];
    [o_file_custom_timing_ckb setTitle: _NS("Custom playback")];
    [o_file_starttime_lbl setStringValue: _NS("Start time")];
    [o_file_starttime_fld setStringValue: @""];
    [o_file_stoptime_lbl setStringValue: _NS("Stop time")];
    [o_file_stoptime_fld setStringValue: @""];

    [o_disc_selector_pop removeAllItems];
    [o_disc_selector_pop setHidden: NO];
    NSString *o_videots = _NS("Open VIDEO_TS folder");
    NSString *o_bdmv = _NS("Open BDMV folder");
    [o_disc_nodisc_lbl setStringValue: _NS("Insert Disc")];
    [o_disc_nodisc_videots_btn setTitle: o_videots];
    [o_disc_nodisc_bdmv_btn setTitle: o_bdmv];
    [o_disc_audiocd_lbl setStringValue: _NS("Audio CD")];
    [o_disc_audiocd_trackcount_lbl setStringValue: @""];
    [o_disc_audiocd_videots_btn setTitle: o_videots];
    [o_disc_audiocd_bdmv_btn setTitle: o_bdmv];
    [o_disc_dvd_lbl setStringValue: @""];
    [o_disc_dvd_disablemenus_btn setTitle: _NS("Disable DVD menus")];
    [o_disc_dvd_videots_btn setTitle: o_videots];
    [o_disc_dvd_bdmv_btn setTitle: o_bdmv];
    [o_disc_dvdwomenus_lbl setStringValue: @""];
    [o_disc_dvdwomenus_enablemenus_btn setTitle: _NS("Enable DVD menus")];
    [o_disc_dvdwomenus_videots_btn setTitle: o_videots];
    [o_disc_dvdwomenus_bdmv_btn setTitle: o_bdmv];
    [o_disc_dvdwomenus_title_lbl setStringValue: _NS("Title")];
    [o_disc_dvdwomenus_chapter_lbl setStringValue: _NS("Chapter")];
    [o_disc_vcd_title_lbl setStringValue: _NS("Title")];
    [o_disc_vcd_chapter_lbl setStringValue: _NS("Chapter")];
    [o_disc_vcd_videots_btn setTitle: o_videots];
    [o_disc_vcd_bdmv_btn setTitle: o_bdmv];
    [o_disc_bd_videots_btn setTitle: o_videots];
    [o_disc_bd_bdmv_btn setTitle: o_bdmv];

    [o_net_udp_port_lbl setStringValue: _NS("Port")];
    [o_net_udpm_addr_lbl setStringValue: _NS("IP Address")];
    [o_net_udpm_port_lbl setStringValue: _NS("Port")];
    [o_net_http_url_lbl setStringValue: _NS("URL")];
    [o_net_help_lbl setStringValue: _NS("To Open a usual network stream (HTTP, RTSP, RTMP, MMS, FTP, etc.), just enter the URL in the field above. If you want to open a RTP or UDP stream, press the button below.")];
    [o_net_help_udp_lbl setStringValue: _NS("If you want to open a multicast stream, enter the respective IP address given by the stream provider. In unicast mode, VLC will use your machine's IP automatically.\n\nTo open a stream using a different protocol, just press Cancel to close this sheet.")];
    [[o_net_http_url cell] accessibilitySetOverrideValue:_NS("Enter a URL here to open the network stream. To open RTP or UDP streams, click on the respective button below.") forAttribute:NSAccessibilityDescriptionAttribute];
    [o_net_udp_cancel_btn setTitle: _NS("Cancel")];
    [o_net_udp_ok_btn setTitle: _NS("Open")];
    [o_net_openUDP_btn setTitle: _NS("Open RTP/UDP Stream")];
    [o_net_udp_mode_lbl setStringValue: _NS("Mode")];
    [o_net_udp_protocol_lbl setStringValue: _NS("Protocol")];
    [o_net_udp_address_lbl setStringValue: _NS("Address")];

    [[o_net_mode cellAtRow:0 column:0] setTitle: _NS("Unicast")];
    [[o_net_mode cellAtRow:1 column:0] setTitle: _NS("Multicast")];

    [o_net_udp_port setIntValue: config_GetInt(VLCIntf, "server-port")];
    [o_net_udp_port_stp setIntValue: config_GetInt(VLCIntf, "server-port")];

    [o_eyetv_chn_bgbar setUsesThreadedAnimation: YES];

    [o_capture_mode_pop removeAllItems];
    [o_capture_mode_pop addItemWithTitle: _NS("Input Devices")];
    [o_capture_mode_pop addItemWithTitle: _NS("Screen")];
    [o_capture_mode_pop addItemWithTitle: @"EyeTV"];
    [o_screen_long_lbl setStringValue: _NS("This input allows you to save, stream or display your current screen contents.")];
    [o_screen_fps_lbl setStringValue: [NSString stringWithFormat:@"%@:",_NS("Frames per Second")]];
    [o_screen_screen_lbl setStringValue: [NSString stringWithFormat:@"%@:",_NS("Screen")]];
    [o_screen_left_lbl setStringValue: [NSString stringWithFormat:@"%@:",_NS("Subscreen left")]];
    [o_screen_top_lbl setStringValue: [NSString stringWithFormat:@"%@:",_NS("Subscreen top")]];
    [o_screen_width_lbl setStringValue: [NSString stringWithFormat:@"%@:",_NS("Subscreen width")]];
    [o_screen_height_lbl setStringValue: [NSString stringWithFormat:@"%@:",_NS("Subscreen height")]];
    [o_screen_follow_mouse_ckb setTitle: _NS("Follow the mouse")];
    [o_screen_qtk_audio_ckb setTitle: _NS("Capture Audio")];
    [o_eyetv_currentChannel_lbl setStringValue: _NS("Current channel:")];
    [o_eyetv_previousProgram_btn setTitle: _NS("Previous Channel")];
    [o_eyetv_nextProgram_btn setTitle: _NS("Next Channel")];
    [o_eyetv_chn_status_txt setStringValue: _NS("Retrieving Channel Info...")];
    [o_eyetv_noInstance_lbl setStringValue: _NS("EyeTV is not launched")];
    [o_eyetv_noInstanceLong_lbl setStringValue: _NS("VLC could not connect to EyeTV.\nMake sure that you installed VLC's EyeTV plugin.")];
    [o_eyetv_launchEyeTV_btn setTitle: _NS("Launch EyeTV now")];
    [o_eyetv_getPlugin_btn setTitle: _NS("Download Plugin")];
    [o_capture_width_lbl setStringValue: [NSString stringWithFormat:@"%@:",_NS("Image width")]];
    [o_capture_height_lbl setStringValue: [NSString stringWithFormat:@"%@:",_NS("Image height")]];

    // setup start / stop time fields
    [o_file_starttime_fld setFormatter:[[[PositionFormatter alloc] init] autorelease]];
    [o_file_stoptime_fld setFormatter:[[[PositionFormatter alloc] init] autorelease]];

    [self qtkvideoDevices];
    [o_qtk_video_device_pop removeAllItems];
    msg_Dbg(VLCIntf, "Found %lu video capture devices", [qtkvideoDevices count]);

    if ([qtkvideoDevices count] >= 1) {
        if (!qtk_currdevice_uid)
            qtk_currdevice_uid = [[[QTCaptureDevice defaultInputDeviceWithMediaType: QTMediaTypeVideo] uniqueID]
                                                                stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

        NSUInteger deviceCount = [qtkvideoDevices count];
        for (int ivideo = 0; ivideo < deviceCount; ivideo++) {
            QTCaptureDevice *qtk_device;
            qtk_device = [qtkvideoDevices objectAtIndex:ivideo];
            [o_qtk_video_device_pop addItemWithTitle: [qtk_device localizedDisplayName]];

            if ([[[qtk_device uniqueID]stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] isEqualToString:qtk_currdevice_uid])
                [o_qtk_video_device_pop selectItemAtIndex:ivideo];
        }
    } else {
        [o_qtk_video_device_pop addItemWithTitle: _NS("None")];
        [qtk_currdevice_uid release];
    }

    [self qtkaudioDevices];
    [o_qtk_audio_device_pop removeAllItems];
    [o_screen_qtk_audio_pop removeAllItems];
    msg_Dbg(VLCIntf, "Found %lu audio capture devices", [qtkaudioDevices count]);

    if ([qtkaudioDevices count] >= 1) {
        if (!qtkaudio_currdevice_uid)
            qtkaudio_currdevice_uid = [[[QTCaptureDevice defaultInputDeviceWithMediaType: QTMediaTypeSound] uniqueID]
                                  stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

        NSUInteger deviceCount = [qtkaudioDevices count];
        for (int iaudio = 0; iaudio < deviceCount; iaudio++) {
            QTCaptureDevice *qtkaudio_device;
            qtkaudio_device = [qtkaudioDevices objectAtIndex:iaudio];
            [o_qtk_audio_device_pop addItemWithTitle: [qtkaudio_device localizedDisplayName]];
            [o_screen_qtk_audio_pop addItemWithTitle: [qtkaudio_device localizedDisplayName]];
            if ([[[qtkaudio_device uniqueID]stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] isEqualToString:qtkaudio_currdevice_uid]) {
                [o_qtk_audio_device_pop selectItemAtIndex:iaudio];
                [o_screen_qtk_audio_pop selectItemAtIndex:iaudio];
            }
        }
    } else {
        [o_qtk_audio_device_pop addItemWithTitle: _NS("None")];
        [o_screen_qtk_audio_pop addItemWithTitle: _NS("None")];
        [qtkaudio_currdevice_uid release];
    }

    [self setSubPanel];

    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(openNetInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_net_udp_port];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(openNetInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_net_udpm_addr];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(openNetInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_net_udpm_port];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(openNetInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_net_http_url];

    [[NSDistributedNotificationCenter defaultCenter] addObserver: self
                                                        selector: @selector(eyetvChanged:)
                                                            name: NULL
                                                          object: @"VLCEyeTVSupport"
                                              suspensionBehavior: NSNotificationSuspensionBehaviorDeliverImmediately];

    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(screenFPSfieldChanged:)
                                                 name: NSControlTextDidChangeNotification
                                               object: o_screen_fps_fld];

    /* register clicks on text fields */
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(textFieldWasClicked:)
                                                 name: @"VLCOpenTextFieldWasClicked"
                                               object: nil];

    /* we want to be notified about removed or added media */
    o_allMediaDevices = [[NSMutableArray alloc] init];
    o_specialMediaFolders = [[NSMutableArray alloc] init];
    o_displayInfos = [[NSMutableArray alloc] init];
    NSWorkspace *sharedWorkspace = [NSWorkspace sharedWorkspace];

    [[sharedWorkspace notificationCenter] addObserver:self selector:@selector(scanOpticalMedia:) name:NSWorkspaceDidMountNotification object:nil];
    [[sharedWorkspace notificationCenter] addObserver:self selector:@selector(scanOpticalMedia:) name:NSWorkspaceDidUnmountNotification object:nil];
    [self performSelector:@selector(qtkToggleUIElements:) withObject:nil afterDelay:.3];
    [self performSelector:@selector(scanOpticalMedia:) withObject:nil afterDelay:.5];

    [self setMRL: @""];
}

- (void)setMRL:(NSString *)newMRL
{
    if (!newMRL)
        newMRL = @"";

    if (o_mrl)
        [o_mrl release];

    o_mrl = newMRL;
    [o_mrl retain];
    [o_mrl_fld performSelectorOnMainThread:@selector(setStringValue:) withObject:o_mrl waitUntilDone:NO];
    if ([o_mrl length] > 0)
        [o_btn_ok setEnabled: YES];
    else
        [o_btn_ok setEnabled: NO];
}

- (NSString *)MRL
{
    return o_mrl;
}

- (void)setSubPanel
{
    int i_index;
    module_config_t * p_item;
    intf_thread_t * p_intf = VLCIntf;

    [o_file_sub_ckbox setTitle: _NS("Add Subtitle File:")];
    [o_file_sub_path_lbl setStringValue: _NS("Choose a file")];
    [o_file_sub_path_lbl setHidden: NO];
    [o_file_sub_path_fld setStringValue: @""];
    [o_file_sub_btn_settings setTitle: _NS("Choose...")];
    [[o_file_btn_browse cell] accessibilitySetOverrideValue:_NS("Click to setup subtitle playback in full detail.") forAttribute:NSAccessibilityDescriptionAttribute];
    [o_file_sub_btn_browse setTitle: _NS("Browse...")];
    [[o_file_sub_btn_browse cell] accessibilitySetOverrideValue:_NS("Click to select a subtitle file.") forAttribute:NSAccessibilityDescriptionAttribute];
    [o_file_sub_override setTitle: _NS("Override parameters")];
    [o_file_sub_delay_lbl setStringValue: _NS("Delay")];
    [o_file_sub_delay_stp setEnabled: NO];
    [o_file_sub_fps_lbl setStringValue: _NS("FPS")];
    [o_file_sub_fps_stp setEnabled: NO];
    [o_file_sub_encoding_lbl setStringValue: _NS("Subtitle encoding")];
    [o_file_sub_encoding_pop removeAllItems];
    [o_file_sub_size_lbl setStringValue: _NS("Font size")];
    [o_file_sub_size_pop removeAllItems];
    [o_file_sub_align_lbl setStringValue: _NS("Subtitle alignment")];
    [o_file_sub_align_pop removeAllItems];
    [o_file_sub_ok_btn setStringValue: _NS("OK")];
    [[o_file_sub_ok_btn cell] accessibilitySetOverrideValue:_NS("Click to dismiss the subtitle setup dialog.") forAttribute:NSAccessibilityDescriptionAttribute];
    [o_file_sub_font_box setTitle: _NS("Font Properties")];
    [o_file_sub_file_box setTitle: _NS("Subtitle File")];

    p_item = config_FindConfig(VLC_OBJECT(p_intf), "subsdec-encoding");

    if (p_item) {
        for (int i = 0; i < p_item->list_count; i++) {
            [o_file_sub_encoding_pop addItemWithTitle: _NS(p_item->list_text[i])];
            [[o_file_sub_encoding_pop lastItem] setRepresentedObject:[NSString stringWithFormat:@"%s", p_item->list.psz[i]]];
            if (p_item->value.psz && !strcmp(p_item->value.psz, p_item->list.psz[i]))
                [o_file_sub_encoding_pop selectItem: [o_file_sub_encoding_pop lastItem]];
        }

        if ([o_file_sub_encoding_pop indexOfSelectedItem] < 0)
            [o_file_sub_encoding_pop selectItemAtIndex:0];
    }

    p_item = config_FindConfig(VLC_OBJECT(p_intf), "subsdec-align");

    if (p_item) {
        for (i_index = 0; i_index < p_item->list_count; i_index++)
            [o_file_sub_align_pop addItemWithTitle: _NS(p_item->list_text[i_index])];

        [o_file_sub_align_pop selectItemAtIndex: p_item->value.i];
    }

    p_item = config_FindConfig(VLC_OBJECT(p_intf), "freetype-rel-fontsize");

    if (p_item) {
        for (i_index = 0; i_index < p_item->list_count; i_index++) {
            [o_file_sub_size_pop addItemWithTitle: _NS(p_item->list_text[i_index])];

            if (p_item->value.i == p_item->list.i[i_index])
                [o_file_sub_size_pop selectItemAtIndex: i_index];
        }
    }
}

- (void)openTarget:(int)i_type
{
    /* check whether we already run a modal dialog */
    if ([NSApp modalWindow] != nil)
        return;

    int i_result;

    b_autoplay = config_GetInt(VLCIntf, "macosx-autoplay");

    [o_tabview selectTabViewItemAtIndex: i_type];
    [o_file_sub_ckbox setState: NSOffState];

    i_result = [NSApp runModalForWindow: o_panel];
    [o_panel close];

    if (i_result) {
        NSMutableDictionary *o_dic;
        NSMutableArray *o_options = [NSMutableArray array];

        o_dic = [NSMutableDictionary dictionaryWithObject: [self MRL] forKey: @"ITEM_URL"];
        if ([o_file_sub_ckbox state] == NSOnState) {
            module_config_t * p_item;

            [o_options addObject: [NSString stringWithFormat: @"sub-file=%@", o_sub_path]];
            if ([o_file_sub_override state] == NSOnState) {
                [o_options addObject: [NSString stringWithFormat: @"sub-delay=%f", ([self fileSubDelay] * 10)]];
                [o_options addObject: [NSString stringWithFormat: @"sub-fps=%f", [self fileSubFps]]];
            }
            [o_options addObject: [NSString stringWithFormat:
                    @"subsdec-encoding=%@", [[o_file_sub_encoding_pop selectedItem] representedObject]]];
            [o_options addObject: [NSString stringWithFormat:
                    @"subsdec-align=%li", [o_file_sub_align_pop indexOfSelectedItem]]];

            p_item = config_FindConfig(VLC_OBJECT(VLCIntf),
                                            "freetype-rel-fontsize");

            if (p_item) {
                [o_options addObject: [NSString stringWithFormat:
                    @"freetype-rel-fontsize=%i",
                    p_item->list.i[[o_file_sub_size_pop indexOfSelectedItem]]]];
            }
        }
        if ([o_file_custom_timing_ckb state] == NSOnState) {
            NSArray * components = [[o_file_starttime_fld stringValue] componentsSeparatedByString:@":"];
            NSUInteger componentCount = [components count];
            NSInteger tempValue;
            if (componentCount == 1)
                tempValue = [[components objectAtIndex:0] intValue];
            else if (componentCount == 2)
                tempValue = [[components objectAtIndex:0] intValue] * 60 + [[components objectAtIndex:1] intValue];
            else if (componentCount == 3)
                tempValue = [[components objectAtIndex:0] intValue] * 3600 + [[components objectAtIndex:1] intValue] * 60 + [[components objectAtIndex:2] intValue];
            if (tempValue > 0)
                [o_options addObject: [NSString stringWithFormat:@"start-time=%li", tempValue]];
            components = [[o_file_stoptime_fld stringValue] componentsSeparatedByString:@":"];
            componentCount = [components count];
            if (componentCount == 1)
                tempValue = [[components objectAtIndex:0] intValue];
            else if (componentCount == 2)
                tempValue = [[components objectAtIndex:0] intValue] * 60 + [[components objectAtIndex:1] intValue];
            else if (componentCount == 3)
                tempValue = [[components objectAtIndex:0] intValue] * 3600 + [[components objectAtIndex:1] intValue] * 60 + [[components objectAtIndex:2] intValue];
            if (tempValue > 0)
                [o_options addObject: [NSString stringWithFormat:@"stop-time=%li", tempValue]];
        }
        if ([o_output_ckbox state] == NSOnState) {
            NSArray * soutMRL = [o_sout_options soutMRL];
            NSUInteger count = [soutMRL count];
            for (NSUInteger i = 0 ; i < count ; i++)
                [o_options addObject: [NSString stringWithString: [soutMRL objectAtIndex:i]]];
        }
        if ([o_file_slave_ckbox state] && o_file_slave_path)
           [o_options addObject: [NSString stringWithFormat: @"input-slave=%@", o_file_slave_path]];
        if ([[[o_tabview selectedTabViewItem] label] isEqualToString: _NS("Capture")]) {
            if ([[[o_capture_mode_pop selectedItem] title] isEqualToString: _NS("Screen")]) {
                int selected_index = [o_screen_screen_pop indexOfSelectedItem];
                NSValue *v = [o_displayInfos objectAtIndex:selected_index];
                struct display_info_t *item = (struct display_info_t *)[v pointerValue];

                [o_options addObject: [NSString stringWithFormat: @"screen-fps=%f", [o_screen_fps_fld floatValue]]];
                [o_options addObject: [NSString stringWithFormat: @"screen-display-id=%i", item->id]];
                [o_options addObject: [NSString stringWithFormat: @"screen-left=%i", [o_screen_left_fld intValue]]];
                [o_options addObject: [NSString stringWithFormat: @"screen-top=%i", [o_screen_top_fld intValue]]];
                [o_options addObject: [NSString stringWithFormat: @"screen-width=%i", [o_screen_width_fld intValue]]];
                [o_options addObject: [NSString stringWithFormat: @"screen-height=%i", [o_screen_height_fld intValue]]];
                if ([o_screen_follow_mouse_ckb intValue] == YES)
                    [o_options addObject: @"screen-follow-mouse"];
                else
                    [o_options addObject: @"no-screen-follow-mouse"];
                if ([o_screen_qtk_audio_ckb state] && qtkaudio_currdevice_uid)
                   [o_options addObject: [NSString stringWithFormat: @"input-slave=qtsound://%@", qtkaudio_currdevice_uid]];
            }
            else if ([[[o_capture_mode_pop selectedItem] title] isEqualToString: _NS("Input Devices")]) {
                if ([o_qtk_video_ckb state]) {
                    [o_options addObject: [NSString stringWithFormat: @"qtcapture-width=%i", [o_capture_width_fld intValue]]];
                    [o_options addObject: [NSString stringWithFormat: @"qtcapture-height=%i", [o_capture_height_fld intValue]]];
                    if ([o_qtk_audio_ckb state] && qtkaudio_currdevice_uid)
                       [o_options addObject: [NSString stringWithFormat: @"input-slave=qtsound://%@", qtkaudio_currdevice_uid]];
                }
            }
        }

        /* apply the options to our item(s) */
        [o_dic setObject: (NSArray *)[o_options copy] forKey: @"ITEM_OPTIONS"];
        if (b_autoplay)
            [[[VLCMain sharedInstance] playlist] appendArray: [NSArray arrayWithObject:o_dic] atPos: -1 enqueue:NO];
        else
            [[[VLCMain sharedInstance] playlist] appendArray: [NSArray arrayWithObject:o_dic] atPos: -1 enqueue:YES];
    }
}

- (IBAction)screenChanged:(id)sender
{
    int selected_index = [o_screen_screen_pop indexOfSelectedItem];
    if (selected_index >= [o_displayInfos count]) return;

    NSValue *v = [o_displayInfos objectAtIndex:selected_index];
    struct display_info_t *item = (struct display_info_t *)[v pointerValue];

    [o_screen_left_stp setMaxValue: item->rect.size.width];
    [o_screen_top_stp setMaxValue: item->rect.size.height];
    [o_screen_width_stp setMaxValue: item->rect.size.width];
    [o_screen_height_stp setMaxValue: item->rect.size.height];

    [o_screen_qtk_audio_pop setEnabled: [o_screen_qtk_audio_ckb state]];
}

- (IBAction)qtkChanged:(id)sender
{
    NSInteger i_selectedDevice = [o_qtk_video_device_pop indexOfSelectedItem];
    if ([qtkvideoDevices count] >= 1) {
        NSValue *sizes = [[[[qtkvideoDevices objectAtIndex:i_selectedDevice] formatDescriptions] objectAtIndex:0] attributeForKey: QTFormatDescriptionVideoEncodedPixelsSizeAttribute];

        [o_capture_width_fld setIntValue: [sizes sizeValue].width];
        [o_capture_height_fld setIntValue: [sizes sizeValue].height];
        [o_capture_width_stp setIntValue: [o_capture_width_fld intValue]];
        [o_capture_height_stp setIntValue: [o_capture_height_fld intValue]];
        qtk_currdevice_uid = [[(QTCaptureDevice *)[qtkvideoDevices objectAtIndex:i_selectedDevice] uniqueID] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    }
}

- (IBAction)qtkAudioChanged:(id)sender
{
    NSInteger i_selectedDevice = [o_qtk_audio_device_pop indexOfSelectedItem];
    if ([qtkaudioDevices count] >= 1) {
        qtkaudio_currdevice_uid = [[(QTCaptureDevice *)[qtkaudioDevices objectAtIndex:i_selectedDevice] uniqueID] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    }
    [o_screen_qtk_audio_pop selectItemAtIndex: i_selectedDevice];
    [o_qtk_audio_device_pop selectItemAtIndex: i_selectedDevice];
}

- (IBAction)qtkToggleUIElements:(id)sender
{
    [o_qtk_audio_device_pop setEnabled:[o_qtk_audio_ckb state]];
    BOOL b_state = [o_qtk_video_ckb state];
    [o_qtk_video_device_pop setEnabled:b_state];
    [o_capture_width_fld setEnabled:b_state];
    [o_capture_width_stp setEnabled:b_state];
    [o_capture_height_fld setEnabled:b_state];
    [o_capture_height_stp setEnabled:b_state];
    [self qtkAudioChanged:sender];
    [self qtkChanged:sender];
    [self openCaptureModeChanged:sender];
}

#pragma mark -
#pragma mark Main Actions

- (void)tabView:(NSTabView *)o_tv didSelectTabViewItem:(NSTabViewItem *)o_tvi
{
    NSString *o_label = [o_tvi label];

    if ([o_label isEqualToString: _NS("File")])
        [self openFilePathChanged: nil];
    else if ([o_label isEqualToString: _NS("Disc")])
        [self scanOpticalMedia: nil];
    else if ([o_label isEqualToString: _NS("Network")])
        [self openNetInfoChanged: nil];
    else if ([o_label isEqualToString: _NS("Capture")])
        [self openCaptureModeChanged: nil];
}

- (IBAction)expandMRLfieldAction:(id)sender
{
    NSRect o_win_rect, o_view_rect;
    o_win_rect = [o_panel frame];
    o_view_rect = [o_mrl_view frame];

    if ([o_mrl_btn state] == NSOffState) {
        /* we need to collaps, restore the panel size */
        o_win_rect.size.height = o_win_rect.size.height - o_view_rect.size.height;
        o_win_rect.origin.y = (o_win_rect.origin.y + o_view_rect.size.height) - o_view_rect.size.height;

        /* remove the MRL view */
        [o_mrl_view removeFromSuperview];
    } else {
        /* we need to expand */
        [o_mrl_view setFrame: NSMakeRect(0,
                                         [o_mrl_btn frame].origin.y,
                                         o_view_rect.size.width,
                                         o_view_rect.size.height)];
        [o_mrl_view setNeedsDisplay: NO];
        [o_mrl_view setAutoresizesSubviews: YES];

        /* enlarge panel size for MRL view */
        o_win_rect.size.height = o_win_rect.size.height + o_view_rect.size.height;
    }

    [[o_panel animator] setFrame: o_win_rect display:YES];

    if ([o_mrl_btn state] == NSOnState)
        [[o_panel contentView] addSubview: o_mrl_view];
}

- (void)openFileGeneric
{
    [self openFilePathChanged: nil];
    [self openTarget: 0];
}

- (void)openDisc
{
    @synchronized (self) {
        [o_specialMediaFolders removeAllObjects];
    }

    [self scanOpticalMedia: nil];
    [self openTarget: 1];
}

- (void)openNet
{
    [self openNetInfoChanged: nil];
    [self openTarget: 2];
}

- (void)openCapture
{
    [self openCaptureModeChanged: nil];
    [self openTarget: 3];
}

- (void)openFile
{
    NSOpenPanel *o_open_panel = [NSOpenPanel openPanel];
    b_autoplay = config_GetInt(VLCIntf, "macosx-autoplay");

    [o_open_panel setAllowsMultipleSelection: YES];
    [o_open_panel setCanChooseDirectories: YES];
    [o_open_panel setTitle: _NS("Open File")];
    [o_open_panel setPrompt: _NS("Open")];

    if ([o_open_panel runModal] == NSOKButton) {
        NSArray * o_urls = [o_open_panel URLs];
        NSUInteger count = [o_urls count];
        NSMutableArray *o_values = [NSMutableArray arrayWithCapacity:count];
        NSMutableArray *o_array = [NSMutableArray arrayWithCapacity:count];
        for (NSUInteger i = 0; i < count; i++)
            [o_values addObject: [[o_urls objectAtIndex:i] path]];
        [o_values sortUsingSelector:@selector(caseInsensitiveCompare:)];

        for (NSUInteger i = 0; i < count; i++) {
            NSDictionary *o_dic;
            char *psz_uri = vlc_path2uri([[o_values objectAtIndex:i] UTF8String], "file");
            if (!psz_uri)
                continue;

            o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];

            free(psz_uri);

            [o_array addObject: o_dic];
        }
        if (b_autoplay)
            [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:NO];
        else
            [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:YES];
    }
}

#pragma mark -
#pragma mark File Panel

- (void)openFilePathChanged:(NSNotification *)o_notification
{
    if (o_file_path && [o_file_path length] > 0) {
        bool b_stream = [o_file_stream state];
        BOOL b_dir = NO;

        [[NSFileManager defaultManager] fileExistsAtPath:o_file_path isDirectory:&b_dir];

        char *psz_uri = vlc_path2uri([o_file_path UTF8String], "file");
        if (!psz_uri) return;

        NSMutableString *o_mrl_string = [NSMutableString stringWithUTF8String: psz_uri ];
        NSRange offile = [o_mrl_string rangeOfString:@"file"];
        free(psz_uri);

        if (b_dir)
            [o_mrl_string replaceCharactersInRange:offile withString: @"directory"];
        else if (b_stream)
            [o_mrl_string replaceCharactersInRange:offile withString: @"stream"];

        [o_file_name setStringValue: [[NSFileManager defaultManager] displayNameAtPath:o_file_path]];
        [o_file_name_stub setHidden: YES];
        [o_file_stream setHidden: NO];
        [o_file_icon_well setImage: [[NSWorkspace sharedWorkspace] iconForFile: o_file_path]];
        [o_file_icon_well setHidden: NO];
        [self setMRL: o_mrl_string];
    } else {
        [o_file_name setStringValue: @""];
        [o_file_name_stub setHidden: NO];
        [o_file_stream setHidden: YES];
        [o_file_icon_well setImage: [NSImage imageNamed:@"generic"]];
        [self setMRL: @""];
    }
}

- (IBAction)openFileBrowse:(id)sender
{
    NSOpenPanel *o_open_panel = [NSOpenPanel openPanel];

    [o_open_panel setAllowsMultipleSelection: NO];
    [o_open_panel setCanChooseDirectories: YES];
    [o_open_panel setTitle: _NS("Open File")];
    [o_open_panel setPrompt: _NS("Open")];
    [o_open_panel beginSheetModalForWindow:[sender window] completionHandler:^(NSInteger returnCode) {
        if (returnCode == NSFileHandlingPanelOKButton) {
            if (o_file_path)
                [o_file_path release];
            o_file_path = [[[o_open_panel URLs] objectAtIndex:0] path];
            [o_file_path retain];
            [self openFilePathChanged: nil];
        }
    }];
}

- (IBAction)openFileStreamChanged:(id)sender
{
    [self openFilePathChanged: nil];
}

- (IBAction)inputSlaveAction:(id)sender
{
    if (sender == o_file_slave_ckbox)
        [o_file_slave_select_btn setEnabled: [o_file_slave_ckbox state]];
    else {
        NSOpenPanel *o_open_panel;
        o_open_panel = [NSOpenPanel openPanel];
        [o_open_panel setCanChooseFiles: YES];
        [o_open_panel setCanChooseDirectories: NO];
        if ([o_open_panel runModal] == NSOKButton) {
            if (o_file_slave_path)
                [o_file_slave_path release];
            o_file_slave_path = [[[o_open_panel URLs] objectAtIndex:0] path];
            [o_file_slave_path retain];
        }
    }
    if (o_file_slave_path && [o_file_slave_ckbox state] == NSOnState) {
        [o_file_slave_filename_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath:o_file_slave_path]];
        [o_file_slave_icon_well setImage: [[NSWorkspace sharedWorkspace] iconForFile: o_file_slave_path]];
    } else {
        [o_file_slave_filename_lbl setStringValue: @""];
        [o_file_slave_icon_well setImage: NULL];
    }
}

- (IBAction)fileTimeCustomization:(id)sender
{
    BOOL b_value = [o_file_custom_timing_ckb state];
    [o_file_starttime_fld setEnabled: b_value];
    [o_file_starttime_lbl setEnabled: b_value];
    [o_file_stoptime_fld setEnabled: b_value];
    [o_file_stoptime_lbl setEnabled: b_value];
}

#pragma mark -
#pragma mark Optical Media Panel

- (void)showOpticalMediaView: theView withIcon:(NSImage *)icon
{
    NSRect o_view_rect;
    o_view_rect = [theView frame];
    [theView setFrame: NSMakeRect(233, 0, o_view_rect.size.width, o_view_rect.size.height)];
    [theView setAutoresizesSubviews: YES];
    if (o_currentOpticalMediaView) {
        [[[[o_tabview tabViewItemAtIndex: [o_tabview indexOfTabViewItemWithIdentifier:@"optical"]] view] animator] replaceSubview: o_currentOpticalMediaView with: theView];
        [o_currentOpticalMediaView release];
    }
    else
        [[[[o_tabview tabViewItemAtIndex: [o_tabview indexOfTabViewItemWithIdentifier:@"optical"]] view] animator] addSubview: theView];
    o_currentOpticalMediaView = theView;
    [o_currentOpticalMediaView retain];

    NSImageView *imageView;
    imageView = [[NSImageView alloc] init];
    [imageView setFrame: NSMakeRect(53, 61, 128, 128)];
    [icon setSize: NSMakeSize(128,128)];
    [imageView setImage: icon];
    if (o_currentOpticalMediaIconView) {
        [[[[o_tabview tabViewItemAtIndex: [o_tabview indexOfTabViewItemWithIdentifier:@"optical"]] view] animator] replaceSubview: o_currentOpticalMediaIconView with: imageView];
        [o_currentOpticalMediaIconView release];
    }
    else
         [[[[o_tabview tabViewItemAtIndex: [o_tabview indexOfTabViewItemWithIdentifier:@"optical"]] view] animator] addSubview: imageView];
    o_currentOpticalMediaIconView = imageView;
    [o_currentOpticalMediaIconView retain];
    [o_currentOpticalMediaView setNeedsDisplay: YES];
    [o_currentOpticalMediaIconView setNeedsDisplay: YES];
    [[[o_tabview tabViewItemAtIndex: [o_tabview indexOfTabViewItemWithIdentifier:@"optical"]] view] setNeedsDisplay: YES];
    [[[o_tabview tabViewItemAtIndex: [o_tabview indexOfTabViewItemWithIdentifier:@"optical"]] view] displayIfNeeded];
}

- (NSString *) getBSDNodeFromMountPath:(NSString *)mountPath
{
    OSStatus err;
    FSRef ref;
    FSVolumeRefNum actualVolume;
    err = FSPathMakeRef ((const UInt8 *) [mountPath fileSystemRepresentation], &ref, NULL);

    // get a FSVolumeRefNum from mountPath
    if (noErr == err) {
        FSCatalogInfo   catalogInfo;
        err = FSGetCatalogInfo (&ref,
                                kFSCatInfoVolume,
                                &catalogInfo,
                                NULL,
                                NULL,
                                NULL
                               );
        if (noErr == err)
            actualVolume = catalogInfo.volume;
        else
            return @"";
    }
    else
        return @"";

    GetVolParmsInfoBuffer volumeParms;
    err = FSGetVolumeParms(actualVolume, &volumeParms, sizeof(volumeParms));
    if (noErr == err) {
        NSString *bsdName = [NSString stringWithUTF8String:(char *)volumeParms.vMDeviceID];
        return [NSString stringWithFormat:@"/dev/r%@", bsdName];
    }

    return @"";
}

- (NSString *)getVolumeTypeFromMountPath:(NSString *)mountPath
{
    OSStatus err;
    FSRef ref;
    FSVolumeRefNum actualVolume;
    err = FSPathMakeRef ((const UInt8 *) [mountPath fileSystemRepresentation], &ref, NULL);

    // get a FSVolumeRefNum from mountPath
    if (noErr == err) {
        FSCatalogInfo   catalogInfo;
        err = FSGetCatalogInfo (&ref,
                                kFSCatInfoVolume,
                                &catalogInfo,
                                NULL,
                                NULL,
                                NULL
                               );
        if (noErr == err)
            actualVolume = catalogInfo.volume;
        else
            return NULL;
    }
    else
        return NULL;

    GetVolParmsInfoBuffer volumeParms;
    err = FSGetVolumeParms(actualVolume, &volumeParms, sizeof(volumeParms));

    CFMutableDictionaryRef matchingDict;
    io_service_t service;

    if (!volumeParms.vMDeviceID)
        return NULL;

    matchingDict = IOBSDNameMatching(kIOMasterPortDefault, 0, volumeParms.vMDeviceID);
    service = IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);

    NSString *returnValue;
    if (IO_OBJECT_NULL != service) {
        if (IOObjectConformsTo(service, kIOCDMediaClass)) {
            returnValue = kVLCMediaAudioCD;
        }
        else if (IOObjectConformsTo(service, kIODVDMediaClass))
            returnValue = kVLCMediaDVD;
        else if (IOObjectConformsTo(service, kIOBDMediaClass))
            returnValue = kVLCMediaBD;
        else {
            if ([mountPath rangeOfString:@"VIDEO_TS" options:NSCaseInsensitiveSearch | NSBackwardsSearch].location != NSNotFound)
                returnValue = kVLCMediaVideoTSFolder;
            else if ([mountPath rangeOfString:@"BDMV" options:NSCaseInsensitiveSearch | NSBackwardsSearch].location != NSNotFound)
                returnValue = kVLCMediaBDMVFolder;
            else {
                // NSFileManager is not thread-safe, don't use defaultManager outside of the main thread
                NSFileManager * fm = [[NSFileManager alloc] init];

                NSArray *dirContents = [fm contentsOfDirectoryAtPath:mountPath error:nil];
                for (int i = 0; i < [dirContents count]; i++) {
                    NSString *currentFile = [dirContents objectAtIndex:i];
                    NSString *fullPath = [mountPath stringByAppendingPathComponent:currentFile];

                    BOOL isDir;
                    if ([fm fileExistsAtPath:fullPath isDirectory:&isDir] && isDir)
                    {
                        if ([currentFile caseInsensitiveCompare:@"SVCD"] == NSOrderedSame) {
                            returnValue = kVLCMediaSVCD;
                            break;
                        }
                        if ([currentFile caseInsensitiveCompare:@"VCD"] == NSOrderedSame) {
                            returnValue = kVLCMediaVCD;
                            break;
                        }
                        if ([currentFile caseInsensitiveCompare:@"BDMV"] == NSOrderedSame) {
                            returnValue = kVLCMediaBDMVFolder;
                            break;
                        }
                        if ([currentFile caseInsensitiveCompare:@"VIDEO_TS"] == NSOrderedSame) {
                            returnValue = kVLCMediaVideoTSFolder;
                            break;
                        }
                    }
                }

                [fm release];

                if (!returnValue)
                    returnValue = kVLCMediaVideoTSFolder;
            }
        }

        IOObjectRelease(service);
    }
    return returnValue;
}

- (void)showOpticalAtPath: (NSDictionary *)o_dict
{
    NSString *diskType = [o_dict objectForKey:@"mediaType"];
    NSString *o_opticalDevicePath = [o_dict objectForKey:@"path"];
    NSString *o_device_path = [o_dict objectForKey:@"devicePath"];
    NSImage *o_image = [o_dict objectForKey:@"image"];

    if ([diskType isEqualToString: kVLCMediaDVD] || [diskType isEqualToString: kVLCMediaVideoTSFolder]) {
        [o_disc_dvd_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath:o_opticalDevicePath]];
        [o_disc_dvdwomenus_lbl setStringValue: [o_disc_dvd_lbl stringValue]];

        if (!b_nodvdmenus) {
            [self setMRL: [NSString stringWithFormat: @"dvdnav://%@", o_device_path]];
            [self showOpticalMediaView: o_disc_dvd_view withIcon:o_image];
        } else {
            [self setMRL: [NSString stringWithFormat: @"dvdread://%@#%i:%i-", o_device_path, [o_disc_dvdwomenus_title intValue], [o_disc_dvdwomenus_chapter intValue]]];
            [self showOpticalMediaView: o_disc_dvdwomenus_view withIcon: o_image];
        }
    } else if ([diskType isEqualToString: kVLCMediaAudioCD]) {
        [o_disc_audiocd_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath: o_opticalDevicePath]];
        [o_disc_audiocd_trackcount_lbl setStringValue: [NSString stringWithFormat:_NS("%i tracks"), [[[NSFileManager defaultManager] subpathsOfDirectoryAtPath: o_opticalDevicePath error:NULL] count] - 1]]; // minus .TOC.plist
        [self showOpticalMediaView: o_disc_audiocd_view withIcon: o_image];
        [self setMRL: [NSString stringWithFormat: @"cdda://%@", o_device_path]];
    } else if ([diskType isEqualToString: kVLCMediaVCD]) {
        [o_disc_vcd_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath: o_opticalDevicePath]];
        [self showOpticalMediaView: o_disc_vcd_view withIcon: o_image];
        [self setMRL: [NSString stringWithFormat: @"vcd://%@#%i:%i", o_device_path, [o_disc_vcd_title intValue], [o_disc_vcd_chapter intValue]]];
    } else if ([diskType isEqualToString: kVLCMediaSVCD]) {
        [o_disc_vcd_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath: o_opticalDevicePath]];
        [self showOpticalMediaView: o_disc_vcd_view withIcon: o_image];
        [self setMRL: [NSString stringWithFormat: @"vcd://%@@%i:%i", o_device_path, [o_disc_vcd_title intValue], [o_disc_vcd_chapter intValue]]];
    } else if ([diskType isEqualToString: kVLCMediaBD] || [diskType isEqualToString: kVLCMediaBDMVFolder]) {
        [o_disc_bd_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath: o_opticalDevicePath]];
        [self showOpticalMediaView: o_disc_bd_view withIcon: o_image];
        [self setMRL: [NSString stringWithFormat: @"bluray://%@", o_opticalDevicePath]];
    } else {
        if (VLCIntf)
            msg_Warn(VLCIntf, "unknown disk type, no idea what to display");

        [self showOpticalMediaView: o_disc_nodisc_view withIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
    }
}

- (NSDictionary *)scanPath:(NSString *)o_path
{
    NSString *o_type = [self getVolumeTypeFromMountPath:o_path];
    NSImage *o_image = [[NSWorkspace sharedWorkspace] iconForFile: o_path];
    NSString *o_device_path;

    if ([o_type isEqualToString: kVLCMediaVideoTSFolder] ||
        [o_type isEqualToString: kVLCMediaBD] ||
        [o_type isEqualToString: kVLCMediaBDMVFolder] ||
        [o_type isEqualToString: kVLCMediaUnknown])
        o_device_path = o_path;
    else
        o_device_path = [self getBSDNodeFromMountPath:o_path];

    return [NSDictionary dictionaryWithObjectsAndKeys: o_path, @"path",
                                                o_device_path, @"devicePath",
                                                       o_type, @"mediaType",
                                                      o_image, @"image", nil];
}

- (void)scanDevicesWithPaths:(NSArray *)o_paths
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    NSUInteger count = [o_paths count];
    NSMutableArray *o_result = [NSMutableArray arrayWithCapacity:count];
    for (NSUInteger i = 0; i < count; i++)
        [o_result addObject: [self scanPath:[o_paths objectAtIndex:i]]];

    @synchronized (self) {
        if (o_opticalDevices)
            [o_opticalDevices release];
        o_opticalDevices = [[NSArray alloc] initWithArray: o_result];
    }

    [self performSelectorOnMainThread:@selector(updateMediaSelector:) withObject:nil waitUntilDone:NO];
    [o_pool release];
}

- (void)scanSpecialPath:(NSString *)o_path
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    NSDictionary *o_dict = [self scanPath:o_path];

    @synchronized (self) {
        [o_specialMediaFolders addObject:o_dict];
    }

    [self performSelectorOnMainThread:@selector(updateMediaSelector:) withObject:[NSNumber numberWithBool:YES] waitUntilDone:NO];
    [o_pool release];
}

- (void)scanOpticalMedia:(NSNotification *)o_notification
{
    [NSThread detachNewThreadSelector:@selector(scanDevicesWithPaths:) toTarget:self withObject:[NSArray arrayWithArray:[[NSWorkspace sharedWorkspace] mountedRemovableMedia]]];
}

- (void)updateMediaSelector:(NSNumber *)o_selection
{
    [o_allMediaDevices removeAllObjects];
    [o_disc_selector_pop removeAllItems];

    @synchronized (self) {
        [o_allMediaDevices addObjectsFromArray:o_opticalDevices];
        [o_allMediaDevices addObjectsFromArray:o_specialMediaFolders];
    }

    NSUInteger count = [o_allMediaDevices count];
    if (count > 0) {
        for (NSUInteger i = 0; i < count ; i++) {
            NSDictionary *o_dict = [o_allMediaDevices objectAtIndex:i];
            [o_disc_selector_pop addItemWithTitle: [[NSFileManager defaultManager] displayNameAtPath:[o_dict objectForKey:@"path"]]];
        }

        if ([o_disc_selector_pop numberOfItems] <= 1)
            [o_disc_selector_pop setHidden: YES];
        else
            [o_disc_selector_pop setHidden: NO];

        // select newly added media folder
        if (o_selection && [o_selection boolValue])
            [o_disc_selector_pop selectItemAtIndex: [[o_disc_selector_pop itemArray] count] - 1];

        [self discSelectorChanged:nil];
    } else {
        msg_Dbg(VLCIntf, "no optical media found");
        [o_disc_selector_pop setHidden: YES];
        [self showOpticalMediaView: o_disc_nodisc_view withIcon: [NSImage imageNamed: @"NSApplicationIcon"]];
    }

}

- (IBAction)discSelectorChanged:(id)sender
{
    NSDictionary *o_dict = [o_allMediaDevices objectAtIndex:[o_disc_selector_pop indexOfSelectedItem]];    
    [self showOpticalAtPath:o_dict];
}

- (IBAction)openSpecialMediaFolder:(id)sender
{
    /* this is currently for VIDEO_TS and BDMV folders */
    NSOpenPanel *o_open_panel = [NSOpenPanel openPanel];

    [o_open_panel setAllowsMultipleSelection: NO];
    [o_open_panel setCanChooseDirectories: YES];
    [o_open_panel setTitle: [sender title]];
    [o_open_panel setPrompt: _NS("Open")];

    /* work-around for Mountain Lion, which treats folders called "BDMV" including an item named "INDEX.BDM"
     * as a _FILE_. Don't ask, move on. There is nothing to see here */
    [o_open_panel setCanChooseFiles: YES];
    [o_open_panel setAllowedFileTypes:[NSArray arrayWithObject:@"public.directory"]];

    if ([o_open_panel runModal] == NSOKButton) {
        NSString *o_path = [[[o_open_panel URLs] objectAtIndex:0] path];
        if ([o_path length] > 0) {
            [NSThread detachNewThreadSelector:@selector(scanSpecialPath:) toTarget:self withObject:o_path];
        }
    }
}

- (IBAction)dvdreadOptionChanged:(id)sender
{
    NSDictionary *o_dict = [o_allMediaDevices objectAtIndex:[o_disc_selector_pop indexOfSelectedItem]];
    NSString *o_device_path = [o_dict objectForKey:@"devicePath"];

    if (sender == o_disc_dvdwomenus_enablemenus_btn) {
        b_nodvdmenus = NO;
        [self setMRL: [NSString stringWithFormat: @"dvdnav://%@", o_device_path]];
        [self showOpticalMediaView: o_disc_dvd_view withIcon: [o_currentOpticalMediaIconView image]];
        return;
    }
    if (sender == o_disc_dvd_disablemenus_btn) {
        b_nodvdmenus = YES;
        [self showOpticalMediaView: o_disc_dvdwomenus_view withIcon: [o_currentOpticalMediaIconView image]];
    }

    if (sender == o_disc_dvdwomenus_title)
        [o_disc_dvdwomenus_title_stp setIntValue: [o_disc_dvdwomenus_title intValue]];
    if (sender == o_disc_dvdwomenus_title_stp)
        [o_disc_dvdwomenus_title setIntValue: [o_disc_dvdwomenus_title_stp intValue]];
    if (sender == o_disc_dvdwomenus_chapter)
        [o_disc_dvdwomenus_chapter_stp setIntValue: [o_disc_dvdwomenus_chapter intValue]];
    if (sender == o_disc_dvdwomenus_chapter_stp)
        [o_disc_dvdwomenus_chapter setIntValue: [o_disc_dvdwomenus_chapter_stp intValue]];

    [self setMRL: [NSString stringWithFormat: @"dvdread://%@#%i:%i-", o_device_path, [o_disc_dvdwomenus_title intValue], [o_disc_dvdwomenus_chapter intValue]]];
}

- (IBAction)vcdOptionChanged:(id)sender
{
    if (sender == o_disc_vcd_title)
        [o_disc_vcd_title_stp setIntValue: [o_disc_vcd_title intValue]];
    if (sender == o_disc_vcd_title_stp)
        [o_disc_vcd_title setIntValue: [o_disc_vcd_title_stp intValue]];
    if (sender == o_disc_vcd_chapter)
        [o_disc_vcd_chapter_stp setIntValue: [o_disc_vcd_chapter intValue]];
    if (sender == o_disc_vcd_chapter_stp)
        [o_disc_vcd_chapter setIntValue: [o_disc_vcd_chapter_stp intValue]];

    NSString *o_device_path = [[o_allMediaDevices objectAtIndex:[o_disc_selector_pop indexOfSelectedItem]] objectForKey:@"devicePath"];
    [self setMRL: [NSString stringWithFormat: @"vcd://%@@%i:%i", o_device_path, [o_disc_vcd_title intValue], [o_disc_vcd_chapter intValue]]];
}

#pragma mark -
#pragma mark Network Panel

- (void)textFieldWasClicked:(NSNotification *)o_notification
{
    if ([o_notification object] == o_net_udp_port)
        [o_net_mode selectCellAtRow: 0 column: 0];
    else if ([o_notification object] == o_net_udpm_addr ||
             [o_notification object] == o_net_udpm_port)
        [o_net_mode selectCellAtRow: 1 column: 0];
    else
        [o_net_mode selectCellAtRow: 2 column: 0];

    [self openNetInfoChanged: nil];
}

- (IBAction)openNetModeChanged:(id)sender
{
    if (sender == o_net_mode) {
        if ([[sender selectedCell] tag] == 0)
            [o_panel makeFirstResponder: o_net_udp_port];
        else if ([[sender selectedCell] tag] == 1)
            [o_panel makeFirstResponder: o_net_udpm_addr];
        else
            msg_Warn(VLCIntf, "Unknown sender tried to change UDP/RTP mode");
    }

    [self openNetInfoChanged: nil];
}

- (IBAction)openNetStepperChanged:(id)sender
{
    int i_tag = [sender tag];

    if (i_tag == 0) {
        [o_net_udp_port setIntValue: [o_net_udp_port_stp intValue]];
        [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCOpenTextFieldWasClicked"
                                                            object: o_net_udp_port];
        [o_panel makeFirstResponder: o_net_udp_port];
    }
    else if (i_tag == 1) {
        [o_net_udpm_port setIntValue: [o_net_udpm_port_stp intValue]];
        [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCOpenTextFieldWasClicked"
                                                            object: o_net_udpm_port];
        [o_panel makeFirstResponder: o_net_udpm_port];
    }

    [self openNetInfoChanged: nil];
}

- (void)openNetInfoChanged:(NSNotification *)o_notification
{
    NSString *o_mrl_string = [NSString string];

    if ([o_net_udp_panel isVisible]) {
        NSString *o_mode;
        o_mode = [[o_net_mode selectedCell] title];

        if ([o_mode isEqualToString: _NS("Unicast")]) {
            int i_port = [o_net_udp_port intValue];

            if ([[o_net_udp_protocol_mat selectedCell] tag] == 0)
                o_mrl_string = @"udp://";
            else
                o_mrl_string = @"rtp://";

            if (i_port != config_GetInt(VLCIntf, "server-port")) {
                o_mrl_string =
                    [o_mrl_string stringByAppendingFormat: @"@:%i", i_port];
            }
        }
        else if ([o_mode isEqualToString: _NS("Multicast")]) {
            NSString *o_addr = [o_net_udpm_addr stringValue];
            int i_port = [o_net_udpm_port intValue];

            if ([[o_net_udp_protocol_mat selectedCell] tag] == 0)
                o_mrl_string = [NSString stringWithFormat: @"udp://@%@", o_addr];
            else
                o_mrl_string = [NSString stringWithFormat: @"rtp://@%@", o_addr];

            if (i_port != config_GetInt(VLCIntf, "server-port")) {
                o_mrl_string =
                    [o_mrl_string stringByAppendingFormat: @":%i", i_port];
            }
        }
    } else
        o_mrl_string = [o_net_http_url stringValue];

    [self setMRL: o_mrl_string];
}

- (IBAction)openNetUDPButtonAction:(id)sender
{
    if (sender == o_net_openUDP_btn) {
        [NSApp beginSheet: o_net_udp_panel
           modalForWindow: o_panel
            modalDelegate: self
           didEndSelector: NULL
              contextInfo: nil];
        [self openNetInfoChanged: nil];
    }
    else if (sender == o_net_udp_cancel_btn) {
        [o_net_udp_panel orderOut: sender];
        [NSApp endSheet: o_net_udp_panel];
    }
    else if (sender == o_net_udp_ok_btn) {
        NSString *o_mrl_string = [NSString string];
        if ([[[o_net_mode selectedCell] title] isEqualToString: _NS("Unicast")]) {
            int i_port = [o_net_udp_port intValue];

            if ([[o_net_udp_protocol_mat selectedCell] tag] == 0)
                o_mrl_string = @"udp://";
            else
                o_mrl_string = @"rtp://";

            if (i_port != config_GetInt(VLCIntf, "server-port")) {
                o_mrl_string =
                [o_mrl_string stringByAppendingFormat: @"@:%i", i_port];
            }
        }
        else if ([[[o_net_mode selectedCell] title] isEqualToString: _NS("Multicast")]) {
            NSString *o_addr = [o_net_udpm_addr stringValue];
            int i_port = [o_net_udpm_port intValue];

            if ([[o_net_udp_protocol_mat selectedCell] tag] == 0)
                o_mrl_string = [NSString stringWithFormat: @"udp://@%@", o_addr];
            else
                o_mrl_string = [NSString stringWithFormat: @"rtp://@%@", o_addr];

            if (i_port != config_GetInt(VLCIntf, "server-port")) {
                o_mrl_string =
                [o_mrl_string stringByAppendingFormat: @":%i", i_port];
            }
        }
        [self setMRL: o_mrl_string];
        [o_net_http_url setStringValue: o_mrl_string];
        [o_net_udp_panel orderOut: sender];
        [NSApp endSheet: o_net_udp_panel];
    }
}

#pragma mark -
#pragma mark Capture Panel

- (void)showCaptureView: theView
{
    NSRect o_view_rect;
    o_view_rect = [theView frame];
    [theView setFrame: NSMakeRect(0, -10, o_view_rect.size.width, o_view_rect.size.height)];
    [theView setAutoresizesSubviews: YES];
    if (o_currentCaptureView) {
        [[[[o_tabview tabViewItemAtIndex: 3] view] animator] replaceSubview: o_currentCaptureView with: theView];
        [o_currentCaptureView release];
    } else {
        [[[[o_tabview tabViewItemAtIndex: 3] view] animator] addSubview: theView];
    }
    o_currentCaptureView = theView;
    [o_currentCaptureView retain];
}

- (IBAction)openCaptureModeChanged:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;

    if ([[[o_capture_mode_pop selectedItem] title] isEqualToString: @"EyeTV"]) {
        if ([[[VLCMain sharedInstance] eyeTVController] eyeTVRunning] == YES) {
            if ([[[VLCMain sharedInstance] eyeTVController] deviceConnected] == YES) {
                [self showCaptureView: o_eyetv_running_view];
                [self setupChannelInfo];
            }
            else
                setEyeTVUnconnected;
        }
        else
            [self showCaptureView: o_eyetv_notLaunched_view];
        [self setMRL: @""];
    }
    else if ([[[o_capture_mode_pop selectedItem] title] isEqualToString: _NS("Screen")]) {
        [self showCaptureView: o_screen_view];
        [self setMRL: @"screen://"];
        [o_screen_height_fld setIntValue: config_GetInt(p_intf, "screen-height")];
        [o_screen_width_fld setIntValue: config_GetInt(p_intf, "screen-width")];
        [o_screen_fps_fld setFloatValue: config_GetFloat(p_intf, "screen-fps")];
        [o_screen_left_fld setIntValue: config_GetInt(p_intf, "screen-left")];
        [o_screen_top_fld setIntValue: config_GetInt(p_intf, "screen-top")];
        [o_screen_follow_mouse_ckb setIntValue: config_GetInt(p_intf, "screen-follow-mouse")];

        int screen_index = config_GetInt(p_intf, "screen-index");
        int display_id = config_GetInt(p_intf, "screen-display-id");
        unsigned int i, displayCount = 0;
        CGLError returnedError;
        struct display_info_t *item;
        NSValue *v;

        returnedError = CGGetOnlineDisplayList(0, NULL, &displayCount);
        if (!returnedError) {
            CGDirectDisplayID *ids;
            ids = (CGDirectDisplayID *)malloc(displayCount * sizeof(CGDirectDisplayID));
            returnedError = CGGetOnlineDisplayList(displayCount, ids, &displayCount);
            if (!returnedError) {
                for (i = 0; i < [o_displayInfos count]; i ++) {
                    v = [o_displayInfos objectAtIndex:i];
                    free([v pointerValue]);
                }
                [o_displayInfos removeAllObjects];
                [o_screen_screen_pop removeAllItems];
                for (i = 0; i < displayCount; i ++) {
                    item = (struct display_info_t *)malloc(sizeof(struct display_info_t));
                    item->id = ids[i];
                    item->rect = CGDisplayBounds(item->id);
                    [o_screen_screen_pop addItemWithTitle: [NSString stringWithFormat:@"Screen %d (%dx%d)", i + 1, (int)item->rect.size.width, (int)item->rect.size.height]];
                    v = [NSValue valueWithPointer:item];
                    [o_displayInfos addObject:v];
                    if (i == 0 || display_id == item->id || screen_index - 1 == i) {
                        [o_screen_screen_pop selectItemAtIndex: i];
                        [o_screen_left_stp setMaxValue: item->rect.size.width];
                        [o_screen_top_stp setMaxValue: item->rect.size.height];
                        [o_screen_width_stp setMaxValue: item->rect.size.width];
                        [o_screen_height_stp setMaxValue: item->rect.size.height];
                    }
                }
            }
            free(ids);
        }
    }
    else if ([[[o_capture_mode_pop selectedItem] title] isEqualToString: _NS("Input Devices")]) {
        [self showCaptureView: o_qtk_view];
        if ([o_capture_width_fld intValue] <= 0)
            [self qtkChanged:nil];

        [self qtkAudioChanged:nil];

        [self setMRL: @""];

        if ([o_qtk_video_ckb state] && qtk_currdevice_uid)
            [self setMRL:[NSString stringWithFormat:@"qtcapture://%@", qtk_currdevice_uid]];
        else if ([o_qtk_audio_ckb state] && qtkaudio_currdevice_uid)
            [self setMRL:[NSString stringWithFormat:@"qtsound://%@", qtkaudio_currdevice_uid]];
    }
}

- (void)screenFPSfieldChanged:(NSNotification *)o_notification
{
    [o_screen_fps_stp setFloatValue: [o_screen_fps_fld floatValue]];
    if ([[o_screen_fps_fld stringValue] isEqualToString: @""])
        [o_screen_fps_fld setFloatValue: 1.0];
    [self setMRL: @"screen://"];
}

- (IBAction)eyetvSwitchChannel:(id)sender
{
    if (sender == o_eyetv_nextProgram_btn) {
        int chanNum = [[[VLCMain sharedInstance] eyeTVController] switchChannelUp: YES];
        [o_eyetv_channels_pop selectItemWithTag:chanNum];
        [self setMRL: [NSString stringWithFormat:@"eyetv:// :eyetv-channel=%d", chanNum]];
    } else if (sender == o_eyetv_previousProgram_btn) {
        int chanNum = [[[VLCMain sharedInstance] eyeTVController] switchChannelUp: NO];
        [o_eyetv_channels_pop selectItemWithTag:chanNum];
        [self setMRL: [NSString stringWithFormat:@"eyetv:// :eyetv-channel=%d", chanNum]];
    } else if (sender == o_eyetv_channels_pop) {
        int chanNum = [[sender selectedItem] tag];
        [[[VLCMain sharedInstance] eyeTVController] setChannel:chanNum];
        [self setMRL: [NSString stringWithFormat:@"eyetv:// :eyetv-channel=%d", chanNum]];
    } else
        msg_Err(VLCIntf, "eyetvSwitchChannel sent by unknown object");
}

- (IBAction)eyetvLaunch:(id)sender
{
    [[[VLCMain sharedInstance] eyeTVController] launchEyeTV];
}

- (IBAction)eyetvGetPlugin:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString: @"http://www.videolan.org/vlc/eyetv"]];
}

- (void)eyetvChanged:(NSNotification *)o_notification
{
    if ([[o_notification name] isEqualToString: @"DeviceAdded"]) {
        msg_Dbg(VLCIntf, "eyetv device was added");
        [self showCaptureView: o_eyetv_running_view];
        [self setupChannelInfo];
    } else if ([[o_notification name] isEqualToString: @"DeviceRemoved"]) {
        /* leave the channel selection like that,
         * switch to our "no device" tab */
        msg_Dbg(VLCIntf, "eyetv device was removed");
        setEyeTVUnconnected;
    } else if ([[o_notification name] isEqualToString: @"PluginQuit"]) {
        /* switch to the "launch eyetv" tab */
        msg_Dbg(VLCIntf, "eyetv was terminated");
        [self showCaptureView: o_eyetv_notLaunched_view];
    } else if ([[o_notification name] isEqualToString: @"PluginInit"]) {
        /* we got no device yet */
        msg_Dbg(VLCIntf, "eyetv was launched, no device yet");
        setEyeTVUnconnected;
    }
}

/* little helper method, since this code needs to be run by multiple objects */
- (void)setupChannelInfo
{
    /* set up channel selection */
    [o_eyetv_channels_pop removeAllItems];
    [o_eyetv_chn_bgbar setHidden: NO];
    [o_eyetv_chn_bgbar animate: self];
    [o_eyetv_chn_status_txt setStringValue: _NS("Retrieving Channel Info...")];
    [o_eyetv_chn_status_txt setHidden: NO];

    /* retrieve info */
    NSEnumerator *channels = [[[VLCMain sharedInstance] eyeTVController] allChannels];
    int x = -2;
    [[[o_eyetv_channels_pop menu] addItemWithTitle: _NS("Composite input")
                                               action: nil
                                        keyEquivalent: @""] setTag:x++];
    [[[o_eyetv_channels_pop menu] addItemWithTitle: _NS("S-Video input")
                                               action: nil
                                        keyEquivalent: @""] setTag:x++];
    if (channels) {
        NSString *channel;
        [[o_eyetv_channels_pop menu] addItem: [NSMenuItem separatorItem]];
        while (channel = [channels nextObject])
            /* we have to add items this way, because we accept duplicates
             * additionally, we save a bit of time */
            [[[o_eyetv_channels_pop menu] addItemWithTitle: channel action: nil keyEquivalent: @""] setTag:++x];

        /* make Tuner the default */
        [o_eyetv_channels_pop selectItemWithTag:[[[VLCMain sharedInstance] eyeTVController] channel]];
    }

    /* clean up GUI */
    [o_eyetv_chn_bgbar setHidden: YES];
    [o_eyetv_chn_status_txt setHidden: YES];
}

#pragma mark -
#pragma mark Subtitle Settings

- (IBAction)subsChanged:(id)sender
{
    if ([o_file_sub_ckbox state] == NSOnState) {
        [o_file_sub_btn_settings setEnabled:YES];
        if (o_sub_path) {
            [o_file_subtitles_filename_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath:o_sub_path]];
            [o_file_subtitles_icon_well setImage: [[NSWorkspace sharedWorkspace] iconForFile:o_sub_path]];
        }
    } else {
        [o_file_sub_btn_settings setEnabled:NO];
        [o_file_subtitles_filename_lbl setStringValue: @""];
        [o_file_subtitles_icon_well setImage: NULL];
    }
}

- (IBAction)subSettings:(id)sender
{
    [NSApp beginSheet: o_file_sub_sheet
        modalForWindow: [sender window]
        modalDelegate: self
        didEndSelector: NULL
        contextInfo: nil];
}

- (IBAction)subCloseSheet:(id)sender
{
    [self subsChanged: nil];
    [o_file_sub_sheet orderOut:sender];
    [NSApp endSheet: o_file_sub_sheet];
}

- (IBAction)subFileBrowse:(id)sender
{
    NSOpenPanel *o_open_panel = [NSOpenPanel openPanel];

    [o_open_panel setAllowsMultipleSelection: NO];
    [o_open_panel setTitle: _NS("Open File")];
    [o_open_panel setPrompt: _NS("Open")];

    if ([o_open_panel runModal] == NSOKButton) {
        o_sub_path = [[[o_open_panel URLs] objectAtIndex:0] path];
        [o_sub_path retain];
        [o_file_subtitles_filename_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath:o_sub_path]];
        [o_file_sub_path_fld setStringValue: [o_file_subtitles_filename_lbl stringValue]];
        [o_file_sub_path_lbl setHidden: YES];
        [o_file_subtitles_icon_well setImage: [[NSWorkspace sharedWorkspace] iconForFile:o_sub_path]];
        [o_file_sub_icon_view setImage: [o_file_subtitles_icon_well image]];
    } else {
        [o_file_sub_path_lbl setHidden: NO];
        [o_file_sub_path_fld setStringValue:@""];
        [o_file_subtitles_filename_lbl setStringValue:@""];
        [o_file_subtitles_icon_well setImage: nil];
        [o_file_sub_icon_view setImage: nil];
    }
}

- (IBAction)subOverride:(id)sender
{
    BOOL b_state = [o_file_sub_override state];
    [o_file_sub_delay setEnabled: b_state];
    [o_file_sub_delay_stp setEnabled: b_state];
    [o_file_sub_fps setEnabled: b_state];
    [o_file_sub_fps_stp setEnabled: b_state];
}

#pragma mark -
#pragma mark Miscellaneous

- (IBAction)panelCancel:(id)sender
{
    [NSApp stopModalWithCode: 0];
}

- (IBAction)panelOk:(id)sender
{
    if ([[self MRL] length])
        [NSApp stopModalWithCode: 1];
    else
        NSBeep();
}

- (NSArray *)qtkvideoDevices
{
    if (!qtkvideoDevices)
        [self qtkrefreshVideoDevices];
    return qtkvideoDevices;
}

- (void)qtkrefreshVideoDevices
{
    [qtkvideoDevices release];
    qtkvideoDevices = [[[QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo] arrayByAddingObjectsFromArray:[QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeMuxed]] retain];
}

- (NSArray *)qtkaudioDevices
{
    if (!qtkaudioDevices)
        [self qtkrefreshAudioDevices];
    return qtkaudioDevices;
}

- (void)qtkrefreshAudioDevices
{
    [qtkaudioDevices release];
    qtkaudioDevices = [[[QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeSound] arrayByAddingObjectsFromArray:[QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeMuxed]] retain];
}

@end

@implementation VLCOpenTextField

- (void)mouseDown:(NSEvent *)theEvent
{
    [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCOpenTextFieldWasClicked"
                                                        object: self];
    [super mouseDown: theEvent];
}

@end
