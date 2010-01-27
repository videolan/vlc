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

    NSMutableArray *array = [NSMutableArray array];
    extension_t *ext;
    FOREACH_ARRAY(ext, _instance->extensions)
        VLCExtension *extension = [[VLCExtension alloc] initWithInstance:ext];
        [array addObject:extension];
        [extension release];
    FOREACH_END()
    return array;
}

- (void)runExtension:(VLCExtension *)extension
{
    extension_t *ext = [extension instance];
    NSLog(@"extension_TriggerOnly = %d", extension_TriggerOnly(_instance, ext));
    NSLog(@"extension_IsActivated = %d", extension_IsActivated(_instance, ext));

    if(extension_TriggerOnly(_instance, ext))
        extension_Trigger(_instance, ext);
    else
    {
        if(!extension_IsActivated(_instance, ext))
            extension_Activate(_instance, ext);
    }
}
@end
