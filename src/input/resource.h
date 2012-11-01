/*****************************************************************************
 * resource.h
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
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

/**
 * This function set the associated input.
 */
void input_resource_SetInput( input_resource_t *, input_thread_t * );

/**
 * This function handles sout request.
 */
sout_instance_t *input_resource_RequestSout( input_resource_t *, sout_instance_t *, const char *psz_sout );

/**
 * This function creates or recycles an audio output.
 */
audio_output_t *input_resource_GetAout( input_resource_t * );

/**
 * This function retains or destroys an audio output.
 */
void input_resource_PutAout( input_resource_t *, audio_output_t * );

/**
 * This function handles vout request.
 */
vout_thread_t *input_resource_RequestVout( input_resource_t *, vout_thread_t *, video_format_t *, unsigned dpb_size, bool b_recycle );

/**
 * This function returns one of the current vout if any.
 *
 * You must call vlc_object_release on the value returned (if non NULL).
 */
vout_thread_t *input_resource_HoldVout( input_resource_t * );

/**
 * This function returns all current vouts if any.
 *
 * You must call vlc_object_release on all values returned (if non NULL).
 */
void input_resource_HoldVouts( input_resource_t *, vout_thread_t ***, size_t * );

/**
 * This function releases all resources (object).
 */
void input_resource_Terminate( input_resource_t * );

/**
 * This function holds the input_resource_t itself
 */
input_resource_t *input_resource_Hold( input_resource_t * );

#endif
