/*
 * Copyright (C) 2022 VLC authors and VideoLAN
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
#include "config.h"
#endif

#include <vlc_diffutil.h>
#include <assert.h>

// An O(ND) Difference Algorithm and Its Variations, EUGENE W. MYERS. (A Linear Space Refinement)
// inspired from blogpost serie https://blog.jcoglan.com/2017/03/22/myers-diff-in-linear-space-theory/

typedef struct {
    int32_t x;
    int32_t y;
} diffutil_snakepoint_t;

typedef struct diffutil_snake_t VLC_VECTOR(diffutil_snakepoint_t) diffutil_snake_t;

typedef struct {
    int32_t left;
    int32_t right;
    int32_t top;
    int32_t bottom;

    //derived values
    int32_t size;
    int32_t delta;
} diffutil_box_t;


typedef struct {
    const vlc_diffutil_callback_t* op;
    const void* dataNew;
    const void* dataOld;
    int32_t z;
    int32_t* forward;
    int32_t* backward;
} diffutil_context_t;

#define POS_MOD(x, n) ((x) % n + n) % n

static bool Forwards(diffutil_context_t* ctx, const diffutil_box_t* box, int32_t d, diffutil_snakepoint_t* snakeOut)
{
    int z = ctx->z;
    int32_t* forward = ctx->forward;
    int32_t* backward = ctx->backward;
    const int32_t boxdelta = box->delta;
    for (int k = d; k >= -d; k -= 2)
    {
        int c = k - boxdelta;

        int x, px;
        if (k == -d || (k != d && forward[POS_MOD(k - 1, z)] < forward[POS_MOD(k + 1, z)]) )
        {
            px = forward[POS_MOD(k + 1, z)];
            x = px;
        }
        else
        {
            px = forward[POS_MOD(k - 1, z)];
            x  = px + 1;
        }

        int32_t y  = box->top + (x - box->left) - k;
        int32_t py = (d == 0 || x != px) ? y : y - 1;

        while (x < box->right
            && y < box->bottom
            && ctx->op->isSame(ctx->dataOld, x, ctx->dataNew, y)
            )
        {
            x++;
            y++;
        }
        forward[POS_MOD(k, z)] = x;

        if ( boxdelta % 2 != 0
             && ((-(d - 1)) <= c && c <= d - 1)
             && y >= backward[POS_MOD(c, z)] )
        {
            snakeOut[0].x = px;
            snakeOut[0].y = py;
            snakeOut[1].x = x;
            snakeOut[1].y = y;
            return true;
        }
    }

    return false;
}

static bool Backward(diffutil_context_t* ctx, const diffutil_box_t* box, int32_t d, diffutil_snakepoint_t* snakeOut)
{
    int32_t z = ctx->z;
    int32_t* forward = ctx->forward;
    int32_t* backward = ctx->backward;
    const int32_t boxdelta = box->delta;
    for (int c = d; c >= -d; c -=2)
    {
        int k = c + boxdelta;

        int y, py;
        if (c == -d || (c != d && backward[POS_MOD(c - 1, z)] > backward[POS_MOD(c + 1, z)]))
        {
            py = backward[POS_MOD(c + 1, z)];
            y = py;
        }
        else
        {
            py = backward[POS_MOD(c - 1, z)];
            y  = py - 1;
        }

        int32_t x  = box->left + (y - box->top) + k;
        int32_t px = (d == 0 || y != py) ? x : x + 1;

        while (x > box->left
            && y > box->top
            && ctx->op->isSame(ctx->dataOld, x-1, ctx->dataNew, y-1)
            )
        {
            x--;
            y--;
        }

        backward[POS_MOD(c, z)] = y;

        if (boxdelta % 2 == 0
            && (-d <= k && k <= d)
            && x <= forward[POS_MOD(k, z)]
            )
        {
            snakeOut[0].x = x;
            snakeOut[0].y = y;
            snakeOut[1].x = px;
            snakeOut[1].y = py;
            return true;
        }
    }
    return false;
}


static bool FindMidPoint(diffutil_context_t* ctx, const diffutil_box_t* box, diffutil_snakepoint_t snakeOut[2])
{
    assert(box != NULL);
    int boxSize = box->size;
    if (boxSize == 0)
        return false;

    int boxmax = (boxSize + 1) / 2;

    ctx->forward[1] = box->left;
    ctx->backward[1] = box->bottom;

    bool ret;
    for (int32_t i = 0; i <= boxmax; i++)
    {
        ret = Forwards(ctx, box, i, snakeOut);
        if (ret)
            return true;
        ret = Backward(ctx, box, i, snakeOut);
        if (ret)
            return true;
    }

    return false;
}

static diffutil_snake_t* MergeSnake(const diffutil_snakepoint_t* start, const diffutil_snakepoint_t* stop,
                                    diffutil_snake_t* head, diffutil_snake_t* tail)
{
    assert(start);
    assert(stop);
    diffutil_snake_t* snake = NULL;

    if (head)
        snake = head;
    else
    {
        snake = malloc(sizeof(diffutil_snake_t));
        if (!snake)
        {
            goto out;
        }
        vlc_vector_init(snake);
        vlc_vector_push(snake, *start);
    }
    if (tail)
    {
        vlc_vector_push_all(snake, tail->data, tail->size);

    }
    else
        vlc_vector_push(snake, *stop);

out:
    if (tail)
    {
        vlc_vector_destroy(tail);
        free(tail);
    }

    return snake;
}

static diffutil_snake_t* DiffUtilFindPath(diffutil_context_t* ctx, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    diffutil_box_t box = {
        .left = left,
        .right = right,
        .bottom = bottom,
        .top = top
    };
    int32_t boxWidth = right - left;
    int32_t boxHeight = bottom - top;
    box.size = boxWidth + boxHeight;
    box.delta = boxWidth - boxHeight;

    diffutil_snakepoint_t snake[2];
    bool ret = FindMidPoint(ctx, &box, snake);
    if (!ret)
        return NULL;

    diffutil_snake_t* head = DiffUtilFindPath(ctx, box.left, box.top, snake[0].x, snake[0].y);
    diffutil_snake_t* tail = DiffUtilFindPath(ctx, snake[1].x, snake[1].y, box.right, box.bottom);
    return MergeSnake(&snake[0], &snake[1], head, tail);
}


diffutil_snake_t* vlc_diffutil_build_snake(const vlc_diffutil_callback_t* diffOp, const void* dataOld, const void* dataNew)
{
    assert(dataNew);
    assert(dataOld);
    assert(diffOp);
    assert(diffOp->getNewSize);
    assert(diffOp->getOldSize);
    assert(diffOp->isSame);

    diffutil_context_t ctx;
    ctx.op = diffOp;
    ctx.dataNew = dataNew;
    ctx.dataOld = dataOld;
    ctx.forward = NULL;
    ctx.backward = NULL;

    diffutil_snake_t* snake = NULL;

    int oldSize = diffOp->getOldSize(dataOld);
    int newSize = diffOp->getOldSize(dataNew);

    if (oldSize < 0 || newSize < 0)
        return NULL;

    if (oldSize == 0 && newSize ==0)
    {
        snake = malloc(sizeof(diffutil_snake_t));
        if (!snake)
            return NULL;
        vlc_vector_init(snake);
        return snake;
    }

    ctx.z = (2 * (oldSize + newSize) + 1);
    ctx.forward = malloc( ctx.z * sizeof(int32_t) );
    if (!ctx.forward)
        goto out;
    ctx.backward = malloc( ctx.z * sizeof(int32_t) );
    if (!ctx.backward)
        goto out;


    snake = DiffUtilFindPath(&ctx, 0, 0, oldSize, newSize);

out:
    if (ctx.forward)
        free(ctx.forward);
    if (ctx.backward)
        free(ctx.backward);
    return snake;
}

void vlc_diffutil_free_snake(diffutil_snake_t* snake)
{
    if (snake)
    {
        vlc_vector_destroy(snake);
        free(snake);
    }
}

bool vlc_diffutil_walk_snake(
    const diffutil_snake_t* snake,
    const vlc_diffutil_snake_callback_t* snakeOp, void* cbData,
    const vlc_diffutil_callback_t* diffOp, const void* dataOld, const void* dataNew)
{
    if (!snake)
        return false;

    for (size_t i = 1; i < snake->size; i++)
    {
        int32_t x1 = snake->data[i-1].x;
        int32_t y1 = snake->data[i-1].y;
        int32_t x2 = snake->data[i].x;
        int32_t y2 = snake->data[i].y;

        while (x1 < x2 && y1 < y2 && diffOp->isSame(dataOld, x1, dataNew, y1))
        {
            if (snakeOp->equal)
                snakeOp->equal(cbData, dataOld, x1, dataNew, y1);
            x1++;
            y1++;
        }

        int32_t sign =  (x2 - x1) - (y2 - y1);
        if (sign < 0)
        {
            if (snakeOp->insert)
                snakeOp->insert(cbData, dataOld, x1, dataNew, y1);
            y1++;
        }
        else if (sign > 0)
        {
            if (snakeOp->remove)
                snakeOp->remove(cbData, dataOld, x1, dataNew, y1);
            x1++;
        }

        while (x1 < x2 && y1 < y2 && diffOp->isSame(dataOld, x1, dataNew, y1))
        {
            if (snakeOp->equal)
                snakeOp->equal(cbData, dataOld, x1, dataNew, y1);
            x1++;
            y1++;
        }
    }

    return true;
}


typedef struct
{
    vlc_diffutil_changelist_t* result;
    enum vlc_diffutil_result_flag flags;
    uint32_t head;
    uint32_t countInsert;
    uint32_t countRemove;
} vlc_build_result_context;

static void buildResultInsert(void* cbData, const void* listOld, uint32_t oldPos, const void* listNew, uint32_t newPos)
{
    VLC_UNUSED(listOld);
    VLC_UNUSED(listNew);

    vlc_build_result_context* ctx = cbData;
    if ((ctx->flags & VLC_DIFFUTIL_RESULT_AGGREGATE)
        && !(ctx->flags & VLC_DIFFUTIL_RESULT_MOVE)
        && ctx->result->size > 0 )
    {
        vlc_diffutil_change_t* previousOp = &ctx->result->data[ctx->result->size - 1];
        if (previousOp->type == VLC_DIFFUTIL_OP_INSERT
            && previousOp->op.insert.y + previousOp->count == newPos)
        {
            ctx->head++;
            previousOp->count += 1;
            return;
        }
    }

    vlc_diffutil_change_t op = {
        .type = VLC_DIFFUTIL_OP_INSERT,
        .op.insert = {
            .x = oldPos,
            .y = newPos,
            .index = ctx->head
        },
        .count = 1,
    };
    ctx->head++;
    ctx->countInsert++;
    vlc_vector_push(ctx->result, op);
}

static void buildResultRemove(
    void* cbData, const void* listOld, unsigned posOld, const void* listNew, uint32_t posNew)
{
    VLC_UNUSED(listOld);
    VLC_UNUSED(listNew);

    vlc_build_result_context* ctx = cbData;
    if ((ctx->flags & VLC_DIFFUTIL_RESULT_AGGREGATE)
        && !(ctx->flags & VLC_DIFFUTIL_RESULT_MOVE)
        && ctx->result->size > 0 )
    {
        vlc_diffutil_change_t* previousOp = &ctx->result->data[ctx->result->size - 1];
        if (previousOp->type == VLC_DIFFUTIL_OP_REMOVE
            && previousOp->op.remove.x + previousOp->count == posOld)
        {
            previousOp->count += 1;
            return;
        }
    }

    vlc_diffutil_change_t op = {
        .type = VLC_DIFFUTIL_OP_REMOVE,
        .op.remove = {
            .x = posOld,
            .y = posNew,
            .index = ctx->head
        },
        .count = 1,
    };
    ctx->countRemove++;
    vlc_vector_push(ctx->result, op);
}

static void buildResultEqual(
    void* cbData, const void* listOld, uint32_t posOld, const void* listNew, uint32_t posNew)
{
    VLC_UNUSED(listOld);
    VLC_UNUSED(posOld);
    VLC_UNUSED(listNew);
    VLC_UNUSED(posNew);

    vlc_build_result_context* ctx = cbData;
    ctx->head++;
}

static void vlc_diffutil_gather_move_changes(
    vlc_build_result_context* ctx,
    const vlc_diffutil_callback_t* diffOp, const void* dataOld, const void* dataNew,
    enum vlc_diffutil_result_flag flags)
{
    vlc_diffutil_change_t* prev = NULL;
    for (size_t i = 0; i < ctx->result->size; i++)
    {
        vlc_diffutil_change_t* op = &ctx->result->data[i];
        if (op->type == VLC_DIFFUTIL_OP_INSERT)
            ctx->countInsert--;
        else if (op->type == VLC_DIFFUTIL_OP_REMOVE)
            ctx->countRemove--;

        if (op->type == VLC_DIFFUTIL_OP_INSERT && ctx->countRemove > 0)
        {
            //account for items in between that will be deleted
            int32_t delta = -1;
            for (size_t j = i+1; j < ctx->result->size; j++)
            {
                if (ctx->result->data[j].type == VLC_DIFFUTIL_OP_REMOVE)
                {
                    vlc_diffutil_change_t* removeOp = &ctx->result->data[j];
                    if (diffOp->isSame(dataOld, removeOp->op.remove.x, dataNew, op->op.insert.y))
                    {
                        int32_t toX = op->op.insert.index;
                        int32_t fromX = removeOp->op.remove.index;
                        int32_t x = removeOp->op.remove.x;
                        int32_t y = op->op.insert.y;

                        op->type = VLC_DIFFUTIL_OP_MOVE;
                        op->op.move.from = fromX + delta;
                        op->op.move.to = toX;
                        op->op.move.x = x;
                        op->op.move.y = y;

                        removeOp->type = VLC_DIFFUTIL_OP_IGNORE;
                        ctx->countRemove--;

                        break;
                    }
                    else
                        delta += 1;
                }
                else if (ctx->result->data[j].type == VLC_DIFFUTIL_OP_INSERT)
                {
                    delta -= 1;
                }
            }
        }
        else if (op->type == VLC_DIFFUTIL_OP_REMOVE && ctx->countInsert > 0)
        {
            //account for items inbetween that will be deleted
            int32_t delta = 1;
            for (size_t j = i+1; j < ctx->result->size; j++)
            {
                if (ctx->result->data[j].type == VLC_DIFFUTIL_OP_INSERT)
                {
                    vlc_diffutil_change_t* insertOp = &ctx->result->data[j];
                    if (diffOp->isSame(dataOld, op->op.remove.x, dataNew, insertOp->op.insert.y))
                    {
                        int32_t toX = insertOp->op.insert.index;
                        int32_t fromX = op->op.remove.index;
                        int32_t x = op->op.remove.x;
                        int32_t y = insertOp->op.insert.y;

                        op->type = VLC_DIFFUTIL_OP_MOVE;
                        op->op.move.from = fromX;
                        op->op.move.to = toX + delta;
                        op->op.move.x = x;
                        op->op.move.y = y;

                        insertOp->type = VLC_DIFFUTIL_OP_IGNORE;
                        ctx->countInsert--;

                        break;
                    }
                    else
                        delta -= 1;
                }
                else if (ctx->result->data[j].type == VLC_DIFFUTIL_OP_REMOVE)
                    delta += 1;
            }
        }

        //test if we can aggregate the node with the previous one
        if ((flags & VLC_DIFFUTIL_RESULT_AGGREGATE) )
        {
            if (op->type == VLC_DIFFUTIL_OP_IGNORE)
                continue;

            else if (prev != NULL && op->type == prev->type)
            {
                if (op->type == VLC_DIFFUTIL_OP_INSERT
                    && op->op.insert.index == prev->op.insert.index + prev->count)
                {
                    prev->count += 1;
                    op->type = VLC_DIFFUTIL_OP_IGNORE;
                }
                else if (op->type == VLC_DIFFUTIL_OP_REMOVE
                    && op->op.remove.index == prev->op.remove.index)
                {
                    prev->count += 1;
                    op->type = VLC_DIFFUTIL_OP_IGNORE;
                }
                else if (op->type == VLC_DIFFUTIL_OP_MOVE )
                {
                    //move forward
                    if (op->op.move.from == prev->op.move.from
                        && op->op.move.to == prev->op.move.to)
                    {
                        prev->count += 1;
                        op->type = VLC_DIFFUTIL_OP_IGNORE;
                    }
                    //move backward
                    else if (op->op.move.from == prev->op.move.from + prev->count
                        && op->op.move.to == prev->op.move.to + prev->count)
                    {
                        prev->count += 1;
                        op->type = VLC_DIFFUTIL_OP_IGNORE;
                    }
                }
            }

            if (op->type != VLC_DIFFUTIL_OP_IGNORE)
                prev = op;
        }
    }
}

vlc_diffutil_changelist_t* vlc_diffutil_build_change_list(const diffutil_snake_t* snake, const vlc_diffutil_callback_t* diffOp,
    const void* dataOld, const void* dataNew, int flags)
{
    assert(snake);
    assert(diffOp);
    assert(dataNew);
    assert(dataOld);

    vlc_diffutil_snake_callback_t snakeOp= {
        .remove = &buildResultRemove,
        .insert = &buildResultInsert,
        .equal = &buildResultEqual,
    };

    vlc_build_result_context ctx;
    ctx.flags = flags;
    ctx.head = 0;
    ctx.result = malloc(sizeof(vlc_diffutil_changelist_t));
    ctx.countInsert = 0;
    ctx.countRemove = 0;

    if (!ctx.result)
        return NULL;
    vlc_vector_init(ctx.result);

    vlc_diffutil_walk_snake(snake, &snakeOp, &ctx, diffOp, dataOld, dataNew);

    //search for move
    if (flags & VLC_DIFFUTIL_RESULT_MOVE)
    {
        vlc_diffutil_gather_move_changes(&ctx, diffOp, dataOld, dataNew, flags);
    }
    return ctx.result;
}

void vlc_diffutil_free_change_list(vlc_diffutil_changelist_t* changelist)
{
    vlc_vector_destroy(changelist);
    free(changelist);
}
