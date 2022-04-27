/*****************************************************************************
 * libvlc_dialog.h:  libvlc dialog API
 *****************************************************************************
 * Copyright © 2016 VLC authors and VideoLAN
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

#ifndef LIBVLC_DIALOG_H
#define LIBVLC_DIALOG_H 1

#include <stdbool.h>

# ifdef __cplusplus
extern "C" {
# endif

typedef struct libvlc_dialog_id libvlc_dialog_id;

/**
 * @defgroup libvlc_dialog LibVLC dialog
 * @ingroup libvlc
 * @{
 * @file
 * LibVLC dialog external API
 */

typedef enum libvlc_dialog_question_type
{
    LIBVLC_DIALOG_QUESTION_NORMAL,
    LIBVLC_DIALOG_QUESTION_WARNING,
    LIBVLC_DIALOG_QUESTION_CRITICAL,
} libvlc_dialog_question_type;

/**
 * Dialog callbacks to be implemented
 *
 * @attention starting with vlc 4.0.0 the error callback (pf_display_error) is
 *            no longer part of this struct and need to be registered separately
 *            using @a libvlc_dialog_set_error_callback
 *
 * @see libvlc_dialog_set_error_callback
 */
typedef struct libvlc_dialog_cbs
{
    /**
     * Called when a login dialog needs to be displayed
     *
     * You can interact with this dialog by calling libvlc_dialog_post_login()
     * to post an answer or libvlc_dialog_dismiss() to cancel this dialog.
     *
     * @note to receive this callback, libvlc_dialog_cbs.pf_cancel should not be
     * NULL.
     *
     * @param p_data opaque pointer for the callback
     * @param p_id id used to interact with the dialog
     * @param psz_title title of the dialog
     * @param psz_text text of the dialog
     * @param psz_default_username user name that should be set on the user form
     * @param b_ask_store if true, ask the user if he wants to save the
     * credentials
     */
    void (*pf_display_login)(void *p_data, libvlc_dialog_id *p_id,
                             const char *psz_title, const char *psz_text,
                             const char *psz_default_username,
                             bool b_ask_store);

    /**
     * Called when a question dialog needs to be displayed
     *
     * You can interact with this dialog by calling libvlc_dialog_post_action()
     * to post an answer or libvlc_dialog_dismiss() to cancel this dialog.
     *
     * @note to receive this callback, libvlc_dialog_cbs.pf_cancel should not be
     * NULL.
     *
     * @param p_data opaque pointer for the callback
     * @param p_id id used to interact with the dialog
     * @param psz_title title of the dialog
     * @param psz_text text of the dialog
     * @param i_type question type (or severity) of the dialog
     * @param psz_cancel text of the cancel button
     * @param psz_action1 text of the first button, if NULL, don't display this
     * button
     * @param psz_action2 text of the second button, if NULL, don't display
     * this button
     */
    void (*pf_display_question)(void *p_data, libvlc_dialog_id *p_id,
                                const char *psz_title, const char *psz_text,
                                libvlc_dialog_question_type i_type,
                                const char *psz_cancel, const char *psz_action1,
                                const char *psz_action2);

    /**
     * Called when a progress dialog needs to be displayed
     *
     * If cancellable (psz_cancel != NULL), you can cancel this dialog by
     * calling libvlc_dialog_dismiss()
     *
     * @note to receive this callback, libvlc_dialog_cbs.pf_cancel and
     * libvlc_dialog_cbs.pf_update_progress should not be NULL.
     *
     * @param p_data opaque pointer for the callback
     * @param p_id id used to interact with the dialog
     * @param psz_title title of the dialog
     * @param psz_text text of the dialog
     * @param b_indeterminate true if the progress dialog is indeterminate
     * @param f_position initial position of the progress bar (between 0.0 and
     * 1.0)
     * @param psz_cancel text of the cancel button, if NULL the dialog is not
     * cancellable
     */
    void (*pf_display_progress)(void *p_data, libvlc_dialog_id *p_id,
                                const char *psz_title, const char *psz_text,
                                bool b_indeterminate, float f_position,
                                const char *psz_cancel);

    /**
     * Called when a displayed dialog needs to be cancelled
     *
     * The implementation must call libvlc_dialog_dismiss() to really release
     * the dialog.
     *
     * @param p_data opaque pointer for the callback
     * @param p_id id of the dialog
     */
    void (*pf_cancel)(void *p_data, libvlc_dialog_id *p_id);

    /**
     * Called when a progress dialog needs to be updated
     *
     * @param p_data opaque pointer for the callback
     * @param p_id id of the dialog
     * @param f_position osition of the progress bar (between 0.0 and 1.0)
     * @param psz_text new text of the progress dialog
     */
    void (*pf_update_progress)(void *p_data, libvlc_dialog_id *p_id,
                               float f_position, const char *psz_text);
} libvlc_dialog_cbs;


/**
 * Called when an error message needs to be displayed
 *
 * @param p_data opaque pointer for the callback
 * @param psz_title title of the dialog
 * @param psz_text text of the dialog
 */
typedef void (*libvlc_dialog_error_cbs)(void *p_data, const char *psz_title, const char *psz_text);

/**
 * Register callbacks in order to handle VLC dialogs
 *
 * @version LibVLC 3.0.0 and later.
 *
 * @param p_cbs a pointer to callbacks, or NULL to unregister callbacks.
 * @param p_data opaque pointer for the callback
 */
LIBVLC_API void
libvlc_dialog_set_callbacks(libvlc_instance_t *p_instance,
                            const libvlc_dialog_cbs *p_cbs, void *p_data);

/*
* Register callback in order to handle VLC error messages
*
* @version LibVLC 4.0.0 and later.
*
* @param p_cbs a pointer to callback, or NULL to unregister callback.
* @param p_data opaque pointer for the callback
*/
LIBVLC_API void
libvlc_dialog_set_error_callback(libvlc_instance_t *p_instance,
                                 libvlc_dialog_error_cbs p_cbs, void *p_data);

/**
 * Associate an opaque pointer with the dialog id
 *
 * @version LibVLC 3.0.0 and later.
 */
LIBVLC_API void
libvlc_dialog_set_context(libvlc_dialog_id *p_id, void *p_context);

/**
 * Return the opaque pointer associated with the dialog id
 * \see libvlc_dialog_set_context
 * @version LibVLC 3.0.0 and later.
 */
LIBVLC_API void *
libvlc_dialog_get_context(libvlc_dialog_id *p_id);

/**
 * Post a login answer
 *
 * After this call, p_id won't be valid anymore
 *
 * @see libvlc_dialog_cbs.pf_display_login
 *
 * @version LibVLC 3.0.0 and later.
 *
 * @param p_id id of the dialog
 * @param psz_username valid and non empty string
 * @param psz_password valid string (can be empty)
 * @param b_store if true, store the credentials
 * @return 0 on success, or -1 on error
 */
LIBVLC_API int
libvlc_dialog_post_login(libvlc_dialog_id *p_id, const char *psz_username,
                         const char *psz_password, bool b_store);

/**
 * Post a question answer
 *
 * After this call, p_id won't be valid anymore
 *
 * @see libvlc_dialog_cbs.pf_display_question
 *
 * @version LibVLC 3.0.0 and later.
 *
 * @param p_id id of the dialog
 * @param i_action 1 for action1, 2 for action2
 * @return 0 on success, or -1 on error
 */
LIBVLC_API int
libvlc_dialog_post_action(libvlc_dialog_id *p_id, int i_action);

/**
 * Dismiss a dialog
 *
 * After this call, p_id won't be valid anymore
 *
 * @see libvlc_dialog_cbs.pf_cancel
 *
 * @version LibVLC 3.0.0 and later.
 *
 * @param p_id id of the dialog
 * @return 0 on success, or -1 on error
 */
LIBVLC_API int
libvlc_dialog_dismiss(libvlc_dialog_id *p_id);

/** @} */

# ifdef __cplusplus
}
# endif

#endif /* LIBVLC_DIALOG_H */
