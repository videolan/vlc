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
#undef VLC_DYNAMIC_PLUGINS

#import <UIKit/UIKit.h>
#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_variables.h>
#include <vlc_plugin.h>

#include <TargetConditionals.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate> {
    @public
    libvlc_instance_t *_libvlc;
    UIWindow *window;
    UIView *subview;

#if TARGET_OS_IOS
    UIPinchGestureRecognizer *_pinchRecognizer;
#endif

    CGRect _pinchRect;
    CGPoint _pinchOrigin;
    CGPoint _pinchPreviousCenter;
}
@end


@implementation AppDelegate
#if TARGET_OS_IOS
- (void)pinchRecognized:(UIPinchGestureRecognizer *)pinchRecognizer
{
    UIGestureRecognizerState state = [pinchRecognizer state];

    switch (state)
    {
        case UIGestureRecognizerStateBegan:
            _pinchRect = [subview frame];
            _pinchOrigin = [pinchRecognizer locationInView:nil];
            _pinchPreviousCenter = [subview center];
            return;
        case UIGestureRecognizerStateEnded:
            return;
        case UIGestureRecognizerStateChanged:
            break;
        default:
            return;
    }

    CGFloat scale = pinchRecognizer.scale;
    CGRect viewBounds = _pinchRect;
    if (scale >= 1.0 && (viewBounds.size.width == 0 || viewBounds.size.height == 0))
            viewBounds.size.width = viewBounds.size.height = 1;
    viewBounds.size.width *= scale;
    viewBounds.size.height *= scale;
    subview.frame = viewBounds;
    CGPoint newPosition = [pinchRecognizer locationInView:nil];
    subview.center = CGPointMake(
            _pinchPreviousCenter.x + newPosition.x - _pinchOrigin.x,
            _pinchPreviousCenter.y + newPosition.y - _pinchOrigin.y);
}
#endif

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

    subview = [[UIView alloc] initWithFrame:window.bounds];
    subview.backgroundColor = [UIColor blueColor];
    [window addSubview:subview];
    [window makeKeyAndVisible];

#if TARGET_OS_IOS
    _pinchRecognizer = [[UIPinchGestureRecognizer alloc]
        initWithTarget:self action:@selector(pinchRecognized:)];
    [window addGestureRecognizer:_pinchRecognizer];
#endif

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
    assert(d != nil && d->subview != nil);
    var_SetAddress(vlc_object_instance(obj), "drawable-nsobject",
                   (__bridge void *)d->subview);

    return VLC_SUCCESS;
}

#define MODULE_NAME ios_interface
#define MODULE_STRING "ios_interface"
vlc_module_begin()
    set_capability("interface", 0)
    set_callback(Open)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};
