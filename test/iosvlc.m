/*****************************************************************************
 * iosvlc.m: iOS specific development main executable for VLC media player
 *****************************************************************************
 * Copyright (C) 2020 Videolabs
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
 *          Alexandre Janniaux <ajanni@videolabs.io>
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

#import <UIKit/UIKit.h>
#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_variables.h>
#include <vlc_plugin.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate> {
    @public
    libvlc_instance_t *_libvlc;
    UIWindow *window;
}
@end


@implementation AppDelegate
/* Called after application launch */
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    /* Set VLC_PLUGIN_PATH for dynamic loading */
    NSString *pluginsDirectory = [[NSBundle mainBundle] privateFrameworksPath];
    setenv("VLC_PLUGIN_PATH", [pluginsDirectory UTF8String], 1);

    /* Store startup arguments to forward them to libvlc */
    NSArray *arguments = [[NSProcessInfo processInfo] arguments];
    unsigned vlc_argc = [arguments count];
    const char **vlc_argv = malloc(vlc_argc * sizeof *vlc_argv);
    if (vlc_argv == NULL)
        return NO;

    for (unsigned i = 0; i < vlc_argc; i++)
         vlc_argv[i] = [[arguments objectAtIndex:i] UTF8String];

    /* Initialize libVLC */
    _libvlc = libvlc_new(vlc_argc, (const char * const*)vlc_argv);
    free(vlc_argv);

    if (_libvlc == NULL)
        return NO;

    /* Initialize main window */
    window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    window.rootViewController = [UIViewController alloc];
    window.backgroundColor = [UIColor whiteColor];
    [window makeKeyAndVisible];

    /* Start glue interface, see code below */
    libvlc_add_intf(_libvlc, "ios_interface,none");

    /* Start parsing arguments and eventual playback */
    libvlc_playlist_play(_libvlc);

    return YES;
}
@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}

/* Glue interface code, define drawable-nsobject for display module */
static int Open(vlc_object_t *obj)
{
    AppDelegate *d = (AppDelegate *)[[UIApplication sharedApplication] delegate];
    assert(d != nil && d->window != nil);
    var_SetAddress(vlc_object_instance(obj), "drawable-nsobject", d->window);

    return VLC_SUCCESS;
}

#define MODULE_NAME ios_interface
#define MODULE_STRING "ios_interface"
vlc_module_begin()
    set_capability("interface", 0)
    set_callback(Open)
vlc_module_end()

/* Inject the glue interface as a static module */
typedef int (*vlc_plugin_cb)(vlc_set_cb, void*);

__attribute__((visibility("default")))
vlc_plugin_cb vlc_static_modules[] = { vlc_entry__ios_interface, NULL };
