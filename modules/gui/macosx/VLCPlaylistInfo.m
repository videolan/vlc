/*****************************************************************************
 * VLCPlaylistInfo.m: Controller for the codec info panel
 *****************************************************************************
 * Copyright (C) 2002-2015 VLC authors and VideoLAN
 * $Id$
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

#import "CompatibilityFixes.h"
#import "VLCMain.h"
#import "VLCPlaylistInfo.h"
#import "VLCPlaylist.h"
#import <vlc_url.h>

@interface VLCInfo () <NSOutlineViewDataSource>
{
    VLCInfoTreeItem *rootItem;

    input_item_t *p_item;

    BOOL b_stats;
}
@end

@implementation VLCInfo

- (id)init
{
    self = [super initWithWindowNibName:@"MediaInfo"];
    if (self) {

    }

    return self;
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
     *Update ourselves */
    [self updatePanelWithItem:p_item];
}


- (void)dealloc
{
    if (p_item)
        input_item_Release(p_item);
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
        [self.window setLevel: [[[VLCMain sharedInstance] voutController] currentStatusWindowLevel]];
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

- (void)updateMetadata
{
    if (!p_item)
        return;

    [self updatePanelWithItem:p_item];
}

- (void)updatePanelWithItem:(input_item_t *)_p_item;
{
    if (_p_item != p_item) {
        if (p_item)
            input_item_Release(p_item);
        [_saveMetaDataButton setEnabled: NO];
        if (_p_item)
            input_item_Hold(_p_item);
        p_item = _p_item;
    }

    if (!self.isWindowLoaded)
        return;

    if (!p_item) {
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
        if (!input_item_IsPreparsed(p_item))
            libvlc_MetadataRequest(getIntf()->obj.libvlc, p_item, META_REQUEST_OPTION_NONE, -1, NULL);

        /* fill uri info */
        char *psz_url = vlc_uri_decode(input_item_GetURI(p_item));
        [_uriTextField setStringValue:toNSStr(psz_url)];
        free(psz_url);

        /* fill title info */
        char *psz_title = input_item_GetTitle(p_item);
        if (!psz_title)
            psz_title = input_item_GetName(p_item);
        [_titleTextField setStringValue:toNSStr(psz_title)];
        free(psz_title);

#define SET( foo, bar ) \
char *psz_##foo = input_item_Get##bar ( p_item ); \
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
        psz_meta = input_item_GetArtURL(p_item);

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

    /* update the stats once to display p_item change faster */
    [self updateStatistics];
}

- (void)updateStatistics
{
    if (!self.isWindowLoaded || !b_stats)
        return;

    if (!p_item || !p_item->p_stats) {
        [self initMediaPanelStats];
        return;
    }

    if (![self.window isVisible])
        return;

    vlc_mutex_lock(&p_item->p_stats->lock);

    /* input */
    [_readBytesTextField setStringValue: [NSString stringWithFormat:
                                          @"%8.0f KiB", (float)(p_item->p_stats->i_read_bytes)/1024]];
    [_inputBitrateTextField setStringValue: [NSString stringWithFormat:
                                             @"%6.0f kb/s", (float)(p_item->p_stats->f_input_bitrate)*8000]];
    [_demuxBytesTextField setStringValue: [NSString stringWithFormat:
                                           @"%8.0f KiB", (float)(p_item->p_stats->i_demux_read_bytes)/1024]];
    [_demuxBitrateTextField setStringValue: [NSString stringWithFormat:
                                             @"%6.0f kb/s", (float)(p_item->p_stats->f_demux_bitrate)*8000]];

    /* Video */
    [_videoDecodedTextField setIntValue: p_item->p_stats->i_decoded_video];
    [_displayedTextField setIntValue: p_item->p_stats->i_displayed_pictures];
    [_lostFramesTextField setIntValue: p_item->p_stats->i_lost_pictures];

    /* Audio */
    [_audioDecodedTextField setIntValue: p_item->p_stats->i_decoded_audio];
    [_playedAudioBuffersTextField setIntValue: p_item->p_stats->i_played_abuffers];
    [_lostAudioBuffersTextField setIntValue: p_item->p_stats->i_lost_abuffers];

    vlc_mutex_unlock(&p_item->p_stats->lock);
}

- (void)updateStreamsList
{
    rootItem = [[VLCInfoTreeItem alloc] init];

    if (p_item) {
        vlc_mutex_lock(&p_item->lock);
        // build list of streams
        NSMutableArray *streams = [NSMutableArray array];

        for (int i = 0; i < p_item->i_categories; i++) {
            info_category_t *cat = p_item->pp_categories[i];

            VLCInfoTreeItem *subItem = [[VLCInfoTreeItem alloc] init];
            subItem.name = toNSStr(cat->psz_name);

            // Build list of codec details
            NSMutableArray *infos = [NSMutableArray array];

            for (int j = 0; j < cat->i_infos; j++) {
                VLCInfoTreeItem *infoItem = [[VLCInfoTreeItem alloc] init];
                infoItem.name = toNSStr(cat->pp_infos[j]->psz_name);
                infoItem.value = toNSStr(cat->pp_infos[j]->psz_value);
                [infos addObject:infoItem];
            }

            subItem.children = [infos copy];
            [streams addObject:subItem];
        }

        rootItem.children = [streams copy];
        vlc_mutex_unlock(&p_item->lock);
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
    if (!p_item) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:_NS("Error while saving meta")];
        [alert setInformativeText:_NS("VLC was unable to save the meta data.")];
        [alert addButtonWithTitle:_NS("OK")];
        [alert runModal];
        return;
    }

    #define utf8( _blub ) \
        [[_blub stringValue] UTF8String]

    input_item_SetName( p_item, utf8( _titleTextField ) );
    input_item_SetTitle( p_item, utf8( _titleTextField ) );
    input_item_SetArtist( p_item, utf8( _authorTextField ) );
    input_item_SetAlbum( p_item, utf8( _collectionTextField ) );
    input_item_SetGenre( p_item, utf8( _genreTextField ) );
    input_item_SetTrackNum( p_item, utf8( _seqNumTextField ) );
    input_item_SetDate( p_item, utf8( _dateTextField ) );
    input_item_SetCopyright( p_item, utf8( _copyrightTextField ) );
    input_item_SetPublisher( p_item, utf8( _publisherTextField ) );
    input_item_SetDescription( p_item, utf8( _descriptionTextField ) );
    input_item_SetLanguage( p_item, utf8( _languageTextField ) );

    playlist_t *p_playlist = pl_Get(getIntf());
    input_item_WriteMeta(VLC_OBJECT(p_playlist), p_item);

    [self updatePanelWithItem: p_item];

    [_saveMetaDataButton setEnabled: NO];
}

- (IBAction)downloadCoverArt:(id)sender
{
    if (p_item)
        libvlc_ArtRequest(getIntf()->obj.libvlc, p_item, META_REQUEST_OPTION_NONE);
}

@end


@implementation VLCInfo (NSTableDataSource)

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    return (item == nil) ? [rootItem children].count : [item children].count;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item {
    return ([item children].count > 0);
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
    return (item == nil) ? [[rootItem children] objectAtIndex:index] : [[item children]objectAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if (!item)
        return @"";

    if ([[tableColumn identifier] isEqualToString:@"0"])
        return [item name];
    else
        return [item value];
}

@end


@implementation VLCInfoTreeItem

@end
