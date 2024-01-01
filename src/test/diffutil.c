/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_diffutil.h>
#include <vlc_memstream.h>
#include <assert.h>

static uint32_t getSize(const void* ptr)
{
    const char* str = (const char*)(ptr);
    return strlen(str);
}

static bool isSame(const void* ptrA, uint32_t posA, const void* ptrB, uint32_t posB)
{
    const char* strA = (const char*)(ptrA);
    const char* strB = (const char*)(ptrB);
    return strA[posA] == strB[posB];
}

static void buildSnakeResultInsert(void* cbData, const void* listOld, uint32_t posOld, const void* listNew, uint32_t posNew)
{
    VLC_UNUSED(listOld);
    VLC_UNUSED(posOld);
    assert(listNew);
    const char* list = listNew;
    struct vlc_memstream* ms = cbData;
    vlc_memstream_putc(ms, '+');
    vlc_memstream_putc(ms, list[posNew]);
}

static void buildSnakeResultRemove(void* cbData, const void* listOld, uint32_t pos, const void* listNew,  uint32_t posNew)
{
    VLC_UNUSED(listNew);
    VLC_UNUSED(posNew);
    assert(listOld);
    const char* list = listOld;
    struct vlc_memstream* ms = cbData;
    vlc_memstream_putc(ms, '-');
    vlc_memstream_putc(ms, list[pos]);
}

static void buildSnakeResultEqual(void* cbData, const void* listOld, uint32_t posOld, const void* listNew, uint32_t posNew)
{
    VLC_UNUSED(listNew);
    VLC_UNUSED(posNew);
    assert(listOld);
    const char* list = listOld;
    struct vlc_memstream* ms = cbData;
    vlc_memstream_putc(ms, '=');
    vlc_memstream_putc(ms, list[posOld]);
}


static void check_snake(const char* oldInput, const char* newInput, const char* modList)
{
    vlc_diffutil_callback_t diffOp = {
        .getOldSize = &getSize,
        .getNewSize = &getSize,
        .isSame = &isSame,
    };

    vlc_diffutil_snake_callback_t snakeOp= {
        .remove = &buildSnakeResultRemove,
        .insert = &buildSnakeResultInsert,
        .equal = &buildSnakeResultEqual,
    };

    struct vlc_memstream output;
    vlc_memstream_open(&output);
    diffutil_snake_t* snake = vlc_diffutil_build_snake(&diffOp, oldInput, newInput);
    assert(snake);
    vlc_diffutil_walk_snake(snake, &snakeOp, &output, &diffOp, oldInput, newInput);
    vlc_diffutil_free_snake(snake);

    int ret = vlc_memstream_close(&output);
    assert(ret == 0);
    assert(strcmp(output.ptr, modList) == 0);
    free(output.ptr);
}

typedef struct {
    char op;
    uint32_t pos;
    const char* data;
} changelist_check_t;

#define CHECK_MOVE_PACK(from, to) (((to & 0xFFFF) << 16) + (from & 0xFFFF))
#define CHECK_MOVE_UNPACK_FROM(num) (num & 0xFFFF)
#define CHECK_MOVE_UNPACK_TO(num) ((num >> 16)  & 0xFFFF)

typedef struct VLC_VECTOR(char) string_vector_t;

static void check_changelist(const char* oldInput, const char* newInput, int flags, size_t expectedLen, const changelist_check_t* expectedResult)
{
    vlc_diffutil_callback_t diffOp = {
        .getOldSize = &getSize,
        .getNewSize = &getSize,
        .isSame = &isSame,
    };

    //rebuild the result from the changelist to verify that we end up with the right result
    string_vector_t strvec;
    vlc_vector_init(&strvec);
    vlc_vector_push_all(&strvec, oldInput, strlen(oldInput) + 1);

    diffutil_snake_t* snake = vlc_diffutil_build_snake(&diffOp, oldInput, newInput);
    vlc_diffutil_changelist_t* changelist = vlc_diffutil_build_change_list(snake, &diffOp, oldInput, newInput, flags);
    size_t changePos = 0;
    for (size_t i = 0; i < changelist->size; i++)
    {
        vlc_diffutil_change_t* op = &changelist->data[i];
        switch(op->type)
        {
        case VLC_DIFFUTIL_OP_INSERT:
            assert(changePos < expectedLen);
            assert(expectedResult[changePos].op == '+');
            assert(expectedResult[changePos].pos == op->op.insert.index);
            assert(strlen(expectedResult[changePos].data) == op->count);
            assert(strncmp(expectedResult[changePos].data, &newInput[op->op.insert.y], op->count) == 0);
            vlc_vector_insert_all(&strvec, op->op.insert.index, &newInput[op->op.insert.y], op->count);
            changePos++;
            break;
        case VLC_DIFFUTIL_OP_REMOVE:
            assert(changePos < expectedLen);
            assert(expectedResult[changePos].op == '-');
            assert(expectedResult[changePos].pos == op->op.remove.index);
            assert(strlen(expectedResult[changePos].data) == op->count);
            assert(strncmp(expectedResult[changePos].data, &oldInput[op->op.remove.x], op->count) == 0);
            vlc_vector_remove_slice(&strvec, op->op.remove.index, op->count);
            changePos++;
            break;
        case VLC_DIFFUTIL_OP_MOVE:
        {
            uint16_t fromPos = CHECK_MOVE_UNPACK_FROM(expectedResult[changePos].pos);
            uint16_t toPos = CHECK_MOVE_UNPACK_TO(expectedResult[changePos].pos);
            assert(changePos < expectedLen);
            assert(expectedResult[changePos].op == 'm');
            assert(fromPos == op->op.move.from);
            assert(toPos == op->op.move.to);
            assert(strlen(expectedResult[changePos].data) == op->count);
            assert(strncmp(expectedResult[changePos].data, &oldInput[op->op.move.x], op->count) == 0);

            //vlc_vector_move_slice represent move operation a bit differently from us
            if (op->op.move.from < op->op.move.to)
                vlc_vector_move_slice(&strvec, op->op.move.from, op->count, op->op.move.to - op->count);
            else
                vlc_vector_move_slice(&strvec, op->op.move.from, op->count, op->op.move.to);
            changePos++;
            break;
        }
        case VLC_DIFFUTIL_OP_IGNORE:
            break;
        }
    }
    assert(strcmp(newInput, strvec.data) == 0);
    vlc_vector_destroy(&strvec);

    vlc_diffutil_free_change_list(changelist);
    vlc_diffutil_free_snake(snake);
}

#define CHECK_CHANGELIST(from, to, flags, ...) \
    do { \
    const changelist_check_t res[] = { __VA_ARGS__ };\
        check_changelist(from, to, flags, ARRAY_SIZE(res), res); \
    } while (0)

#define CHECK_SIMPLE(from, to, ...) \
    CHECK_CHANGELIST(from, to, 0, __VA_ARGS__)
#define CHECK_AGGREG(from, to, ...) \
    CHECK_CHANGELIST(from, to, VLC_DIFFUTIL_RESULT_AGGREGATE, __VA_ARGS__)
#define CHECK_MOVE(from, to, ...) \
    CHECK_CHANGELIST(from, to, VLC_DIFFUTIL_RESULT_MOVE, __VA_ARGS__)
#define CHECK_MOVE_AG(from, to, ...) \
    CHECK_CHANGELIST(from, to, VLC_DIFFUTIL_RESULT_MOVE | VLC_DIFFUTIL_RESULT_AGGREGATE, __VA_ARGS__)


int main(void)
{
    check_snake("", "", "");
    check_snake("unchanged", "unchanged", "=u=n=c=h=a=n=g=e=d");
    check_snake("", "add only", "+a+d+d+ +o+n+l+y");
    check_snake("delete only", "", "-d-e-l-e-t-e- -o-n-l-y");

    check_snake("DDDD", "AAA", "-D-D-D-D+A+A+A");
    check_snake("____", "__AA__", "=_=_+A+A=_=_");
    check_snake("__DD__", "____", "=_=_-D-D=_=_");
    check_snake("__DD__", "__A__", "=_=_-D-D+A=_=_");
    check_snake("__DD____", "____A__", "=_=_-D-D=_=_+A=_=_");
    check_snake("____DD__", "__A____", "=_=_+A=_=_-D-D=_=_");
    check_snake("_MD___D_", "__A_A_M_", "=_-M-D=_+A=_+A=_-D+M=_");

    // check sequences without moves nor aggregation
    CHECK_SIMPLE("", "");
    CHECK_SIMPLE("unchanged", "unchanged");
    CHECK_SIMPLE("", "add only",
        {'+', 0, "a"}, {'+', 1, "d"}, {'+', 2, "d"}, {'+', 3, " "},
        {'+', 4, "o"}, {'+', 5, "n"}, {'+', 6, "l"}, {'+', 7, "y"});
    CHECK_SIMPLE("delete only", "",
        {'-', 0, "d"}, {'-', 0, "e"}, {'-', 0, "l"}, {'-', 0, "e"}, {'-', 0, "t"}, {'-', 0, "e"},
        {'-', 0, " "}, {'-', 0, "o"}, {'-', 0, "n"}, {'-', 0, "l"}, {'-', 0, "y"});

    CHECK_SIMPLE("____", "__AA__", {'+', 2, "A"}, {'+', 3, "A"});
    CHECK_SIMPLE("____", "__A_A_", {'+', 2, "A"}, {'+', 4, "A"});
    CHECK_SIMPLE("__", "A__A", {'+', 0, "A"}, {'+', 3, "A"});
    CHECK_SIMPLE("____", "AA____AA", {'+', 0, "A"}, {'+', 1, "A"}, {'+', 6, "A"}, {'+', 7, "A"});
    CHECK_SIMPLE("__DD__", "____", {'-', 2, "D"}, {'-', 2, "D"});
    CHECK_SIMPLE("__D_D_", "____", {'-', 2, "D"}, {'-', 3, "D"});
    CHECK_SIMPLE("D__D", "__", {'-', 0, "D"}, {'-', 2, "D"});
    CHECK_SIMPLE("__DD__", "__A__", {'-', 2, "D"}, {'-', 2, "D"}, {'+', 2, "A"});
    CHECK_SIMPLE("__DD____", "____A__", {'-', 2, "D"}, {'-', 2, "D"}, {'+', 4, "A"});
    CHECK_SIMPLE("____DD__", "__A____", {'+', 2, "A"}, {'-', 5, "D"}, {'-', 5, "D"});

    // check insert&delete only with aggregation
    CHECK_AGGREG("", "");
    CHECK_AGGREG("unchanged", "unchanged");
    CHECK_AGGREG("", "add only", {'+', 0, "add only"});
    CHECK_AGGREG("delete only", "", {'-', 0, "delete only"});

    CHECK_AGGREG("DDDD", "AAA", {'-', 0, "DDDD"}, {'+', 0, "AAA"});
    CHECK_AGGREG("____", "__AA__", {'+', 2, "AA"});
    CHECK_AGGREG("____", "__A_A_", {'+', 2, "A"}, {'+', 4, "A"});
    CHECK_AGGREG("____", "AA____AA", {'+', 0, "AA"}, {'+', 6, "AA"});
    CHECK_AGGREG("____", "AA__AA__AA", {'+', 0, "AA"}, {'+', 4, "AA"}, {'+', 8, "AA"});
    CHECK_AGGREG("__DD__", "____", {'-', 2, "DD"});
    CHECK_AGGREG("__D_D_", "____", {'-', 2, "D"}, {'-', 3, "D"});
    CHECK_AGGREG("DD____DD", "____", {'-', 0, "DD"}, {'-', 4, "DD"});
    CHECK_AGGREG("DD__DD__DD", "____", {'-', 0, "DD"}, {'-', 2, "DD"}, {'-', 4, "DD"});
    CHECK_AGGREG("__DD__", "__A__", {'-', 2, "DD"}, {'+', 2, "A"});
    CHECK_AGGREG("__DD____", "____A__", {'-', 2, "DD"}, {'+', 4, "A"});
    CHECK_AGGREG("____DD__", "__A____", {'+', 2, "A"}, {'-', 5, "DD"});
    CHECK_AGGREG("__DD__", "AA____AA", {'+', 0, "AA"}, {'-', 4, "DD"}, {'+', 6, "AA"});
    CHECK_AGGREG("DD____DD", "__AA__", {'-', 0, "DD"}, {'+', 2, "AA"}, {'-', 6, "DD"});


    //move forward
    CHECK_MOVE("_M____", "____M_", {'m', CHECK_MOVE_PACK(1, 5), "M"});
    CHECK_MOVE("_DM____", "____M_", {'-', 1, "D"}, {'m', CHECK_MOVE_PACK(1, 5), "M"});
    CHECK_MOVE("_MD____", "____M_", {'m', CHECK_MOVE_PACK(1, 6), "M"}, {'-', 1, "D"});
    CHECK_MOVE("_mM____", "____mM_", {'m', CHECK_MOVE_PACK(1, 6), "m"}, {'m', CHECK_MOVE_PACK(1, 6), "M"});
    CHECK_MOVE("_M__D_D__", "_____M_", {'m', CHECK_MOVE_PACK(1, 8), "M"}, {'-', 3, "D"}, {'-', 4, "D"});
    CHECK_MOVE("_M__DD___", "_____M_", {'m', CHECK_MOVE_PACK(1, 8), "M"}, {'-', 3, "D"}, {'-', 3, "D"});
    CHECK_MOVE("_M____", "__A_A_M_", {'m', CHECK_MOVE_PACK(1, 5), "M"}, {'+', 2, "A"}, {'+', 4, "A"});
    CHECK_MOVE("_MD___D_", "__A_A_M_",
        {'m', CHECK_MOVE_PACK(1, 7), "M"}, {'-', 1, "D"}, {'+', 2, "A"}, {'+', 4, "A"}, {'-', 6, "D"});

    //move backward
    CHECK_MOVE("____M_", "_M____", {'m', CHECK_MOVE_PACK(4, 1), "M"});
    CHECK_MOVE("____DM_", "_M____",  {'m', CHECK_MOVE_PACK(5, 1), "M"}, {'-', 5, "D"});
    CHECK_MOVE("____MD_", "_M____", {'m', CHECK_MOVE_PACK(4, 1), "M"}, {'-', 5, "D"});
    CHECK_MOVE("____mM_", "_mM____", {'m', CHECK_MOVE_PACK(4, 1), "m"}, {'m', CHECK_MOVE_PACK(5, 2), "M"});
    CHECK_MOVE("___D_M_", "_M_A___", {'m', CHECK_MOVE_PACK(5, 1), "M"}, {'+', 3, "A"}, {'-', 5, "D"});


    //swap
    CHECK_MOVE("_M___m_", "_m___M_", {'m', CHECK_MOVE_PACK(1, 6), "M"}, {'m', CHECK_MOVE_PACK(4, 1), "m"});
    CHECK_MOVE("_M_n__m_N_", "_m_N__M_n_",
        {'m', CHECK_MOVE_PACK(1, 7), "M"}, {'m', CHECK_MOVE_PACK(5, 1), "m"},
        {'m', CHECK_MOVE_PACK(3, 9), "n"}, {'m', CHECK_MOVE_PACK(7, 3), "N"});

    CHECK_MOVE_AG("", "");
    CHECK_MOVE_AG("unchanged", "unchanged");
    CHECK_MOVE_AG("", "add only", {'+', 0, "add only"});
    CHECK_MOVE_AG("delete only", "", {'-', 0, "delete only"});

    //Move with aggregate
    CHECK_MOVE_AG("DDDD", "AAA", {'-', 0, "DDDD"}, {'+', 0, "AAA"});
    CHECK_MOVE_AG("____", "__AA__", {'+', 2, "AA"});
    CHECK_MOVE_AG("____", "__A_A_", {'+', 2, "A"}, {'+', 4, "A"});
    CHECK_MOVE_AG("__DD__", "____", {'-', 2, "DD"});
    CHECK_MOVE_AG("__D_D_", "____", {'-', 2, "D"}, {'-', 3, "D"});
    CHECK_MOVE_AG("__DD__", "__A__", {'-', 2, "DD"}, {'+', 2, "A"});
    CHECK_MOVE_AG("__DD____", "____A__", {'-', 2, "DD"}, {'+', 4, "A"});
    CHECK_MOVE_AG("____DD__", "__A____", {'+', 2, "A"}, {'-', 5, "DD"});
    CHECK_MOVE_AG("____", "AA____AA", {'+', 0, "AA"}, {'+', 6, "AA"});
    CHECK_MOVE_AG("____", "AA__AA__AA", {'+', 0, "AA"}, {'+', 4, "AA"}, {'+', 8, "AA"});
    CHECK_MOVE_AG("DD____DD", "____", {'-', 0, "DD"}, {'-', 4, "DD"});
    CHECK_MOVE_AG("DD__DD__DD", "____", {'-', 0, "DD"}, {'-', 2, "DD"}, {'-', 4, "DD"});
    CHECK_MOVE_AG("__DD__", "AA____AA", {'+', 0, "AA"}, {'-', 4, "DD"}, {'+', 6, "AA"});
    CHECK_MOVE_AG("DD____DD", "__AA__", {'-', 0, "DD"}, {'+', 2, "AA"}, {'-', 6, "DD"});

    CHECK_MOVE_AG("_123______", "______123_", {'m', CHECK_MOVE_PACK(1, 9), "123"});
    CHECK_MOVE_AG("______123_", "_123______", {'m', CHECK_MOVE_PACK(6, 1), "123"});
    CHECK_MOVE_AG("_123_____456_", "_456_____123_", {'m', CHECK_MOVE_PACK(1, 12), "123"}, {'m', CHECK_MOVE_PACK(6, 1), "456"});

    CHECK_MOVE_AG("_123__DD____", "_____AA_123_", {'m', CHECK_MOVE_PACK(1, 11), "123"}, {'-', 3, "DD"}, {'+', 5, "AA"} );
    CHECK_MOVE_AG("___DD___123_", "_123____AA__", {'m', CHECK_MOVE_PACK(8, 1), "123"}, {'-', 6, "DD"}, {'+', 8, "AA"});

    CHECK_MOVE_AG("_123___", "___312_", {'m', CHECK_MOVE_PACK(1, 6), "12"}, {'m', CHECK_MOVE_PACK(1, 4), "3"});
    CHECK_MOVE_AG("______123_", "_312______", {'m', CHECK_MOVE_PACK(8, 1), "3"}, {'m', CHECK_MOVE_PACK(7, 2), "12"});

    return 0;
}
