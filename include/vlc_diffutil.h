/******************************************************************************
 * vlc_diffutil.h
 ******************************************************************************
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
#ifndef VLC_DIFFUTIL_H
#define VLC_DIFFUTIL_H

#include <vlc_common.h>
#include <vlc_vector.h>

/**
 * this struture defines callback to access and compare elements from
 * the old and the new list
 */
typedef struct {
    /// return the size of the old list @a list
    uint32_t (*getOldSize)(const void* list);
    /// return the size of the new list @a list
    uint32_t (*getNewSize)(const void* list);
    /// compare 2 elements
    bool (*isSame)(const void* listOld, uint32_t oldIndex, const void* listNew,  uint32_t newIndex);
} vlc_diffutil_callback_t;

typedef struct {
    /**
     * notify that the item from @a listNew at position @a posNew is inserted in list @a listOld at position @a posOld
     *
     * @param opaque user data from function vlc_diffutil_walk_snake
     * @param listOld pointer to the old model
     * @param posOld position of the element inserted in the old model (before removal)
     * @param listNew pointer to the new model
     * @param posNew position of the element inserted in the new model
     */
    void (*insert)(void* opaque, const void* listOld, uint32_t posOld, const void* listNew,  uint32_t posNew);
    /**
     * notify that the item from @a listOld at position @a posOld is removed
     * @param opaque user data from function vlc_diffutil_walk_snake
     * @param listOld pointer to the old model
     * @param posOld position of the element removed in the old model
     * @param listNew pointer to the new model
     * @param posNew position of the element removed in the new model (before removal)
     */
    void (*remove)(void* opaque, const void* listOld, uint32_t posOld, const void* listNew,  uint32_t posNew);
    /**
     * notify that the item as @a posOld from the old list @a listOld is unchanged, the respective item
     * position in the new list is at the positoin @a posNew in @a listNew
     */
    void (*equal)(void* opaque, const void* listOld, uint32_t posOld, const void* listNew, uint32_t posNew);
} vlc_diffutil_snake_callback_t;

typedef struct diffutil_snake_t diffutil_snake_t;

enum vlc_diffutil_op_type {
    ///items have been added to the list
    VLC_DIFFUTIL_OP_INSERT,
    ///items have been removed from the list
    VLC_DIFFUTIL_OP_REMOVE,
    ///items have been moved within the list
    VLC_DIFFUTIL_OP_MOVE,
    ///current change should be ignored
    VLC_DIFFUTIL_OP_IGNORE,
};


/**
 * represent a change to the model, each change assumes that previous changes
 * have already been applied
 *
 * for instance with a model "aBcDef", the operations [remove(index=1, count=1), remove(index=2, count=1)]
 * will result in "acef" (with "acDef" as intermediary step)
 */
typedef struct {
    union {
        /**
         * the data positionned at newModel[ y ] is inserted at position index in the current model
         *
         * @example
         * model = "abcdefg"
         * newModel[3] = 'X'
         * after operation insert(y=3, index = 3), model will be
         * model = "abcXdefg"
         */
        struct {
            /// data position in the old model
            uint32_t x;
            /// data position in the new model
            uint32_t y;
            /// insertion position in the updated model
            uint32_t index;
        } insert;

        /**
         * the data positionned at oldModel[ y ] is removed at position index in the current model
         *
         * @example
         * model = "abCdefg"
         * oldModel[4] = 'C'
         * after operation remove(x=4, index = 2), model will be
         * model = "abdefg"
         */
        struct {
            /// data position in the old model
            uint32_t x;
            /// data position in the new model
            uint32_t y;
            /// removal position in the updated model
            uint32_t index;
        } remove;

        /**
         * moves the data from position model[ from ] to model[ to ]
         * the data is available either at newModel[ y ] or oldModel[ x ]
         *
         * the positions @a from and @a to are given in the referenrial before the operation
         *
         * @example
         * model = "aBCdefg"
         * after operation move(from=1, to=5, count=2), model will be
         * model = "adeCBfg"
         */
        struct {
            /// move origin
            uint32_t from;
            /// move destination
            uint32_t to;
            /// data position in the old model
            uint32_t x;
            /// data position in the new model
            uint32_t y;
        } move;

    } op;

    /// type of change operation
    enum vlc_diffutil_op_type type;

    /// number of elements to be inserted/removed/moved
    uint32_t count;
} vlc_diffutil_change_t;

typedef struct VLC_VECTOR(vlc_diffutil_change_t) vlc_diffutil_changelist_t;

enum vlc_diffutil_result_flag {
    /// try to transform an insertion with a matching supression into a move operation
    VLC_DIFFUTIL_RESULT_MOVE = 0x1,
    /**
     * aggreate similar consecutive operations into a single operation
     * for instance this:
     *  [{INSERT, i=5}{INSERT, x=6}{REMOVE, i=10}{REMOVE, i=10}{REMOVE, i=10}]
     * would be tranformed into:
     *  [{INSERT, i=5, count=2}{REMOVE, i=10, count=3}]
     */
    VLC_DIFFUTIL_RESULT_AGGREGATE = 0x2,
};

/**
 * vlc_diffutil_build_snake compute a diff model
 * between the @a dataOld model and the @a dataNew model. This model can be
 * processed manually using vlc_diffutil_walk_snake or translated into a change list using
 * vlc_diffutil_build_change_list
 *
 * @param diffOp callback to compare the elements from the old and new model
 * @param dataOld old model
 * @param dataNew new model
 * @return the diff model, NULL on error
 */
VLC_API struct diffutil_snake_t* vlc_diffutil_build_snake(const vlc_diffutil_callback_t* diffOp, const void* dataOld, const void* dataNew);

/// free the snake created by vlc_diffutil_build_snake
VLC_API void vlc_diffutil_free_snake(struct diffutil_snake_t* snake);

/**
 * iterate over the changelist and callback user on each operation (keep/insert/remove)
 *
 * @param snake the snake created with vlc_diffutil_build_snake
 * @param snakeOp snake callback
 * @param cbData user data for snake callbacks
 * @param diffOp callbacks used in vlc_diffutil_build_snake
 * @param dataOld old model
 * @param dataNew new model
 * @return false on error
 *
 * @warning @a dataOld and @a dataNew should not be altered during the operation
 */
VLC_API bool vlc_diffutil_walk_snake(
    const diffutil_snake_t* snake,
    const vlc_diffutil_snake_callback_t* snakeOp, void* cbData,
    const vlc_diffutil_callback_t* diffOp, const void* dataOld, const void* dataNew);

/**
 * vlc_diffutil_build_change_list creates a list of changes to apply to transform @a dataOld into @a dataNew
 *
 * @param snake the snake created with vlc_diffutil_build_snake
 * @param diffOp callbacks used in vlc_diffutil_build_snake
 * @param dataOld old model
 * @param dataNew new model
 * @param flags vlc_diffutil_result_flag flags
 * @return the list of changes, NULL on error
 */
VLC_API vlc_diffutil_changelist_t* vlc_diffutil_build_change_list(
    const struct diffutil_snake_t* snake,
    const vlc_diffutil_callback_t* diffOp, const void* dataOld, const void* dataNew,
    int flags);

/// free the changelist created by vlc_diffutil_build_change_list
VLC_API void vlc_diffutil_free_change_list(vlc_diffutil_changelist_t* changelist);


#endif // VLC_DIFFUTIL_H
