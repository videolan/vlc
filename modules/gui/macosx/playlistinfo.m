/*****************************************************************************
 r playlistinfo.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2008 the VideoLAN team
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
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include "intf.h"
#include "playlistinfo.h"
#include "playlist.h"

/*****************************************************************************
 * VLCPlaylistInfo Implementation
 *****************************************************************************/

@implementation VLCInfo

static VLCInfo *_o_sharedInstance = nil;

+ (VLCInfo *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if( _o_sharedInstance ) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
        
        if( _o_sharedInstance != nil )
        {
            p_item = NULL;
            o_statUpdateTimer = nil;
        }
    }

    return _o_sharedInstance;
}

- (void)awakeFromNib
{
    [o_info_window setExcludedFromWindowsMenu: TRUE];

    [o_info_window setTitle: _NS("Media Information")];
    [o_uri_lbl setStringValue: _NS("Location")];
    [o_title_lbl setStringValue: _NS("Title")];
    [o_author_lbl setStringValue: _NS("Artist")];
    [o_saveMetaData_btn setStringValue: _NS("Save Metadata" )];

    [[o_tab_view tabViewItemAtIndex: 0] setLabel: _NS("General")];
    [[o_tab_view tabViewItemAtIndex: 1] setLabel: _NS("Codec Details")];
    [[o_tab_view tabViewItemAtIndex: 2] setLabel: _NS("Statistics")];
    [o_tab_view selectTabViewItemAtIndex: 0];

    /* constants defined in vlc_meta.h */
    [o_genre_lbl setStringValue: _NS(VLC_META_GENRE)];
    [o_copyright_lbl setStringValue: _NS(VLC_META_COPYRIGHT)];
    [o_collection_lbl setStringValue: _NS(VLC_META_ALBUM)];
    [o_seqNum_lbl setStringValue: _NS(VLC_META_TRACK_NUMBER)];
    [o_description_lbl setStringValue: _NS(VLC_META_DESCRIPTION)];
    [o_date_lbl setStringValue: _NS(VLC_META_DATE)];
    [o_language_lbl setStringValue: _NS(VLC_META_LANGUAGE)];
    [o_nowPlaying_lbl setStringValue: _NS(VLC_META_NOW_PLAYING)];
    [o_publisher_lbl setStringValue: _NS(VLC_META_PUBLISHER)];

    /* statistics */
    [o_input_box setTitle: _NS("Input")];
    [o_read_bytes_lbl setStringValue: _NS("Read at media")];
    [o_input_bitrate_lbl setStringValue: _NS("Input bitrate")];
    [o_demux_bytes_lbl setStringValue: _NS("Demuxed")];
    [o_demux_bitrate_lbl setStringValue: _NS("Stream bitrate")];

    [o_video_box setTitle: _NS("Video")];
    [o_video_decoded_lbl setStringValue: _NS("Decoded blocks")];
    [o_displayed_lbl setStringValue: _NS("Displayed frames")];
    [o_lost_frames_lbl setStringValue: _NS("Lost frames")];
	[o_fps_lbl setStringValue: _NS("Frames per Second")];

    [o_sout_box setTitle: _NS("Streaming")];
    [o_sent_packets_lbl setStringValue: _NS("Sent packets")];
    [o_sent_bytes_lbl setStringValue: _NS("Sent bytes")];
    [o_sent_bitrate_lbl setStringValue: _NS("Send rate")];

    [o_audio_box setTitle: _NS("Audio")];
    [o_audio_decoded_lbl setStringValue: _NS("Decoded blocks")];
    [o_played_abuffers_lbl setStringValue: _NS("Played buffers")];
    [o_lost_abuffers_lbl setStringValue: _NS("Lost buffers")];

    [o_info_window setInitialFirstResponder: o_uri_txt];
}

- (void)dealloc
{
    /* make sure that the timer is released in any case */
    if( [o_statUpdateTimer isValid] )
        [o_statUpdateTimer invalidate];

    if ( o_statUpdateTimer )
        [o_statUpdateTimer release];

    [super dealloc];
}

- (void)initPanel
{
    BOOL b_stats = config_GetInt(VLCIntf, "stats");
    [self initMediaPanelStats];
    if( b_stats )
    {
        o_statUpdateTimer = [NSTimer scheduledTimerWithTimeInterval: 1
            target: self selector: @selector(updateStatistics:)
            userInfo: nil repeats: YES];
        [o_statUpdateTimer fire];
        [o_statUpdateTimer retain];
    }
    else
    {
        if( [o_tab_view numberOfTabViewItems] > 2 )
            [o_tab_view removeTabViewItem: [o_tab_view tabViewItemAtIndex: 2]];
    }

    [self updatePanel];
    [o_info_window makeKeyAndOrderFront: self];
}

- (void)initMediaPanelStats
{
    //Initializing Input Variables
    [o_read_bytes_txt setStringValue: [NSString stringWithFormat:@"%8.0f kB", (float)0]];
    [o_input_bitrate_txt setStringValue: [NSString stringWithFormat:@"%6.0f kb/s", (float)0]];
    [o_demux_bytes_txt setStringValue: [NSString stringWithFormat:@"%8.0f kB", (float)0]];
    [o_demux_bitrate_txt setStringValue: [NSString stringWithFormat:@"%6.0f kb/s", (float)0]];
    
    //Initializing Video Variables
    [o_video_decoded_txt setIntValue:0];
    [o_displayed_txt setIntValue:0];
    [o_lost_frames_txt setIntValue:0];
    [o_fps_txt setFloatValue:0];

    //Initializing Output Variables
    [o_sent_packets_txt setIntValue: 0];
    [o_sent_bytes_txt setStringValue: [NSString stringWithFormat:@"%8.0f kB", (float)0]];
    [o_sent_bitrate_txt setStringValue: [NSString stringWithFormat:@"%6.0f kb/s", (float)0]];

    //Initializing Audio Variables
    [o_audio_decoded_txt setIntValue:0];
    [o_played_abuffers_txt setIntValue: 0];
    [o_lost_abuffers_txt setIntValue: 0];

}

- (void)updatePanel
{
    /* make sure that we got the current item and not an outdated one */
    intf_thread_t * p_intf = VLCIntf;
        playlist_t * p_playlist = pl_Yield( p_intf );

    p_item = p_playlist->status.p_item;
    vlc_object_release( p_playlist );

    /* check whether our item is valid, because we would crash if not */
    if(! [self isItemInPlaylist: p_item] ) return;

    /* fill uri info */
    if( input_item_GetURI( p_item->p_input ) != NULL )
    {
        [o_uri_txt setStringValue: [NSString stringWithUTF8String: input_item_GetURI( p_item->p_input ) ]];
    }

#define SET( foo, bar ) \
    char *psz_##foo = input_item_Get##bar ( p_item->p_input ); \
    [self setMeta: psz_##foo forLabel: o_##foo##_txt]; \
    FREENULL( psz_##foo );

    /* fill the other fields */
    SET( title, Title );
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

#undef SET

    char *psz_meta;
    NSImage *o_image;
    psz_meta = input_item_GetArtURL( p_item->p_input );
    if( psz_meta && !strncmp( psz_meta, "file://", 7 ) )
        o_image = [[NSImage alloc] initWithContentsOfURL: [NSURL URLWithString: [NSString stringWithUTF8String: psz_meta]]];
    else
        o_image = [[NSImage imageNamed: @"noart.png"] retain];
    [o_image_well setImage: o_image];
    [o_image release];
    FREENULL( psz_meta );

    /* reload the advanced table */
    [[VLCInfoTreeItem rootItem] refresh];
    [o_outline_view reloadData];

    /* update the stats once to display p_item change faster */
    [self updateStatistics: nil];
}

- (void)setMeta: (char *)psz_meta forLabel: (id)theItem
{
    if( psz_meta != NULL && *psz_meta)
        [theItem setStringValue: [NSString stringWithUTF8String:psz_meta]];
    else
        [theItem setStringValue: @""];
}

- (void)updateStatistics:(NSTimer*)theTimer
{
    if( [self isItemInPlaylist: p_item] )
    {
        vlc_mutex_lock( &p_item->p_input->p_stats->lock );

        /* input */
        [o_read_bytes_txt setStringValue: [NSString stringWithFormat:
            @"%8.0f kB", (float)(p_item->p_input->p_stats->i_read_bytes)/1000]];
        [o_input_bitrate_txt setStringValue: [NSString stringWithFormat:
            @"%6.0f kb/s", (float)(p_item->p_input->p_stats->f_input_bitrate)*8000]];
        [o_demux_bytes_txt setStringValue: [NSString stringWithFormat:
            @"%8.0f kB", (float)(p_item->p_input->p_stats->i_demux_read_bytes)/1000]];
        [o_demux_bitrate_txt setStringValue: [NSString stringWithFormat:
            @"%6.0f kb/s", (float)(p_item->p_input->p_stats->f_demux_bitrate)*8000]];

        /* Video */
        [o_video_decoded_txt setIntValue: p_item->p_input->p_stats->i_decoded_video];
        [o_displayed_txt setIntValue: p_item->p_input->p_stats->i_displayed_pictures];
        [o_lost_frames_txt setIntValue: p_item->p_input->p_stats->i_lost_pictures];
        float f_fps = 0;
        /* FIXME: input_Control( p_item->p_input, INPUT_GET_VIDEO_FPS, &f_fps ); */
        [o_fps_txt setFloatValue: f_fps];

        /* Sout */
        [o_sent_packets_txt setIntValue: p_item->p_input->p_stats->i_sent_packets];
        [o_sent_bytes_txt setStringValue: [NSString stringWithFormat: @"%8.0f kB",
            (float)(p_item->p_input->p_stats->i_sent_bytes)/1000]];
        [o_sent_bitrate_txt setStringValue: [NSString stringWithFormat:
            @"%6.0f kb/s", (float)(p_item->p_input->p_stats->f_send_bitrate*8)*1000]];

        /* Audio */
        [o_audio_decoded_txt setIntValue: p_item->p_input->p_stats->i_decoded_audio];
        [o_played_abuffers_txt setIntValue: p_item->p_input->p_stats->i_played_abuffers];
        [o_lost_abuffers_txt setIntValue: p_item->p_input->p_stats->i_lost_abuffers];

        vlc_mutex_unlock( &p_item->p_input->p_stats->lock );
    }
}

- (IBAction)metaFieldChanged:(id)sender
{
    [o_saveMetaData_btn setEnabled: YES];
}

- (IBAction)saveMetaData:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Yield( p_intf );
    vlc_value_t val;

    if( [self isItemInPlaylist: p_item] )
    {
        meta_export_t p_export;
        p_export.p_item = p_item->p_input;

        if( p_item->p_input == NULL )
            goto end;

        /* we can write meta data only in a file */
        vlc_mutex_lock( &p_item->p_input->lock );
        int i_type = p_item->p_input->i_type;
        vlc_mutex_unlock( &p_item->p_input->lock );
        if( i_type == ITEM_TYPE_FILE )
        {
            char *psz_uri_orig = input_item_GetURI( p_item->p_input );
            char *psz_uri = psz_uri_orig;
            if( !strncmp( psz_uri, "file://", 7 ) )
                psz_uri += 7; /* strlen("file://") = 7 */
            
            p_export.psz_file = strndup( psz_uri, PATH_MAX );
            free( psz_uri_orig );
        }
        else
            goto end;

        #define utf8( o_blub ) \
            [[o_blub stringValue] UTF8String]

        input_item_SetName( p_item->p_input, utf8( o_title_txt ) );
        input_item_SetTitle( p_item->p_input, utf8( o_title_txt ) );
        input_item_SetArtist( p_item->p_input, utf8( o_author_txt ) );
        input_item_SetAlbum( p_item->p_input, utf8( o_collection_txt ) );
        input_item_SetGenre( p_item->p_input, utf8( o_genre_txt ) );
        input_item_SetTrackNum( p_item->p_input, utf8( o_seqNum_txt ) );
        input_item_SetDate( p_item->p_input, utf8( o_date_txt ) );
        input_item_SetCopyright( p_item->p_input, utf8( o_copyright_txt ) );
        input_item_SetPublisher( p_item->p_input, utf8( o_publisher_txt ) );
        input_item_SetDescription( p_item->p_input, utf8( o_description_txt ) );
        input_item_SetLanguage( p_item->p_input, utf8( o_language_txt ) );

        PL_LOCK;
        p_playlist->p_private = &p_export;

        module_t *p_mod = module_Need( p_playlist, "meta writer", NULL, 0 );
        if( p_mod )
            module_Unneed( p_playlist, p_mod );
        PL_UNLOCK;

        val.b_bool = true;
        var_Set( p_playlist, "intf-change", val );
        [self updatePanel];
    }

    end:
    vlc_object_release( p_playlist );
    [o_saveMetaData_btn setEnabled: NO];
}

- (playlist_item_t *)getItem
{
    return p_item;
}

- (BOOL)isItemInPlaylist:(playlist_item_t *)p_local_item
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Yield( p_intf );
    int i;

    for( i = 0 ; i < p_playlist->all_items.i_size ; i++ )
    {
        if( ARRAY_VAL( p_playlist->all_items, i ) == p_local_item )
        {
            vlc_object_release( p_playlist );
            return YES;
        }
    }
    vlc_object_release( p_playlist );
    return NO;
}

- (BOOL)windowShouldClose:(id)sender
{
    if( [o_statUpdateTimer isValid] )
        [o_statUpdateTimer invalidate];

    if( o_statUpdateTimer )
        [o_statUpdateTimer release];

    return YES;
}

@end

@implementation VLCInfo (NSMenuValidation)

- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    BOOL bEnabled = TRUE;

    intf_thread_t * p_intf = VLCIntf;
    input_thread_t * p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                       FIND_ANYWHERE );

    if( [[o_mi title] isEqualToString: _NS("Information")] )
    {
        if( p_input == NULL )
        {
            bEnabled = FALSE;
        }
    }
    if( p_input ) vlc_object_release( p_input );

    return( bEnabled );
}

@end

@implementation VLCInfo (NSTableDataSource)

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    return (item == nil) ? [[VLCInfoTreeItem rootItem] numberOfChildren] : [item numberOfChildren];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item {
    return ([item numberOfChildren] > 0);
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    return (item == nil) ? [[VLCInfoTreeItem rootItem] childAtIndex:index] : (id)[item childAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if ([[tableColumn identifier] isEqualToString:@"0"])
    {
        return (item == nil) ? @"" : (id)[item getName];
    }
    else
    {
        return (item == nil) ? @"" : (id)[item getValue];
    }
}


@end

@implementation VLCInfoTreeItem

static VLCInfoTreeItem *o_root_item = nil;

#define IsALeafNode ((id)-1)

- (id)initWithName: (NSString *)o_item_name value: (NSString *)o_item_value ID: (int)i_id parent:(VLCInfoTreeItem *)o_parent_item
{
    self = [super init];

    if( self != nil )
    {
        o_name = [o_item_name copy];
        o_value = [o_item_value copy];
        i_object_id = i_id;
        o_parent = o_parent_item;
        if( [[VLCMain sharedInstance] getInfo] != nil )
            p_item = [[[VLCMain sharedInstance] getInfo] getItem];
        else
            p_item = NULL;
    }
    return( self );
}

+ (VLCInfoTreeItem *)rootItem {
    if( o_root_item == nil )
        o_root_item = [[VLCInfoTreeItem alloc] initWithName:@"main" value: @"" ID: 0 parent:nil];
    return o_root_item;
}

- (void)dealloc
{
    if( o_children != IsALeafNode ) [o_children release];
    [o_name release];
    [super dealloc];
}

/* Creates and returns the array of children
 * Loads children incrementally */
- (NSArray *)children
{
    if (o_children == NULL)
    {
        int i;

        if( [[[VLCMain sharedInstance] getInfo] isItemInPlaylist: p_item] )
        {
            if( self == o_root_item )
            {
                vlc_mutex_lock( &p_item->p_input->lock );
                o_children = [[NSMutableArray alloc] initWithCapacity:
                                                p_item->p_input->i_categories];
                for (i = 0 ; i < p_item->p_input->i_categories ; i++)
                {
                    [o_children addObject:[[VLCInfoTreeItem alloc]
                        initWithName: [NSString stringWithUTF8String:
                            p_item->p_input->pp_categories[i]->psz_name]
                        value: @""
                        ID: i
                        parent: self]];
                }
                vlc_mutex_unlock( &p_item->p_input->lock );
            }
            else if( o_parent == o_root_item )
            {
                vlc_mutex_lock( &p_item->p_input->lock );
                o_children = [[NSMutableArray alloc] initWithCapacity:
                    p_item->p_input->pp_categories[i_object_id]->i_infos];

                for (i = 0 ; i < p_item->p_input->pp_categories[i_object_id]->i_infos ; i++)
                {
                    [o_children addObject:[[VLCInfoTreeItem alloc]
                    initWithName: [NSString stringWithUTF8String:
                            p_item->p_input->pp_categories[i_object_id]->pp_infos[i]->psz_name]
                        value: [NSString stringWithUTF8String:
                            p_item->p_input->pp_categories[i_object_id]->pp_infos[i]->psz_value ? : ""]
                        ID: i
                        parent: self]];
                }
                vlc_mutex_unlock( &p_item->p_input->lock );
            }
            else
            {
                o_children = IsALeafNode;
            }
        }
    }
    return o_children;
}

- (NSString *)getName
{
    return o_name;
}

- (NSString *)getValue
{
    return o_value;
}

- (VLCInfoTreeItem *)childAtIndex:(int)i_index {
    return [[self children] objectAtIndex:i_index];
}

- (int)numberOfChildren {
    id i_tmp = [self children];
    return ( i_tmp == IsALeafNode ) ? (-1) : (int)[i_tmp count];
}

- (void)refresh
{
    p_item = [[[VLCMain sharedInstance] getInfo] getItem];
    if( o_children != NULL )
    {
        [o_children release];
        o_children = NULL;
    }
}

@end

