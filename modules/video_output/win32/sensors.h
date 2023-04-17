/*****************************************************************************
 * sensors.h: Windows sensor handling
 *****************************************************************************
 * Copyright © 2017 Steve Lhomme
 * Copyright © 2017 VideoLabs
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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


#ifndef VLC_WIN32_COMMON_SENSOR_H
#define VLC_WIN32_COMMON_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

void* HookWindowsSensors(struct vlc_logger *, const vout_display_owner_t *, HWND);
void UnhookWindowsSensors(void*);

# ifdef __cplusplus
}
# endif

#endif // VLC_WIN32_COMMON_SENSOR_H
