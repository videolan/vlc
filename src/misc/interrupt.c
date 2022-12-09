/*****************************************************************************
 * interrupt.c:
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
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

/** @ingroup interrupt */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_EVENTFD_H
# include <sys/eventfd.h>
#endif

#include <vlc_common.h>
#include <vlc_fs.h> /* vlc_pipe */
#include <vlc_network.h> /* vlc_accept */

#include "interrupt.h"
#include "libvlc.h"

#ifdef I_CAN_HAZ_TSD
static thread_local vlc_interrupt_t *vlc_interrupt_var;
#else
#include <pthread.h>
static pthread_key_t vlc_interrupt_var_key;
static pthread_once_t vlc_interrupt_var_key_once = PTHREAD_ONCE_INIT;
static void vlc_interrupt_var_key_create()
{
    pthread_key_create(&vlc_interrupt_var_key, NULL);
}
#endif

/**
 * Initializes an interruption context.
 */
void vlc_interrupt_init(vlc_interrupt_t *ctx)
{
#ifndef I_CAN_HAZ_TSD
    pthread_once(&vlc_interrupt_var_key_once, vlc_interrupt_var_key_create);
#endif
    vlc_mutex_init(&ctx->lock);
    ctx->interrupted = false;
    atomic_init(&ctx->killed, false);
    ctx->callback = NULL;
}

vlc_interrupt_t *vlc_interrupt_create(void)
{
    vlc_interrupt_t *ctx = malloc(sizeof (*ctx));
    if (likely(ctx != NULL))
        vlc_interrupt_init(ctx);
    return ctx;
}

/**
 * Deinitializes an interruption context.
 * The context shall no longer be used by any thread.
 */
void vlc_interrupt_deinit(vlc_interrupt_t *ctx)
{
    assert(ctx->callback == NULL);
}

void vlc_interrupt_destroy(vlc_interrupt_t *ctx)
{
    assert(ctx != NULL);
    vlc_interrupt_deinit(ctx);
    free(ctx);
}

void vlc_interrupt_raise(vlc_interrupt_t *ctx)
{
    assert(ctx != NULL);

    /* This function must be reentrant. But the callback typically is not
     * reentrant. The lock ensures that all calls to the callback for a given
     * context are serialized. The lock also protects against invalid memory
     * accesses to the callback pointer proper, and the interrupted flag. */
    vlc_mutex_lock(&ctx->lock);
    ctx->interrupted = true;
    if (ctx->callback != NULL)
        ctx->callback(ctx->data);
    vlc_mutex_unlock(&ctx->lock);
}

vlc_interrupt_t *vlc_interrupt_set(vlc_interrupt_t *newctx)
{
#ifdef I_CAN_HAZ_TSD
    vlc_interrupt_t *oldctx = vlc_interrupt_var;

    vlc_interrupt_var = newctx;
#else
    vlc_interrupt_t *oldctx = pthread_getspecific(vlc_interrupt_var_key);

    pthread_setspecific(vlc_interrupt_var_key, newctx);
#endif
    return oldctx;
}

/**
 * Prepares to enter interruptible wait.
 * @param cb callback to interrupt the wait (i.e. wake up the thread)
 * @param data opaque data pointer for the callback
 * @note Any <b>successful</b> call <b>must</b> be paired with a call to
 * vlc_interrupt_finish().
 */
static void vlc_interrupt_prepare(vlc_interrupt_t *ctx,
                                  void (*cb)(void *), void *data)
{
    assert(ctx != NULL);
#ifdef I_CAN_HAZ_TSD
    assert(ctx == vlc_interrupt_var);
#else
    assert(ctx == pthread_getspecific(vlc_interrupt_var_key));
#endif

    vlc_mutex_lock(&ctx->lock);
    assert(ctx->callback == NULL);
    ctx->callback = cb;
    ctx->data = data;

    if (unlikely(ctx->interrupted))
        cb(data);
    vlc_mutex_unlock(&ctx->lock);
}

/**
 * Cleans up after an interruptible wait: waits for any pending invocations of
 * the callback previously registered with vlc_interrupt_prepare(), and rechecks
 * for any pending interruption.
 *
 * @warning As this function waits for ongoing callback invocation to complete,
 * the caller must not hold any resource necessary for the callback to run.
 * Otherwise a deadlock may occur.
 *
 * @return EINTR if an interruption occurred, zero otherwise
 */
static int vlc_interrupt_finish(vlc_interrupt_t *ctx)
{
    int ret = 0;

    assert(ctx != NULL);
#ifdef I_CAN_HAZ_TSD
    assert(ctx == vlc_interrupt_var);
#else
    assert(ctx == pthread_getspecific(vlc_interrupt_var_key));
#endif

    /* Wait for pending callbacks to prevent access by other threads. */
    vlc_mutex_lock(&ctx->lock);
    ctx->callback = NULL;
    if (ctx->interrupted)
    {
        ret = EINTR;
        ctx->interrupted = false;
    }
    vlc_mutex_unlock(&ctx->lock);
    return ret;
}

void vlc_interrupt_register(void (*cb)(void *), void *opaque)
{
#ifdef I_CAN_HAZ_TSD
    vlc_interrupt_t *ctx = vlc_interrupt_var;
#else
    vlc_interrupt_t *ctx = pthread_getspecific(vlc_interrupt_var_key);
#endif
    if (ctx != NULL)
        vlc_interrupt_prepare(ctx, cb, opaque);
}

int vlc_interrupt_unregister(void)
{
#ifdef I_CAN_HAZ_TSD
    vlc_interrupt_t *ctx = vlc_interrupt_var;
#else
    vlc_interrupt_t *ctx = pthread_getspecific(vlc_interrupt_var_key);
#endif
    return (ctx != NULL) ? vlc_interrupt_finish(ctx) : 0;
}

static void vlc_interrupt_cleanup(void *opaque)
{
    vlc_interrupt_finish(opaque);
}

void vlc_interrupt_kill(vlc_interrupt_t *ctx)
{
    assert(ctx != NULL);

    atomic_store(&ctx->killed, true);
    vlc_interrupt_raise(ctx);
}

bool vlc_killed(void)
{
#ifdef I_CAN_HAZ_TSD
    vlc_interrupt_t *ctx = vlc_interrupt_var;
#else
    vlc_interrupt_t *ctx = pthread_getspecific(vlc_interrupt_var_key);
#endif

    return (ctx != NULL) && atomic_load(&ctx->killed);
}

static void vlc_interrupt_sem(void *opaque)
{
    vlc_sem_post(opaque);
}

int vlc_sem_wait_i11e(vlc_sem_t *sem)
{
#ifdef I_CAN_HAZ_TSD
    vlc_interrupt_t *ctx = vlc_interrupt_var;
#else
    vlc_interrupt_t *ctx = pthread_getspecific(vlc_interrupt_var_key);
#endif
    if (ctx == NULL)
        return vlc_sem_wait(sem), 0;

    vlc_interrupt_prepare(ctx, vlc_interrupt_sem, sem);

    vlc_cleanup_push(vlc_interrupt_cleanup, ctx);
    vlc_sem_wait(sem);
    vlc_cleanup_pop();

    return vlc_interrupt_finish(ctx);
}

static void vlc_mwait_i11e_wake(void *opaque)
{
    vlc_cond_signal(opaque);
}

static void vlc_mwait_i11e_cleanup(void *opaque)
{
    vlc_interrupt_t *ctx = opaque;

    vlc_mutex_unlock(&ctx->lock);
    vlc_interrupt_finish(ctx);
}

int vlc_mwait_i11e(vlc_tick_t deadline)
{
#ifdef I_CAN_HAZ_TSD
    vlc_interrupt_t *ctx = vlc_interrupt_var;
#else
    vlc_interrupt_t *ctx = pthread_getspecific(vlc_interrupt_var_key);
#endif
    if (ctx == NULL)
        return vlc_tick_wait(deadline), 0;

    vlc_cond_t wait;
    vlc_cond_init(&wait);

    vlc_interrupt_prepare(ctx, vlc_mwait_i11e_wake, &wait);

    vlc_mutex_lock(&ctx->lock);
    vlc_cleanup_push(vlc_mwait_i11e_cleanup, ctx);
    while (!ctx->interrupted
        && vlc_cond_timedwait(&wait, &ctx->lock, deadline) == 0);
    vlc_cleanup_pop();
    vlc_mutex_unlock(&ctx->lock);

    return vlc_interrupt_finish(ctx);
}

static void vlc_interrupt_forward_wake(void *opaque)
{
    void **data = opaque;
    vlc_interrupt_t *to = data[0];
    vlc_interrupt_t *from = data[1];

    (atomic_load(&from->killed) ? vlc_interrupt_kill
                                : vlc_interrupt_raise)(to);
}

void vlc_interrupt_forward_start(vlc_interrupt_t *to, void *data[2])
{
    data[0] = data[1] = NULL;

#ifdef I_CAN_HAZ_TSD
    vlc_interrupt_t *from = vlc_interrupt_var;
#else
    vlc_interrupt_t *from = pthread_getspecific(vlc_interrupt_var_key);
#endif
    if (from == NULL)
        return;

    assert(from != to);
    data[0] = to;
    data[1] = from;
    vlc_interrupt_prepare(from, vlc_interrupt_forward_wake, data);
}

int vlc_interrupt_forward_stop(void *const data[2])
{
    vlc_interrupt_t *from = data[1];
    if (from == NULL)
        return 0;

    assert(from->callback == vlc_interrupt_forward_wake);
    assert(from->data == data);
    return vlc_interrupt_finish(from);
}

#ifndef _WIN32
# include <fcntl.h>
# include <sys/uio.h>
# ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
# endif

static void vlc_poll_i11e_wake(void *opaque)
{
    uint64_t value = 1;
    int *fd = opaque;
    int canc;

    canc = vlc_savecancel();
    write(fd[1], &value, sizeof (value));
    vlc_restorecancel(canc);
}

static void vlc_poll_i11e_cleanup(void *opaque)
{
    vlc_interrupt_t *ctx = opaque;
    int *fd = ctx->data;

    vlc_interrupt_finish(ctx);
    if (fd[1] != fd[0])
        vlc_close(fd[1]);
    vlc_close(fd[0]);
}

static int vlc_poll_i11e_inner(struct pollfd *restrict fds, unsigned nfds,
                               int timeout, vlc_interrupt_t *ctx,
                               struct pollfd *restrict ufd)
{
    int fd[2];
    int ret = -1;
    int canc;

    /* TODO: cache this */
# if defined (HAVE_EVENTFD) && defined (EFD_CLOEXEC)
    canc = vlc_savecancel();
    fd[0] = eventfd(0, EFD_CLOEXEC);
    vlc_restorecancel(canc);
    if (fd[0] != -1)
        fd[1] = fd[0];
    else
# endif
    if (vlc_pipe(fd))
    {
        vlc_testcancel();
        errno = ENOMEM;
        return -1;
    }

    for (unsigned i = 0; i < nfds; i++)
    {
        ufd[i].fd = fds[i].fd;
        ufd[i].events = fds[i].events;
    }
    ufd[nfds].fd = fd[0];
    ufd[nfds].events = POLLIN;

    vlc_interrupt_prepare(ctx, vlc_poll_i11e_wake, fd);

    vlc_cleanup_push(vlc_poll_i11e_cleanup, ctx);
    ret = poll(ufd, nfds + 1, timeout);

    for (unsigned i = 0; i < nfds; i++)
        fds[i].revents = ufd[i].revents;

    if (ret > 0 && ufd[nfds].revents)
    {
        uint64_t dummy;

        read(fd[0], &dummy, sizeof (dummy));
        ret--;
    }
    vlc_cleanup_pop();

    if (vlc_interrupt_finish(ctx))
    {
        errno = EINTR;
        ret = -1;
    }

    canc = vlc_savecancel();
    if (fd[1] != fd[0])
        vlc_close(fd[1]);
    vlc_close(fd[0]);
    vlc_restorecancel(canc);
    return ret;
}

int vlc_poll_i11e(struct pollfd *fds, unsigned nfds, int timeout)
{
#ifdef I_CAN_HAZ_TSD
    vlc_interrupt_t *ctx = vlc_interrupt_var;
#else
    vlc_interrupt_t *ctx = pthread_getspecific(vlc_interrupt_var_key);
#endif
    if (ctx == NULL)
        return poll(fds, nfds, timeout);

    int ret;

    if (likely(nfds < 255))
    {   /* Fast path with stack allocation */
        struct pollfd ufd[nfds + 1];

        ret = vlc_poll_i11e_inner(fds, nfds, timeout, ctx, ufd);
    }
    else
    {   /* Slow path but poll() is slow with large nfds anyway. */
        struct pollfd *ufd = vlc_alloc(nfds + 1, sizeof (*ufd));
        if (unlikely(ufd == NULL))
            return -1; /* ENOMEM */

        vlc_cleanup_push(free, ufd);
        ret = vlc_poll_i11e_inner(fds, nfds, timeout, ctx, ufd);
        vlc_cleanup_pop();
        free(ufd);
    }
    return ret;
}

static int vlc_poll_file(int fd, unsigned int mask)
{
    struct pollfd ufd;

    ufd.fd = fd;
    ufd.events = mask;
    return vlc_poll_i11e(&ufd, 1, -1);
}

static int vlc_poll_sock(int sock, unsigned int mask)
{
    return vlc_poll_file(sock, mask);
}

#else /* !_WIN32 */
static void CALLBACK vlc_poll_i11e_wake_self(ULONG_PTR data)
{
    (void) data; /* Nothing to do */
}

static void vlc_poll_i11e_wake(void *opaque)
{
    HANDLE th = opaque;
    QueueUserAPC(vlc_poll_i11e_wake_self, th, 0);
}

static void vlc_poll_i11e_cleanup(void *opaque)
{
    vlc_interrupt_t *ctx = opaque;
    HANDLE th = ctx->data;

    vlc_interrupt_finish(ctx);
    CloseHandle(th);
}

int vlc_poll_i11e(struct pollfd *fds, unsigned nfds, int timeout)
{
#ifdef I_CAN_HAZ_TSD
    vlc_interrupt_t *ctx = vlc_interrupt_var;
#else
    vlc_interrupt_t *ctx = pthread_getspecific(vlc_interrupt_var_key);
#endif
    if (ctx == NULL)
        return vlc_poll(fds, nfds, timeout);

    int ret = -1;
    HANDLE th;

    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                         GetCurrentProcess(), &th, 0, FALSE,
                         DUPLICATE_SAME_ACCESS))
    {
        errno = ENOMEM;
        return -1;
    }

    vlc_interrupt_prepare(ctx, vlc_poll_i11e_wake, th);

    vlc_cleanup_push(vlc_poll_i11e_cleanup, ctx);
    ret = vlc_poll(fds, nfds, timeout);
    vlc_cleanup_pop();

    if (vlc_interrupt_finish(ctx))
    {
        errno = EINTR;
        ret = -1;
    }

    CloseHandle(th);
    return ret;
}

static int vlc_poll_file(int fd, unsigned int mask)
{
    (void) fd; (void) mask;
    return 1;
}

static int vlc_poll_sock(int sock, unsigned int mask)
{
    struct pollfd ufd;

    ufd.fd = sock;
    ufd.events = mask;
    return vlc_poll_i11e(&ufd, 1, -1);
}
#endif /* _WIN32 */

/* There are currently no ways to atomically force a non-blocking read or write
 * operations. Even for sockets, the MSG_DONTWAIT flag is non-standard.
 *
 * So in the event that more than one thread tries to read or write on the same
 * file at the same time, there is a race condition where these functions might
 * block in spite of an interruption. This should never happen in practice.
 */

/**
 * Wrapper for readv() that returns the EINTR error upon VLC I/O interruption.
 * @warning This function ignores the non-blocking file flag.
 */
ssize_t vlc_readv_i11e(int fd, struct iovec *iov, int count)
{
    if (vlc_poll_file(fd, POLLIN) < 0)
        return -1;
    return readv(fd, iov, count);
}

/**
 * Wrapper for writev() that returns the EINTR error upon VLC I/O interruption.
 *
 * @note Like writev(), once some but not all bytes are written, the function
 * might wait for write completion, regardless of signals and interruptions.
 * @warning This function ignores the non-blocking file flag.
 */
ssize_t vlc_writev_i11e(int fd, const struct iovec *iov, int count)
{
    if (vlc_poll_file(fd, POLLOUT) < 0)
        return -1;
    return vlc_writev(fd, iov, count);
}

/**
 * Wrapper for read() that returns the EINTR error upon VLC I/O interruption.
 * @warning This function ignores the non-blocking file flag.
 */
ssize_t vlc_read_i11e(int fd, void *buf, size_t count)
{
    struct iovec iov = { .iov_base = buf, .iov_len = count };
    return vlc_readv_i11e(fd, &iov, 1);
}

/**
 * Wrapper for write() that returns the EINTR error upon VLC I/O interruption.
 *
 * @note Like write(), once some but not all bytes are written, the function
 * might wait for write completion, regardless of signals and interruptions.
 * @warning This function ignores the non-blocking file flag.
 */
ssize_t vlc_write_i11e(int fd, const void *buf, size_t count)
{
    struct iovec iov = { .iov_base = (void*)buf, .iov_len = count };
    return vlc_writev_i11e(fd, &iov, 1);
}

ssize_t vlc_recvmsg_i11e(int fd, struct msghdr *msg, int flags)
{
    if (vlc_poll_sock(fd, POLLIN) < 0)
        return -1;
    /* NOTE: MSG_OOB and MSG_PEEK should work fine here.
     * MSG_WAITALL is not supported at this point. */
    return recvmsg(fd, msg, flags);
}

#ifndef _WIN32
ssize_t vlc_recvfrom_i11e(int fd, void *buf, size_t len, int flags,
                        struct sockaddr *addr, socklen_t *addrlen)
{
    struct iovec iov = { .iov_base = buf, .iov_len = len };
    struct msghdr msg = {
        .msg_name = addr,
        .msg_namelen = (addrlen != NULL) ? *addrlen : 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };

    ssize_t ret = vlc_recvmsg_i11e(fd, &msg, flags);
    if (ret >= 0 && addrlen != NULL)
        *addrlen = msg.msg_namelen;
    return ret;
}
#endif

ssize_t vlc_sendmsg_i11e(int fd, const struct msghdr *msg, int flags)
{
    if (vlc_poll_sock(fd, POLLOUT) < 0)
        return -1;
    /* NOTE: MSG_EOR and MSG_OOB should all work fine here. */
    return vlc_sendmsg(fd, msg, flags);
}

#ifndef _WIN32
ssize_t vlc_sendto_i11e(int fd, const void *buf, size_t len, int flags,
                      const struct sockaddr *addr, socklen_t addrlen)
{
    struct iovec iov = { .iov_base = (void *)buf, .iov_len = len };
    struct msghdr msg = {
        .msg_name = (struct sockaddr *)addr,
        .msg_namelen = addrlen,
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };

    return vlc_sendmsg_i11e(fd, &msg, flags);
}

#else /* _WIN32 */

ssize_t vlc_recvfrom_i11e(int fd, void *buf, size_t len, int flags,
                        struct sockaddr *addr, socklen_t *addrlen)
{
    if (vlc_poll_sock(fd, POLLIN) < 0)
        return -1;

    ssize_t ret = recvfrom(fd, buf, len, flags, addr, addrlen);
    if (ret < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
        errno = EAGAIN;
    return ret;
}

ssize_t vlc_sendto_i11e(int fd, const void *buf, size_t len, int flags,
                      const struct sockaddr *addr, socklen_t addrlen)
{
    if (vlc_poll_sock(fd, POLLOUT) < 0)
        return -1;

    ssize_t ret = sendto(fd, buf, len, flags, addr, addrlen);
    if (ret < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
        errno = EAGAIN;
    return ret;
}

#endif

int vlc_accept_i11e(int fd, struct sockaddr *addr, socklen_t *addrlen,
                  bool nonblock)
{
    if (vlc_poll_sock(fd, POLLIN) < 0)
        return -1;
    return vlc_accept(fd, addr, addrlen, nonblock);
}

