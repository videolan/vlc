//
//  VLCMediaThumbnailer.m
//  VLCKit
//
//  Created by Pierre d'Herbemont on 7/10/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <vlc/vlc.h>

#import "VLCMediaThumbnailer.h"
#import "VLCLibVLCBridging.h"


@interface VLCMediaThumbnailer ()
- (void)didFetchThumbnail;
- (void)notifyDelegate;
- (void)fetchThumbnail;
- (void)startFetchingThumbnail;
@property (readonly, assign) void *dataPointer;
@property (readonly, assign) BOOL shouldRejectFrames;
@end

static void *lock(void *opaque, void **pixels)
{
    VLCMediaThumbnailer *thumbnailer = opaque;

    *pixels = [thumbnailer dataPointer];
    assert(*pixels);
    return NULL;
}

static const size_t kDefaultImageWidth = 320;
static const size_t kDefaultImageHeight = 240;
static const float kSnapshotPosition = 0.5;

void unlock(void *opaque, void *picture, void *const *p_pixels)
{
    VLCMediaThumbnailer *thumbnailer = opaque;
    assert(!picture);

    assert([thumbnailer dataPointer] == *p_pixels);

    // We may already have a thumbnail if we are receiving picture after the first one.
    // Just ignore.
    if ([thumbnailer thumbnail] || [thumbnailer shouldRejectFrames])
        return;

    [thumbnailer performSelectorOnMainThread:@selector(didFetchThumbnail) withObject:nil waitUntilDone:YES];
}

void display(void *opaque, void *picture)
{
}

@implementation VLCMediaThumbnailer
@synthesize media=_media;
@synthesize delegate=_delegate;
@synthesize thumbnail=_thumbnail;
@synthesize dataPointer=_data;
@synthesize thumbnailWidth=_thumbnailWidth;
@synthesize thumbnailHeight=_thumbnailHeight;
@synthesize shouldRejectFrames=_shouldRejectFrames;

+ (VLCMediaThumbnailer *)thumbnailerWithMedia:(VLCMedia *)media andDelegate:(id<VLCMediaThumbnailerDelegate>)delegate
{
    id obj = [[[self class] alloc] init];
    [obj setMedia:media];
    [obj setDelegate:delegate];
    return [obj autorelease];
}

- (void)dealloc
{
    NSAssert(!_thumbnailingTimeoutTimer, @"Timer not released");
    NSAssert(!_parsingTimeoutTimer, @"Timer not released");
    NSAssert(!_data, @"Data not released");
    NSAssert(!_mp, @"Not properly retained");
    if (_thumbnail)
        CGImageRelease(_thumbnail);
    [_media release];
    [super dealloc];
}


- (void)fetchThumbnail
{
    NSAssert(!_data, @"We are already fetching a thumbnail");

    [self retain]; // Balanced in -notifyDelegate

    if (![_media isParsed]) {
        [_media addObserver:self forKeyPath:@"parsed" options:0 context:NULL];
        [_media parse];
        NSAssert(!_parsingTimeoutTimer, @"We already have a timer around");
        _parsingTimeoutTimer = [[NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(mediaParsingTimedOut) userInfo:nil repeats:NO] retain];
        return;
    }

    [self startFetchingThumbnail];
}


- (void)startFetchingThumbnail
{
    NSArray *tracks = [_media tracksInformation];


    // Find the video track
    NSDictionary *videoTrack = nil;
    for (NSDictionary *track in tracks) {
        NSString *type = [track objectForKey:VLCMediaTracksInformationType];
        if ([type isEqualToString:VLCMediaTracksInformationTypeVideo]) {
            videoTrack = track;
            break;
        }
    }

    unsigned imageWidth = _thumbnailWidth > 0 ? _thumbnailWidth : kDefaultImageWidth;
    unsigned imageHeight = _thumbnailHeight > 0 ? _thumbnailHeight : kDefaultImageHeight;

    if (!videoTrack)
        NSLog(@"WARNING: Can't find video track info, still attempting to thumbnail in doubt");
    else {
        int videoHeight = [[videoTrack objectForKey:VLCMediaTracksInformationVideoHeight] intValue];
        int videoWidth = [[videoTrack objectForKey:VLCMediaTracksInformationVideoWidth] intValue];

        // Constraining to the aspect ratio of the video.
        double ratio;
        if ((double)imageWidth / imageHeight < (double)videoWidth / videoHeight)
            ratio = (double)imageHeight / videoHeight;
        else
            ratio = (double)imageWidth / videoWidth;

        int newWidth = round(videoWidth * ratio);
        int newHeight = round(videoHeight * ratio);

        imageWidth = newWidth > 0 ? newWidth : imageWidth;
        imageHeight = newHeight > 0 ? newHeight : imageHeight;
    }

    _numberOfReceivedFrames = 0;
    NSAssert(!_shouldRejectFrames, @"Are we still running?");

    _effectiveThumbnailHeight = imageHeight;
    _effectiveThumbnailWidth = imageWidth;

    _data = calloc(1, imageWidth * imageHeight * 4);
    NSAssert(_data, @"Can't create data");

    NSAssert(!_mp, @"We are already fetching a thumbnail");
    _mp = libvlc_media_player_new([VLCLibrary sharedInstance]);

    libvlc_media_add_option([_media libVLCMediaDescriptor], "no-audio");

    libvlc_media_player_set_media(_mp, [_media libVLCMediaDescriptor]);
    libvlc_video_set_format(_mp, "RGBA", imageWidth, imageHeight, 4 * imageWidth);
    libvlc_video_set_callbacks(_mp, lock, unlock, display, self);
    libvlc_media_player_play(_mp);
    libvlc_media_player_set_position(_mp, kSnapshotPosition);

    NSAssert(!_thumbnailingTimeoutTimer, @"We already have a timer around");
    _thumbnailingTimeoutTimer = [[NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(mediaThumbnailingTimedOut) userInfo:nil repeats:NO] retain];
}

- (void)mediaParsingTimedOut
{
    NSLog(@"WARNING: media thumbnailer media parsing timed out");
    [_media removeObserver:self forKeyPath:@"parsed"];

    [self startFetchingThumbnail];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (object == _media && [keyPath isEqualToString:@"parsed"]) {
        if ([_media isParsed]) {
            [_parsingTimeoutTimer invalidate];
            [_parsingTimeoutTimer release];
            _parsingTimeoutTimer = nil;
            [_media removeObserver:self forKeyPath:@"parsed"];
            [self startFetchingThumbnail];
        }
        return;
    }
    return [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

- (void)didFetchThumbnail
{
    if (_shouldRejectFrames)
        return;

    // The video thread is blocking on us. Beware not to do too much work.

    _numberOfReceivedFrames++;

    // Make sure we are getting the right frame
    if (libvlc_media_player_get_position(_mp) < kSnapshotPosition / 2 &&
        // Arbitrary choice to work around broken files.
        libvlc_media_player_get_length(_mp) > 1000 &&
        _numberOfReceivedFrames < 10)
    {
        return;
    }

    NSAssert(_data, @"We have no data");
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    const CGFloat width = _effectiveThumbnailWidth;
    const CGFloat height = _effectiveThumbnailHeight;
    const CGFloat pitch = 4 * width;
    CGContextRef bitmap = CGBitmapContextCreate(_data,
                                 width,
                                 height,
                                 8,
                                 pitch,
                                 colorSpace,
                                 kCGImageAlphaNoneSkipLast);

    CGColorSpaceRelease(colorSpace);
    NSAssert(bitmap, @"Can't create bitmap");

    // Create the thumbnail image
    //NSAssert(!_thumbnail, @"We already have a thumbnail");
    if (_thumbnail)
        CGImageRelease(_thumbnail);
    _thumbnail = CGBitmapContextCreateImage(bitmap);

    // Put a new context there.
    CGContextRelease(bitmap);

    // Make sure we don't block the video thread now
    [self performSelector:@selector(notifyDelegate) withObject:nil afterDelay:0];
}

- (void)stopAsync
{
    libvlc_media_player_stop(_mp);
    libvlc_media_player_release(_mp);
    _mp = NULL;

    // Now release data
    free(_data);
    _data = NULL;

    _shouldRejectFrames = NO;
}

- (void)endThumbnailing
{
    _shouldRejectFrames = YES;

    [_thumbnailingTimeoutTimer invalidate];
    [_thumbnailingTimeoutTimer release];
    _thumbnailingTimeoutTimer = nil;

    // Stop the media player
    NSAssert(_mp, @"We have already destroyed mp");

    [self performSelectorInBackground:@selector(stopAsync) withObject:nil];

    [self autorelease]; // Balancing -fetchThumbnail
}

- (void)notifyDelegate
{
    [self endThumbnailing];

    // Call delegate
    [_delegate mediaThumbnailer:self didFinishThumbnail:_thumbnail];

}

- (void)mediaThumbnailingTimedOut
{
    [self endThumbnailing];

    // Call delegate
    [_delegate mediaThumbnailerDidTimeOut:self];
}
@end
