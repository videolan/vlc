/*****************************************************************************
 * VLCMediaSource.m: MacOS X interface module
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

#import "VLCMediaSource.h"

#import "library/VLCInputItem.h"

#import "extensions/NSString+Helpers.h"

@interface VLCMediaSource ()
{
    BOOL _respondsToDiskChanges;
    libvlc_int_t *_p_libvlcInstance;
    vlc_media_source_t *_p_mediaSource;
    vlc_media_tree_listener_id *_p_treeListenerID;
}
@end

NSString *VLCMediaSourceChildrenReset = @"VLCMediaSourceChildrenReset";
NSString *VLCMediaSourceChildrenAdded = @"VLCMediaSourceChildrenAdded";
NSString *VLCMediaSourceChildrenRemoved = @"VLCMediaSourceChildrenRemoved";
NSString *VLCMediaSourcePreparsingEnded = @"VLCMediaSourcePreparsingEnded";

static void cb_children_reset(vlc_media_tree_t *p_tree,
                              input_item_node_t *p_node,
                              void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCMediaSource *mediaSource = (__bridge VLCMediaSource *)p_data;
        [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourceChildrenReset
                                                            object:mediaSource];
    });
}

static void cb_children_added(vlc_media_tree_t *p_tree,
                              input_item_node_t *p_node,
                              input_item_node_t *const p_children[],
                              size_t count,
                              void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCMediaSource *mediaSource = (__bridge VLCMediaSource *)p_data;
        [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourceChildrenAdded
                                                            object:mediaSource];
    });
}

static void cb_children_removed(vlc_media_tree_t *p_tree,
                                input_item_node_t *p_node,
                                input_item_node_t *const p_children[],
                                size_t count,
                                void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCMediaSource *mediaSource = (__bridge VLCMediaSource *)p_data;
        [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourceChildrenRemoved
                                                            object:mediaSource];
    });
}

static void cb_preparse_ended(vlc_media_tree_t *p_tree,
                              input_item_node_t *p_node,
                              enum input_item_preparse_status status,
                              void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCMediaSource *mediaSource = (__bridge VLCMediaSource *)p_data;
        [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourcePreparsingEnded
                                                            object:mediaSource];
    });
}

static const struct vlc_media_tree_callbacks treeCallbacks = {
    cb_children_reset,
    cb_children_added,
    cb_children_removed,
    cb_preparse_ended,
};

static const char *const localDevicesDescription = "My Machine";

#pragma mark - VLCMediaSource methods
@implementation VLCMediaSource

- (instancetype)initForLocalDevices:(libvlc_int_t *)p_libvlcInstance
{
    self = [super init];
    if (self) {
        _respondsToDiskChanges = NO;
        _p_libvlcInstance = p_libvlcInstance;
        
        _p_mediaSource = malloc(sizeof(vlc_media_source_t));
        if (!_p_mediaSource) {
            return self;
        }
        
        _p_mediaSource->description = localDevicesDescription;
        _p_mediaSource->tree = calloc(1, sizeof(vlc_media_tree_t));
        
        if (_p_mediaSource->tree == NULL) {
            free(_p_mediaSource);
            _p_mediaSource = NULL;
            return self;
        }
        
        _category = SD_CAT_MYCOMPUTER;
    }
    return self;
}

- (instancetype)initWithMediaSource:(vlc_media_source_t *)p_mediaSource
                  andLibVLCInstance:(libvlc_int_t *)p_libvlcInstance
                        forCategory:(enum services_discovery_category_e)category
{
    self = [super init];
    if (self && p_mediaSource != NULL) {
        _respondsToDiskChanges = NO;
        _p_libvlcInstance = p_libvlcInstance;
        _p_mediaSource = p_mediaSource;
        vlc_media_source_Hold(_p_mediaSource);
        _p_treeListenerID = vlc_media_tree_AddListener(_p_mediaSource->tree,
                                                       &treeCallbacks,
                                                       (__bridge void *)self,
                                                       NO);
        _category = category;
    }
    return self;
}

- (void)dealloc
{
    if (_p_mediaSource != NULL) {
        if (_p_treeListenerID) {
            vlc_media_tree_RemoveListener(_p_mediaSource->tree,
                                          _p_treeListenerID);
        }
        if (_p_mediaSource->description == localDevicesDescription) {
            _p_mediaSource->description = NULL;
            input_item_node_t **childrenNodes = _p_mediaSource->tree->root.pp_children;
            if (childrenNodes) {
                for (int i = 0; i <_p_mediaSource->tree->root.i_children; ++i) {
                    input_item_node_t *childNode = childrenNodes[i];
                    input_item_node_RemoveNode(&(_p_mediaSource->tree->root), childNode);
                    input_item_node_Delete(childNode);
                }
            }
            
            _p_mediaSource->description = NULL;
            
            free(_p_mediaSource->tree);
            free(_p_mediaSource);
            _p_mediaSource = NULL;
        } else {
            vlc_media_source_Release(_p_mediaSource);
        }
    }
    if (_respondsToDiskChanges) {
        [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
    }
}

- (void)preparseInputNodeWithinTree:(VLCInputNode *)inputNode
{
    if(!inputNode) {
        NSLog(@"Could not preparese input node, is null.");
        return;
    }

    if (_p_mediaSource->description == localDevicesDescription) {
        [self generateLocalDevicesTree];
    }

    if (inputNode == nil || inputNode.inputItem == nil) {
        return;
    }

    if (inputNode.inputItem.inputType == ITEM_TYPE_DIRECTORY) {
        input_item_node_t *vlcInputNode = inputNode.vlcInputItemNode;
        NSURL *dirUrl = [NSURL URLWithString:inputNode.inputItem.MRL];

        [self clearChildNodesForNode:vlcInputNode]; // Clear existing nodes, refresh
        [self generateChildNodesForDirectoryNode:vlcInputNode withUrl:dirUrl];
        return;
    }

    vlc_media_tree_Preparse(_p_mediaSource->tree, _p_libvlcInstance, inputNode.inputItem.vlcInputItem, NULL);
}

- (void)clearChildNodesForNode:(nonnull input_item_node_t*)inputNode
{
    NSAssert(inputNode != NULL, @"Could not clear child nodes for input node as node is null");

    while(inputNode->i_children > 0) {
        input_item_node_t *childNode = inputNode->pp_children[0];
        input_item_node_RemoveNode(inputNode, childNode);
        input_item_node_Delete(childNode);
    }
}

- (void)generateLocalDevicesTree
{
    if (_p_mediaSource->tree->root.i_children > 0) {
        return;
    }
    
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), ^{
        NSArray<NSURL *> *mountedVolumesUrls = [[NSFileManager defaultManager]
                                                mountedVolumeURLsIncludingResourceValuesForKeys:@[NSURLVolumeIsEjectableKey, NSURLVolumeIsRemovableKey]
                                                                                        options:NSVolumeEnumerationSkipHiddenVolumes];
        
        NSURL *homeDirectoryURL = [NSURL fileURLWithPath:NSHomeDirectoryForUser(NSUserName())];
        NSString *homeDirectoryDescription = [NSString stringWithFormat:@"%@'s home", NSUserName()];
        
        if (homeDirectoryURL) {
            input_item_t *homeDirItem = input_item_NewExt(homeDirectoryURL.absoluteString.UTF8String, homeDirectoryDescription.UTF8String, 0, ITEM_TYPE_DIRECTORY, ITEM_LOCAL);
            if (homeDirItem != NULL) {
                input_item_node_t *homeDirNode = input_item_node_Create(homeDirItem);
                if (homeDirNode) {
                    input_item_node_AppendNode(&(self->_p_mediaSource->tree->root), homeDirNode);
                }
                input_item_Release(homeDirItem);
                homeDirItem = NULL;
            }
        }
        
        for (NSURL *url in mountedVolumesUrls) {
            NSNumber *isVolume;
            NSNumber *isEjectable;
            NSNumber *isInternal;
            NSNumber *isLocal;

            NSString *localizedDescription;

            BOOL const getKeyResult = [url getResourceValue:&isVolume forKey:NSURLIsVolumeKey error:nil];
            if (unlikely(!getKeyResult || !isVolume.boolValue)) {
                continue;
            }

            [url getResourceValue:&isEjectable forKey:NSURLVolumeIsEjectableKey error:nil];
            [url getResourceValue:&isInternal forKey:NSURLVolumeIsInternalKey error:nil];
            [url getResourceValue:&isLocal forKey:NSURLVolumeIsLocalKey error:nil];
            [url getResourceValue:&localizedDescription forKey:NSURLVolumeLocalizedNameKey error:nil];
            
            const enum input_item_type_e inputType = isEjectable.boolValue ? ITEM_TYPE_DISC : ITEM_TYPE_DIRECTORY;
            const enum input_item_net_type netType = isLocal.boolValue ? ITEM_LOCAL : ITEM_NET;
            
            input_item_t *urlInputItem = input_item_NewExt(url.absoluteString.UTF8String, localizedDescription.UTF8String, 0, inputType, netType);
            if (urlInputItem != NULL) {
                input_item_node_t *urlNode = input_item_node_Create(urlInputItem);
                if (urlNode) {
                    input_item_node_AppendNode(&(self->_p_mediaSource->tree->root), urlNode);
                }
                input_item_Release(urlInputItem);
                urlInputItem = NULL;
            }
        }
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourceChildrenReset object:self];
            
            if (!self->_respondsToDiskChanges) {
                // We register the notifications here, as they are retrieved from the OS.
                // We need to avoid receiving a notification while the array is still being populated.
                NSNotificationCenter *workspaceNotificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
                [workspaceNotificationCenter addObserver:self
                                                selector:@selector(volumeIsMounted:)
                                                    name:NSWorkspaceDidMountNotification
                                                  object:nil];
                [workspaceNotificationCenter addObserver:self
                                                selector:@selector(volumeIsUnmounted:)
                                                    name:NSWorkspaceWillUnmountNotification
                                                  object:nil];
                
                self->_respondsToDiskChanges = YES;
            }
        });
    });
}

- (void)generateChildNodesForDirectoryNode:(input_item_node_t*)directoryNode withUrl:(NSURL*)directoryUrl
{
    if(directoryNode == NULL || directoryUrl == nil) {
        return;
    }

    NSError *error;
    NSArray<NSURL *> *subDirectories = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:directoryUrl
                                                                     includingPropertiesForKeys:@[NSURLIsDirectoryKey]
                                                                                        options:NSDirectoryEnumerationSkipsHiddenFiles | NSDirectoryEnumerationSkipsSubdirectoryDescendants
                                                                                          error:&error];
    if (subDirectories == nil || subDirectories.count == 0 || error) {
        NSLog(@"Failed to get directories: %@.", error);
        return;
    }

    for (NSURL *url in subDirectories) {
        NSNumber *isDirectory;
        NSNumber *isVolume;
        NSNumber *isEjectable;
        NSNumber *isInternal;
        NSNumber *isLocal;

	[url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];
        [url getResourceValue:&isVolume forKey:NSURLIsVolumeKey error:nil];
        [url getResourceValue:&isEjectable forKey:NSURLVolumeIsEjectableKey error:nil];
        [url getResourceValue:&isInternal forKey:NSURLVolumeIsInternalKey error:nil];
        [url getResourceValue:&isLocal forKey:NSURLVolumeIsLocalKey error:nil];
        
        const enum input_item_type_e inputType = isDirectory.boolValue ? isEjectable.boolValue ? ITEM_TYPE_DISC : ITEM_TYPE_DIRECTORY : ITEM_TYPE_FILE;
        const enum input_item_net_type netType = isLocal.boolValue ? ITEM_LOCAL : ITEM_NET;
        
        input_item_t *urlInputItem = input_item_NewExt(url.absoluteString.UTF8String, url.lastPathComponent.UTF8String, 0, inputType, netType);
        if (urlInputItem != NULL) {
            input_item_node_t *urlNode = input_item_node_Create(urlInputItem);
            if (urlNode) {
                input_item_node_AppendNode(directoryNode, urlNode);
            }
            input_item_Release(urlInputItem);
            urlInputItem = NULL;
        }
    }
}

- (NSString *)mediaSourceDescription
{
    if (_p_mediaSource != NULL) {
        return toNSStr(_p_mediaSource->description);
    }
    return @"";
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ — %@", NSStringFromClass([self class]), self.mediaSourceDescription];
}

- (VLCInputNode *)rootNode
{
    VLCInputNode *inputNode = nil;
    if (_p_mediaSource->description == localDevicesDescription) {
        // Since it is a manually constructed tree, we skip the locking
        inputNode = [[VLCInputNode alloc] initWithInputNode:&_p_mediaSource->tree->root];
    } else {
        vlc_media_tree_Lock(_p_mediaSource->tree);
        inputNode = [[VLCInputNode alloc] initWithInputNode:&_p_mediaSource->tree->root];
        vlc_media_tree_Unlock(_p_mediaSource->tree);
    }
    return inputNode;
}

#pragma mark - Local Media Items Methods
- (void)volumeIsMounted:(NSNotification *)aNotification
{
    NSURL *mountedUrl = [aNotification.userInfo valueForKey:NSWorkspaceVolumeURLKey];
    if (mountedUrl == nil) {
        return;
    }
        
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), ^{
        
        NSNumber *isEjectable;
        NSNumber *isLocal;
        NSString *localizedDescription;

        [mountedUrl getResourceValue:&isEjectable forKey:NSURLVolumeIsEjectableKey error:nil];
        [mountedUrl getResourceValue:&isLocal forKey:NSURLVolumeIsLocalKey error:nil];
        [mountedUrl getResourceValue:&localizedDescription forKey:NSURLVolumeLocalizedNameKey error:nil];

        enum input_item_type_e const inputType = isEjectable.boolValue ? ITEM_TYPE_DISC : ITEM_TYPE_DIRECTORY;
        enum input_item_net_type const netType = isLocal.boolValue ? ITEM_LOCAL : ITEM_NET;
        
        input_item_t *urlInputItem = input_item_NewExt(mountedUrl.absoluteString.UTF8String, localizedDescription.UTF8String, 0, inputType, netType);
        if (urlInputItem != NULL) {
            input_item_node_t *urlNode = input_item_node_Create(urlInputItem);
            if (urlNode) {
                input_item_node_AppendNode(&(self->_p_mediaSource->tree->root), urlNode);
            }
            input_item_Release(urlInputItem);
            urlInputItem = NULL;
        }
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourceChildrenAdded
                                                                object:self];
        });
    });
}

- (void)volumeIsUnmounted:(NSNotification *)aNotification
{
    NSURL *unmountedUrl = [aNotification.userInfo valueForKey:NSWorkspaceVolumeURLKey];
    if (unmountedUrl == nil) {
        return;
    }
        
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), ^{
        
        const char *const urlString = unmountedUrl.absoluteString.UTF8String;
        
        input_item_node_t *nodeToRemove = NULL;
        input_item_node_t **childrenNodes = self->_p_mediaSource->tree->root.pp_children;
        
        if (childrenNodes == NULL) {
            return;
        }
        
        for (int i = 0; i < self->_p_mediaSource->tree->root.i_children; ++i) {
            input_item_node_t *childNode = childrenNodes[i];
            if (childNode->p_item == NULL) {
                continue;
            }
            if (strcmp(urlString, childNode->p_item->psz_uri) == 0) {
                nodeToRemove = childNode;
                break;
            }
        }
        
        if (nodeToRemove == NULL) {
            return;
        }
        
        input_item_node_RemoveNode(&(self->_p_mediaSource->tree->root), nodeToRemove);
        input_item_node_Delete(nodeToRemove);
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourceChildrenRemoved
                                                                object:self];
        });
        
    });
}


@end
