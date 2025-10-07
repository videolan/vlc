/*****************************************************************************
 * avsamplebuffer.m: AVSampleBufferRender plugin for iOS and macOS
 *****************************************************************************
 * Copyright (C) 2024 VLC authors, VideoLAN and VideoLABS
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
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_aout.h>

#if TARGET_OS_IPHONE || TARGET_OS_TV || TARGET_OS_VISION
#define HAS_AVAUDIOSESSION
#import "avaudiosession_common.h"
#endif

#import "channel_layout.h"

// for (void)setRate:(float)rate time:(CMTime)time atHostTime:(CMTime)hostTime
#define MIN_MACOS 11.3
#define MIN_IOS 14.5
#define MIN_TVOS 14.5
#define MIN_WATCHOS 7.4

// work-around to fix compilation on older Xcode releases
#if defined(TARGET_OS_VISION) && TARGET_OS_VISION
    #define MIN_VISIONOS 1.0
    #define VISIONOS_API_AVAILABLE , visionos(MIN_VISIONOS)
#else
    #define VISIONOS_API_AVAILABLE
#endif

#pragma mark Private

API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS) VISIONOS_API_AVAILABLE)
@interface VLCAVSample : NSObject
{
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

    block_t *_outChain;
    block_t **_outChainLast;

    int64_t _ptsSamples;
    vlc_tick_t _firstPts;
    vlc_tick_t _lastDate;
    unsigned _sampleRate;
    BOOL _stopped;
    BOOL _dateReached;
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

    _outChain = NULL;
    _outChainLast = &_outChain;

    /* The first call to CMAudioFormatDescriptionCreate() might take some time
     * (between 200 and 600ms) as it is initializing some static context/libs.
     * Therefore, call it from the Open() callback with dummy params. Indeed,
     * the playback is not yet started and a longer Open() call won't mess with
     * playback timings. */
    static const AudioStreamBasicDescription dummyDesc = {
        .mSampleRate = 48000,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagsNativeFloatPacked,
        .mChannelsPerFrame = 2,
        .mFramesPerPacket = 1,
        .mBitsPerChannel = 32,
    };
    CMAudioFormatDescriptionRef dummyFmtDesc;
    OSStatus status =
        CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &dummyDesc, 0, nil,
                                       0, nil, nil, &dummyFmtDesc);
    if (status == noErr)
        CFRelease(dummyFmtDesc);

    self = [super init];
    if (self == nil)
        return nil;

    return self;
}

- (void)dealloc
{
    assert(_outChain == NULL);
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
        msg_Err(_aout, "CMBlockBufferRef creation failure %li", (long)status);
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
        msg_Warn(_aout, "CMSampleBufferRef creation failure %li", (long)status);
        return nil;
    }

    return sampleBuf;
}

- (void)selectDevice:(const char *)name
{
}

- (void)setMute:(BOOL)muted
{
    _renderer.muted = muted;
    aout_MuteReport(_aout, muted);
}

- (void)setVolume:(float)volume
{
    _renderer.volume = volume * volume * volume;
    aout_VolumeReport(_aout, volume);
}

+ (vlc_tick_t)CMTimeTotick:(CMTime) timestamp
{
    CMTime scaled = CMTimeConvertScale(
            timestamp, CLOCK_FREQ,
            kCMTimeRoundingMethod_Default);

    return scaled.value;
}

- (void)flush
{
    if (_ptsSamples >= 0)
        [self stopSyncRenderer];

    _ptsSamples = -1;
    _dateReached = NO;
    _firstPts = VLC_TICK_INVALID;
    _lastDate = VLC_TICK_INVALID;
}

- (void)pause:(BOOL)pause date:(vlc_tick_t)date
{
    (void) date;

    if (_ptsSamples >= 0)
        _sync.rate = pause ? 0.0f : 1.0f;
}

- (void)whenTimeObserved:(CMTime) time
{
    assert(_firstPts != VLC_TICK_INVALID);

    if (time.value == 0)
        return;
    vlc_tick_t system_now = vlc_tick_now();
    vlc_tick_t pos_ticks = [VLCAVSample CMTimeTotick:time] + _firstPts;

    aout_TimingReport(_aout, system_now, pos_ticks);
}

- (void)startNow:(vlc_tick_t)delta
{
    assert(!_dateReached);

    _dateReached = YES;
    CMTime time = CMTimeMake(0, _sampleRate);
    [_sync setRate:1.0f time:time];

    const CMTime interval = CMTimeMake(CLOCK_FREQ, CLOCK_FREQ);
    __weak typeof(self) weakSelf = self;
    _observer = [_sync addPeriodicTimeObserverForInterval:interval
                                                    queue:_timeQueue
                                               usingBlock:^ (CMTime time){
        [weakSelf whenTimeObserved:time];
    }];
}

- (void)whenDataReady
{
    vlc_mutex_lock(&_bufferLock);

    while (_renderer.readyForMoreMediaData || !_dateReached)
    {
        if (!_dateReached && !_stopped)
        {
            /* Start playback at the requested date */

            CMTime writtenTime = CMTimeMake(_ptsSamples, _sampleRate);
            vlc_tick_t writtenTicks = [VLCAVSample CMTimeTotick:writtenTime];
            vlc_tick_t now = vlc_tick_now();
            vlc_tick_t deadline = _lastDate - writtenTicks;
            vlc_tick_t delta = deadline - now;

            if (delta <= 0)
            {
                msg_Dbg(_aout, "starting late (%"PRId64" us)", delta);
                [self startNow:delta];
            }
            else
            {
                msg_Dbg(_aout, "deferring start (%"PRId64" us)", delta);

                int timeout = 0;
                /* Wait for the start date if there are no buffers to enqueue */
                while (!_stopped && _outChain == NULL && timeout == 0)
                {
                    timeout = vlc_cond_timedwait(&_bufferWait, &_bufferLock,
                                                 deadline);
                    deadline = _lastDate - writtenTicks;
                }

                if (timeout != 0 && !_stopped)
                {
                    msg_Dbg(_aout, "started");
                    [self startNow:0];
                }
            }
        }

        while (!_stopped && _outChain == NULL)
            vlc_cond_wait(&_bufferWait, &_bufferLock);

        if (_stopped)
        {
            vlc_mutex_unlock(&_bufferLock);
            return;
        }

        block_t *block = _outChain;
        _outChain = _outChain->p_next;
        if (_outChain == NULL)
            _outChainLast = &_outChain;

        CMSampleBufferRef buffer = [self wrapBuffer:&block];
        if (buffer == nil)
        {
            vlc_mutex_unlock(&_bufferLock);
            return;
        }

        _ptsSamples += CMSampleBufferGetNumSamples(buffer);

        [_renderer enqueueSampleBuffer:buffer];

        CFRelease(buffer);
    }

    vlc_mutex_unlock(&_bufferLock);
}

- (void)play:(block_t *)block date:(vlc_tick_t)date
{
    vlc_mutex_lock(&_bufferLock);

    if (_ptsSamples == -1)
    {
        _stopped = NO;
        _firstPts = block->i_pts;
        _ptsSamples = 0;

        __weak typeof(self) weakSelf = self;
        [_renderer requestMediaDataWhenReadyOnQueue:_dataQueue usingBlock:^{
            [weakSelf whenDataReady];
        }];
    }
    _lastDate = date;

    block_ChainLastAppend(&_outChainLast, block);

    vlc_cond_signal(&_bufferWait);
    vlc_mutex_unlock(&_bufferLock);

    if (_renderer.status == AVQueuedSampleBufferRenderingStatusFailed)
    {
        msg_Err(_aout, "AVQueuedSampleBufferRenderingStatusFailed, restarting");
        aout_RestartRequest(_aout, false);
    }
}

- (void)stopSyncRenderer
{
    vlc_mutex_lock(&_bufferLock);
    _sync.rate = 0.0f;

    [_renderer stopRequestingMediaData];
    [_renderer flush];

    if (_dateReached)
        [_sync removeTimeObserver:_observer];

    _stopped = YES;

    block_ChainRelease(_outChain);
    _outChain = NULL;
    _outChainLast = &_outChain;

    vlc_cond_signal(&_bufferWait);
    vlc_mutex_unlock(&_bufferLock);

    /* From the doc: "Call dispatch_sync after removeTimeObserver: to wait for
     * any in-flight blocks to finish executing." */
    dispatch_sync(_timeQueue, ^{});

    /* Not in any doc:, stopRequestingMediaData() and flush() won't wait for
     * any blocks to finish executing, so wait here. */
     dispatch_sync(_dataQueue, ^{});
}

- (void)stop
{
    NSNotificationCenter *notifCenter = [NSNotificationCenter defaultCenter];

    if (_ptsSamples >= 0)
        [self stopSyncRenderer];

    [_sync removeRenderer:_renderer atTime:kCMTimeInvalid completionHandler:nil];

#ifdef HAS_AVAUDIOSESSION
    avas_SetActive(_aout, [AVAudioSession sharedInstance], false,
                   AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation);
#endif

    CFRelease(_fmtDesc);

    [notifCenter removeObserver:self];
}

- (void)flushedAutomatically:(NSNotification *)notification
{
    msg_Warn(_aout, "flushedAutomatically");
    aout_RestartRequest(_aout, false);
}

- (void)outputConfigurationChanged:(NSNotification *)notification
{
    msg_Warn(_aout, "outputConfigurationChanged");
    aout_RestartRequest(_aout, false);
}

- (BOOL)start:(audio_sample_format_t *)fmt
{
    if (aout_BitsPerSample(fmt->i_format) == 0)
        return NO; /* Can handle PT */

    NSNotificationCenter *notifCenter = [NSNotificationCenter defaultCenter];

    fmt->i_format = VLC_CODEC_FL32;

#ifdef HAS_AVAUDIOSESSION
    AVAudioSession *instance = [AVAudioSession sharedInstance];
    if (avas_SetActive(_aout, instance, true, 0) != VLC_SUCCESS)
        return NO;
    avas_PrepareFormat(_aout, instance, fmt, true);

    enum port_type port_type;
    if (avas_GetPortType(_aout, instance, &port_type) == VLC_SUCCESS)
    {
        msg_Dbg(_aout, "Output on %s, channel count: %u",
                port_type == PORT_TYPE_HDMI ? "HDMI" :
                port_type == PORT_TYPE_USB ? "USB" :
                port_type == PORT_TYPE_HEADPHONES ? "Headphones" : "Default",
                aout_FormatNbChannels(fmt));

        _aout->current_sink_info.headphones = port_type == PORT_TYPE_HEADPHONES;
    }
#endif

    AudioChannelLayout *inlayout_buf = NULL;
    size_t inlayout_size = 0;
    int err = channel_layout_MapFromVLC(_aout, fmt, &inlayout_buf,
                                        &inlayout_size);
    if (err != VLC_SUCCESS)
        goto error_avas;

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
                                       &desc,
                                       inlayout_size,
                                       inlayout_buf,
                                       0,
                                       nil,
                                       nil,
                                       &_fmtDesc);
    free(inlayout_buf);
    if (status != noErr)
    {
        msg_Warn(_aout, "CMAudioFormatDescriptionRef creation failure %li", (long)status);
        goto error_avas;
    }

    _renderer = [[AVSampleBufferAudioRenderer alloc] init];
    if (_renderer == nil)
        goto error;

    _sync = [[AVSampleBufferRenderSynchronizer alloc] init];
    if (_sync == nil)
    {
        _renderer = nil;
        goto error;
    }

    _sync.delaysRateChangeUntilHasSufficientMediaData = NO;
    [_sync addRenderer:_renderer];

    _stopped = NO;
    _dateReached = NO;

    _ptsSamples = -1;
    _firstPts = VLC_TICK_INVALID;
    _lastDate = VLC_TICK_INVALID;
    _sampleRate = fmt->i_rate;
    _bytesPerFrame = desc.mBytesPerFrame;

    [notifCenter addObserver:self
                    selector:@selector(flushedAutomatically:)
                        name:AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification
                      object:nil];
    if (@available(macOS 12.0, iOS 15.0, tvOS 15.0, watchOS 8.0, *))
    {
        [notifCenter addObserver:self
                        selector:@selector(outputConfigurationChanged:)
                            name:AVSampleBufferAudioRendererOutputConfigurationDidChangeNotification
                          object:nil];
    }

    return YES;
error:
    CFRelease(_fmtDesc);
error_avas:
#ifdef HAS_AVAUDIOSESSION
    avas_SetActive(_aout, instance, false,
                   AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation);
#endif
    return NO;
}

@end

static int API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS), watchos(MIN_WATCHOS) VISIONOS_API_AVAILABLE)
DeviceSelect(audio_output_t *aout, const char *name)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys selectDevice:name];

    return VLC_SUCCESS;
}

static int API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS), watchos(MIN_WATCHOS) VISIONOS_API_AVAILABLE)
MuteSet(audio_output_t *aout, bool mute)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys setMute:mute];

    return VLC_SUCCESS;
}

static int API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS), watchos(MIN_WATCHOS) VISIONOS_API_AVAILABLE)
VolumeSet(audio_output_t *aout, float volume)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys setVolume:volume];

    return VLC_SUCCESS;
}

static void API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS), watchos(MIN_WATCHOS) VISIONOS_API_AVAILABLE)
Flush(audio_output_t *aout)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys flush];
}

static void API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS), watchos(MIN_WATCHOS) VISIONOS_API_AVAILABLE)
Pause(audio_output_t *aout, bool pause, vlc_tick_t date)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys pause:pause date:date];
}

static void API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS), watchos(MIN_WATCHOS) VISIONOS_API_AVAILABLE)
Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys play:block date:date];
}

static void API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS), watchos(MIN_WATCHOS) VISIONOS_API_AVAILABLE)
Stop(audio_output_t *aout)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    [sys stop];
}

static int API_AVAILABLE(macos(MIN_MACOS), ios(MIN_IOS), tvos(MIN_TVOS), watchos(MIN_WATCHOS) VISIONOS_API_AVAILABLE)
Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    VLCAVSample *sys = (__bridge VLCAVSample*)aout->sys;

    return [sys start:fmt] ? VLC_SUCCESS : VLC_EGENERIC;
}

static void
Close(vlc_object_t *obj)
{
    if (@available(macOS MIN_MACOS, iOS MIN_IOS, tvOS MIN_TVOS, watchOS MIN_WATCHOS, *))
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

    if (@available(macOS MIN_MACOS, iOS MIN_IOS, tvOS MIN_TVOS, watchOS MIN_WATCHOS, *))
    {
        aout->sys = (__bridge_retained void*) [[VLCAVSample alloc] init:aout];
        if (aout->sys == nil)
            return VLC_EGENERIC;

        aout->start = Start;
        aout->stop = Stop;
        aout->play = Play;
        aout->pause = Pause;
        aout->flush = Flush;
        aout->volume_set = VolumeSet;
        aout->mute_set = MuteSet;
        aout->device_select = DeviceSelect;

        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

vlc_module_begin ()
    set_shortname("avsample")
    set_description(N_("AVSampleBufferAudioRenderer output"))
    set_capability("audio output", 100)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    set_callbacks(Open, Close)
vlc_module_end ()
