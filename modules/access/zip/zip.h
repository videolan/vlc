/*****************************************************************************
 * zip.h: Module (access+filter) to extract different archives, based on zlib
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jean-Philippe André <jpeg@videolan.org>
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

/** **************************************************************************
 * Common includes and shared headers
 *****************************************************************************/

#ifndef ZIP_ACCESSDEMUX_H
#define ZIP_ACCESSDEMUX_H

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_arrays.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include "unzip.h"
#include "ioapi.h"

#include <assert.h>

#define ZIP_FILENAME_LEN 512
#define ZIP_BUFFER_LEN 32768
#define ZIP_SEP      "!/"
#define ZIP_SEP_LEN  2


/** **************************************************************************
 * Module access points: stream_filter
 *****************************************************************************/
int StreamOpen( vlc_object_t* );
void StreamClose( vlc_object_t* );

/** **************************************************************************
 * Module access points: access
 *****************************************************************************/
int AccessOpen( vlc_object_t *p_this );
void AccessClose( vlc_object_t *p_this );

/** Common function */
bool isAllowedChar( char c );

/** **************************************************************************
 * zipIO function headers : how to use vlc_stream to read the zip
 * Note: static because the implementations differ
 *****************************************************************************/
static void* ZCALLBACK ZipIO_Open( void* opaque, const char* filename, int m );
static uLong ZCALLBACK ZipIO_Read( void*, void* stream, void* buf, uLong sz );
static uLong ZCALLBACK ZipIO_Write( void*, void* stream, const void*, uLong );
static long ZCALLBACK ZipIO_Tell( void*, void* stream );
static long ZCALLBACK ZipIO_Seek( void*, void* stream, uLong offset, int ori );
static int ZCALLBACK ZipIO_Close( void*, void* stream );
static int ZCALLBACK ZipIO_Error( void*, void* stream );

#endif /* ZIP_ACCESSDEMUX_H */
