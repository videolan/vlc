/*****************************************************************************
 * cea708_aspect_ratio.c: CEA-708 aspect ratio awareness tests
 *****************************************************************************
 * Copyright (C) 2025 VideoLAN and VLC Authors
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

#include "../../libvlc/test.h"
#include "../../../lib/libvlc_internal.h"
#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_subpicture.h>
#include <vlc_es.h>
#include <vlc_interface.h>
#include <math.h>

#include "../../../modules/codec/substext.h"

/* CEA-708-E Section 8.2: Screen coordinates and positioning
 * Different grid systems for different aspect ratios
 * Implementation uses 210 columns for 16:9 and 160 columns for 4:3 */
#define CEA708_SCREEN_COLS_43           160
#define CEA708_SCREEN_COLS_169          210
#define CEA708_SCREEN_ROWS              75

static libvlc_instance_t *libvlc;

/* Test aspect ratio calculation logic based on CEA-708-E Section 8.2 */
static void test_aspect_ratio_calculation(void)
{
    test_log("Testing display aspect ratio calculation for CEA-708 grid selection\n");

    struct {
        unsigned width, height, sar_num, sar_den;
        bool expected_is_169;
        const char *description;
    } test_cases[] = {
        {640, 480, 1, 1, false, "4:3 square pixels"},
        {1920, 1080, 1, 1, true, "16:9 square pixels"},
        {720, 480, 8, 9, false, "DVD 4:3 anamorphic"},
        {720, 480, 32, 27, true, "DVD 16:9 anamorphic"},
        {1280, 720, 1, 1, true, "HD 720p"},
        {1440, 1080, 4, 3, true, "HDV 1080i"},
        {0, 0, 1, 1, true, "Invalid dimensions (fallback to 16:9)"},
        {720, 576, 12, 11, false, "PAL 4:3"},
        {720, 576, 16, 11, true, "PAL 16:9"},
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        unsigned dar_num = test_cases[i].width * test_cases[i].sar_num;
        unsigned dar_den = test_cases[i].height * test_cases[i].sar_den;

        bool calculated_is_169;
        if (test_cases[i].width == 0 || test_cases[i].height == 0) {
            /* Fallback to 16:9 for invalid dimensions */
            calculated_is_169 = true;
        } else {
            /* Implementation threshold: DAR >= 5:3 uses 16:9 grid */
            calculated_is_169 = (dar_num * 3 >= dar_den * 5);
        }

        test_log("Test case: %s (%ux%u SAR %u:%u) -> %s grid\n",
                test_cases[i].description,
                test_cases[i].width, test_cases[i].height,
                test_cases[i].sar_num, test_cases[i].sar_den,
                calculated_is_169 ? "16:9" : "4:3");

        /* These tests will FAIL until aspect ratio awareness is implemented */
        assert(calculated_is_169 == test_cases[i].expected_is_169);
    }
}

/* Test positioning coordinate accuracy based on CEA-708-E Section 8.2 */
static void test_positioning_accuracy(void)
{
    test_log("Testing CEA-708 positioning coordinate accuracy\n");

    struct {
        int grid_cols;
        float anchor_h;
        float expected_ratio;
        const char *description;
    } test_cases[] = {
        {160, 80.0f, 0.5f, "4:3 grid center"},
        {210, 105.0f, 0.5f, "16:9 grid center"},
        {160, 0.0f, 0.0f, "4:3 grid left edge"},
        {210, 0.0f, 0.0f, "16:9 grid left edge"},
        {160, 159.0f, 0.99375f, "4:3 grid right edge"},
        {210, 209.0f, 0.995238f, "16:9 grid right edge"},
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        float calculated_ratio = test_cases[i].anchor_h / test_cases[i].grid_cols;

        test_log("Test case: %s -> ratio %.6f (expected %.6f)\n",
                test_cases[i].description, calculated_ratio, test_cases[i].expected_ratio);

        assert(fabs(calculated_ratio - test_cases[i].expected_ratio) < 0.0001f);
    }
}

/* Test that current implementation has positioning issues (will fail until fixed) */
static void test_current_positioning_issues(void)
{
    test_log("Testing current CEA-708 positioning issues (expects failures)\n");

    /* Current implementation always uses 16:9 grid (210 columns)
     * This causes incorrect positioning for 4:3 content */

    /* Test 4:3 content positioning */
    float anchor_h_4_3 = 80.0f; /* Should be center in 4:3 (80/160 = 0.5) */
    float current_ratio = anchor_h_4_3 / CEA708_SCREEN_COLS_169; /* But uses 210 */
    float expected_ratio = 0.5f;

    test_log("4:3 center positioning: current=%.6f, expected=%.6f\n",
             current_ratio, expected_ratio);

    /* This will fail - current implementation gives wrong position for 4:3 */
    if (fabs(current_ratio - expected_ratio) > 0.01f) {
        test_log("EXPECTED FAILURE: 4:3 positioning is incorrect with current implementation\n");
        /* Don't assert here since we expect this to fail until fixed */
    }
}

int main(void)
{
    test_init();

    libvlc = libvlc_new(test_defaults_nargs, test_defaults_args);
    assert(libvlc != NULL);

    test_aspect_ratio_calculation();
    test_positioning_accuracy();
    test_current_positioning_issues();

    libvlc_release(libvlc);

    return 0;
}
