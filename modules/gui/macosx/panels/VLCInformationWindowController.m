/*****************************************************************************
 * VLCInformationWindowController.m: Controller for the codec info panel
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
 ******************************************************************************/

#import "VLCInformationWindowController.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryModel.h"

#import "main/VLCMain.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

#import "views/VLCImageView.h"
#import "views/VLCSettingTextField.h"

#import "windows/video/VLCVideoOutputProvider.h"

#define PERFORM_ACTION_READWRITE_TEXTFIELDS(actionCallback) \
actionCallback(title);                                      \
actionCallback(artist);                                     \
actionCallback(album);                                      \
actionCallback(genre);                                      \
actionCallback(trackNumber);                                \
actionCallback(date);                                       \
actionCallback(actors);                                     \
actionCallback(director);                                   \
actionCallback(showName);                                   \
actionCallback(copyright);                                  \
actionCallback(publisher);                                  \
actionCallback(language);                                   \
actionCallback(contentDescription);

#define PERFORM_ACTION_ALL_TEXTFIELDS(actionCallback)   \
PERFORM_ACTION_READWRITE_TEXTFIELDS(actionCallback);    \
actionCallback(decodedMRL);                             \
actionCallback(trackTotal);                             \
actionCallback(season);                                 \
actionCallback(episode);                                \
actionCallback(nowPlaying);                             \
actionCallback(encodedBy);

#pragma mark - data storage object

@interface VLCCodecInformationTreeItem : NSObject

@property (readwrite) NSString *propertyName;
@property (readwrite) NSString *propertyValue;

@property (readwrite) NSArray *children;

@end

@implementation VLCCodecInformationTreeItem

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@: name: %@ value: %@ children: %lu", NSStringFromClass([self class]), self.propertyName, self.propertyValue, self.children.count];
}

@end

#pragma mark - window controller

@interface VLCInformationWindowController () <NSOutlineViewDataSource>
{
    VLCCodecInformationTreeItem *_rootCodecInformationItem;
    NSImage *_artwork;
    NSURL *_newArtworkURL;
    BOOL _statisticsEnabled;
}
@end

@implementation VLCInformationWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"VLCInformationWindow"];
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)awakeFromNib
{
    [self.window setExcludedFromWindowsMenu: YES];
    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];
    [self.window setInitialFirstResponder: _decodedMRLLabel];

    _outlineView.dataSource = self;

    NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
    if (_mainMenuInstance) {
        [notificationCenter addObserver:self
                               selector:@selector(currentPlayingItemChanged:)
                                   name:VLCPlayerCurrentMediaItemChanged
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(updateStatistics:)
                                   name:VLCPlayerStatisticsUpdated
                                 object:nil];
    }
    [notificationCenter addObserver:self
                           selector:@selector(updateCocoaWindowLevel:)
                               name:VLCWindowShouldUpdateLevel
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(mediaItemWasParsed:)
                               name:VLCInputItemParsingSucceeded
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(mediaItemWasParsed:)
                               name:VLCInputItemPreparsingSucceeded
                             object:nil];

    [notificationCenter postNotificationName:VLCPlayerStatisticsUpdated object:self];

    [self initStrings];

    _statisticsEnabled = var_InheritBool(getIntf(), "stats");
    if (!_statisticsEnabled || !_mainMenuInstance) {
        if ([_segmentedView segmentCount] >= 3)
            [_segmentedView setSegmentCount: 2];
    } else {
        [self initMediaPanelStats];
    }

#define SET_METADATA_SETTING_FIELD_DELEGATE(field)  \
_##field##TextField.delegate = self

    PERFORM_ACTION_ALL_TEXTFIELDS(SET_METADATA_SETTING_FIELD_DELEGATE)

    [self updateRepresentation];
}

- (void)initStrings
{
    [self.window setTitle: _NS("Media Information")];

    [_decodedMRLLabel setStringValue: _NS("Location")];
    [_titleLabel setStringValue: NSTR(VLC_META_TITLE)];
    [_artistLabel setStringValue: NSTR(VLC_META_ARTIST)];
    [_saveMetaDataButton setStringValue: _NS("Save Metadata")];

    [_segmentedView setLabel:_NS("General") forSegment:0];
    [_segmentedView setLabel:_NS("Codec Details") forSegment:1];
    [_segmentedView setLabel:_NS("Statistics") forSegment:2];

    /* constants defined in vlc_meta.h */
    [_genreLabel setStringValue: NSTR(VLC_META_GENRE)];
    [_copyrightLabel setStringValue: NSTR(VLC_META_COPYRIGHT)];
    [_albumLabel setStringValue: NSTR(VLC_META_ALBUM)];
    [_trackNumberLabel setStringValue: NSTR(VLC_META_TRACK_NUMBER)];
    [_trackTotalLabel setStringValue: _NS("Track Total")];
    [_contentDescriptionLabel setStringValue: NSTR(VLC_META_DESCRIPTION)];
    [_dateLabel setStringValue: NSTR(VLC_META_DATE)];
    [_languageLabel setStringValue: NSTR(VLC_META_LANGUAGE)];
    [_nowPlayingLabel setStringValue: NSTR(VLC_META_NOW_PLAYING)];
    [_publisherLabel setStringValue: NSTR(VLC_META_PUBLISHER)];
    [_encodedByLabel setStringValue: NSTR(VLC_META_ENCODED_BY)];
    [_showNameLabel setStringValue: NSTR(VLC_META_SHOW_NAME)];
    [_episodeLabel setStringValue: NSTR(VLC_META_EPISODE)];
    [_seasonLabel setStringValue: NSTR(VLC_META_SEASON)];
    [_actorsLabel setStringValue: NSTR(VLC_META_ACTORS)];
    [_directorLabel setStringValue: NSTR(VLC_META_DIRECTOR)];

    /* statistics */
    [_inputLabel setStringValue: _NS("Input")];
    [_inputReadBytesLabel setStringValue: _NS("Read at media")];
    [_inputBitrateLabel setStringValue: _NS("Input bitrate")];
    [_inputReadPacketsLabel setStringValue: _NS("Read packets")];
    [_demuxReadBytesLabel setStringValue: _NS("Demuxed")];
    [_demuxReadPacketsLabel setStringValue: _NS("Demuxed packets")];
    [_demuxBitrateLabel setStringValue: _NS("Stream bitrate")];
    [_demuxCorruptedLabel setStringValue: _NS("Corrupted")];
    [_demuxDiscontinuitiesLabel setStringValue: _NS("Discontinuities")];

    [_videoLabel setStringValue: _NS("Video")];
    [_videoDecodedLabel setStringValue: _NS("Decoded blocks")];
    [_displayedLabel setStringValue: _NS("Displayed frames")];
    [_lostFramesLabel setStringValue: _NS("Lost frames")];
    [_lateFramesLabel setStringValue: _NS("Late frames")];

    [_audioLabel setStringValue: _NS("Audio")];
    [_audioDecodedLabel setStringValue: _NS("Decoded blocks")];
    [_playedAudioBuffersLabel setStringValue: _NS("Played buffers")];
    [_lostAudioBuffersLabel setStringValue: _NS("Lost buffers")];
}

- (void)updateCocoaWindowLevel:(NSNotification *)aNotification
{
    NSInteger windowLevel = [aNotification.userInfo[VLCWindowLevelKey] integerValue];
    if ([self.window isVisible] && [self.window level] != windowLevel)
        [self.window setLevel: windowLevel];
}

- (IBAction)toggleWindow:(id)sender
{
    if ([self.window isKeyWindow])
        [self.window orderOut:sender];
    else {
        [self.window setLevel: VLCMain.sharedInstance.voutProvider.currentStatusWindowLevel];
        [self.window makeKeyAndOrderFront:sender];
    }
}

- (void)initMediaPanelStats
{
    //Initializing Input Variables
    [_inputReadBytesTextField setStringValue: [NSString stringWithFormat:_NS("%.1f KiB"), (float)0]];
    [_inputReadPacketsTextField setIntValue: 0];
    [_inputBitrateTextField setStringValue: [NSString stringWithFormat:@"%6.0f kb/s", (float)0]];
    [_demuxReadBytesTextField setStringValue: [NSString stringWithFormat:_NS("%.1f KiB"), (float)0]];
    [_demuxReadPacketsTextField setIntValue: 0];
    [_demuxBitrateTextField setStringValue: [NSString stringWithFormat:@"%6.0f kb/s", (float)0]];
    [_demuxDiscontinuitiesTextField setIntValue: 0];
    [_demuxCorruptedTextField setIntValue: 0];

    //Initializing Video Variables
    [_videoDecodedTextField setIntValue:0];
    [_displayedTextField setIntValue:0];
    [_lateFramesTextField setIntValue:0];
    [_lostFramesTextField setIntValue:0];

    //Initializing Audio Variables
    [_audioDecodedTextField setIntValue:0];
    [_playedAudioBuffersTextField setIntValue: 0];
    [_lostAudioBuffersTextField setIntValue: 0];
}

- (void)currentPlayingItemChanged:(NSNotification *)aNotification
{
    VLCPlayerController * const playerController = VLCMain.sharedInstance.playlistController.playerController;
    VLCInputItem * const currentMediaItem = playerController.currentMedia;
    [self setRepresentedInputItem:currentMediaItem];
}

- (void)setRepresentedInputItem:(VLCInputItem *)representedInputItem
{
    _representedInputItems = (representedInputItem == nil) ? @[] : @[representedInputItem];
    _artwork = [VLCLibraryImageCache thumbnailForInputItem:representedInputItem];
    [self updateRepresentation];
}

- (void)setRepresentedMediaLibraryAudioGroup:(id<VLCMediaLibraryAudioGroupProtocol>)representedMediaLibraryAudioGroup
{
    NSMutableArray<VLCInputItem *> * const inputItems = NSMutableArray.array;
    for (VLCMediaLibraryMediaItem * const mediaItem in representedMediaLibraryAudioGroup.mediaItems) {
        [inputItems addObject:mediaItem.inputItem];
    }

    _representedInputItems = [inputItems copy];

    // HACK: Input items retrieved via an audio group do not acquire an artwork URL.
    // To show something in the information window, set the small artwork from the audio group.
    _artwork = [VLCLibraryImageCache thumbnailForLibraryItem:representedMediaLibraryAudioGroup];

    [self updateRepresentation];
}

- (void)mediaItemWasParsed:(NSNotification *)aNotification
{
    [self updateRepresentation];
}

- (void)updateStatistics:(NSNotification *)aNotification
{
    if (!_statisticsEnabled)
        return;

    if (![self.window isVisible])
        return;

    VLCInputStats *inputStats = aNotification.userInfo[VLCPlayerInputStats];
    if (!inputStats) {
        [self initMediaPanelStats];
        return;
    }

    /* input */
    [_inputReadBytesTextField setStringValue: [NSString stringWithFormat:
                                          @"%8.0f KiB", (float)(inputStats.inputReadBytes)/1024]];
    [_inputReadPacketsTextField setIntegerValue:inputStats.inputReadPackets];
    [_inputBitrateTextField setStringValue: [NSString stringWithFormat:
                                             @"%6.0f kb/s", (float)(inputStats.inputBitrate)*8000]];
    [_demuxReadBytesTextField setStringValue: [NSString stringWithFormat:
                                           @"%8.0f KiB", (float)(inputStats.demuxReadBytes)/1024]];
    [_demuxReadPacketsTextField setIntegerValue:inputStats.demuxReadPackets];
    [_demuxBitrateTextField setStringValue: [NSString stringWithFormat:
                                             @"%6.0f kb/s", (float)(inputStats.demuxBitrate)*8000]];
    [_demuxCorruptedTextField setIntegerValue:inputStats.demuxCorrupted];
    [_demuxDiscontinuitiesTextField setIntegerValue:inputStats.demuxDiscontinuity];

    /* Video */
    [_videoDecodedTextField setIntegerValue: inputStats.decodedVideo];
    [_displayedTextField setIntegerValue: inputStats.displayedPictures];
    [_lateFramesTextField setIntegerValue: inputStats.latePictures];
    [_lostFramesTextField setIntegerValue: inputStats.lostPictures];

    /* Audio */
    [_audioDecodedTextField setIntegerValue: inputStats.decodedAudio];
    [_playedAudioBuffersTextField setIntegerValue: inputStats.playedAudioBuffers];
    [_lostAudioBuffersTextField setIntegerValue: inputStats.lostAudioBuffers];
}

- (void)fillWindowWithInputItemData:(VLCInputItem *)inputItem
{
    NSParameterAssert(inputItem != nil);
    if (!inputItem.preparsed) {
        [inputItem preparseInputItem];
    }

#define FILL_FIELD_FROM_INPUTITEM(field)                            \
    {                                                               \
    NSString * const inputItemString = inputItem.field;             \
    if (inputItemString != nil) {                                   \
        _##field##TextField.originalStateString = inputItemString;  \
    } else {                                                        \
        _##field##TextField.originalStateString = @"";              \
    }                                                               \
}

    PERFORM_ACTION_ALL_TEXTFIELDS(FILL_FIELD_FROM_INPUTITEM);

#undef FILL_FIELD_FROM_INPUTITEM

    _artworkImageButton.image = _artwork;

    if (!_mainMenuInstance) {
        [self.window setTitle:inputItem.title];
    }
}

- (void)fillWindowWithDictionaryData:(NSDictionary<NSString *, id> *)dict
{
    NSParameterAssert(dict != nil);

#define FILL_FIELD_FROM_DICT(field)                                         \
{                                                                           \
    NSString * const dictKey = [NSString stringWithUTF8String:#field];      \
    NSString * const fieldValue = [dict objectForKey:dictKey];              \
                                                                            \
    if (fieldValue != nil) {                                                \
        _##field##TextField.originalStateString = fieldValue;               \
    } else {                                                                \
        _##field##TextField.originalStateString = @"";                      \
    }                                                                       \
}                                                                           \

    PERFORM_ACTION_ALL_TEXTFIELDS(FILL_FIELD_FROM_DICT);

#undef FILL_FIELD_FROM_DICT

    _artworkImageButton.image = _artwork;
}

- (void)updateRepresentation
{
    _saveMetaDataButton.enabled = NO;
    _newArtworkURL = nil;
    _artworkImageButton.image = _artwork;
    _artworkImageButton.alternateImage = _artwork;

    if (_representedInputItems.count == 0) {
        /* Erase */
#define CLEAR_TEXT(field) \
_##field##TextField.originalStateString = @"";

        PERFORM_ACTION_ALL_TEXTFIELDS(CLEAR_TEXT);

#undef CLEAR_TEXT
        _artworkImageButton.image = [NSImage imageNamed:@"noart.png"];
    } else if (_representedInputItems.count == 1) {
        [self fillWindowWithInputItemData:_representedInputItems.firstObject];
    } else if (_representedInputItems.count > 1) {
        NSDictionary * const commonItemsData = commonInputItemData(_representedInputItems);

        if ([commonItemsData objectForKey:@"inputItem"]) {
            [self setRepresentedInputItem:[commonItemsData objectForKey:@"inputItem"]];
        } else {
            [self fillWindowWithDictionaryData:commonItemsData];
        }
    }

    /* reload the codec details table */
    [self updateStreamsList];
}

- (void)updateStreamsForInputItems:(NSArray<VLCInputItem *> *)inputItems
{
    NSParameterAssert(inputItems != nil);

    for (VLCInputItem * const inputItem in inputItems) {
        input_item_t * const p_input = inputItem.vlcInputItem;
        vlc_mutex_lock(&p_input->lock);
        // build list of streams
        NSMutableArray * const streams = NSMutableArray.array;

        info_category_t *cat;
        vlc_list_foreach(cat, &p_input->categories, node) {
            if (info_category_IsHidden(cat)) {
                continue;
            }

            VLCCodecInformationTreeItem * const subItem = [[VLCCodecInformationTreeItem alloc] init];
            subItem.propertyName = toNSStr(cat->psz_name);

            // Build list of codec details
            NSMutableArray * const infos = NSMutableArray.array;

            info_t *info;
            info_foreach(info, &cat->infos) {
                VLCCodecInformationTreeItem * const infoItem = [[VLCCodecInformationTreeItem alloc] init];
                infoItem.propertyName = toNSStr(info->psz_name);
                infoItem.propertyValue = toNSStr(info->psz_value);
                [infos addObject:infoItem];
            }

            subItem.children = [infos copy];
            [streams addObject:subItem];
        }

        _rootCodecInformationItem.children = [streams copy];
        vlc_mutex_unlock(&p_input->lock);
    }
}

- (void)updateStreamsList
{
    _rootCodecInformationItem = [[VLCCodecInformationTreeItem alloc] init];

    if (_representedInputItems.count > 0) {
        [self updateStreamsForInputItems:_representedInputItems];
    }

    [_outlineView reloadData];
    [_outlineView expandItem:nil expandChildren:YES];
}

- (void)controlTextDidChange:(NSNotification *)notification
{
    BOOL settingsChanged = NO;

#define CHECK_FIELD_SETTING_CHANGED(field)                              \
settingsChanged = settingsChanged || _##field##TextField.settingChanged;

    PERFORM_ACTION_ALL_TEXTFIELDS(CHECK_FIELD_SETTING_CHANGED);

#undef CHECK_FIELD_SETTING_CHANGED

    settingsChanged = settingsChanged || _newArtworkURL != nil;

    _saveMetaDataButton.enabled = settingsChanged;
}

- (void)reloadMediaLibraryFoldersForInputItems:(NSArray<VLCInputItem *> *)inputItems
{
    VLCLibraryController * const libraryController = VLCMain.sharedInstance.libraryController;
    NSArray<VLCMediaLibraryEntryPoint *> * const entryPoints = libraryController.libraryModel.listOfMonitoredFolders;
    NSMutableSet<NSString *> * const reloadMRLs = NSMutableSet.set;
    NSMutableSet<VLCInputItem *> * const checkedInputItems = NSMutableSet.set;

    for (VLCMediaLibraryEntryPoint * const entryPoint in entryPoints) {
        for (VLCInputItem * const inputItem in inputItems) {
            if ([checkedInputItems containsObject:inputItem]) {
                continue;
            }

            if ([inputItem.MRL hasPrefix:entryPoint.MRL]) {
                [reloadMRLs addObject:entryPoint.MRL];
                [checkedInputItems addObject:inputItem];
                break;
            }
        }
    }

    for (NSString * const entryPointMRL in reloadMRLs) {
        NSURL * const entryPointURL = [NSURL URLWithString:entryPointMRL];
        [libraryController reloadFolderWithFileURL:entryPointURL];
    }
}

- (void)saveInputItemsMetadata:(NSArray<VLCInputItem *> *)inputItems
{
    NSParameterAssert(inputItems);

#define SET_INPUTITEM_PROP(field, prop)                          \
{                                                                \
    NSString * const value = _##field##TextField.stringValue;    \
    if (_##field##TextField.settingChanged) {                    \
        inputItem.prop = value;                                  \
    }                                                            \
}

#define SET_INPUTITEM_MATCHING_PROP(field)      \
SET_INPUTITEM_PROP(field, field)                \

    for (VLCInputItem * const inputItem in inputItems) {
        SET_INPUTITEM_PROP(title, name); // Input items do not have a title field
        PERFORM_ACTION_READWRITE_TEXTFIELDS(SET_INPUTITEM_MATCHING_PROP);

        if (_newArtworkURL != nil) { // Artwork urls require their own handling
            inputItem.artworkURL = _newArtworkURL;
        }

        [inputItem writeMetadataToFile];
    }

#undef SET_INPUTITEM_MATCHING_PROP
#undef SET_INPUTITEM_PROP

    [self updateRepresentation];
    [self reloadMediaLibraryFoldersForInputItems:inputItems];
}

- (IBAction)saveMetaData:(id)sender
{
    if (_representedInputItems.count > 0) {
        [self saveInputItemsMetadata:_representedInputItems];
    } else {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:_NS("Error while saving meta")];
        [alert setInformativeText:_NS("VLC was unable to save the meta data.")];
        [alert addButtonWithTitle:_NS("OK")];
        [alert runModal];
    }
}

- (IBAction)chooseArtwork:(id)sender
{
    NSOpenPanel * const panel = [NSOpenPanel openPanel];
    [panel setAllowedFileTypes:@[@"png", @"jpg", @"jpeg", @"gif", @"tiff", @"tif", @"bmp"]];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];
    [panel setCanCreateDirectories:NO];
    [panel setResolvesAliases:YES];
    [panel setAllowsOtherFileTypes:NO];
    [panel setPrompt:_NS("Choose")];
    [panel beginSheetModalForWindow:self.window completionHandler:^(const NSInteger result) {
        if (result != NSFileHandlingPanelOKButton) {
            return;
        }

        NSURL * const url = panel.URLs.firstObject;
        NSImage * const image = [[NSImage alloc] initWithContentsOfURL:url];
        if (!image) {
            return;
        }

        _artwork = image;
        _artworkImageButton.image = [[NSImage alloc] initWithContentsOfURL:url];

        _newArtworkURL = url;
        _saveMetaDataButton.enabled = YES;
    }];
}

@end

@implementation VLCInformationWindowController (NSTableDataSource)

- (NSInteger)outlineView:(NSOutlineView *)outlineView
  numberOfChildrenOfItem:(id)item
{
    return (item == nil) ? [_rootCodecInformationItem children].count : [item children].count;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView
   isItemExpandable:(id)item
{
    return ([item children].count > 0);
}

- (id)outlineView:(NSOutlineView *)outlineView
            child:(NSInteger)index
           ofItem:(id)item
{
    return (item == nil) ? [_rootCodecInformationItem children][index] : [item children][index];
}

- (id)outlineView:(NSOutlineView *)outlineView
objectValueForTableColumn:(NSTableColumn *)tableColumn
           byItem:(id)item
{
    if (!item)
        return @"";

    if ([[tableColumn identifier] isEqualToString:@"0"]) {
        return [item propertyName];
    } else {
        return [item propertyValue];
    }
}

@end
