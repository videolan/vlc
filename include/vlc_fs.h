/*****************************************************************************
 * vlc_fs.h: File system helpers
 *****************************************************************************
 * Copyright © 2006-2010 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_FS_H
#define VLC_FS_H 1

/**
 * \file
 * Those functions convert file paths from UTF-8 to the system-specific
 * encoding (especially UTF-16 on Windows). Also, they always mark file
 * descriptor with the close-on-exec flag.
 */

#include <sys/types.h>
#include <dirent.h>

VLC_EXPORT( int, utf8_open, ( const char *filename, int flags, ... ) LIBVLC_USED );
VLC_EXPORT( FILE *, utf8_fopen, ( const char *filename, const char *mode ) LIBVLC_USED );

VLC_EXPORT( DIR *, utf8_opendir, ( const char *dirname ) LIBVLC_USED );
VLC_EXPORT( char *, utf8_readdir, ( DIR *dir ) LIBVLC_USED );
VLC_EXPORT( int, utf8_loaddir, ( DIR *dir, char ***namelist, int (*select)( const char * ), int (*compar)( const char **, const char ** ) ) );
VLC_EXPORT( int, utf8_scandir, ( const char *dirname, char ***namelist, int (*select)( const char * ), int (*compar)( const char **, const char ** ) ) );
VLC_EXPORT( int, utf8_mkdir, ( const char *filename, mode_t mode ) );

VLC_EXPORT( int, utf8_unlink, ( const char *filename ) );
/* Not exported */
int utf8_rename( const char *, const char * );

#if defined( WIN32 ) && !defined( UNDER_CE )
# define stat _stati64
#endif

VLC_EXPORT( int, utf8_stat, ( const char *filename, struct stat *buf ) );
VLC_EXPORT( int, utf8_lstat, ( const char *filename, struct stat *buf ) );

VLC_EXPORT( int, utf8_mkstemp, ( char * ) );

#endif
