/*****************************************************************************
 * vlc_block.h: Data blocks management functions
 *****************************************************************************
 * Copyright (C) 2003 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC_BLOCK_H
#define VLC_BLOCK_H 1


#include <vlc_frame.h>

#define BLOCK_FLAG_DISCONTINUITY VLC_FRAME_FLAG_DISCONTINUITY

#define BLOCK_FLAG_TYPE_I VLC_FRAME_FLAG_TYPE_I
#define BLOCK_FLAG_TYPE_P VLC_FRAME_FLAG_TYPE_P
#define BLOCK_FLAG_TYPE_B VLC_FRAME_FLAG_TYPE_B
#define BLOCK_FLAG_TYPE_PB VLC_FRAME_FLAG_TYPE_PB
#define BLOCK_FLAG_HEADER VLC_FRAME_FLAG_HEADER
#define BLOCK_FLAG_END_OF_SEQUENCE VLC_FRAME_FLAG_END_OF_SEQUENCE
#define BLOCK_FLAG_CLOCK VLC_FRAME_FLAG_CLOCK
#define BLOCK_FLAG_SCRAMBLED VLC_FRAME_FLAG_SCRAMBLED
#define BLOCK_FLAG_PREROLL VLC_FRAME_FLAG_PREROLL
#define BLOCK_FLAG_CORRUPTED VLC_FRAME_FLAG_CORRUPTED
#define BLOCK_FLAG_AU_END VLC_FRAME_FLAG_AU_END
#define BLOCK_FLAG_TOP_FIELD_FIRST VLC_FRAME_FLAG_TOP_FIELD_FIRST
#define BLOCK_FLAG_BOTTOM_FIELD_FIRST VLC_FRAME_FLAG_BOTTOM_FIELD_FIRST
#define BLOCK_FLAG_SINGLE_FIELD VLC_FRAME_FLAG_SINGLE_FIELD
#define BLOCK_FLAG_INTERLACED_MASK VLC_FRAME_FLAG_INTERLACED_MASK
#define BLOCK_FLAG_TYPE_MASK VLC_FRAME_FLAG_TYPE_MASK
#define BLOCK_FLAG_CORE_PRIVATE_MASK VLC_FRAME_FLAG_CORE_PRIVATE_MASK
#define BLOCK_FLAG_CORE_PRIVATE_SHIFT VLC_FRAME_FLAG_CORE_PRIVATE_SHIFT
#define BLOCK_FLAG_PRIVATE_MASK VLC_FRAME_FLAG_PRIVATE_MASK
#define BLOCK_FLAG_PRIVATE_SHIFT VLC_FRAME_FLAG_PRIVATE_SHIFT

#define vlc_block_callbacks vlc_frame_callbacks

#define block_Init vlc_frame_Init
#define block_Alloc vlc_frame_Alloc
#define block_TryRealloc vlc_frame_TryRealloc
#define block_Realloc vlc_frame_Realloc
#define block_Release vlc_frame_Release
#define block_CopyProperties vlc_frame_CopyProperties
#define block_Duplicate vlc_frame_Duplicate
#define block_heap_Alloc vlc_frame_heap_Alloc
#define block_mmap_Alloc vlc_frame_mmap_Alloc
#define block_shm_Alloc vlc_frame_shm_Alloc
#define block_File vlc_frame_File
#define block_FilePath vlc_frame_FilePath
#define block_Cleanup vlc_frame_Cleanup
#define block_cleanup_push vlc_frame_cleanup_push
#define block_ChainAppend vlc_frame_ChainAppend
#define block_ChainLastAppend vlc_frame_ChainLastAppend
#define block_ChainRelease vlc_frame_ChainRelease
#define block_ChainExtract vlc_frame_ChainExtract
#define block_ChainProperties vlc_frame_ChainProperties
#define block_ChainGather vlc_frame_ChainGather

#endif /* VLC_BLOCK_H */
