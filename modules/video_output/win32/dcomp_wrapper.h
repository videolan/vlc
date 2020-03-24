/*****************************************************************************
 * Copyright (c) 2020 VideoLAN
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
#ifndef VLC_DCOMP_WRAPPER_H_
#define VLC_DCOMP_WRAPPER_H_

#include <windows.h>
#include <unknwn.h>

#ifdef __cplusplus
extern "C" {
#endif

HRESULT IDCompositionVisual_SetContent(void* visual, IUnknown *content);
HRESULT IDCompositionDevice_Commit(void* device);

#ifdef __cplusplus
}
#endif

#endif
