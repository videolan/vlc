/*****************************************************************************
 * cea708.c: CEA-708 decoder unit tests
 *****************************************************************************
 * Copyright (C) 2026 VideoLabs, VideoLAN and VLC Authors
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
#include <vlc_codec.h>

#include "../../../modules/codec/cea708.h"

#include "../../libvlc/test.h"

const char vlc_module_name[] = "cea708_test";

/*
 * Regression test for BT-direction scroll i_lastrow underflow.
 *
 * A window with row_count=1 and scroll_direction=BT triggers a uint8_t
 * underflow of i_lastrow (0 -> 255) inside CEA708_Window_Scroll().
 * Subsequent operations like CLW iterate rows[0..255] on a 15-element
 * heap array, causing out-of-bounds reads and writes.
 */
static void test_bt_scroll_underflow(void)
{
    cea708_t *dec = CEA708_Decoder_New(NULL);
    assert(dec);

    uint8_t payload[128];
    int pos = 0;

    /* DF0 (Define Window 0): row_count=1, visible=0, default style #1 (BT) */
    payload[pos++] = 0x98;  /* C1: DF0 */
    payload[pos++] = 0x18;  /* row_lock | col_lock, visible=0 */
    payload[pos++] = 0x00;  /* anchor_v=0 */
    payload[pos++] = 0x00;  /* anchor_h=0 */
    payload[pos++] = 0x00;  /* anchor_point=0, row_count-1=0 (row_count=1) */
    payload[pos++] = 0x2A;  /* col_count=42 */
    payload[pos++] = 0x00;  /* window_style=0, pen_style=0 -> defaults to style #1 */

    /* 42 printable chars fill the single row, triggering
     * Forward -> CarriageReturn -> Scroll BT with row_count=1 */
    for (int i = 0; i < 42; i++)
        payload[pos++] = 0x41;  /* 'A' */

    /* CLW (Clear Windows): iterates rows[firstrow..lastrow].
     * With the underflow, lastrow=255 -> OOB on 15-element array */
    payload[pos++] = 0x88;  /* C1: CLW */
    payload[pos++] = 0x01;  /* window bitmask: window 0 */

    CEA708_Decoder_Push(dec, 0, payload, pos);

    CEA708_Decoder_Release(dec);
}

int main(void)
{
    test_init();

    test_bt_scroll_underflow();

    return 0;
}
