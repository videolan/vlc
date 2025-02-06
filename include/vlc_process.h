// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * vlc_process.h: vlc_process functions
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifndef VLC_PROCESS_H
#define VLC_PROCESS_H

#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_vector.h>
#include <vlc_tick.h>
#include <vlc_threads.h>
#include <vlc_stream.h>

/**
 * @ingroup misc
 * @file
 * VLC_PROCESS API
 * @defgroup process Process API
 * @{
 */

/**
 * Spawn a new process with input and output redirection.
 *
 * Creates and starts a new vlc_process for the specified executable path with
 * the given arguments. Sets up pipes to allow reading from the process's
 * standard output and writing to its standard input.
 *
 * @param [in]  path    Path to the executable to run. Must not be NULL.
 * @param [in]  argc    Number of arguments passed to the process (must be
 *                      greater than 0).
 * @param [in]  argv    Array of argument strings (argv[0] must not be NULL).
 *
 * @return      A pointer to the newly created vlc_process structure on
 *              success, or NULL on failure.
 */
VLC_API struct vlc_process *
vlc_process_Spawn(const char *path, int argc, const char *const *argv);

/**
 * Stop a vlc_process and wait for its termination.
 *
 * Closes its file descriptors, and waits for it to exit. Optionally sends a
 * termination signal to the process,
 *
 * @param [in]  process        Pointer to the vlc_process instance. Must not
 *                             be NULL.
 * @param [in]  kill_process   Whether to forcibly terminate the process
 *                             before waiting.
 *
 * @return      The exit status of the process, or -1 on error.
 */
VLC_API int
vlc_process_Terminate(struct vlc_process *process, bool kill_process);

/**
 * Read data from the process's standard output with a timeout.
 *
 * Attempts to read up to @p size bytes from the process's standard output
 * into the provided buffer, waiting up to @p timeout_ms milliseconds for data
 * to become available.
 *
 * On POSIX systems, this uses poll to wait for readability. On Windows,
 * a platform-specific implementation is used due to limitations with poll on
 * non-socket handles.
 *
 * @param [in]  process     Pointer to the vlc_process instance.
 * @param [out] buf         Buffer where the read data will be stored.
 * @param [in]  size        Maximum number of bytes to read.
 * @param [in]  timeout_ms  Timeout in milliseconds to wait for data.
 *
 * @return      The number of bytes read on success,
 *              -1 on error, and errno is set to indicate the error.
 */
VLC_API ssize_t
vlc_process_fd_Read(struct vlc_process *process, uint8_t *buf, size_t size,
                    vlc_tick_t timeout_ms);

/**
 * Write data to the process's standard input with a timeout.
 *
 * Attempts to write up to @p size bytes from the provided buffer to the
 * process's standard input, waiting up to @p timeout_ms milliseconds for the
 * pipe to become writable.
 *
 * On POSIX systems, this uses poll to wait for writability. On Windows,
 * a platform-specific implementation is used due to limitations with poll on
 * non-socket handles.
 *
 * @param [in]  process     Pointer to the vlc_process instance.
 * @param [in]  buf         Buffer containing the data to write.
 * @param [in]  size        Number of bytes to write.
 * @param [in]  timeout_ms  Timeout in milliseconds to wait for the pipe to be
 *                          writable.
 *
 * @return      The number of bytes read on success,
 *              -1 on error, and errno is set to indicate the error.
 */
VLC_API ssize_t
vlc_process_fd_Write(struct vlc_process *process, const uint8_t *buf, size_t size,
                     vlc_tick_t timeout_ms);

/**
 * @} process
 */

#endif /* VLC_PROCESS_H */
