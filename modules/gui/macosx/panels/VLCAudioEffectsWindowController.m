/*****************************************************************************
 * VLCAudioEffectsWindowController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2004-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan dot org>
 *          David Fuhrmann <dfuhrmann # videolan dot org>
 *          Jérôme Decoodt <djc@videolan.org>
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

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import "VLCAudioEffectsWindowController.h"

#import <vlc_common.h>
#import <math.h>

#import "../../../audio_filter/equalizer_presets.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"
#import "main/VLCMain.h"
#import "main/CompatibilityFixes.h"
#import "panels/dialogs/VLCPopupPanelController.h"
#import "panels/dialogs/VLCTextfieldPanelController.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "windows/video/VLCVideoOutputProvider.h"

NSString *VLCAudioEffectsEqualizerValuesKey = @"EQValues";
NSString *VLCAudioEffectsEqualizerPreampValuesKey = @"EQPreampValues";
NSString *VLCAudioEffectsEqualizerProfileTitlesKey = @"EQTitles";
NSString *VLCAudioEffectsEqualizerProfileNamesKey = @"EQNames";
NSString *VLCAudioEffectsProfilesKey = @"AudioEffectProfiles";
NSString *VLCAudioEffectsProfileNamesKey = @"AudioEffectProfileNames";

@interface VLCAudioEffectsWindowController ()
{
    VLCPlayerController *_playerController;
    VLCPopupPanelController *_popupPanel;
    VLCTextfieldPanelController *_textfieldPanel;
}
@end

#pragma mark -
#pragma mark Initialization

@implementation VLCAudioEffectsWindowController

+ (void)initialize
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    NSString *workString;
    NSMutableArray *workValues = [[NSMutableArray alloc] initWithCapacity:NB_PRESETS];
    NSMutableArray *workPreamp = [[NSMutableArray alloc] initWithCapacity:NB_PRESETS];
    NSMutableArray *workTitles = [[NSMutableArray alloc] initWithCapacity:NB_PRESETS];
    NSMutableArray *workNames = [[NSMutableArray alloc] initWithCapacity:NB_PRESETS];

    for (int i = 0 ; i < NB_PRESETS ; i++) {
        workString = [NSString stringWithFormat:@"%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f",
                      eqz_preset_10b[i].f_amp[0],
                      eqz_preset_10b[i].f_amp[1],
                      eqz_preset_10b[i].f_amp[2],
                      eqz_preset_10b[i].f_amp[3],
                      eqz_preset_10b[i].f_amp[4],
                      eqz_preset_10b[i].f_amp[5],
                      eqz_preset_10b[i].f_amp[6],
                      eqz_preset_10b[i].f_amp[7],
                      eqz_preset_10b[i].f_amp[8],
                      eqz_preset_10b[i].f_amp[9]];
        [workValues addObject:workString];
        [workPreamp addObject:[NSString stringWithFormat:@"%1.f", eqz_preset_10b[i].f_preamp]];
        [workTitles addObject:toNSStr(preset_list_text[i])];
        [workNames addObject:toNSStr(preset_list[i])];
    }

    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 [NSArray arrayWithArray:workValues], VLCAudioEffectsEqualizerValuesKey,
                                 [NSArray arrayWithArray:workPreamp], VLCAudioEffectsEqualizerPreampValuesKey,
                                 [NSArray arrayWithArray:workTitles], VLCAudioEffectsEqualizerProfileTitlesKey,
                                 [NSArray arrayWithArray:workNames], VLCAudioEffectsEqualizerProfileNamesKey,
                                 [NSArray arrayWithObject:[VLCAudioEffectsWindowController defaultProfileString]], VLCAudioEffectsProfilesKey,
                                 [NSArray arrayWithObject:_NS("Default")], VLCAudioEffectsProfileNamesKey,
                                  nil];
    [defaults registerDefaults:appDefaults];
}

+ (NSString *)defaultProfileString
{
    return [NSString stringWithFormat:@"ZmxhdA==;;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%i",
            .0,25.,100.,-11.,8.,2.5,7.,.85,1.,.4,.5,.5,2.,0];
}

- (id)init
{
    self = [super initWithWindowNibName:@"AudioEffects"];
    if (self) {
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_playerController = [[[VLCMain sharedInstance] playlistController] playerController];

            NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
            if ([defaults boolForKey:@"AudioEffectApplyProfileOnStartup"])
            {
                // This does not reset the UI (which does not exist yet), but it initalizes needed playlist vars
                [self equalizerUpdated];
                [self resetCompressor];
                [self resetSpatializer];
                [self resetAudioFilters];

                [self loadProfile];
            } else {
                [self saveCurrentProfileIndex:0];
            }
        });
    }

    return self;
}

- (NSInteger)getPresetIndexForProfile:(NSInteger)profileIndex
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *profile = [[defaults objectForKey:VLCAudioEffectsProfilesKey] objectAtIndex:profileIndex];
    NSString *presetName = B64DecNSStr([[profile componentsSeparatedByString:@";"] firstObject]);
    return [[defaults objectForKey:VLCAudioEffectsEqualizerProfileNamesKey] indexOfObject:presetName];
}

/// Loads values from profile into variables
- (void)loadProfile
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSInteger profileIndex = [self currentProfileIndex];
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (!p_aout)
        return;

    /* disable existing filters */
    aout_EnableFilter(p_aout, "equalizer", false);
    aout_EnableFilter(p_aout, "compressor", false);
    aout_EnableFilter(p_aout, "spatializer", false);
    aout_EnableFilter(p_aout, "compressor", false);
    aout_EnableFilter(p_aout, "headphone", false);
    aout_EnableFilter(p_aout, "normvol", false);
    aout_EnableFilter(p_aout, "karaoke", false);

    /* fetch preset */
    NSString *profileString;
    if (profileIndex == 0)
        profileString = [VLCAudioEffectsWindowController defaultProfileString];
    else
        profileString = [[defaults objectForKey:VLCAudioEffectsProfilesKey] objectAtIndex:profileIndex];

    NSArray *items = [profileString componentsSeparatedByString:@";"];

    /* eq preset */
    char const *psz_eq_preset = [B64DecNSStr([items firstObject]) UTF8String];
    var_SetString(p_aout, "equalizer-preset", psz_eq_preset);

    /* filter handling */
    NSString *audioFilters = B64DecNSStr([items objectAtIndex:1]);
    var_SetString(p_aout, "audio-filter", audioFilters.UTF8String);

    NSInteger presetIndex = [self getPresetIndexForProfile:profileIndex];

    /* values */
    var_SetFloat(p_aout, "compressor-rms-peak", [[items objectAtIndex:2] floatValue]);
    var_SetFloat(p_aout, "compressor-attack", [[items objectAtIndex:3] floatValue]);
    var_SetFloat(p_aout, "compressor-release", [[items objectAtIndex:4] floatValue]);
    var_SetFloat(p_aout, "compressor-threshold", [[items objectAtIndex:5] floatValue]);
    var_SetFloat(p_aout, "compressor-ratio", [[items objectAtIndex:6] floatValue]);
    var_SetFloat(p_aout, "compressor-knee", [[items objectAtIndex:7] floatValue]);
    var_SetFloat(p_aout, "compressor-makeup-gain", [[items objectAtIndex:8] floatValue]);
    var_SetFloat(p_aout, "spatializer-roomsize", [[items objectAtIndex:9] floatValue]);
    var_SetFloat(p_aout, "spatializer-width", [[items objectAtIndex:10] floatValue]);
    var_SetFloat(p_aout, "spatializer-wet", [[items objectAtIndex:11] floatValue]);
    var_SetFloat(p_aout, "spatializer-dry", [[items objectAtIndex:12] floatValue]);
    var_SetFloat(p_aout, "spatializer-damp", [[items objectAtIndex:13] floatValue]);
    var_SetFloat(p_aout, "norm-max-level", [[items objectAtIndex:14] floatValue]);
    var_SetBool(p_aout, "equalizer-2pass", (BOOL)[[items objectAtIndex:15] intValue]);
    var_SetString(p_aout, "equalizer-bands", [[[defaults objectForKey:VLCAudioEffectsEqualizerValuesKey] objectAtIndex:presetIndex] UTF8String]);
    var_SetFloat(p_aout, "equalizer-preamp", [[[defaults objectForKey:VLCAudioEffectsEqualizerPreampValuesKey] objectAtIndex:presetIndex] floatValue]);
    var_SetString(p_aout, "equalizer-preset", [[[defaults objectForKey:VLCAudioEffectsEqualizerProfileNamesKey] objectAtIndex:presetIndex] UTF8String]);

    aout_Release(p_aout);
}

- (void)windowDidLoad
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(updateCocoaWindowLevel:)
                               name:VLCWindowShouldUpdateLevel
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(saveCurrentProfileAtTerminate:)
                               name:NSApplicationWillTerminateNotification
                             object:nil];

    [_applyProfileCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"AudioEffectApplyProfileOnStartup"]];
    [_applyProfileCheckbox setTitle:_NS("Apply profile at next launch")];

    /* setup the user's language */
    /* Equalizer */
    [_equalizerEnableCheckbox setTitle:_NS("Enable")];
    [_equalizerTwoPassCheckbox setTitle:_NS("2 Pass")];
    [_equalizerTwoPassCheckbox setToolTip:_NS("Filter the audio twice. This provides a more "  \
                                              "intense effect.")];
    [_equalizerPreampLabel setStringValue:_NS("Preamp")];
    [_equalizerPreampLabel setToolTip:_NS("Set the global gain in dB (-20 ... 20).")];

    /* Compressor */
    [_compressorEnableCheckbox setTitle:_NS("Enable dynamic range compressor")];
    [_compressorResetButton setTitle:_NS("Reset")];
    [_compressorBand1Label setStringValue:_NS("RMS/peak")];;
    [_compressorBand2Label setStringValue:_NS("Attack")];
    [_compressorBand3Label setStringValue:_NS("Release")];
    [_compressorBand4Label setStringValue:_NS("Threshold")];
    [_compressorBand5Label setStringValue:_NS("Ratio")];
    [_compressorBand6Label setStringValue:_NS("Knee radius")];
    [_compressorBand7Label setStringValue:_NS("Makeup gain")];

    /* Spatializer */
    [_spatializerEnableCheckbox setTitle:_NS("Enable Spatializer")];
    [_spatializerResetButton setTitle:_NS("Reset")];
    [_spatializerBand1Label setStringValue:_NS("Size")];
    [_spatializerBand1Label setToolTip:_NS("Defines the virtual surface of the room" \
                                           " emulated by the filter.")];
    [_spatializerBand2Label setStringValue:_NS("Width")];
    [_spatializerBand2Label setToolTip:_NS("Width of the virtual room")];
    [_spatializerBand3Label setStringValue:_NS("Wet")];
    [_spatializerBand4Label setStringValue:_NS("Dry")];
    [_spatializerBand5Label setStringValue:_NS("Damp")];

    /* Filter */
    [_filterHeadPhoneCheckbox setTitle:_NS("Headphone virtualization")];
    [_filterHeadPhoneCheckbox setToolTip:_NS("This effect gives you the feeling that you are standing in a room " \
                                             "with a complete 7.1 speaker set when using only a headphone, " \
                                             "providing a more realistic sound experience. It should also be " \
                                             "more comfortable and less tiring when listening to music for " \
                                             "long periods of time.\nIt works with any source format from mono " \
                                             "to 7.1.")];
    [_filterNormLevelCheckbox setTitle:_NS("Volume normalization")];
    [_filterNormLevelCheckbox setToolTip:_NS("Volume normalizer")];
    [_filterNormLevelLabel setToolTip:_NS("If the average power over the last N buffers " \
                                          "is higher than this value, the volume will be normalized. " \
                                          "This value is a positive floating point number. A value " \
                                          "between 0.5 and 10 seems sensible.")];
    [_filterNormLevelLabel setStringValue:_NS("Maximum level")];
    [_filterKaraokeCheckbox setTitle:_NS("Karaoke")];
    [_filterKaraokeCheckbox setToolTip:_NS("Simple Karaoke filter")];
    [_filterScaleTempoCheckbox setTitle:_NS("Scaletempo")];
    [_filterScaleTempoCheckbox setToolTip:_NS("Audio tempo scaler synched with rate")];
    [_filterStereoEnhancerCheckbox setTitle:_NS("Stereo Enhancer")];
    [_filterStereoEnhancerCheckbox setToolTip:_NS("This filter enhances the stereo effect by "\
                                                  "suppressing mono (signal common to both channels) "\
                                                  "and by delaying the signal of left into right and vice versa, "\
                                                  "thereby widening the stereo effect.")];

    /* generic */
    [_segmentView setLabel:_NS("Equalizer") forSegment:0];
    [_segmentView setLabel:_NS("Compressor") forSegment:1];
    [_segmentView setLabel:_NS("Spatializer") forSegment:2];
    [_segmentView setLabel:_NS("Filter") forSegment:3];

    [self.window setTitle:_NS("Audio Effects")];
    [self.window setExcludedFromWindowsMenu:YES];
    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [self equalizerUpdated];
    [self resetCompressor];
    [self resetSpatializer];
    [self resetAudioFilters];
    [self resetProfileSelector];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark -
#pragma mark internal functions


- (void)saveCurrentProfileIndex:(NSInteger)index
{
    [[NSUserDefaults standardUserDefaults] setInteger:index forKey:@"AudioEffectSelectedProfile"];
}

- (NSInteger)currentProfileIndex
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:@"AudioEffectSelectedProfile"];
}

/// Returns the list of profile names (omitting the Default entry)
- (NSArray *)nonDefaultProfileNames
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    NSMutableArray *names = [[defaults stringArrayForKey:VLCAudioEffectsProfileNamesKey] mutableCopy];
    [names removeObjectAtIndex:0];
    return [names copy];
}

- (void)resetProfileSelector
{
    [_profilePopup removeAllItems];

    // Ignore "Default" index 0 from settings
    [_profilePopup addItemWithTitle:_NS("Default")];

    [_profilePopup addItemsWithTitles:[self nonDefaultProfileNames]];

    [[_profilePopup menu] addItem:[NSMenuItem separatorItem]];
    [_profilePopup addItemWithTitle:_NS("Duplicate current profile...")];
    [[_profilePopup lastItem] setTarget: self];
    [[_profilePopup lastItem] setAction: @selector(addAudioEffectsProfile:)];

    if ([[self nonDefaultProfileNames] count] > 0) {
        [_profilePopup addItemWithTitle:_NS("Organize Profiles...")];
        [[_profilePopup lastItem] setTarget: self];
        [[_profilePopup lastItem] setAction: @selector(removeAudioEffectsProfile:)];
    }

    [_profilePopup selectItemAtIndex:[self currentProfileIndex]];
    // Loading only non-default profiles ensures that vlcrc or command line settings are not overwritten
    if ([self currentProfileIndex] > 0)
        [self profileSelectorAction:self];
}

#pragma mark -
#pragma mark generic code
- (void)updateCocoaWindowLevel:(NSNotification *)aNotification
{
    NSInteger i_level = [aNotification.userInfo[VLCWindowLevelKey] integerValue];
    if (self.isWindowLoaded && [self.window isVisible] && [self.window level] != i_level)
        [self.window setLevel: i_level];
}

- (IBAction)toggleWindow:(id)sender
{
    if ([self.window isKeyWindow])
        [self.window orderOut:sender];
    else {
        [self.window setLevel: [[[VLCMain sharedInstance] voutProvider] currentStatusWindowLevel]];
        [self.window makeKeyAndOrderFront:sender];
    }
}

- (NSString *)generateProfileString
{
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (!p_aout)
        return nil;

    return [NSString stringWithFormat:@"%@;%@;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%i",
                     B64EncAndFree(var_GetNonEmptyString(p_aout, "equalizer-preset")),
                     B64EncAndFree(var_InheritString(p_aout, "audio-filter")),
                     var_InheritFloat(p_aout, "compressor-rms-peak"),
                     var_InheritFloat(p_aout, "compressor-attack"),
                     var_InheritFloat(p_aout, "compressor-release"),
                     var_InheritFloat(p_aout, "compressor-threshold"),
                     var_InheritFloat(p_aout, "compressor-ratio"),
                     var_InheritFloat(p_aout, "compressor-knee"),
                     var_InheritFloat(p_aout, "compressor-makeup-gain"),
                     var_InheritFloat(p_aout, "spatializer-roomsize"),
                     var_InheritFloat(p_aout, "spatializer-width"),
                     var_InheritFloat(p_aout, "spatializer-wet"),
                     var_InheritFloat(p_aout, "spatializer-dry"),
                     var_InheritFloat(p_aout, "spatializer-damp"),
                     var_InheritFloat(p_aout, "norm-max-level"),
                     var_InheritBool(p_aout,"equalizer-2pass")];
}

- (void)saveCurrentProfile
{
    NSInteger currentProfileIndex = [self currentProfileIndex];
    if (currentProfileIndex == 0)
        return;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    /* fetch all the current settings in a uniform string */
    NSString *newProfile = [self generateProfileString];

    NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsProfilesKey]];
    if (currentProfileIndex >= [workArray count])
        return;

    [workArray replaceObjectAtIndex:currentProfileIndex withObject:newProfile];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsProfilesKey];
}

- (void)saveCurrentProfileAtTerminate:(NSNotification *)notification
{
    if ([self currentProfileIndex] > 0) {
        [self saveCurrentProfile];
        return;
    }

    if (_applyProfileCheckbox.state == NSOffState)
        return;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([[self generateProfileString] compare:[VLCAudioEffectsWindowController defaultProfileString]] == NSOrderedSame)
        return;

    // FIXME: Current code does not allow auto save of equalizer profiles, those profiles currently need to be saved
    // individually with the popup menu, before saving the overall profile.
    // Below code for auto duplication is not enough to fix current behaviour and results in too many duplicated profiles
    // For auto-saving of eq profiles, a different stragety needs to be found, involving save also once overall profile changes.
/*
    NSString *newPresetString = [NSString stringWithCString:var_InheritString(p_playlist, "equalizer-bands") encoding:NSASCIIStringEncoding];
    float newPresetPreamp = var_InheritFloat(p_playlist, "equalizer-preamp");

    // TODO: Comparing against profile 0 is mostly useless and looks wrong (profile 0 is flat usually)
    NSInteger defaultPresetIndex = [self getPresetIndexForProfile:0];
    NSString *defaultPresetString = [[defaults objectForKey:VLCAudioEffectsEqualizerValuesKey] objectAtIndex:defaultPresetIndex];
    float defaultPresetPreamp = [[[defaults objectForKey:VLCAudioEffectsEqualizerPreampValuesKey] objectAtIndex:defaultPresetIndex] floatValue];

    NSMutableArray *workArray;
    int num_custom;

    if ([newPresetString compare:defaultPresetString] != NSOrderedSame ||
        newPresetPreamp != defaultPresetPreamp)
    {
        // preset title
        NSArray<NSString *> *presetTitles = [defaults objectForKey:VLCAudioEffectsEqualizerProfileTitlesKey];
        NSString *newPresetTitle;

        num_custom = 0;
        do
            newPresetTitle = [@"Custom" stringByAppendingString:[NSString stringWithFormat:@"%03i",num_custom++]];
        while ([presetTitles containsObject:newPresetTitle]);

        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerProfileTitlesKey]];
        [workArray addObject:newPresetTitle];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerProfileTitlesKey];

        // preset name
        NSString *decomposedStringWithCanonicalMapping = [newPresetTitle decomposedStringWithCanonicalMapping];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerProfileNamesKey]];
        [workArray addObject:decomposedStringWithCanonicalMapping];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerProfileNamesKey];
        var_SetString(p_playlist, "equalizer-preset", [decomposedStringWithCanonicalMapping UTF8String]);

        // preset bands
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerValuesKey]];
        [workArray addObject:newPresetString];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerValuesKey];

        // preset preamp
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerPreampValuesKey]];
        [workArray addObject:[NSString stringWithFormat:@"%.1f", [_equalizerPreampSlider floatValue]]];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerPreampValuesKey];
    }
*/

    NSMutableArray *workArray;
    /* profile string */
    workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsProfilesKey]];
    [workArray addObject:[self generateProfileString]];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsProfilesKey];

    /* profile name */
    NSArray<NSString *> *profileNames = [defaults objectForKey:VLCAudioEffectsProfileNamesKey];
    NSString *newProfileName;

    int num_custom = 0;
    do
        newProfileName = [@"Custom" stringByAppendingString:[NSString stringWithFormat:@"%03i",num_custom++]];
    while ([profileNames containsObject:newProfileName]);

    workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsProfileNamesKey]];
    [workArray addObject:newProfileName];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsProfileNamesKey];

    [self saveCurrentProfileIndex:([workArray count] - 1)];
}

- (IBAction)profileSelectorAction:(id)sender
{
    [self saveCurrentProfile];

    [self saveCurrentProfileIndex:[_profilePopup indexOfSelectedItem]];
    [self loadProfile];

    /* update UI */
    [self equalizerUpdated];
    [self resetCompressor];
    [self resetSpatializer];
    [self resetAudioFilters];
    [self updatePresetSelector];
}

- (void)addAudioEffectsProfile:(id)sender
{
    if (!_textfieldPanel) {
        _textfieldPanel = [[VLCTextfieldPanelController alloc] init];
    }

    /* show panel */
    [_textfieldPanel setTitleString:_NS("Duplicate current profile for a new profile")];
    [_textfieldPanel setSubTitleString:_NS("Enter a name for the new profile:")];
    [_textfieldPanel setCancelButtonString:_NS("Cancel")];
    [_textfieldPanel setOkButtonString:_NS("Save")];

    __weak typeof(self) _self = self;
    [_textfieldPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSString *resultingText) {

        NSInteger currentProfileIndex = [_self currentProfileIndex];
        if (returnCode != NSModalResponseOK) {
            [self->_profilePopup selectItemAtIndex:currentProfileIndex];
            return;
        }

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSArray *profileNames = [defaults objectForKey:VLCAudioEffectsProfileNamesKey];

        // duplicate names are not allowed in the popup control
        if ([resultingText length] == 0 || [profileNames containsObject:resultingText]) {
            [self->_profilePopup selectItemAtIndex:currentProfileIndex];

            NSAlert *alert = [[NSAlert alloc] init];
            [alert setAlertStyle:NSCriticalAlertStyle];
            [alert setMessageText:_NS("Please enter a unique name for the new profile.")];
            [alert setInformativeText:_NS("Multiple profiles with the same name are not allowed.")];
            [alert beginSheetModalForWindow:_self.window
                          completionHandler:nil];
            return;
        }

        NSString *newProfile = [_self generateProfileString];

        /* add string to user defaults as well as a label */
        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsProfilesKey]];
        [workArray addObject:newProfile];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsProfilesKey];

        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsProfileNamesKey]];
        [workArray addObject:resultingText];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsProfileNamesKey];

        [_self saveCurrentProfileIndex:([workArray count] - 1)];

        [_self resetProfileSelector];
    }];
}

- (void)removeAudioEffectsProfile:(id)sender
{
    if (!_popupPanel) {
        _popupPanel = [[VLCPopupPanelController alloc] init];
    }

    /* show panel */
    [_popupPanel setTitleString:_NS("Remove a preset")];
    [_popupPanel setSubTitleString:_NS("Select the preset you would like to remove:")];
    [_popupPanel setOkButtonString:_NS("Remove")];
    [_popupPanel setCancelButtonString:_NS("Cancel")];
    [_popupPanel setPopupButtonContent:[self nonDefaultProfileNames]];

    __weak typeof(self) _self = self;
    [_popupPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSInteger selectedIndex) {

        NSInteger currentProfileIndex = [_self currentProfileIndex];
        if (returnCode != NSModalResponseOK) {
            [self->_profilePopup selectItemAtIndex:currentProfileIndex];
            return;
        }

        // Popup panel does not contain the "Default" entry
        selectedIndex++;

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        /* remove selected profile from settings */
        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsProfilesKey]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsProfilesKey];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsProfileNamesKey]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsProfileNamesKey];

        if (currentProfileIndex >= selectedIndex)
            [_self saveCurrentProfileIndex:(currentProfileIndex - 1)];

        [_self resetProfileSelector];
    }];
}

- (IBAction)applyProfileCheckboxChanged:(id)sender
{
    [[NSUserDefaults standardUserDefaults] setBool:[sender state] forKey:@"AudioEffectApplyProfileOnStartup"];
}

#pragma mark -
#pragma mark Equalizer
static bool GetEqualizerStatus(intf_thread_t *p_custom_intf,
                               VLCPlayerController *playerController,
                               char *psz_name)
{
    char *psz_parser, *psz_string = NULL;
    audio_output_t *p_aout = [playerController mainAudioOutput];
    if (!p_aout)
        return false;

    psz_string = var_InheritString(p_aout, "audio-filter");

    aout_Release(p_aout);

    if (!psz_string)
        return false;

    psz_parser = strstr(psz_string, psz_name);

    free(psz_string);

    if (psz_parser)
        return true;
    else
        return false;
}

- (void)updatePresetSelector
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *presets = [defaults objectForKey:VLCAudioEffectsEqualizerProfileNamesKey];

    [_equalizerPresetsPopup removeAllItems];
    [_equalizerPresetsPopup addItemsWithTitles:[[NSUserDefaults standardUserDefaults] objectForKey:VLCAudioEffectsEqualizerProfileTitlesKey]];
    [[_equalizerPresetsPopup menu] addItem:[NSMenuItem separatorItem]];
    [_equalizerPresetsPopup addItemWithTitle:_NS("Add new Preset...")];
    [[_equalizerPresetsPopup lastItem] setTarget: self];
    [[_equalizerPresetsPopup lastItem] setAction: @selector(addPresetAction:)];

    if ([presets count] > 1) {
        [_equalizerPresetsPopup addItemWithTitle:_NS("Organize Presets...")];
        [[_equalizerPresetsPopup lastItem] setTarget: self];
        [[_equalizerPresetsPopup lastItem] setAction: @selector(deletePresetAction:)];
    }

    audio_output_t *p_aout = [_playerController mainAudioOutput];

    NSString *currentPreset = nil;
    if (p_aout) {
        char *psz_preset_string = var_GetNonEmptyString(p_aout, "equalizer-preset");
        currentPreset = toNSStr(psz_preset_string);
        free(psz_preset_string);
        aout_Release(p_aout);
    }

    NSUInteger currentPresetIndex = 0;
    if (currentPreset && [currentPreset length] > 0) {
        currentPresetIndex = [presets indexOfObject:currentPreset];

        if (currentPresetIndex == NSNotFound)
            currentPresetIndex = [presets count] - 1;
    }

    [_equalizerPresetsPopup selectItemAtIndex:currentPresetIndex];
    [self equalizerChangePreset:_equalizerPresetsPopup];

    [_equalizerPreampSlider setFloatValue:[[[defaults objectForKey:VLCAudioEffectsEqualizerPreampValuesKey] objectAtIndex:currentPresetIndex] floatValue]];
    [self setBandSliderValuesForPreset:currentPresetIndex];
}

- (void)equalizerUpdated
{
    intf_thread_t *p_intf = getIntf();
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (!p_aout)
        return;
    bool b_2p = var_CreateGetBool(p_aout, "equalizer-2pass");
    bool bEnabled = GetEqualizerStatus(p_intf, _playerController, (char *)"equalizer");

    /* Setup sliders */
    var_Create(p_aout, "equalizer-preset",
               VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create(p_aout, "equalizer-bands",
               VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create(p_aout, "equalizer-preamp",
               VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    [self updatePresetSelector];

    /* Set the the checkboxes */
    [_equalizerView enableSubviews: bEnabled];
    [_equalizerEnableCheckbox setState: bEnabled];
    [_equalizerTwoPassCheckbox setState: b_2p];
}

- (id)sliderByIndex:(int)index
{
    switch(index) {
        case 0 : return _equalizerBand1Slider;
        case 1 : return _equalizerBand2Slider;
        case 2 : return _equalizerBand3Slider;
        case 3 : return _equalizerBand4Slider;
        case 4 : return _equalizerBand5Slider;
        case 5 : return _equalizerBand6Slider;
        case 6 : return _equalizerBand7Slider;
        case 7 : return _equalizerBand8Slider;
        case 8 : return _equalizerBand9Slider;
        case 9 : return _equalizerBand10Slider;
        default : return nil;
    }
}

- (void)setBandSliderValuesForPreset:(NSInteger)presetID
{
    NSString *preset = [[[NSUserDefaults standardUserDefaults] objectForKey:VLCAudioEffectsEqualizerValuesKey] objectAtIndex:presetID];
    NSArray *values = [preset componentsSeparatedByString:@" "];
    NSUInteger count = [values count];
    for (int x = 0; x < count; x++)
        [self setValue:[[values objectAtIndex:x] floatValue] forSlider:x];
}

- (NSString *)generatePresetString
{
    return [NSString stringWithFormat:@"%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f",
            [_equalizerBand1Slider floatValue],
            [_equalizerBand2Slider floatValue],
            [_equalizerBand3Slider floatValue],
            [_equalizerBand4Slider floatValue],
            [_equalizerBand5Slider floatValue],
            [_equalizerBand6Slider floatValue],
            [_equalizerBand7Slider floatValue],
            [_equalizerBand8Slider floatValue],
            [_equalizerBand9Slider floatValue],
            [_equalizerBand10Slider floatValue]];
}

- (void)setValue:(float)value forSlider:(int)index
{
    id slider = [self sliderByIndex:index];

    if (slider != nil)
        [slider setFloatValue:value];
}

- (IBAction)equalizerEnable:(id)sender
{
    [_equalizerView enableSubviews:[sender state]];
    [_playerController enableAudioFilterWithName:@"equalizer" state:[sender state]];
}

- (IBAction)equalizerBandSliderUpdated:(id)sender
{
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (!p_aout)
        return;
    char const *psz_preset_values = [[self generatePresetString] UTF8String];
    var_SetString(p_aout, "equalizer-bands", psz_preset_values);
    aout_Release(p_aout);
}

- (IBAction)equalizerChangePreset:(id)sender
{
    NSInteger numberOfChosenPreset = [sender indexOfSelectedItem];
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    char const *psz_eq_bands = [[[defaults objectForKey:VLCAudioEffectsEqualizerValuesKey] objectAtIndex:numberOfChosenPreset] UTF8String];
    float f_eq_preamp = [[[defaults objectForKey:VLCAudioEffectsEqualizerPreampValuesKey] objectAtIndex:numberOfChosenPreset] floatValue];
    char const *psz_eq_preset = [[[defaults objectForKey:VLCAudioEffectsEqualizerProfileNamesKey] objectAtIndex:numberOfChosenPreset] UTF8String];

    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (p_aout) {
        var_SetString(p_aout, "equalizer-bands", psz_eq_bands);
        var_SetFloat(p_aout, "equalizer-preamp", f_eq_preamp);
        var_SetString(p_aout, "equalizer-preset" , psz_eq_preset);
        aout_Release(p_aout);
    }

    [_equalizerPreampSlider setFloatValue: f_eq_preamp];
    [self setBandSliderValuesForPreset:numberOfChosenPreset];
}

- (IBAction)equalizerPreAmpSliderUpdated:(id)sender
{
    float fPreamp = [sender floatValue] ;

    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (p_aout) {
        var_SetFloat(p_aout, "equalizer-preamp", fPreamp);
        aout_Release(p_aout);
    }
}

- (IBAction)equalizerTwoPass:(id)sender
{
    bool b_2p = [sender state] ? true : false;

    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (p_aout) {
        var_SetBool(p_aout, "equalizer-2pass", b_2p);
        aout_Release(p_aout);
    }
}

- (IBAction)addPresetAction:(id)sender
{
    if (!_textfieldPanel) {
        _textfieldPanel = [[VLCTextfieldPanelController alloc] init];
    }

    /* show panel */
    [_textfieldPanel setTitleString:_NS("Save current selection as new preset")];
    [_textfieldPanel setSubTitleString:_NS("Enter a name for the new preset:")];
    [_textfieldPanel setCancelButtonString:_NS("Cancel")];
    [_textfieldPanel setOkButtonString:_NS("Save")];

    __weak typeof(self) _self = self;
    [_textfieldPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSString *resultingText) {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

        // EQ settings
        if (returnCode != NSModalResponseOK || [resultingText length] == 0)
            return;

        NSString *decomposedStringWithCanonicalMapping = [resultingText decomposedStringWithCanonicalMapping];
        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerValuesKey]];
        [workArray addObject:[self generatePresetString]];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerValuesKey];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerProfileTitlesKey]];
        [workArray addObject:resultingText];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerProfileTitlesKey];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerPreampValuesKey]];
        [workArray addObject:[NSString stringWithFormat:@"%.1f", [self->_equalizerPreampSlider floatValue]]];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerPreampValuesKey];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerProfileNamesKey]];
        [workArray addObject:decomposedStringWithCanonicalMapping];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerProfileNamesKey];

        /* update VLC internals */
        char const *psz_eq_preset = [decomposedStringWithCanonicalMapping UTF8String];
        audio_output_t *p_aout = [self->_playerController mainAudioOutput];
        if (p_aout) {
            var_SetString(p_aout, "equalizer-preset", psz_eq_preset);
            aout_Release(p_aout);
        }

        /* update UI */
        [_self updatePresetSelector];
    }];
}

- (IBAction)deletePresetAction:(id)sender
{
    if (!_popupPanel) {
        _popupPanel = [[VLCPopupPanelController alloc] init];
    }

    [_popupPanel setTitleString:_NS("Remove a preset")];
    [_popupPanel setSubTitleString:_NS("Select the preset you would like to remove:")];
    [_popupPanel setOkButtonString:_NS("Remove")];
    [_popupPanel setCancelButtonString:_NS("Cancel")];
    [_popupPanel setPopupButtonContent:[[NSUserDefaults standardUserDefaults] objectForKey:VLCAudioEffectsEqualizerProfileTitlesKey]];

    __weak typeof(self) _self = self;
    [_popupPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSInteger selectedIndex) {

        if (returnCode != NSModalResponseOK)
            return;

        /* remove requested profile from the arrays */
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerValuesKey]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerValuesKey];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerProfileTitlesKey]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerProfileTitlesKey];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerPreampValuesKey]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerPreampValuesKey];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:VLCAudioEffectsEqualizerProfileNamesKey]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:VLCAudioEffectsEqualizerProfileNamesKey];

        /* update UI */
        [_self updatePresetSelector];
    }];
}

#pragma mark -
#pragma mark Compressor
- (void)resetCompressor
{
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (!p_aout)
        return;
    BOOL bEnable_compressor = NO;
    char *psz_afilters;

    psz_afilters = var_InheritString(p_aout, "audio-filter");
    if (psz_afilters) {
        bEnable_compressor = strstr(psz_afilters, "compressor") != NULL;
        [_compressorEnableCheckbox setState: (NSInteger)strstr(psz_afilters, "compressor") ];
        free(psz_afilters);
    }

    [_compressorView enableSubviews:bEnable_compressor];
    [_compressorEnableCheckbox setState:(bEnable_compressor ? NSOnState : NSOffState)];

    [_compressorBand1Slider setFloatValue: var_CreateGetFloat(p_aout, "compressor-rms-peak")];
    [_compressorBand1TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [_compressorBand1Slider floatValue]]];
    [_compressorBand2Slider setFloatValue: var_CreateGetFloat(p_aout, "compressor-attack")];
    [_compressorBand2TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f ms", [_compressorBand2Slider floatValue]]];
    [_compressorBand3Slider setFloatValue: var_CreateGetFloat(p_aout, "compressor-release")];
    [_compressorBand3TextField setStringValue:[NSString localizedStringWithFormat:@"%3.1f ms", [_compressorBand3Slider floatValue]]];
    [_compressorBand4Slider setFloatValue: var_CreateGetFloat(p_aout, "compressor-threshold")];
    [_compressorBand4TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f dB", [_compressorBand4Slider floatValue]]];
    [_compressorBand5Slider setFloatValue: var_CreateGetFloat(p_aout, "compressor-ratio")];
    [_compressorBand5TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f:1", [_compressorBand5Slider floatValue]]];
    [_compressorBand6Slider setFloatValue: var_CreateGetFloat(p_aout, "compressor-knee")];
    [_compressorBand6TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [_compressorBand6Slider floatValue]]];
    [_compressorBand7Slider setFloatValue: var_CreateGetFloat(p_aout, "compressor-makeup-gain")];
    [_compressorBand7TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [_compressorBand7Slider floatValue]]];

    aout_Release(p_aout);
}

- (IBAction)resetCompressorValues:(id)sender
{
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (p_aout) {
        var_SetFloat(p_aout, "compressor-rms-peak", 0.000000);
        var_SetFloat(p_aout, "compressor-attack", 25.000000);
        var_SetFloat(p_aout, "compressor-release", 100.000000);
        var_SetFloat(p_aout, "compressor-threshold", -11.000000);
        var_SetFloat(p_aout, "compressor-ratio", 8.000000);
        var_SetFloat(p_aout, "compressor-knee", 2.500000);
        var_SetFloat(p_aout, "compressor-makeup-gain", 7.000000);
        aout_Release(p_aout);
    }
    [self resetCompressor];
}

- (IBAction)compressorEnable:(id)sender
{
    [_compressorView enableSubviews:[sender state]];
    [_playerController enableAudioFilterWithName:@"compressor" state:[sender state]];
}

- (IBAction)compressorSliderUpdated:(id)sender
{
    char *psz_property = nil;
    float f_value = [sender floatValue];

    if (sender == _compressorBand1Slider)
        psz_property = "compressor-rms-peak";
    else if (sender == _compressorBand2Slider)
        psz_property = "compressor-attack";
    else if (sender == _compressorBand3Slider)
        psz_property = "compressor-release";
    else if (sender == _compressorBand4Slider)
        psz_property = "compressor-threshold";
    else if (sender == _compressorBand5Slider)
        psz_property = "compressor-ratio";
    else if (sender == _compressorBand6Slider)
        psz_property = "compressor-knee";
    else if (sender == _compressorBand7Slider)
        psz_property = "compressor-makeup-gain";

    assert(psz_property);

    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (p_aout) {
        var_SetFloat(p_aout, psz_property, f_value);
        aout_Release(p_aout);
    }

    if (sender == _compressorBand1Slider)
        [_compressorBand1TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
    else if (sender == _compressorBand2Slider)
        [_compressorBand2TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f ms", f_value]];
    else if (sender == _compressorBand3Slider)
        [_compressorBand3TextField setStringValue:[NSString localizedStringWithFormat:@"%3.1f ms", f_value]];
    else if (sender == _compressorBand4Slider)
        [_compressorBand4TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f dB", f_value]];
    else if (sender == _compressorBand5Slider)
        [_compressorBand5TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f:1", f_value]];
    else if (sender == _compressorBand6Slider)
        [_compressorBand6TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", f_value]];
    else if (sender == _compressorBand7Slider)
        [_compressorBand7TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", f_value]];
}

#pragma mark -
#pragma mark Spatializer
- (void)resetSpatializer
{
    BOOL bEnable_spatializer = NO;
    char *psz_afilters = NULL;
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (!p_aout)
        return;

    psz_afilters = var_InheritString(p_aout, "audio-filter");
    if (psz_afilters) {
        bEnable_spatializer = strstr(psz_afilters, "spatializer") != NULL;
        free(psz_afilters);
    }

    [_spatializerView enableSubviews:bEnable_spatializer];
    [_spatializerEnableCheckbox setState:(bEnable_spatializer ? NSOnState : NSOffState)];


#define setSlider(bandsld, bandfld, var) \
[bandsld setFloatValue: var_CreateGetFloat(p_aout, var) * 10.]; \
[bandfld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [bandsld floatValue]]]

    setSlider(_spatializerBand1Slider, _spatializerBand1TextField, "spatializer-roomsize");
    setSlider(_spatializerBand2Slider, _spatializerBand2TextField, "spatializer-width");
    setSlider(_spatializerBand3Slider, _spatializerBand3TextField, "spatializer-wet");
    setSlider(_spatializerBand4Slider, _spatializerBand4TextField, "spatializer-dry");
    setSlider(_spatializerBand5Slider, _spatializerBand5TextField, "spatializer-damp");

#undef setSlider

    aout_Release(p_aout);
}

- (IBAction)resetSpatializerValues:(id)sender
{
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (p_aout) {
        var_SetFloat(p_aout, "spatializer-roomsize", .85);
        var_SetFloat(p_aout, "spatializer-width", 1.);
        var_SetFloat(p_aout, "spatializer-wet", .4);
        var_SetFloat(p_aout, "spatializer-dry", .5);
        var_SetFloat(p_aout, "spatializer-damp", .5);
        aout_Release(p_aout);
    }
    [self resetSpatializer];
}

- (IBAction)spatializerEnable:(id)sender
{
    [_spatializerView enableSubviews:[sender state]];
    [_playerController enableAudioFilterWithName:@"spatializer" state:[sender state]];
}

- (IBAction)spatializerSliderUpdated:(id)sender
{
    char *psz_property = nil;
    float f_value = [sender floatValue];

    if (sender == _spatializerBand1Slider)
        psz_property = "spatializer-roomsize";
    else if (sender == _spatializerBand2Slider)
        psz_property = "spatializer-width";
    else if (sender == _spatializerBand3Slider)
        psz_property = "spatializer-wet";
    else if (sender == _spatializerBand4Slider)
        psz_property = "spatializer-dry";
    else if (sender == _spatializerBand5Slider)
        psz_property = "spatializer-damp";

    assert(psz_property);

    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (p_aout) {
        var_SetFloat(p_aout, psz_property, f_value / 10.f);
        aout_Release(p_aout);
    }

    if (sender == _spatializerBand1Slider)
        [_spatializerBand1TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
    else if (sender == _spatializerBand2Slider)
        [_spatializerBand2TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
    else if (sender == _spatializerBand3Slider)
        [_spatializerBand3TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
    else if (sender == _spatializerBand4Slider)
        [_spatializerBand4TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
    else if (sender == _spatializerBand5Slider)
        [_spatializerBand5TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
}

#pragma mark -
#pragma mark Filter
- (void)resetAudioFilters
{
    BOOL bEnable_normvol = NO;
    char *psz_afilters;
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (!p_aout)
        return;
    psz_afilters = var_InheritString(p_aout, "audio-filter");
    if (psz_afilters) {
        [_filterHeadPhoneCheckbox setState: (NSInteger)strstr(psz_afilters, "headphone") ];
        [_filterKaraokeCheckbox setState: (NSInteger)strstr(psz_afilters, "karaoke") ];
        [_filterScaleTempoCheckbox setState: (NSInteger)strstr(psz_afilters, "scaletempo") ];
        [_filterStereoEnhancerCheckbox setState: (NSInteger)strstr(psz_afilters, "stereo_widen") ];
        bEnable_normvol = strstr(psz_afilters, "normvol") != NULL;
        free(psz_afilters);
    } else {
        [_filterHeadPhoneCheckbox setState: NSOffState];
        [_filterKaraokeCheckbox setState: NSOffState];
        [_filterScaleTempoCheckbox setState: NSOffState];
        [_filterStereoEnhancerCheckbox setState: NSOffState];
    }

    [_filterNormLevelSlider setEnabled:bEnable_normvol];
    [_filterNormLevelLabel setEnabled:bEnable_normvol];
    [_filterNormLevelCheckbox setState:(bEnable_normvol ? NSOnState : NSOffState)];

    [_filterNormLevelSlider setFloatValue: var_CreateGetFloat(p_aout, "norm-max-level")];
    aout_Release(p_aout);
}

- (IBAction)filterEnableHeadPhoneVirt:(id)sender
{
    [_playerController enableAudioFilterWithName:@"headphone" state:[sender state]];
}

- (IBAction)filterEnableVolumeNorm:(id)sender
{
    [_filterNormLevelSlider setEnabled:[sender state]];
    [_filterNormLevelLabel setEnabled:[sender state]];
    [_playerController enableAudioFilterWithName:@"normvol" state:[sender state]];
}

- (IBAction)filterVolumeNormSliderUpdated:(id)sender
{
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    float f_value = [_filterNormLevelSlider floatValue];

    if (p_aout) {
        var_SetFloat(p_aout, "norm-max-level", f_value);
        aout_Release(p_aout);
    }
}

- (IBAction)filterEnableKaraoke:(id)sender
{
    [_playerController enableAudioFilterWithName:@"karaoke" state:[sender state]];
}

- (IBAction)filterEnableScaleTempo:(id)sender
{
    [_playerController enableAudioFilterWithName:@"scaletempo" state:[sender state]];
}

- (IBAction)filterEnableStereoEnhancer:(id)sender
{
    [_playerController enableAudioFilterWithName:@"stereo_widen" state:[sender state]];
}

@end
