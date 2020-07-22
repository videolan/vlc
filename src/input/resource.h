/*****************************************************************************
 * resource.h
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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

#ifndef LIBVLC_INPUT_RESOURCE_H
#define LIBVLC_INPUT_RESOURCE_H 1

#include <vlc_common.h>
#include <vlc_mouse.h>
#include "../video_output/vout_internal.h"

/**
 * This function set the associated input.
 */
void input_resource_SetInput( input_resource_t *, input_thread_t * );

/**
 * This function handles sout request.
 */
sout_instance_t *input_resource_RequestSout( input_resource_t *, sout_instance_t *, const char *psz_sout );

vout_thread_t *input_resource_RequestVout(input_resource_t *, vlc_video_context *,
                                         const vout_configuration_t *,
                                         enum vlc_vout_order *order,
                                         bool *has_started);
void input_resource_PutVout(input_resource_t *, vout_thread_t *, bool *has_stopped);

/**
 * This function returns one of the current vout if any.
 *
 * You must call vout_Release() on the value returned (if non NULL).
 */
vout_thread_t *input_resource_HoldVout( input_resource_t * );

/**
 * This function returns the dummy vout. It will be the parent of the future
 * main vout and can be used to pre-configure it. */
vout_thread_t *input_resource_HoldDummyVout( input_resource_t * );

/**
 * This function returns all current vouts if any.
 *
 * You must call vout_Release() on all values returned (if non NULL).
 */
void input_resource_HoldVouts( input_resource_t *, vout_thread_t ***, size_t * );

void input_resource_StopFreeVout( input_resource_t * );

/**
 * This function holds the input_resource_t itself
 */
input_resource_t *input_resource_Hold( input_resource_t * );

void input_resource_ResetAout( input_resource_t * );

#endif
