/*****************************************************************************
 * coregraphicslayer.m: CoreGraphics video output for NPAPI plugins
 *****************************************************************************
 * Copyright (C) 2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_vout_display.h>
#import <vlc_picture_pool.h>

#import <QuartzCore/QuartzCore.h>
#import <AppKit/AppKit.h>

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_description(N_("CoreGraphics video output"))
    set_shortname("CoreGraphics")

    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 0)

    set_callbacks(Open, Close)
vlc_module_end ()

@protocol VLCCoreGraphicsVideoLayerEmbedding <NSObject>
- (void)addVoutLayer:(CALayer *)aLayer;
- (void)removeVoutLayer:(CALayer *)aLayer;
- (CGSize)currentOutputSize;
@end

@interface VLCCoreGraphicsLayer : CALayer {
    CGImageRef _lastFrame;
    bool lock;
    vout_display_t *_vd;
}
@property (nonatomic) vout_display_t *vd;
- (void)setLastFrame:(CGImageRef)lastFrame;
@end

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct vout_display_sys_t {
    picture_pool_t *pool;
    picture_resource_t resource;

    VLCCoreGraphicsLayer *cgLayer;

    vout_window_t *embed;
    bool has_first_frame;

    size_t componentsPerPixel;
    size_t bitsPerComponent;
    size_t bitsPerPixel;

    CALayer <VLCCoreGraphicsVideoLayerEmbedding> *container;
    CGColorSpaceRef colorspace;
};
static picture_pool_t *Pool(vout_display_t *, unsigned);
static void Display(vout_display_t *, picture_t *, subpicture_t *);
static int Control(vout_display_t *, int, va_list);

/*****************************************************************************
 * OpenVideo: activates dummy vout display method
 *****************************************************************************/
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = calloc(1, sizeof(*sys));
    NSAutoreleasePool *nsPool = nil;

    if (!sys)
        return VLC_ENOMEM;
    sys->pool = NULL;

    id container = var_CreateGetAddress(vd, "drawable-nsobject");
    if (container)
        vout_display_DeleteWindow(vd, NULL);
    else {
        vout_window_cfg_t wnd_cfg;

        memset(&wnd_cfg, 0, sizeof(wnd_cfg));
        wnd_cfg.type = VOUT_WINDOW_TYPE_NSOBJECT;
        wnd_cfg.x = var_InheritInteger(vd, "video-x");
        wnd_cfg.y = var_InheritInteger(vd, "video-y");
        wnd_cfg.height = vd->cfg->display.height;
        wnd_cfg.width = vd->cfg->display.width;

        sys->embed = vout_display_NewWindow(vd, &wnd_cfg);
        if (sys->embed)
            container = sys->embed->handle.nsobject;

        if (!container) {
            msg_Err(vd, "No drawable-nsobject found!");
            goto bailout;
        }
    }

    /* store for later, released in Close() */
    sys->container = [container retain];

    /* allocate pool */
    nsPool = [[NSAutoreleasePool alloc] init];

    if ([container respondsToSelector:@selector(addVoutLayer:)]) {
        msg_Dbg(vd, "container implements implicit protocol");
        sys->cgLayer = [[VLCCoreGraphicsLayer alloc] init];
        [container addVoutLayer:sys->cgLayer];
    } else if ([container respondsToSelector:@selector(addSublayer:)] || [container isKindOfClass:[CALayer class]]) {
        msg_Dbg(vd, "container doesn't implement implicit protocol, fallback mode used");
        sys->cgLayer = [[VLCCoreGraphicsLayer alloc] init];
        [container addSublayer:sys->cgLayer];
    } else {
        msg_Err(vd, "Provided NSObject container isn't compatible");
        goto bailout;
    }
    [sys->cgLayer setVd:vd];

    /* setup output format */
    video_format_t fmt = vd->fmt;

    char *chroma = "RGBA";
    fmt.i_chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, chroma);
    msg_Dbg(vd, "forcing chroma 0x%.8x (%4.4s)", fmt.i_chroma, (char*)&fmt.i_chroma);

    video_format_FixRgb(&fmt);
    msg_Dbg(vd, "will use pixel format %4.4s", (char*)&fmt.i_chroma);

    /* setup vout display */
    vout_display_info_t info = vd->info;
    info.has_hide_mouse = false;
    vd->sys = sys;
    vd->fmt = fmt;
    vd->info = info;
    vd->pool    = Pool;
    vd->display = Display;
    vd->control = Control;
    vd->prepare = NULL;
    vd->manage  = NULL;

    /* setup initial state */
    CGSize outputSize;
    if ([container respondsToSelector:@selector(currentOutputSize)])
        outputSize = [container currentOutputSize];
    else
        outputSize = [sys->container visibleRect].size;
    vout_display_SendEventFullscreen(vd, false);
    vout_display_SendEventDisplaySize(vd, (int)outputSize.width, (int)outputSize.height, false);

    sys->colorspace = CGColorSpaceCreateDeviceRGB();
    sys->componentsPerPixel = 4;
    sys->bitsPerComponent = sizeof(unsigned char) * 8;
    sys->bitsPerPixel = sys->bitsPerComponent * sys->componentsPerPixel;

    return VLC_SUCCESS;

bailout:
    if (nsPool)
        [nsPool release];

    return VLC_EGENERIC;
}

static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Delete(sys->pool);

    if (sys->cgLayer) {
        if ([sys->container respondsToSelector:@selector(removeVoutLayer:)])
            [sys->container removeVoutLayer:sys->cgLayer];
        else
            [sys->cgLayer removeFromSuperlayer];
        [sys->cgLayer release];
    }

    if (sys->container)
        [sys->container release];

    free(sys);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;
    if (!sys->pool)
        sys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
    return sys->pool;
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->cgLayer) {
        msg_Warn(vd, "no cglayer to Display in");
        return;
    }
    uint32_t sourceWidth, sourceHeight;

    sourceWidth = picture->p[0].i_visible_pitch / picture->p[0].i_pixel_pitch;
    sourceHeight = picture->p[0].i_visible_lines;

    const int crop_offset = vd->source.i_y_offset * picture->p->i_pitch +
    vd->source.i_x_offset * picture->p->i_pixel_pitch;

    CFDataRef dataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                    &picture->p->p_pixels[crop_offset],
                                                    sizeof(picture->p->p_pixels[crop_offset]),
                                                    kCFAllocatorNull);
    CGDataProviderRef dataProvider = CGDataProviderCreateWithCFData(dataRef);

    CGImageRef newFrame = CGImageCreate(sourceWidth,
                                        sourceHeight,
                                        sys->bitsPerComponent,
                                        sys->bitsPerPixel,
                                        sys->componentsPerPixel * sourceWidth,
                                        sys->colorspace,
                                        kCGBitmapByteOrder16Big,
                                        dataProvider,
                                        NULL,
                                        true,
                                        kCGRenderingIntentPerceptual);

    CGDataProviderRelease(dataProvider);
    CFRelease(dataRef);

    if (!newFrame)
        goto end;

    [sys->cgLayer setLastFrame:newFrame];

end:
    VLC_UNUSED(subpicture);
    picture_Release(picture);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(vd);
    VLC_UNUSED(query);
    VLC_UNUSED(args);
    return VLC_SUCCESS;
}

@implementation VLCCoreGraphicsLayer
@synthesize vd=_vd;

- (id)init
{
    if (self = [super init]) {
        [CATransaction begin];
        self.needsDisplayOnBoundsChange = YES;
        self.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        [CATransaction commit];
    }

    return self;
}

- (void) dealloc
{
    if (_lastFrame)
        CGImageRelease(_lastFrame);

    [super dealloc];
}

- (bool)locked
{
    return lock;
}

- (void)setLastFrame:(CGImageRef)lastFrame
{
    if (lock) {
        /* drop frame since we currently drawing */
        CGImageRelease(lastFrame);
        return;
    }

    if (_lastFrame)
        CGImageRelease(_lastFrame);

    _lastFrame = lastFrame;
    CGRect invalidRect = CGRectMake(0, 0, CGImageGetWidth(_lastFrame), CGImageGetHeight(_lastFrame));
    [CATransaction begin];
    [self setNeedsDisplayInRect:invalidRect];
    [CATransaction commit];
}

- (void)drawInContext:(CGContextRef)cgContext
{
    if (!cgContext)
        return;

    if (!_lastFrame)
        return;

    if (lock)
        return;

    lock = YES;

    CGRect layerRect = self.bounds;
    float display_width = 0.;
    float display_height = 0.;
    float media_width = CGImageGetWidth(_lastFrame);
    float media_height = CGImageGetHeight(_lastFrame);

    float src_aspect = (float)media_width / media_height;
    float dst_aspect = (float)layerRect.size.width/layerRect.size.height;
    if (src_aspect > dst_aspect) {
        if (layerRect.size.width != media_width) { //don't scale if size equal
            display_width = layerRect.size.width;
            display_height = display_width / src_aspect; // + 0.5);
        } else {
            display_width = media_width;
            display_height = media_height;
        }
    } else {
        if (layerRect.size.height != media_height) { //don't scale if size equal
            display_height = layerRect.size.height;
            display_width = display_height * src_aspect; // + 0.5);
        } else {
            display_width = media_width;
            display_height = media_height;
        }
    }

    /* Compute the position of the video */
    float left = (layerRect.size.width  - display_width)  / 2.;
    float top  = (layerRect.size.height - display_height) / 2.;

    CGContextSaveGState(cgContext);

    // draw a clear background
    CGContextClearRect(cgContext, CGRectMake(0., 0., layerRect.size.width, layerRect.size.height));

    // draw the image
    CGRect rect = CGRectMake(left, top, display_width, display_height);
    CGContextDrawImage(cgContext, rect, _lastFrame);
    lock = NO;

    CGContextRestoreGState(cgContext);
}

- (void)resizeWithOldSuperlayerSize:(CGSize)size
{
    [super resizeWithOldSuperlayerSize:size];

    if (!_vd)
        return;

    CGSize outputSize = [_vd->sys->container currentOutputSize];

    vout_display_place_t place;
    vout_display_cfg_t cfg_tmp = *(_vd->cfg);
    cfg_tmp.display.width  = outputSize.width;
    cfg_tmp.display.height = outputSize.height;

    vout_display_PlacePicture (&place, &_vd->source, &cfg_tmp, false);
    vout_display_SendEventDisplaySize (_vd, outputSize.width, outputSize.height, _vd->cfg->is_fullscreen);
}

@end
