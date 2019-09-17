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
#import "views/VLCImageView.h"

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
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)awakeFromNib
{
    [self.window setExcludedFromWindowsMenu: YES];
    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];
    [self.window setInitialFirstResponder: _uriLabel];

    _outlineView.dataSource = self;

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    if (_mainMenuInstance) {
        [notificationCenter addObserver:self
                               selector:@selector(currentPlaylistItemChanged:)
                                   name:VLCPlaylistCurrentItemChanged
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

    [self updateRepresentation];
}

- (void)initStrings
{
    [self.window setTitle: _NS("Media Information")];

    [_uriLabel setStringValue: _NS("Location")];
    [_titleLabel setStringValue: _NS(VLC_META_TITLE)];
    [_artistLabel setStringValue: _NS(VLC_META_ARTIST)];
    [_saveMetaDataButton setStringValue: _NS("Save Metadata")];

    [_segmentedView setLabel:_NS("General") forSegment:0];
    [_segmentedView setLabel:_NS("Codec Details") forSegment:1];
    [_segmentedView setLabel:_NS("Statistics") forSegment:2];

    /* constants defined in vlc_meta.h */
    [_genreLabel setStringValue: _NS(VLC_META_GENRE)];
    [_copyrightLabel setStringValue: _NS(VLC_META_COPYRIGHT)];
    [_albumLabel setStringValue: _NS(VLC_META_ALBUM)];
    [_trackNumberLabel setStringValue: _NS(VLC_META_TRACK_NUMBER)];
    [_trackTotalLabel setStringValue: _NS("Track Total")];
    [_descriptionLabel setStringValue: _NS(VLC_META_DESCRIPTION)];
    [_dateLabel setStringValue: _NS(VLC_META_DATE)];
    [_languageLabel setStringValue: _NS(VLC_META_LANGUAGE)];
    [_nowPlayingLabel setStringValue: _NS(VLC_META_NOW_PLAYING)];
    [_publisherLabel setStringValue: _NS(VLC_META_PUBLISHER)];
    [_encodedbyLabel setStringValue: _NS(VLC_META_ENCODED_BY)];
    [_showNameLabel setStringValue: _NS(VLC_META_SHOW_NAME)];
    [_episodeLabel setStringValue: _NS(VLC_META_EPISODE)];
    [_seasonLabel setStringValue: _NS(VLC_META_SEASON)];
    [_actorsLabel setStringValue: _NS(VLC_META_ACTORS)];
    [_directorLabel setStringValue: _NS(VLC_META_DIRECTOR)];

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
        [self.window setLevel: [[[VLCMain sharedInstance] voutProvider] currentStatusWindowLevel]];
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
    [_lostFramesTextField setIntegerValue: inputStats.lostPictures];

    /* Audio */
    [_audioDecodedTextField setIntegerValue: inputStats.decodedAudio];
    [_playedAudioBuffersTextField setIntegerValue: inputStats.playedAudioBuffers];
    [_lostAudioBuffersTextField setIntegerValue: inputStats.lostAudioBuffers];
}

- (void)updateRepresentation
{
    [_saveMetaDataButton setEnabled: NO];

    if (!_representedInputItem) {
        /* Erase */
#define SET( foo ) \
[_##foo##TextField setStringValue:@""];
        SET( uri );
        SET( title );
        SET( artist );
        SET( album );
        SET( trackNumber );
        SET( trackTotal );
        SET( genre );
        SET( season );
        SET( episode );
        SET( actors );
        SET( director );
        SET( showName );
        SET( copyright );
        SET( publisher );
        SET( nowPlaying );
        SET( language );
        SET( date );
        SET( description );
        SET( encodedby );
#undef SET
        [_artworkImageView setImage: [NSImage imageNamed:@"noart.png"]];
    } else {
        if (!_representedInputItem.preparsed) {
            [_representedInputItem preparseInputItem];
        }

        _uriTextField.stringValue = _representedInputItem.decodedMRL;
        _titleTextField.stringValue = _representedInputItem.title;
        _artistTextField.stringValue = _representedInputItem.artist;
        _albumTextField.stringValue = _representedInputItem.albumName;
        _trackNumberTextField.stringValue = _representedInputItem.trackNumber;
        _trackTotalTextField.stringValue = _representedInputItem.trackTotal;
        _genreTextField.stringValue = _representedInputItem.genre;
        _seasonTextField.stringValue = _representedInputItem.season;
        _episodeTextField.stringValue = _representedInputItem.episode;
        _actorsTextField.stringValue = _representedInputItem.actors;
        _directorTextField.stringValue = _representedInputItem.director;
        _showNameTextField.stringValue = _representedInputItem.showName;
        _copyrightTextField.stringValue = _representedInputItem.copyright;
        _publisherTextField.stringValue = _representedInputItem.publisher;
        _nowPlayingTextField.stringValue = _representedInputItem.nowPlaying;
        _languageTextField.stringValue = _representedInputItem.language;
        _dateTextField.stringValue = _representedInputItem.date;
        _descriptionTextField.stringValue = _representedInputItem.contentDescription;
        _encodedbyTextField.stringValue = _representedInputItem.encodedBy;

        NSURL *artworkURL = _representedInputItem.artworkURL;
        NSImage *placeholderImage = [NSImage imageNamed: @"noart.png"];
        [_artworkImageView setImageURL:artworkURL placeholderImage:placeholderImage];

        if (!_mainMenuInstance) {
            [self.window setTitle:_representedInputItem.title];
        }
    }

    /* reload the codec details table */
    [self updateStreamsList];
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
            subItem.propertyName = toNSStr(cat->psz_name);

            // Build list of codec details
            NSMutableArray *infos = [NSMutableArray array];

            info_foreach(info, &cat->infos) {
                VLCCodecInformationTreeItem *infoItem = [[VLCCodecInformationTreeItem alloc] init];
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
    _representedInputItem.artist = _artistTextField.stringValue;
    _representedInputItem.albumName = _albumTextField.stringValue;
    _representedInputItem.genre = _genreTextField.stringValue;
    _representedInputItem.trackNumber = _trackNumberTextField.stringValue;
    _representedInputItem.date = _dateTextField.stringValue;
    _representedInputItem.copyright = _copyrightTextField.stringValue;
    _representedInputItem.publisher = _publisherTextField.stringValue;
    _representedInputItem.contentDescription = _descriptionTextField.stringValue;
    _representedInputItem.language = _languageTextField.stringValue;
    _representedInputItem.showName = _showNameTextField.stringValue;
    _representedInputItem.actors = _actorsTextField.stringValue;
    _representedInputItem.director = _directorTextField.stringValue;

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
