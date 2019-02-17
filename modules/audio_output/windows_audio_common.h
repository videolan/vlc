/*****************************************************************************
 * windows_audio_common.h: Windows Audio common code
 *****************************************************************************
 * Copyright (C) 2001-2009 VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include <windows.h>
#include <mmsystem.h>

/*****************************************************************************
 * DirectSound GUIDs.
 * Defining them here allows us to get rid of the dxguid library during
 * the linking stage.
 *****************************************************************************/

#define INITGUID /* Doesn't define the DEFINE_GUID as extern */
#include <initguid.h>

#include <vlc_codecs.h>

DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF, WAVE_FORMAT_DOLBY_AC3_SPDIF, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );

static const GUID __KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID __KSDATAFORMAT_SUBTYPE_PCM = {WAVE_FORMAT_PCM, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID __KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF = {WAVE_FORMAT_DOLBY_AC3_SPDIF, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

#define FRAMES_NUM 8                                      /* Needs to be > 3 */

#include <dsound.h>

#ifndef SPEAKER_FRONT_LEFT
#   define SPEAKER_FRONT_LEFT             0x1
#   define SPEAKER_FRONT_RIGHT            0x2
#   define SPEAKER_FRONT_CENTER           0x4
#   define SPEAKER_LOW_FREQUENCY          0x8
#   define SPEAKER_BACK_LEFT              0x10
#   define SPEAKER_BACK_RIGHT             0x20
#   define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#   define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#   define SPEAKER_BACK_CENTER            0x100
#   define SPEAKER_SIDE_LEFT              0x200
#   define SPEAKER_SIDE_RIGHT             0x400
#   define SPEAKER_TOP_CENTER             0x800
#   define SPEAKER_TOP_FRONT_LEFT         0x1000
#   define SPEAKER_TOP_FRONT_CENTER       0x2000
#   define SPEAKER_TOP_FRONT_RIGHT        0x4000
#   define SPEAKER_TOP_BACK_LEFT          0x8000
#   define SPEAKER_TOP_BACK_CENTER        0x10000
#   define SPEAKER_TOP_BACK_RIGHT         0x20000
#   define SPEAKER_RESERVED               0x80000000
#endif

#ifndef DSSPEAKER_DSSPEAKER_DIRECTOUT
#   define DSSPEAKER_DSSPEAKER_DIRECTOUT  0x00000000
#endif
#ifndef DSSPEAKER_HEADPHONE
#   define DSSPEAKER_HEADPHONE            0x00000001
#endif
#ifndef DSSPEAKER_MONO
#   define DSSPEAKER_MONO                 0x00000002
#endif
#ifndef DSSPEAKER_QUAD
#   define DSSPEAKER_QUAD                 0x00000003
#endif
#ifndef DSSPEAKER_STEREO
#   define DSSPEAKER_STEREO               0x00000004
#endif
#ifndef DSSPEAKER_SURROUND
#   define DSSPEAKER_SURROUND             0x00000005
#endif
#ifndef DSSPEAKER_5POINT1
#   define DSSPEAKER_5POINT1              0x00000006
#endif
#ifndef DSSPEAKER_5POINT1_BACK
#   define DSSPEAKER_5POINT1_BACK         DSSPEAKER_5POINT1
#endif
#ifndef DSSPEAKER_7POINT1
#   define DSSPEAKER_7POINT1              0x00000007
#endif
#ifndef DSSPEAKER_7POINT1_SURROUND
#   define DSSPEAKER_7POINT1_SURROUND     0x00000008
#endif
#ifndef DSSPEAKER_5POINT1_SURROUND
#   define DSSPEAKER_5POINT1_SURROUND     0x00000009
#endif
#ifndef DSSPEAKER_7POINT1_WIDE
#   define DSSPEAKER_7POINT1_WIDE         DSSPEAKER_7POINT1
#endif

static const uint32_t pi_channels_in[] =
    { SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT,
      SPEAKER_SIDE_LEFT, SPEAKER_SIDE_RIGHT,
      SPEAKER_BACK_LEFT, SPEAKER_BACK_RIGHT, SPEAKER_BACK_CENTER,
      SPEAKER_FRONT_CENTER, SPEAKER_LOW_FREQUENCY, 0 };
static const uint32_t pi_channels_out[] =
    { SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT,
      SPEAKER_FRONT_CENTER, SPEAKER_LOW_FREQUENCY,
      SPEAKER_BACK_LEFT, SPEAKER_BACK_RIGHT,
      SPEAKER_BACK_CENTER,
      SPEAKER_SIDE_LEFT, SPEAKER_SIDE_RIGHT, 0 };

#define FLOAT_TEXT N_("Use float32 output")
#define FLOAT_LONGTEXT N_( \
    "The option allows you to enable or disable the high-quality float32 " \
    "audio output mode (which is not well supported by some soundcards)." )

