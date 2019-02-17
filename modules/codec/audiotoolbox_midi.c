/*****************************************************************************
 * audiotoolbox_midi.c: Software MIDI synthesizer using AudioToolbox
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
 *
 * Based on the fluidsynth module by Rémi Denis-Courmont
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
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_dialog.h>

#include <CoreFoundation/CoreFoundation.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#include <TargetConditionals.h>

#ifndef on_err_goto
#define on_err_goto(errorCode, exceptionLabel)  \
do { if ((errorCode) != noErr) goto exceptionLabel; \
} while ( 0 )
#endif

#define SOUNDFONT_TEXT N_("SoundFont file")
#define SOUNDFONT_LONGTEXT N_( \
    "SoundFont file to use for software synthesis." )

static int  Open  (vlc_object_t *);
static void Close (vlc_object_t *);

#define CFG_PREFIX "aumidi-"

vlc_module_begin()
    set_description(N_("AudioToolbox MIDI synthesizer"))
    set_capability("audio decoder", 100)
    set_shortname(N_("AUMIDI"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACODEC)
    set_callbacks(Open, Close)
    add_loadfile(CFG_PREFIX "soundfont", "",
                 SOUNDFONT_TEXT, SOUNDFONT_LONGTEXT)
vlc_module_end()


typedef struct
{
    AUGraph     graph;
    AudioUnit   synthUnit;
    AudioUnit   outputUnit;
    date_t       end_date;
} decoder_sys_t;

static int  DecodeBlock (decoder_t *p_dec, block_t *p_block);
static void Flush (decoder_t *);

/* MIDI constants */
enum
{
    kMidiMessage_NoteOff            = 0x80,
    kMidiMessage_NoteOn             = 0x90,
    kMidiMessage_PolyPressure       = 0xA0,
    kMidiMessage_ControlChange      = 0xB0,
    kMidiMessage_ProgramChange      = 0xC0,
    kMidiMessage_ChannelPressure    = 0xD0,
    kMidiMessage_PitchWheel         = 0xE0,
    kMidiMessage_SysEx              = 0xF0,

    kMidiMessage_BankMSBControl     = 0,
    kMidiMessage_BankLSBControl     = 32,

    /* Values for kMidiMessage_ControlChange */
    kMidiController_AllSoundOff         = 0x78,
    kMidiController_ResetAllControllers = 0x79,
    kMidiController_AllNotesOff         = 0x7B
};

/* Helper functions */
static OSStatus AddAppleAUNode(AUGraph graph, OSType type, OSType subtype, AUNode *node)
{
    AudioComponentDescription cDesc = {};
    cDesc.componentType = type;
    cDesc.componentSubType = subtype;
    cDesc.componentManufacturer = kAudioUnitManufacturer_Apple;

    return AUGraphAddNode(graph, &cDesc, node);
}

static OSStatus CreateAUGraph(AUGraph *outGraph, AudioUnit *outSynth, AudioUnit *outOut)
{
    OSStatus res;

    // AudioUnit nodes
    AUNode synthNode, limiterNode, outNode;

    // Create the Graph to which we will add our nodes
    on_err_goto(res = NewAUGraph(outGraph), bailout);

    // Create/add the MIDI synthesizer node (DLS Synth)
#if TARGET_OS_IPHONE
    // On iOS/tvOS use MIDISynth, DLSSynth does not exist there
    on_err_goto(res = AddAppleAUNode(*outGraph,
                                     kAudioUnitType_MusicDevice,
                                     kAudioUnitSubType_MIDISynth,
                                     &synthNode), bailout);
#else
    // Prefer DLSSynth on macOS, as it has a better default behavior
    on_err_goto(res = AddAppleAUNode(*outGraph,
                                     kAudioUnitType_MusicDevice,
                                     kAudioUnitSubType_DLSSynth,
                                     &synthNode), bailout);
#endif

    // Create/add the peak limiter node
    on_err_goto(res = AddAppleAUNode(*outGraph,
                                     kAudioUnitType_Effect,
                                     kAudioUnitSubType_PeakLimiter,
                                     &limiterNode), bailout);

    // Create/add the output node (GenericOutput)
    on_err_goto(res = AddAppleAUNode(*outGraph,
                                     kAudioUnitType_Output,
                                     kAudioUnitSubType_GenericOutput,
                                     &outNode), bailout);

    // Open the Graph, this opens the units that belong to the graph
    // so that we can connect them
    on_err_goto(res = AUGraphOpen(*outGraph), bailout);

    // Connect the synthesizer node to the limiter
    on_err_goto(res = AUGraphConnectNodeInput(*outGraph, synthNode, 0, limiterNode, 0), bailout);
    // Connect the limiter node to the output
    on_err_goto(res = AUGraphConnectNodeInput(*outGraph, limiterNode, 0, outNode, 0), bailout);

    // Get reference to the synthesizer node
    on_err_goto(res = AUGraphNodeInfo(*outGraph, synthNode, 0, outSynth), bailout);
    // Get reference to the output node
    on_err_goto(res = AUGraphNodeInfo(*outGraph, outNode, 0, outOut), bailout);

bailout:
    return res;
}

static int SetSoundfont(decoder_t *p_dec, AudioUnit synthUnit, const char *sfPath) {
    if (!sfPath) {
        msg_Dbg(p_dec, "using default soundfont");
        return VLC_SUCCESS;
    }

    msg_Dbg(p_dec, "using custom soundfont: '%s'", sfPath);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                                           (const UInt8 *)sfPath,
                                                           strlen(sfPath), false);
    if (unlikely(url == NULL))
        return VLC_ENOMEM;

    OSStatus status = AudioUnitSetProperty(synthUnit,
                                           kMusicDeviceProperty_SoundBankURL,
                                           kAudioUnitScope_Global, 0,
                                           &url, sizeof(url));
    CFRelease(url);

    if (status != noErr) {
        msg_Err(p_dec, "failed setting custom SoundFont for MIDI synthesis (%i)", status);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    OSStatus status = noErr;
    int ret = VLC_SUCCESS;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_MIDI)
        return VLC_EGENERIC;

    decoder_sys_t *p_sys = malloc(sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->graph = NULL;
    status = CreateAUGraph(&p_sys->graph, &p_sys->synthUnit, &p_sys->outputUnit);
    if (unlikely(status != noErr)) {
        msg_Err(p_dec, "failed to create audiograph (%i)", status);
        ret = VLC_EGENERIC;
        goto bailout;
    }

    // Set custom soundfont
    char *sfPath = var_InheritString(p_dec, CFG_PREFIX "soundfont");
    ret = SetSoundfont(p_dec, p_sys->synthUnit, sfPath);
    free(sfPath);
    if (unlikely(ret != VLC_SUCCESS))
        goto bailout;

    // Set VLC output audio format info
    p_dec->fmt_out.i_codec = VLC_CODEC_FL32;
    p_dec->fmt_out.audio.i_bitspersample = 32;
    p_dec->fmt_out.audio.i_rate = 44100;
    p_dec->fmt_out.audio.i_channels = 2;
    p_dec->fmt_out.audio.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;

    if (decoder_UpdateAudioFormat(p_dec) < 0) {
        ret = VLC_EGENERIC;
        goto bailout;
    }

    // Prepare AudioUnit output audio format info
    AudioStreamBasicDescription ASBD = {};
    unsigned bytesPerSample = sizeof(Float32);
    ASBD.mFormatID = kAudioFormatLinearPCM;
    ASBD.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    ASBD.mSampleRate = 44100;
    ASBD.mFramesPerPacket = 1;
    ASBD.mChannelsPerFrame = 2;
    ASBD.mBytesPerFrame = bytesPerSample * ASBD.mChannelsPerFrame;
    ASBD.mBytesPerPacket = ASBD.mBytesPerFrame * ASBD.mFramesPerPacket;
    ASBD.mBitsPerChannel = 8 * bytesPerSample;

    // Set AudioUnit format
    status = AudioUnitSetProperty(p_sys->outputUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output, 0, &ASBD,
                                  sizeof(AudioStreamBasicDescription));
    if (unlikely(status != noErr)) {
        msg_Err(p_dec, "failed setting output format for output unit (%i)", status);
        ret = VLC_EGENERIC;
        goto bailout;
    }

    // Prepare the AU
    status = AUGraphInitialize (p_sys->graph);
    if (unlikely(status != noErr)) {
        if (status == kAudioUnitErr_InvalidFile)
            msg_Err(p_dec, "failed initializing audiograph: invalid soundfont file");
        else
            msg_Err(p_dec, "failed initializing audiograph (%i)", status);
        ret = VLC_EGENERIC;
        goto bailout;
    }

    // Prepare MIDI soundbank
    MusicDeviceMIDIEvent(p_sys->synthUnit,
                         kMidiMessage_ControlChange,
                         kMidiMessage_BankMSBControl, 0, 0);

    // Start the AU
    status = AUGraphStart(p_sys->graph);
    if (unlikely(status != noErr)) {
        msg_Err(p_dec, "failed starting audiograph (%i)", status);
        ret = VLC_EGENERIC;
        goto bailout;
    }

    // Initialize date (for PTS)
    date_Init(&p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1);

    p_dec->p_sys = p_sys;
    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = Flush;

bailout:
    // Cleanup if error occured
    if (ret != VLC_SUCCESS) {
        if (p_sys->graph)
            DisposeAUGraph(p_sys->graph);
        free(p_sys);
    }
    return ret;
}


static void Close (vlc_object_t *p_this)
{
    decoder_sys_t *p_sys = ((decoder_t *)p_this)->p_sys;

    if (p_sys->graph) {
        AUGraphStop(p_sys->graph);
        DisposeAUGraph(p_sys->graph);
    }
    free(p_sys);
}

static void Flush (decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set(&p_sys->end_date, VLC_TICK_INVALID);

    // Turn all sound on all channels off
    // else 'old' notes could still be playing
    for (unsigned channel = 0; channel < 16; channel++) {
        MusicDeviceMIDIEvent(p_sys->synthUnit, kMidiMessage_ControlChange | channel, kMidiController_AllSoundOff, 0, 0);
    }
}

static int DecodeBlock (decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_out = NULL;
    OSStatus status = noErr;

    if (p_block == NULL) /* No Drain */
        return VLCDEC_SUCCESS;

    if (p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED)) {
        Flush(p_dec);
        if (p_block->i_flags & BLOCK_FLAG_CORRUPTED) {
            block_Release(p_block);
            return VLCDEC_SUCCESS;
        }
    }

    if ( p_block->i_pts != VLC_TICK_INVALID &&
         date_Get(&p_sys->end_date) == VLC_TICK_INVALID ) {
        date_Set(&p_sys->end_date, p_block->i_pts);
    } else if (p_block->i_pts < date_Get(&p_sys->end_date)) {
        msg_Warn(p_dec, "MIDI message in the past?");
        goto drop;
    }

    if (p_block->i_buffer < 1)
        goto drop;

    uint8_t event = p_block->p_buffer[0];
    uint8_t data1 = (p_block->i_buffer > 1) ? (p_block->p_buffer[1]) : 0;
    uint8_t data2 = (p_block->i_buffer > 2) ? (p_block->p_buffer[2]) : 0;

    switch (event & 0xF0)
    {
        case kMidiMessage_NoteOff:
        case kMidiMessage_NoteOn:
        case kMidiMessage_PolyPressure:
        case kMidiMessage_ControlChange:
        case kMidiMessage_ProgramChange:
        case kMidiMessage_ChannelPressure:
        case kMidiMessage_PitchWheel:
            MusicDeviceMIDIEvent(p_sys->synthUnit, event, data1, data2, 0);
        break;

        case kMidiMessage_SysEx:
            if (p_block->i_buffer < UINT32_MAX)
                MusicDeviceSysEx(p_sys->synthUnit, p_block->p_buffer, (UInt32)p_block->i_buffer);
        break;

        default:
            msg_Warn(p_dec, "unhandled MIDI event: %x", event & 0xF0);
        break;
    }

    // Calculate frame count
    // Simplification of 44100 / 1000000
    // TODO: Other samplerates
    unsigned frames =
       (p_block->i_pts - date_Get(&p_sys->end_date)) * 441 / 10000;

    if (frames == 0)
        goto drop;

    p_out = decoder_NewAudioBuffer(p_dec, frames);
    if (p_out == NULL)
        goto drop;

    p_out->i_pts = date_Get(&p_sys->end_date );
    p_out->i_length = date_Increment(&p_sys->end_date, frames)
                      - p_out->i_pts;

    // Prepare Timestamp for the AudioUnit render call
    AudioTimeStamp timestamp = {};
    timestamp.mFlags = kAudioTimeStampWordClockTimeValid;
    timestamp.mWordClockTime = p_out->i_pts;

    // Prepare Buffer for the AudioUnit render call
    AudioBufferList bufferList;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mNumberChannels = 2;
    bufferList.mBuffers[0].mDataByteSize = frames * sizeof(Float32) * 2;
    bufferList.mBuffers[0].mData = p_out->p_buffer;

    status = AudioUnitRender(p_sys->outputUnit,
                             NULL,
                             &timestamp, 0,
                             frames, &bufferList);

    if (status != noErr) {
        msg_Warn(p_dec, "rendering audio unit failed: %i", status);
        block_Release(p_out);
        p_out = NULL;
    }

drop:
    block_Release(p_block);
    if (p_out != NULL)
        decoder_QueueAudio(p_dec, p_out);
    return VLCDEC_SUCCESS;
}
