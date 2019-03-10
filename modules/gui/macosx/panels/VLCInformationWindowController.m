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

#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

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
    VLCCodecInformationTreeItem *rootItem;

    input_item_t *_mediaItem;

    BOOL b_stats;
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
    }
    return self;
}

- (void)dealloc
{
    if (_mediaItem)
        input_item_Release(_mediaItem);

    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)windowDidLoad
{
    [self.window setExcludedFromWindowsMenu: YES];
    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [self.window setTitle: _NS("Media Information")];

    _outlineView.dataSource = self;

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

    [self.window setInitialFirstResponder: _uriLabel];

    b_stats = var_InheritBool(getIntf(), "stats");
    if (!b_stats) {
        if ([_segmentedView segmentCount] >= 3)
            [_segmentedView setSegmentCount: 2];
    }
    else
        [self initMediaPanelStats];

    /* We may be awoken from nib way after initialisation
     * Update ourselves */
    [self updatePanelWithItem:_mediaItem];
}

- (void)updateCocoaWindowLevel:(NSInteger)i_level
{
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
    input_item_t *currentMediaItem = playlistController.currentlyPlayingInputItem;
    [self updatePanelWithItem:currentMediaItem];
}

- (void)updatePanelWithItem:(input_item_t *)newInputItem;
{
    if (newInputItem != _mediaItem) {
        if (_mediaItem)
            input_item_Release(_mediaItem);
        [_saveMetaDataButton setEnabled: NO];
        _mediaItem = newInputItem;
    }

    if (!self.isWindowLoaded)
        return;

    if (!_mediaItem) {
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
        if (!input_item_IsPreparsed(_mediaItem))
            libvlc_MetadataRequest(vlc_object_instance(getIntf()), _mediaItem, META_REQUEST_OPTION_NONE,
                                   NULL, NULL, -1, NULL);

        /* fill uri info */
        char *psz_url = vlc_uri_decode(input_item_GetURI(_mediaItem));
        [_uriTextField setStringValue:toNSStr(psz_url)];
        free(psz_url);

        /* fill title info */
        char *psz_title = input_item_GetTitle(_mediaItem);
        if (!psz_title)
            psz_title = input_item_GetName(_mediaItem);
        [_titleTextField setStringValue:toNSStr(psz_title)];
        free(psz_title);

#define SET( foo, bar ) \
char *psz_##foo = input_item_Get##bar ( _mediaItem ); \
[_##foo##TextField setStringValue:toNSStr(psz_##foo)]; \
FREENULL( psz_##foo );

        /* fill the other fields */
        SET( author, Artist );
        SET( collection, Album );
        SET( seqNum, TrackNum );
        SET( genre, Genre );
        SET( copyright, Copyright );
        SET( publisher, Publisher );
        SET( nowPlaying, NowPlaying );
        SET( language, Language );
        SET( date, Date );
        SET( description, Description );
        SET( encodedby, EncodedBy );

#undef SET

        char *psz_meta;
        NSImage *image;
        psz_meta = input_item_GetArtURL(_mediaItem);

        /* FIXME Can also be attachment:// */
        if (psz_meta && strncmp(psz_meta, "attachment://", 13))
            image = [[NSImage alloc] initWithContentsOfURL: [NSURL URLWithString:toNSStr(psz_meta)]];
        else
            image = [NSImage imageNamed: @"noart.png"];
        [_imageWell setImage: image];
        FREENULL(psz_meta);
    }

    /* reload the codec details table */
    [self updateStreamsList];
}

- (void)updateStatistics:(NSNotification *)aNotification
{
    if (!self.isWindowLoaded || !b_stats)
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
    rootItem = [[VLCCodecInformationTreeItem alloc] init];

    if (_mediaItem) {
        vlc_mutex_lock(&_mediaItem->lock);
        // build list of streams
        NSMutableArray *streams = [NSMutableArray array];

        for (int i = 0; i < _mediaItem->i_categories; i++) {
            info_category_t *cat = _mediaItem->pp_categories[i];
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

        rootItem.children = [streams copy];
        vlc_mutex_unlock(&_mediaItem->lock);
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
    if (!_mediaItem) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:_NS("Error while saving meta")];
        [alert setInformativeText:_NS("VLC was unable to save the meta data.")];
        [alert addButtonWithTitle:_NS("OK")];
        [alert runModal];
        return;
    }

    #define utf8( _blub ) \
        [[_blub stringValue] UTF8String]

    input_item_SetName( _mediaItem, utf8( _titleTextField ) );
    input_item_SetTitle( _mediaItem, utf8( _titleTextField ) );
    input_item_SetArtist( _mediaItem, utf8( _authorTextField ) );
    input_item_SetAlbum( _mediaItem, utf8( _collectionTextField ) );
    input_item_SetGenre( _mediaItem, utf8( _genreTextField ) );
    input_item_SetTrackNum( _mediaItem, utf8( _seqNumTextField ) );
    input_item_SetDate( _mediaItem, utf8( _dateTextField ) );
    input_item_SetCopyright( _mediaItem, utf8( _copyrightTextField ) );
    input_item_SetPublisher( _mediaItem, utf8( _publisherTextField ) );
    input_item_SetDescription( _mediaItem, utf8( _descriptionTextField ) );
    input_item_SetLanguage( _mediaItem, utf8( _languageTextField ) );

    playlist_t *p_playlist = pl_Get(getIntf());
    input_item_WriteMeta(VLC_OBJECT(p_playlist), _mediaItem);

    [self updatePanelWithItem: _mediaItem];

    [_saveMetaDataButton setEnabled: NO];
}

- (IBAction)downloadCoverArt:(id)sender
{
    if (_mediaItem)
        libvlc_ArtRequest(vlc_object_instance(getIntf()), _mediaItem, META_REQUEST_OPTION_NONE,
                          NULL, NULL);
}

@end


@implementation VLCInformationWindowController (NSTableDataSource)

- (NSInteger)outlineView:(NSOutlineView *)outlineView
  numberOfChildrenOfItem:(id)item
{
    return (item == nil) ? [rootItem children].count : [item children].count;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView
   isItemExpandable:(id)item {
    return ([item children].count > 0);
}

- (id)outlineView:(NSOutlineView *)outlineView
            child:(NSInteger)index
           ofItem:(id)item
{
    return (item == nil) ? [rootItem children][index] : [item children][index];
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
