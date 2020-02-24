/*****************************************************************************
 * placebo_utils.h: Definition of various libplacebo helpers
 *****************************************************************************
 * Copyright (C) 2018 Niklas Haas
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

#ifndef VLC_PLACEBO_UTILS_H
#define VLC_PLACEBO_UTILS_H 1

#include <vlc_common.h>
#include <vlc_picture.h>

#include <libplacebo/colorspace.h>
#include <libplacebo/shaders/colorspace.h>
#include <libplacebo/utils/upload.h>

// Create a libplacebo context, hooked up to the log system; or NULL on OOM
struct pl_context *vlc_placebo_Create(vlc_object_t *);

// Turn a video_format_t into the equivalent libplacebo values
struct pl_color_space vlc_placebo_ColorSpace(const video_format_t *);
struct pl_color_repr vlc_placebo_ColorRepr(const video_format_t *);
enum pl_chroma_location vlc_placebo_ChromaLoc(const video_format_t *);

// Fill a pl_plane_data array with various data. Returns the number of planes,
// or 0 if the format is unsupported by the libplacebo API. If `buf` is set,
// then all addresses of the picture_t must lie within `buf`'s mapped memory.
int vlc_placebo_PlaneFormat(const video_format_t *, struct pl_plane_data[4]);
int vlc_placebo_PlaneData(const picture_t *, struct pl_plane_data[4],
                          const struct pl_buf *buf);

// See if a given FourCC is physically supported by a given GPU
bool vlc_placebo_FormatSupported(const struct pl_gpu *, vlc_fourcc_t);

// Shared options strings/structs for libplacebo options

#define RENDER_INTENT_TEXT "Rendering intent for color conversion"
#define RENDER_INTENT_LONGTEXT "The mapping type used to convert between color spaces."

static const int intent_values[] = {
    PL_INTENT_PERCEPTUAL,
    PL_INTENT_RELATIVE_COLORIMETRIC,
    PL_INTENT_SATURATION,
    PL_INTENT_ABSOLUTE_COLORIMETRIC,
};

static const char * const intent_text[] = {
    "Perceptual",
    "Relative colorimetric",
    "Absolute colorimetric",
    "Saturation",
};

#define PRIM_TEXT "Display primaries"
#define PRIM_LONGTEXT "The primaries associated with the output display"

static const int prim_values[] = {
    PL_COLOR_PRIM_UNKNOWN,
    PL_COLOR_PRIM_BT_601_525,
    PL_COLOR_PRIM_BT_601_625,
    PL_COLOR_PRIM_BT_709,
    PL_COLOR_PRIM_BT_470M,
    PL_COLOR_PRIM_BT_2020,
    PL_COLOR_PRIM_APPLE,
    PL_COLOR_PRIM_ADOBE,
    PL_COLOR_PRIM_PRO_PHOTO,
    PL_COLOR_PRIM_CIE_1931,
    PL_COLOR_PRIM_DCI_P3,
    PL_COLOR_PRIM_V_GAMUT,
    PL_COLOR_PRIM_S_GAMUT,
    PL_COLOR_PRIM_DISPLAY_P3,
};

static const char * const prim_text[] = {
    "Automatic / Unknown primaries",
    "ITU-R Rec. BT.601 (525-line = NTSC, SMPTE-C)",
    "ITU-R Rec. BT.601 (625-line = PAL, SECAM)",
    "ITU-R Rec. BT.709 (HD), also sRGB",
    "ITU-R Rec. BT.470 M",
    "ITU-R Rec. BT.2020 (UltraHD)",
    "Apple RGB",
    "Adobe RGB (1998)",
    "ProPhoto RGB (ROMM)",
    "CIE 1931 RGB primaries",
    "DCI-P3 (Digital Cinema)",
    "Panasonic V-Gamut (VARICAM)",
    "Sony S-Gamut",
    "Display-P3 (Digital Cinema with D65)",
};

#define TRC_TEXT "Display gamma / transfer function"
#define TRC_LONGTEXT "The gamma/transfer function associated with the output display"

static const int trc_values[] = {
    PL_COLOR_TRC_UNKNOWN,
    PL_COLOR_TRC_BT_1886,
    PL_COLOR_TRC_SRGB,
    PL_COLOR_TRC_LINEAR,
    PL_COLOR_TRC_GAMMA18,
    PL_COLOR_TRC_GAMMA22,
    PL_COLOR_TRC_GAMMA28,
    PL_COLOR_TRC_PRO_PHOTO,
    PL_COLOR_TRC_PQ,
    PL_COLOR_TRC_HLG,
    PL_COLOR_TRC_V_LOG,
    PL_COLOR_TRC_S_LOG1,
    PL_COLOR_TRC_S_LOG2,
};

static const char * const trc_text[] = {
    "Automatic / Unknown gamma",
    "ITU-R Rec. BT.1886 (CRT emulation + OOTF)",
    "IEC 61966-2-4 sRGB (CRT emulation)",
    "Linear light content",
    "Pure power gamma 1.8",
    "Pure power gamma 2.2",
    "Pure power gamma 2.8",
    "ProPhoto RGB (ROMM)",
    "ITU-R BT.2100 PQ (perceptual quantizer), aka SMPTE ST2084",
    "ITU-R BT.2100 HLG (hybrid log-gamma), aka ARIB STD-B67",
    "Panasonic V-Log (VARICAM)",
    "Sony S-Log1",
    "Sony S-Log2",
};


#define TONEMAPPING_TEXT "Tone-mapping algorithm"
#define TONEMAPPING_LONGTEXT "Algorithm to use when converting from wide gamut to standard gamut, or from HDR to SDR."

static const int tone_values[] = {
    PL_TONE_MAPPING_HABLE,
    PL_TONE_MAPPING_MOBIUS,
    PL_TONE_MAPPING_REINHARD,
    PL_TONE_MAPPING_GAMMA,
    PL_TONE_MAPPING_LINEAR,
    PL_TONE_MAPPING_CLIP,
};

static const char * const tone_text[] = {
    "Hable (filmic mapping, recommended)",
    "Mobius (linear + knee)",
    "Reinhard (simple non-linear)",
    "Gamma-Power law",
    "Linear stretch (peak to peak)",
    "Hard clip out-of-gamut",
};

#define TONEMAP_PARAM_TEXT "Tone-mapping parameter"
#define TONEMAP_PARAM_LONGTEXT "This parameter can be used to tune the tone-mapping curve. Specifics depend on the curve used. If left as 0, the curve's preferred default is used."

#define TONEMAP_DESAT_TEXT "Tone-mapping desaturation coefficient"
#define TONEMAP_DESAT_LONGTEXT "How strongly to desaturate bright spectral colors towards white. 0.0 disables this behavior."

#define DESAT_STRENGTH_TEXT "Desaturation strength"
#define DESAT_STRENGTH_LONGTEXT "How strongly to desaturate bright spectral colors towards white. 0.0 disables this behavior, 1.0 enables full desaturation (hollywood-style)"

#define DESAT_EXPONENT_TEXT "Desaturation exponent"
#define DESAT_EXPONENT_LONGTEXT "Controls the steepness of the desaturation curve. If you set this to 0.0, the curve will be flat, i.e. desaturation always enabled (hollywood-style)."

#define DESAT_BASE_TEXT "Desaturation base"
#define DESAT_BASE_LONGTEXT "Controls the starting offset of the desaturation curve. Brightness values below this base will always be colorimetrically tone mapped (never desaturated)."

#define MAX_BOOST_TEXT "Maximum brightness boost"
#define MAX_BOOST_LONGTEXT "Maximum allowed brightness boost to compensate for dark scenes. A value of 1.0 means no brightness boost is allowed."

#define GAMUT_WARN_TEXT "Highlight clipped pixels"
#define GAMUT_WARN_LONGTEXT "Debugging tool to indicate which pixels were clipped as part of the tone mapping process."

#define PEAK_FRAMES_TEXT "HDR peak detection buffer size"
#define PEAK_FRAMES_LONGTEXT "How many input frames to consider when determining the brightness of HDR signals. Higher values result in a slower/smoother response to brightness level changes. Setting this to 0 disables peak detection entirely."

#define PEAK_PERIOD_TEXT "HDR peak detection period"
#define PEAK_PERIOD_LONGTEXT "This option enables peak detection with the specified smoothing period. A value of 0.0 disables peak detection. Higher values result in a larger smoothing period which means the detected values will be stable over a longer number of frames, at the cost of responding more slowly to changes in scene brightness"

#define TARGET_AVG_TEXT "Target peak brightness average"
#define TARGET_AVG_LONGTEXT "If the source frame has an average brightness exceeding this number, the frame will be automatically darkened to compensate. This feature only works when peak detection is enabled."

#define SCENE_THRESHOLD_TEXT "HDR peak scene change threshold"
#define SCENE_THRESHOLD_LONGTEXT "When using HDR peak detection, this sets a threshold for sudden brightness changes that should be considered as scene changes. This will result in the detected peak being immediately updated to the new value, rather than gradually being adjusted. Setting this to 0 disables this feature."

#define SCENE_THRESHOLD_LOW_TEXT "Scene change lower threshold"
#define SCENE_THRESHOLD_LOW_LONGTEXT "When using HDR peak detection, this sets the lower boundary of a brightness change indicating a scene change. Brightness changes between frames that exceed this threshold will begin to be mixed into the detected peak more strongly, bypassing the peak smoothing. Setting this to a negative number disables this logic."

#define SCENE_THRESHOLD_HIGH_TEXT "Scene change upper threshold"
#define SCENE_THRESHOLD_HIGH_LONGTEXT "This sets the upper boundary of a brightness change indicating a scene change. Brightness changes that exceed this value will instantly replace the detected peak, bypassing all smoothing. Setting this to a negative number disables this logic."

#define DITHER_TEXT "Dithering algorithm"
#define DITHER_LONGTEXT "The algorithm to use when dithering to a lower bit depth."

static const int dither_values[] = {
    -1, // no dithering
    PL_DITHER_BLUE_NOISE,
    PL_DITHER_ORDERED_FIXED,
    PL_DITHER_ORDERED_LUT,
    PL_DITHER_WHITE_NOISE,
};

static const char * const dither_text[] = {
    "Disabled",
    "Blue noise (high quality)",
    "Bayer matrix (ordered dither), 16x16 fixed size (fast)",
    "Bayer matrix (ordered dither), any size",
    "White noise (fast but low quality)",
};

#define DITHER_SIZE_TEXT "Dither LUT size (log 2)"
#define DITHER_SIZE_LONGTEXT "Controls the size of the dither matrix, as a power of two (e.g. the default of 6 corresponds to a 64x64 matrix). Does not affect all algorithms."

#define TEMPORAL_DITHER_TEXT "Temporal dithering"
#define TEMPORAL_DITHER_LONGTEXT "Enables perturbing the dither matrix across frames. This reduces the persistence of dithering artifacts, but can cause flickering on some (cheap) LCD screens."

#define DITHER_DEPTH_TEXT "Dither depth override (0 = auto)"
#define DITHER_DEPTH_LONGTEXT "Overrides the detected framebuffer depth. Useful to dither to lower bit depths than otherwise required."

enum {
    SCALE_BUILTIN = 0,
    SCALE_SPLINE16,
    SCALE_SPLINE36,
    SCALE_SPLINE64,
    SCALE_MITCHELL,
    SCALE_BICUBIC,
    SCALE_EWA_LANCZOS,
    SCALE_NEAREST,
    SCALE_BILINEAR,
    SCALE_GAUSSIAN,
    SCALE_LANCZOS,
    SCALE_GINSENG,
    SCALE_EWA_GINSENG,
    SCALE_EWA_HANN,
    SCALE_HAASNSOFT,
    SCALE_CATMULL_ROM,
    SCALE_ROBIDOUX,
    SCALE_ROBIDOUXSHARP,
    SCALE_EWA_ROBIDOUX,
    SCALE_EWA_ROBIDOUXSHARP,
    SCALE_SINC,
    SCALE_EWA_JINC,
    SCALE_CUSTOM,
};

static const int scale_values[] = {
    SCALE_BUILTIN,
    SCALE_SPLINE16,
    SCALE_SPLINE36,
    SCALE_SPLINE64,
    SCALE_MITCHELL,
    SCALE_BICUBIC,
    SCALE_EWA_LANCZOS,
    SCALE_NEAREST,
    SCALE_BILINEAR,
    SCALE_GAUSSIAN,
    SCALE_LANCZOS,
    SCALE_GINSENG,
    SCALE_EWA_GINSENG,
    SCALE_EWA_HANN,
    SCALE_HAASNSOFT,
    SCALE_CATMULL_ROM,
    SCALE_ROBIDOUX,
    SCALE_ROBIDOUXSHARP,
    SCALE_EWA_ROBIDOUX,
    SCALE_EWA_ROBIDOUXSHARP,
    SCALE_SINC,
    SCALE_EWA_JINC,
    SCALE_CUSTOM,
};

static const char * const scale_text[] = {
    "Built-in / fixed function (fast)",
    "Spline 2 taps",
    "Spline 3 taps (recommended upscaler)",
    "Spline 4 taps",
    "Mitchell-Netravali (recommended downscaler)",
    "Bicubic",
    "Jinc / EWA Lanczos 3 taps (high quality, slow)",
    "Nearest neighbor",
    "Bilinear",
    "Gaussian",
    "Lanczos 3 taps",
    "Ginseng 3 taps",
    "EWA Ginseng",
    "EWA Hann",
    "HaasnSoft (blurred EWA Hann)",
    "Catmull-Rom",
    "Robidoux",
    "RobidouxSharp",
    "EWA Robidoux",
    "EWA RobidouxSharp",
    "Unwindowed sinc (clipped)",
    "Unwindowed EWA Jinc (clipped)",
    "Custom (see below)",
};

static const struct pl_filter_config *const scale_config[] = {
    [SCALE_BUILTIN]             = NULL,
    [SCALE_SPLINE16]            = &pl_filter_spline16,
    [SCALE_SPLINE36]            = &pl_filter_spline36,
    [SCALE_SPLINE64]            = &pl_filter_spline64,
    [SCALE_NEAREST]             = &pl_filter_box,
    [SCALE_BILINEAR]            = &pl_filter_triangle,
    [SCALE_GAUSSIAN]            = &pl_filter_gaussian,
    [SCALE_SINC]                = &pl_filter_sinc,
    [SCALE_LANCZOS]             = &pl_filter_lanczos,
    [SCALE_GINSENG]             = &pl_filter_ginseng,
    [SCALE_EWA_JINC]            = &pl_filter_ewa_jinc,
    [SCALE_EWA_LANCZOS]         = &pl_filter_ewa_lanczos,
    [SCALE_EWA_GINSENG]         = &pl_filter_ewa_ginseng,
    [SCALE_EWA_HANN]            = &pl_filter_ewa_hann,
    [SCALE_HAASNSOFT]           = &pl_filter_haasnsoft,
    [SCALE_BICUBIC]             = &pl_filter_bicubic,
    [SCALE_CATMULL_ROM]         = &pl_filter_catmull_rom,
    [SCALE_MITCHELL]            = &pl_filter_mitchell,
    [SCALE_ROBIDOUX]            = &pl_filter_robidoux,
    [SCALE_ROBIDOUXSHARP]       = &pl_filter_robidouxsharp,
    [SCALE_EWA_ROBIDOUX]        = &pl_filter_robidoux,
    [SCALE_EWA_ROBIDOUXSHARP]   = &pl_filter_robidouxsharp,
    [SCALE_CUSTOM]              = NULL,
};

#define UPSCALER_PRESET_TEXT "Upscaler preset"
#define DOWNSCALER_PRESET_TEXT "Downscaler preset"
#define SCALER_PRESET_LONGTEXT "Choose from one of the built-in scaler presets. If set to custom, you can choose your own combination of kernel/window functions."

#define LUT_ENTRIES_TEXT "Scaler LUT size"
#define LUT_ENTRIES_LONGTEXT "Size of the LUT texture used for up/downscalers that require one. Reducing this may boost performance at the cost of quality."

#define ANTIRING_TEXT "Anti-ringing strength"
#define ANTIRING_LONGTEXT "Enables anti-ringing for non-polar filters. A value of 1.0 completely removes ringing, a value of 0.0 is a no-op."

enum {
    FILTER_NONE = 0,
    FILTER_BOX,
    FILTER_TRIANGLE,
    FILTER_HANN,
    FILTER_HAMMING,
    FILTER_WELCH,
    FILTER_KAISER,
    FILTER_BLACKMAN,
    FILTER_GAUSSIAN,
    FILTER_SINC,
    FILTER_JINC,
    FILTER_SPHINX,
    FILTER_BCSPLINE,
    FILTER_CATMULL_ROM,
    FILTER_MITCHELL,
    FILTER_ROBIDOUX,
    FILTER_ROBIDOUXSHARP,
    FILTER_BICUBIC,
    FILTER_SPLINE16,
    FILTER_SPLINE36,
    FILTER_SPLINE64,
};

static const int filter_values[] = {
    FILTER_NONE,
    FILTER_BOX,
    FILTER_TRIANGLE,
    FILTER_HANN,
    FILTER_HAMMING,
    FILTER_WELCH,
    FILTER_KAISER,
    FILTER_BLACKMAN,
    FILTER_GAUSSIAN,
    FILTER_SINC,
    FILTER_JINC,
    FILTER_SPHINX,
    FILTER_BCSPLINE,
    FILTER_CATMULL_ROM,
    FILTER_MITCHELL,
    FILTER_ROBIDOUX,
    FILTER_ROBIDOUXSHARP,
    FILTER_BICUBIC,
    FILTER_SPLINE16,
    FILTER_SPLINE36,
    FILTER_SPLINE64,
};

static const char * const filter_text[] = {
    "None",
    "Box / Nearest",
    "Triangle / Linear",
    "Hann",
    "Hamming",
    "Welch",
    "Kaiser",
    "Blackman",
    "Gaussian",
    "Sinc",
    "Jinc",
    "Sphinx",
    "BC spline",
    "Catmull-Rom",
    "Mitchell-Netravali",
    "Robidoux",
    "RobidouxSharp",
    "Bicubic",
    "Spline16",
    "Spline36",
    "Spline64",
};

static const struct pl_filter_function *const filter_fun[] = {
    [FILTER_NONE]           = NULL,
    [FILTER_BOX]            = &pl_filter_function_box,
    [FILTER_TRIANGLE]       = &pl_filter_function_triangle,
    [FILTER_HANN]           = &pl_filter_function_hann,
    [FILTER_HAMMING]        = &pl_filter_function_hamming,
    [FILTER_WELCH]          = &pl_filter_function_welch,
    [FILTER_KAISER]         = &pl_filter_function_kaiser,
    [FILTER_BLACKMAN]       = &pl_filter_function_blackman,
    [FILTER_GAUSSIAN]       = &pl_filter_function_gaussian,
    [FILTER_SINC]           = &pl_filter_function_sinc,
    [FILTER_JINC]           = &pl_filter_function_jinc,
    [FILTER_SPHINX]         = &pl_filter_function_sphinx,
    [FILTER_BCSPLINE]       = &pl_filter_function_bcspline,
    [FILTER_CATMULL_ROM]    = &pl_filter_function_catmull_rom,
    [FILTER_MITCHELL]       = &pl_filter_function_mitchell,
    [FILTER_ROBIDOUX]       = &pl_filter_function_robidoux,
    [FILTER_ROBIDOUXSHARP]  = &pl_filter_function_robidouxsharp,
    [FILTER_BICUBIC]        = &pl_filter_function_bicubic,
    [FILTER_SPLINE16]       = &pl_filter_function_spline16,
    [FILTER_SPLINE36]       = &pl_filter_function_spline36,
    [FILTER_SPLINE64]       = &pl_filter_function_spline64,
};

#define KERNEL_TEXT "Kernel function"
#define KERNEL_LONGTEXT "Main function defining the filter kernel."

#define WINDOW_TEXT "Window function"
#define WINDOW_LONGTEXT "Window the kernel by an additional function. (Optional)"

#define CLAMP_TEXT "Clamping coefficient"
#define CLAMP_LONGTEXT "If 1.0, clamp the kernel to only allow non-negative coefficients. If 0.0, no clamping is performed. Values in between are linear."

#define BLUR_TEXT "Blur/Sharpen coefficient"
#define BLUR_LONGTEXT "If 1.0, no change is performed. Values below 1.0 sharpen/narrow the kernel, values above 1.0 blur/widen the kernel. Avoid setting too low values!"

#define TAPER_TEXT "Taper width"
#define TAPER_LONGTEXT "Taper the kernel - all inputs within the range [0, taper] will return 1.0, and the rest of the kernel is squished into (taper, radius]."

#define POLAR_TEXT "Use as EWA / Polar filter"
#define POLAR_LONGTEXT "EWA/Polar filters are much slower but higher quality. Not all functions are good candidates. It's recommended to use jinc as the kernel."

#define DEBAND_TEXT "Enable debanding"
#define DEBAND_LONGTEXT "Turns on the debanding step. This algorithm can be further tuned with the iterations and grain options."

#define DEBAND_ITER_TEXT "Debanding iterations"
#define DEBAND_ITER_LONGTEXT "The number of debanding steps to perform per sample. Each step reduces a bit more banding, but takes time to compute. Note that the strength of each step falls off very quickly, so high numbers (>4) are practically useless. A value of 0 is a no-op."

#define DEBAND_THRESH_TEXT "Gradient threshold"
#define DEBAND_THRESH_LONGTEXT "The debanding filter's cut-off threshold. Higher numbers increase the debanding strength dramatically, but progressively diminish image details."

#define DEBAND_RADIUS_TEXT "Search radius"
#define DEBAND_RADIUS_LONGTEXT "The debanding filter's initial radius. The radius increases linearly for each iteration. A higher radius will find more gradients, but a lower radius will smooth more aggressively."

#define DEBAND_GRAIN_TEXT "Grain strength"
#define DEBAND_GRAIN_LONGTEXT "Add some extra noise to the image. This significantly helps cover up remaining quantization artifacts. Higher numbers add more noise."

#define SIGMOID_TEXT "Use sigmoidization when upscaling"
#define SIGMOID_LONGTEXT "If true, sigmoidizes the signal before upscaling. This helps prevent ringing artifacts. Not always in effect, even if enabled."

#define SIGMOID_CENTER_TEXT "Sigmoid center"
#define SIGMOID_CENTER_LONGTEXT "The center (bias) of the sigmoid curve."

#define SIGMOID_SLOPE_TEXT "Sigmoid slope"
#define SIGMOID_SLOPE_LONGTEXT "The slope (steepness) of the sigmoid curve."

#define POLAR_CUTOFF_TEXT "Cut-off value for polar samplers"
#define POLAR_CUTOFF_LONGTEXT "As a micro-optimization, all samples with a weight below this value will be ignored. This reduces the need to perform unnecessary work that doesn't noticeably change the resulting image. Setting it to a value of 0.0 disables this optimization."

#define SKIP_AA_TEXT "Disable anti-aliasing when downscaling"
#define SKIP_AA_LONGTEXT "This will result in moiré artifacts and nasty, jagged pixels when downscaling, except for some very limited special cases (e.g. bilinear downsampling to exactly 0.5x). Significantly speeds up downscaling with high downscaling ratios."

#define OVERLAY_DIRECT_TEXT "Force GPU built-in sampling for overlay textures"
#define OVERLAY_DIRECT_LONGTEXT "Normally, the configured up/downscalers will be used when overlay textures (such as subtitles) need to be scaled up or down. Enabling this option overrides this behavior and forces overlay textures to go through the GPU's built-in sampling instead (typically bilinear)."

#define DISABLE_LINEAR_TEXT "Don't linearize before scaling"
#define DISABLE_LINEAR_LONGTEXT "Normally, the image is converted to linear light before scaling (under certain conditions). Enabling this option disables this behavior."

#define FORCE_GENERAL_TEXT "Force the use of general-purpose scalers"
#define FORCE_GENERAL_LONGTEXT "Normally, certain special scalers will be replaced by faster versions instead of going through the general scaler architecture. Enabling this option disables these optimizations."

#define DELAYED_PEAK_TEXT "Allow delaying peak detection by up to one frame"
#define DELAYED_PEAK_LONGTEXT "In some cases, peak detection may be more convenient to compute if the results are delayed by a frame. When this option is disabled, libplacebo will use an indirect buffer simply to force peak detection results to be up-to-date. Enabling it allows skipping this indirection in order to improve performance at the cost of some potentially noticeable brightness flickering immediately after a scene change."

#endif // VLC_PLACEBO_UTILS_H
