//
//  VLCExtensionsManager.m
//  VLCKit
//
//  Created by Pierre d'Herbemont on 1/26/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import "VLCExtensionsManager.h"
#import "VLCExtension.h"
#import "VLCLibrary.h"
#import "VLCLibVLCBridging.h"
#import <vlc_extensions.h>
#import <vlc_input.h>

#define _instance ((extensions_manager_t *)instance)

@implementation VLCExtensionsManager
static VLCExtensionsManager *sharedManager = nil;

+ (VLCExtensionsManager *)sharedManager
{
    if (!sharedManager)
    {
        /* Initialize a shared instance */
        sharedManager = [[self alloc] init];
    }
    return sharedManager;
}

- (void)dealloc
{
    vlc_object_t *libvlc = libvlc_get_vlc_instance([VLCLibrary sharedInstance]);
    vlc_object_release(libvlc);
    module_unneed(_instance, _instance->p_module);
    vlc_object_release(_instance);

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [_extensions release];
    [super dealloc];
}

- (NSArray *)extensions
{
    if (!instance)
    {
        vlc_object_t *libvlc = libvlc_get_vlc_instance([VLCLibrary sharedInstance]);
        instance = vlc_object_create(libvlc, sizeof(extensions_manager_t));
        if (!_instance)
        {
            vlc_object_release(libvlc);
            return nil;
        }
        vlc_object_attach(_instance, libvlc);

        _instance->p_module = module_need(_instance, "extension", NULL, false);
        NSAssert(_instance->p_module, @"Unable to load extensions module");

        vlc_object_release(libvlc);
    }

    if (_extensions)
        return _extensions;
    _extensions = [[NSMutableArray alloc] init];
    extension_t *ext;
    FOREACH_ARRAY(ext, _instance->extensions)
        VLCExtension *extension = [[VLCExtension alloc] initWithInstance:ext];
        [_extensions addObject:extension];
        [extension release];
    FOREACH_END()
    return _extensions;
}

- (void)runExtension:(VLCExtension *)extension
{
    extension_t *ext = [extension instance];

    if(extension_TriggerOnly(_instance, ext))
        extension_Trigger(_instance, ext);
    else
    {
        if(!extension_IsActivated(_instance, ext))
            extension_Activate(_instance, ext);
    }
}

- (void)mediaPlayerLikelyChangedInput
{
    input_thread_t *input = _player ? libvlc_media_player_get_input_thread([_player libVLCMediaPlayer]) : NULL;

    // Don't send more than appropriate
    if (_previousInput == input)
        return;
    _previousInput = input;

    for(VLCExtension *extension in _extensions)
        extension_SetInput(_instance, [extension instance], input);
    if (input)
        vlc_object_release(input);
}

- (void)setMediaPlayer:(VLCMediaPlayer *)player
{
    if (_player == player)
        return;
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center removeObserver:self name:VLCMediaPlayerStateChanged object:_player];

    [_player release];
    _player = [player retain];

    [self mediaPlayerLikelyChangedInput];


    if (player)
        [center addObserver:self selector:@selector(mediaPlayerLikelyChangedInput) name:VLCMediaPlayerStateChanged object:_player];
}

- (VLCMediaPlayer *)mediaPlayer
{
    return _player;
}
@end
