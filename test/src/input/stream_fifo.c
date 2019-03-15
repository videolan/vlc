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
static vlc_stream_fifo_t *writer;
static stream_t *reader;

int main(void)
{
    block_t *block;
    const unsigned char *peek;
    ssize_t val;
    char buf[16];
    bool b;

    test_init();

    vlc = libvlc_new(0, NULL);
    assert(vlc != NULL);
    parent = VLC_OBJECT(vlc->p_libvlc_int);

    writer = vlc_stream_fifo_New(parent, &reader);
    assert(writer != NULL);
    val = vlc_stream_Control(reader, STREAM_CAN_SEEK, &b);
    assert(val == VLC_SUCCESS && !b);
    val = vlc_stream_GetSize(reader, &(uint64_t){ 0 });
    assert(val < 0);
    val = vlc_stream_Control(reader, STREAM_GET_PTS_DELAY, &(vlc_tick_t){ 0 });
    assert(val == VLC_SUCCESS);
    vlc_stream_Delete(reader);
    vlc_stream_fifo_Close(writer);

    writer = vlc_stream_fifo_New(parent, &reader);
    assert(writer != NULL);
    assert(vlc_stream_Tell(reader) == 0);
    assert(!vlc_stream_Eof(reader));
    val = vlc_stream_fifo_Write(writer, "123", 3);
    vlc_stream_fifo_Close(writer);

    val = vlc_stream_Read(reader, buf, 0);
    assert(val == 0);
    assert(vlc_stream_Tell(reader) == 0);
    assert(!vlc_stream_Eof(reader));

    val = vlc_stream_Read(reader, buf, sizeof (buf));
    assert(val == 3);
    assert(vlc_stream_Tell(reader) == 3);
    assert(memcmp(buf, "123", 3) == 0);
    val = vlc_stream_Read(reader, buf, sizeof (buf));
    assert(val == 0);
    assert(vlc_stream_Tell(reader) == 3);
    assert(vlc_stream_Eof(reader));
    vlc_stream_Delete(reader);

    writer = vlc_stream_fifo_New(parent, &reader);
    assert(writer != NULL);
    val = vlc_stream_fifo_Write(writer, "Hello ", 6);
    assert(val == 6);
    val = vlc_stream_fifo_Write(writer, "world!\n", 7);
    assert(val == 7);
    val = vlc_stream_fifo_Write(writer, "blahblah", 8);
    assert(val == 8);

    val = vlc_stream_Read(reader, buf, 13);
    assert(val == 13);
    assert(memcmp(buf, "Hello world!\n", 13) == 0);
    vlc_stream_Delete(reader);

    val = vlc_stream_fifo_Write(writer, "cough cough", 11);
    assert(val == -1 && errno == EPIPE);
    vlc_stream_fifo_Close(writer);

    writer = vlc_stream_fifo_New(parent, &reader);
    assert(writer != NULL);
    val = vlc_stream_Peek(reader, &peek, 0);
    assert(val == 0);
    val = vlc_stream_fifo_Write(writer, "1st block\n", 10);
    assert(val == 10);
    val = vlc_stream_fifo_Write(writer, "2nd block\n", 10);
    assert(val == 10);
    val = vlc_stream_fifo_Write(writer, "3rd block\n", 10);
    assert(val == 10);

    val = vlc_stream_Peek(reader, &peek, 5);
    assert(val == 5);
    assert(vlc_stream_Tell(reader) == 0);
    assert(!vlc_stream_Eof(reader));
    assert(memcmp(peek, "1st b", 5) == 0);

    val = vlc_stream_Peek(reader, &peek, 0);
    assert(val == 0);
    assert(vlc_stream_Tell(reader) == 0);
    assert(!vlc_stream_Eof(reader));

    val = vlc_stream_Peek(reader, &peek, 15);
    assert(val == 15);
    assert(vlc_stream_Tell(reader) == 0);
    assert(!vlc_stream_Eof(reader));
    assert(memcmp(peek, "1st block\n2nd b", 15) == 0);

    vlc_stream_fifo_Close(writer);
    val = vlc_stream_Peek(reader, &peek, 40);
    assert(val == 30);
    assert(vlc_stream_Tell(reader) == 0);
    assert(!vlc_stream_Eof(reader));
    assert(memcmp(peek, "1st block\n2nd block\n3rd block\n", 30) == 0);

    val = vlc_stream_Peek(reader, &peek, 25);
    assert(val == 25);
    assert(vlc_stream_Tell(reader) == 0);
    assert(!vlc_stream_Eof(reader));
    assert(memcmp(peek, "1st block\n2nd block\n3rd b", 25) == 0);

    block = vlc_stream_ReadBlock(reader);
    assert(block != NULL);
    assert(vlc_stream_Tell(reader) == block->i_buffer);
    assert(block->i_buffer <= 30);
    assert(memcmp(block->p_buffer, "1st block\n2nd block\n3rd block\n",
                  block->i_buffer) == 0);
    block_Release(block);

    block = vlc_stream_ReadBlock(reader);
    assert(block == NULL);
    assert(vlc_stream_Eof(reader));
    vlc_stream_Delete(reader);

    writer = vlc_stream_fifo_New(parent, &reader);
    assert(writer != NULL);
    val = vlc_stream_fifo_Write(writer, "1st block\n", 10);
    assert(val == 10);
    val = vlc_stream_fifo_Write(writer, "2nd block\n", 10);
    assert(val == 10);
    val = vlc_stream_fifo_Write(writer, "3rd block\n", 10);
    assert(val == 10);
    val = vlc_stream_fifo_Write(writer, "4th block\n", 10);
    assert(val == 10);
    val = vlc_stream_fifo_Write(writer, "5th block\n", 10);
    assert(val == 10);
    vlc_stream_fifo_Close(writer);

    block = vlc_stream_ReadBlock(reader);
    assert(block != NULL);
    assert(vlc_stream_Tell(reader) == 10);
    assert(block->i_buffer == 10);
    assert(memcmp(block->p_buffer, "1st block\n", 10) == 0);
    block_Release(block);

    val = vlc_stream_Read(reader, buf, 5);
    assert(val == 5);
    assert(vlc_stream_Tell(reader) == 15);
    assert(memcmp(buf, "2nd b", 5) == 0);

    val = vlc_stream_Read(reader, buf, 7);
    assert(val == 7);
    assert(vlc_stream_Tell(reader) == 22);
    assert(memcmp(buf, "lock\n3r", 7) == 0);

    val = vlc_stream_Peek(reader, &peek, 2);
    assert(val == 2);
    assert(vlc_stream_Tell(reader) == 22);
    assert(!vlc_stream_Eof(reader));
    assert(memcmp(peek, "d ", 2) == 0);

    /* seeking within peek buffer has to work... */
    val = vlc_stream_Seek(reader, 23);
    assert(val == VLC_SUCCESS);
    assert(vlc_stream_Tell(reader) == 23);
    assert(!vlc_stream_Eof(reader));

    val = vlc_stream_Seek(reader, 24);
    assert(val == VLC_SUCCESS);
    assert(vlc_stream_Tell(reader) == 24);
    assert(!vlc_stream_Eof(reader));

    val = vlc_stream_Peek(reader, &peek, 2);
    assert(val == 2);
    assert(vlc_stream_Tell(reader) == 24);
    assert(!vlc_stream_Eof(reader));
    assert(memcmp(peek, "bl", 2) == 0);

    block = vlc_stream_Block(reader, 3);
    assert(block != NULL);
    assert(block->i_buffer == 3);
    assert(vlc_stream_Tell(reader) == 27);
    assert(memcmp(block->p_buffer, "blo", 3) == 0);
    block_Release(block);

    block = vlc_stream_ReadBlock(reader);
    assert(block != NULL);
    assert(block->i_buffer == 3);
    assert(vlc_stream_Tell(reader) == 30);
    assert(memcmp(block->p_buffer, "ck\n", 3) == 0);
    block_Release(block);

    val = vlc_stream_Read(reader, buf, 5);
    assert(val == 5);
    assert(vlc_stream_Tell(reader) == 35);
    assert(!vlc_stream_Eof(reader));
    assert(memcmp(buf, "4th b", 5) == 0);

    block = vlc_stream_ReadBlock(reader);
    assert(block != NULL);
    assert(block->i_buffer == 5);
    assert(vlc_stream_Tell(reader) == 40);
    assert(memcmp(block->p_buffer, "lock\n", 5) == 0);
    vlc_stream_Delete(reader);
    block_Release(block);

    libvlc_release(vlc);

    return 0;
}
