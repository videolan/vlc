/*****************************************************************************
 * libvlc.h:  libvlc external API
 *****************************************************************************
 * Copyright (C) 1998-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

/**
 * \file
 * This file defines libvlc external API
 */

/**
 * \defgroup libvlc libvlc
 * This is libvlc, the base library of the VLC program.
 *
 * @{
 */

#ifndef VLC_LIBVLC_H
#define VLC_LIBVLC_H 1

#if defined (WIN32) && defined (DLL_EXPORT)
# define VLC_PUBLIC_API __declspec(dllexport)
#else
# define VLC_PUBLIC_API
#endif

#ifdef __LIBVLC__
/* Avoid unuseful warnings from libvlc with our deprecated APIs */
#   define VLC_DEPRECATED_API VLC_PUBLIC_API
#elif defined(__GNUC__) && \
      (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 0)
# define VLC_DEPRECATED_API VLC_PUBLIC_API __attribute__((deprecated))
#else
# define VLC_DEPRECATED_API VLC_PUBLIC_API
#endif

# ifdef __cplusplus
extern "C" {
# endif

#include <stdarg.h>
#include <vlc/libvlc_structures.h>

/*****************************************************************************
 * Exception handling
 *****************************************************************************/
/** \defgroup libvlc_exception libvlc_exception
 * \ingroup libvlc_core
 * LibVLC Exceptions handling
 * @{
 */

/**
 * Initialize an exception structure. This can be called several times to
 * reuse an exception structure.
 *
 * \param p_exception the exception to initialize
 */
VLC_PUBLIC_API void libvlc_exception_init( libvlc_exception_t *p_exception );

/**
 * Has an exception been raised?
 *
 * \param p_exception the exception to query
 * \return 0 if the exception was raised, 1 otherwise
 */
VLC_PUBLIC_API int
libvlc_exception_raised( const libvlc_exception_t *p_exception );

/**
 * Raise an exception using a user-provided message.
 *
 * \param p_exception the exception to raise
 * \param psz_format the exception message format string
 * \param ... the format string arguments
 */
VLC_PUBLIC_API void
libvlc_exception_raise( libvlc_exception_t *p_exception,
                        const char *psz_format, ... );

/**
 * Clear an exception object so it can be reused.
 * The exception object must have be initialized.
 *
 * \param p_exception the exception to clear
 */
VLC_PUBLIC_API void libvlc_exception_clear( libvlc_exception_t * );

/**@} */

/*****************************************************************************
 * Error handling
 *****************************************************************************/
/** \defgroup libvlc_error libvlc_error
 * \ingroup libvlc_core
 * LibVLC error handling
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
VLC_PUBLIC_API const char *libvlc_errmsg (void);

/**
 * Clears the LibVLC error status for the current thread. This is optional.
 * By default, the error status is automatically overriden when a new error
 * occurs, and destroyed when the thread exits.
 */
VLC_PUBLIC_API void libvlc_clearerr (void);

/**
 * Sets the LibVLC error status and message for the current thread.
 * Any previous error is overriden.
 * @return a nul terminated string in any case
 */
const char *libvlc_vprinterr (const char *fmt, va_list ap);

/**
 * Sets the LibVLC error status and message for the current thread.
 * Any previous error is overriden.
 * @return a nul terminated string in any case
 */
const char *libvlc_printerr (const char *fmt, ...);

/**@} */


/*****************************************************************************
 * Core handling
 *****************************************************************************/

/** \defgroup libvlc_core libvlc_core
 * \ingroup libvlc
 * LibVLC Core
 * @{
 */

/**
 * Create and initialize a libvlc instance.
 *
 * \param argc the number of arguments
 * \param argv command-line-type arguments. argv[0] must be the path of the
 *        calling program.
 * \param p_e an initialized exception pointer
 * \return the libvlc instance
 */
VLC_PUBLIC_API libvlc_instance_t *
libvlc_new( int , const char *const *, libvlc_exception_t *);

/**
 * Decrement the reference count of a libvlc instance, and destroy it
 * if it reaches zero.
 *
 * \param p_instance the instance to destroy
 */
VLC_PUBLIC_API void libvlc_release( libvlc_instance_t * );

/**
 * Increments the reference count of a libvlc instance.
 * The initial reference count is 1 after libvlc_new() returns.
 *
 * \param p_instance the instance to reference
 */
VLC_PUBLIC_API void libvlc_retain( libvlc_instance_t * );

/**
 * Try to start a user interface for the libvlc instance.
 *
 * \param p_instance the instance
 * \param name interface name, or NULL for default
 * \param p_exception an initialized exception pointer
 * \return 0 on success, -1 on error.
 */
VLC_PUBLIC_API
int libvlc_add_intf( libvlc_instance_t *p_instance, const char *name,
                     libvlc_exception_t *p_exception );

/**
 * Waits until an interface causes the instance to exit.
 * You should start at least one interface first, using libvlc_add_intf().
 *
 * \param p_instance the instance
 */
VLC_PUBLIC_API
void libvlc_wait( libvlc_instance_t *p_instance );

/**
 * Retrieve libvlc version.
 *
 * Example: "0.9.0-git Grishenko"
 *
 * \return a string containing the libvlc version
 */
VLC_PUBLIC_API const char * libvlc_get_version(void);

/**
 * Retrieve libvlc compiler version.
 *
 * Example: "gcc version 4.2.3 (Ubuntu 4.2.3-2ubuntu6)"
 *
 * \return a string containing the libvlc compiler version
 */
VLC_PUBLIC_API const char * libvlc_get_compiler(void);

/**
 * Retrieve libvlc changeset.
 *
 * Example: "aa9bce0bc4"
 *
 * \return a string containing the libvlc changeset
 */
VLC_PUBLIC_API const char * libvlc_get_changeset(void);

struct vlc_object_t;

/**
 * Get the internal main VLC object.
 * Use of this function is usually a hack and should be avoided.
 * @note
 * You will need to link with libvlccore to make any use of the underlying VLC
 * object. The libvlccore programming and binary interfaces are not stable.
 * @warning
 * Remember to release the object with vlc_object_release().
 *
 * \param p_instance the libvlc instance
 * @return a VLC object of type "libvlc"
 */
VLC_PUBLIC_API struct vlc_object_t *libvlc_get_vlc_instance(libvlc_instance_t *p_instance);

/**
 * Frees an heap allocation (char *) returned by a LibVLC API.
 * If you know you're using the same underlying C run-time as the LibVLC
 * implementation, then you can call ANSI C free() directly instead.
 */
VLC_PUBLIC_API void libvlc_free( void *ptr );

/** @}*/

/*****************************************************************************
 * Event handling
 *****************************************************************************/

/** \defgroup libvlc_event libvlc_event
 * \ingroup libvlc_core
 * LibVLC Events
 * @{
 */

/**
 * Event manager that belongs to a libvlc object, and from whom events can
 * be received.
 */

typedef struct libvlc_event_manager_t libvlc_event_manager_t;
typedef struct libvlc_event_t libvlc_event_t;
typedef uint32_t libvlc_event_type_t;
    
/**
 * Callback function notification
 * \param p_event the event triggering the callback
 */

typedef void ( *libvlc_callback_t )( const libvlc_event_t *, void * );
    
/**
 * Register for an event notification.
 *
 * \param p_event_manager the event manager to which you want to attach to.
 *        Generally it is obtained by vlc_my_object_event_manager() where
 *        my_object is the object you want to listen to.
 * \param i_event_type the desired event to which we want to listen
 * \param f_callback the function to call when i_event_type occurs
 * \param user_data user provided data to carry with the event
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_event_attach( libvlc_event_manager_t *p_event_manager,
                                         libvlc_event_type_t i_event_type,
                                         libvlc_callback_t f_callback,
                                         void *user_data,
                                         libvlc_exception_t *p_e );

/**
 * Unregister an event notification.
 *
 * \param p_event_manager the event manager
 * \param i_event_type the desired event to which we want to unregister
 * \param f_callback the function to call when i_event_type occurs
 * \param p_user_data user provided data to carry with the event
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_event_detach( libvlc_event_manager_t *p_event_manager,
                                         libvlc_event_type_t i_event_type,
                                         libvlc_callback_t f_callback,
                                         void *p_user_data,
                                         libvlc_exception_t *p_e );

/**
 * Get an event's type name.
 *
 * \param i_event_type the desired event
 */
VLC_PUBLIC_API const char * libvlc_event_type_name( libvlc_event_type_t event_type );

/** @} */

/*****************************************************************************
 * Message log handling
 *****************************************************************************/

/** \defgroup libvlc_log libvlc_log
 * \ingroup libvlc_core
 * LibVLC Message Logging
 * @{
 */

/**
 * Return the VLC messaging verbosity level.
 *
 * \param p_instance libvlc instance
 * \return verbosity level for messages
 */
VLC_PUBLIC_API unsigned libvlc_get_log_verbosity( const libvlc_instance_t *p_instance );

/**
 * Set the VLC messaging verbosity level.
 *
 * \param p_instance libvlc log instance
 * \param level log level
 */
VLC_PUBLIC_API void libvlc_set_log_verbosity( libvlc_instance_t *p_instance, unsigned level );

/**
 * Open a VLC message log instance.
 *
 * \param p_instance libvlc instance
 * \param p_e an initialized exception pointer
 * \return log message instance
 */
VLC_PUBLIC_API libvlc_log_t *libvlc_log_open( libvlc_instance_t *, libvlc_exception_t *);

/**
 * Close a VLC message log instance.
 *
 * \param p_log libvlc log instance or NULL
 */
VLC_PUBLIC_API void libvlc_log_close( libvlc_log_t *p_log );

/**
 * Returns the number of messages in a log instance.
 *
 * \param p_log libvlc log instance or NULL
 * \param p_e an initialized exception pointer
 * \return number of log messages, 0 if p_log is NULL
 */
VLC_PUBLIC_API unsigned libvlc_log_count( const libvlc_log_t *p_log );

/**
 * Clear a log instance.
 *
 * All messages in the log are removed. The log should be cleared on a
 * regular basis to avoid clogging.
 *
 * \param p_log libvlc log instance or NULL
 */
VLC_PUBLIC_API void libvlc_log_clear( libvlc_log_t *p_log );

/**
 * Allocate and returns a new iterator to messages in log.
 *
 * \param p_log libvlc log instance
 * \param p_e an initialized exception pointer
 * \return log iterator object
 */
VLC_PUBLIC_API libvlc_log_iterator_t *libvlc_log_get_iterator( const libvlc_log_t *, libvlc_exception_t *);

/**
 * Release a previoulsy allocated iterator.
 *
 * \param p_iter libvlc log iterator or NULL
 */
VLC_PUBLIC_API void libvlc_log_iterator_free( libvlc_log_iterator_t *p_iter );

/**
 * Return whether log iterator has more messages.
 *
 * \param p_iter libvlc log iterator or NULL
 * \return true if iterator has more message objects, else false
 */
VLC_PUBLIC_API int libvlc_log_iterator_has_next( const libvlc_log_iterator_t *p_iter );

/**
 * Return the next log message.
 *
 * The message contents must not be freed
 *
 * \param p_iter libvlc log iterator or NULL
 * \param p_buffer log buffer
 * \param p_e an initialized exception pointer
 * \return log message object
 */
VLC_PUBLIC_API libvlc_log_message_t *libvlc_log_iterator_next( libvlc_log_iterator_t *p_iter,
                                                               libvlc_log_message_t *p_buffer,
                                                               libvlc_exception_t *p_e );

/** @} */

# ifdef __cplusplus
}
# endif

#endif /* <vlc/libvlc.h> */
