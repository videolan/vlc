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

#include "directx_va.h"

extern const GUID DXVA2_ModeMPEG2_VLD;
extern const GUID DXVA2_ModeMPEG2and1_VLD;
extern const GUID DXVA2_ModeH264_E;
extern const GUID DXVA2_ModeH264_F;
extern const GUID DXVA_Intel_H264_NoFGT_ClearVideo;
extern const GUID DXVA_ModeH264_VLD_WithFMOASO_NoFGT;
extern const GUID DXVA_ModeH264_VLD_NoFGT_Flash;
extern const GUID DXVA2_ModeVC1_D;
extern const GUID DXVA2_ModeVC1_D2010;
extern const GUID DXVA_ModeHEVC_VLD_Main10;
extern const GUID DXVA_ModeHEVC_VLD_Main;
extern const GUID DXVA_ModeVP9_VLD_Profile0;
extern const GUID DXVA_ModeVP9_VLD_10bit_Profile2;

enum DriverTestCommand {
    BLAnyDriver,
    BLBelowBuild, /* driverBuild is the first driver version known to work */
};

struct decoders {
    const UINT deviceID;
    const GUID **decoder_list;
    const enum DriverTestCommand cmd;
    const UINT driverBuild;
};

static const GUID *NoHEVC[] = {
    &DXVA_ModeHEVC_VLD_Main,
    &DXVA_ModeHEVC_VLD_Main10,
    NULL,
};

static const GUID *AnyDecoder[] = {
    &DXVA2_ModeMPEG2_VLD,
    &DXVA2_ModeMPEG2and1_VLD,
    &DXVA2_ModeH264_E,
    &DXVA2_ModeH264_F,
    &DXVA_Intel_H264_NoFGT_ClearVideo,
    &DXVA_ModeH264_VLD_WithFMOASO_NoFGT,
    &DXVA_ModeH264_VLD_NoFGT_Flash,
    &DXVA2_ModeVC1_D,
    &DXVA2_ModeVC1_D2010,
    &DXVA_ModeHEVC_VLD_Main,
    &DXVA_ModeHEVC_VLD_Main10,
    &DXVA_ModeVP9_VLD_Profile0,
    &DXVA_ModeVP9_VLD_10bit_Profile2,
    NULL,
};

static struct decoders IntelDevices[] = {
    /* Intel Broadwell GPUs with hybrid HEVC */
    { 0x1606, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics */
    { 0x160E, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics */
    { 0x1612, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics 5600 */
    { 0x1616, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics 5500 */
    { 0x161A, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics P5700 */
    { 0x161E, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics 5300 */
    { 0x1622, NoHEVC, BLAnyDriver, 0 }, /* Iris Pro Graphics 6200 */
    { 0x1626, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics 6000 */
    { 0x162A, NoHEVC, BLAnyDriver, 0 }, /* Iris Pro Graphics P6300 */
    { 0x162B, NoHEVC, BLAnyDriver, 0 }, /* Iris Graphics 6100 */

    { 0x0402, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics */
    { 0x0406, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics */
    { 0x040A, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics */
    { 0x0412, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics 4600 */
    { 0x0416, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics 4600 */
    { 0x041E, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics 4400 */
    { 0x041A, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics P4600/P4700 */

    { 0x0A06, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics */
    { 0x0A0E, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics */
    { 0x0A16, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics Family */
    { 0x0A1E, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics Family */
    { 0x0A26, NoHEVC, BLAnyDriver, 0 }, /* HD Graphics 5000 */
    { 0x0A2E, NoHEVC, BLAnyDriver, 0 }, /* Iris(TM) Graphics 5100 */

    { 0x0D22, NoHEVC, BLAnyDriver, 0 }, /* Iris(TM) Pro Graphics 5200 */
    { 0x0D26, NoHEVC, BLAnyDriver, 0 }, /* Iris(TM) Pro Graphics 5200 */

    /* Intel Eaglelake/GMA X4500 too old to decode properly */
    { 0x2A42, AnyDecoder, BLAnyDriver, 0 }, /* Mobile Intel(R) 4 Series Express Chipset Family */
    { 0x2A43, AnyDecoder, BLAnyDriver, 0 }, /* Mobile Intel(R) 4 Series Express Chipset Family */
    { 0x2E02, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) 4 Series Internal Chipset */
    { 0x2E03, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) 4 Series Internal Chipset */
    { 0x2E12, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) Q45/Q43 Express Chipset */
    { 0x2E13, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) Q45/Q43 Express Chipset */
    { 0x2E22, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) G45/G43 Express Chipset */
    { 0x2E23, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) G45/G43 Express Chipset */
    { 0x2E32, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) G41 Express Chipset */
    { 0x2E33, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) G41 Express Chipset */
    { 0x2E42, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) B43 Express Chipset */
    { 0x2E43, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) B43 Express Chipset */
    { 0x2E92, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) B43 Express Chipset */
    { 0x2E93, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) B43 Express Chipset */

    { 0x29D3, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) Q33 Express Chipset Family */
    { 0x29D2, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) Q33 Express Chipset Family */
    { 0x29B3, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) Q35 Express Chipset Family */
    { 0x29B2, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) Q35 Express Chipset Family */
    { 0x29C3, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) G33/G31 Express Chipset Family */
    { 0x29C2, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) G33/G31 Express Chipset Family */
    { 0x2A13, AnyDecoder, BLAnyDriver, 0 }, /* Mobile Intel(R) 965 Express Chipset Family */
    { 0x2A12, AnyDecoder, BLAnyDriver, 0 }, /* Mobile Intel(R) 965 Express Chipset Family */
    { 0x2A03, AnyDecoder, BLAnyDriver, 0 }, /* Mobile Intel(R) 965 Express Chipset Family */
    { 0x2A02, AnyDecoder, BLAnyDriver, 0 }, /* Mobile Intel(R) 965 Express Chipset Family */
    { 0x2973, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R)  946GZ Express Chipset Family */
    { 0x2972, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R)  946GZ Express Chipset Family */
    { 0x29A3, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R)  G965 Express Chipset Family */
    { 0x29A2, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R)  G965 Express Chipset Family */
    { 0x2993, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R)  Q965/Q963 Express Chipset Family */
    { 0x2992, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R)  Q965/Q963 Express Chipset Family */
    { 0x2983, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R)  G35 Express Chipset Family */
    { 0x2982, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R)  G35 Express Chipset Family */
    { 0x2772, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) 82945G Express Chipset Family */
    { 0x2776, AnyDecoder, BLAnyDriver, 0 }, /* Intel(R) 82945G Express Chipset Family */
    { 0x27A2, AnyDecoder, BLAnyDriver, 0 }, /* Mobile Intel(R) 945 Express Chipset Family */
    { 0x27A6, AnyDecoder, BLAnyDriver, 0 }, /* Mobile Intel(R) 945 Express Chipset Family */
    { 0x27AE, AnyDecoder, BLAnyDriver, 0 }, /* Mobile Intel(R) 945 Express Chipset Family */

    {0, NULL, BLAnyDriver, 0}
};

static struct {
    const UINT vendor;
    const struct decoders *devices;
} gpu_blacklist[] = {
    { .vendor = GPU_MANUFACTURER_INTEL, .devices = IntelDevices },
};

bool directx_va_canUseDecoder(vlc_va_t *va, UINT VendorId, UINT DeviceId, const GUID *pCodec, UINT driverBuild)
{
    if (va->obj.force)
        return true;

    for (size_t i=0; i<ARRAY_SIZE(gpu_blacklist); i++)
    {
        if (gpu_blacklist[i].vendor == VendorId)
        {
            const struct decoders *pDevice = gpu_blacklist[i].devices;
            while (pDevice->deviceID != 0)
            {
                if (pDevice->deviceID == DeviceId)
                {
                    const GUID **pGuid = pDevice->decoder_list;
                    while (*pGuid != NULL)
                    {
                        if (IsEqualGUID(pCodec, *pGuid))
                        {
                            if (pDevice->cmd == BLAnyDriver)
                                return false;
                            if (pDevice->cmd == BLBelowBuild && driverBuild < pDevice->driverBuild)
                                return false;
                            break;
                        }
                        pGuid++;
                    }
                    return true;
                }
                pDevice++;
            }
            break;
        }
    }

    return true;
}
