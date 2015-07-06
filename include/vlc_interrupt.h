/*****************************************************************************
 * vlc_interrupt.h:
 *****************************************************************************
 * Copyright (C) 2015 Remlab T:mi
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

/**
 * @file
 * This file declares interruptible sleep functions.
 */

#ifndef VLC_INTERRUPT_H
# define VLC_INTERRUPT_H 1
# include <vlc_threads.h>
# ifndef _WIN32
#  include <sys/socket.h> /* socklen_t */
# else
#  include <ws2tcpip.h>
# endif

struct pollfd;
struct iovec;
struct sockaddr;
struct msghdr;

/**
 * @defgroup interrupt Interruptible sleep
 * @{
 * @defgroup interrupt_sleep Interruptible sleep functions
 * @{
 */

/**
 * Interruptible variant of vlc_sem_wait().
 *
 * Waits on a semaphore like vlc_sem_wait(). If the calling thread has an
 * interruption context (as set by vlc_interrupt_set()), and another thread
 * invokes vlc_interrupt_raise() on that context, the semaphore is incremented.
 *
 * @warning The calling thread should be the only thread ever to wait on the
 * specified semaphore. Otherwise, interruptions may not be delivered
 * accurately (the wrong thread may be woken up).
 *
 * @note This function is (always) a cancellation point.
 *
 * @return EINTR if the semaphore was incremented due to an interruption,
 * otherwise zero.
 */
VLC_API int vlc_sem_wait_i11e(vlc_sem_t *);

/**
 * Interruptible variant of mwait().
 *
 * Waits for a specified timestamp or, if the calling thread has an
 * interruption context, an interruption.
 *
 * @return EINTR if an interruption occurred, otherwise 0 once the timestamp is
 * reached.
 */
VLC_API int vlc_mwait_i11e(mtime_t);

/**
 * Interruptible variant of msleep().
 *
 * Waits for a specified timeout duration or, if the calling thread has an
 * interruption context, an interruption.
 *
 * @param delay timeout value (in microseconds)
 *
 * @return EINTR if an interruption occurred, otherwise 0 once the timeout
 * expired.
 */
static inline int vlc_msleep_i11e(mtime_t delay)
{
    return vlc_mwait_i11e(mdate() + delay);
}

/**
 * Interruptible variant of poll().
 *
 * Waits for file descriptors I/O events, a timeout, a signal or a VLC I/O
 * interruption. Except for VLC I/O interruptions, this function behaves
 * just like the standard poll().
 *
 * @note This function is always a cancellation point (as poll()).
 * @see poll() manual page
 *
 * @param fds table of events to wait for
 * @param nfds number of entries in the table
 * @param timeout time to wait in milliseconds or -1 for infinite
 *
 * @return A strictly positive result represent the number of pending events.
 * 0 is returned if the time-out is reached without events.
 * -1 is returned if a VLC I/O interrupt occurs (and errno is set to EINTR)
 * or if an error occurs.
 */
VLC_API int vlc_poll_i11e(struct pollfd *, unsigned, int);

VLC_API ssize_t vlc_readv_i11e(int fd, struct iovec *, int);
VLC_API ssize_t vlc_writev_i11e(int fd, const struct iovec *, int);
VLC_API ssize_t vlc_read_i11e(int fd, void *, size_t);
VLC_API ssize_t vlc_write_i11e(int fd, const void *, size_t);

VLC_API ssize_t vlc_recvmsg_i11e(int fd, struct msghdr *, int flags);
VLC_API ssize_t vlc_sendmsg_i11e(int fd, const struct msghdr *, int flags);

VLC_API ssize_t vlc_recvfrom_i11e(int fd, void *, size_t, int flags,
                                struct sockaddr *, socklen_t *);
VLC_API ssize_t vlc_sendto_i11e(int fd, const void *, size_t, int flags,
                              const struct sockaddr *, socklen_t);

static inline ssize_t vlc_recv_i11e(int fd, void *buf, size_t len, int flags)
{
    return vlc_recvfrom_i11e(fd, buf, len, flags, NULL, NULL);
}

static inline
ssize_t vlc_send_i11e(int fd, const void *buf, size_t len, int flags)
{
    return vlc_sendto_i11e(fd, buf, len, flags, NULL, 0);
}

VLC_API int vlc_accept_i11e(int fd, struct sockaddr *, socklen_t *, bool);

/**
 * @}
 * @defgroup interrupt_context Interrupt context signaling and manipulation
 * @{
 */
typedef struct vlc_interrupt vlc_interrupt_t;

/**
 * Creates an interruption context.
 */
VLC_API vlc_interrupt_t *vlc_interrupt_create(void) VLC_USED;

/**
 * Destroys an interrupt context.
 */
VLC_API void vlc_interrupt_destroy(vlc_interrupt_t *);

/**
 * Sets the interruption context for the calling thread.
 * @param newctx the interruption context to attach or NULL for none
 * @return the previous interruption context or NULL if none
 *
 * @note This function is not a cancellation point.
 * @warning A context can be attached to no more than one thread at a time.
 */
VLC_API vlc_interrupt_t *vlc_interrupt_set(vlc_interrupt_t *);

/**
 * Raises an interruption through a specified context.
 *
 * This is used to asynchronously wake a thread up while it is waiting on some
 * other events (typically I/O events).
 *
 * @note This function is thread-safe.
 * @note This function is not a cancellation point.
 */
VLC_API void vlc_interrupt_raise(vlc_interrupt_t *);

/**
 * Marks the interruption context as "killed".
 *
 * This is not reversible.
 */
VLC_API void vlc_interrupt_kill(vlc_interrupt_t *);

/**
 * Checks if the interruption context was "killed".
 *
 * Indicates whether the interruption context of the calling thread (if any)
 * was killed with vlc_interrupt_kill().
 */
VLC_API bool vlc_killed(void) VLC_USED;

/** @} @} */
#endif
