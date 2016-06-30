/*****************************************************************************
 * stream_fifo.c: FIFO stream unit test
 *****************************************************************************
 * Copyright © 2016 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_stream.h>
#include "../../../lib/libvlc_internal.h"
#include "../../libvlc/test.h"

#include <vlc/vlc.h>

static libvlc_instance_t *vlc;
static vlc_object_t *parent;
static stream_t *s;

int main(void)
{
    ssize_t val;
    char buf[16];
    bool b;

    test_init();

    vlc = libvlc_new(0, NULL);
    assert(vlc != NULL);
    parent = VLC_OBJECT(vlc->p_libvlc_int);

    s = vlc_stream_fifo_New(parent);
    assert(s != NULL);
    val = stream_Control(s, STREAM_CAN_SEEK, &b);
    assert(val == VLC_SUCCESS && !b);
    val = stream_GetSize(s, &(uint64_t){ 0 });
    assert(val < 0);
    val = stream_Control(s, STREAM_GET_PTS_DELAY, &(int64_t){ 0 });
    assert(val == VLC_SUCCESS);
    stream_Delete(s);
    vlc_stream_fifo_Close(s);

    s = vlc_stream_fifo_New(parent);
    assert(s != NULL);
    val = vlc_stream_fifo_Write(s, "123", 3);
    vlc_stream_fifo_Close(s);
    val = stream_Read(s, buf, sizeof (buf));
    assert(val == 3);
    assert(memcmp(buf, "123", 3) == 0);
    val = stream_Read(s, buf, sizeof (buf));
    assert(val == 0);
    stream_Delete(s);

    s = vlc_stream_fifo_New(parent);
    assert(s != NULL);
    val = vlc_stream_fifo_Write(s, "Hello ", 6);
    assert(val == 6);
    val = vlc_stream_fifo_Write(s, "world!\n", 7);
    assert(val == 7);
    val = vlc_stream_fifo_Write(s, "blahblah", 8);
    assert(val == 8);

    val = stream_Read(s, buf, 13);
    assert(val == 13);
    assert(memcmp(buf, "Hello world!\n", 13) == 0);
    stream_Delete(s);

    val = vlc_stream_fifo_Write(s, "cough cough", 11);
    assert(val == -1 && errno == EPIPE);
    vlc_stream_fifo_Close(s);

    libvlc_release(vlc);

    return 0;
}
