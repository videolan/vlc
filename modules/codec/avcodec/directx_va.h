/*****************************************************************************
 * directx_va.h: DirectX Generic Video Acceleration helpers
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * Copyright (C) 2009 Laurent Aimar
 * Copyright (C) 2015 Steve Lhomme
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Steve Lhomme <robux4@gmail.com>
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

#ifndef AVCODEC_DIRECTX_VA_H
#define AVCODEC_DIRECTX_VA_H

#include <vlc_common.h>

#include <libavcodec/avcodec.h>
#include "va.h"

#include <unknwn.h>
#include <stdatomic.h>

#include "va_surface.h"

#include <dxva.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct input_list_t {
    void (*pf_release)(struct input_list_t *);
    GUID *list;
    unsigned count;
} input_list_t;

typedef struct {
    const char   *name;
    const GUID   *guid;
    int           bit_depth;
    struct {
        uint8_t log2_chroma_w;
        uint8_t log2_chroma_h;
    };
    enum AVCodecID codec;
    const int    *p_profiles; // NULL or ends with 0
    int           workaround;
} directx_va_mode_t;

#define MAX_SURFACE_COUNT (64)
typedef struct
{
    /**
     * Read the list of possible input GUIDs
     */
    int (*pf_get_input_list)(vlc_va_t *, input_list_t *);
    /**
     * Find a suitable decoder configuration for the input and set the
     * internal state to use that output
     */
    int (*pf_setup_output)(vlc_va_t *, const directx_va_mode_t *, const video_format_t *fmt);

} directx_sys_t;

const directx_va_mode_t * directx_va_Setup(vlc_va_t *, const directx_sys_t *, const AVCodecContext *, const AVPixFmtDescriptor *,
                                           const es_format_t *, int flag_xbox,
                                           video_format_t *fmt_out, unsigned *surface_count);
bool directx_va_canUseDecoder(vlc_va_t *, UINT VendorId, UINT DeviceId, const GUID *pCodec, UINT driverBuild);

#ifdef _MSC_VER
// MSVC should have all the DXVA_xxx GUIDs but not the few DXVA2_xxx ones we
// need depending on the configuration (they don't have DXVA_xxx equivalents)
# if HAVE_LIBAVCODEC_DXVA2_H

// nothing to do, dxva2api.h will have them

# elif HAVE_LIBAVCODEC_D3D11VA_H

#  define DXVA2_ModeMPEG2_VLD     D3D11_DECODER_PROFILE_MPEG2_VLD
#  define DXVA2_ModeMPEG2_MoComp  D3D11_DECODER_PROFILE_MPEG2_MOCOMP
#  define DXVA2_ModeMPEG2_IDCT    D3D11_DECODER_PROFILE_MPEG2_IDCT

# endif // !HAVE_LIBAVCODEC_xxx

#elif defined(__MINGW64_VERSION_MAJOR) // mingw-w64 doesn't have all the standard GUIDs
# if HAVE_LIBAVCODEC_DXVA2_H

// redirect missing DXVA_xxx to existing DXVA2_xxx variants
#  define DXVA_ModeMPEG1_VLD       DXVA2_ModeMPEG1_VLD
#  define DXVA_ModeMPEG2and1_VLD   DXVA2_ModeMPEG2and1_VLD

#  define DXVA_ModeH264_A          DXVA2_ModeH264_A
#  define DXVA_ModeH264_B          DXVA2_ModeH264_B
#  define DXVA_ModeH264_C          DXVA2_ModeH264_C
#  define DXVA_ModeH264_D          DXVA2_ModeH264_D
#  define DXVA_ModeH264_E          DXVA2_ModeH264_E
#  define DXVA_ModeH264_F          DXVA2_ModeH264_F

#  define DXVA_ModeH264_VLD_Stereo_Progressive_NoFGT  DXVA2_ModeH264_VLD_Stereo_Progressive_NoFGT
#  define DXVA_ModeH264_VLD_Stereo_NoFGT              DXVA2_ModeH264_VLD_Stereo_NoFGT
#  define DXVA_ModeH264_VLD_Multiview_NoFGT           DXVA2_ModeH264_VLD_Multiview_NoFGT

#  define DXVA_ModeWMV8_A          DXVA2_ModeWMV8_A
#  define DXVA_ModeWMV8_B          DXVA2_ModeWMV8_B

#  define DXVA_ModeWMV9_A          DXVA2_ModeWMV9_A
#  define DXVA_ModeWMV9_B          DXVA2_ModeWMV9_B
#  define DXVA_ModeWMV9_C          DXVA2_ModeWMV9_C

#  define DXVA_ModeVC1_A           DXVA2_ModeVC1_A
#  define DXVA_ModeVC1_B           DXVA2_ModeVC1_B
#  define DXVA_ModeVC1_C           DXVA2_ModeVC1_C
#  define DXVA_ModeVC1_D           DXVA2_ModeVC1_D
#  define DXVA_ModeVC1_D2010       DXVA2_ModeVC1_D2010

#  define DXVA_ModeMPEG4pt2_VLD_Simple           DXVA2_ModeMPEG4pt2_VLD_Simple
#  define DXVA_ModeMPEG4pt2_VLD_AdvSimple_NoGMC  DXVA2_ModeMPEG4pt2_VLD_AdvSimple_NoGMC
#  define DXVA_ModeMPEG4pt2_VLD_AdvSimple_GMC    DXVA2_ModeMPEG4pt2_VLD_AdvSimple_GMC

#  define DXVA_ModeHEVC_VLD_Main    DXVA2_ModeHEVC_VLD_Main
#  define DXVA_ModeHEVC_VLD_Main10  DXVA2_ModeHEVC_VLD_Main10

#  define DXVA_ModeVP8_VLD                 DXVA2_ModeVP8_VLD
#  define DXVA_ModeVP9_VLD_Profile0        DXVA2_ModeVP9_VLD_Profile0
#  define DXVA_ModeVP9_VLD_10bit_Profile2  DXVA2_ModeVP9_VLD_10bit_Profile2

# elif HAVE_LIBAVCODEC_D3D11VA_H && __MINGW64_VERSION_MAJOR > 11

// redirect missing DXVA_xxx to existing D3D11_DECODER_PROFILE_xxx variants
#  define DXVA2_ModeMPEG2_VLD     D3D11_DECODER_PROFILE_MPEG2_VLD
#  define DXVA2_ModeMPEG2_MoComp  D3D11_DECODER_PROFILE_MPEG2_MOCOMP
#  define DXVA2_ModeMPEG2_IDCT    D3D11_DECODER_PROFILE_MPEG2_IDCT
#  define DXVA_ModeMPEG1_VLD      D3D11_DECODER_PROFILE_MPEG1_VLD
#  define DXVA_ModeMPEG2and1_VLD  D3D11_DECODER_PROFILE_MPEG2and1_VLD

#  define DXVA_ModeH264_A         D3D11_DECODER_PROFILE_H264_MOCOMP_NOFGT
#  define DXVA_ModeH264_B         D3D11_DECODER_PROFILE_H264_MOCOMP_FGT
#  define DXVA_ModeH264_C         D3D11_DECODER_PROFILE_H264_IDCT_NOFGT
#  define DXVA_ModeH264_D         D3D11_DECODER_PROFILE_H264_IDCT_FGT
#  define DXVA_ModeH264_E         D3D11_DECODER_PROFILE_H264_VLD_NOFGT
#  define DXVA_ModeH264_F         D3D11_DECODER_PROFILE_H264_VLD_FGT

#  define DXVA_ModeH264_VLD_Stereo_Progressive_NoFGT  D3D11_DECODER_PROFILE_H264_VLD_STEREO_PROGRESSIVE_NOFGT
#  define DXVA_ModeH264_VLD_Stereo_NoFGT              D3D11_DECODER_PROFILE_H264_VLD_STEREO_NOFGT
#  define DXVA_ModeH264_VLD_Multiview_NoFGT           D3D11_DECODER_PROFILE_H264_VLD_MULTIVIEW_NOFGT

#  define DXVA_ModeWMV8_A         D3D11_DECODER_PROFILE_WMV8_POSTPROC
#  define DXVA_ModeWMV8_B         D3D11_DECODER_PROFILE_WMV8_MOCOMP

#  define DXVA_ModeWMV9_A         D3D11_DECODER_PROFILE_WMV9_POSTPROC
#  define DXVA_ModeWMV9_B         D3D11_DECODER_PROFILE_WMV9_MOCOMP
#  define DXVA_ModeWMV9_C         D3D11_DECODER_PROFILE_WMV9_IDCT

#  define DXVA_ModeVC1_A          D3D11_DECODER_PROFILE_VC1_POSTPROC
#  define DXVA_ModeVC1_B          D3D11_DECODER_PROFILE_VC1_MOCOMP
#  define DXVA_ModeVC1_C          D3D11_DECODER_PROFILE_VC1_IDCT
#  define DXVA_ModeVC1_D          D3D11_DECODER_PROFILE_VC1_VLD
#  define DXVA_ModeVC1_D2010      D3D11_DECODER_PROFILE_VC1_D2010

#  define DXVA_ModeHEVC_VLD_Main    D3D11_DECODER_PROFILE_HEVC_VLD_MAIN
#  define DXVA_ModeHEVC_VLD_Main10  D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10

#  define DXVA_ModeVP8_VLD                 D3D11_DECODER_PROFILE_VP8_VLD
#  define DXVA_ModeVP9_VLD_Profile0        D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0
#  define DXVA_ModeVP9_VLD_10bit_Profile2  D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2

# endif // !HAVE_LIBAVCODEC_xxx

#endif // !__MINGW64_VERSION_MAJOR && !_MSC_VER

#ifdef __cplusplus
}
#endif

#endif /* AVCODEC_DIRECTX_VA_H */
