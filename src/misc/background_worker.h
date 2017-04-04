/*****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#ifndef BACKGROUND_WORKER_H__
#define BACKGROUND_WORKER_H__

struct background_worker_config {
    /**
     * Default timeout for completing a task
     *
     * If less-than 0 a task can run indefinitely without being killed, whereas
     * a positive value denotes the maximum number of milliseconds a task can
     * run before \ref pf_stop is called to kill it.
     **/
    mtime_t default_timeout;

    /**
     * Release an entity
     *
     * This callback will be called in order to decrement the ref-count of a
     * entity within the background-worker. It will happen either when \ref
     * pf_stop has finished executing, or if the entity is removed from the
     * queue (through \ref background_worker_Cancel)
     *
     * \param entity the entity to release
     **/
    void( *pf_release )( void* entity );

    /**
     * Hold a queued item
     *
     * This callback will be called in order to increment the ref-count of an
     * entity. It will happen when the entity is pushed into the queue of
     * pending tasks as part of \ref background_worker_Push.
     *
     * \param entity the entity to hold
     **/
    void( *pf_hold )( void* entity );

    /**
     * Start a new task
     *
     * This callback is called in order to construct a new background task. In
     * order for the background-worker to be able to continue processing
     * incoming requests, \ref pf_start is meant to start a task (such as a
     * thread), and then store the associated handle in `*out`.
     *
     * The value of `*out` will then be the value of the argument named `handle`
     * in terms of \ref pf_probe and \ref pf_stop.
     *
     * \param owner the owner of the background-worker
     * \param entity the entity for which a task is to be created
     * \param out [out] `*out` shall, on success, refer to the handle associated
     *                   with the running task.
     * \return VLC_SUCCESS if a task was created, an error-code on failure.
     **/
    int( *pf_start )( void* owner, void* entity, void** out );

    /**
     * Probe a running task
     *
     * This callback is called in order to see whether or not a running task has
     * finished or not. It can be called anytime between a successful call to
     * \ref pf_start, and the corresponding call to \ref pf_stop.
     *
     * \param owner the owner of the background-worker
     * \param handle the handle associated with the running task
     * \return 0 if the task is still running, any other value if finished.
     **/
    int( *pf_probe )( void* owner, void* handle );

    /**
     * Stop a running task
     *
     * This callback is called in order to stop a running task. If \ref pf_start
     * has created a non-detached thread, \ref pf_stop is where you would
     * interrupt and then join it.
     *
     * \warning This function is called either after \ref pf_probe has stated
     *          that the task has finished, or if the timeout (if any) for the
     *          task has been reached.
     *
     * \param owner the owner of the background-worker
     * \parma handle the handle associated with the task to be stopped
     **/
    void( *pf_stop )( void* owner, void* handle );
};

/**
 * Create a background-worker
 *
 * This function creates a new background-worker using the passed configuration.
 *
 * \warning all members of `config` shall have been set by the caller.
 * \warning the returned resource must be destroyed using \ref
 *          background_worker_Delete on success.
 *
 * \param owner the owner of the background-worker
 * \param config the background-worker's configuration
 * \return a pointer-to the created background-worker on success,
 *         `NULL` on failure.
 **/
struct background_worker* background_worker_New( void* owner,
    struct background_worker_config* config );

/**
 * Request the background-worker to probe the current task
 *
 * This function is used to signal the background-worker that it should do
 * another probe to see whether the current task is still alive.
 *
 * \warning Note that the function will not wait for the probing to finish, it
 *          will simply ask the background worker to recheck it as soon as
 *          possible.
 *
 * \param worker the background-worker
 **/
void background_worker_RequestProbe( struct background_worker* worker );

/**
 * Push an entity into the background-worker
 *
 * This function is used to push an entity into the queue of pending work. The
 * entities will be processed in the order in which they are received (in terms
 * of the order of invocations in a single-threaded environment).
 *
 * \param worker the background-worker
 * \param entity the entity which is to be queued
 * \param id a value suitable for identifying the entity, or `NULL`
 * \param timeout the timeout of the entity in milliseconds, `0` denotes no
 *                timeout, a negative value will use the default timeout
 *                associated with the background-worker.
 * \return VLC_SUCCESS if the entity was successfully queued, an error-code on
 *         failure.
 **/
int background_worker_Push( struct background_worker* worker, void* entity,
    void* id, int timeout );

/**
 * Remove entities from the background-worker
 *
 * This function is used to remove processing of a certain entity given its
 * associated id, or to remove all queued (including currently running)
 * entities.
 *
 * \warning if the `id` passed refers to an entity that is currently being
 *          processed, the call will block until the task has been terminated.
 *
 * \param worker the background-worker
 * \param id NULL if every entity shall be removed, and the currently running
 *        task (if any) shall be cancelled.
 **/
void background_worker_Cancel( struct background_worker* worker, void* id );

/**
 * Delete a background-worker
 *
 * This function will destroy a background-worker created through \ref
 * background_worker_New. It will effectively stop the currently running task,
 * if any, and empty the queue of pending entities.
 *
 * \warning If there is a currently running task, the function will block until
 *          it has been stopped.
 *
 * \param worker the background-worker
 **/
void background_worker_Delete( struct background_worker* worker );
#endif
