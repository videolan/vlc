/*****************************************************************************
 * libvlc.h:  libvlc external API
 *****************************************************************************
 * Copyright (C) 1998-2009 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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
 * \defgroup libvlc LibVLC
 * LibVLC is the external programming interface of the VLC media player.
 * It is used to embed VLC into other applications or frameworks.
 * @{
 * \file
 * LibVLC core external API
 */

#ifndef VLC_LIBVLC_H
#define VLC_LIBVLC_H 1

#if defined (_WIN32) && defined (DLL_EXPORT)
# define LIBVLC_API __declspec(dllexport)
#elif defined (__GNUC__) && (__GNUC__ >= 4)
# define LIBVLC_API __attribute__((visibility("default")))
#else
# define LIBVLC_API
#endif

#ifdef __LIBVLC__
/* Avoid unhelpful warnings from libvlc with our deprecated APIs */
#   define LIBVLC_DEPRECATED
#elif defined(__GNUC__) && \
      (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 0)
# define LIBVLC_DEPRECATED __attribute__((deprecated))
#else
# define LIBVLC_DEPRECATED
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

# ifdef __cplusplus
extern "C" {
# endif

/** \defgroup libvlc_core LibVLC core
 * \ingroup libvlc
 * Before it can do anything useful, LibVLC must be initialized.
 * You can create one (or more) instance(s) of LibVLC in a given process,
 * with libvlc_new() and destroy them with libvlc_release().
 *
 * \version Unless otherwise stated, these functions are available
 * from LibVLC versions numbered 1.1.0 or more.
 * Earlier versions (0.9.x and 1.0.x) are <b>not</b> compatible.
 * @{
 */

/** This structure is opaque. It represents a libvlc instance */
typedef struct libvlc_instance_t libvlc_instance_t;

typedef int64_t libvlc_time_t;

/** \defgroup libvlc_error LibVLC error handling
 * @{
 */

/**
 * A human-readable error message for the last LibVLC error in the calling
 * thread. The resulting string is valid until another error occurs (at least
 * until the next LibVLC call).
 *
 * @warning
 * This will be NULL if there was no error.
 */
LIBVLC_API const char *libvlc_errmsg (void);

/**
 * Clears the LibVLC error status for the current thread. This is optional.
 * By default, the error status is automatically overridden when a new error
 * occurs, and destroyed when the thread exits.
 */
LIBVLC_API void libvlc_clearerr (void);

/**
 * Sets the LibVLC error status and message for the current thread.
 * Any previous error is overridden.
 * \param fmt the format string
 * \param ap the arguments
 * \return a nul terminated string in any case
 */
LIBVLC_API const char *libvlc_vprinterr (const char *fmt, va_list ap);

/**
 * Sets the LibVLC error status and message for the current thread.
 * Any previous error is overridden.
 * \param fmt the format string
 * \param args the arguments
 * \return a nul terminated string in any case
 */
LIBVLC_API const char *libvlc_printerr (const char *fmt, ...);

/**@} */

/**
 * Create and initialize a libvlc instance.
 * This functions accept a list of "command line" arguments similar to the
 * main(). These arguments affect the LibVLC instance default configuration.
 *
 * \note
 * LibVLC may create threads. Therefore, any thread-unsafe process
 * initialization must be performed before calling libvlc_new(). In particular
 * and where applicable:
 * - setlocale() and textdomain(),
 * - setenv(), unsetenv() and putenv(),
 * - with the X11 display system, XInitThreads()
 *   (see also libvlc_media_player_set_xwindow()) and
 * - on Microsoft Windows, SetErrorMode().
 * - sigprocmask() shall never be invoked; pthread_sigmask() can be used.
 *
 * On POSIX systems, the SIGCHLD signal <b>must not</b> be ignored, i.e. the
 * signal handler must set to SIG_DFL or a function pointer, not SIG_IGN.
 * Also while LibVLC is active, the wait() function shall not be called, and
 * any call to waitpid() shall use a strictly positive value for the first
 * parameter (i.e. the PID). Failure to follow those rules may lead to a
 * deadlock or a busy loop.
 * Also on POSIX systems, it is recommended that the SIGPIPE signal be blocked,
 * even if it is not, in principles, necessary, e.g.:
 * @code
   sigset_t set;

   signal(SIGCHLD, SIG_DFL);
   sigemptyset(&set);
   sigaddset(&set, SIGPIPE);
   pthread_sigmask(SIG_BLOCK, &set, NULL);
 * @endcode
 *
 * On Microsoft Windows, setting the default DLL directories to SYSTEM32
 * exclusively is strongly recommended for security reasons:
 * @code
   SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
 * @endcode
 *
 * \version
 * Arguments are meant to be passed from the command line to LibVLC, just like
 * VLC media player does. The list of valid arguments depends on the LibVLC
 * version, the operating system and platform, and set of available LibVLC
 * plugins. Invalid or unsupported arguments will cause the function to fail
 * (i.e. return NULL). Also, some arguments may alter the behaviour or
 * otherwise interfere with other LibVLC functions.
 *
 * \warning
 * There is absolutely no warranty or promise of forward, backward and
 * cross-platform compatibility with regards to libvlc_new() arguments.
 * We recommend that you do not use them, other than when debugging.
 *
 * \param argc the number of arguments (should be 0)
 * \param argv list of arguments (should be NULL)
 * \return the libvlc instance or NULL in case of error
 */
LIBVLC_API libvlc_instance_t *
libvlc_new( int argc , const char *const *argv );

/**
 * Decrement the reference count of a libvlc instance, and destroy it
 * if it reaches zero.
 *
 * \param p_instance the instance to destroy
 */
LIBVLC_API void libvlc_release( libvlc_instance_t *p_instance );

/**
 * Increments the reference count of a libvlc instance.
 * The initial reference count is 1 after libvlc_new() returns.
 *
 * \param p_instance the instance to reference
 */
LIBVLC_API void libvlc_retain( libvlc_instance_t *p_instance );

/**
 * Try to start a user interface for the libvlc instance.
 *
 * \param p_instance the instance
 * \param name interface name, or NULL for default
 * \return 0 on success, -1 on error.
 */
LIBVLC_API
int libvlc_add_intf( libvlc_instance_t *p_instance, const char *name );

/**
 * Registers a callback for the LibVLC exit event. This is mostly useful if
 * the VLC playlist and/or at least one interface are started with
 * libvlc_playlist_play() or libvlc_add_intf() respectively.
 * Typically, this function will wake up your application main loop (from
 * another thread).
 *
 * \note This function should be called before the playlist or interface are
 * started. Otherwise, there is a small race condition: the exit event could
 * be raised before the handler is registered.
 *
 * \param p_instance LibVLC instance
 * \param cb callback to invoke when LibVLC wants to exit,
 *           or NULL to disable the exit handler (as by default)
 * \param opaque data pointer for the callback
 */
LIBVLC_API
void libvlc_set_exit_handler( libvlc_instance_t *p_instance,
                              void (*cb) (void *), void *opaque );

/**
 * Sets the application name. LibVLC passes this as the user agent string
 * when a protocol requires it.
 *
 * \param p_instance LibVLC instance
 * \param name human-readable application name, e.g. "FooBar player 1.2.3"
 * \param http HTTP User Agent, e.g. "FooBar/1.2.3 Python/2.6.0"
 * \version LibVLC 1.1.1 or later
 */
LIBVLC_API
void libvlc_set_user_agent( libvlc_instance_t *p_instance,
                            const char *name, const char *http );

/**
 * Sets some meta-information about the application.
 * See also libvlc_set_user_agent().
 *
 * \param p_instance LibVLC instance
 * \param id Java-style application identifier, e.g. "com.acme.foobar"
 * \param version application version numbers, e.g. "1.2.3"
 * \param icon application icon name, e.g. "foobar"
 * \version LibVLC 2.1.0 or later.
 */
LIBVLC_API
void libvlc_set_app_id( libvlc_instance_t *p_instance, const char *id,
                        const char *version, const char *icon );

/**
 * Retrieve libvlc version.
 *
 * Example: "1.1.0-git The Luggage"
 *
 * \return a string containing the libvlc version
 */
LIBVLC_API const char * libvlc_get_version(void);

/**
 * Retrieve libvlc compiler version.
 *
 * Example: "gcc version 4.2.3 (Ubuntu 4.2.3-2ubuntu6)"
 *
 * \return a string containing the libvlc compiler version
 */
LIBVLC_API const char * libvlc_get_compiler(void);

/**
 * Retrieve libvlc changeset.
 *
 * Example: "aa9bce0bc4"
 *
 * \return a string containing the libvlc changeset
 */
LIBVLC_API const char * libvlc_get_changeset(void);

/**
 * Frees an heap allocation returned by a LibVLC function.
 * If you know you're using the same underlying C run-time as the LibVLC
 * implementation, then you can call ANSI C free() directly instead.
 *
 * \param ptr the pointer
 */
LIBVLC_API void libvlc_free( void *ptr );

/** \defgroup libvlc_event LibVLC asynchronous events
 * LibVLC emits asynchronous events.
 *
 * Several LibVLC objects (such @ref libvlc_instance_t as
 * @ref libvlc_media_player_t) generate events asynchronously. Each of them
 * provides @ref libvlc_event_manager_t event manager. You can subscribe to
 * events with libvlc_event_attach() and unsubscribe with
 * libvlc_event_detach().
 * @{
 */

/**
 * Event manager that belongs to a libvlc object, and from whom events can
 * be received.
 */
typedef struct libvlc_event_manager_t libvlc_event_manager_t;

struct libvlc_event_t;

/**
 * Type of a LibVLC event.
 */
typedef int libvlc_event_type_t;

/**
 * Callback function notification
 * \param p_event the event triggering the callback
 */
typedef void ( *libvlc_callback_t )( const struct libvlc_event_t *p_event, void *p_data );

/**
 * Register for an event notification.
 *
 * \param p_event_manager the event manager to which you want to attach to.
 *        Generally it is obtained by vlc_my_object_event_manager() where
 *        my_object is the object you want to listen to.
 * \param i_event_type the desired event to which we want to listen
 * \param f_callback the function to call when i_event_type occurs
 * \param user_data user provided data to carry with the event
 * \return 0 on success, ENOMEM on error
 */
LIBVLC_API int libvlc_event_attach( libvlc_event_manager_t *p_event_manager,
                                        libvlc_event_type_t i_event_type,
                                        libvlc_callback_t f_callback,
                                        void *user_data );

/**
 * Unregister an event notification.
 *
 * \param p_event_manager the event manager
 * \param i_event_type the desired event to which we want to unregister
 * \param f_callback the function to call when i_event_type occurs
 * \param p_user_data user provided data to carry with the event
 */
LIBVLC_API void libvlc_event_detach( libvlc_event_manager_t *p_event_manager,
                                         libvlc_event_type_t i_event_type,
                                         libvlc_callback_t f_callback,
                                         void *p_user_data );

/** @} */

/** \defgroup libvlc_log LibVLC logging
 * libvlc_log_* functions provide access to the LibVLC messages log.
 * This is used for logging and debugging.
 * @{
 */

/**
 * Logging messages level.
 * \note Future LibVLC versions may define new levels.
 */
enum libvlc_log_level
{
    LIBVLC_DEBUG=0,   /**< Debug message */
    LIBVLC_NOTICE=2,  /**< Important informational message */
    LIBVLC_WARNING=3, /**< Warning (potential error) message */
    LIBVLC_ERROR=4    /**< Error message */
};

typedef struct vlc_log_t libvlc_log_t;

/**
 * Gets log message debug infos.
 *
 * This function retrieves self-debug information about a log message:
 * - the name of the VLC module emitting the message,
 * - the name of the source code module (i.e. file) and
 * - the line number within the source code module.
 *
 * The returned module name and file name will be NULL if unknown.
 * The returned line number will similarly be zero if unknown.
 *
 * \param ctx message context (as passed to the @ref libvlc_log_cb callback)
 * \param module module name storage (or NULL) [OUT]
 * \param file source code file name storage (or NULL) [OUT]
 * \param line source code file line number storage (or NULL) [OUT]
 * \warning The returned module name and source code file name, if non-NULL,
 * are only valid until the logging callback returns.
 *
 * \version LibVLC 2.1.0 or later
 */
LIBVLC_API void libvlc_log_get_context(const libvlc_log_t *ctx,
                       const char **module, const char **file, unsigned *line);

/**
 * Gets log message info.
 *
 * This function retrieves meta-information about a log message:
 * - the type name of the VLC object emitting the message,
 * - the object header if any, and
 * - a temporaly-unique object identifier.
 *
 * This information is mainly meant for <b>manual</b> troubleshooting.
 *
 * The returned type name may be "generic" if unknown, but it cannot be NULL.
 * The returned header will be NULL if unset; in current versions, the header
 * is used to distinguish for VLM inputs.
 * The returned object ID will be zero if the message is not associated with
 * any VLC object.
 *
 * \param ctx message context (as passed to the @ref libvlc_log_cb callback)
 * \param name object name storage (or NULL) [OUT]
 * \param header object header (or NULL) [OUT]
 * \param line source code file line number storage (or NULL) [OUT]
 * \warning The returned module name and source code file name, if non-NULL,
 * are only valid until the logging callback returns.
 *
 * \version LibVLC 2.1.0 or later
 */
LIBVLC_API void libvlc_log_get_object(const libvlc_log_t *ctx,
                        const char **name, const char **header, uintptr_t *id);

/**
 * Callback prototype for LibVLC log message handler.
 *
 * \param data data pointer as given to libvlc_log_set()
 * \param level message level (@ref libvlc_log_level)
 * \param ctx message context (meta-information about the message)
 * \param fmt printf() format string (as defined by ISO C11)
 * \param args variable argument list for the format
 * \note Log message handlers <b>must</b> be thread-safe.
 * \warning The message context pointer, the format string parameters and the
 *          variable arguments are only valid until the callback returns.
 */
typedef void (*libvlc_log_cb)(void *data, int level, const libvlc_log_t *ctx,
                              const char *fmt, va_list args);

/**
 * Unsets the logging callback.
 *
 * This function deregisters the logging callback for a LibVLC instance.
 * This is rarely needed as the callback is implicitly unset when the instance
 * is destroyed.
 *
 * \note This function will wait for any pending callbacks invocation to
 * complete (causing a deadlock if called from within the callback).
 *
 * \param p_instance libvlc instance
 * \version LibVLC 2.1.0 or later
 */
LIBVLC_API void libvlc_log_unset( libvlc_instance_t *p_instance );

/**
 * Sets the logging callback for a LibVLC instance.
 *
 * This function is thread-safe: it will wait for any pending callbacks
 * invocation to complete.
 *
 * \param cb callback function pointer
 * \param data opaque data pointer for the callback function
 *
 * \note Some log messages (especially debug) are emitted by LibVLC while
 * is being initialized. These messages cannot be captured with this interface.
 *
 * \warning A deadlock may occur if this function is called from the callback.
 *
 * \param p_instance libvlc instance
 * \version LibVLC 2.1.0 or later
 */
LIBVLC_API void libvlc_log_set( libvlc_instance_t *p_instance,
                                libvlc_log_cb cb, void *data );


/**
 * Sets up logging to a file.
 * \param p_instance libvlc instance
 * \param stream FILE pointer opened for writing
 *         (the FILE pointer must remain valid until libvlc_log_unset())
 * \version LibVLC 2.1.0 or later
 */
LIBVLC_API void libvlc_log_set_file( libvlc_instance_t *p_instance, FILE *stream );

/** @} */

/**
 * Description of a module.
 */
typedef struct libvlc_module_description_t
{
    char *psz_name;
    char *psz_shortname;
    char *psz_longname;
    char *psz_help;
    struct libvlc_module_description_t *p_next;
} libvlc_module_description_t;

/**
 * Release a list of module descriptions.
 *
 * \param p_list the list to be released
 */
LIBVLC_API
void libvlc_module_description_list_release( libvlc_module_description_t *p_list );

/**
 * Returns a list of audio filters that are available.
 *
 * \param p_instance libvlc instance
 *
 * \return a list of module descriptions. It should be freed with libvlc_module_description_list_release().
 *         In case of an error, NULL is returned.
 *
 * \see libvlc_module_description_t
 * \see libvlc_module_description_list_release
 */
LIBVLC_API
libvlc_module_description_t *libvlc_audio_filter_list_get( libvlc_instance_t *p_instance );

/**
 * Returns a list of video filters that are available.
 *
 * \param p_instance libvlc instance
 *
 * \return a list of module descriptions. It should be freed with libvlc_module_description_list_release().
 *         In case of an error, NULL is returned.
 *
 * \see libvlc_module_description_t
 * \see libvlc_module_description_list_release
 */
LIBVLC_API
libvlc_module_description_t *libvlc_video_filter_list_get( libvlc_instance_t *p_instance );

/** @} */

/** \defgroup libvlc_clock LibVLC time
 * These functions provide access to the LibVLC time/clock.
 * @{
 */

/**
 * Return the current time as defined by LibVLC. The unit is the microsecond.
 * Time increases monotonically (regardless of time zone changes and RTC
 * adjustements).
 * The origin is arbitrary but consistent across the whole system
 * (e.g. the system uptim, the time since the system was booted).
 * \note On systems that support it, the POSIX monotonic clock is used.
 */
LIBVLC_API
int64_t libvlc_clock(void);

/**
 * Return the delay (in microseconds) until a certain timestamp.
 * \param pts timestamp
 * \return negative if timestamp is in the past,
 * positive if it is in the future
 */
static inline int64_t libvlc_delay(int64_t pts)
{
    return pts - libvlc_clock();
}

/** @} */

# ifdef __cplusplus
}
# endif

#endif /** @} */
