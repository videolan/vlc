/*****************************************************************************
 * avaudiosession_common.h: AVAudioSession common code for iOS aouts
 *****************************************************************************
 * Copyright (C) 2012 - 2024 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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

enum port_type
{
    PORT_TYPE_DEFAULT,
    PORT_TYPE_USB,
    PORT_TYPE_HDMI,
    PORT_TYPE_HEADPHONES
};

void
avas_PrepareFormat(audio_output_t *p_aout, AVAudioSession *instance,
                   audio_sample_format_t *fmt, bool spatial_audio);

int
avas_GetPortType(audio_output_t *p_aout, AVAudioSession *instance,
                 enum port_type *pport_type);

int
avas_SetActive(audio_output_t *p_aout, AVAudioSession *instance, bool active,
               NSUInteger options);
