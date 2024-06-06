/*****************************************************************************
 * iosvlc.m: watchOS specific development main executable for VLC media player
 *****************************************************************************
 * Copyright (C) 2020, 2024 Videolabs
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
 *          Alexandre Janniaux <ajanni@videolabs.io>
 *          Felix Paul Kühne <fkuehne@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#undef VLC_DYNAMIC_PLUGINS

#define MODULE_NAME ios_interface
#undef VLC_DYNAMIC_PLUGINS

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_variables.h>
#include <vlc_plugin.h>

#include "../lib/libvlc_internal.h"

#import <WatchKit/WatchKit.h>

@interface InterfaceController : WKInterfaceController

@end

@implementation InterfaceController

- (void)awakeWithContext:(id)context {
    [super awakeWithContext:context];
}

- (void)willActivate {
    [super willActivate];
}

- (void)didDeactivate {
    [super didDeactivate];
}

@end

@interface AppDelegate : NSObject <WKApplicationDelegate> {
    @public
    libvlc_instance_t *_libvlc;
}
@end

@implementation AppDelegate
/* Called after application launch */
- (void)applicationDidFinishLaunching {
    /* Store startup arguments to forward them to libvlc */
    NSArray *arguments = [[NSProcessInfo processInfo] arguments];
    unsigned vlc_argc = [arguments count] - 1;
    const char **vlc_argv = malloc(vlc_argc * sizeof *vlc_argv);
    if (vlc_argv == NULL)
        return;

    for (unsigned i = 0; i < vlc_argc; i++)
        vlc_argv[i] = [[arguments objectAtIndex:i + 1] UTF8String];

    /* Initialize libVLC */
    _libvlc = libvlc_new(vlc_argc, (const char * const*)vlc_argv);
    free(vlc_argv);

    if (_libvlc == NULL)
        return;

    /* Start glue interface, see code below */
    libvlc_InternalAddIntf(_libvlc->p_libvlc_int, "ios_interface,none");

    /* Start parsing arguments and eventual playback */
    libvlc_InternalPlay(_libvlc->p_libvlc_int);
}

- (Class)applicationRootInterfaceControllerClass {
    return [InterfaceController class];
}
@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return WKApplicationMain(argc, argv, NSStringFromClass([AppDelegate class]));
    }
}

/* Glue interface code */
static int Open(vlc_object_t *obj)
{
    return VLC_SUCCESS;
}

#include <vlc_stream.h>
#include <vlc_access.h>

static int OpenAssetDemux(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;

    if (access->psz_location == NULL)
        return VLC_EGENERIC;

    /* Store startup arguments to forward them to libvlc */
    NSString *bundle_path = [[NSBundle mainBundle] resourcePath];
    const char *resource_path = [bundle_path UTF8String];
    size_t resource_path_length = strlen(resource_path);

    char *url;
    if (asprintf(&url, "file://%s/%s", resource_path, access->psz_location) < 0)
        return VLC_ENOMEM;

    access->psz_url = url;

    return VLC_ACCESS_REDIRECT;
}

vlc_module_begin()
    set_capability("interface", 0)
    set_callback(Open)

    add_submodule()
    set_capability("access", 1)
    set_callback(OpenAssetDemux)
    add_shortcut("asset")
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};
