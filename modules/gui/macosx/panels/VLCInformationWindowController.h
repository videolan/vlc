/*****************************************************************************
 * VLCInformationWindowController.h: Controller for the codec info panel
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
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

#import <Cocoa/Cocoa.h>

@class VLCInputItem;
@class VLCImageView;
@class VLCLibraryRepresentedItem;
@class VLCSettingTextField;
@protocol VLCMediaLibraryAudioGroupProtocol;

@interface VLCInformationWindowController : NSWindowController<NSTextFieldDelegate>

@property (readwrite, weak) IBOutlet NSOutlineView *outlineView;
@property (readwrite, weak) IBOutlet NSSegmentedControl *segmentedView;

@property (readwrite, weak) IBOutlet NSButton *mrlCopyButton;
@property (readwrite, weak) IBOutlet NSTextField *decodedMRLLabel;
@property (readwrite, weak) IBOutlet NSTextField *titleLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *decodedMRLTextField;
@property (readwrite, weak) IBOutlet VLCSettingTextField *titleTextField;
@property (readwrite, weak) IBOutlet NSTextField *artistLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *artistTextField;
@property (readwrite, weak) IBOutlet NSTextField *albumLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *albumTextField;
@property (readwrite, weak) IBOutlet NSTextField *copyrightLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *copyrightTextField;
@property (readwrite, weak) IBOutlet NSTextField *dateLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *dateTextField;
@property (readwrite, weak) IBOutlet NSTextField *contentDescriptionLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *contentDescriptionTextField;
@property (readwrite, weak) IBOutlet NSTextField *encodedByLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *encodedByTextField;
@property (readwrite, weak) IBOutlet NSTextField *genreLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *genreTextField;
@property (readwrite, weak) IBOutlet NSTextField *languageLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *languageTextField;
@property (readwrite, weak) IBOutlet NSTextField *nowPlayingLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *nowPlayingTextField;
@property (readwrite, weak) IBOutlet NSTextField *publisherLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *publisherTextField;
@property (readwrite, weak) IBOutlet NSTextField *trackNumberLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *trackNumberTextField;
@property (readwrite, weak) IBOutlet NSTextField *trackTotalLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *trackTotalTextField;
@property (readwrite, weak) IBOutlet NSTextField *showNameLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *showNameTextField;
@property (readwrite, weak) IBOutlet NSTextField *seasonLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *seasonTextField;
@property (readwrite, weak) IBOutlet NSTextField *episodeLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *episodeTextField;
@property (readwrite, weak) IBOutlet NSTextField *actorsLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *actorsTextField;
@property (readwrite, weak) IBOutlet NSTextField *directorLabel;
@property (readwrite, weak) IBOutlet VLCSettingTextField *directorTextField;
@property (readwrite, weak) IBOutlet NSButton *artworkImageButton;
@property (readwrite, weak) IBOutlet NSButton *saveMetaDataButton;

@property (readwrite, weak) IBOutlet NSTextField *audioLabel;
@property (readwrite, weak) IBOutlet NSTextField *audioDecodedLabel;
@property (readwrite, weak) IBOutlet NSTextField *audioDecodedTextField;
@property (readwrite, weak) IBOutlet NSTextField *playedAudioBuffersLabel;
@property (readwrite, weak) IBOutlet NSTextField *playedAudioBuffersTextField;
@property (readwrite, weak) IBOutlet NSTextField *lostAudioBuffersLabel;
@property (readwrite, weak) IBOutlet NSTextField *lostAudioBuffersTextField;
@property (readwrite, weak) IBOutlet NSTextField *videoLabel;
@property (readwrite, weak) IBOutlet NSTextField *videoDecodedLabel;
@property (readwrite, weak) IBOutlet NSTextField *videoDecodedTextField;
@property (readwrite, weak) IBOutlet NSTextField *displayedLabel;
@property (readwrite, weak) IBOutlet NSTextField *displayedTextField;
@property (readwrite, weak) IBOutlet NSTextField *lateFramesLabel;
@property (readwrite, weak) IBOutlet NSTextField *lateFramesTextField;
@property (readwrite, weak) IBOutlet NSTextField *lostFramesLabel;
@property (readwrite, weak) IBOutlet NSTextField *lostFramesTextField;
@property (readwrite, weak) IBOutlet NSTextField *inputLabel;
@property (readwrite, weak) IBOutlet NSTextField *inputReadBytesLabel;
@property (readwrite, weak) IBOutlet NSTextField *inputReadBytesTextField;
@property (readwrite, weak) IBOutlet NSTextField *inputBitrateLabel;
@property (readwrite, weak) IBOutlet NSTextField *inputBitrateTextField;
@property (readwrite, weak) IBOutlet NSTextField *inputReadPacketsLabel;
@property (readwrite, weak) IBOutlet NSTextField *inputReadPacketsTextField;
@property (readwrite, weak) IBOutlet NSTextField *demuxReadBytesLabel;
@property (readwrite, weak) IBOutlet NSTextField *demuxReadBytesTextField;
@property (readwrite, weak) IBOutlet NSTextField *demuxBitrateLabel;
@property (readwrite, weak) IBOutlet NSTextField *demuxBitrateTextField;
@property (readwrite, weak) IBOutlet NSTextField *demuxReadPacketsLabel;
@property (readwrite, weak) IBOutlet NSTextField *demuxReadPacketsTextField;
@property (readwrite, weak) IBOutlet NSTextField *demuxCorruptedLabel;
@property (readwrite, weak) IBOutlet NSTextField *demuxCorruptedTextField;
@property (readwrite, weak) IBOutlet NSTextField *demuxDiscontinuitiesLabel;
@property (readwrite, weak) IBOutlet NSTextField *demuxDiscontinuitiesTextField;

@property (readwrite, strong, nonatomic) NSArray<VLCInputItem *> *representedInputItems;
@property (readwrite) BOOL mainMenuInstance;

- (IBAction)toggleWindow:(id)sender;
- (IBAction)copyMrl:(id)sender;
- (IBAction)saveMetaData:(id)sender;
- (IBAction)chooseArtwork:(id)sender;

- (void)setRepresentedMediaLibraryItems:(NSArray<VLCLibraryRepresentedItem *> *)representedMediaLibraryItems;

@end
