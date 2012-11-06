/*****************************************************************************
 * grain.c: add film grain
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <assert.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_cpu.h>

#include <vlc_rand.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define BANK_SIZE (64)

#define CFG_PREFIX "grain-"

#define VARIANCE_MIN        (0.0)
#define VARIANCE_MAX        (10.0)
#define VARIANCE_TEXT       N_("Variance")
#define VARIANCE_LONGTEXT   N_("Variance of the gaussian noise")

#define PERIOD_MIN          1
#define PERIOD_MAX          BANK_SIZE
#define PERIOD_MIN_TEXT     N_("Minimal period")
#define PERIOD_MIN_LONGTEXT N_("Minimal period of the noise grain in pixel")
#define PERIOD_MAX_TEXT     N_("Maximal period")
#define PERIOD_MAX_LONGTEXT N_("Maximal period of the noise grain in pixel")

vlc_module_begin()
    set_description(N_("Grain video filter"))
    set_shortname( N_("Grain"))
    set_help(N_("Adds filtered gaussian noise"))
    set_capability( "video filter2", 0 )
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    add_float_with_range(CFG_PREFIX "variance", 2.0, VARIANCE_MIN, VARIANCE_MAX,
                         VARIANCE_TEXT, VARIANCE_LONGTEXT, false)
    add_integer_with_range(CFG_PREFIX "period-min", 1, PERIOD_MIN, PERIOD_MAX,
                           PERIOD_MIN_TEXT, PERIOD_MIN_LONGTEXT, false)
    add_integer_with_range(CFG_PREFIX "period-max", 3*PERIOD_MAX/4, PERIOD_MIN, PERIOD_MAX,
                           PERIOD_MAX_TEXT, PERIOD_MAX_LONGTEXT, false)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

#define BLEND_SIZE (8)
struct filter_sys_t {
    bool     is_uv_filtered;
    uint32_t seed;

    int      scale;
    int16_t  bank[BANK_SIZE * BANK_SIZE];
    int16_t  bank_y[BANK_SIZE * BANK_SIZE];
    int16_t  bank_uv[BANK_SIZE * BANK_SIZE];

    void (*blend)(uint8_t *dst, size_t dst_pitch,
                  const uint8_t *src, size_t src_pitch,
                  const int16_t *noise);
    void (*emms)(void);

    struct {
        vlc_mutex_t lock;
        double      variance;
    } cfg;
};

/* Simple and *really fast* RNG (xorshift[13,17,5])*/
#define URAND_SEED (2463534242)
static uint32_t urand(uint32_t *seed)
{
    uint32_t s = *seed;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return *seed = s;
}
/* Uniform random value between 0 and 1 */
static double drand(uint32_t *seed)
{
    return urand(seed) / (double)UINT32_MAX;
}
/* Gaussian random value with a mean of 0 and a variance of 1 */
static void grand(double *r1, double *r2, uint32_t *seed)
{
    double s;
    double u1, u2;
    do {
        u1 = 2 * drand(seed) - 1;
        u2 = 2 * drand(seed) - 1;
        s = u1 * u1 + u2 * u2;
    } while (s >= 1.0);

    s = sqrt(-2 * log(s) / s);
    *r1 = u1 * s;
    *r2 = u2 * s;
}

static void BlockBlend(uint8_t *dst, size_t dst_pitch,
                       const uint8_t *src, size_t src_pitch,
                       const int16_t *noise,
                       int w, int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            dst[y * dst_pitch + x] =
                clip_uint8_vlc(src[y * src_pitch + x] + noise[y * BANK_SIZE +x]);
        }
    }
}

static void BlockBlendC(uint8_t *dst, size_t dst_pitch,
                        const uint8_t *src, size_t src_pitch,
                        const int16_t *noise)
{
    BlockBlend(dst, dst_pitch, src, src_pitch, noise,
               BLEND_SIZE, BLEND_SIZE);
}

#ifdef CAN_COMPILE_SSE2
#define _STRING(x) #x
#define STRING(x) _STRING(x)
VLC_SSE
static void BlockBlendSse2(uint8_t *dst, size_t dst_pitch,
                           const uint8_t *src, size_t src_pitch,
                           const int16_t *noise)
{
#if BLEND_SIZE == 8
    /* TODO It is possible to do the math on 8 bits using
     * paddusb X  and then psubusb -X.
     */
    asm volatile ("pxor %%xmm0, %%xmm0\n" : :);
    for (int i = 0; i < 8/2; i++) {
        asm volatile (
            "movq       (%[src1]),   %%xmm1\n"
            "movq       (%[src2]),   %%xmm3\n"
            "movdqu     (%[noise]), %%xmm2\n"
            "movdqu 2*"STRING(BANK_SIZE)"(%[noise]), %%xmm4\n"

            "punpcklbw  %%xmm0,     %%xmm1\n"
            "punpcklbw  %%xmm0,     %%xmm3\n"

            "paddsw    %%xmm2,     %%xmm1\n"
            "paddsw    %%xmm4,     %%xmm3\n"
            "packuswb   %%xmm1,     %%xmm1\n"
            "packuswb   %%xmm3,     %%xmm3\n"
            "movq       %%xmm1,     (%[dst1])\n"
            "movq       %%xmm3,     (%[dst2])\n"
            : : [dst1]"r"(&dst[(2*i+0) * dst_pitch]),
                [dst2]"r"(&dst[(2*i+1) * dst_pitch]),
                [src1]"r"(&src[(2*i+0) * src_pitch]),
                [src2]"r"(&src[(2*i+1) * src_pitch]),
                [noise]"r"(&noise[2*i * BANK_SIZE])
            : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "memory");
    }
#else
#   error "BLEND_SIZE unsupported"
#endif
}
static void Emms(void)
{
    asm volatile ("emms");
}
#endif

/**
 * Scale the given signed data (on 7 bits + 1 for sign) using scale on 8 bits.
 */
static void Scale(int16_t *dst, int16_t *src, int scale)
{
    const int N = BANK_SIZE;
    const int shift = 7 + 8;

    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            const int v = src[y * N + x];
            int vq;
            if (v >= 0)
                vq =   ( v * scale + (1 << (shift-1)) - 1) >> shift;
            else
                vq = -((-v * scale + (1 << (shift-1)) - 1) >> shift);
            dst[y * N + x] = vq;
        }
    }
}

static void PlaneFilter(filter_t *filter,
                        plane_t *dst, const plane_t *src,
                        int16_t *bank, uint32_t *seed)
{
    filter_sys_t *sys = filter->p_sys;

    for (int y = 0; y < dst->i_visible_lines; y += BLEND_SIZE) {
        for (int x = 0; x < dst->i_visible_pitch; x += BLEND_SIZE) {
            int bx = urand(seed) % (BANK_SIZE - BLEND_SIZE + 1);
            int by = urand(seed) % (BANK_SIZE - BLEND_SIZE + 1);
            const int16_t *noise = &bank[by * BANK_SIZE + bx];

            int w  = dst->i_visible_pitch - x;
            int h  = dst->i_visible_lines - y;

            const uint8_t *srcp = &src->p_pixels[y * src->i_pitch + x];
            uint8_t       *dstp = &dst->p_pixels[y * dst->i_pitch + x];

            if (w >= BLEND_SIZE && h >= BLEND_SIZE)
                sys->blend(dstp, dst->i_pitch, srcp, src->i_pitch, noise);
            else
                BlockBlend(dstp, dst->i_pitch, srcp, src->i_pitch, noise,
                           __MIN(w, BLEND_SIZE), __MIN(h, BLEND_SIZE));
        }
    }
    if (sys->emms)
        sys->emms();
}

static picture_t *Filter(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;

    picture_t *dst = filter_NewPicture(filter);
    if (!dst) {
        picture_Release(src);
        return NULL;
    }

    vlc_mutex_lock(&sys->cfg.lock);
    const double variance = VLC_CLIP(sys->cfg.variance, VARIANCE_MIN, VARIANCE_MAX);
    vlc_mutex_unlock(&sys->cfg.lock);

    const int scale = 256 * sqrt(variance);
    if (scale != sys->scale) {
        sys->scale = scale;
        Scale(sys->bank_y,  sys->bank, sys->scale);
        Scale(sys->bank_uv, sys->bank, sys->scale / 2);
    }

    for (int i = 0; i < dst->i_planes; i++) {
        const plane_t *srcp = &src->p[i];
        plane_t       *dstp = &dst->p[i];

        if (i == 0 || sys->is_uv_filtered) {
            int16_t *bank = i == 0 ? sys->bank_y :
                                     sys->bank_uv;
            PlaneFilter(filter, dstp, srcp, bank, &sys->seed);
        }
        else {
            plane_CopyPixels(dstp, srcp);
        }
    }

    picture_CopyProperties(dst, src);
    picture_Release(src);
    return dst;
}

/**
 * Generate a filteried gaussian noise within [-127, 127] range.
 */
static int Generate(int16_t *bank, int h_min, int h_max, int v_min, int v_max)
{
    const int N = BANK_SIZE;
    double *workspace = calloc(3 * N * N, sizeof(*workspace));
    if (!workspace)
        return VLC_ENOMEM;

    double *gn        = &workspace[0 * N * N];
    double *cij       = &workspace[1 * N * N];
    double *tmp       = &workspace[2 * N * N];

    /* Create a gaussian noise matrix */
    assert((N % 2) == 0);
    uint32_t seed = URAND_SEED;
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N/2; x++) {
            grand(&gn[y * N + 2 * x + 0], &gn[y * N + 2 * x + 1], &seed);
        }
    }

    /* Clear non selected frequency.
     * Only the central band is kept */
    int zero = 0;
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            if ((x < h_min && y < v_min) || x > h_max || y > v_max) {
                gn[y * N + x] = 0.0;
                zero++;
            }
        }
    }
    const double correction = sqrt((double)N * N  / (N * N - zero));

    /* Filter the gaussian noise using an IDCT
     * The algo is simple/stupid and does C * GN * Ct */
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            cij[i * N + j] = i == 0 ? sqrt(1.0f / N) :
                                      sqrt(2.0f / N) * cos((2 * j + 1) * i * M_PI / 2 / N);
        }
    }

    //mtime_t tmul_0 = mdate();
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double v = 0.0;
            for (int k = 0; k < N; k++)
                v += gn[i * N + k] * cij[k * N + j];
            tmp[i * N + j] = v;
        }
    }
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double v = 0.0;
            for (int k = 0; k < N; k++)
                v += cij[k * N + i] * tmp[k * N + j];
            /* Do not bias when rounding */
            int vq;
            if (v >= 0)
                vq =  (int)( v * correction * 127 + 0.5);
            else
                vq = -(int)(-v * correction * 127 + 0.5);
            bank[i * N + j] = VLC_CLIP(vq, INT16_MIN, INT16_MAX);
        }
    }
    //mtime_t mul_duration = mdate() - tmul_0;
    //fprintf(stderr, "IDCT took %d ms\n", (int)(mul_duration / 1000));

    free(workspace);
    return VLC_SUCCESS;
}

static int Callback(vlc_object_t *object, char const *cmd,
                    vlc_value_t oldval, vlc_value_t newval, void *data)
{
    filter_t     *filter = (filter_t *)object;
    filter_sys_t *sys = filter->p_sys;
    VLC_UNUSED(cmd); VLC_UNUSED(oldval); VLC_UNUSED(data);

    vlc_mutex_lock(&sys->cfg.lock);
    sys->cfg.variance = newval.f_float;
    vlc_mutex_unlock(&sys->cfg.lock);

    return VLC_SUCCESS;
}

static int Open(vlc_object_t *object)
{
    filter_t *filter = (filter_t *)object;

    const vlc_chroma_description_t *chroma =
        vlc_fourcc_GetChromaDescription(filter->fmt_in.video.i_chroma);
    if (!chroma || chroma->plane_count < 3 || chroma->pixel_size != 1) {
        msg_Err(filter, "Unsupported chroma (%4.4s)",
                (char*)&(filter->fmt_in.video.i_chroma));
        return VLC_EGENERIC;
    }

    filter_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    sys->is_uv_filtered = true;
    sys->scale          = -1;
    sys->seed           = URAND_SEED;

    int cutoff_low = BANK_SIZE - var_InheritInteger(filter, CFG_PREFIX "period-max");
    int cutoff_high= BANK_SIZE - var_InheritInteger(filter, CFG_PREFIX "period-min");
    cutoff_low  = VLC_CLIP(cutoff_low, 1, BANK_SIZE - 1);
    cutoff_high = VLC_CLIP(cutoff_high, 1, BANK_SIZE - 1);
    if (Generate(sys->bank, cutoff_low, cutoff_high, cutoff_low, cutoff_high)) {
        free(sys);
        return VLC_EGENERIC;
    }

    sys->blend = BlockBlendC;
    sys->emms  = NULL;
#if defined(CAN_COMPILE_SSE2) && 1
    if (vlc_CPU_SSE2()) {
        sys->blend = BlockBlendSse2;
        sys->emms  = Emms;
    }
#endif

    vlc_mutex_init(&sys->cfg.lock);
    sys->cfg.variance = var_CreateGetFloatCommand(filter, CFG_PREFIX "variance");
    var_AddCallback(filter, CFG_PREFIX "variance", Callback, NULL);

    filter->p_sys           = sys;
    filter->pf_video_filter = Filter;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    filter_t     *filter = (filter_t *)object;
    filter_sys_t *sys    = filter->p_sys;

    var_DelCallback(filter, CFG_PREFIX "variance", Callback, NULL);
    vlc_mutex_destroy(&sys->cfg.lock);
    free(sys);
}

