/*****************************************************************************
 * coreaudio_common.h: Common AudioUnit code for iOS and macOS
 *****************************************************************************
 * Copyright (C) 2005 - 2017 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_aout.h>
#import <vlc_threads.h>

#import <AudioUnit/AudioUnit.h>
#import <AudioToolbox/AudioToolbox.h>
#import <os/lock.h>
#import <mach/mach_time.h>

#define STREAM_FORMAT_MSG(pre, sfm) \
    pre "[%f][%4.4s][%u][%u][%u][%u][%u][%u]", \
    sfm.mSampleRate, (char *)&sfm.mFormatID, \
    (unsigned int)sfm.mFormatFlags, (unsigned int)sfm.mBytesPerPacket, \
    (unsigned int)sfm.mFramesPerPacket, (unsigned int)sfm.mBytesPerFrame, \
    (unsigned int)sfm.mChannelsPerFrame, (unsigned int)sfm.mBitsPerChannel

#define ca_LogErr(fmt) msg_Err(p_aout, fmt ", OSStatus: %d", (int) err)
#define ca_LogWarn(fmt) msg_Warn(p_aout, fmt ", OSStatus: %d", (int) err)

struct aout_sys_common
{
    /* The following is owned by common.c (initialized from ca_Open) */

    mach_timebase_info_data_t tinfo;

    size_t              i_underrun_size;
    bool                b_paused;
    bool                b_do_flush;

    size_t              i_out_max_size;
    size_t              i_out_size;
    bool                b_played;
    block_t             *p_out_chain;
    block_t             **pp_out_last;
    uint64_t            i_render_host_time;
    uint64_t            i_first_render_host_time;
    uint32_t            i_render_frames;

    vlc_sem_t           flush_sem;

    union lock
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
        os_unfair_lock  unfair;
#pragma clang diagnostic pop
        vlc_mutex_t     mutex;
    } lock;

    int                 i_rate;
    unsigned int        i_bytes_per_frame;
    unsigned int        i_frame_length;
    uint8_t             chans_to_reorder;
    uint8_t             chan_table[AOUT_CHAN_MAX];
    /* ca_TimeGet extra latency, in micro-seconds */
    vlc_tick_t          i_dev_latency_us;
};

int ca_Open(audio_output_t *p_aout);

void ca_Render(audio_output_t *p_aout, uint32_t i_nb_samples, uint64_t i_host_time,
               uint8_t *p_output, size_t i_requested);

int  ca_TimeGet(audio_output_t *p_aout, vlc_tick_t *delay);

void ca_Flush(audio_output_t *p_aout);

void ca_Pause(audio_output_t * p_aout, bool pause, vlc_tick_t date);

void ca_Play(audio_output_t * p_aout, block_t * p_block, vlc_tick_t date);

int  ca_Initialize(audio_output_t *p_aout, const audio_sample_format_t *fmt,
                   vlc_tick_t i_dev_latency_us);

void ca_Uninitialize(audio_output_t *p_aout);

void ca_SetAliveState(audio_output_t *p_aout, bool alive);

void ca_SetDeviceLatency(audio_output_t *p_aout, vlc_tick_t i_dev_latency_us);

AudioUnit au_NewOutputInstance(audio_output_t *p_aout, OSType comp_sub_type);

int  au_Initialize(audio_output_t *p_aout, AudioUnit au,
                   audio_sample_format_t *fmt,
                   const AudioChannelLayout *outlayout, vlc_tick_t i_dev_latency_us,
                   bool *warn_configuration);

void au_Uninitialize(audio_output_t *p_aout, AudioUnit au);
