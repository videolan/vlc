/*****************************************************************************
 * directx_va.c: DirectX Generic Video Acceleration helpers
 *****************************************************************************
 * Copyright © 2018 VLC authors and VideoLAN, VideoLabs
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_codecs.h>
#include <vlc_codec.h>

#include "../../video_chroma/dxgi_fmt.h"

#define D3D_DecoderType     IUnknown
#define D3D_DecoderDevice   IUnknown
#define D3D_DecoderSurface  IUnknown

struct picture_sys_t
{
    void *dummy;
};

#include "directx_va.h"

static UINT IntelDevices[] = {
    /* Intel Broadwell GPUs with hybrid HEVC */
    0x1606, /* HD Graphics */
    0x160E, /* HD Graphics */
    0x1612, /* HD Graphics 5600 */
    0x1616, /* HD Graphics 5500 */
    0x161A, /* HD Graphics P5700 */
    0x161E, /* HD Graphics 5300 */
    0x1622, /* Iris Pro Graphics 6200 */
    0x1626, /* HD Graphics 6000 */
    0x162A, /* Iris Pro Graphics P6300 */
    0x162B, /* Iris Graphics 6100 */

    0x0402, /* HD Graphics */
    0x0406, /* HD Graphics */
    0x040A, /* HD Graphics */
    0x0412, /* HD Graphics 4600 */
    0x0416, /* HD Graphics 4600 */
    0x041E, /* HD Graphics 4400 */
    0x041A, /* HD Graphics P4600/P4700 */

    0x0A06, /* HD Graphics */
    0x0A0E, /* HD Graphics */
    0x0A16, /* HD Graphics Family */
    0x0A1E, /* HD Graphics Family */
    0x0A26, /* HD Graphics 5000 */
    0x0A2E, /* Iris(TM) Graphics 5100 */

    0x0D22, /* Iris(TM) Pro Graphics 5200 */
    0x0D26, /* Iris(TM) Pro Graphics 5200 */
    0
};

static struct {
    UINT vendor;
    const UINT *devices;
} hevc_blacklist[] = {
    { .vendor = GPU_MANUFACTURER_INTEL, .devices = IntelDevices },
};

bool directx_va_canUseHevc(vlc_va_t *va, UINT VendorId, UINT DeviceId)
{
    if (va->obj.force)
        return true;

    for (size_t i=0; i<ARRAY_SIZE(hevc_blacklist); i++)
    {
        if (hevc_blacklist[i].vendor == VendorId)
        {
            const UINT *pDevice = hevc_blacklist[i].devices;
            while (*pDevice != 0)
            {
                if (*pDevice == DeviceId)
                {
                    msg_Warn(va, "Intel Hybrid HEVC detected, disabling hardware decoding");
                    return false;
                }
                pDevice++;
            }
            break;
        }
    }

    return true;
}
