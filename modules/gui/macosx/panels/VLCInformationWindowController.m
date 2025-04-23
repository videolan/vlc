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

#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSTextField+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayerController.h"

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

@interface VLCInformationWindowController () <NSOutlineViewDataSource, NSOutlineViewDelegate>
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
    _statisticsEnabled = var_InheritBool(getIntf(), "stats");

    [self.window setExcludedFromWindowsMenu: YES];
    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];
    [self.window setInitialFirstResponder: _decodedMRLLabel];

    self.outlineView.dataSource = self;
    self.outlineView.delegate = self;
    self.outlineView.tableColumns.lastObject.resizingMask = NSTableColumnAutoresizingMask;

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    if (_mainMenuInstance && _statisticsEnabled) {
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

    [self initStrings];

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

- (IBAction)copyMrl:(id)sender
{
    if (self.representedInputItems.count == 0) {
        return;
    } else if (self.representedInputItems.count == 1) {
        NSPasteboard * const pasteboard = NSPasteboard.generalPasteboard;
        [pasteboard clearContents];
        [pasteboard setString:self.representedInputItems.firstObject.MRL
                      forType:NSPasteboardTypeString];
    } else {
        NSMenu * const choiceMenu = [[NSMenu alloc] initWithTitle:_NS("Copy MRL")];
        for (VLCInputItem * const inputItem in self.representedInputItems) {
            NSMenuItem * const menuItem =
                [[NSMenuItem alloc] initWithTitle:inputItem.title
                                           action:@selector(copyMrlFromMenuItem:)
                                    keyEquivalent:@""];
            menuItem.representedObject = inputItem;
            [choiceMenu addItem:menuItem];
        }
        CGFloat senderHeight = 0;
        if ([sender isKindOfClass:NSView.class]) {
            senderHeight = ((NSView *)sender).frame.size.height;
        }
        [choiceMenu popUpMenuPositioningItem:nil
                                  atLocation:NSMakePoint(0, senderHeight)
                                      inView:sender];
    }
}

- (void)copyMrlFromMenuItem:(id)sender
{
    NSParameterAssert(sender);
    NSParameterAssert([sender isKindOfClass:NSMenuItem.class]);
    NSMenuItem * const menuItem = (NSMenuItem *)sender;
    NSParameterAssert(menuItem.representedObject);
    NSParameterAssert([menuItem.representedObject isKindOfClass:VLCInputItem.class]);
    VLCInputItem * const inputItem = (VLCInputItem *)menuItem.representedObject;
    NSPasteboard * const pasteboard = NSPasteboard.generalPasteboard;
    [pasteboard clearContents];
    [pasteboard setString:inputItem.MRL forType:NSPasteboardTypeString];
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

- (void)setRepresentedMediaLibraryItems:(NSArray<VLCLibraryRepresentedItem *> *)representedMediaLibraryItems
{
    NSMutableArray<VLCInputItem *> * const inputItems = NSMutableArray.array;
    for (VLCLibraryRepresentedItem * const representedItem in representedMediaLibraryItems) {
        NSArray<VLCMediaLibraryMediaItem *> * const mediaItems = representedItem.item.mediaItems;
        for (VLCMediaLibraryMediaItem * const mediaItem in mediaItems) {
            [inputItems addObject:mediaItem.inputItem];
        }
    }

    NSParameterAssert(inputItems.count > 0);
    NSArray<VLCInputItem *> * const nonMutableInputItems = inputItems.copy;
    _representedInputItems = nonMutableInputItems;

    NSMutableSet * const artworkMrlSet = NSMutableSet.set;

    __weak typeof(self) weakSelf = self;
    const dispatch_queue_t queue = dispatch_queue_create("vlc_infowindow_libraryitemimg_queue", 0);
    dispatch_async(queue, ^{
        NSMutableArray<NSImage *> * const artworkImages = NSMutableArray.array;
        dispatch_group_t group = dispatch_group_create();

        for (VLCLibraryRepresentedItem * const item in representedMediaLibraryItems) {
            NSArray<VLCMediaLibraryMediaItem *> * const mediaItems = item.item.mediaItems;

            for (VLCMediaLibraryMediaItem * const mediaItem in mediaItems) {
                @synchronized (artworkMrlSet) {
                    NSString * const itemArtworkMrl = mediaItem.smallArtworkMRL;
                    if ([artworkMrlSet containsObject:itemArtworkMrl] || itemArtworkMrl == nil) {
                        continue;
                    }
                    [artworkMrlSet addObject:itemArtworkMrl];
                }

                @synchronized (artworkImages) {
                    dispatch_group_enter(group);
                    [VLCLibraryImageCache thumbnailForLibraryItem:mediaItem
                                                   withCompletion:^(NSImage * const image) {
                        if (!weakSelf || nonMutableInputItems != weakSelf.representedInputItems) {
                            dispatch_group_leave(group);
                            return;
                        }
                        if (image) {
                            [artworkImages addObject:image];
                        }
                        dispatch_group_leave(group);
                    }];
                }
            }
        }

        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

        if (artworkImages.count == 0) {
            dispatch_async(dispatch_get_main_queue(), ^{
                _artwork = [NSImage imageNamed:@"noart.png"];
                [self updateRepresentation];
            });
            return;
        }

        // Without an image set the button's size can be {{0, 0}, {0, 0}}
        const CGFloat buttonHeight = self.artworkImageButton.frame.size.height;
        const NSSize artworkSize =
            buttonHeight == 0 ? NSMakeSize(256, 256) : NSMakeSize(buttonHeight, buttonHeight);
        NSArray<NSValue *> * const frames =
            [NSImage framesForCompositeImageSquareGridWithImages:artworkImages
                                                            size:artworkSize
                                                   gridItemCount:artworkImages.count];

        dispatch_async(dispatch_get_main_queue(), ^{
            _artwork = [NSImage compositeImageWithImages:artworkImages
                                                frames:frames
                                                    size:artworkSize];

            [self updateRepresentation];
        });
    });
}

- (void)setRepresentedInputItems:(NSArray<VLCInputItem *> *)representedInputItems
{
    if (representedInputItems == _representedInputItems) {
        return;
    }

    NSParameterAssert(representedInputItems.count > 0);
    _representedInputItems = representedInputItems;

    __weak typeof(self) weakSelf = self;
    const dispatch_queue_t queue = dispatch_queue_create("vlc_infowindow_inputitemimg_queue", 0);
    dispatch_async(queue, ^{
        NSMutableArray<NSImage *> * const artworkImages = NSMutableArray.array;
        dispatch_group_t group = dispatch_group_create();

        for (VLCInputItem * const item in representedInputItems) {
            @synchronized (artworkImages) {
                dispatch_group_enter(group);
                [VLCLibraryImageCache thumbnailForInputItem:item
                                             withCompletion:^(NSImage * const image) {
                    if (!weakSelf || representedInputItems != weakSelf.representedInputItems) {
                        dispatch_group_leave(group);
                        return;
                    }
                    if (image) {
                        [artworkImages addObject:image];
                    }
                    dispatch_group_leave(group);
                }];
            }
        }

        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

        if (artworkImages.count == 0) {
            dispatch_async(dispatch_get_main_queue(), ^{
                _artwork = [NSImage imageNamed:@"noart.png"];
                [self updateRepresentation];
            });
            return;
        }

        // Without an image set the button's size can be {{0, 0}, {0, 0}}
        const CGFloat buttonHeight = self.artworkImageButton.frame.size.height;
        const NSSize artworkSize =
            buttonHeight == 0 ? NSMakeSize(256, 256) : NSMakeSize(buttonHeight, buttonHeight);
        NSArray<NSValue *> * const frames =
            [NSImage framesForCompositeImageSquareGridWithImages:artworkImages
                                                            size:artworkSize
                                                   gridItemCount:artworkImages.count];

        dispatch_async(dispatch_get_main_queue(), ^{
            _artwork = [NSImage compositeImageWithImages:artworkImages
                                                frames:frames
                                                    size:artworkSize];
            [self updateRepresentation];
        });
    });
}

- (void)mediaItemWasParsed:(NSNotification *)aNotification
{
    [self updateRepresentation];
}

- (void)updateStatistics:(NSNotification *)aNotification
{
    NSAssert(self.representedInputItems.count == 1, @"Should not be updating stats for many items");
    VLCPlayerController * const playerController =
        VLCMain.sharedInstance.playQueueController.playerController;
    VLCInputItem * const currentPlayingItem = playerController.currentMedia;
    VLCInputItem * const firstItem = self.representedInputItems.firstObject;
    
    if (self.mainMenuInstance && ![currentPlayingItem.MRL isEqualToString:firstItem.MRL]) {
        self.representedInputItems = @[currentPlayingItem];
    }

    NSAssert(_statisticsEnabled, @"Statistics should not be updated when they are disabled!");
    VLCInputStats * const inputStats = aNotification.userInfo[VLCPlayerInputStats];
    NSAssert(inputStats != nil, @"inputStats received for statistics update should not be nil!");

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
        [inputItem parseInputItem];
    }

#define FILL_FIELD_FROM_INPUTITEM(field)                                                    \
_##field##TextField.originalStateString = inputItem.field == nil ? @"" : inputItem.field;

    PERFORM_ACTION_ALL_TEXTFIELDS(FILL_FIELD_FROM_INPUTITEM);

#undef FILL_FIELD_FROM_INPUTITEM

    _artworkImageButton.image = _artwork;

    if (!_mainMenuInstance) {
        self.window.title = inputItem.title;
    }
}

- (void)fillWindowWithDictionaryData:(NSDictionary<NSString *, id> *)dict
{
    NSParameterAssert(dict != nil);

#define FILL_FIELD_FROM_DICT(field)                                                 \
{                                                                                   \
    NSString * const dictKey = [NSString stringWithUTF8String:#field];              \
    NSString * const fieldValue = [dict objectForKey:dictKey];                      \
                                                                                    \
    if ([fieldValue isEqualToString:VLCInputItemCommonDataDifferingFlagString]) {   \
        _##field##TextField.placeholderString = _NS("(Multiple values)");           \
        _##field##TextField.originalStateString = @"";                              \
    } else if (fieldValue != nil) {                                                 \
        _##field##TextField.placeholderString = @"";                                \
        _##field##TextField.originalStateString = fieldValue;                       \
    } else {                                                                        \
        _##field##TextField.placeholderString = @"";                                \
        _##field##TextField.originalStateString = @"";                              \
    }                                                                               \
}

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
            self.representedInputItems = @[[commonItemsData objectForKey:@"inputItem"]];
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

    [self.outlineView reloadData];
    [self.outlineView expandItem:nil expandChildren:YES];
    [self.outlineView sizeLastColumnToFit];
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
    [libraryController reloadMediaLibraryFoldersForInputItems:inputItems];
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
        // We do not have a textfield for names; set the contents of the title field to the input
        // item's name
        SET_INPUTITEM_PROP(title, name);
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

@implementation VLCInformationWindowController (NSOutlineViewDataSource)

- (NSInteger)outlineView:(NSOutlineView *)outlineView
  numberOfChildrenOfItem:(id)item
{
    return (item == nil) ? _rootCodecInformationItem.children.count : [item children].count;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView
   isItemExpandable:(id)item
{
    return [item children].count > 0;
}

- (id)outlineView:(NSOutlineView *)outlineView
            child:(NSInteger)index
           ofItem:(id)item
{
    return (item == nil) ? _rootCodecInformationItem.children[index] : [item children][index];
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

@implementation VLCInformationWindowController (NSOutlineViewDelegate)

- (NSView *)outlineView:(NSTableView *)outlineView
     viewForTableColumn:(nullable NSTableColumn *)tableColumn
                   item:(nonnull id)item
{
    NSTextField * const cellView = [NSTextField defaultLabelWithString:@""];
    cellView.objectValue =
        [self outlineView:self.outlineView objectValueForTableColumn:tableColumn byItem:item];
    return cellView;
}

@end
