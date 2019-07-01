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
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "library/VLCInputItem.h"

#import <vlc_url.h>

#pragma mark - data storage object

@interface VLCCodecInformationTreeItem : NSObject

@property (readwrite) NSString *name;
@property (readwrite) NSString *value;

@property (readwrite) NSArray *children;

@end

@implementation VLCCodecInformationTreeItem

@end

#pragma mark - window controller

@interface VLCInformationWindowController () <NSOutlineViewDataSource>
{
    VLCCodecInformationTreeItem *_rootCodecInformationItem;

    BOOL _statisticsEnabled;
}
@end

@implementation VLCInformationWindowController

- (id)init
{
    self = [super initWithWindowNibName:@"MediaInfo"];
    if (self) {
        NSNotificationCenter *defaultNotificationCenter = [NSNotificationCenter defaultCenter];
        [defaultNotificationCenter addObserver:self
                                                 selector:@selector(currentPlaylistItemChanged:)
                                                     name:VLCPlaylistCurrentItemChanged
                                                   object:nil];
        [defaultNotificationCenter addObserver:self
                                      selector:@selector(updateStatistics:)
                                          name:VLCPlayerStatisticsUpdated
                                        object:nil];
        [defaultNotificationCenter addObserver:self
                                      selector:@selector(updateCocoaWindowLevel:)
                                          name:VLCWindowShouldUpdateLevel
                                        object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)windowDidLoad
{
    [self.window setExcludedFromWindowsMenu: YES];
    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];
    [self.window setInitialFirstResponder: _uriLabel];

    _outlineView.dataSource = self;

    [self initStrings];

    _statisticsEnabled = var_InheritBool(getIntf(), "stats");
    if (!_statisticsEnabled) {
        if ([_segmentedView segmentCount] >= 3)
            [_segmentedView setSegmentCount: 2];
    } else {
        [self initMediaPanelStats];
    }
}

- (void)initStrings
{
    [self.window setTitle: _NS("Media Information")];

    [_uriLabel setStringValue: _NS("Location")];
    [_titleLabel setStringValue: _NS("Title")];
    [_authorLabel setStringValue: _NS("Artist")];
    [_saveMetaDataButton setStringValue: _NS("Save Metadata")];

    [_segmentedView setLabel:_NS("General") forSegment:0];
    [_segmentedView setLabel:_NS("Codec Details") forSegment:1];
    [_segmentedView setLabel:_NS("Statistics") forSegment:2];

    /* constants defined in vlc_meta.h */
    [_genreLabel setStringValue: _NS(VLC_META_GENRE)];
    [_copyrightLabel setStringValue: _NS(VLC_META_COPYRIGHT)];
    [_collectionLabel setStringValue: _NS(VLC_META_ALBUM)];
    [_seqNumLabel setStringValue: _NS(VLC_META_TRACK_NUMBER)];
    [_descriptionLabel setStringValue: _NS(VLC_META_DESCRIPTION)];
    [_dateLabel setStringValue: _NS(VLC_META_DATE)];
    [_languageLabel setStringValue: _NS(VLC_META_LANGUAGE)];
    [_nowPlayingLabel setStringValue: _NS(VLC_META_NOW_PLAYING)];
    [_publisherLabel setStringValue: _NS(VLC_META_PUBLISHER)];
    [_encodedbyLabel setStringValue: _NS(VLC_META_ENCODED_BY)];

    /* statistics */
    [_inputLabel setStringValue: _NS("Input")];
    [_readBytesLabel setStringValue: _NS("Read at media")];
    [_inputBitrateLabel setStringValue: _NS("Input bitrate")];
    [_demuxBytesLabel setStringValue: _NS("Demuxed")];
    [_demuxBitrateLabel setStringValue: _NS("Stream bitrate")];

    [_videoLabel setStringValue: _NS("Video")];
    [_videoDecodedLabel setStringValue: _NS("Decoded blocks")];
    [_displayedLabel setStringValue: _NS("Displayed frames")];
    [_lostFramesLabel setStringValue: _NS("Lost frames")];

    [_audioLabel setStringValue: _NS("Audio")];
    [_audioDecodedLabel setStringValue: _NS("Decoded blocks")];
    [_playedAudioBuffersLabel setStringValue: _NS("Played buffers")];
    [_lostAudioBuffersLabel setStringValue: _NS("Lost buffers")];
}

- (void)updateCocoaWindowLevel:(NSNotification *)aNotification
{
    NSInteger windowLevel = [aNotification.userInfo[VLCWindowLevelKey] integerValue];
    if (self.isWindowLoaded && [self.window isVisible] && [self.window level] != windowLevel)
        [self.window setLevel: windowLevel];
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

- (void)initMediaPanelStats
{
    //Initializing Input Variables
    [_readBytesTextField setStringValue: [NSString stringWithFormat:_NS("%.1f KiB"), (float)0]];
    [_inputBitrateTextField setStringValue: [NSString stringWithFormat:@"%6.0f kb/s", (float)0]];
    [_demuxBytesTextField setStringValue: [NSString stringWithFormat:_NS("%.1f KiB"), (float)0]];
    [_demuxBitrateTextField setStringValue: [NSString stringWithFormat:@"%6.0f kb/s", (float)0]];

    //Initializing Video Variables
    [_videoDecodedTextField setIntValue:0];
    [_displayedTextField setIntValue:0];
    [_lostFramesTextField setIntValue:0];

    //Initializing Audio Variables
    [_audioDecodedTextField setIntValue:0];
    [_playedAudioBuffersTextField setIntValue: 0];
    [_lostAudioBuffersTextField setIntValue: 0];
}

- (void)currentPlaylistItemChanged:(NSNotification *)aNotification
{
    VLCPlaylistController *playlistController = [[VLCMain sharedInstance] playlistController];
    VLCInputItem *currentMediaItem = playlistController.currentlyPlayingInputItem;
    [self setRepresentedInputItem:currentMediaItem];
}

- (void)setRepresentedInputItem:(VLCInputItem *)representedInputItem
{
    _representedInputItem = representedInputItem;

    [_saveMetaDataButton setEnabled: NO];

    if (!self.isWindowLoaded)
        return;

    if (!_representedInputItem) {
        /* Erase */
#define SET( foo ) \
[_##foo##TextField setStringValue:@""];
        SET( uri );
        SET( title );
        SET( author );
        SET( collection );
        SET( seqNum );
        SET( genre );
        SET( copyright );
        SET( publisher );
        SET( nowPlaying );
        SET( language );
        SET( date );
        SET( description );
        SET( encodedby );
#undef SET
        [_imageWell setImage: [NSImage imageNamed: @"noart.png"]];
    } else {
        if (!_representedInputItem.preparsed) {
            [_representedInputItem preparseInputItem];
        }

        _uriTextField.stringValue = _representedInputItem.MRL;
        _titleTextField.stringValue = _representedInputItem.title;
        _authorTextField.stringValue = _representedInputItem.artist;
        _collectionTextField.stringValue = _representedInputItem.albumName;
        _seqNumTextField.stringValue = _representedInputItem.trackNumber;
        _genreTextField.stringValue = _representedInputItem.genre;
        _copyrightTextField.stringValue = _representedInputItem.copyright;
        _publisherTextField.stringValue = _representedInputItem.publisher;
        _nowPlayingTextField.stringValue = _representedInputItem.nowPlaying;
        _languageTextField.stringValue = _representedInputItem.language;
        _dateTextField.stringValue = _representedInputItem.date;
        _descriptionTextField.stringValue = _representedInputItem.contentDescription;
        _encodedbyTextField.stringValue = _representedInputItem.encodedBy;

        NSURL *artworkURL = _representedInputItem.artworkURL;
        NSImage *artwork;
        if (artworkURL) {
            artwork = [[NSImage alloc] initWithContentsOfURL:_representedInputItem.artworkURL];
        }
        if (!artwork) {
            artwork = [NSImage imageNamed: @"noart.png"];
        }
        [_imageWell setImage:artwork];
    }

    /* reload the codec details table */
    [self updateStreamsList];
}

- (void)updateStatistics:(NSNotification *)aNotification
{
    if (!self.isWindowLoaded || !_statisticsEnabled)
        return;

    if (![self.window isVisible])
        return;

    VLCInputStats *inputStats = aNotification.userInfo[VLCPlayerInputStats];
    if (!inputStats) {
        [self initMediaPanelStats];
        return;
    }

    /* input */
    [_readBytesTextField setStringValue: [NSString stringWithFormat:
                                          @"%8.0f KiB", (float)(inputStats.inputReadBytes)/1024]];
    [_inputBitrateTextField setStringValue: [NSString stringWithFormat:
                                             @"%6.0f kb/s", (float)(inputStats.inputBitrate)*8000]];
    [_demuxBytesTextField setStringValue: [NSString stringWithFormat:
                                           @"%8.0f KiB", (float)(inputStats.demuxReadBytes)/1024]];
    [_demuxBitrateTextField setStringValue: [NSString stringWithFormat:
                                             @"%6.0f kb/s", (float)(inputStats.demuxBitrate)*8000]];

    /* Video */
    [_videoDecodedTextField setIntegerValue: inputStats.decodedVideo];
    [_displayedTextField setIntegerValue: inputStats.displayedPictures];
    [_lostFramesTextField setIntegerValue: inputStats.lostPictures];

    /* Audio */
    [_audioDecodedTextField setIntegerValue: inputStats.decodedAudio];
    [_playedAudioBuffersTextField setIntegerValue: inputStats.playedAudioBuffers];
    [_lostAudioBuffersTextField setIntegerValue: inputStats.lostAudioBuffers];
}

- (void)updateStreamsList
{
    _rootCodecInformationItem = [[VLCCodecInformationTreeItem alloc] init];

    if (_representedInputItem) {
        input_item_t *p_input = _representedInputItem.vlcInputItem;
        vlc_mutex_lock(&p_input->lock);
        // build list of streams
        NSMutableArray *streams = [NSMutableArray array];

        for (int i = 0; i < p_input->i_categories; i++) {
            info_category_t *cat = p_input->pp_categories[i];
            info_t *info;

            VLCCodecInformationTreeItem *subItem = [[VLCCodecInformationTreeItem alloc] init];
            subItem.name = toNSStr(cat->psz_name);

            // Build list of codec details
            NSMutableArray *infos = [NSMutableArray array];

            info_foreach(info, &cat->infos) {
                VLCCodecInformationTreeItem *infoItem = [[VLCCodecInformationTreeItem alloc] init];
                infoItem.name = toNSStr(info->psz_name);
                infoItem.value = toNSStr(info->psz_value);
                [infos addObject:infoItem];
            }

            subItem.children = [infos copy];
            [streams addObject:subItem];
        }

        _rootCodecInformationItem.children = [streams copy];
        vlc_mutex_unlock(&p_input->lock);
    }

    [_outlineView reloadData];
    [_outlineView expandItem:nil expandChildren:YES];
}

- (IBAction)metaFieldChanged:(id)sender
{
    [_saveMetaDataButton setEnabled: YES];
}

- (IBAction)saveMetaData:(id)sender
{
    if (!_representedInputItem) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:_NS("Error while saving meta")];
        [alert setInformativeText:_NS("VLC was unable to save the meta data.")];
        [alert addButtonWithTitle:_NS("OK")];
        [alert runModal];
        return;
    }

    _representedInputItem.name = _titleTextField.stringValue;
    _representedInputItem.title = _titleTextField.stringValue;
    _representedInputItem.artist = _authorTextField.stringValue;
    _representedInputItem.albumName = _collectionTextField.stringValue;
    _representedInputItem.genre = _genreTextField.stringValue;
    _representedInputItem.trackNumber = _seqNumTextField.stringValue;
    _representedInputItem.date = _dateTextField.stringValue;
    _representedInputItem.copyright = _copyrightTextField.stringValue;
    _representedInputItem.publisher = _publisherTextField.stringValue;
    _representedInputItem.contentDescription = _descriptionTextField.stringValue;
    _representedInputItem.language = _languageTextField.stringValue;

    [_representedInputItem writeMetadataToFile];
    [_saveMetaDataButton setEnabled: NO];
}

@end

@implementation VLCInformationWindowController (NSTableDataSource)

- (NSInteger)outlineView:(NSOutlineView *)outlineView
  numberOfChildrenOfItem:(id)item
{
    return (item == nil) ? [_rootCodecInformationItem children].count : [item children].count;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView
   isItemExpandable:(id)item {
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
        return [item name];
    } else {
        return [item value];
    }
}

@end
