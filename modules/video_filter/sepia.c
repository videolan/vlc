/*****************************************************************************
 * sepia.c : Sepia video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2010 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Branko Kokanovic <branko.kokanovic@gmail.com>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_cpu.h>
#include <vlc_atomic.h>

#include <assert.h>
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create      ( vlc_object_t * );
static void Destroy     ( vlc_object_t * );

static void RVSepia( picture_t *, picture_t *, int );
static void PlanarI420Sepia( picture_t *, picture_t *, int);
static void PackedYUVSepia( picture_t *, picture_t *, int);
static picture_t *Filter( filter_t *, picture_t * );
static const char *const ppsz_filter_options[] = {
    "intensity", NULL
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SEPIA_INTENSITY_TEXT N_("Sepia intensity")
#define SEPIA_INTENSITY_LONGTEXT N_("Intensity of sepia effect" )

#define CFG_PREFIX "sepia-"

vlc_module_begin ()
    set_description( N_("Sepia video filter") )
    set_shortname( N_("Sepia" ) )
    set_help( N_("Gives video a warmer tone by applying sepia effect") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )
    add_integer_with_range( CFG_PREFIX "intensity", 120, 0, 255,
                           SEPIA_INTENSITY_TEXT, SEPIA_INTENSITY_LONGTEXT,
                           false )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * callback prototypes
 *****************************************************************************/
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

typedef void (*SepiaFunction)( picture_t *, picture_t *, int );

static const struct
{
    vlc_fourcc_t i_chroma;
    SepiaFunction pf_sepia;
} p_sepia_cfg[] = {
    { VLC_CODEC_I420, PlanarI420Sepia },
    { VLC_CODEC_RGB24, RVSepia },
    { VLC_CODEC_RGB32, RVSepia },
    { VLC_CODEC_UYVY, PackedYUVSepia },
    { VLC_CODEC_VYUY, PackedYUVSepia },
    { VLC_CODEC_YUYV, PackedYUVSepia },
    { VLC_CODEC_YVYU, PackedYUVSepia },
    { 0, NULL }
};

/*****************************************************************************
 * filter_sys_t: adjust filter method descriptor
 *****************************************************************************/
struct filter_sys_t
{
    SepiaFunction pf_sepia;
    atomic_int i_intensity;
};

/*****************************************************************************
 * Create: allocates Sepia video thread output method
 *****************************************************************************
 * This function allocates and initializes a Sepia vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->pf_sepia = NULL;

    for( int i = 0; p_sepia_cfg[i].i_chroma != 0; i++ )
    {
        if( p_sepia_cfg[i].i_chroma != p_filter->fmt_in.video.i_chroma )
            continue;
        p_sys->pf_sepia = p_sepia_cfg[i].pf_sepia;
    }

    if( p_sys->pf_sepia == NULL )
    {
        msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                (char*)&(p_filter->fmt_in.video.i_chroma) );
        free( p_sys );
        return VLC_EGENERIC;
    }

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    atomic_init( &p_sys->i_intensity,
             var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "intensity" ) );
    var_AddCallback( p_filter, CFG_PREFIX "intensity", FilterCallback, NULL );

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy sepia video thread output method
 *****************************************************************************
 * Terminate an output method
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    var_DelCallback( p_filter, CFG_PREFIX "intensity", FilterCallback, NULL );

    free( p_filter->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to sepia image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;

    if( !p_pic ) return NULL;

    filter_sys_t *p_sys = p_filter->p_sys;
    int intensity = atomic_load( &p_sys->i_intensity );

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        picture_Release( p_pic );
        return NULL;
    }

    p_sys->pf_sepia( p_pic, p_outpic, intensity );

    return CopyInfoAndRelease( p_outpic, p_pic );
}

#if defined(CAN_COMPILE_SSE2)
/*****************************************************************************
 * Sepia8ySSE2
 *****************************************************************************
 * This function applies sepia effect to eight bytes of yellow using SSE4.1
 * instructions. It copies those 8 bytes to 128b register and fills the gaps
 * with zeroes and following operations are made with word-operating instructs.
 *****************************************************************************/
VLC_SSE
static inline void Sepia8ySSE2(uint8_t * dst, const uint8_t * src,
                         int i_intensity_spread)
{
    __asm__ volatile (
        // y = y - y / 4 + i_intensity / 4
        "movq            (%1), %%xmm1\n"
        "punpcklbw     %%xmm7, %%xmm1\n"
        "movq            (%1), %%xmm2\n" // store bytes as words with 0s in between
        "punpcklbw     %%xmm7, %%xmm2\n"
        "movd              %2, %%xmm3\n"
        "pshufd    $0, %%xmm3, %%xmm3\n"
        "psrlw             $2, %%xmm2\n"    // rotate right 2
        "psubusb       %%xmm1, %%xmm2\n"    // subtract
        "psrlw             $2, %%xmm3\n"
        "paddsb        %%xmm1, %%xmm3\n"    // add
        "packuswb      %%xmm2, %%xmm1\n"    // pack back to bytes
        "movq          %%xmm1, (%0)  \n"    // load to dest
        :
        :"r" (dst), "r"(src), "r"(i_intensity_spread)
        :"memory", "xmm1", "xmm2", "xmm3");
}

VLC_SSE
static void PlanarI420SepiaSSE( picture_t *p_pic, picture_t *p_outpic,
                                int i_intensity )
{
    /* prepared values to copy for U and V channels */
    const uint8_t filling_const_8u = 128 - i_intensity / 6;
    const uint8_t filling_const_8v = 128 + i_intensity / 14;
    /* prepared value for faster broadcasting in xmm register */
    int i_intensity_spread = 0x10001 * (uint8_t) i_intensity;

    __asm__ volatile(
        "pxor      %%xmm7, %%xmm7\n"
        ::: "xmm7");

    /* iterate for every two visible line in the frame */
    for (int y = 0; y < p_pic->p[Y_PLANE].i_visible_lines - 1; y += 2)
    {
        const int i_dy_line1_start = y * p_outpic->p[Y_PLANE].i_pitch;
        const int i_dy_line2_start = (y + 1) * p_outpic->p[Y_PLANE].i_pitch;
        const int i_du_line_start =  (y / 2) * p_outpic->p[U_PLANE].i_pitch;
        const int i_dv_line_start =  (y / 2) * p_outpic->p[V_PLANE].i_pitch;
        int x = 0;
        /* iterate for every visible line in the frame (eight values at once) */
        for ( ; x < p_pic->p[Y_PLANE].i_visible_pitch - 15; x += 16 )
        {
            /* Compute yellow channel values with asm function */
            Sepia8ySSE2(&p_outpic->p[Y_PLANE].p_pixels[i_dy_line1_start + x],
                        &p_pic->p[Y_PLANE].p_pixels[i_dy_line1_start + x],
                        i_intensity_spread );
            Sepia8ySSE2(&p_outpic->p[Y_PLANE].p_pixels[i_dy_line2_start + x],
                        &p_pic->p[Y_PLANE].p_pixels[i_dy_line2_start + x],
                        i_intensity_spread );
            Sepia8ySSE2(&p_outpic->p[Y_PLANE].p_pixels[i_dy_line1_start + x + 8],
                        &p_pic->p[Y_PLANE].p_pixels[i_dy_line1_start + x + 8],
                        i_intensity_spread );
            Sepia8ySSE2(&p_outpic->p[Y_PLANE].p_pixels[i_dy_line2_start + x + 8],
                        &p_pic->p[Y_PLANE].p_pixels[i_dy_line2_start + x + 8],
                        i_intensity_spread );
            /* Copy precomputed values to destination memory location */
            memset(&p_outpic->p[U_PLANE].p_pixels[i_du_line_start + (x / 2)],
                   filling_const_8u, 8 );
            memset(&p_outpic->p[V_PLANE].p_pixels[i_dv_line_start + (x / 2)],
                   filling_const_8v, 8 );
        }
        /* Completing the job, the cycle above takes really big chunks, so
           this makes sure the job will be done completely */
        for ( ; x < p_pic->p[Y_PLANE].i_visible_pitch - 1; x += 2 )
        {
            // y = y - y/4 {to prevent overflow} + intensity / 4
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line1_start + x] =
                p_pic->p[Y_PLANE].p_pixels[i_dy_line1_start + x] -
                (p_pic->p[Y_PLANE].p_pixels[i_dy_line1_start + x] >> 2) +
                (i_intensity >> 2);
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line1_start + x + 1] =
                p_pic->p[Y_PLANE].p_pixels[i_dy_line1_start + x + 1] -
                (p_pic->p[Y_PLANE].p_pixels[i_dy_line1_start + x + 1] >> 2) +
                (i_intensity >> 2);
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line2_start + x] =
                p_pic->p[Y_PLANE].p_pixels[i_dy_line2_start + x] -
                (p_pic->p[Y_PLANE].p_pixels[i_dy_line2_start + x] >> 2) +
                (i_intensity >> 2);
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line2_start + x + 1] =
                p_pic->p[Y_PLANE].p_pixels[i_dy_line2_start + x + 1] -
                (p_pic->p[Y_PLANE].p_pixels[i_dy_line2_start + x + 1] >> 2) +
                (i_intensity >> 2);
            // u = 128 {half => B&W} - intensity / 6
            p_outpic->p[U_PLANE].p_pixels[i_du_line_start + (x / 2)] =
                filling_const_8u;
            // v = 128 {half => B&W} + intensity / 14
            p_outpic->p[V_PLANE].p_pixels[i_dv_line_start + (x / 2)] =
                filling_const_8v;
        }
    }
}
#endif

/*****************************************************************************
 * PlanarI420Sepia: Applies sepia to one frame of the planar I420 video
 *****************************************************************************
 * This function applies sepia effect to one frame of the video by iterating
 * through video lines. We iterate for every two lines and for every two pixels
 * in line to calculate new sepia values for four y components as well for u
 * and v components.
 *****************************************************************************/
static void PlanarI420Sepia( picture_t *p_pic, picture_t *p_outpic,
                               int i_intensity )
{
#if defined(CAN_COMPILE_SSE2)
    if (vlc_CPU_SSE2())
        return PlanarI420SepiaSSE( p_pic, p_outpic, i_intensity );
#endif

    // prepared values to copy for U and V channels
    const uint8_t filling_const_8u = 128 - i_intensity / 6;
    const uint8_t filling_const_8v = 128 + i_intensity / 14;

    /* iterate for every two visible line in the frame */
    for( int y = 0; y < p_pic->p[Y_PLANE].i_visible_lines - 1; y += 2)
    {
        const int i_dy_line1_start = y * p_outpic->p[Y_PLANE].i_pitch;
        const int i_dy_line2_start = ( y + 1 ) * p_outpic->p[Y_PLANE].i_pitch;
        const int i_du_line_start = (y/2) * p_outpic->p[U_PLANE].i_pitch;
        const int i_dv_line_start = (y/2) * p_outpic->p[V_PLANE].i_pitch;
        // to prevent sigsegv if one pic is smaller (theoretically)
        int i_picture_size_limit = p_pic->p[Y_PLANE].i_visible_pitch
                  < p_outpic->p[Y_PLANE].i_visible_pitch
                  ? (p_pic->p[Y_PLANE].i_visible_pitch - 1) :
                  (p_outpic->p[Y_PLANE].i_visible_pitch - 1);
        /* iterate for every two visible line in the frame */
        for( int x = 0; x < i_picture_size_limit; x += 2)
        {
            // y = y - y/4 {to prevent overflow} + intensity / 4
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line1_start + x] =
                p_pic->p[Y_PLANE].p_pixels[i_dy_line1_start + x] -
                (p_pic->p[Y_PLANE].p_pixels[i_dy_line1_start + x] >> 2) +
                (i_intensity >> 2);
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line1_start + x + 1] =
                p_pic->p[Y_PLANE].p_pixels[i_dy_line1_start + x + 1] -
                (p_pic->p[Y_PLANE].p_pixels[i_dy_line1_start + x + 1] >> 2) +
                (i_intensity >> 2);
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line2_start + x] =
                p_pic->p[Y_PLANE].p_pixels[i_dy_line2_start + x] -
                (p_pic->p[Y_PLANE].p_pixels[i_dy_line2_start + x] >> 2) +
                (i_intensity >> 2);
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line2_start + x + 1] =
                p_pic->p[Y_PLANE].p_pixels[i_dy_line2_start + x + 1] -
                (p_pic->p[Y_PLANE].p_pixels[i_dy_line2_start + x + 1] >> 2) +
                (i_intensity >> 2);
            // u = 128 {half => B&W} - intensity / 6
            p_outpic->p[U_PLANE].p_pixels[i_du_line_start + (x / 2)] =
                filling_const_8u;
            // v = 128 {half => B&W} + intensity / 14
            p_outpic->p[V_PLANE].p_pixels[i_dv_line_start + (x / 2)] =
                filling_const_8v;
        }
    }
}

/*****************************************************************************
 * PackedYUVSepia: Applies sepia to one frame of the packed YUV video
 *****************************************************************************
 * This function applies sepia effext to one frame of the video by iterating
 * through video lines. In every pass, we calculate new values for pixels
 * (UYVY, VYUY, YUYV and YVYU formats are supported)
 *****************************************************************************/
static void PackedYUVSepia( picture_t *p_pic, picture_t *p_outpic,
                           int i_intensity )
{
    uint8_t *p_in, *p_in_end, *p_line_end, *p_out;
    int i_yindex = 1, i_uindex = 2, i_vindex = 0;

    GetPackedYuvOffsets( p_outpic->format.i_chroma,
                        &i_yindex, &i_uindex, &i_vindex );

    // prepared values to copy for U and V channels
    const uint8_t filling_const_8u = 128 - i_intensity / 6;
    const uint8_t filling_const_8v = 128 + i_intensity / 14;

    p_in = p_pic->p[0].p_pixels;
    p_in_end = p_in + p_pic->p[0].i_visible_lines
        * p_pic->p[0].i_pitch;
    p_out = p_outpic->p[0].p_pixels;

    {
        while( p_in < p_in_end )
        {
            p_line_end = p_in + p_pic->p[0].i_visible_pitch;
            while( p_in < p_line_end )
            {
                /* calculate new, sepia values */
                p_out[i_yindex] =
                    p_in[i_yindex] - (p_in[i_yindex] >> 2) + (i_intensity >> 2);
                p_out[i_yindex + 2] =
                    p_in[i_yindex + 2] - (p_in[i_yindex + 2] >> 2)
                    + (i_intensity >> 2);
                p_out[i_uindex] = filling_const_8u;
                p_out[i_vindex] = filling_const_8v;
                p_in += 4;
                p_out += 4;
            }
            p_in += p_pic->p[0].i_pitch - p_pic->p[0].i_visible_pitch;
            p_out += p_outpic->p[0].i_pitch
                - p_outpic->p[0].i_visible_pitch;
        }
    }
}

/*****************************************************************************
 * RVSepia: Applies sepia to one frame of the RV24/RV32 video
 *****************************************************************************
 * This function applies sepia effect to one frame of the video by iterating
 * through video lines and calculating new values for every byte in chunks of
 * 3 (RV24) or 4 (RV32) bytes.
 *****************************************************************************/
static void RVSepia( picture_t *p_pic, picture_t *p_outpic, int i_intensity )
{
#define SCALEBITS 10
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))
    uint8_t *p_in, *p_in_end, *p_line_end, *p_out;
    bool b_isRV32 = p_pic->format.i_chroma == VLC_CODEC_RGB32;
    int i_rindex = 0, i_gindex = 1, i_bindex = 2;

    GetPackedRgbIndexes( &p_outpic->format, &i_rindex, &i_gindex, &i_bindex );

    p_in = p_pic->p[0].p_pixels;
    p_in_end = p_in + p_pic->p[0].i_visible_lines
        * p_pic->p[0].i_pitch;
    p_out = p_outpic->p[0].p_pixels;

    /* Precompute values constant for this certain i_intensity, using the same
     * formula as YUV functions above */
    uint8_t r_intensity = (( FIX( 1.40200 * 255.0 / 224.0 ) * (i_intensity * 14)
                        + ONE_HALF )) >> SCALEBITS;
    uint8_t g_intensity = (( - FIX(0.34414*255.0/224.0) * ( - i_intensity / 6 )
                        - FIX( 0.71414 * 255.0 / 224.0) * ( i_intensity * 14 )
                        + ONE_HALF )) >> SCALEBITS;
    uint8_t b_intensity = (( FIX( 1.77200 * 255.0 / 224.0) * ( - i_intensity / 6 )
                        + ONE_HALF )) >> SCALEBITS;

    while (p_in < p_in_end)
    {
        p_line_end = p_in + p_pic->p[0].i_visible_pitch;
        while (p_in < p_line_end)
        {
            /* do sepia: this calculation is based on the formula to calculate
             * YUV->RGB and RGB->YUV (in filter_picture.h) mode and that
             * y = y - y/4 + intensity/4 . As Y is the only channel that changes
             * through the whole image. After that, precomputed values are added
             * for each RGB channel and saved in the output image.
             * FIXME: needs cleanup */
            uint8_t i_y = ((( 66 * p_in[i_rindex] + 129 * p_in[i_gindex] +  25
                      * p_in[i_bindex] + 128 ) >> 8 ) * FIX(255.0/219.0))
                      - (((( 66 * p_in[i_rindex] + 129 * p_in[i_gindex] + 25
                      * p_in[i_bindex] + 128 ) >> 8 )
                      * FIX( 255.0 / 219.0 )) >> 2 ) + ( i_intensity >> 2 );
            p_out[i_rindex] = vlc_uint8(i_y + r_intensity);
            p_out[i_gindex] = vlc_uint8(i_y + g_intensity);
            p_out[i_bindex] = vlc_uint8(i_y + b_intensity);
            p_in += 3;
            p_out += 3;
            /* for rv32 we take 4 chunks at the time */
            if (b_isRV32) {
            /* alpha channel stays the same */
            *p_out++ = *p_in++;
            }
        }

        p_in += p_pic->p[0].i_pitch - p_pic->p[0].i_visible_pitch;
        p_out += p_outpic->p[0].i_pitch
            - p_outpic->p[0].i_visible_pitch;
    }
#undef SCALEBITS
#undef ONE_HALF
#undef FIX
}

static int FilterCallback ( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    atomic_store( &p_sys->i_intensity, newval.i_int );
    return VLC_SUCCESS;
}
