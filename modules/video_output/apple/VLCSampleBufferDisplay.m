/*****************************************************************************
 * VLCSampleBufferDisplay.m: video output display using
 * AVSampleBufferDisplayLayer on macOS
 *****************************************************************************
 * Copyright (C) 2023-2024 VLC authors and VideoLAN
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
#include <vlc_vout_display.h>
#include <vlc_atomic.h>
#include <vlc_modules.h>

#import "VLCDrawable.h"

#import <AVFoundation/AVFoundation.h>
#import <AVKit/AVKit.h>

#include "../../codec/vt_utils.h"
#include "vlc_pip_controller.h"

#import <VideoToolbox/VideoToolbox.h>

#if __is_target_os(ios)
#define IS_VT_ROTATION_API_AVAILABLE __IPHONE_OS_VERSION_MAX_ALLOWED >= 160000
#elif __is_target_os(macos)
#define IS_VT_ROTATION_API_AVAILABLE __MAC_OS_X_VERSION_MAX_ALLOWED >= 130000
#elif __is_target_os(tvos)
#define IS_VT_ROTATION_API_AVAILABLE __TV_OS_VERSION_MAX_ALLOWED >= 160000
#elif __is_target_os(visionos)
#define IS_VT_ROTATION_API_AVAILABLE __VISION_OS_VERSION_MAX_ALLOWED >= 10000
#endif

typedef NS_ENUM(NSUInteger, VLCSampleBufferPixelRotation) {
    kVLCSampleBufferPixelRotation_0 = 0,
    kVLCSampleBufferPixelRotation_90CW,
    kVLCSampleBufferPixelRotation_180,
    kVLCSampleBufferPixelRotation_90CCW,
};

typedef NS_ENUM(NSUInteger, VLCSampleBufferPixelFlip) {
    kVLCSampleBufferPixelFlip_None = 0,
    kVLCSampleBufferPixelFlip_H = 1 << 0,
    kVLCSampleBufferPixelFlip_V = 1 << 1,
};

#pragma mark - VLCRotatedPixelBufferProvider

@interface VLCRotatedPixelBufferProvider : NSObject
- (CVPixelBufferRef)provideFromBuffer:(CVPixelBufferRef)pixelBuffer
                             rotation:(VLCSampleBufferPixelRotation)rotation;
@end

@implementation VLCRotatedPixelBufferProvider
{
    CVPixelBufferPoolRef _rotationPool;
}

- (BOOL)_validateRotationPoolWithBuffer:(CVPixelBufferRef)pixelBuffer
                               rotation:(VLCSampleBufferPixelRotation)rotation
{
    if (!_rotationPool)
        return NO;

    uint32_t poolWidth, poolHeigth, bufferWidth, bufferHeight;

    bufferWidth = (uint32_t)CVPixelBufferGetWidth(pixelBuffer);
    bufferHeight = (uint32_t)CVPixelBufferGetHeight(pixelBuffer);
    if (rotation == kVLCSampleBufferPixelRotation_90CW || rotation == kVLCSampleBufferPixelRotation_90CCW)
    {
        uint32_t swap = bufferWidth;
        bufferWidth = bufferHeight;
        bufferHeight = swap;
    }

    CFDictionaryRef poolAttr = CVPixelBufferPoolGetPixelBufferAttributes(_rotationPool);
    if (!poolAttr) {
        return NO;
    }
    CFTypeRef value;
    value = CFDictionaryGetValue(poolAttr, kCVPixelBufferWidthKey);
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID()
        || !CFNumberGetValue(value, kCFNumberIntType, &poolWidth)
        || poolWidth != bufferWidth) 
    {
        return NO;
    }

    value = CFDictionaryGetValue(poolAttr, kCVPixelBufferHeightKey);
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID() 
        || !CFNumberGetValue(value, kCFNumberIntType, &poolHeigth)
        || poolHeigth != bufferHeight)
    {
        return NO;
    }

    return YES;
}

- (CVPixelBufferRef)provideFromBuffer:(CVPixelBufferRef)pixelBuffer
                             rotation:(VLCSampleBufferPixelRotation)rotation
{
    if (![self _validateRotationPoolWithBuffer:pixelBuffer rotation:rotation])
        CVPixelBufferPoolRelease(_rotationPool);

    if (!_rotationPool) {
        bool rotated = rotation == kVLCSampleBufferPixelRotation_90CW || rotation == kVLCSampleBufferPixelRotation_90CCW;
        uint32_t srcWidth = CVPixelBufferGetWidth(pixelBuffer);
        uint32_t srcHeight = CVPixelBufferGetHeight(pixelBuffer);
        uint32_t dstWidth = rotated ? srcHeight : srcWidth;
        uint32_t dstHeight = rotated ? srcWidth : srcHeight;
#if defined(TARGET_OS_VISION) && TARGET_OS_VISION
        const int numValues = 5;
#else
        const int numValues = 6;
#endif
        CFTypeRef keys[numValues] = {
            kCVPixelBufferPixelFormatTypeKey,
            kCVPixelBufferWidthKey,
            kCVPixelBufferHeightKey,
            kCVPixelBufferIOSurfacePropertiesKey,
            kCVPixelBufferMetalCompatibilityKey,
#if TARGET_OS_OSX
            kCVPixelBufferOpenGLCompatibilityKey,
#elif !defined(TARGET_OS_VISION) || !TARGET_OS_VISION
            kCVPixelBufferOpenGLESCompatibilityKey,
#endif
        };

        CFTypeRef values[numValues] = {
            (__bridge CFNumberRef)(@(CVPixelBufferGetPixelFormatType(pixelBuffer))),
            (__bridge CFNumberRef)(@(dstWidth)),
            (__bridge CFNumberRef)(@(dstHeight)),
            (__bridge CFDictionaryRef)@{},
            kCFBooleanTrue,
#if !defined(TARGET_OS_VISION) || !TARGET_OS_VISION
            kCFBooleanTrue
#endif
        };

        CFDictionaryRef poolAttr = CFDictionaryCreate(NULL, keys, values, numValues, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        OSStatus status = CVPixelBufferPoolCreate(NULL, NULL, poolAttr, &_rotationPool);
        CFRelease(poolAttr);
        if (status != noErr)
            return NULL;
    }

    CVPixelBufferRef rotated;
    OSStatus status = CVPixelBufferPoolCreatePixelBuffer(NULL, _rotationPool, &rotated);
    if (status != noErr) {
        return NULL;
    }
    CFDictionaryRef attachments;
    if (@available(iOS 15.0, tvOS 15.0, macOS 12.0, *)) {
        attachments = CVBufferCopyAttachments(pixelBuffer, kCVAttachmentMode_ShouldPropagate);
    } else {
        attachments = CVBufferGetAttachments(pixelBuffer, kCVAttachmentMode_ShouldPropagate);
    }
    CVBufferSetAttachments(rotated, attachments, kCVAttachmentMode_ShouldPropagate);
    if (@available(iOS 15.0, tvOS 15.0, macOS 12.0, *)) {
        CFRelease(attachments);
    }
    return rotated;
}

- (void)dealloc
{
    CVPixelBufferPoolRelease(_rotationPool);
}
@end

#pragma mark - VLCPixelBufferRotationContext

@protocol VLCPixelBufferRotationContext
@property(nonatomic) VLCSampleBufferPixelRotation rotation;
@property(nonatomic) VLCSampleBufferPixelFlip flip;
- (CVPixelBufferRef)rotate:(CVPixelBufferRef)pixelBuffer;
@end

#pragma mark - VLCPixelBufferRotationContextVT

#if IS_VT_ROTATION_API_AVAILABLE
API_AVAILABLE(ios(16.0), tvos(16.0), macosx(13.0))
@interface VLCPixelBufferRotationContextVT : NSObject <VLCPixelBufferRotationContext>

@end

@implementation VLCPixelBufferRotationContextVT
{
    VLCRotatedPixelBufferProvider *_bufferProvider;
    VTPixelRotationSessionRef _rotationSession;
}

@synthesize rotation = _rotation, flip = _flip;

- (instancetype)init
{
    self = [super init];
    if (self) {
        if (@available(iOS 16.0, tvOS 16.0, macOS 13.0, *)) {
            OSStatus status = VTPixelRotationSessionCreate(NULL, &_rotationSession);
            if (status != noErr)
                return nil;
        } else {
            return nil;
        }
    }
    return self;
}

- (void)setRotation:(VLCSampleBufferPixelRotation)rotation {
    if (_rotation == rotation)
        return;
    _rotation = rotation;
    switch (rotation) {
        case kVLCSampleBufferPixelRotation_90CW:
            VTSessionSetProperty(_rotationSession, kVTPixelRotationPropertyKey_Rotation, kVTRotation_CW90);
            break;
        case kVLCSampleBufferPixelRotation_180:
            VTSessionSetProperty(_rotationSession, kVTPixelRotationPropertyKey_Rotation, kVTRotation_180);
            break;
        case kVLCSampleBufferPixelRotation_90CCW:
            VTSessionSetProperty(_rotationSession, kVTPixelRotationPropertyKey_Rotation, kVTRotation_CCW90);
            break;
        case kVLCSampleBufferPixelRotation_0:
        default:
            VTSessionSetProperty(_rotationSession, kVTPixelRotationPropertyKey_Rotation, kVTRotation_0);
            break;
    }
}

- (void)setFlip:(VLCSampleBufferPixelFlip)flip {
    if (_flip == flip)
        return;
    _flip = flip;
    VTSessionSetProperty(_rotationSession, kVTPixelRotationPropertyKey_FlipHorizontalOrientation, flip & kVLCSampleBufferPixelFlip_H ? kCFBooleanTrue : kCFBooleanFalse);
    VTSessionSetProperty(_rotationSession, kVTPixelRotationPropertyKey_FlipVerticalOrientation, flip & kVLCSampleBufferPixelFlip_V ? kCFBooleanTrue : kCFBooleanFalse);
}

- (CVPixelBufferRef)rotate:(CVPixelBufferRef)pixelBuffer {
    if (!_bufferProvider)
        _bufferProvider = [VLCRotatedPixelBufferProvider new];

    CVPixelBufferRef rotated;
    rotated = [_bufferProvider provideFromBuffer:pixelBuffer rotation:_rotation];
    if (!rotated)
        return NULL;

    OSStatus status = VTPixelRotationSessionRotateImage(_rotationSession, pixelBuffer, rotated);
    if (status != noErr) {
        CFRelease(rotated);
        return NULL;
    }

    return rotated;
}

- (void)dealloc
{
    if (_rotationSession) {
        VTPixelRotationSessionInvalidate(_rotationSession);
        CFRelease(_rotationSession);
    }
}

@end

#endif // IS_VT_ROTATION_API_AVAILABLE

#pragma mark - VLCPixelBufferRotationContextCI

@interface VLCPixelBufferRotationContextCI : NSObject <VLCPixelBufferRotationContext>

@end

@implementation VLCPixelBufferRotationContextCI
{
    VLCRotatedPixelBufferProvider *_bufferProvider;
    CIContext *_rotationContext;
    CGImagePropertyOrientation _orientation;
}

@synthesize rotation = _rotation, flip = _flip;

- (instancetype)init
{
    self = [super init];
    if (self) {
        _rotationContext = [[CIContext alloc] initWithOptions:nil];
        if (!_rotationContext)
            return nil;
    }
    return self;
}

- (void)_updateOrientation {
    switch (_rotation) {
        case kVLCSampleBufferPixelRotation_90CW:
        {
            if (_flip == kVLCSampleBufferPixelFlip_None)
                _orientation = kCGImagePropertyOrientationRight;
            if (_flip == kVLCSampleBufferPixelFlip_H)
                _orientation = kCGImagePropertyOrientationLeftMirrored;
            if (_flip == kVLCSampleBufferPixelFlip_V)
                _orientation = kCGImagePropertyOrientationRightMirrored;
            if (_flip == (kVLCSampleBufferPixelFlip_H | kVLCSampleBufferPixelFlip_V))
                _orientation = kCGImagePropertyOrientationLeft;
            break;
        }
        case kVLCSampleBufferPixelRotation_180:
        {
            if (_flip == kVLCSampleBufferPixelFlip_None)
                _orientation = kCGImagePropertyOrientationDown;
            if (_flip == kVLCSampleBufferPixelFlip_H)
                _orientation = kCGImagePropertyOrientationDownMirrored;
            if (_flip == kVLCSampleBufferPixelFlip_V)
                _orientation = kCGImagePropertyOrientationUpMirrored;
            if (_flip == (kVLCSampleBufferPixelFlip_H | kVLCSampleBufferPixelFlip_V))
                _orientation = kCGImagePropertyOrientationUp;
            break;
        }
        case kVLCSampleBufferPixelRotation_90CCW:
        {
            if (_flip == kVLCSampleBufferPixelFlip_None)
                _orientation = kCGImagePropertyOrientationLeft;
            if (_flip == kVLCSampleBufferPixelFlip_H)
                _orientation = kCGImagePropertyOrientationRightMirrored;
            if (_flip == kVLCSampleBufferPixelFlip_V)
                _orientation = kCGImagePropertyOrientationLeftMirrored;
            if (_flip == (kVLCSampleBufferPixelFlip_H | kVLCSampleBufferPixelFlip_V))
                _orientation = kCGImagePropertyOrientationRight;
            break;
        }
        case kVLCSampleBufferPixelRotation_0:
        default:
        {
            if (_flip == kVLCSampleBufferPixelFlip_None)
                _orientation = kCGImagePropertyOrientationUp;
            if (_flip == kVLCSampleBufferPixelFlip_H)
                _orientation = kCGImagePropertyOrientationUpMirrored;
            if (_flip == kVLCSampleBufferPixelFlip_V)
                _orientation = kCGImagePropertyOrientationDownMirrored;
            if (_flip == (kVLCSampleBufferPixelFlip_H | kVLCSampleBufferPixelFlip_V))
                _orientation = kCGImagePropertyOrientationDown;
            break;
        }
    }
}

- (void)setRotation:(VLCSampleBufferPixelRotation)rotation {
    if (_rotation == rotation)
        return;
    _rotation = rotation;
    [self _updateOrientation];
}

- (void)setFlip:(VLCSampleBufferPixelFlip)flip {
    if (_flip == flip)
        return;
    _flip = flip;
    [self _updateOrientation];
}

- (CVPixelBufferRef)rotate:(CVPixelBufferRef)pixelBuffer {
    if (!_bufferProvider)
        _bufferProvider = [VLCRotatedPixelBufferProvider new];

    CVPixelBufferRef rotated;
    rotated = [_bufferProvider provideFromBuffer:pixelBuffer rotation:_rotation];
    if (!rotated)
        return NULL;

    CIImage *image = [[CIImage alloc] initWithCVPixelBuffer:pixelBuffer];
    image = [image imageByApplyingOrientation:_orientation];
    [_rotationContext render:image toCVPixelBuffer:rotated];
    
    return rotated;
}

@end

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

    converter->p_module = vlc_filter_LoadModule(converter, "video converter", NULL, false);
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

    vlc_filter_UnloadModule( p_converter );

    es_format_Clean( &p_converter->fmt_in );
    es_format_Clean( &p_converter->fmt_out );

    vlc_object_delete(p_converter);
}

static pip_controller_t * CreatePipController( vout_display_t *vd, void *cbs_opaque );
static void DeletePipController( pip_controller_t * pipcontroller );

#pragma mark -
@class VLCSampleBufferSubpicture, VLCSampleBufferDisplay;

@interface VLCSampleBufferSubpictureRegion: NSObject
@property (nonatomic, weak) VLCSampleBufferSubpicture *subpicture;
@property (nonatomic) CGRect backingFrame;
@property (nonatomic) CGImageRef image;
@property (nonatomic) CGFloat    alpha;
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
        CGContextSetAlpha(cgCtx, region.alpha);
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

@interface VLCSampleBufferDisplay: NSObject
{
    @public
    vout_display_place_t place;
    filter_t *converter;
}
    @property (nonatomic, readonly, weak) VLCView *window;
    @property (nonatomic, readonly) vout_display_t *vd;
    @property (nonatomic) VLCSampleBufferDisplayView *displayView;
    @property (nonatomic) AVSampleBufferDisplayLayer *displayLayer;
    @property (nonatomic) VLCSampleBufferSubpictureView *spuView;
    @property (nonatomic) VLCSampleBufferSubpicture *subpicture;
    @property (nonatomic) id<VLCPixelBufferRotationContext> rotationContext;

    @property (nonatomic, readonly) pip_controller_t *pipcontroller;

    - (instancetype)init NS_UNAVAILABLE;
    + (instancetype)new NS_UNAVAILABLE;
    - (instancetype)initWithVoutDisplay:(vout_display_t *)vd;
@end

@implementation VLCSampleBufferDisplay

- (id<VLCPixelBufferRotationContext>)rotationContext
{
    if (_rotationContext)
        return _rotationContext;
#if IS_VT_ROTATION_API_AVAILABLE
    if (@available(iOS 16.0, tvOS 16.0, macOS 13.0, *))
        _rotationContext = [VLCPixelBufferRotationContextVT new];
#endif
    if (!_rotationContext)
        _rotationContext = [VLCPixelBufferRotationContextCI new];
    return _rotationContext;
}


- (instancetype)initWithVoutDisplay:(vout_display_t *)vd
{
    self = [super init];
    if (!self)
        return nil;
    
    if (vd->cfg->window->type != VLC_WINDOW_TYPE_NSOBJECT)
        return nil;
    
    VLCView *window = (__bridge VLCView *)vd->cfg->window->handle.nsobject;
    if (!window) {
        msg_Err(vd, "No window found!");
        return nil;
    }

    _window = window;

    _pipcontroller = CreatePipController(vd, (__bridge void *)self);

    _vd = vd;
    
    return self;
}

- (void)preparePictureInPicture {
    if ( !_pipcontroller)
        return;
    
    if ( _pipcontroller->ops->set_display_layer ) {
        _pipcontroller->ops->set_display_layer(
            _pipcontroller,
            (__bridge void*)_displayView.displayLayer
        );
    }
}

- (void)prepareDisplay {
    @synchronized(_displayLayer) {
        if (_displayLayer)
            return;
    }

    VLCSampleBufferDisplay *sys = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (sys.displayView)
            return;

        VLCSampleBufferDisplayView *displayView;
        VLCSampleBufferSubpictureView *spuView;
        VLCView *window = sys.window;
        
        displayView = 
            [[VLCSampleBufferDisplayView alloc] initWithVoutDisplay:sys.vd];
        spuView = [VLCSampleBufferSubpictureView new];
        [window addSubview:displayView];
        [window addSubview:spuView];
        [displayView setFrame:[window bounds]];
        [spuView setFrame:[window bounds]];

        sys->place = *sys.vd->place;

        sys.displayView = displayView;
        sys.spuView = spuView;
        @synchronized(sys.displayLayer) {
            sys.displayLayer = displayView.displayLayer;
        }
        [sys preparePictureInPicture];
    });
}

- (void)close {
    VLCSampleBufferDisplay *sys = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        [sys.displayView removeFromSuperview];
        [sys.spuView removeFromSuperview];
    });
    DeletePipController(_pipcontroller);
    _pipcontroller = NULL;
}

@end

#pragma mark -
#pragma mark Module functions

static void Close(vout_display_t *vd)
{
    VLCSampleBufferDisplay *sys;
    sys = (__bridge_transfer VLCSampleBufferDisplay*)vd->sys;

    DeleteCVPXConverter(sys->converter);

    [sys close];
}

static void RenderPicture(vout_display_t *vd, picture_t *pic, vlc_tick_t date) {
    VLCSampleBufferDisplay *sys;
    sys = (__bridge VLCSampleBufferDisplay*)vd->sys;

    switch (vd->fmt->orientation) {
    case ORIENT_HFLIPPED:
        sys.rotationContext.flip = kVLCSampleBufferPixelFlip_H;
        sys.rotationContext.rotation = kVLCSampleBufferPixelRotation_0;
        break;
    case ORIENT_VFLIPPED:
        sys.rotationContext.flip = kVLCSampleBufferPixelFlip_V;
        sys.rotationContext.rotation = kVLCSampleBufferPixelRotation_0;
        break;
    case ORIENT_ROTATED_90:
        sys.rotationContext.flip = kVLCSampleBufferPixelFlip_None;
        sys.rotationContext.rotation = kVLCSampleBufferPixelRotation_90CW;
        break;
    case ORIENT_ROTATED_180:
        sys.rotationContext.flip = kVLCSampleBufferPixelFlip_None;
        sys.rotationContext.rotation = kVLCSampleBufferPixelRotation_180;
        break;
    case ORIENT_ROTATED_270:
        sys.rotationContext.flip = kVLCSampleBufferPixelFlip_None;
        sys.rotationContext.rotation = kVLCSampleBufferPixelRotation_90CCW;
        break;
    case ORIENT_TRANSPOSED:
        sys.rotationContext.flip = kVLCSampleBufferPixelFlip_V;
        sys.rotationContext.rotation = kVLCSampleBufferPixelRotation_90CW;
        break;
    case ORIENT_ANTI_TRANSPOSED:
        sys.rotationContext.flip = kVLCSampleBufferPixelFlip_H;
        sys.rotationContext.rotation = kVLCSampleBufferPixelRotation_90CW;
    case ORIENT_NORMAL:
    default:
        sys.rotationContext = nil;
        break;
    }

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
        return;
    }
    
    if (vd->fmt->orientation != ORIENT_NORMAL) {
        CVPixelBufferRef rotated = [sys.rotationContext rotate:pixelBuffer];
        if (rotated) {
            CVPixelBufferRelease(pixelBuffer);
            pixelBuffer = rotated;
        }
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
            space, kCGBitmapByteOrderDefault | kCGImageAlphaFirst,
            provider, NULL, true, kCGRenderingIntentDefault
            );
        VLCSampleBufferSubpictureRegion *region;
        region = [VLCSampleBufferSubpictureRegion new];
        region.subpicture = sys.subpicture;
        region.image = image;
        region.alpha = r->i_alpha / 255.f;

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

    [sys prepareDisplay];
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

static pip_controller_t * CreatePipController( vout_display_t *vd, void *cbs_opaque )
{
    pip_controller_t *pip_controller = vlc_object_create(vd, sizeof(pip_controller_t));

    module_t **mods;
    ssize_t total = vlc_module_match("pictureinpicture", NULL, false, &mods, NULL);
    for (ssize_t i = 0; i < total; ++i)
    {
        int (*open)(pip_controller_t *) = vlc_module_map(vd->obj.logger, mods[i]);

        if (open && open(pip_controller) == VLC_SUCCESS)
        {
            free(mods);
            return pip_controller;
        }
    }

    free(mods);
    vlc_object_delete(pip_controller);
    return NULL;
}

static void DeletePipController( pip_controller_t * pip_controller )
{
    if (pip_controller == NULL)
        return;

    if( pip_controller->ops->close )
    {
        pip_controller->ops->close(pip_controller);
    }

    vlc_object_delete(pip_controller);
}

static int Open (vout_display_t *vd,
                 video_format_t *fmt, vlc_video_context *context)
{
    if (var_InheritBool(vd, "force-darwin-legacy-display")) {
        return VLC_EGENERIC;
    }
    // Display isn't compatible with 360 content hence opening with this kind
    // of projection should fail if display use isn't forced
    if (!vd->obj.force && fmt->projection_mode != PROJECTION_MODE_RECTANGULAR) {
        return VLC_EGENERIC;
    }

    // Display will only work with CVPX video context
    filter_t *converter = NULL;
    if (!vlc_video_context_GetPrivate(context, VLC_VIDEO_CONTEXT_CVPX)) {
        converter = CreateCVPXConverter(vd);
        if (!converter)
            return VLC_EGENERIC;
    }

    @autoreleasepool {
        VLCSampleBufferDisplay *sys = 
            [[VLCSampleBufferDisplay alloc] initWithVoutDisplay:vd];

        if (sys == nil) {
            DeleteCVPXConverter(converter);
            return VLC_ENOMEM;
        }

        sys->converter = converter;

        vd->sys = (__bridge_retained void*)sys;

        static const struct vlc_display_operations ops = {
            .close = Close,
            .prepare = Prepare,
            .display = Display,
            .control = Control,
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

#define FORCE_LEGACY_DISPLAY_TEXT N_("Force fallback to legacy display")
#define FORCE_LEGACY_DISPLAY_LONGTEXT N_( \
    "Triggers an initialization failure to allow fallback to any other legacy display.")

#define HELP_TEXT N_("This display handles hardware decoded pixel buffers "\
                     "and renders them in a view/layer. "\
                     "It can also convert and display software decoded frame buffers. "\
                     "This is the default display for Apple platforms. "\
                     "--force-darwin-legacy-display option can be used to abort the "\
                     "display's initialization and allows fallback to legacy displays like "\
                     "other OpenGL/ES based video outputs.")

vlc_module_begin()
    set_description(N_("CoreMedia sample buffers based video output display"))
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_bool("force-darwin-legacy-display", false,
             FORCE_LEGACY_DISPLAY_TEXT, FORCE_LEGACY_DISPLAY_LONGTEXT)
        change_volatile()
    set_help(HELP_TEXT)
    set_callback_display(Open, 600)
vlc_module_end()
