/*****************************************************************************
 * VLCSampleBufferDisplay.m: video output display using
 * AVSampleBufferDisplayLayer on macOS
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Maxime Chapelet <umxprime at videolabs dot io>
 *
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

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_vout_display.h>
#include <vlc_atomic.h>

# import <TargetConditionals.h>
# if TARGET_OS_OSX
#     import <Cocoa/Cocoa.h>
#     define VLCView NSView
# else
#     import <Foundation/Foundation.h>
#     import <UIKit/UIKit.h>
#     define VLCView UIView
# endif

#import <AVFoundation/AVFoundation.h>
#import <AVKit/AVKit.h>

#include "../../codec/vt_utils.h"

static vlc_decoder_device * CVPXHoldDecoderDevice(vlc_object_t *o, void *sys)
{
    VLC_UNUSED(o);
    vout_display_t *vd = sys;
    vlc_decoder_device *device =
        vlc_decoder_device_Create(VLC_OBJECT(vd), vd->cfg->window);
    static const struct vlc_decoder_device_operations ops =
    {
        NULL,
    };
    device->ops = &ops;
    device->type = VLC_DECODER_DEVICE_VIDEOTOOLBOX;
    return device;
}

static filter_t *
CreateCVPXConverter(vout_display_t *vd)
{
    filter_t *converter = vlc_object_create(vd, sizeof(filter_t));
    if (!converter)
        return NULL;

    static const struct filter_video_callbacks cbs =
    {
        .buffer_new = NULL,
        .hold_device = CVPXHoldDecoderDevice,
    };
    converter->owner.video = &cbs;
    converter->owner.sys = vd;

    es_format_InitFromVideo( &converter->fmt_in,  vd->fmt );
    es_format_InitFromVideo( &converter->fmt_out,  vd->fmt );

    converter->fmt_out.video.i_chroma =
    converter->fmt_out.i_codec = VLC_CODEC_CVPX_BGRA;

    converter->p_module = module_need(converter, "video converter", NULL, false);
    if (!converter->p_module)
    {
        vlc_object_delete(converter);
        return NULL;
    }
    assert( converter->ops != NULL );

    return converter;
}


static void DeleteCVPXConverter( filter_t * p_converter )
{
    if (!p_converter)
        return;

    if( p_converter->p_module )
    {
        filter_Close( p_converter );
        module_unneed( p_converter, p_converter->p_module );
    }

    es_format_Clean( &p_converter->fmt_in );
    es_format_Clean( &p_converter->fmt_out );

    vlc_object_delete(p_converter);
}

/**
 * Protocol declaration that drawable-nsobject should follow
 */
@protocol VLCOpenGLVideoViewEmbedding <NSObject>
- (void)addVoutSubview:(VLCView *)view;
- (void)removeVoutSubview:(VLCView *)view;
@end

#pragma mark -
@class VLCSampleBufferSubpicture, VLCSampleBufferDisplay;

@interface VLCSampleBufferSubpictureRegion: NSObject
@property (nonatomic, weak) VLCSampleBufferSubpicture *subpicture;
@property (nonatomic) CGRect backingFrame;
@property (nonatomic) CGImageRef image;
@end

@implementation VLCSampleBufferSubpictureRegion
- (void)dealloc {
    CGImageRelease(_image);
}
@end

#pragma mark -

@interface VLCSampleBufferSubpicture: NSObject
@property (nonatomic, weak) VLCSampleBufferDisplay *sys;
@property (nonatomic) NSArray<VLCSampleBufferSubpictureRegion *> *regions;
@property (nonatomic) int64_t order;
@end

@implementation VLCSampleBufferSubpicture

@end

#pragma mark -

@interface VLCSampleBufferSubpictureView: VLCView
- (void)drawSubpicture:(VLCSampleBufferSubpicture *)subpicture;
@end

@implementation VLCSampleBufferSubpictureView
{
    VLCSampleBufferSubpicture *_pendingSubpicture;
}

- (instancetype)init {
    self = [super init];
    if (!self)
        return nil;
#if TARGET_OS_OSX
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.wantsLayer = YES;
#else
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.backgroundColor = [UIColor clearColor];
#endif
    return self;
}

- (void)drawSubpicture:(VLCSampleBufferSubpicture *)subpicture {
    _pendingSubpicture = subpicture;
#if TARGET_OS_OSX
    [self setNeedsDisplay:YES];
#else
    [self setNeedsDisplay];
#endif
}

- (void)drawRect:(CGRect)dirtyRect {
    #if TARGET_OS_OSX
    NSGraphicsContext *graphicsCtx = [NSGraphicsContext currentContext];
    CGContextRef cgCtx = [graphicsCtx CGContext];
    #else
    CGContextRef cgCtx = UIGraphicsGetCurrentContext();
    #endif

    CGContextClearRect(cgCtx, self.bounds);

#if TARGET_OS_IPHONE
    CGContextSaveGState(cgCtx);
    CGAffineTransform translate = CGAffineTransformTranslate(CGAffineTransformIdentity, 0.0, self.frame.size.height);
    CGFloat scale = 1.0f / self.contentScaleFactor;
    CGAffineTransform transform = CGAffineTransformScale(translate, scale, -scale);
    CGContextConcatCTM(cgCtx, transform);
#endif
    VLCSampleBufferSubpictureRegion *region;
    for (region in _pendingSubpicture.regions) {
#if TARGET_OS_OSX
        CGRect regionFrame = [self convertRectFromBacking:region.backingFrame];
#else
        CGRect regionFrame = region.backingFrame;
#endif
        CGContextDrawImage(cgCtx, regionFrame, region.image);
    }
#if TARGET_OS_IPHONE
    CGContextRestoreGState(cgCtx);
#endif
}

@end

#pragma mark -

@interface VLCSampleBufferDisplayView: VLCView <CALayerDelegate>
@property (nonatomic, readonly) vout_display_t *vd;
- (instancetype)initWithVoutDisplay:(vout_display_t *)vd;
- (AVSampleBufferDisplayLayer *)displayLayer;
@end

@implementation VLCSampleBufferDisplayView

- (instancetype)initWithVoutDisplay:(vout_display_t *)vd {
    self = [super init];
    if (!self)
        return nil;
    _vd = vd;
#if TARGET_OS_OSX
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.wantsLayer = YES;
#else
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
#endif
    return self;
}

#if TARGET_OS_OSX
- (CALayer *)makeBackingLayer {
    AVSampleBufferDisplayLayer *layer;
    layer = [AVSampleBufferDisplayLayer new];
    layer.delegate = self;
    layer.videoGravity = AVLayerVideoGravityResizeAspect;
    [CATransaction lock];
    layer.needsDisplayOnBoundsChange = YES;
    layer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
    layer.opaque = 1.0;
    layer.hidden = NO;
    [CATransaction unlock];
    return layer;
}
#else
+ (Class)layerClass {
    return [AVSampleBufferDisplayLayer class];
}
#endif

- (AVSampleBufferDisplayLayer *)displayLayer {
    return (AVSampleBufferDisplayLayer *)self.layer;
}

#if TARGET_OS_OSX
/* Layer delegate method that ensures the layer always get the
 * correct contentScale based on whether the view is on a HiDPI
 * display or not, and when it is moved between displays.
 */
- (BOOL)layer:(CALayer *)layer
shouldInheritContentsScale:(CGFloat)newScale
   fromWindow:(NSWindow *)window
{
    return YES;
}
#endif

/*
 * General properties
 */

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

@end

#pragma mark -

@interface VLCSampleBufferDisplay: NSObject {
    @public
    filter_t *converter;
}
    @property (nonatomic) id<VLCOpenGLVideoViewEmbedding> container;
    @property (nonatomic) VLCSampleBufferDisplayView *displayView;
    @property (nonatomic) AVSampleBufferDisplayLayer *displayLayer;
    @property (nonatomic) VLCSampleBufferSubpictureView *spuView;
    @property (nonatomic) VLCSampleBufferSubpicture *subpicture;
@end

@implementation VLCSampleBufferDisplay
@end

#pragma mark -
#pragma mark Module functions

static void Close(vout_display_t *vd)
{
    VLCSampleBufferDisplay *sys;
    sys = (__bridge_transfer VLCSampleBufferDisplay*)vd->sys;

    DeleteCVPXConverter(sys->converter);

    dispatch_async(dispatch_get_main_queue(), ^{
        if ([sys.container respondsToSelector:@selector(removeVoutSubview:)]) {
            [sys.container removeVoutSubview:sys.displayView];
        }
        [sys.displayView removeFromSuperview];
        [sys.spuView removeFromSuperview];
    });
}

static void RenderPicture(vout_display_t *vd, picture_t *pic, vlc_tick_t date) {
    VLCSampleBufferDisplay *sys;
    sys = (__bridge VLCSampleBufferDisplay*)vd->sys;

    @synchronized(sys.displayLayer) {
        if (sys.displayLayer == nil)
            return;
    }

    picture_Hold(pic);

    picture_t *dst = pic;
    if (sys->converter) {
        dst = sys->converter->ops->filter_video(sys->converter, pic);
    }

    CVPixelBufferRef pixelBuffer = cvpxpic_get_ref(dst);
    CVPixelBufferRetain(pixelBuffer);
    picture_Release(dst);

    if (pixelBuffer == NULL) {
        msg_Err(vd, "No pixelBuffer ref attached to pic!");
        CVPixelBufferRelease(pixelBuffer);
        return;
    }

    id aspectRatio = @{
        (__bridge NSString*)kCVImageBufferPixelAspectRatioHorizontalSpacingKey:
            @(vd->source->i_sar_num),
        (__bridge NSString*)kCVImageBufferPixelAspectRatioVerticalSpacingKey:
            @(vd->source->i_sar_den)
    };

    CVBufferSetAttachment(
        pixelBuffer,
        kCVImageBufferPixelAspectRatioKey,
        (__bridge CFDictionaryRef)aspectRatio,
        kCVAttachmentMode_ShouldPropagate
    );

    CMSampleBufferRef sampleBuffer = NULL;
    CMVideoFormatDescriptionRef formatDesc = NULL;
    OSStatus err = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer, &formatDesc);
    if (err != noErr) {
        msg_Err(vd, "Image buffer format desciption creation failed!");
        CVPixelBufferRelease(pixelBuffer);
        return;
    }

    vlc_tick_t now = vlc_tick_now();
    CFTimeInterval ca_now = CACurrentMediaTime();
    vlc_tick_t ca_now_ts = vlc_tick_from_sec(ca_now);
    vlc_tick_t diff = date - now;
    CFTimeInterval ca_date = secf_from_vlc_tick(ca_now_ts + diff);
    CMSampleTimingInfo sampleTimingInfo = {
        .decodeTimeStamp = kCMTimeInvalid,
        .duration = kCMTimeInvalid,
        .presentationTimeStamp = CMTimeMakeWithSeconds(ca_date, 1000000)
    };

    err = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, pixelBuffer, formatDesc, &sampleTimingInfo, &sampleBuffer);
    CFRelease(formatDesc);
    CVPixelBufferRelease(pixelBuffer);
    if (err != noErr) {
        msg_Err(vd, "Image buffer creation failed!");
        return;
    }

    @synchronized(sys.displayLayer) {
        [sys.displayLayer enqueueSampleBuffer:sampleBuffer];
    }

    CFRelease(sampleBuffer);
}

static CGRect RegionBackingFrame(unsigned display_height,
                                 const struct subpicture_region_rendered *r)
{
    // Invert y coords for CoreGraphics
    const int y = display_height - r->place.height - r->place.y;

    return CGRectMake(
        r->place.x,
        y,
        r->place.width,
        r->place.height
    );
}

static void UpdateSubpictureRegions(vout_display_t *vd,
                                    const vlc_render_subpicture *subpicture)
{
    VLCSampleBufferDisplay *sys;
    sys = (__bridge VLCSampleBufferDisplay*)vd->sys;

    if (sys.subpicture == nil || subpicture == NULL)
        return;

    NSMutableArray *regions = [NSMutableArray new];
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    const struct subpicture_region_rendered *r;
    vlc_vector_foreach(r, &subpicture->regions) {
        CFIndex length = r->p_picture->format.i_height * r->p_picture->p->i_pitch;
        const size_t pixels_offset =
                r->p_picture->format.i_y_offset * r->p_picture->p->i_pitch +
                r->p_picture->format.i_x_offset * r->p_picture->p->i_pixel_pitch;

        CFDataRef data = CFDataCreate(
            NULL,
            r->p_picture->p->p_pixels + pixels_offset,
            length - pixels_offset);
        CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
        CGImageRef image = CGImageCreate(
            r->p_picture->format.i_visible_width, r->p_picture->format.i_visible_height,
            8, 32, r->p_picture->p->i_pitch,
            space, kCGImageAlphaFirst,
            provider, NULL, true, kCGRenderingIntentDefault
            );
        VLCSampleBufferSubpictureRegion *region;
        region = [VLCSampleBufferSubpictureRegion new];
        region.subpicture = sys.subpicture;
        region.image = image;

        region.backingFrame = RegionBackingFrame(vd->cfg->display.height, r);
        [regions addObject:region];
        CGDataProviderRelease(provider);
        CFRelease(data);
    }
    CGColorSpaceRelease(space);

    sys.subpicture.regions = regions;
}

static bool IsSubpictureDrawNeeded(vout_display_t *vd, const vlc_render_subpicture *subpicture)
{
    VLCSampleBufferDisplay *sys;
    sys = (__bridge VLCSampleBufferDisplay*)vd->sys;

    if (subpicture == NULL)
    {
        if (sys.subpicture == nil)
            return false;
        sys.subpicture = nil;
        /* Need to draw one last time in order to clear the current subpicture */
        return true;
    }

    size_t count = subpicture->regions.size;
    const struct subpicture_region_rendered *r;

    if (!sys.subpicture || subpicture->i_order != sys.subpicture.order)
    {
        /* Subpicture content is different */
        sys.subpicture = [VLCSampleBufferSubpicture new];
        sys.subpicture.sys = sys;
        sys.subpicture.order = subpicture->i_order;
        UpdateSubpictureRegions(vd, subpicture);
        return true;
    }

    bool draw = false;

    if (count == sys.subpicture.regions.count)
    {
        size_t i = 0;
        vlc_vector_foreach(r, &subpicture->regions)
        {
            VLCSampleBufferSubpictureRegion *region =
                sys.subpicture.regions[i++];

            CGRect newRegion = RegionBackingFrame(vd->cfg->display.height, r);

            if ( !CGRectEqualToRect(region.backingFrame, newRegion) )
            {
                /* Subpicture regions are different */
                draw = true;
                break;
            }
        }
    }
    else
    {
        /* Subpicture region count is different */
        draw = true;
    }

    if (!draw)
        return false;

    /* Store the current subpicture regions in order to compare then later.
     */

    UpdateSubpictureRegions(vd, subpicture);
    return true;
}

static void RenderSubpicture(vout_display_t *vd, const vlc_render_subpicture *spu)
{
    if (!IsSubpictureDrawNeeded(vd, spu))
        return;

    VLCSampleBufferDisplay *sys;
    sys = (__bridge VLCSampleBufferDisplay*)vd->sys;

    dispatch_async(dispatch_get_main_queue(), ^{
        [sys.spuView drawSubpicture:sys.subpicture];
    });
}

static void PrepareDisplay (vout_display_t *vd) {
    VLCSampleBufferDisplay *sys;
    sys = (__bridge VLCSampleBufferDisplay*)vd->sys;

    @synchronized(sys.displayLayer) {
        if (sys.displayLayer)
            return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        if (sys.displayView)
            return;
        VLCSampleBufferDisplayView *displayView =
            [[VLCSampleBufferDisplayView alloc] initWithVoutDisplay:vd];
        VLCSampleBufferSubpictureView *spuView =
            [VLCSampleBufferSubpictureView new];
        id container = sys.container;
        //TODO: Is it still relevant ?
        if ([container respondsToSelector:@selector(addVoutSubview:)]) {
            [container addVoutSubview:displayView];
            [container addVoutSubview:spuView];
        } else if ([container isKindOfClass:[VLCView class]]) {
            VLCView *containerView = container;
            [containerView addSubview:displayView];
            [containerView addSubview:spuView];
            [displayView setFrame:containerView.bounds];
            [spuView setFrame:containerView.bounds];
        } else {
            displayView = nil;
            spuView = nil;
        }

        sys.displayView = displayView;
        sys.spuView = spuView;
        @synchronized(sys.displayLayer) {
            sys.displayLayer = displayView.displayLayer;
        }
    });
}

static void Prepare (vout_display_t *vd, picture_t *pic,
                     const vlc_render_subpicture *subpicture, vlc_tick_t date)
{
    PrepareDisplay(vd);
    if (pic) {
        RenderPicture(vd, pic, date);
    }

    RenderSubpicture(vd, subpicture);
}

static void Display(vout_display_t *vd, picture_t *pic)
{
}

static int Control (vout_display_t *vd, int query)
{
    VLCSampleBufferDisplay *sys;
    sys = (__bridge VLCSampleBufferDisplay*)vd->sys;

    switch (query)
    {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        case VOUT_DISPLAY_CHANGE_SOURCE_PLACE:
            break;
        default:
            msg_Err (vd, "Unhandled request %d", query);
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int Open (vout_display_t *vd,
                 video_format_t *fmt, vlc_video_context *context)
{
    // Display isn't compatible with 360 content hence opening with this kind
    // of projection should fail if display use isn't forced
    if (!vd->obj.force && fmt->projection_mode != PROJECTION_MODE_RECTANGULAR) {
        return VLC_EGENERIC;
    }

    if (vd->cfg->window->type != VLC_WINDOW_TYPE_NSOBJECT)
        return VLC_EGENERIC;

    VLCSampleBufferDisplay *sys = [VLCSampleBufferDisplay new];
    if (sys == nil) {
        return VLC_ENOMEM;
    }

    // Display will only work with CVPX video context
    filter_t *converter = NULL;
    if (!vlc_video_context_GetPrivate(context, VLC_VIDEO_CONTEXT_CVPX)) {
        converter = CreateCVPXConverter(vd);
        if (!converter)
            return VLC_EGENERIC;
    }
    sys->converter = converter;

    @autoreleasepool {
        id container = (__bridge id)vd->cfg->window->handle.nsobject;
        if (!container) {
            msg_Err(vd, "No drawable-nsobject found!");
            DeleteCVPXConverter(converter);
            return VLC_EGENERIC;
        }

        sys.container = container;

        vd->sys = (__bridge_retained void*)sys;

        static const struct vlc_display_operations ops = {
            Close, Prepare, Display, Control, NULL, NULL, NULL,
        };

        vd->ops = &ops;

        static const vlc_fourcc_t subfmts[] = {
            VLC_CODEC_ARGB,
            0
        };

        vd->info.subpicture_chromas = subfmts;

        return VLC_SUCCESS;
    }
}

/*
 * Module descriptor
 */
vlc_module_begin()
    set_description(N_("CoreMedia sample buffers based video output display"))
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_callback_display(Open, 600)
vlc_module_end()
