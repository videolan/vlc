/*****************************************************************************
 * randomizer.c
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

#ifdef TEST_RANDOMIZER
# undef NDEBUG
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_rand.h>
#include "randomizer.h"

/**
 * \addtogroup playlist_randomizer Playlist randomizer helper
 * \ingroup playlist
 *
 * Playlist helper to manage random playback.
 *
 * The purpose is to guarantee the following rules:
 *  - an item must never be selected twice
 *  - "prev" navigates back in the history of previously selected items
 *  - insertions and removals can occur at any time; all these rules must still
 *    apply
 *  - the user can request to select a specific item (typically by
 *    double-clicking on an item in the playlist)
 *
 * If loop (repeat) is enabled:
 *  - the random order must be "reshuffled" on every cycle
 *  - an item must never be selected twice in each cycle
 *  - the same item must never be selected twice in a row; in particular, it
 *    must not be the first item of a new cycle if it was the last item of the
 *    previous one (except if the playlist contains only one item)
 *  - crossing a new "cycle" is invisible to the user (e.g. it is still
 *    possible to navigate to previous selected items)
 *
 * To achieve these goals, a "randomizer" stores a single vector containing all
 * the items of the playlist, along with 3 indexes.
 *
 * The whole vector is not shuffled at once: instead, steps of the Fisher-Yates
 * algorithm are executed one-by-one on demand. This has several advantages:
 *  - on insertions and removals, there is no need to reshuffle or shift the
 *    whole array;
 *  - if loop is enabled, the history of the last cycle can be kept in place.
 *
 * 'head' indicates the end of the items already determinated for the current
 * cycle (if loop is disabled, there is only one cycle).
 * (0 <= head <= size)
 *
 * 'next' points to the item after the current one (we use 'next' instead of
 * 'current' so that all indexes are unsigned, while 'current' could be -1).
 * The current item is the one returned by the previous call to _Prev() or
 * _Next(). Each call to _Next() makes 'next' (and possibly 'head') move
 * forward, each call to _Prev() makes it move back (modulo size).
 * 'next' is always in the determinated range (0 <= next <= head) or in the
 * "history" range (history < next < size).
 *
 * 'history' is only used in loop mode, and references the first item of the
 * ordered history from the last cycle.
 *
 * 0              next  head          history       size
 * |---------------|-----|.............|-------------|
 *  <------------------->               <----------->
 *   determinated range                 history range
 *
 * Here is a sample scenario to understand how it works.
 *
 * The playlist initially adds 5 items (A, B, C, D and E).
 *
 *                                          history
 *                 next                     |
 *                 head                     |
 *                 |                        |
 *                 A    B    C    D    E
 *
 * The playlist calls _Next() to retrieve the next random item. The randomizer
 * picks one item (say, D), and swaps it with the current head (A). _Next()
 * returns D.
 *
 *                                          history
 *                      next                |
 *                      head                |
 *                      |                   |
 *                 D    B    C    A    E
 *               <--->
 *            determinated range
 *
 * The playlist calls _Next() one more time. The randomizer selects one item
 * outside the determinated range (say, E). _Next() returns E.
 *
 *                                          history
 *                           next           |
 *                           head           |
 *                           |              |
 *                 D    E    C    A    B
 *               <-------->
 *            determinated range
 *
 * The playlist calls _Next() one more time. The randomizer selects C (already
 * in place). _Next() returns C.
 *
 *                                          history
 *                                next      |
 *                                head      |
 *                                |         |
 *                 D    E    C    A    B
 *               <------------->
 *             determinated range
 *
 * The playlist then calls _Prev(). Since the "current" item is C, the previous
 * one is E, so _Prev() returns E, and 'next' moves back.
 *
 *                                          history
 *                           next           |
 *                           |    head      |
 *                           |    |         |
 *                 D    E    C    A    B
 *               <------------->
 *             determinated range
 *
 * The playlist calls _Next(), which returns C, as expected.
 *
 *                                          history
 *                                next      |
 *                                head      |
 *                                |         |
 *                 D    E    C    A    B
 *               <------------->
 *             determinated range
 *
 * The playlist calls _Next(), the randomizer selects B, and returns it.
 *
 *                                          history
 *                                     next |
 *                                     head |
 *                                     |    |
 *                 D    E    C    B    A
 *               <------------------>
 *                determinated range
 *
 * The playlist calls _Next(), the randomizer selects the last item (it has no
 * choice). 'next' and 'head' now point one item past the end (their value is
 * the vector size).
 *
 *                                          history
 *                                          next
 *                                          head
 *                                          |
 *                 D    E    C    B    A
 *               <----------------------->
 *                  determinated range
 *
 * At this point, if loop is disabled, it is not possible to call _Next()
 * anymore (_HasNext() returns false). So let's enable it by calling
 * _SetLoop(), then let's call _Next() again.
 *
 * This will start a new loop cycle. Firstly, 'next' and 'head' are reset, and
 * the whole vector belongs to the last cycle history.
 *
 *                  history
 *                  next
 *                  head
 *                  |
 *                  D    E    C    B    A
 *               <------------------------>
 *                     history range
 *
 * Secondly, to avoid to select A twice in a row (as the last item of the
 * previous cycle and the first item of the new one), the randomizer will
 * immediately determine another item in the vector (say C) to be the first of
 * the new cycle. The items that belong to the history are kept in order.
 * 'head' and 'history' move forward.
 *
 *                      history
 *                 next |
 *                 |    head
 *                 |    |
 *                 C    D    E    B    A
 *               <---><------------------>
 *       determinated     history range
 *              range
 *
 * Finally, it will actually select and return the first item (C).
 *
 *                      history
 *                      next
 *                      head
 *                      |
 *                 C    D    E    B    A
 *               <---><------------------>
 *       determinated     history range
 *              range
 *
 * Then, the user adds an item to the playlist (F). This item is added in front
 * of history.
 *
 *                           history
 *                      next |
 *                      head |
 *                      |    |
 *                 C    F    D    E    B    A
 *               <--->     <------------------>
 *       determinated          history range
 *              range
 *
 * The playlist calls _Next(), the randomizer randomly selects E. E
 * "disappears" from the history of the last cycle. This is a general property:
 * each item may not appear more than one in the "history" (both from the last
 * and the new cycle). The history order is preserved.
 *
 *                                history
 *                           next |
 *                           head |
 *                           |    |
 *                 C    E    F    D    B    A
 *               <-------->     <-------------->
 *              determinated     history range
 *                 range
 *
 * The playlist then calls _Prev() 3 times, that yield C, then A, then B.
 * 'next' is decremented (modulo size) on each call.
 *
 *                                history
 *                                |    next
 *                           head |    |
 *                           |    |    |
 *                 C    E    F    D    B    A
 *               <-------->     <-------------->
 *              determinated     history range
 *                 range
 */

/* On auto-reshuffle, avoid to select the same item before at least
 * NOT_SAME_BEFORE other items have been selected (between the end of the
 * previous shuffle and the start of the new shuffle). */
#define NOT_SAME_BEFORE 1

void
randomizer_Init(struct randomizer *r)
{
    vlc_vector_init(&r->items);

    /* initialize separately instead of using vlc_lrand48() to avoid locking
     * the mutex for every random number generation */
    vlc_rand_bytes(r->xsubi, sizeof(r->xsubi));

    r->loop = false;
    r->head = 0;
    r->next = 0;
    r->history = 0;
}

void
randomizer_Destroy(struct randomizer *r)
{
    vlc_vector_destroy(&r->items);
}

void
randomizer_SetLoop(struct randomizer *r, bool loop)
{
    r->loop = loop;
}

static inline ssize_t
randomizer_IndexOf(struct randomizer *r, const vlc_playlist_item_t *item)
{
    ssize_t index;
    vlc_vector_index_of(&r->items, item, &index);
    return index;
}

bool
randomizer_Count(struct randomizer *r)
{
    return r->items.size;
}

void
randomizer_Reshuffle(struct randomizer *r)
{
    /* yeah, it's that simple */
    r->head = 0;
    r->next = 0;
    r->history = r->items.size;
}

static inline void
swap_items(struct randomizer *r, int i, int j)
{
    vlc_playlist_item_t *item = r->items.data[i];
    r->items.data[i] = r->items.data[j];
    r->items.data[j] = item;
}

static inline void
randomizer_DetermineOne_(struct randomizer *r, size_t avoid_last_n)
{
    assert(r->head < r->items.size);
    assert(r->items.size - r->head > avoid_last_n);
    size_t range_len = r->items.size - r->head - avoid_last_n;
    size_t selected = r->head + (nrand48(r->xsubi) % range_len);
    swap_items(r, r->head, selected);

    if (r->head == r->history)
        r->history++;
    r->head++;
}

static inline void
randomizer_DetermineOne(struct randomizer *r)
{
    randomizer_DetermineOne_(r, 0);
}

/* An autoreshuffle occurs if loop is enabled, once all item have been played.
 * In that case, we reshuffle and determine first items so that the same item
 * may not be selected before NOT_SAME_BEFORE selections. */
static void
randomizer_AutoReshuffle(struct randomizer *r)
{
    assert(r->items.size > 0);
    r->head = 0;
    r->next = 0;
    r->history = 0; /* the whole content is history */
    size_t avoid_last_n = NOT_SAME_BEFORE;
    if (avoid_last_n > r->items.size - 1)
        /* cannot ignore all */
        avoid_last_n = r->items.size - 1;
    while (avoid_last_n)
        randomizer_DetermineOne_(r, avoid_last_n--);
}

bool
randomizer_HasPrev(struct randomizer *r)
{
    if (!r->loop)
        /* a previous exists if the current is > 0, i.e. next > 1 */
        return r->next > 1;

    if (!r->items.size)
        /* avoid modulo 0 */
        return false;

    /* there is no previous only if (current - history) == 0 (modulo size),
     * i.e. (next - history) == 1 (modulo size) */
    return (r->next + r->items.size - r->history) % r->items.size != 1;
}

bool
randomizer_HasNext(struct randomizer *r)
{
    return r->loop || r->next < r->items.size;
}

vlc_playlist_item_t *
randomizer_PeekPrev(struct randomizer *r)
{
    assert(randomizer_HasPrev(r));
    size_t index = (r->next + r->items.size - 2) % r->items.size;
    return r->items.data[index];
}

vlc_playlist_item_t *
randomizer_PeekNext(struct randomizer *r)
{
    assert(randomizer_HasNext(r));

    if (r->next == r->items.size && r->next == r->history)
    {
        assert(r->loop);
        randomizer_AutoReshuffle(r);
    }

    if (r->next == r->head)
        /* execute 1 step of the Fisher-Yates shuffle */
        randomizer_DetermineOne(r);

    return r->items.data[r->next];
}

vlc_playlist_item_t *
randomizer_Prev(struct randomizer *r)
{
    assert(randomizer_HasPrev(r));
    vlc_playlist_item_t *item = randomizer_PeekPrev(r);
    r->next = r->next ? r->next - 1 : r->items.size - 1;
    return item;
}

vlc_playlist_item_t *
randomizer_Next(struct randomizer *r)
{
    assert(randomizer_HasNext(r));
    vlc_playlist_item_t *item = randomizer_PeekNext(r);
    r->next++;
    if (r->next == r->items.size && r->next != r->head)
        r->next = 0;
    return item;
}

bool
randomizer_Add(struct randomizer *r, vlc_playlist_item_t *items[], size_t count)
{
    if (!vlc_vector_insert_all(&r->items, r->history, items, count))
        return false;
    /* the insertion shifted history (and possibly next) */
    if (r->next > r->history)
        r->next += count;
    r->history += count;
    return true;
}

static void
randomizer_SelectIndex(struct randomizer *r, size_t index)
{
    vlc_playlist_item_t *selected = r->items.data[index];
    if (r->history && index >= r->history)
    {
        if (index > r->history)
        {
            memmove(&r->items.data[r->history + 1],
                    &r->items.data[r->history],
                    (index - r->history) * sizeof(selected));
            index = r->history;
        }
        r->history = (r->history + 1) % r->items.size;
    }

    if (index >= r->head)
    {
        r->items.data[index] = r->items.data[r->head];
        r->items.data[r->head] = selected;
        r->head++;
    }
    else if (index < r->items.size - 1)
    {
        memmove(&r->items.data[index],
                &r->items.data[index + 1],
                (r->head - index - 1) * sizeof(selected));
        r->items.data[r->head - 1] = selected;
    }

    r->next = r->head;
}

void
randomizer_Select(struct randomizer *r, const vlc_playlist_item_t *item)
{
    ssize_t index = randomizer_IndexOf(r, item);
    assert(index != -1); /* item must exist */
    randomizer_SelectIndex(r, (size_t) index);
}

static void
randomizer_RemoveAt(struct randomizer *r, size_t index)
{
    /*
     * 0          head                                history   next  size
     * |-----------|...................................|---------|-----|
     * |<--------->|                                   |<------------->|
     *    ordered            order irrelevant               ordered
     */

    /* update next before index may be updated */
    if (index < r->next)
        r->next--;

    if (index < r->head)
    {
        /* item was selected, keep the selected part ordered */
        memmove(&r->items.data[index],
                &r->items.data[index + 1],
                (r->head - index - 1) * sizeof(*r->items.data));
        r->head--;
        index = r->head; /* the new index to remove */
    }

    if (index < r->history)
    {
        /* this part is unordered, no need to shift all items */
        r->items.data[index] = r->items.data[r->history - 1];
        index = r->history - 1;
        r->history--;
    }

    if (index < r->items.size - 1)
    {
        /* shift the ordered history part by one */
        memmove(&r->items.data[index],
                &r->items.data[index + 1],
                (r->items.size - index - 1) * sizeof(*r->items.data));
    }

    r->items.size--;
}

static void
randomizer_RemoveOne(struct randomizer *r, const vlc_playlist_item_t *item)
{
    ssize_t index = randomizer_IndexOf(r, item);
    assert(index >= 0); /* item must exist */
    randomizer_RemoveAt(r, index);
}

void
randomizer_Remove(struct randomizer *r, vlc_playlist_item_t *const items[],
                  size_t count)
{
    for (size_t i = 0; i < count; ++i)
        randomizer_RemoveOne(r, items[i]);

    vlc_vector_autoshrink(&r->items);
}

void
randomizer_Clear(struct randomizer *r)
{
    vlc_vector_clear(&r->items);
    r->head = 0;
    r->next = 0;
    r->history = 0;
}

#ifndef DOC
#ifdef TEST_RANDOMIZER

/* fake structure to simplify tests */
struct vlc_playlist_item {
    size_t index;
};

static void
ArrayInit(vlc_playlist_item_t *array[], size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        array[i] = malloc(sizeof(*array[i]));
        assert(array[i]);
        array[i]->index = i;
    }
}

static void
ArrayDestroy(vlc_playlist_item_t *array[], size_t len)
{
    for (size_t i = 0; i < len; ++i)
        free(array[i]);
}

static void
test_all_items_selected_exactly_once(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);

    #define SIZE 100
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, SIZE);
    assert(ok);

    bool selected[SIZE] = {0};
    for (int i = 0; i < SIZE; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        vlc_playlist_item_t *item = randomizer_Next(&randomizer);
        assert(item);
        assert(!selected[item->index]); /* never selected twice */
        selected[item->index] = true;
    }

    assert(!randomizer_HasNext(&randomizer)); /* no more items */

    for (int i = 0; i < SIZE; ++i)
        assert(selected[i]); /* all selected */

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

static void
test_all_items_selected_exactly_once_per_cycle(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);
    randomizer_SetLoop(&randomizer, true);

    #define SIZE 100
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, SIZE);
    assert(ok);

    for (int cycle = 0; cycle < 4; ++cycle)
    {
        bool selected[SIZE] = {0};
        for (int i = 0; i < SIZE; ++i)
        {
            assert(randomizer_HasNext(&randomizer));
            vlc_playlist_item_t *item = randomizer_Next(&randomizer);
            assert(item);
            assert(!selected[item->index]); /* never selected twice */
            selected[item->index] = true;
        }

        assert(randomizer_HasNext(&randomizer)); /* still has items in loop */

        for (int i = 0; i < SIZE; ++i)
            assert(selected[i]); /* all selected */
    }

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

static void
test_all_items_selected_exactly_once_with_additions(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);

    #define SIZE 100
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, 75);
    assert(ok);

    bool selected[SIZE] = {0};
    for (int i = 0; i < 50; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        vlc_playlist_item_t *item = randomizer_Next(&randomizer);
        assert(item);
        assert(!selected[item->index]); /* never selected twice */
        selected[item->index] = true;
    }

    ok = randomizer_Add(&randomizer, &items[75], 25);
    assert(ok);
    for (int i = 50; i < SIZE; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        vlc_playlist_item_t *item = randomizer_Next(&randomizer);
        assert(item);
        assert(!selected[item->index]); /* never selected twice */
        selected[item->index] = true;
    }

    assert(!randomizer_HasNext(&randomizer)); /* no more items */

    for (int i = 0; i < SIZE; ++i)
        assert(selected[i]); /* all selected */

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

static void
test_all_items_selected_exactly_once_with_removals(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);

    #define SIZE 100
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, SIZE);
    assert(ok);

    bool selected[SIZE] = {0};
    for (int i = 0; i < 50; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        vlc_playlist_item_t *item = randomizer_Next(&randomizer);
        assert(item);
        assert(!selected[item->index]); /* never selected twice */
        selected[item->index] = true;
    }

    vlc_playlist_item_t *to_remove[20];
    /* copy 10 items already selected */
    memcpy(to_remove, &randomizer.items.data[20], 10 * sizeof(*to_remove));
    /* copy 10 items not already selected */
    memcpy(&to_remove[10], &randomizer.items.data[70], 10 * sizeof(*to_remove));

    randomizer_Remove(&randomizer, to_remove, 20);

    for (int i = 50; i < SIZE - 10; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        vlc_playlist_item_t *item = randomizer_Next(&randomizer);
        assert(item);
        assert(!selected[item->index]); /* never selected twice */
        selected[item->index] = true;
    }

    assert(!randomizer_HasNext(&randomizer)); /* no more items */

    int count = 0;
    for (int i = 0; i < SIZE; ++i)
        if (selected[i])
            count++;

    assert(count == SIZE - 10);

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

static void
test_cycle_after_manual_selection(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);
    randomizer_SetLoop(&randomizer, true);

    #define SIZE 100
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, 100);
    assert(ok);

    /* force selection of the first item */
    randomizer_Select(&randomizer, randomizer.items.data[0]);

    for (int i = 0; i < 2 * SIZE; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        vlc_playlist_item_t *item = randomizer_Next(&randomizer);
        assert(item);
    }

    assert(randomizer_HasNext(&randomizer)); /* still has items in loop */

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

static void
test_cycle_with_additions_and_removals(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);
    randomizer_SetLoop(&randomizer, true);

    #define SIZE 100
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, 80);
    assert(ok);

    for (int i = 0; i < 30; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        vlc_playlist_item_t *item = randomizer_Next(&randomizer);
        assert(item);
    }

    vlc_playlist_item_t *to_remove[20];
    /* copy 10 items already selected */
    memcpy(to_remove, &randomizer.items.data[15], 10 * sizeof(*to_remove));
    /* copy 10 items not already selected */
    memcpy(&to_remove[10], &randomizer.items.data[60], 10 * sizeof(*to_remove));

    randomizer_Remove(&randomizer, to_remove, 20);

    /* it remains 40 items in the first cycle (30 already selected, and 10
     * removed from the 50 remaining) */
    for (int i = 0; i < 40; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        vlc_playlist_item_t *item = randomizer_Next(&randomizer);
        assert(item);
    }

    /* the first cycle is complete */
    assert(randomizer_HasNext(&randomizer));
    /* force the determination of the first item of the next cycle */
    vlc_playlist_item_t *item = randomizer_PeekNext(&randomizer);
    assert(item);

    assert(randomizer.items.size == 60);
    assert(randomizer.history == 1);

    /* save current history */
    vlc_playlist_item_t *history[59];
    memcpy(history, &randomizer.items.data[1], 59 * sizeof(*history));

    /* insert 20 new items */
    ok = randomizer_Add(&randomizer, &items[80], 20);
    assert(ok);

    assert(randomizer.items.size == 80);
    assert(randomizer.history == 21);

    for (int i = 0; i < 59; ++i)
        assert(history[i] == randomizer.items.data[21 + i]);

    /* remove 10 items in the history part */
    memcpy(to_remove, &randomizer.items.data[30], 10 * sizeof(*to_remove));
    randomizer_Remove(&randomizer, to_remove, 10);

    assert(randomizer.items.size == 70);
    assert(randomizer.history == 21);

    /* the other items in the history must be kept in order */
    for (int i = 0; i < 9; ++i)
        assert(history[i] == randomizer.items.data[21 + i]);
    for (int i = 0; i < 40; ++i)
        assert(history[i + 19] == randomizer.items.data[30 + i]);

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

static void
test_force_select_new_item(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);

    #define SIZE 100
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, SIZE);
    assert(ok);

    bool selected[SIZE] = {0};
    for (int i = 0; i < SIZE; ++i)
    {
        vlc_playlist_item_t *item;
        if (i != 50)
        {
            assert(randomizer_HasNext(&randomizer));
            item = randomizer_Next(&randomizer);
        }
        else
        {
            /* force the selection of a new item not already selected */
            item = randomizer.items.data[62];
            randomizer_Select(&randomizer, item);
            /* the item should now be the last selected one */
            assert(randomizer.items.data[randomizer.next - 1] == item);
        }
        assert(item);
        assert(!selected[item->index]); /* never selected twice */
        selected[item->index] = true;
    }

    assert(!randomizer_HasNext(&randomizer)); /* no more items */

    for (int i = 0; i < SIZE; ++i)
        assert(selected[i]); /* all selected */

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
}

static void
test_force_select_item_already_selected(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);

    #define SIZE 100
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, SIZE);
    assert(ok);

    bool selected[SIZE] = {0};
    /* we need an additional loop cycle, since we select the same item twice */
    for (int i = 0; i < SIZE + 1; ++i)
    {
        vlc_playlist_item_t *item;
        if (i != 50)
        {
            assert(randomizer_HasNext(&randomizer));
            item = randomizer_Next(&randomizer);
        }
        else
        {
            /* force the selection of an item already selected */
            item = randomizer.items.data[42];
            randomizer_Select(&randomizer, item);
            /* the item should now be the last selected one */
            assert(randomizer.items.data[randomizer.next - 1] == item);
        }
        assert(item);
        /* never selected twice, except for item 50 */
        assert((i != 50) ^ selected[item->index]);
        selected[item->index] = true;
    }

    assert(!randomizer_HasNext(&randomizer)); /* no more items */

    for (int i = 0; i < SIZE; ++i)
        assert(selected[i]); /* all selected */

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

static void
test_prev(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);

    #define SIZE 10
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, SIZE);
    assert(ok);

    assert(!randomizer_HasPrev(&randomizer));

    vlc_playlist_item_t *actual[SIZE];
    for (int i = 0; i < SIZE; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        actual[i] = randomizer_Next(&randomizer);
        assert(actual[i]);
    }

    assert(!randomizer_HasNext(&randomizer));

    for (int i = SIZE - 2; i >= 0; --i)
    {
        assert(randomizer_HasPrev(&randomizer));
        vlc_playlist_item_t *item = randomizer_Prev(&randomizer);
        assert(item == actual[i]);
    }

    assert(!randomizer_HasPrev(&randomizer));

    for (int i = 1; i < SIZE; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        vlc_playlist_item_t *item = randomizer_Next(&randomizer);
        assert(item == actual[i]);
    }

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

static void
test_prev_with_select(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);

    #define SIZE 10
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, SIZE);
    assert(ok);

    assert(!randomizer_HasPrev(&randomizer));

    vlc_playlist_item_t *actual[SIZE];
    for (int i = 0; i < 5; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        actual[i] = randomizer_Next(&randomizer);
        assert(actual[i]);
    }

    randomizer_Select(&randomizer, actual[2]);

    vlc_playlist_item_t *item;

    assert(randomizer_HasPrev(&randomizer));
    item = randomizer_Prev(&randomizer);
    assert(item == actual[4]);

    assert(randomizer_HasPrev(&randomizer));
    item = randomizer_Prev(&randomizer);
    assert(item == actual[3]);

    assert(randomizer_HasPrev(&randomizer));
    item = randomizer_Prev(&randomizer);
    assert(item == actual[1]);

    assert(randomizer_HasPrev(&randomizer));
    item = randomizer_Prev(&randomizer);
    assert(item == actual[0]);

    assert(!randomizer_HasPrev(&randomizer));

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

static void
test_prev_across_reshuffle_loops(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);

    #define SIZE 10
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, SIZE);
    assert(ok);

    assert(!randomizer_HasPrev(&randomizer));

    vlc_playlist_item_t *actual[SIZE];
    for (int i = 0; i < SIZE; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        actual[i] = randomizer_Next(&randomizer);
        assert(actual[i]);
    }

    assert(!randomizer_HasNext(&randomizer));
    randomizer_SetLoop(&randomizer, true);
    assert(randomizer_HasNext(&randomizer));

    vlc_playlist_item_t *actualnew[4];
    /* determine the 4 first items */
    for (int i = 0; i < 4; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        actualnew[i] = randomizer_Next(&randomizer);
        assert(actualnew[i]);
    }

    /* go back to the first */
    for (int i = 2; i >= 0; --i)
    {
        assert(randomizer_HasPrev(&randomizer));
        actualnew[i] = randomizer_Prev(&randomizer);
        assert(actualnew[i]);
    }

    assert(actualnew[0] == randomizer.items.data[0]);

    /* from now, any "prev" goes back to the history */

    int index_in_actual = 9;
    for (int i = 0; i < 6; ++i)
    {
        assert(randomizer_HasPrev(&randomizer));
        vlc_playlist_item_t *item = randomizer_Prev(&randomizer);

        int j;
        for (j = 3; j >= 0; --j)
            if (item == actualnew[j])
                break;
        bool in_actualnew = j != 0;

        if (in_actualnew)
            /* the item has been selected for the new order, it is not in the
             * history anymore */
            index_in_actual--;
        else
            /* the remaining previous items are retrieved in reverse order in
             * the history */
            assert(item == actual[index_in_actual]);
    }

    /* no more history: 4 in the current shuffle, 6 in the history */
    assert(!randomizer_HasPrev(&randomizer));

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

/* when loop is enabled, we must take care that the last items of the
 * previous order are not the same as the first items of the new order */
static void
test_loop_respect_not_same_before(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);
    randomizer_SetLoop(&randomizer, true);

    #define SIZE (NOT_SAME_BEFORE + 2)
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, SIZE);
    assert(ok);

    vlc_playlist_item_t *actual[SIZE];
    for (int i = 0; i < SIZE; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        actual[i] = randomizer_Next(&randomizer);
    }

    for (int cycle = 0; cycle < 20; cycle++)
    {
        /* check that the first items are not the same as the last ones of the
         * previous order */
        for (int i = 0; i < NOT_SAME_BEFORE; ++i)
        {
            assert(randomizer_HasNext(&randomizer));
            actual[i] = randomizer_Next(&randomizer);
            for (int j = (i + SIZE - NOT_SAME_BEFORE) % SIZE;
                 j != i;
                 j = (j + 1) % SIZE)
            {
                assert(actual[i] != actual[j]);
            }
        }
        for (int i = NOT_SAME_BEFORE; i < SIZE; ++i)
        {
            assert(randomizer_HasNext(&randomizer));
            actual[i] = randomizer_Next(&randomizer);
        }
    }

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

/* if there are less items than NOT_SAME_BEFORE, obviously we can't avoid
 * repeating last items in the new order, but it must still work */
static void
test_loop_respect_not_same_before_impossible(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);
    randomizer_SetLoop(&randomizer, true);

    #define SIZE NOT_SAME_BEFORE
    vlc_playlist_item_t *items[SIZE];
    ArrayInit(items, SIZE);

    bool ok = randomizer_Add(&randomizer, items, SIZE);
    assert(ok);

    for (int i = 0; i < 10 * SIZE; ++i)
    {
        assert(randomizer_HasNext(&randomizer));
        vlc_playlist_item_t *item = randomizer_Next(&randomizer);
        assert(item);
    }

    ArrayDestroy(items, SIZE);
    randomizer_Destroy(&randomizer);
    #undef SIZE
}

static void
test_has_prev_next_empty(void)
{
    struct randomizer randomizer;
    randomizer_Init(&randomizer);

    assert(!randomizer_HasPrev(&randomizer));
    assert(!randomizer_HasNext(&randomizer));

    randomizer_SetLoop(&randomizer, true);

    assert(!randomizer_HasPrev(&randomizer));

    /* there are always next items in loop mode */
    assert(randomizer_HasNext(&randomizer));

    randomizer_Destroy(&randomizer);
}

int main(void)
{
    test_all_items_selected_exactly_once();
    test_all_items_selected_exactly_once_per_cycle();
    test_all_items_selected_exactly_once_with_additions();
    test_all_items_selected_exactly_once_with_removals();
    test_cycle_after_manual_selection();
    test_cycle_with_additions_and_removals();
    test_force_select_new_item();
    test_force_select_item_already_selected();
    test_prev();
    test_prev_with_select();
    test_prev_across_reshuffle_loops();
    test_loop_respect_not_same_before();
    test_loop_respect_not_same_before_impossible();
    test_has_prev_next_empty();
}

#endif
#endif
