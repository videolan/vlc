/*****************************************************************************
 * vector.c : Test for vlc_vector macros
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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

#undef NDEBUG

#include <assert.h>

#include <vlc_common.h>
#include <vlc_vector.h>

static void test_vector_insert_remove(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    bool ok;

    ok = vlc_vector_push(&vec, 42);
    assert(ok);
    assert(vec.data[0] == 42);
    assert(vec.size == 1);

    ok = vlc_vector_push(&vec, 37);
    assert(ok);
    assert(vec.size == 2);
    assert(vec.data[0] == 42);
    assert(vec.data[1] == 37);

    ok = vlc_vector_insert(&vec, 1, 100);
    assert(ok);
    assert(vec.size == 3);
    assert(vec.data[0] == 42);
    assert(vec.data[1] == 100);
    assert(vec.data[2] == 37);

    ok = vlc_vector_push(&vec, 77);
    assert(ok);
    assert(vec.size == 4);
    assert(vec.data[0] == 42);
    assert(vec.data[1] == 100);
    assert(vec.data[2] == 37);
    assert(vec.data[3] == 77);

    vlc_vector_remove(&vec, 1);
    assert(vec.size == 3);
    assert(vec.data[0] == 42);
    assert(vec.data[1] == 37);
    assert(vec.data[2] == 77);

    vlc_vector_clear(&vec);
    assert(vec.size == 0);

    vlc_vector_destroy(&vec);
}

static void test_vector_push_array(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;
    bool ok;

    ok = vlc_vector_push(&vec, 3); assert(ok);
    ok = vlc_vector_push(&vec, 14); assert(ok);
    ok = vlc_vector_push(&vec, 15); assert(ok);
    ok = vlc_vector_push(&vec, 92); assert(ok);
    ok = vlc_vector_push(&vec, 65); assert(ok);
    assert(vec.size == 5);

    int items[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    ok = vlc_vector_push_all(&vec, items, 8);

    assert(ok);
    assert(vec.size == 13);
    assert(vec.data[0] == 3);
    assert(vec.data[1] == 14);
    assert(vec.data[2] == 15);
    assert(vec.data[3] == 92);
    assert(vec.data[4] == 65);
    assert(vec.data[5] == 1);
    assert(vec.data[6] == 2);
    assert(vec.data[7] == 3);
    assert(vec.data[8] == 4);
    assert(vec.data[9] == 5);
    assert(vec.data[10] == 6);
    assert(vec.data[11] == 7);
    assert(vec.data[12] == 8);

    vlc_vector_destroy(&vec);
}

static void test_vector_insert_array(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;
    bool ok;

    ok = vlc_vector_push(&vec, 3); assert(ok);
    ok = vlc_vector_push(&vec, 14); assert(ok);
    ok = vlc_vector_push(&vec, 15); assert(ok);
    ok = vlc_vector_push(&vec, 92); assert(ok);
    ok = vlc_vector_push(&vec, 65); assert(ok);
    assert(vec.size == 5);

    int items[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    ok = vlc_vector_insert_all(&vec, 3, items, 8);
    assert(ok);
    assert(vec.size == 13);
    assert(vec.data[0] == 3);
    assert(vec.data[1] == 14);
    assert(vec.data[2] == 15);
    assert(vec.data[3] == 1);
    assert(vec.data[4] == 2);
    assert(vec.data[5] == 3);
    assert(vec.data[6] == 4);
    assert(vec.data[7] == 5);
    assert(vec.data[8] == 6);
    assert(vec.data[9] == 7);
    assert(vec.data[10] == 8);
    assert(vec.data[11] == 92);
    assert(vec.data[12] == 65);

    vlc_vector_destroy(&vec);
}

static void test_vector_remove_slice(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    bool ok;

    for (int i = 0; i < 100; ++i)
    {
        ok = vlc_vector_push(&vec, i);
        assert(ok);
    }

    assert(vec.size == 100);

    vlc_vector_remove_slice(&vec, 32, 60);
    assert(vec.size == 40);
    assert(vec.data[31] == 31);
    assert(vec.data[32] == 92);

    vlc_vector_destroy(&vec);
}

static void test_vector_swap_remove(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    bool ok;

    ok = vlc_vector_push(&vec, 3); assert(ok);
    ok = vlc_vector_push(&vec, 14); assert(ok);
    ok = vlc_vector_push(&vec, 15); assert(ok);
    ok = vlc_vector_push(&vec, 92); assert(ok);
    ok = vlc_vector_push(&vec, 65); assert(ok);
    assert(vec.size == 5);

    vlc_vector_swap_remove(&vec, 1);
    assert(vec.size == 4);
    assert(vec.data[0] == 3);
    assert(vec.data[1] == 65);
    assert(vec.data[2] == 15);
    assert(vec.data[3] == 92);

    vlc_vector_destroy(&vec);
}

static void test_vector_index_of(void)
{
    struct VLC_VECTOR(int) vec;
    vlc_vector_init(&vec);

    bool ok;

    for (int i = 0; i < 10; ++i)
    {
        ok = vlc_vector_push(&vec, i);
        assert(ok);
    }

    ssize_t idx;

    vlc_vector_index_of(&vec, 0, &idx);
    assert(idx == 0);

    vlc_vector_index_of(&vec, 1, &idx);
    assert(idx == 1);

    vlc_vector_index_of(&vec, 4, &idx);
    assert(idx == 4);

    vlc_vector_index_of(&vec, 9, &idx);
    assert(idx == 9);

    vlc_vector_index_of(&vec, 12, &idx);
    assert(idx == -1);

    vlc_vector_destroy(&vec);
}

static void test_vector_foreach(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    bool ok;

    for (int i = 0; i < 10; ++i)
    {
        ok = vlc_vector_push(&vec, i);
        assert(ok);
    }

    int count = 0;
    int item;
    vlc_vector_foreach(item, &vec)
    {
        assert(item == count);
        count++;
    }
    assert(count == 10);

    vlc_vector_destroy(&vec);
}

static void test_vector_grow()
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    bool ok;

    for (int i = 0; i < 50; ++i)
    {
        ok = vlc_vector_push(&vec, i); /* append */
        assert(ok);
    }

    assert(vec.cap >= 50);
    assert(vec.size == 50);

    for (int i = 0; i < 25; ++i)
    {
        ok = vlc_vector_insert(&vec, 20, i); /* insert in the middle */
        assert(ok);
    }

    assert(vec.cap >= 75);
    assert(vec.size == 75);

    for (int i = 0; i < 25; ++i)
    {
        ok = vlc_vector_insert(&vec, 0, i); /* prepend */
        assert(ok);
    }

    assert(vec.cap >= 100);
    assert(vec.size == 100);

    for (int i = 0; i < 50; ++i)
        vlc_vector_remove(&vec, 20); /* remove from the middle */

    assert(vec.cap >= 50);
    assert(vec.size == 50);

    for (int i = 0; i < 25; ++i)
        vlc_vector_remove(&vec, 0); /* remove from the head */

    assert(vec.cap >= 25);
    assert(vec.size == 25);

    for (int i = 24; i >=0; --i)
        vlc_vector_remove(&vec, i); /* remove from the tail */

    assert(vec.size == 0);

    vlc_vector_destroy(&vec);
}

static void test_vector_exp_growth(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    size_t oldcap = vec.cap;
    int realloc_count = 0;
    bool ok;
    for (int i = 0; i < 10000; ++i)
    {
        ok = vlc_vector_push(&vec, i);
        assert(ok);
        if (vec.cap != oldcap)
        {
            realloc_count++;
            oldcap = vec.cap;
        }
    }

    /* Test speciically for an expected growth factor of 1.5. In practice, the
     * result is even lower (19) due to the first alloc of size 10 */
    assert(realloc_count <= 23); /* ln(10000) / ln(1.5) ~= 23 */

    realloc_count = 0;
    for (int i = 9999; i >= 0; --i)
    {
        vlc_vector_remove(&vec, i);
        if (vec.cap != oldcap)
        {
            realloc_count++;
            oldcap = vec.cap;
        }
    }

    assert(realloc_count <= 23); /* same expectations for removals */
    assert(realloc_count > 0); /* vlc_vector_remove() must autoshrink */

    vlc_vector_destroy(&vec);
}

static void test_vector_reserve(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    bool ok;

    ok = vlc_vector_reserve(&vec, 800);
    assert(ok);
    assert(vec.cap >= 800);
    assert(vec.size == 0);

    size_t initial_cap = vec.cap;

    for (int i = 0; i < 800; ++i)
    {
        ok = vlc_vector_push(&vec, i);
        assert(ok);
        assert(vec.cap == initial_cap); /* no realloc */
    }

    vlc_vector_destroy(&vec);
}

static void test_vector_shrink_to_fit(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    bool ok;

    ok = vlc_vector_reserve(&vec, 800);
    assert(ok);
    for (int i = 0; i < 250; ++i)
    {
        ok = vlc_vector_push(&vec, i);
        assert(ok);
    }

    assert(vec.cap >= 800);
    assert(vec.size == 250);

    vlc_vector_shrink_to_fit(&vec);
    assert(vec.cap == 250);
    assert(vec.size == 250);

    vlc_vector_destroy(&vec);
}

static void test_vector_move(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    for (int i = 0; i < 7; ++i)
    {
        bool ok = vlc_vector_push(&vec, i);
        assert(ok);
    }

    /* move item at 1 so that its new position is 4 */
    vlc_vector_move(&vec, 1, 4);

    assert(vec.size == 7);
    assert(vec.data[0] == 0);
    assert(vec.data[1] == 2);
    assert(vec.data[2] == 3);
    assert(vec.data[3] == 4);
    assert(vec.data[4] == 1);
    assert(vec.data[5] == 5);
    assert(vec.data[6] == 6);

    vlc_vector_destroy(&vec);
}

static void test_vector_move_slice_forward(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    for (int i = 0; i < 10; ++i)
    {
        bool ok = vlc_vector_push(&vec, i);
        assert(ok);
    }

    /* move slice {2, 3, 4, 5} so that its new position is 5 */
    vlc_vector_move_slice(&vec, 2, 4, 5);

    assert(vec.size == 10);
    assert(vec.data[0] == 0);
    assert(vec.data[1] == 1);
    assert(vec.data[2] == 6);
    assert(vec.data[3] == 7);
    assert(vec.data[4] == 8);
    assert(vec.data[5] == 2);
    assert(vec.data[6] == 3);
    assert(vec.data[7] == 4);
    assert(vec.data[8] == 5);
    assert(vec.data[9] == 9);

    vlc_vector_destroy(&vec);
}

static void test_vector_move_slice_backward(void)
{
    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;

    for (int i = 0; i < 10; ++i)
    {
        bool ok = vlc_vector_push(&vec, i);
        assert(ok);
    }

    /* move slice {5, 6, 7} so that its new position is 2 */
    vlc_vector_move_slice(&vec, 5, 3, 2);

    assert(vec.size == 10);
    assert(vec.data[0] == 0);
    assert(vec.data[1] == 1);
    assert(vec.data[2] == 5);
    assert(vec.data[3] == 6);
    assert(vec.data[4] == 7);
    assert(vec.data[5] == 2);
    assert(vec.data[6] == 3);
    assert(vec.data[7] == 4);
    assert(vec.data[8] == 8);
    assert(vec.data[9] == 9);

    vlc_vector_destroy(&vec);
}

int main(void) {
    test_vector_insert_remove();
    test_vector_push_array();
    test_vector_insert_array();
    test_vector_remove_slice();
    test_vector_swap_remove();
    test_vector_move();
    test_vector_move_slice_forward();
    test_vector_move_slice_backward();
    test_vector_index_of();
    test_vector_foreach();
    test_vector_grow();
    test_vector_exp_growth();
    test_vector_reserve();
    test_vector_shrink_to_fit();
    return 0;
}
