/*****************************************************************************
 * VLCInputItem.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCInputItem.h"

#import "main/VLCMain.h"
#import "extensions/NSString+Helpers.h"

#import <vlc_url.h>

NSString *VLCInputItemParsingSucceeded = @"VLCInputItemParsingSucceeded";
NSString *VLCInputItemParsingFailed = @"VLCInputItemParsingFailed";
NSString *VLCInputItemSubtreeAdded = @"VLCInputItemSubtreeAdded";
NSString *VLCInputItemPreparsingSkipped = @"VLCInputItemPreparsingSkipped";
NSString *VLCInputItemPreparsingFailed = @"VLCInputItemPreparsingFailed";
NSString *VLCInputItemPreparsingTimeOut = @"VLCInputItemPreparsingTimeOut";
NSString *VLCInputItemPreparsingSucceeded = @"VLCInputItemPreparsingSucceeded";

@interface VLCInputItem()
{
    input_item_parser_id_t *_p_parserID;
}

- (void)parsingEnded:(int)status;
- (void)subTreeAdded:(input_item_node_t *)p_node;
- (void)preparsingEnded:(enum input_item_preparse_status)status;

@end

static void cb_parsing_ended(input_item_t *p_item, int status, void *p_data)
{
    VLC_UNUSED(p_item);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCInputItem *inputItem = (__bridge VLCInputItem *)p_data;
        [inputItem parsingEnded:status];
    });
}

static void cb_subtree_added(input_item_t *p_item, input_item_node_t *p_node, void *p_data)
{
    VLC_UNUSED(p_item);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCInputItem *inputItem = (__bridge VLCInputItem *)p_data;
        [inputItem subTreeAdded:p_node];
    });
}

static const struct input_item_parser_cbs_t parserCallbacks =
{
    cb_parsing_ended,
    cb_subtree_added,
};

static void cb_preparse_ended(input_item_t *p_item, enum input_item_preparse_status status, void *p_data)
{
    VLC_UNUSED(p_item);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCInputItem *inputItem = (__bridge VLCInputItem *)p_data;
        [inputItem preparsingEnded:status];
    });
}

static const struct input_preparser_callbacks_t preparseCallbacks = {
    cb_preparse_ended,
    cb_subtree_added,
};

@implementation VLCInputItem

- (instancetype)initWithInputItem:(struct input_item_t *)p_inputItem
{
    self = [super init];
    if (self && p_inputItem != NULL) {
        _vlcInputItem = p_inputItem;
        input_item_Hold(_vlcInputItem);
    }
    return self;
}

- (void)dealloc
{
    if (_p_parserID) {
        input_item_parser_id_Release(_p_parserID);
    }
    input_item_Release(_vlcInputItem);
}

- (NSString *)name
{
    if (_vlcInputItem) {
        return toNSStr(_vlcInputItem->psz_name);
    }
    return @"";
}
- (void)setName:(NSString *)name
{
    if (_vlcInputItem) {
        input_item_SetName(_vlcInputItem, [name UTF8String]);
    }
}

- (NSString *)title
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_title = input_item_GetTitle(_vlcInputItem);
    if (!psz_title) {
        return self.name;
    }

    NSString *returnValue = toNSStr(psz_title);
    FREENULL(psz_title);
    return returnValue;
}
-(void)setTitle:(NSString *)title
{
    if (_vlcInputItem) {
        input_item_SetTitle(_vlcInputItem, [title UTF8String]);
    }
}

- (NSString *)artist
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_artist = input_item_GetArtist(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_artist);
    FREENULL(psz_artist);
    return returnValue;
}
- (void)setArtist:(NSString *)artist
{
    if (_vlcInputItem) {
        input_item_SetArtist(_vlcInputItem, [artist UTF8String]);
    }
}

- (NSString *)albumName
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_album = input_item_GetAlbum(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_album);
    FREENULL(psz_album);
    return returnValue;
}
- (void)setAlbumName:(NSString *)albumName
{
    if (_vlcInputItem) {
        input_item_SetAlbum(_vlcInputItem, [albumName UTF8String]);
    }
}

- (NSString *)trackNumber
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_trackNumber = input_item_GetTrackNumber(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_trackNumber);
    FREENULL(psz_trackNumber);
    return returnValue;
}
- (void)setTrackNumber:(NSString *)trackNumber
{
    if (_vlcInputItem) {
        input_item_SetTrackNumber(_vlcInputItem, [trackNumber UTF8String]);
    }
}

- (NSString *)genre
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_genre = input_item_GetGenre(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_genre);
    FREENULL(psz_genre);
    return returnValue;
}
- (void)setGenre:(NSString *)genre
{
    if (_vlcInputItem) {
        input_item_SetGenre(_vlcInputItem, [genre UTF8String]);
    }
}

- (NSString *)copyright
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_copyright = input_item_GetCopyright(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_copyright);
    FREENULL(psz_copyright);
    return returnValue;
}
- (void)setCopyright:(NSString *)copyright
{
    if (_vlcInputItem) {
        input_item_SetCopyright(_vlcInputItem, [copyright UTF8String]);
    }
}

- (NSString *)publisher
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_publisher = input_item_GetPublisher(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_publisher);
    FREENULL(psz_publisher);
    return returnValue;
}
- (void)setPublisher:(NSString *)publisher
{
    if (_vlcInputItem) {
        input_item_SetPublisher(_vlcInputItem, [publisher UTF8String]);
    }
}

- (NSString *)nowPlaying
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_nowPlaying = input_item_GetNowPlaying(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_nowPlaying);
    FREENULL(psz_nowPlaying);
    return returnValue;
}

- (NSString *)language
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_language = input_item_GetLanguage(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_language);
    FREENULL(psz_language);
    return returnValue;
}
- (void)setLanguage:(NSString *)language
{
    if (_vlcInputItem) {
        input_item_SetLanguage(_vlcInputItem, [language UTF8String]);
    }
}

- (NSString *)date
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_date = input_item_GetDate(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_date);
    FREENULL(psz_date);
    return returnValue;
}
- (void)setDate:(NSString *)date
{
    if (_vlcInputItem) {
        input_item_SetDate(_vlcInputItem, [date UTF8String]);
    }
}

- (NSString *)contentDescription
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_description = input_item_GetDescription(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_description);
    FREENULL(psz_description);
    return returnValue;
}
- (void)setContentDescription:(NSString *)contentDescription
{
    if (_vlcInputItem) {
        input_item_SetDescription(_vlcInputItem, [contentDescription UTF8String]);
    }
}

- (NSString *)encodedBy
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_encodedBy = input_item_GetEncodedBy(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_encodedBy);
    FREENULL(psz_encodedBy);
    return returnValue;
}

- (NSString *)trackID
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_trackID = input_item_GetTrackID(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_trackID);
    FREENULL(psz_trackID);
    return returnValue;
}

- (NSString *)trackTotal
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_trackTotal = input_item_GetTrackTotal(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_trackTotal);
    FREENULL(psz_trackTotal);
    return returnValue;
}

- (NSString *)director
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_director = input_item_GetDirector(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_director);
    FREENULL(psz_director);
    return returnValue;
}
- (void)setDirector:(NSString *)director
{
    if (_vlcInputItem) {
        input_item_SetDirector(_vlcInputItem, [director UTF8String]);
    }
}

- (NSString *)season
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_season = input_item_GetSeason(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_season);
    FREENULL(psz_season);
    return returnValue;
}

- (NSString *)episode
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_episode = input_item_GetEpisode(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_episode);
    FREENULL(psz_episode);
    return returnValue;
}

- (NSString *)showName
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_showName = input_item_GetShowName(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_showName);
    FREENULL(psz_showName);
    return returnValue;
}
- (void)setShowName:(NSString *)showName
{
    if (_vlcInputItem) {
        input_item_SetShowName(_vlcInputItem, [showName UTF8String]);
    }
}

- (NSString *)actors
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_actors = input_item_GetActors(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_actors);
    FREENULL(psz_actors);
    return returnValue;
}
- (void)setActors:(NSString *)actors
{
    if (_vlcInputItem) {
        input_item_SetActors(_vlcInputItem, [actors UTF8String]);
    }
}

- (NSString *)discNumber
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_discNumber = input_item_GetDiscNumber(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_discNumber);
    FREENULL(psz_discNumber);
    return returnValue;
}

- (NSString *)totalNumberOfDiscs
{
    if (!_vlcInputItem) {
        return nil;
    }
    char *psz_totalDiscNumber = input_item_GetDiscTotal(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_totalDiscNumber);
    FREENULL(psz_totalDiscNumber);
    return returnValue;
}

- (NSString *)MRL
{
    if (_vlcInputItem) {
        return toNSStr(_vlcInputItem->psz_uri);
    }
    return @"";
}

- (NSString *)decodedMRL
{
    if (_vlcInputItem) {
        char *psz_url = vlc_uri_decode(input_item_GetURI(_vlcInputItem));
        NSString *returnValue = toNSStr(psz_url);
        FREENULL(psz_url);
        return returnValue;
    }
    return nil;
}

- (vlc_tick_t)duration
{
    if (_vlcInputItem) {
        return _vlcInputItem->i_duration;
    }
    return -1;
}

- (enum input_item_type_e)inputType
{
    if (_vlcInputItem) {
        return _vlcInputItem->i_type;
    }
    return ITEM_TYPE_UNKNOWN;
}

- (NSURL *)artworkURL
{
    if (_vlcInputItem) {
        char *p_artworkURL = input_item_GetArtworkURL(_vlcInputItem);
        if (p_artworkURL) {
            NSString *artworkURLString = toNSStr(p_artworkURL);
            FREENULL(p_artworkURL);
            return [NSURL URLWithString:artworkURLString];
        }
    }
    return nil;
}

- (void)parseInputItem
{
    _p_parserID = input_item_Parse(_vlcInputItem,
                                   (vlc_object_t *)getIntf(),
                                   &parserCallbacks,
                                   (__bridge void *) self);
}

- (void)cancelParsing
{
    if (_p_parserID) {
        input_item_parser_id_Interrupt(_p_parserID);
    }
}

- (void)parsingEnded:(int)status
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    if (status) {
        [notificationCenter postNotificationName:VLCInputItemParsingSucceeded object:self];
    } else {
        [notificationCenter postNotificationName:VLCInputItemParsingFailed object:self];
    }
    input_item_parser_id_Release(_p_parserID);
    _p_parserID = NULL;
}

- (BOOL)preparsed
{
    if (_vlcInputItem) {
        return input_item_IsPreparsed(_vlcInputItem);
    }
    return NO;
}

- (void)preparsingEnded:(enum input_item_preparse_status)status
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    switch (status) {
        case ITEM_PREPARSE_SKIPPED:
            [notificationCenter postNotificationName:VLCInputItemPreparsingSkipped object:self];
            break;
        case ITEM_PREPARSE_FAILED:
            [notificationCenter postNotificationName:VLCInputItemPreparsingFailed object:self];
            break;
        case ITEM_PREPARSE_TIMEOUT:
            [notificationCenter postNotificationName:VLCInputItemPreparsingTimeOut object:self];

        case ITEM_PREPARSE_DONE:
        default:
            [notificationCenter postNotificationName:VLCInputItemPreparsingSucceeded object:self];
            break;
    }
}

- (int)preparseInputItem
{
    if (!_vlcInputItem) {
        return VLC_ENOVAR;
    }

    return libvlc_MetadataRequest(vlc_object_instance(getIntf()),
                                  _vlcInputItem,
                                  META_REQUEST_OPTION_SCOPE_ANY |
                                  META_REQUEST_OPTION_FETCH_LOCAL,
                                  &preparseCallbacks,
                                  (__bridge void *)self,
                                  -1, NULL);
}

- (void)subTreeAdded:(input_item_node_t *)p_node
{
    _subTree = p_node;
    [[NSNotificationCenter defaultCenter] postNotificationName:VLCInputItemSubtreeAdded object:self];
}

- (int)writeMetadataToFile
{
    if (!_vlcInputItem) {
        return VLC_ENOVAR;
    }
    return input_item_WriteMeta(VLC_OBJECT(getIntf()), _vlcInputItem);
}

@end

@interface VLCInputNode()
{
    struct input_item_node_t *_p_inputNode;
}
@end

@implementation VLCInputNode

- (instancetype)initWithInputNode:(struct input_item_node_t *)p_inputNode
{
    self = [super init];
    if (self && p_inputNode != NULL) {
        _p_inputNode = p_inputNode;
    }
    return self;
}

- (NSString *)description
{
    NSString *inputItemName;
    if (_p_inputNode->p_item)
        inputItemName = toNSStr(_p_inputNode->p_item->psz_name);
    else
        inputItemName = @"p_item == nil";
    return [NSString stringWithFormat:@"%@: node: %p input name: %@, number of children: %i", NSStringFromClass([self class]), _p_inputNode, inputItemName, self.numberOfChildren];
}

- (VLCInputItem *)inputItem
{
    if (_p_inputNode->p_item) {
        return [[VLCInputItem alloc] initWithInputItem:_p_inputNode->p_item];
    }
    return nil;
}

- (int)numberOfChildren
{
    return _p_inputNode->i_children;
}

- (NSArray<VLCInputNode *> *)children
{
    NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:_p_inputNode->i_children];
    for (int i = 0; i < _p_inputNode->i_children; i++) {
        VLCInputNode *inputNode = [[VLCInputNode alloc] initWithInputNode:_p_inputNode->pp_children[i]];
        if (inputNode) {
            [mutableArray addObject:inputNode];
        }
    }
    return [mutableArray copy];
}

@end
