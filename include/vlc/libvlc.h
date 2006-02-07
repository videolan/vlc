/*****************************************************************************
 * libvlc.h:  libvlc_* new external API
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id: vlc.h 13701 2005-12-12 17:58:56Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * \defgroup libvlc Libvlc
 * This is libvlc, the base library of the VLC program.
 *
 * @{
 */


#ifndef _LIBVLC_H
#define _LIBVLC_H 1

#include <vlc/vlc.h>

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Exception handling
 *****************************************************************************/
/** defgroup libvlc_exception Exceptions
 * \ingroup libvlc
 * LibVLC Exceptions handling
 * @{
 */

struct libvlc_exception_t
{
    int b_raised;
    char *psz_message;
};
typedef struct libvlc_exception_t libvlc_exception_t;

/**
 * Initialize an exception structure. This can be called several times to reuse
 * an exception structure.
 * \param p_exception the exception to initialize
 */
void libvlc_exception_init( libvlc_exception_t *p_exception );

/**
 * Has an exception been raised ?
 * \param p_exception the exception to query
 * \return 0 if no exception raised, 1 else
 */
int libvlc_exception_raised( libvlc_exception_t *p_exception );

/**
 * Raise an exception
 * \param p_exception the exception to raise
 * \param psz_message the exception message
 */
void libvlc_exception_raise( libvlc_exception_t *p_exception, char *psz_message );

/**
 * Clear an exception object so it can be reused.
 * The exception object must be initialized
 * \param p_exception the exception to clear
 */
void libvlc_exception_clear( libvlc_exception_t * );

/**
 * Get exception message
 * \param p_exception the exception to query
 * \return the exception message or NULL if not applicable (exception not raised
 * for example)
 */
char* libvlc_exception_get_message( libvlc_exception_t *p_exception );



/** @} */

/*****************************************************************************
 * Core handling
 *****************************************************************************/

/** defgroup libvlc_core Core
 * \ingroup libvlc
 * LibVLC Core
 * @{
 */

/** This structure is opaque. It represents a libvlc instance */
typedef struct libvlc_instance_t libvlc_instance_t;

/**
 * Create an initialized libvlc instance
 * \param argc the number of arguments
 * \param argv command-line-type arguments
 * \param exception an initialized exception pointer
 */
libvlc_instance_t * libvlc_new( int , char **, libvlc_exception_t *);

/**
 * Destroy a libvlc instance
 * \param p_instance the instance to destroy
 */
void libvlc_destroy( libvlc_instance_t *);

/** @}*/

/*****************************************************************************
 * Playlist
 *****************************************************************************/
/** defgroup libvlc_playlist Playlist
 * \ingroup libvlc
 * LibVLC Playlist handling
 * @{
 */

/**
 * Start playing. You can give some additionnal playlist item options
 * that will be added to the item before playing it.
 * \param p_instance the instance
 * \param i_id the item to play. If this is a negative number, the next
 * item will be selected. Else, the item with the given ID will be played
 * \param i_options the number of options to add to the item
 * \param ppsz_options the options to add to the item
 * \param p_exception an initialized exception
 */
void libvlc_playlist_play( libvlc_instance_t*, int, int, char **,
                           libvlc_exception_t * );

/**
 * Stop playing
 * \param p_instance the instance to stop
 * \param p_exception an initialized exception
 */
void libvlc_playlist_stop( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Remove all playlist ites
 * \param p_instance the instance
 * \param p_exception an initialized exception
 */
void libvlc_playlist_clear( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Go to next playlist item
 * \param p_instance the instance
 * \param p_exception an initialized exception
 */
void libvlc_playlist_next( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Go to Previous playlist item
 * \param p_instance the instance
 * \param p_exception an initialized exception
 */
void libvlc_playlist_prev( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Add an item at the end of the playlist
 * If you need more advanced options, \see libvlc_playlist_add_extended
 * \param p_instance the instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \return the identifier of the new item
 */
int libvlc_playlist_add( libvlc_instance_t *, const char *, const char *,
                         libvlc_exception_t * );

/**
 * Add an item at the end of the playlist, with additional input options
 * \param p_instance the instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \param i_options the number of options to add
 * \param ppsz_options strings representing the options to add
 * \return the identifier of the new item
 */
int libvlc_playlist_add_extended( libvlc_instance_t *, const char *,
                                  const char *, int, const char **,
                                  libvlc_exception_t * );




typedef struct libvlc_input_t libvlc_input_t;

///\todo document me
libvlc_input_t *libvlc_playlist_get_input( libvlc_instance_t *,
                                           libvlc_exception_t * );



/** @}*/

/*****************************************************************************
 * Input
 *****************************************************************************/
/** defgroup libvlc_input Input
 * \ingroup libvlc
 * LibVLC Input handling
 * @{
 */

/** Free an input object
 * \param p_input the input to free
 */
void libvlc_input_free( libvlc_input_t * );

/// \bug This might go away ... to be replaced by a broader system
vlc_int64_t libvlc_input_get_length( libvlc_input_t *, libvlc_exception_t *);
vlc_int64_t libvlc_input_get_time( libvlc_input_t *, libvlc_exception_t *);
float libvlc_input_get_position( libvlc_input_t *, libvlc_exception_t *);


void libvlc_toggle_fullscreen( libvlc_input_t *, libvlc_exception_t * );


/** @} */



# ifdef __cplusplus
}
# endif

#endif /* <vlc/vlc_control.h> */
