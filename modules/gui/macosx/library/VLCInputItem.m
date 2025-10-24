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

#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryController.h"

#import <vlc_url.h>
#import <vlc_preparser.h>

NSString *VLCInputItemParsingSucceeded = @"VLCInputItemParsingSucceeded";
NSString *VLCInputItemParsingFailed = @"VLCInputItemParsingFailed";
NSString *VLCInputItemSubtreeAdded = @"VLCInputItemSubtreeAdded";
NSString * const VLCInputItemCommonDataDifferingFlagString = @"<differing>";

@interface VLCInputItem()
{
    input_item_parser_id_t *_p_parserID;
}

- (void)parsingEnded:(int)status;
- (void)subTreeAdded:(input_item_node_t *)p_node;

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
    input_item_node_Delete(p_node);
}

static const struct input_item_parser_cbs_t parserCallbacks =
{
    .on_ended = cb_parsing_ended,
    .on_subtree_added = cb_subtree_added,
};

@implementation VLCInputItem

+ (nullable instancetype)inputItemFromURL:(NSURL *)url
{
    const char * const psz_uri = url.absoluteString.UTF8String;
    const char * const psz_name = url.lastPathComponent.stringByDeletingPathExtension.UTF8String;
    input_item_t * const p_input_item = input_item_New(psz_uri, psz_name);
    if (p_input_item == NULL)
        return nil;
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:p_input_item];
    input_item_Release(p_input_item);
    return inputItem;
}

- (nullable instancetype)initWithInputItem:(struct input_item_t *)p_inputItem
{
    self = [super init];
    if (self && p_inputItem != NULL) {
        _vlcInputItem = input_item_Hold(p_inputItem);
    } else {
        return nil;
    }
    return self;
}

- (void)dealloc
{
    if (_p_parserID)
        input_item_parser_id_Release(_p_parserID);
    if (_vlcInputItem)
        input_item_Release(_vlcInputItem);
}

- (NSString *)name
{
    return toNSStr(_vlcInputItem->psz_name);
}
- (void)setName:(NSString *)name
{
    input_item_SetName(_vlcInputItem, [name UTF8String]);
}

- (NSString *)title
{
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
    input_item_SetTitle(_vlcInputItem, [title UTF8String]);
}

- (NSString *)artist
{
    char *psz_artist = input_item_GetArtist(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_artist);
    FREENULL(psz_artist);
    return returnValue;
}
- (void)setArtist:(NSString *)artist
{
    input_item_SetArtist(_vlcInputItem, [artist UTF8String]);
}

- (NSString *)album
{
    char *psz_album = input_item_GetAlbum(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_album);
    FREENULL(psz_album);
    return returnValue;
}
- (void)setAlbum:(NSString *)albumName
{
    input_item_SetAlbum(_vlcInputItem, [albumName UTF8String]);
}

- (NSString *)trackNumber
{
    char *psz_trackNumber = input_item_GetTrackNumber(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_trackNumber);
    FREENULL(psz_trackNumber);
    return returnValue;
}
- (void)setTrackNumber:(NSString *)trackNumber
{
    input_item_SetTrackNumber(_vlcInputItem, [trackNumber UTF8String]);
}

- (NSString *)genre
{
    char *psz_genre = input_item_GetGenre(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_genre);
    FREENULL(psz_genre);
    return returnValue;
}
- (void)setGenre:(NSString *)genre
{
    input_item_SetGenre(_vlcInputItem, [genre UTF8String]);
}

- (NSString *)copyright
{
    char *psz_copyright = input_item_GetCopyright(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_copyright);
    FREENULL(psz_copyright);
    return returnValue;
}
- (void)setCopyright:(NSString *)copyright
{
    input_item_SetCopyright(_vlcInputItem, [copyright UTF8String]);
}

- (NSString *)publisher
{
    char *psz_publisher = input_item_GetPublisher(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_publisher);
    FREENULL(psz_publisher);
    return returnValue;
}
- (void)setPublisher:(NSString *)publisher
{
    input_item_SetPublisher(_vlcInputItem, [publisher UTF8String]);
}

- (NSString *)nowPlaying
{
    char *psz_nowPlaying = input_item_GetNowPlaying(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_nowPlaying);
    FREENULL(psz_nowPlaying);
    return returnValue;
}

- (NSString *)language
{
    char *psz_language = input_item_GetLanguage(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_language);
    FREENULL(psz_language);
    return returnValue;
}
- (void)setLanguage:(NSString *)language
{
    input_item_SetLanguage(_vlcInputItem, [language UTF8String]);
}

- (NSString *)date
{
    char *psz_date = input_item_GetDate(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_date);
    FREENULL(psz_date);
    return returnValue;
}
- (void)setDate:(NSString *)date
{
    input_item_SetDate(_vlcInputItem, [date UTF8String]);
}

- (NSString *)contentDescription
{
    char *psz_description = input_item_GetDescription(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_description);
    FREENULL(psz_description);
    return returnValue;
}
- (void)setContentDescription:(NSString *)contentDescription
{
    input_item_SetDescription(_vlcInputItem, [contentDescription UTF8String]);
}

- (NSString *)encodedBy
{
    char *psz_encodedBy = input_item_GetEncodedBy(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_encodedBy);
    FREENULL(psz_encodedBy);
    return returnValue;
}

- (NSString *)trackID
{
    char *psz_trackID = input_item_GetTrackID(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_trackID);
    FREENULL(psz_trackID);
    return returnValue;
}

- (NSString *)trackTotal
{
    char *psz_trackTotal = input_item_GetTrackTotal(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_trackTotal);
    FREENULL(psz_trackTotal);
    return returnValue;
}

- (NSString *)director
{
    char *psz_director = input_item_GetDirector(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_director);
    FREENULL(psz_director);
    return returnValue;
}
- (void)setDirector:(NSString *)director
{
    input_item_SetDirector(_vlcInputItem, [director UTF8String]);
}

- (NSString *)season
{
    char *psz_season = input_item_GetSeason(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_season);
    FREENULL(psz_season);
    return returnValue;
}

- (NSString *)episode
{
    char *psz_episode = input_item_GetEpisode(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_episode);
    FREENULL(psz_episode);
    return returnValue;
}

- (NSString *)showName
{
    char *psz_showName = input_item_GetShowName(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_showName);
    FREENULL(psz_showName);
    return returnValue;
}
- (void)setShowName:(NSString *)showName
{
    input_item_SetShowName(_vlcInputItem, [showName UTF8String]);
}

- (NSString *)actors
{
    char *psz_actors = input_item_GetActors(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_actors);
    FREENULL(psz_actors);
    return returnValue;
}
- (void)setActors:(NSString *)actors
{
    input_item_SetActors(_vlcInputItem, [actors UTF8String]);
}

- (NSString *)discNumber
{
    char *psz_discNumber = input_item_GetDiscNumber(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_discNumber);
    FREENULL(psz_discNumber);
    return returnValue;
}

- (NSString *)totalNumberOfDiscs
{
    char *psz_totalDiscNumber = input_item_GetDiscTotal(_vlcInputItem);
    NSString *returnValue = toNSStr(psz_totalDiscNumber);
    FREENULL(psz_totalDiscNumber);
    return returnValue;
}

- (NSString *)MRL
{
    return toNSStr(_vlcInputItem->psz_uri);
}

- (NSString *)decodedMRL
{
    char *psz_url = vlc_uri_decode(input_item_GetURI(_vlcInputItem));
    NSString *returnValue = toNSStr(psz_url);
    FREENULL(psz_url);
    return returnValue;
}

- (NSString*)path
{
    if (!_vlcInputItem->b_net) {
        char *psz_url = input_item_GetURI(_vlcInputItem);
        if (!psz_url) {
            return @"";
        }

        char *psz_path = vlc_uri2path(psz_url);
        NSString *path = toNSStr(psz_path);
        free(psz_url);
        free(psz_path);

        return path;
    }

    return @"";
}

- (vlc_tick_t)duration
{
    return _vlcInputItem->i_duration;
}

- (enum input_item_type_e)inputType
{
    return _vlcInputItem->i_type;
}

- (NSURL *)artworkURL
{
    char *p_artworkURL = input_item_GetArtworkURL(_vlcInputItem);
    if (p_artworkURL) {
        NSString *artworkURLString = toNSStr(p_artworkURL);
        FREENULL(p_artworkURL);
        return [NSURL URLWithString:artworkURLString];
    }
    return nil;
}

- (void)setArtworkURL:(NSURL *)artworkURL
{

    if (artworkURL != nil) {
        input_item_SetArtworkURL(_vlcInputItem, artworkURL.absoluteString.UTF8String);
    } else {
        input_item_SetArtworkURL(_vlcInputItem, NULL);
    }
}

- (void)parseInputItem
{
    const struct input_item_parser_cfg cfg = {
        .cbs = &parserCallbacks,
        .cbs_data = (__bridge void *) self,
    };

    if (_p_parserID)
        input_item_parser_id_Release(_p_parserID);
    _p_parserID = input_item_Parse(VLC_OBJECT(getIntf()), _vlcInputItem, &cfg);
}

- (void)cancelParsing
{
    if (_p_parserID) {
        input_item_parser_id_Interrupt(_p_parserID);
    }
}

- (void)parsingEnded:(int)status
{
    NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
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
    return input_item_IsPreparsed(_vlcInputItem);
}

- (BOOL)isStream
{
    return (BOOL)_vlcInputItem->b_net;
}

- (void)subTreeAdded:(input_item_node_t *)p_node
{
    _subTree = p_node;
    [NSNotificationCenter.defaultCenter postNotificationName:VLCInputItemSubtreeAdded object:self];
}

- (int)writeMetadataToFile
{
    return input_item_WriteMeta(VLC_OBJECT(getIntf()), _vlcInputItem);
}

- (void)thumbnailWithSize:(NSSize)size completionHandler:(void(^)(NSImage * image))completionHandler
{
    if (self.isStream) {
        completionHandler(nil);
        return;
    }

    char * const psz_url = input_item_GetURI(_vlcInputItem);
    if (psz_url == NULL) {
        completionHandler(nil);
        return;
    }

    char * const psz_path = vlc_uri2path(psz_url);
    free(psz_url);
    if (psz_path == NULL) {
        completionHandler(nil);
        return;
    }

    NSString * const path = toNSStr(psz_path);
    free(psz_path);

    [NSImage quickLookPreviewForLocalPath:path 
                                 withSize:size
                        completionHandler:^(NSImage * image) {
        if (image) {
            completionHandler(image);
            return;
        }

        NSImage * const workspaceImage = [NSWorkspace.sharedWorkspace iconForFile:path];
        if (workspaceImage) {
            image.size = size;
            completionHandler(image);
            return;
        }

        completionHandler(nil);
    }];
}

- (void)moveToTrash
{
    if (self.isStream) {
        return;
    }

    NSURL * const pathUrl = [NSURL URLWithString:self.path];
    if (pathUrl == nil) {
        return;
    }

    [NSFileManager.defaultManager trashItemAtURL:pathUrl
                                resultingItemURL:nil
                                           error:nil];
    
    VLCLibraryController * const libraryController = VLCMain.sharedInstance.libraryController;
    [libraryController reloadMediaLibraryFoldersForInputItems:@[self]];
}

- (void)revealInFinder
{
    if (self.isStream) {
        return;
    }

    NSURL *pathUrl = [NSURL URLWithString:self.path];
    if (pathUrl == nil) {
        return;
    }

    [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[pathUrl]];
}

- (nullable NSArray<NSString *> *)options
{
    const int i_options = _vlcInputItem->i_options;
    NSMutableArray * const options = [NSMutableArray arrayWithCapacity:i_options];
    for (NSUInteger i = 0; i < i_options; ++i) {
        const char * const psz_option = _vlcInputItem->ppsz_options[i];
        NSString * const option = NSTR(psz_option);
        [options addObject:option];
    }
    return options.copy;
}

@end


NSDictionary<NSString *, id> *commonInputItemData(NSArray<VLCInputItem *> * const inputItems)
{
    if (inputItems.count == 0) {
        return @{};
    }

    VLCInputItem * const firstInputItem = inputItems.firstObject;

    if (inputItems.count == 1) {
        return @{@"inputItem": firstInputItem};
    }

    NSMutableDictionary<NSString *, id> *const commonData = [[NSMutableDictionary alloc] init];

#define PERFORM_ACTION_PER_INPUTITEM_NSSTRING_PROP(action)                                  \
action(MRL);                                                                                \
action(decodedMRL);                                                                         \
action(title);                                                                              \
action(artist);                                                                             \
action(album);                                                                              \
action(trackNumber);                                                                        \
action(trackTotal);                                                                         \
action(genre);                                                                              \
action(date);                                                                               \
action(episode);                                                                            \
action(actors);                                                                             \
action(director);                                                                           \
action(showName);                                                                           \
action(copyright);                                                                          \
action(publisher);                                                                          \
action(nowPlaying);                                                                         \
action(language);                                                                           \
action(contentDescription);                                                                 \
action(encodedBy);

#define PERFORM_ACTION_PER_INPUTITEM_PROP(action)                                           \
PERFORM_ACTION_PER_INPUTITEM_NSSTRING_PROP(action)                                          \
action(artworkURL);

#define CREATE_DIFFER_BOOL(prop)                                                            \
BOOL differing_##prop = NO;

#define CREATE_PROP_VAR(prop)                                                               \
NSString * const firstItem_##prop = firstInputItem.prop;

#define UPDATE_IF_DIFFERING_BOOL(prop)                                                          \
differing_##prop = differing_##prop || ![inputItem.prop isEqualToString:firstItem_##prop];

#define ADD_PROP_TO_DICT(prop)                                                              \
NSString * firstItemValue_##prop = firstItem_##prop == nil ? @"" : firstItem_##prop;        \
NSString * const value_##prop =                                                             \
    differing_##prop ? VLCInputItemCommonDataDifferingFlagString : firstItemValue_##prop;   \
[commonData setObject:value_##prop forKey:[NSString stringWithUTF8String:#prop]];           \

    PERFORM_ACTION_PER_INPUTITEM_PROP(CREATE_DIFFER_BOOL);
    PERFORM_ACTION_PER_INPUTITEM_NSSTRING_PROP(CREATE_PROP_VAR);
    // Since artworkURL is a URL, we have to handle it differently
    NSString * const firstItem_artworkURL = firstInputItem.artworkURL.absoluteString;

    // Skip first item
    for (uint i = 1; i < inputItems.count; ++i) {
        VLCInputItem * const inputItem = inputItems[i];

        PERFORM_ACTION_PER_INPUTITEM_NSSTRING_PROP(UPDATE_IF_DIFFERING_BOOL);
        NSString * const inputItem_artworkURL = inputItem.artworkURL.absoluteString;
        differing_artworkURL |= ![inputItem_artworkURL isEqualToString:firstItem_artworkURL];
    }

    PERFORM_ACTION_PER_INPUTITEM_PROP(ADD_PROP_TO_DICT);

#undef PERFORM_ACTION_PER_INPUTITEM_PROP
#undef CREATE_DIFFER_BOOL
#undef CREATE_PROP_VAR
#undef SET_IF_DIFFERING
#undef ADD_PROP_TO_DICT_IF_DIFFERING

    return [commonData copy];
}


@implementation VLCInputNode

- (instancetype)initWithInputNode:(struct input_item_node_t *)p_inputNode
{
    self = [super init];
    if (self && p_inputNode != NULL) {
        _vlcInputItemNode = p_inputNode;

        if (_vlcInputItemNode->p_item) {
            _inputItem = [[VLCInputItem alloc] initWithInputItem:_vlcInputItemNode->p_item];
        }
    }
    return self;
}

- (NSString *)description
{
    NSString *inputItemName;
    if (_vlcInputItemNode && _vlcInputItemNode->p_item)
        inputItemName = toNSStr(_vlcInputItemNode->p_item->psz_name);
    else
        inputItemName = @"p_item == nil";
    return [NSString stringWithFormat:@"%@: node: %p input name: %@, number of children: %i", NSStringFromClass([self class]),_vlcInputItemNode, inputItemName, self.numberOfChildren];
}

- (int)numberOfChildren
{
    return _vlcInputItemNode ? _vlcInputItemNode->i_children : 0;
}

- (nullable NSArray<VLCInputNode *> *)children
{
    if (_vlcInputItemNode == NULL) {
        return nil;
    }
    NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:_vlcInputItemNode->i_children];
    for (int i = 0; i < _vlcInputItemNode->i_children; i++) {
        VLCInputNode *inputNode = [[VLCInputNode alloc] initWithInputNode:_vlcInputItemNode->pp_children[i]];
        if (inputNode) {
            [mutableArray addObject:inputNode];
        }
    }
    return [mutableArray copy];
}

@end
