/*****************************************************************************
 * avsamplebuffer.m: AVSampleBufferRender plugin for iOS and macOS
 *****************************************************************************
 * Copyright (C) 2021 VLC authors, VideoLAN and VideoLABS
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

#import "config.h"

#import <TargetConditionals.h>

#if TARGET_OS_IPHONE || TARGET_OS_TV
#define USE_ROUTE_SHARING_POLICY
#endif

#import <AVFoundation/AVFoundation.h>

#ifdef USE_ROUTE_SHARING_POLICY
#import <AudioUnit/AudioUnit.h>
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_aout.h>

// for (void)setRate:(float)rate time:(CMTime)time atHostTime:(CMTime)hostTime
#define MIN_MACOS 11.3
#define MIN_IOS 14.5
#define MIN_TVOS 14.5

#pragma mark Private

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS))
@interface VLCAVSample : NSObject {
    audio_output_t *_aout;

    CMAudioFormatDescriptionRef _fmtDesc;
    AVSampleBufferAudioRenderer *_renderer;
    AVSampleBufferRenderSynchronizer *_sync;
    id _observer;
    dispatch_queue_t _dataQueue;
    dispatch_queue_t _timeQueue;
    size_t _bytesPerFrame;

    vlc_mutex_t _bufferLock;
    vlc_cond_t _bufferWait;
    vlc_cond_t _bufferReadWait;
    CMSampleBufferRef _bufferReady;

    int64_t _ptsSamples;
    int64_t _firstPtsSamples;
    unsigned _sampleRate;
    BOOL _paused;
    BOOL _started;
    BOOL _stopped;
    atomic_bool _timeReady;
}
@end

@implementation VLCAVSample

- (id)init:(audio_output_t*)aout
{
    _aout = aout;
    _dataQueue = dispatch_queue_create("VLC AVSampleBuffer data queue", DISPATCH_QUEUE_SERIAL);
    if (_dataQueue == nil)
        return nil;

    _timeQueue = dispatch_queue_create("VLC AVSampleBuffer time queue", DISPATCH_QUEUE_SERIAL);
    if (_timeQueue == nil)
        return nil;

    vlc_mutex_init(&_bufferLock);
    vlc_cond_init(&_bufferWait);
    vlc_cond_init(&_bufferReadWait);
    atomic_init(&_timeReady, false);

    self = [super init];
    if (self == nil)
        return nil;

    return self;
}

static void
customBlock_Free(void *refcon, void *doomedMemoryBlock, size_t sizeInBytes)
{
    block_t *block = refcon;

    assert(block->i_buffer == sizeInBytes);
    block_Release(block);

    (void) doomedMemoryBlock;
    (void) sizeInBytes;
}

- (CMSampleBufferRef)wrapBuffer:(block_t **)pblock
{
    // This function take the block ownership
    block_t *block = *pblock;
    *pblock = NULL;

    const CMBlockBufferCustomBlockSource blockSource = {
        .version = kCMBlockBufferCustomBlockSourceVersion,
        .FreeBlock = customBlock_Free,
        .refCon = block,
    };

    OSStatus status;
    CMBlockBufferRef blockBuf;
    status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,
                                                block->p_buffer,  // memoryBlock
                                                block->i_buffer,  // blockLength
                                                NULL,             // blockAllocator
                                                &blockSource,     // customBlockSource
                                                0,                // offsetToData
                                                block->i_buffer,  // dataLength
                                                0,                // flags
                                                &blockBuf);
    if (status != noErr)
    {
        msg_Err(_aout, "CMBlockBufferRef creation failure %i", (int)status);
        block_Release(block);
        return nil;
    }

    const CMSampleTimingInfo timeInfo = {
        .duration = kCMTimeInvalid,
        .presentationTimeStamp = CMTimeMake(_ptsSamples, _sampleRate),
        .decodeTimeStamp = kCMTimeInvalid,
    };

    CMSampleBufferRef sampleBuf;
    status = CMSampleBufferCreateReady(kCFAllocatorDefault,
                                       blockBuf,             // dataBuffer
                                       _fmtDesc,             // formatDescription
                                       block->i_nb_samples,  // numSamples
                                       1,                    // numSampleTimingEntries
                                       &timeInfo,            // sampleTimingArray
                                       1,                    // numSampleSizeEntries
                                       &_bytesPerFrame,      // sampleSizeArray
                                       &sampleBuf);
    CFRelease(blockBuf);

    if (status != noErr)
    {
        msg_Warn(_aout, "CMSampleBufferRef creation failure %i", (int)status);
        return nil;
    }

    return sampleBuf;
}

- (void)selectDevice:(const char *)name
{
    // TODO re-use auhal code and _renderer.audioOutputDeviceUniqueID
}

- (void)setMute:(BOOL)muted
{
    _renderer.muted = muted;
}

- (void)setVolume:(float)volume
{
    if (volume > 1.0f)
        volume = 1.0f; // TODO: handle gain > 1.0f
    _renderer.volume = volume;
}

+ (vlc_tick_t)CMTimeTotick:(CMTime) timestamp
{
    CMTime scaled = CMTimeConvertScale(
            timestamp, CLOCK_FREQ,
            kCMTimeRoundingMethod_Default);

    return scaled.value;
}

- (vlc_tick_t)getDelay
{
    CMTime played = CMTimeMake(_ptsSamples, _sampleRate);
    CMTime current = _sync.currentTime;
    CMTime delay = CMTimeSubtract(played, current);
    return [VLCAVSample CMTimeTotick:delay];
}

- (BOOL)getTime:(vlc_tick_t *)outDelay
{
    if (!atomic_load(&_timeReady))
        return NO;
    if (!_started)
        return NO;

    CMTime current = _sync.currentTime;
    CMTime played = CMTimeMake(_ptsSamples, _sampleRate);
    CMTime delay = CMTimeSubtract(played, current);

    vlc_tick_t delayTick = [VLCAVSample CMTimeTotick:delay];

    if (delayTick < 0)
    {
        msg_Warn(_aout, "negative delay! %"PRId64, delayTick);
        return NO;
    }

    *outDelay = delayTick;
    return YES;
}

- (void)flush
{
    vlc_tick_t now = vlc_tick_now();
    _sync.rate = 0.0f;

    if (_started)
    {
        [_sync removeTimeObserver:_observer];
        [_renderer stopRequestingMediaData];

        [_renderer flush];
    }

    _firstPtsSamples = -1;
    _ptsSamples = -1;
    _started = NO;
    _observer = nil;
    atomic_store(&_timeReady, false);

    // TODO should not be needed
#if 0
    [_sync removeRenderer:_renderer atTime:kCMTimeInvalid completionHandler:nil];

    _renderer = [[AVSampleBufferAudioRenderer alloc] init];
    if (_renderer == nil)
    {
        return;
    }
    _sync = [[AVSampleBufferRenderSynchronizer alloc] init];
    if (_sync == nil)
    {
        _renderer = nil;
        return;
    }
    [_sync addRenderer:_renderer];
#endif
    fprintf(stderr, "FLUSHED in %"PRId64"\n", vlc_tick_now() - now);
}

- (void)pause:(BOOL)pause date:(vlc_tick_t)date
{
    (void) date; // TODO
    _paused = pause;

    //TODO handle conflict with _started
    if (_started)
        _sync.rate = pause ? 0.0f : 1.0f;
}

- (void)whenTimeObserved
{
    atomic_store(&_timeReady, true);
}

- (void)whenDataReady
{
    vlc_mutex_lock(&_bufferLock);

    while (_renderer.readyForMoreMediaData)
    {
        while (!_stopped && _bufferReady == nil)
            vlc_cond_wait(&_bufferWait, &_bufferLock);

        if (_stopped)
        {
            vlc_mutex_unlock(&_bufferLock);
            return;
        }

        [_renderer enqueueSampleBuffer:_bufferReady];
        _bufferReady = nil;

        vlc_cond_signal(&_bufferReadWait);
    }

    vlc_mutex_unlock(&_bufferLock);
}

- (void)play:(block_t *)block date:(vlc_tick_t)date
{
    if (_ptsSamples == -1)
    {
        [_renderer requestMediaDataWhenReadyOnQueue:_dataQueue usingBlock:^{
            [self whenDataReady];
        }];

        _firstPtsSamples = _ptsSamples = samples_from_vlc_tick(block->i_pts, _sampleRate);
        vlc_tick_t delta = date - vlc_tick_now();
        CMTime hostTime = CMTimeAdd(CMClockGetTime(CMClockGetHostTimeClock()),
                                    CMTimeMake(delta, CLOCK_FREQ));
        CMTime time = CMTimeMake(_ptsSamples, _sampleRate);
        [_sync setRate:1.0f time:time atHostTime:hostTime];

        time = CMTimeAdd(time, CMTimeMake(delta, CLOCK_FREQ));
        NSArray *array = [NSArray arrayWithObject:[NSValue valueWithCMTime:time]];
        _observer = [_sync addBoundaryTimeObserverForTimes:array
                                                     queue:_timeQueue
                                                usingBlock:^{
            [self whenTimeObserved];
        }];

        _started = YES;
    }

    CMSampleBufferRef buffer = [self wrapBuffer:&block];
    assert(block == nil);
    if (buffer == nil)
        return;

    if (!_started)
    {
        vlc_tick_t delta = date - vlc_tick_now() - [self getDelay];

        if (_renderer.readyForMoreMediaData && delta > 0)
        {
            msg_Info(_aout, "deferring start (%"PRId64" us, delay: %"PRId64")", delta, [self getDelay]);
        }
        else
        {
            msg_Info(_aout, "starting (delta: %"PRId64" us, delay: %"PRId64")", delta, [self getDelay]);

            CMTime hostTime = CMTimeAdd(CMClockGetTime(CMClockGetHostTimeClock()),
                                        CMTimeMake(delta, CLOCK_FREQ));
            hostTime = CMTimeSubtract(hostTime, CMTimeMake(_ptsSamples  - _firstPtsSamples, _sampleRate));
            //[_sync setRate:1.0f
            //          time:CMTimeMake(_firstPtsSamples, _sampleRate)
            //    atHostTime:hostTime];
            [_sync setRate:1.0f
                     time:CMTimeMake(_firstPtsSamples, _sampleRate)];
            _started = YES;
        }
    }

    vlc_mutex_lock(&_bufferLock);

    assert(_bufferReady == nil);
    _bufferReady = buffer;
    vlc_cond_signal(&_bufferWait);

    while (_bufferReady != nil)
        vlc_cond_wait(&_bufferReadWait, &_bufferLock);

    vlc_mutex_unlock(&_bufferLock);

    _ptsSamples += CMSampleBufferGetNumSamples(buffer);

    CFRelease(buffer);
}

- (void)rateChanged:(NSNotification *)notification
{
    fprintf(stderr, "rate changed: %f\n", _sync.rate);
}

- (void)stop
{
    vlc_tick_t now = vlc_tick_now();

    vlc_mutex_lock(&_bufferLock);
    _stopped = true;
    vlc_cond_signal(&_bufferWait);
    vlc_mutex_unlock(&_bufferLock);

    _sync.rate = 0.0f;

    if (_started)
    {
        [_sync removeTimeObserver:_observer];
        [_renderer stopRequestingMediaData];

        [_renderer flush];
    }

    [_sync removeRenderer:_renderer atTime:kCMTimeInvalid completionHandler:nil];

     [[NSNotificationCenter defaultCenter] removeObserver:self
                                                     name:AVSampleBufferRenderSynchronizerRateDidChangeNotification
                                                   object:nil];

    _sync = nil;
    _renderer = nil;
    CFRelease(_fmtDesc);

    fprintf(stderr, "STOPPED in %"PRId64"\n", vlc_tick_now() - now);
}

- (BOOL)start:(audio_sample_format_t *)fmt
{
    if (aout_BitsPerSample(fmt->i_format) == 0)
        return NO; /* TODO: Can handle PT ? */

    AudioStreamBasicDescription desc = {
        .mSampleRate = fmt->i_rate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagsNativeFloatPacked,
        .mChannelsPerFrame = aout_FormatNbChannels(fmt),
        .mFramesPerPacket = 1,
        .mBitsPerChannel = 32,
    };

    desc.mBytesPerFrame = desc.mBitsPerChannel * desc.mChannelsPerFrame / 8;
    desc.mBytesPerPacket = desc.mBytesPerFrame * desc.mFramesPerPacket;

    OSStatus status =
        CMAudioFormatDescriptionCreate(kCFAllocatorDefault,
                                       &desc,   // asbd
                                       0,       // TODO: layoutSize
                                       nil,     // TODO: layout
                                       0,       // magicCookieSize
                                       nil,     // magicCookie
                                       nil,     // extensions
                                       &_fmtDesc);
    if (status != noErr)
    {
        msg_Warn(_aout, "CMAudioFormatDescriptionRef creation failure %i", (int)status);
        return NO;
    }

#ifdef USE_ROUTE_SHARING_POLICY
    //TODO: use GetRouteSharingPolicy from audiounit_ios.m
    NSError *error = nil;
    AVAudioSessionCategory category = AVAudioSessionCategoryPlayback;
    AVAudioSessionMode mode = AVAudioSessionModeMoviePlayback;
    AVAudioSessionRouteSharingPolicy policy = AVAudioSessionRouteSharingPolicyLongFormAudio;

    [[AVAudioSession sharedInstance] setCategory:category
                                            mode:mode
                              routeSharingPolicy:policy
                                         options:0
                                           error:&error];
#endif

    _renderer = [[AVSampleBufferAudioRenderer alloc] init];
    if (_renderer == nil)
    {
        CFRelease(_fmtDesc);
        return NO;
    }

    _sync = [[AVSampleBufferRenderSynchronizer alloc] init];
    if (_sync == nil)
    {
        CFRelease(_fmtDesc);
        _renderer = nil;
        return NO;
    }

    [_sync addRenderer:_renderer];

    // but not the paused state
    _paused = NO;
    _started = NO;
    _stopped = NO;
    _observer = nil;
    atomic_store(&_timeReady, false);

    [_renderer requestMediaDataWhenReadyOnQueue:_dataQueue usingBlock:^{
        [self whenDataReady];
    }];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(rateChanged:)
                                                 name:AVSampleBufferRenderSynchronizerRateDidChangeNotification
                                               object:nil];

    _ptsSamples = -1;
    _sampleRate = fmt->i_rate;
    _bytesPerFrame = desc.mBytesPerFrame;
    fmt->i_format = VLC_CODEC_FL32;

    return YES;
}

@end

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS))
static int
DeviceSelect(audio_output_t *aout, const char *name)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys selectDevice:name];

    return VLC_SUCCESS;
}

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS))
static int
MuteSet(audio_output_t *aout, bool mute)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys setMute:mute];

    return VLC_SUCCESS;
}

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS))
static int
VolumeSet(audio_output_t *aout, float volume)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys setVolume:volume];

    return VLC_SUCCESS;
}

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS))
static int
TimeGet(audio_output_t *aout, vlc_tick_t *delay)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    return [sys getTime:delay] ? VLC_SUCCESS : VLC_EGENERIC;
}

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS))
static void
Flush(audio_output_t *aout)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys flush];
}

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS))
static void
Pause(audio_output_t *aout, bool pause, vlc_tick_t date)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys pause:pause date:date];
}

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS))
static void
Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys play:block date:date];
}

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS))
static void
Stop(audio_output_t *aout)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys stop];
}

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS))
static int
Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    return [sys start:fmt] ? VLC_SUCCESS : VLC_EGENERIC;
}

static void
Close(vlc_object_t *obj)
{
    if (@available(macOS MIN_MACOS, iOS MIN_IOS, tvOS MIN_TVOS, *))
    {
        audio_output_t *aout = (audio_output_t *)obj;
        /* Transfer ownership back from VLC to ARC so that it can be released. */
        VLCAVSample *sys = (__bridge_transfer VLCAVSample*)aout->sys;
        (void) sys;
    }
}

static int
Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    if (@available(macOS MIN_MACOS, iOS MIN_IOS, tvOS MIN_TVOS, *))
    {
        aout->sys = (__bridge_retained void*) [[VLCAVSample alloc] init:aout];
        if (aout->sys == nil)
            return VLC_EGENERIC;

        aout->start = Start;
        aout->stop = Stop;
        aout->play = Play;
        aout->pause = Pause;
        aout->flush = Flush;
        aout->time_get = TimeGet;
        aout->volume_set = VolumeSet;
        aout->mute_set = MuteSet;
        aout->device_select = DeviceSelect;

        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

#define AOUT_VOLUME_DEFAULT             256
#define AOUT_VOLUME_MAX                 512

#define VOLUME_TEXT N_("Audio volume")

vlc_module_begin ()
    set_shortname("avsample")
    set_description(N_("AVSampleBufferRender output"))
    set_capability("audio output", 100)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    set_callbacks(Open, Close)
    add_integer("auhal-volume", AOUT_VOLUME_DEFAULT,
                VOLUME_TEXT, NULL)
    change_integer_range(0, AOUT_VOLUME_MAX)
    change_private()
vlc_module_end ()
