/*****************************************************************************
 * VLCPlaylistInfo.m: MacOS X interface module
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
#import "intf.h"
#import "VLCPlaylistInfo.h"
#import "VLCPlaylist.h"
#import <vlc_url.h>

@interface VLCInfo () <NSOutlineViewDataSource, NSOutlineViewDelegate>
{
    VLCInfoTreeItem *rootItem;

    input_item_t *p_item;

    BOOL b_nibLoaded;
    BOOL b_awakeFromNib;
    BOOL b_stats;
}
@end

@implementation VLCInfo

+ (VLCInfo *)sharedInstance
{
    static VLCInfo *sharedInstance = nil;
    static dispatch_once_t pred;

    dispatch_once(&pred, ^{
        sharedInstance = [VLCInfo new];
    });

    return sharedInstance;
}

- (void)awakeFromNib
{
    [_infoPanel setExcludedFromWindowsMenu: YES];
    [_infoPanel setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [_infoPanel setTitle: _NS("Media Information")];

    _outlineView.dataSource = self;
    _outlineView.delegate = self;

    [_uriLabel setStringValue: _NS("Location")];
    [_titleLabel setStringValue: _NS("Title")];
    [_authorLabel setStringValue: _NS("Artist")];
    [_saveMetaDataButton setStringValue: _NS("Save Metadata")];

    [[_tabView tabViewItemAtIndex: 0] setLabel: _NS("General")];
    [[_tabView tabViewItemAtIndex: 1] setLabel: _NS("Codec Details")];
    [[_tabView tabViewItemAtIndex: 2] setLabel: _NS("Statistics")];
    [_tabView selectTabViewItemAtIndex: 0];

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

    [_soutLabel setStringValue: _NS("Streaming")];
    [_sentPacketsLabel setStringValue: _NS("Sent packets")];
    [_sentBytesLabel setStringValue: _NS("Sent bytes")];
    [_sentBitrateLabel setStringValue: _NS("Send rate")];

    [_audioLabel setStringValue: _NS("Audio")];
    [_audioDecodedLabel setStringValue: _NS("Decoded blocks")];
    [_playedAudioBuffersLabel setStringValue: _NS("Played buffers")];
    [_lostAudioBuffersLabel setStringValue: _NS("Lost buffers")];

    [_infoPanel setInitialFirstResponder: _uriLabel];

    b_awakeFromNib = YES;

    /* We may be awoken from nib way after initialisation
     *Update ourselves */
    [self updatePanelWithItem:p_item];
}


- (void)dealloc
{
    if (p_item)
        vlc_gc_decref(p_item);
}

- (void)updateCocoaWindowLevel:(NSInteger)i_level
{
    if (_infoPanel && [_infoPanel isVisible] && [_infoPanel level] != i_level)
        [_infoPanel setLevel: i_level];
}

- (void)initPanel
{
    if (!b_nibLoaded)
        b_nibLoaded = [NSBundle loadNibNamed:@"MediaInfo" owner: self];

    b_stats = var_InheritBool(VLCIntf, "stats");
    if (!b_stats) {
        if ([_tabView numberOfTabViewItems] > 2)
            [_tabView removeTabViewItem: [_tabView tabViewItemAtIndex: 2]];
    }
    else
        [self initMediaPanelStats];

    NSInteger i_level = [[[VLCMain sharedInstance] voutController] currentStatusWindowLevel];
    [_infoPanel setLevel: i_level];
    [_infoPanel makeKeyAndOrderFront:nil];
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

    //Initializing Output Variables
    [_sentPacketsTextField setIntValue: 0];
    [_sentBytesTextField setStringValue: [NSString stringWithFormat:_NS("%.1f KiB"), (float)0]];
    [_sentBitrateTextField setStringValue: [NSString stringWithFormat:@"%6.0f kb/s", (float)0]];

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
    @autoreleasepool {
        rootItem = [[VLCInfoTreeItem alloc] init];

        if (_p_item != p_item) {
            if (p_item)
                vlc_gc_decref(p_item);
            [_saveMetaDataButton setEnabled: NO];
            if (_p_item)
                vlc_gc_incref(_p_item);
            p_item = _p_item;
        }

        if (!p_item) {
            /* Erase */
        #define SET( foo ) \
            [self setMeta: "" forLabel: _##foo##TextField];
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
                libvlc_MetaRequest(VLCIntf->p_libvlc, p_item, META_REQUEST_OPTION_NONE);

            /* fill uri info */
            char *psz_url = decode_URI(input_item_GetURI(p_item));
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
            [self setMeta: psz_##foo forLabel: _##foo##TextField]; \
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

        /* reload the advanced table */
        [rootItem refresh];
        [_outlineView reloadData];
        [_outlineView expandItem:nil expandChildren:YES];

        /* update the stats once to display p_item change faster */
        [self updateStatistics];
    }
}

- (void)setMeta: (char *)psz_meta forLabel: (id)theItem
{
    if (psz_meta != NULL && *psz_meta)
        [theItem setStringValue: toNSStr(psz_meta)];
    else
        [theItem setStringValue: @""];
}

- (void)updateStatistics
{
    if (!b_awakeFromNib || !b_stats)
        return;

    if ([_infoPanel isVisible]) {
        if (!p_item || !p_item->p_stats) {
            [self initMediaPanelStats];
            return;
        }

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

        /* Sout */
        [_sentPacketsTextField setIntValue: p_item->p_stats->i_sent_packets];
        [_sentBytesTextField setStringValue: [NSString stringWithFormat: @"%8.0f KiB",
            (float)(p_item->p_stats->i_sent_bytes)/1024]];
        [_sentBitrateTextField setStringValue: [NSString stringWithFormat:
            @"%6.0f kb/s", (float)(p_item->p_stats->f_send_bitrate*8)*1000]];

        /* Audio */
        [_audioDecodedTextField setIntValue: p_item->p_stats->i_decoded_audio];
        [_playedAudioBuffersTextField setIntValue: p_item->p_stats->i_played_abuffers];
        [_lostAudioBuffersTextField setIntValue: p_item->p_stats->i_lost_abuffers];

        vlc_mutex_unlock(&p_item->p_stats->lock);
    }
}

- (IBAction)metaFieldChanged:(id)sender
{
    [_saveMetaDataButton setEnabled: YES];
}

- (IBAction)saveMetaData:(id)sender
{
    if (!p_item)
        goto error;

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

    playlist_t *p_playlist = pl_Get(VLCIntf);
    input_item_WriteMeta(VLC_OBJECT(p_playlist), p_item);

    var_SetBool(p_playlist, "intf-change", true);
    [self updatePanelWithItem: p_item];

    [_saveMetaDataButton setEnabled: NO];
    return;

error:
    NSRunAlertPanel(_NS("Error while saving meta"), @"%@",
                    _NS("OK"), nil, nil,
                    _NS("VLC was unable to save the meta data."));
}

- (IBAction)downloadCoverArt:(id)sender
{
    playlist_t *p_playlist = pl_Get(VLCIntf);
    if (p_item) libvlc_ArtRequest(VLCIntf->p_libvlc, p_item, META_REQUEST_OPTION_NONE);
}

- (input_item_t *)item
{
    if (p_item) vlc_gc_incref(p_item);
    return p_item;
}

@end

@implementation VLCInfo (NSMenuValidation)

- (BOOL)validateMenuItem:(NSMenuItem *)mi
{
    if ([[mi title] isEqualToString: _NS("Information")]) {
        return ![[[VLCMain sharedInstance] playlist] isSelectionEmpty];
    }

    return TRUE;
}

@end

@implementation VLCInfo (NSTableDataSource)

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    return (item == nil) ? [rootItem numberOfChildren] : [item numberOfChildren];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item {
    return ([item numberOfChildren] > 0);
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
    return (item == nil) ? [rootItem childAtIndex:index] : (id)[item childAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if ([[tableColumn identifier] isEqualToString:@"0"])
        return (item == nil) ? @"" : (id)[item name];
    else
        return (item == nil) ? @"" : (id)[item value];
}

@end

@interface VLCInfoTreeItem ()
{
    int i_object_id;
    input_item_t *p_item;
    VLCInfoTreeItem *_parent;
    NSMutableArray *_children;
    BOOL _isALeafNode;
}

@end

@implementation VLCInfoTreeItem

- (id)initWithName:(NSString *)item_name
             value:(NSString *)item_value
                ID:(int)i_id
            parent:(VLCInfoTreeItem *)parent_item
{
    self = [super init];

    if (self != nil) {
        _name = [item_name copy];
        _value = [item_value copy];
        i_object_id = i_id;
        _parent = parent_item;
        p_item = [[VLCInfo sharedInstance] item];
    }
    return self;
}

- (id)init
{
    return [self initWithName:@"main" value:@"" ID:-1 parent:nil];
}

- (void)dealloc
{
    if (p_item)
        vlc_gc_decref(p_item);
}

/* Creates and returns the array of children
 *Loads children incrementally */
- (void)_updateChildren
{
    if (!p_item)
        return;

    if (_children != nil)
        return;

    _children = [[NSMutableArray alloc] init];
    if (i_object_id == -1) {
        vlc_mutex_lock(&p_item->lock);
        for (int i = 0 ; i < p_item->i_categories ; i++) {
            NSString *name = toNSStr(p_item->pp_categories[i]->psz_name);
            VLCInfoTreeItem *item = [[VLCInfoTreeItem alloc]
                                      initWithName:name
                                      value:@""
                                      ID:i
                                      parent:self];
            [_children addObject:item];
        }
        vlc_mutex_unlock(&p_item->lock);
        _isALeafNode = NO;
    }
    else if (_parent->i_object_id == -1) {
        vlc_mutex_lock(&p_item->lock);
        info_category_t *cat = p_item->pp_categories[i_object_id];
        for (int i = 0 ; i < cat->i_infos ; i++) {
            NSString *name = toNSStr(cat->pp_infos[i]->psz_name);
            NSString *value  = toNSStr(cat->pp_infos[i]->psz_value);
            VLCInfoTreeItem *item = [[VLCInfoTreeItem alloc]
                                     initWithName:name
                                     value:value
                                     ID:i
                                     parent:self];
            [_children addObject:item];
        }
        vlc_mutex_unlock(&p_item->lock);
        _isALeafNode = NO;
    }
    else
        _isALeafNode = YES;
}

- (void)refresh
{
    if (p_item)
        vlc_gc_decref(p_item);

    p_item = [[VLCInfo sharedInstance] item];

    _children = nil;
}

- (VLCInfoTreeItem *)childAtIndex:(NSUInteger)i_index
{
    return [_children objectAtIndex:i_index];
}

- (int)numberOfChildren
{
    [self _updateChildren];

    if (_isALeafNode)
        return -1;

    return [_children count];
}

@end
