/*****************************************************************************
 * vlc_fs.h: File system helpers
 *****************************************************************************
 * Copyright © 2006-2010 Rémi Denis-Courmont
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

#ifndef VLC_FS_H
#define VLC_FS_H 1

#include <sys/types.h>

struct stat;
struct iovec;

#ifdef _WIN32
# include <io.h>
# include <sys/stat.h>
# ifndef stat
#  define stat _stati64
# endif
# ifndef fstat
#  define fstat _fstati64
# endif
# undef lseek
# define lseek _lseeki64
#else // !_WIN32
#include <dirent.h>
#endif

#ifdef __ANDROID__
# define lseek lseek64
#endif


/**
 * \defgroup os Operating system
 * \ingroup vlc
 * \defgroup file File system
 * \ingroup os
 * @{
 *
 * \file
 * The functions in this file help with using low-level Unix-style file
 * descriptors, BSD sockets and directories. In general, they retain the
 * prototype and most semantics from their respective standard equivalents.
 * However, there are a few differences:
 *  - On Windows, file path arguments are expected in UTF-8 format.
 *    They are converted to UTF-16 internally, thus enabling access to paths
 *    outside of the local Windows ANSI code page.
 *  - On POSIX systems, file descriptors are created with the close-on-exec
 *    flag set (atomically where possible), so that they do not leak to
 *    child process after fork-and-exec.
 *  - vlc_scandir(), inspired by GNU scandir(), passes file names rather than
 *    dirent structure pointers to its callbacks.
 *  - vlc_accept() takes an extra boolean for nonblocking mode (compare with
 *    the flags parameter in POSIX.next accept4()).
 *  - Writing functions do not emit a SIGPIPE signal in case of broken pipe.
 *
 * \defgroup fd File descriptors
 * @{
 */

/**
 * Opens a system file handle.
 *
 * @param filename file path to open (with UTF-8 encoding)
 * @param flags open() flags, see the C library open() documentation
 * @return a file handle on success, -1 on error (see errno).
 * @note Contrary to standard open(), this function returns a file handle
 * with the close-on-exec flag preset.
 */
VLC_API int vlc_open(const char *filename, int flags, ...) VLC_USED;

/**
 * Opens a system file handle relative to an existing directory handle.
 *
 * @param dir directory file descriptor
 * @param filename file path to open (with UTF-8 encoding)
 * @param flags open() flags, see the C library open() documentation
 * @return a file handle on success, -1 on error (see errno).
 * @note Contrary to standard open(), this function returns a file handle
 * with the close-on-exec flag preset.
 */
VLC_API int vlc_openat(int fd, const char *filename, int flags, ...) VLC_USED;

VLC_API int vlc_mkstemp( char * );

/**
 * Duplicates a file descriptor.
 *
 * @param oldfd file descriptor to duplicate
 *
 * @note Contrary to standard dup(), the new file descriptor has the
 * close-on-exec descriptor flag preset.
 * @return a new file descriptor, -1 (see @c errno)
 */
VLC_API int vlc_dup(int oldfd) VLC_USED;

/**
 * Replaces a file descriptor.
 *
 * This function duplicates a file descriptor to a specified file descriptor.
 * This is primarily used to atomically replace a described file.
 *
 * @param oldfd source file descriptor to copy
 * @param newfd destination file descriptor to replace
 *
 * @note Contrary to standard dup2(), the new file descriptor has the
 * close-on-exec descriptor flag preset.
 *
 * @retval newfd success
 * @retval -1 failure (see @c errno)
 */
VLC_API int vlc_dup2(int oldfd, int newfd);

/**
 * Creates a pipe (see "man pipe" for further reference). The new file
 * descriptors have the close-on-exec flag preset.
 * @return 0 on success, -1 on error (see errno)
 */
VLC_API int vlc_pipe(int [2]) VLC_USED;

/**
 * Creates an anonymous regular file descriptor, i.e. a descriptor for a
 * temporary file.
 *
 * The file is initially empty. The storage space is automatically reclaimed
 * when all file descriptors referencing it are closed.
 *
 * The new file descriptor has the close-on-exec flag preset.
 *
 * @return a file descriptor on success, -1 on error (see errno)
 */
VLC_API int vlc_memfd(void) VLC_USED;

/**
 * Writes data to a file descriptor. Unlike write(), if EPIPE error occurs,
 * this function does not generate a SIGPIPE signal.
 * @note If the file descriptor is known to be neither a pipe/FIFO nor a
 * connection-oriented socket, the normal write() should be used.
 */
VLC_API ssize_t vlc_write(int, const void *, size_t);

/**
 * Writes data from an iovec structure to a file descriptor. Unlike writev(),
 * if EPIPE error occurs, this function does not generate a SIGPIPE signal.
 */
VLC_API ssize_t vlc_writev(int, const struct iovec *, int);

/**
 * Closes a file descriptor.
 *
 * This closes a file descriptor. If this is a last file descriptor for the
 * underlying open file, the file is closed too; the exact semantics depend
 * on the type of file.
 *
 * @note The file descriptor is always closed when the function returns, even
 * if the function returns an error. The sole exception is if the file
 * descriptor was not currently valid, and thus cannot be closed (errno will
 * then be set to EBADF).
 *
 * @param fd file descriptor
 * @return Normally, zero is returned.
 * If an I/O error is detected before or while closing, the function may return
 * -1. Such an error is unrecoverable since the descriptor is closed.
 *
 * A nul return value does not necessarily imply that all pending I/O
 * succeeded, since I/O might still occur asynchronously afterwards.
 */
VLC_API int vlc_close(int fd);

/**
 * @}
 */

/**
 * Finds file/inode information - like stat().
 * @note As far as possible, fstat() should be used instead.
 *
 * @param filename UTF-8 file path
 */
VLC_API int vlc_stat(const char *filename, struct stat *) VLC_USED;

/**
 * Finds file/inode information, as lstat().
 * Consider using fstat() instead, if possible.
 *
 * @param filename UTF-8 file path
 */
VLC_API int vlc_lstat(const char *filename, struct stat *) VLC_USED;

/**
 * Removes a file.
 *
 * @param filename a UTF-8 string with the name of the file you want to delete.
 * @return A 0 return value indicates success. A -1 return value indicates an
 *        error, and an error code is stored in errno
 */
VLC_API int vlc_unlink(const char *filename);

/**
 * Moves a file atomically. This only works within a single file system.
 *
 * @param oldpath path to the file before the move
 * @param newpath intended path to the file after the move
 * @return A 0 return value indicates success. A -1 return value indicates an
 *        error, and an error code is stored in errno
 */
VLC_API int vlc_rename(const char *oldpath, const char *newpath);

VLC_API FILE * vlc_fopen( const char *filename, const char *mode ) VLC_USED;

/**
 * \defgroup dir Directories
 * @{
 */

#if defined( _WIN32 )
typedef struct vlc_DIR vlc_DIR;
#else // !_WIN32
typedef DIR vlc_DIR;
#endif

/**
 * Opens a DIR pointer.
 *
 * @param dirname UTF-8 representation of the directory name
 * @return a pointer to the DIR struct, or NULL in case of error.
 * Release with vlc_closedir().
 */
VLC_API vlc_DIR *vlc_opendir(const char *dirname) VLC_USED;

/**
 * Reads the next file name from an open directory.
 *
 * @param dir directory handle as returned by vlc_opendir()
 *            (must not be used by another thread concurrently)
 *
 * @return a UTF-8 string of the directory entry. The string is valid until
 * the next call to vlc_readdir() or vlc_closedir() on the handle.
 * If there are no more entries in the directory, NULL is returned.
 * If an error occurs, errno is set and NULL is returned.
 */
VLC_API const char *vlc_readdir(vlc_DIR *dir) VLC_USED;

VLC_API int vlc_loaddir( vlc_DIR *dir, char ***namelist, int (*select)( const char * ), int (*compar)( const char **, const char ** ) );
VLC_API int vlc_scandir( const char *dirname, char ***namelist, int (*select)( const char * ), int (*compar)( const char **, const char ** ) );

VLC_API void vlc_closedir( vlc_DIR *dir );
VLC_API void vlc_rewinddir( vlc_DIR *dir );

/**
 * Creates a directory.
 *
 * @param dirname a UTF-8 string with the name of the directory that you
 *        want to create.
 * @param mode directory permissions
 * @return 0 on success, -1 on error (see errno).
 */
VLC_API int vlc_mkdir(const char *dirname, mode_t mode);

/**
 * Determines the current working directory.
 *
 * @return the current working directory (must be free()'d)
 *         or NULL on error
 */
VLC_API char *vlc_getcwd(void) VLC_USED;

/** @} */

#ifdef __ANDROID__
# define lseek lseek64
#endif

/** @} */
#endif
