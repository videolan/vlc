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

#define MODULE_NAME ios_interface
#undef VLC_DYNAMIC_PLUGINS

#import <UIKit/UIKit.h>
#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_variables.h>
#include <vlc_plugin.h>

#include <TargetConditionals.h>

#include "../lib/libvlc_internal.h"

@interface AppDelegate : UIResponder <UIApplicationDelegate> {
    @public
    libvlc_instance_t *_libvlc;
    UIWindow *window;
    UIView *subview;

#if !TARGET_OS_TV
    UIPinchGestureRecognizer *_pinchRecognizer;
#endif

    CGRect _pinchRect;
    CGPoint _pinchOrigin;
    CGPoint _pinchPreviousCenter;
}
@end


@implementation AppDelegate
#if !TARGET_OS_TV
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
    /* Store startup arguments to forward them to libvlc */
    NSArray *arguments = [[NSProcessInfo processInfo] arguments];
    unsigned vlc_argc = [arguments count] - 1;
    const char **vlc_argv = malloc(vlc_argc * sizeof *vlc_argv);
    if (vlc_argv == NULL)
        return NO;

    for (unsigned i = 0; i < vlc_argc; i++)
        vlc_argv[i] = [[arguments objectAtIndex:i + 1] UTF8String];

    /* Initialize libVLC */
    _libvlc = libvlc_new(vlc_argc, (const char * const*)vlc_argv);
    free(vlc_argv);

    if (_libvlc == NULL)
        return NO;

    /* Initialize main window */
#if TARGET_OS_VISION
    /* UIScreen is unavailable so we need create a size on our own */
    window = [[UIWindow alloc] initWithFrame:CGRectMake(0., 0., 1200., 800.)];
#else
    window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
#endif
    window.rootViewController = [[UIViewController alloc] init];
    window.backgroundColor = [UIColor whiteColor];

    subview = [[UIView alloc] initWithFrame:window.bounds];
    subview.backgroundColor = [UIColor blueColor];
    [window addSubview:subview];
    [window makeKeyAndVisible];

#if !TARGET_OS_TV
    _pinchRecognizer = [[UIPinchGestureRecognizer alloc]
        initWithTarget:self action:@selector(pinchRecognized:)];
    [window addGestureRecognizer:_pinchRecognizer];
#endif

    /* Start glue interface, see code below */

    libvlc_InternalAddIntf(_libvlc->p_libvlc_int, "ios_interface,none");

    /* Start parsing arguments and eventual playback */
    libvlc_InternalPlay(_libvlc->p_libvlc_int);

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
