/*****************************************************************************
 * chroma_probe.c: chroma conversion probing
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#include <vlc_chroma_probe.h>
#include <vlc_fourcc.h>
#include <vlc_threads.h>
#include <vlc_modules.h>
#include <vlc_sort.h>
#include <vlc_memstream.h>

static int
modules_Probe(vlc_chroma_conv_vec *chroma_table)
{
    module_t **mods;
    ssize_t total = vlc_module_match("chroma probe", NULL, false, &mods, NULL);
    if (total == -1)
        return -ENOENT;

    for (ssize_t i = 0; i < total; ++i)
    {
        vlc_chroma_conv_probe fn = vlc_module_map(NULL, mods[i]);
        if (fn == NULL)
            continue;
        fn(chroma_table);
    }
    free(mods);
    return 0;
}

/* Breadth First Search (BFS) node */
struct bfs_node
{
    vlc_fourcc_t chain[VLC_CHROMA_CONV_CHAIN_COUNT_MAX];
    unsigned depth; /* Max deep is VLC_CHROMA_CONV_CHAIN_COUNT_MAX -1 */
    float cost_factor;
};
typedef struct VLC_VECTOR(struct bfs_node) bfs_queue_vec;

static int
bfs_Run(vlc_fourcc_t chroma_from, vlc_fourcc_t chroma_to, unsigned max_depth,
        const vlc_chroma_conv_vec *chroma_table, int flags,
        bfs_queue_vec *queue)
{
    struct bfs_node start = {
        .chain[0] = chroma_from,
        .cost_factor = 1,
        .depth = 0,
    };
    bool success = vlc_vector_push(queue, start);
    if (!success)
        return -ENOMEM;

    for (size_t queue_idx = 0; queue_idx < queue->size; queue_idx++)
    {
        const struct bfs_node current = queue->data[queue_idx];
        vlc_fourcc_t current_chroma = current.chain[current.depth];

        if (chroma_to != 0 && current_chroma == chroma_to)
            continue; /* Found a path to 'chroma_to' */

        if (current.depth == max_depth)
            continue;

        /* Enqueue neighbors */
        for (size_t chroma_idx = 0; chroma_idx < chroma_table->size; chroma_idx++)
        {
            struct vlc_chroma_conv_entry *entry = &chroma_table->data[chroma_idx];
            vlc_fourcc_t from = entry->in;
            vlc_fourcc_t to = entry->out;
            float cost_factor = entry->cost_factor;

            if (from == current_chroma)
            {
                vlc_fourcc_t next_chroma = to;

                /* Apply filters from flags */
                if (flags & VLC_CHROMA_CONV_FLAG_ONLY_YUV)
                {
                    if (!vlc_fourcc_IsYUV(next_chroma))
                        continue;
                }
                else if (flags & VLC_CHROMA_CONV_FLAG_ONLY_RGB)
                {
                    const vlc_chroma_description_t *desc =
                        vlc_fourcc_GetChromaDescription(next_chroma);
                    if (desc == NULL || desc->subtype != VLC_CHROMA_SUBTYPE_RGB)
                        continue;
                }

                /* If next_chroma is already in the chain at any previous step,
                 * we've encountered a cycle or a duplicate. */
                bool already_visited = false;
                for (size_t i = 0; i < current.depth; ++i)
                    if (current.chain[i] == next_chroma)
                    {
                        already_visited = true;
                        break;
                    }
                if (already_visited)
                    continue;

                struct bfs_node next = current;
                next.depth = current.depth + 1;
                next.cost_factor = current.cost_factor * cost_factor;

                next.chain[next.depth] = next_chroma;
                success = vlc_vector_push(queue, next);
                if (!success)
                    return -ENOMEM;
            }
        }
    }
    return 0;
}

static uint64_t
GetChromaBits(const vlc_chroma_description_t *desc,
              unsigned width, unsigned height)
{
    if (desc->plane_count == 0)
    {
        /* Fallback to the size of the subtype */
        switch (desc->subtype)
        {
            case VLC_CHROMA_SUBTYPE_OTHER:
                return 0;
            case VLC_CHROMA_SUBTYPE_YUV444:
                return width * height * 3 * desc->color_bits;
            case VLC_CHROMA_SUBTYPE_YUV440:
            case VLC_CHROMA_SUBTYPE_YUV422:
                return width * height * 2 * desc->color_bits;
            case VLC_CHROMA_SUBTYPE_YUV420:
            case VLC_CHROMA_SUBTYPE_YUV411:
                return width * height * 1.5 * desc->color_bits;
            case VLC_CHROMA_SUBTYPE_YUV410:
                return width * height * 1.125 * desc->color_bits;
            case VLC_CHROMA_SUBTYPE_YUV211:
            case VLC_CHROMA_SUBTYPE_GREY:
                return width * height * desc->color_bits;
            case VLC_CHROMA_SUBTYPE_RGB:
                return width * height * 4 * desc->color_bits;
            default:
                vlc_assert_unreachable();
        }
    }

    uint64_t total_bits = 0;
    for (unsigned i = 0; i < desc->plane_count; i++)
    {
        const vlc_rational_t rw = desc->p[i].w;
        const vlc_rational_t rh = desc->p[i].h;

        unsigned plane_width = width * rw.num / rw.den;
        unsigned plane_height = height * rh.num / rh.den;

        uint64_t plane_pixels = plane_width * plane_height;
        uint64_t plane_bits = plane_pixels * desc->pixel_bits;

        total_bits += plane_bits;
    }

    return total_bits;
}

static float
GetColorRatio(enum vlc_chroma_subtype subtype)
{
    switch (subtype)
    {
        case VLC_CHROMA_SUBTYPE_YUV444:
        case VLC_CHROMA_SUBTYPE_RGB:
            return 1.f;
        case VLC_CHROMA_SUBTYPE_YUV422:
            return 0.67;
        case VLC_CHROMA_SUBTYPE_YUV440:
            return 0.5; /* should be like YUV422, but it is less common */
        case VLC_CHROMA_SUBTYPE_YUV420:
            return 0.5;
        case VLC_CHROMA_SUBTYPE_YUV411:
            return 0.33;
        case VLC_CHROMA_SUBTYPE_YUV410:
            return 0.25;
        case VLC_CHROMA_SUBTYPE_YUV211:
        case VLC_CHROMA_SUBTYPE_OTHER:
            return 0.2;
        case VLC_CHROMA_SUBTYPE_GREY:
            return 0.1;
        default:
            vlc_assert_unreachable();
    }
}

static float
CompareDescs(const vlc_chroma_description_t *in_desc,
             const vlc_chroma_description_t *out_desc)
{
    /* Compare color bits */
    float bits_ratio;
    if (in_desc->color_bits == 0 || out_desc->color_bits == 0)
        bits_ratio = 1.f;
    else
    {
        bits_ratio = out_desc->color_bits / in_desc->color_bits;
        if (bits_ratio > 1.f)
            bits_ratio = 1.f;
    }

    /* Compare color ratios, favor same or near subtype */
    if (in_desc->subtype == out_desc->subtype)
        return bits_ratio;

    float color_ratio = GetColorRatio(out_desc->subtype)
                      / GetColorRatio(in_desc->subtype);
    if (color_ratio > 1.f)
        color_ratio = 1.f;

    /* Malus for CPU YUV <-> Other. Favor staying in the same color model. */
    bool in_is_yuv = vlc_chroma_description_IsYUV(in_desc);
    bool out_is_yuv = vlc_chroma_description_IsYUV(out_desc);
    if ((in_desc->plane_count != 0 && out_desc->plane_count != 0)
     && (in_is_yuv || out_is_yuv) && (in_is_yuv != out_is_yuv))
        color_ratio *= 0.9;

    return color_ratio * bits_ratio;
}

static void
vlc_chroma_conv_result_FromNode(struct vlc_chroma_conv_result *res,
                                const struct bfs_node *node,
                                unsigned width, unsigned height)
{
    res->chain_count = node->depth + 1;
    res->cost = 0;

    uint64_t total_cost = 0;
    float total_quality = 1.f;
    for (size_t i = 0; i < res->chain_count; ++i)
    {
        res->chain[i] = node->chain[i];

        if (i > 0)
        {
            const vlc_chroma_description_t *from_desc =
                vlc_fourcc_GetChromaDescription(res->chain[i - 1]);
            const vlc_chroma_description_t *to_desc =
                vlc_fourcc_GetChromaDescription(res->chain[i]);

            if (from_desc == NULL || to_desc == NULL)
            {
                /* Unlikely, fallback for a big cost */
                total_cost += width * height * 4 * 8 * node->cost_factor;
                continue;
            }

            uint64_t from_bits = GetChromaBits(from_desc, width, height);
            uint64_t to_bits = GetChromaBits(to_desc, width, height);

            /* Unlikely case */
            if (from_bits == 0) /* OTHER -> ANY */
                from_bits = to_bits;
            else if (to_bits == 0) /* ANY -> OTHER */
                to_bits = from_bits;

            total_cost += (from_bits + to_bits) * node->cost_factor;

            float quality = CompareDescs(from_desc, to_desc);
            assert(quality > 0.f && quality <= 1.f);

            total_quality *= quality;
        }
    }
    res->cost = total_cost / width / height;
    res->quality = 100 * total_quality;
}

static int
SortResults(const void *a, const void *b, void *arg)
{
    const struct vlc_chroma_conv_result *ra = a;
    const struct vlc_chroma_conv_result *rb = b;
    bool *sort_by_quality = arg;

    int cost_score = 0, quality_score = 0;

    /* Lower cost is better */
    if (ra->cost < rb->cost)
        cost_score = -1;
    else if (ra->cost > rb->cost)
        cost_score = 1;

    /* Higher Quality is better */
    if (ra->quality > rb->quality)
        quality_score = -1;
    else if (ra->quality < rb->quality)
        quality_score = 1;

    /* Fallback to secondary score in same score */
    if (*sort_by_quality)
        return quality_score != 0 ? quality_score : cost_score;
    else
        return cost_score != 0 ? cost_score : quality_score;
}

static bool
bfs_node_IsResult(const struct bfs_node *node, vlc_fourcc_t to)
{
    vlc_fourcc_t current_chroma = node->chain[node->depth];
    return to == 0 || current_chroma == to;
}

static bool
vlc_chroma_conv_result_Equals(struct vlc_chroma_conv_result *a,
                              struct vlc_chroma_conv_result *b)
{
    if (a->chain_count != b->chain_count)
        return false;
    if (a->quality != b->quality)
        return false;
    /* Don't check cost since we want to merge results with different costs */
    for (size_t i = 0; i < a->chain_count; ++i)
        if (a->chain[i] != b->chain[i])
            return false;
    return true;
}

struct vlc_chroma_conv_result *
vlc_chroma_conv_Probe(vlc_fourcc_t from, vlc_fourcc_t to,
                      unsigned width, unsigned height,
                      unsigned max_indirect_steps, int flags, size_t *count)
{
    assert(from != 0);
    assert(max_indirect_steps <= VLC_CHROMA_CONV_MAX_INDIRECT_STEPS);
    vlc_chroma_conv_vec chroma_table;
    vlc_vector_init(&chroma_table);

    if (width == 0 || height == 0)
    {
        width = 3840;
        height = 2160;
    }

    if (max_indirect_steps > 0)
    {
        /* Allow indirect steps only when converting from/to a GPU chroma */
        bool from_cpu = vlc_fourcc_GetChromaBPP(from) != 0;
        bool to_cpu = to == 0 ? true : vlc_fourcc_GetChromaBPP(to) != 0;
        if (from_cpu && to_cpu)
            max_indirect_steps--;
    }

    /* Probe modules */
    int ret = modules_Probe(&chroma_table);
    if (ret != 0 || chroma_table.size == 0)
    {
        vlc_vector_destroy(&chroma_table);
        return NULL;
    }

    /* Run tree search */
    bfs_queue_vec bfs_queue;
    vlc_vector_init(&bfs_queue);
    ret = bfs_Run(from, to, max_indirect_steps + 1 , &chroma_table, flags,
                  &bfs_queue);

    vlc_vector_destroy(&chroma_table);

    size_t result_count = 0;
    for (size_t i = 1 /* skip start node */; i < bfs_queue.size; ++i)
        if (bfs_node_IsResult(&bfs_queue.data[i], to))
            result_count++;

    if (unlikely(ret != 0) || result_count == 0)
    {
        vlc_vector_destroy(&bfs_queue);
        return NULL;
    }

    /* Allocate the result array */
    struct VLC_VECTOR(struct vlc_chroma_conv_result) result_vec =
        VLC_VECTOR_INITIALIZER;
    bool success = vlc_vector_push_hole(&result_vec, result_count);
    if (!success)
    {
        vlc_vector_destroy(&bfs_queue);
        return NULL;
    }

    /* Fill the result from the tree search */
    size_t res_idx = 0;
    for (size_t i = 1 /* skip start node */; i < bfs_queue.size; ++i)
    {
        const struct bfs_node *node = &bfs_queue.data[i];
        if (!bfs_node_IsResult(node, to))
            continue;

        assert(res_idx < result_count);
        struct vlc_chroma_conv_result *res = &result_vec.data[res_idx++];
        vlc_chroma_conv_result_FromNode(res, node, width, height);
    }
    assert(res_idx == result_count);

    vlc_vector_destroy(&bfs_queue);

    /* Sort */
    bool sort_by_quality = (flags & VLC_CHROMA_CONV_FLAG_SORT_COST) == 0;
    vlc_qsort(result_vec.data, result_count,
              sizeof(struct vlc_chroma_conv_result), SortResults,
              &sort_by_quality);

    /* Remove duplicate entries, it can happen when more the 2 modules probe
     * the same conversion. They are not necessarily one after the other as
     * they might have different quality. */
    for (size_t i = 0; i < result_vec.size - 1; ++ i)
    {
        struct vlc_chroma_conv_result *cur = &result_vec.data[i];

        size_t j = i + 1;
        while (j < result_vec.size)
        {
            struct vlc_chroma_conv_result *next = &result_vec.data[j];
            if (vlc_chroma_conv_result_Equals(cur, next))
            {
                /* Keep the lowest cost */
                if (next->cost < cur->cost)
                    cur->cost = next->cost;
                vlc_vector_remove(&result_vec, j);
            }
            else
                j++;
        }
    }

    *count = result_vec.size;
    return result_vec.data;
}

char *
vlc_chroma_conv_result_ToString(const struct vlc_chroma_conv_result *res)
{
    struct vlc_memstream ms;
    int ret = vlc_memstream_open(&ms);
    if (ret != 0)
        return NULL;
    vlc_memstream_printf(&ms, "[c=%u|q=%u] ", res->cost, res->quality);

    for (size_t i = 0; i < res->chain_count; ++i)
    {
        vlc_memstream_printf(&ms, "%4.4s", (const char *) &res->chain[i]);
        if (i != res->chain_count - 1)
            vlc_memstream_puts(&ms, " -> ");
    }
    ret = vlc_memstream_close(&ms);
    return ret == 0 ? ms.ptr : NULL;
}
