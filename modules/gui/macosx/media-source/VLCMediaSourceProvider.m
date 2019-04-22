/*****************************************************************************
 * VLCMediaSourceProvider.m: MacOS X interface module
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

#import "VLCMediaSourceProvider.h"

#import "media-source/VLCMediaSource.h"
#import "main/VLCMain.h"

#import <vlc_media_source.h>

@implementation VLCMediaSourceProvider

+ (NSArray <VLCMediaSource *> *)listOfMediaSourcesForCategory:(enum services_discovery_category_e)category
{
    libvlc_int_t *p_libvlcInstance = vlc_object_instance(getIntf());
    vlc_media_source_provider_t *p_sourceProvider = vlc_media_source_provider_Get(p_libvlcInstance);

    if (p_sourceProvider == NULL) {
        return @[];
    }

    vlc_media_source_meta_list_t *p_sourceMetaList = vlc_media_source_provider_List(p_sourceProvider,
                                                                                    category);

    size_t count = vlc_media_source_meta_list_Count(p_sourceMetaList);
    NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:count];

    for (size_t x = 0; x < count; x++) {
        struct vlc_media_source_meta *p_sourceMetaItem = vlc_media_source_meta_list_Get(p_sourceMetaList, x);

        vlc_media_source_t *p_mediaSource = vlc_media_source_provider_GetMediaSource(p_sourceProvider, p_sourceMetaItem->name);
        if (p_mediaSource == NULL) {
            continue;
        }

        VLCMediaSource *mediaSource = [[VLCMediaSource alloc] initWithMediaSource:p_mediaSource andLibVLCInstance:p_libvlcInstance];
        [mutableArray addObject:mediaSource];
    }

    vlc_media_source_meta_list_Delete(p_sourceMetaList);
    return [mutableArray copy];
}

@end
