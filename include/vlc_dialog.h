/*****************************************************************************
 * vlc_dialog.h: user interaction dialogs
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
 * Copyright (C) 2016 VLC authors and VideoLAN
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

#ifndef VLC_DIALOG_H_
#define VLC_DIALOG_H_
# include <stdarg.h>

typedef struct vlc_dialog_provider vlc_dialog_provider;
typedef struct vlc_dialog_id vlc_dialog_id;
typedef struct extension_dialog_t extension_dialog_t;

/* Called from src/libvlc.c */
int
libvlc_InternalDialogInit(libvlc_int_t *p_libvlc);

/* Called from src/libvlc.c */
void
libvlc_InternalDialogClean(libvlc_int_t *p_libvlc);

/**
 * @defgroup vlc_dialog VLC dialog
 * @ingroup interface
 * @{
 * @file
 * This file declares VLC dialog functions
 * @defgroup vlc_dialog_api VLC dialog functions
 * In order to interact with the user
 * @{
 */

/**
 * Dialog question type, see vlc_dialog_wait_question()
 */
typedef enum vlc_dialog_question_type
{
    VLC_DIALOG_QUESTION_NORMAL,
    VLC_DIALOG_QUESTION_WARNING,
    VLC_DIALOG_QUESTION_CRITICAL,
} vlc_dialog_question_type;

/**
 * Sends an error message
 *
 * This function returns immediately
 *
 * @param p_obj the VLC object emitting the error
 * @param psz_title title of the error dialog
 * @param psz_fmt format string for the error message
 * @return VLC_SUCCESS on success, or a VLC error code on error
 */
VLC_API int
vlc_dialog_display_error(vlc_object_t *p_obj, const char *psz_title,
                         const char *psz_fmt, ...) VLC_FORMAT(3,4);
#define vlc_dialog_display_error(a, b, c, ...) \
    vlc_dialog_display_error(VLC_OBJECT(a), b, c, ##__VA_ARGS__)

/**
 * Sends an error message
 *
 * Equivalent to vlc_dialog_display_error() expect that it's called with a
 * va_list.
 */
VLC_API int
vlc_dialog_display_error_va(vlc_object_t *p_obj, const char *psz_title,
                            const char *psz_fmt, va_list ap);

/**
 * Requests an user name and a password
 *
 * This function waits until the user dismisses the dialog or responds. It's
 * interruptible via vlc_interrupt. In that case, vlc_dialog_cbs.pf_cancel()
 * will be invoked. If p_store is not NULL, the user will be asked to store the
 * password or not.
 *
 * @param p_obj the VLC object emitting the dialog
 * @param ppsz_username a pointer to the user name provided by the user, it
 * must be freed with free() on success
 * @param ppsz_password a pointer to the password provided by the user, it must
 * be freed with free() on success
 * @param p_store a pointer to the store answer provided by the user (optional)
 * @param psz_default_username default user name proposed
 * @param psz_title title of the login dialog
 * @param psz_fmt format string for the login message
 * @return < 0 on error, 0 if the user cancelled it, and 1 if ppsz_username and
 * ppsz_password are valid.
 */
VLC_API int
vlc_dialog_wait_login(vlc_object_t *p_obj, char **ppsz_username,
                      char **ppsz_password, bool *p_store,
                      const char *psz_default_username,
                      const char *psz_title, const char *psz_fmt, ...)
                      VLC_FORMAT(7,8);
#define vlc_dialog_wait_login(a, b, c, d, e, f, g, ...) \
    vlc_dialog_wait_login(VLC_OBJECT(a), b, c, d, e, f, g, ##__VA_ARGS__)

/**
 * Requests an user name and a password
 *
 * Equivalent to vlc_dialog_wait_login() expect that it's called with a
 * va_list.
 */
VLC_API int
vlc_dialog_wait_login_va(vlc_object_t *p_obj, char **ppsz_username,
                         char **ppsz_password, bool *p_store,
                         const char *psz_default_username,
                         const char *psz_title, const char *psz_fmt, va_list ap);

/**
 * Asks a total (Yes/No/Cancel) question
 *
 * This function waits until the user dismisses the dialog or responds. It's
 * interruptible via vlc_interrupt. In that case, vlc_dialog_cbs.pf_cancel()
 * will be invoked. The psz_cancel is mandatory since this dialog is always
 * cancellable by the user.
 *
 * @param p_obj the VLC object emitting the dialog
 * @param i_type question type (severity of the question)
 * @param psz_cancel text of the cancel button
 * @param psz_action1 first choice/button text (optional)
 * @param psz_action2 second choice/button text (optional)
 * @param psz_title title of the question dialog
 * @param psz_fmt format string for the question message
 * @return < 0 on error, 0 if the user cancelled it, 1 on action1, 2 on action2
 */
VLC_API int
vlc_dialog_wait_question(vlc_object_t *p_obj,
                         vlc_dialog_question_type i_type,
                         const char *psz_cancel, const char *psz_action1,
                         const char *psz_action2, const char *psz_title,
                         const char *psz_fmt, ...) VLC_FORMAT(7,8);
#define vlc_dialog_wait_question(a, b, c, d, e, f, g, ...) \
    vlc_dialog_wait_question(VLC_OBJECT(a), b, c, d, e, f, g, ##__VA_ARGS__)

/**
 * Asks a total (Yes/No/Cancel) question
 *
 * Equivalent to vlc_dialog_wait_question() expect that it's called with a
 * va_list.
 */
VLC_API int
vlc_dialog_wait_question_va(vlc_object_t *p_obj,
                            vlc_dialog_question_type i_type,
                            const char *psz_cancel, const char *psz_action1,
                            const char *psz_action2, const char *psz_title,
                            const char *psz_fmt, va_list ap);

/**
 * Display a progress dialog
 *
 * This function returns immediately
 *
 * @param p_obj the VLC object emitting the dialog
 * @param b_indeterminate true if the progress dialog is indeterminate
 * @param f_position initial position of the progress bar (between 0.0 and 1.0)
 * @param psz_cancel text of the cancel button, if NULL the dialog is not
 * cancellable (optional)
 * @param psz_title title of the progress dialog
 * @param psz_fmt format string for the progress message
 * @return a valid vlc_dialog_id on success, must be released with
 * vlc_dialog_id_release()
 */
VLC_API vlc_dialog_id *
vlc_dialog_display_progress(vlc_object_t *p_obj, bool b_indeterminate,
                            float f_position, const char *psz_cancel,
                            const char *psz_title, const char *psz_fmt, ...)
                            VLC_FORMAT(6,7);
#define vlc_dialog_display_progress(a, b, c, d, e, f, ...) \
    vlc_dialog_display_progress(VLC_OBJECT(a), b, c, d, e, f, ##__VA_ARGS__)

/**
 * Display a progress dialog
 *
 * Equivalent to vlc_dialog_display_progress() expect that it's called with a
 * va_list.
 */
VLC_API vlc_dialog_id *
vlc_dialog_display_progress_va(vlc_object_t *p_obj, bool b_indeterminate,
                               float f_position, const char *psz_cancel,
                               const char *psz_title, const char *psz_fmt,
                               va_list ap);

/**
 * Update the position of the progress dialog
 *
 * @param p_obj the VLC object emitting the dialog
 * @param p_id id of the dialog to update
 * @param f_position position of the progress bar (between 0.0 and 1.0)
 * @return VLC_SUCCESS on success, or a VLC error code on error
 */
VLC_API int
vlc_dialog_update_progress(vlc_object_t *p_obj, vlc_dialog_id *p_id,
                           float f_position);
#define vlc_dialog_update_progress(a, b, c) \
    vlc_dialog_update_progress(VLC_OBJECT(a), b, c)

/**
 * Update the position and the message of the progress dialog
 *
 * @param p_obj the VLC object emitting the dialog
 * @param p_id id of the dialog to update
 * @param f_position position of the progress bar (between 0.0 and 1.0)
 * @param psz_fmt format string for the progress message
 * @return VLC_SUCCESS on success, or a VLC error code on error
 */
VLC_API int
vlc_dialog_update_progress_text(vlc_object_t *p_obj, vlc_dialog_id *p_id,
                                float f_position, const char *psz_fmt, ...)
                                VLC_FORMAT(4, 5);
#define vlc_dialog_update_progress_text(a, b, c, d, ...) \
    vlc_dialog_update_progress_text(VLC_OBJECT(a), b, c, d, ##__VA_ARGS__)

/**
 * Update the position and the message of the progress dialog
 *
 * Equivalent to vlc_dialog_update_progress_text() expect that it's called
 * with a va_list.
 */
VLC_API int
vlc_dialog_update_progress_text_va(vlc_object_t *p_obj, vlc_dialog_id *p_id,
                                   float f_position, const char *psz_fmt,
                                   va_list ap);

/**
 * Release the dialog id returned by vlc_dialog_display_progress()
 *
 * It causes the vlc_dialog_cbs.pf_cancel() callback to be invoked.
 *
 * @param p_obj the VLC object emitting the dialog
 * @param p_id id of the dialog to release
 */
VLC_API void
vlc_dialog_release(vlc_object_t *p_obj, vlc_dialog_id *p_id);
#define vlc_dialog_release(a, b) \
    vlc_dialog_release(VLC_OBJECT(a), b)

/**
 * Return true if the dialog id is cancelled
 *
 * @param p_obj the VLC object emitting the dialog
 * @param p_id id of the dialog
 */
VLC_API bool
vlc_dialog_is_cancelled(vlc_object_t *p_obj, vlc_dialog_id *p_id);
#define vlc_dialog_is_cancelled(a, b) \
    vlc_dialog_is_cancelled(VLC_OBJECT(a), b)

/**
 * @}
 * @defgroup vlc_dialog_impl VLC dialog callbacks
 * Need to be implemented by GUI modules or libvlc
 * @{
 */

/**
 * Dialog callbacks to be implemented
 */
typedef struct vlc_dialog_cbs
{
    /**
     * Called when an error message needs to be displayed
     *
     * @param p_data opaque pointer for the callback
     * @param psz_title title of the dialog
     * @param psz_text text of the dialog
     */
    void (*pf_display_error)(void *p_data, const char *psz_title,
                             const char *psz_text);

    /**
     * Called when a login dialog needs to be displayed
     *
     * You can interact with this dialog by calling vlc_dialog_id_post_login()
     * to post an answer or vlc_dialog_id_dismiss() to cancel this dialog.
     *
     * @note to receive this callback, vlc_dialog_cbs.pf_cancel should not be
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
    void (*pf_display_login)(void *p_data, vlc_dialog_id *p_id,
                             const char *psz_title, const char *psz_text,
                             const char *psz_default_username,
                             bool b_ask_store);

    /**
     * Called when a question dialog needs to be displayed
     *
     * You can interact with this dialog by calling vlc_dialog_id_post_action()
     * to post an answer or vlc_dialog_id_dismiss() to cancel this dialog.
     *
     * @note to receive this callback, vlc_dialog_cbs.pf_cancel should not be
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
    void (*pf_display_question)(void *p_data, vlc_dialog_id *p_id,
                                const char *psz_title, const char *psz_text,
                                vlc_dialog_question_type i_type,
                                const char *psz_cancel, const char *psz_action1,
                                const char *psz_action2);

    /**
     * Called when a progress dialog needs to be displayed
     *
     * If cancellable (psz_cancel != NULL), you can cancel this dialog by
     * calling vlc_dialog_id_dismiss()
     *
     * @note to receive this callback, vlc_dialog_cbs.pf_cancel and
     * vlc_dialog_cbs.pf_update_progress should not be NULL.
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
    void (*pf_display_progress)(void *p_data, vlc_dialog_id *p_id,
                                const char *psz_title, const char *psz_text,
                                bool b_indeterminate, float f_position,
                                const char *psz_cancel);

    /**
     * Called when a displayed dialog needs to be cancelled
     *
     * The implementation must call vlc_dialog_id_dismiss() to really release
     * the dialog.
     *
     * @param p_data opaque pointer for the callback
     * @param p_id id of the dialog
     */
    void (*pf_cancel)(void *p_data, vlc_dialog_id *p_id);

    /**
     * Called when a progress dialog needs to be updated
     *
     * @param p_data opaque pointer for the callback
     * @param p_id id of the dialog
     * @param f_position osition of the progress bar (between 0.0 and 1.0)
     * @param psz_text new text of the progress dialog
     */
    void (*pf_update_progress)(void *p_data, vlc_dialog_id *p_id,
                               float f_position, const char *psz_text);
} vlc_dialog_cbs;

/**
 * Register callbacks to handle VLC dialogs
 *
 * @param p_cbs a pointer to callbacks, or NULL to unregister callbacks.
 * @param p_data opaque pointer for the callback
 */
VLC_API void
vlc_dialog_provider_set_callbacks(vlc_object_t *p_obj,
                                  const vlc_dialog_cbs *p_cbs, void *p_data);
#define vlc_dialog_provider_set_callbacks(a, b, c) \
    vlc_dialog_provider_set_callbacks(VLC_OBJECT(a), b, c)

/**
 * Associate an opaque pointer with the dialog id
 */
VLC_API void
vlc_dialog_id_set_context(vlc_dialog_id *p_id, void *p_context);

/**
 * Return the opaque pointer associated with the dialog id
 */
VLC_API void *
vlc_dialog_id_get_context(vlc_dialog_id *p_id);

/**
 * Post a login answer
 *
 * After this call, p_id won't be valid anymore
 *
 * @see vlc_dialog_cbs.pf_display_login
 *
 * @param p_id id of the dialog
 * @param psz_username valid and non empty string
 * @param psz_password valid string (can be empty)
 * @param b_store if true, store the credentials
 * @return VLC_SUCCESS on success, or a VLC error code on error
 */
VLC_API int
vlc_dialog_id_post_login(vlc_dialog_id *p_id, const char *psz_username,
                         const char *psz_password, bool b_store);

/**
 * Post a question answer
 *
 * After this call, p_id won't be valid anymore
 *
 * @see vlc_dialog_cbs.pf_display_question
 *
 * @param p_id id of the dialog
 * @param i_action 1 for action1, 2 for action2
 * @return VLC_SUCCESS on success, or a VLC error code on error
 */
VLC_API int
vlc_dialog_id_post_action(vlc_dialog_id *p_id, int i_action);

/**
 * Dismiss a dialog
 *
 * After this call, p_id won't be valid anymore
 *
 * @see vlc_dialog_cbs.pf_cancel
 *
 * @param p_id id of the dialog
 * @return VLC_SUCCESS on success, or a VLC error code on error
 */
VLC_API int
vlc_dialog_id_dismiss(vlc_dialog_id *p_id);

/**
 * @}
 * @defgroup vlc_dialog_ext VLC extension dialog functions
 * @{
 */

VLC_API int
vlc_ext_dialog_update(vlc_object_t *p_obj, extension_dialog_t *dialog);
#define vlc_ext_dialog_update(a, b) \
    vlc_ext_dialog_update(VLC_OBJECT(a), b)

/**
 * Dialog extension callback to be implemented
 */
typedef void (*vlc_dialog_ext_update_cb)(extension_dialog_t *p_ext_dialog,
                                         void *p_data);

/**
 * Register a callback for VLC extension dialog
 *
 * @param pf_update a pointer to the update callback, or NULL to unregister
 * callback
 * @param p_data opaque pointer for the callback
 */
VLC_API void
vlc_dialog_provider_set_ext_callback(vlc_object_t *p_obj,
                                     vlc_dialog_ext_update_cb pf_update,
                                     void *p_data);
#define vlc_dialog_provider_set_ext_callback(a, b, c) \
    vlc_dialog_provider_set_ext_callback(VLC_OBJECT(a), b, c)

/** @} @} */

#endif
