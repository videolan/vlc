/*****************************************************************************
 * dialog.c: User dialog functions
 *****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
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

/** @ingroup vlc_dialog */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdarg.h>

#include <vlc_common.h>
#include <vlc_dialog.h>
#include <vlc_interrupt.h>
#include <vlc_extensions.h>
#include <assert.h>
#include "libvlc.h"

static vlc_mutex_t provider_lock = VLC_STATIC_MUTEX;

#undef dialog_Register
/**
 * Registers an object as the dialog provider.
 * It is assumed that the appropriate variable callbacks are already
 * registered.
 */
int dialog_Register (vlc_object_t *obj)
{
    libvlc_priv_t *priv = libvlc_priv (obj->p_libvlc);
    int ret = VLC_EGENERIC;

    vlc_mutex_lock (&provider_lock);
    if (priv->p_legacy_dialog_provider == NULL)
    {   /* Since the object is responsible for unregistering itself before
         * it terminates, at reference is not needed. */
        priv->p_legacy_dialog_provider = obj;
        ret = VLC_SUCCESS;
    }
    vlc_mutex_unlock (&provider_lock);
    return ret;
}

#undef dialog_Unregister
/**
 * Unregisters the dialog provider.
 * Note that unless you have unregistered the callbacks already, the provider
 * might still be in use by other threads. Also, you need to cancel all
 * pending dialogs yourself.
 */
int dialog_Unregister (vlc_object_t *obj)
{
    libvlc_priv_t *priv = libvlc_priv (obj->p_libvlc);
    int ret = VLC_EGENERIC;

    vlc_mutex_lock (&provider_lock);
    if (priv->p_legacy_dialog_provider == obj)
    {
        priv->p_legacy_dialog_provider = NULL;
        ret = VLC_SUCCESS;
    }
    vlc_mutex_unlock (&provider_lock);
    return ret;
}

static vlc_object_t *dialog_GetProvider (vlc_object_t *obj)
{
    libvlc_priv_t *priv = libvlc_priv (obj->p_libvlc);
    vlc_object_t *provider;

    vlc_mutex_lock (&provider_lock);
    if ((provider = priv->p_legacy_dialog_provider) != NULL)
        vlc_object_hold (provider);
    vlc_mutex_unlock (&provider_lock);
    return provider;
}

/**
 * Sends an error message through the user interface (if any).
 * @param obj the VLC object emitting the error
 * @param modal whether to wait for user to acknowledge the error
 *              before returning control to the caller
 * @param title title of the error dialog
 * @param fmt format string for the error message
 * @param ap parameters list for the formatted error message
 */
void dialog_VFatal (vlc_object_t *obj, bool modal, const char *title,
                    const char *fmt, va_list ap)
{
    char *text;

    if (obj->i_flags & OBJECT_FLAGS_NOINTERACT)
        return;

    vlc_object_t *provider = dialog_GetProvider (obj);
    if (provider == NULL)
    {
        msg_Err (obj, "%s", title);
        msg_GenericVa (obj, VLC_MSG_ERR, fmt, ap);
        return;
    }

    if (vasprintf (&text, fmt, ap) != -1)
    {
        dialog_fatal_t dialog = { title, text, };
        var_SetAddress (provider,
                        modal ? "dialog-critical" : "dialog-error", &dialog);
        free (text);
    }
    vlc_object_release (provider);
}

#undef dialog_vaLogin
void dialog_vaLogin (vlc_object_t *obj, const char *default_username,
                   char **username, char **password, bool *store,
                   const char *title, const char *fmt, va_list ap)
{
    assert ((username != NULL) && (password != NULL));

    *username = *password = NULL;
    if (obj->i_flags & OBJECT_FLAGS_NOINTERACT)
        return;

    vlc_object_t *provider = dialog_GetProvider (obj);
    if (provider == NULL)
        return;

    char *text;

    if (vasprintf (&text, fmt, ap) != -1)
    {
        dialog_login_t dialog = { title, text, default_username, username,
                                  password, store };
        var_SetAddress (provider, "dialog-login", &dialog);
        free (text);
    }
    vlc_object_release (provider);
}

#undef dialog_Login
/**
 * Requests a username and password through the user interface.
 * @param obj the VLC object requesting credential information
 * @param username a pointer to the specified username [OUT]
 * @param password a pointer to the specified password [OUT]
 * @param title title for the dialog
 * @param fmt format string for the message in the dialog
 * @return Nothing. If a user name resp. a password was specified,
 * it will be returned as a heap-allocated character array
 * into the username resp password pointer. Those must be freed with free().
 * Otherwise *username resp *password will be NULL.
 */
void dialog_Login (vlc_object_t *obj, const char *default_username,
                   char **username, char **password,
                   bool *store, const char *title, const char *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    dialog_vaLogin (obj, default_username, username, password, store,
                    title, fmt, ap);
    va_end (ap);
}

#undef dialog_Question
/**
 * Asks a total (Yes/No/Cancel) question through the user interface.
 * @param obj VLC object emitting the question
 * @param title dialog box title
 * @param fmt format string for the dialog box text
 * @param yes first choice/button text
 * @param no second choice/button text
 * @param cancel third answer/button text, or NULL if no third option
 * @return 0 if the user could not answer the question (e.g. there is no UI),
 * 1, 2 resp. 3 if the user pressed the first, second resp. third button.
 */
int dialog_Question (vlc_object_t *obj, const char *title, const char *fmt,
                     const char *yes, const char *no, const char *cancel, ...)
{
    if (obj->i_flags & OBJECT_FLAGS_NOINTERACT)
        return 0;

    vlc_object_t *provider = dialog_GetProvider (obj);
    if (provider == NULL)
        return 0;

    char *text;
    va_list ap;
    int answer = 0;

    va_start (ap, cancel);
    if (vasprintf (&text, fmt, ap) != -1)
    {
        dialog_question_t dialog = { title, text, yes, no, cancel, 0, };
        var_SetAddress (provider, "dialog-question", &dialog);
        answer = dialog.answer;
        free (text);
    }
    va_end (ap);
    vlc_object_release (provider);
    return answer;
}

#undef dialog_ProgressCreate
/**
 * Creates a progress bar dialog.
 */
dialog_progress_bar_t *
dialog_ProgressCreate (vlc_object_t *obj, const char *title,
                       const char *message, const char *cancel)
{
    if (obj->i_flags & OBJECT_FLAGS_NOINTERACT)
        return NULL;

    vlc_object_t *provider = dialog_GetProvider (obj);
    if (provider == NULL)
        return NULL;

    dialog_progress_bar_t *dialog = malloc (sizeof (*dialog));
    if (dialog != NULL)
    {
        dialog->title = title;
        dialog->message = message;
        dialog->cancel = cancel;
        var_SetAddress (provider, "dialog-progress-bar", dialog);
#ifndef NDEBUG
        dialog->title = dialog->message = dialog->cancel = NULL;
#endif
        assert (dialog->pf_update);
        assert (dialog->pf_check);
        assert (dialog->pf_destroy);
    }

    /* FIXME: This could conceivably crash if the dialog provider is destroyed
     * before the dialog user. Holding the provider does not help, as it only
     * protects object variable operations. For instance, it does not prevent
     * unloading of the interface plugin. In the short term, the only solution
     * is to not use progress dialog after deinitialization of the interfaces.
     */
    vlc_object_release (provider);
    return dialog;
}

void dialog_ProgressDestroy (dialog_progress_bar_t *dialog)
{
    assert (dialog);

    dialog->pf_destroy (dialog->p_sys);
    free (dialog);
}

void dialog_ProgressSet (dialog_progress_bar_t *dialog, const char *text,
                         float value)
{
    assert (dialog);

    dialog->pf_update (dialog->p_sys, text, value);
}

bool dialog_ProgressCancelled (dialog_progress_bar_t *dialog)
{
    assert (dialog);

    return dialog->pf_check (dialog->p_sys);
}

#undef dialog_ExtensionUpdate
int dialog_ExtensionUpdate (vlc_object_t *obj, extension_dialog_t *dialog)
{
    assert (obj);
    assert (dialog);

    vlc_object_t *dp = dialog_GetProvider(obj);
    if (!dp)
    {
        msg_Warn (obj, "Dialog provider is not set, can't update dialog '%s'",
                  dialog->psz_title);
        return VLC_EGENERIC;
    }

    // Signaling the dialog provider
    int ret = var_SetAddress (dp, "dialog-extension", dialog);

    vlc_object_release (dp);
    return ret;
}

struct vlc_dialog_provider
{
    vlc_mutex_t                 lock;
    vlc_array_t                 dialog_array;
    vlc_dialog_cbs              cbs;
    void *                      p_cbs_data;

    vlc_dialog_ext_update_cb    pf_ext_update;
    void *                      p_ext_data;
};

enum dialog_type
{
    VLC_DIALOG_ERROR,
    VLC_DIALOG_LOGIN,
    VLC_DIALOG_QUESTION,
    VLC_DIALOG_PROGRESS,
};

struct dialog_answer
{
    enum dialog_type i_type;
    union
    {
        struct
        {
            char *psz_username;
            char *psz_password;
            bool b_store;
        } login;
        struct
        {
            int i_action;
        } question;
    } u;
};

struct dialog
{
    enum dialog_type i_type;
    const char *psz_title;
    const char *psz_text;

    union
    {
        struct
        {
            const char *psz_default_username;
            bool b_ask_store;
        } login;
        struct
        {
            vlc_dialog_question_type i_type;
            const char *psz_cancel;
            const char *psz_action1;
            const char *psz_action2;
        } question;
        struct
        {
            bool b_indeterminate;
            float f_position;
            const char *psz_cancel;
        } progress;
    } u;
};

struct vlc_dialog_id
{
    vlc_mutex_t             lock;
    vlc_cond_t              wait;
    enum dialog_type        i_type;
    void *                  p_context;
    int                     i_refcount;
    bool                    b_cancelled;
    bool                    b_answered;
    bool                    b_progress_indeterminate;
    char *                  psz_progress_text;
    struct dialog_answer    answer;
};

struct dialog_i11e_context
{
    vlc_dialog_provider *   p_provider;
    vlc_dialog_id *         p_id;
};

static inline vlc_dialog_provider *
get_dialog_provider(vlc_object_t *p_obj, bool b_check_interact)
{
    if (b_check_interact && p_obj->i_flags & OBJECT_FLAGS_NOINTERACT)
        return NULL;

    vlc_dialog_provider *p_provider =
        libvlc_priv(p_obj->p_libvlc)->p_dialog_provider;
    assert(p_provider != NULL);
    return p_provider;
}

static void
dialog_id_release(vlc_dialog_id *p_id)
{
    if (p_id->answer.i_type == VLC_DIALOG_LOGIN)
    {
        free(p_id->answer.u.login.psz_username);
        free(p_id->answer.u.login.psz_password);
    }
    free(p_id->psz_progress_text);
    vlc_mutex_destroy(&p_id->lock);
    vlc_cond_destroy(&p_id->wait);
    free(p_id);
}

vlc_dialog_provider *
vlc_dialog_provider_new(void)
{
    vlc_dialog_provider *p_provider = malloc(sizeof(*p_provider));
    if( p_provider == NULL )
        return NULL;

    vlc_mutex_init(&p_provider->lock);
    vlc_array_init(&p_provider->dialog_array);

    memset(&p_provider->cbs, 0, sizeof(p_provider->cbs));
    p_provider->p_cbs_data = NULL;

    p_provider->pf_ext_update = NULL;
    p_provider->p_ext_data = NULL;

    return p_provider;
}

static int
dialog_get_idx_locked(vlc_dialog_provider *p_provider, vlc_dialog_id *p_id)
{
    for (int i = 0; i < vlc_array_count(&p_provider->dialog_array); ++i)
    {
        if (p_id == vlc_array_item_at_index(&p_provider->dialog_array, i))
            return i;
    }
    return -1;
}

static void
dialog_cancel_locked(vlc_dialog_provider *p_provider, vlc_dialog_id *p_id)
{
    vlc_mutex_lock(&p_id->lock);
    if (p_id->b_cancelled || p_id->b_answered)
    {
        vlc_mutex_unlock(&p_id->lock);
        return;
    }
    p_id->b_cancelled = true;
    vlc_mutex_unlock(&p_id->lock);

    p_provider->cbs.pf_cancel(p_id, p_provider->p_cbs_data);
}

static vlc_dialog_id *
dialog_add_locked(vlc_dialog_provider *p_provider, enum dialog_type i_type)
{
    vlc_dialog_id *p_id = calloc(1, sizeof(*p_id));

    if (p_id == NULL)
        return NULL;
    vlc_mutex_init(&p_id->lock);
    vlc_cond_init(&p_id->wait);

    p_id->i_type = i_type;
    p_id->i_refcount = 2; /* provider and callbacks */

    vlc_array_append(&p_provider->dialog_array, p_id);
    return p_id;
}

static void
dialog_remove_locked(vlc_dialog_provider *p_provider, vlc_dialog_id *p_id)
{
    int i_array_idx = dialog_get_idx_locked(p_provider, p_id);
    assert(i_array_idx >= 0);

    vlc_array_remove(&p_provider->dialog_array, i_array_idx);

    vlc_mutex_lock(&p_id->lock);
    p_id->i_refcount--;
    if (p_id->i_refcount == 0)
    {
        vlc_mutex_unlock(&p_id->lock);
        dialog_id_release(p_id);
    }
    else
        vlc_mutex_unlock(&p_id->lock);
}

static void
dialog_clear_all_locked(vlc_dialog_provider *p_provider)
{
    for (int i = 0; i < vlc_array_count(&p_provider->dialog_array); ++i)
    {
        vlc_dialog_id *p_id =
            vlc_array_item_at_index(&p_provider->dialog_array, i);
        dialog_cancel_locked(p_provider, p_id);
    }
    vlc_array_clear(&p_provider->dialog_array);
}

void
vlc_dialog_provider_release(vlc_dialog_provider *p_provider)
{
    assert(p_provider != NULL);

    vlc_mutex_lock(&p_provider->lock);
    dialog_clear_all_locked(p_provider);
    vlc_mutex_unlock(&p_provider->lock);

    vlc_mutex_destroy(&p_provider->lock);
    free(p_provider);
}

#undef vlc_dialog_provider_set_callbacks
void
vlc_dialog_provider_set_callbacks(vlc_object_t *p_obj,
                                  const vlc_dialog_cbs *p_cbs, void *p_data)
{
    assert(p_obj != NULL);
    vlc_dialog_provider *p_provider = get_dialog_provider(p_obj, false);

    vlc_mutex_lock(&p_provider->lock);
    dialog_clear_all_locked(p_provider);

    if (p_cbs == NULL)
    {
        memset(&p_provider->cbs, 0, sizeof(p_provider->cbs));
        p_provider->p_cbs_data = NULL;
    }
    else
    {
        p_provider->cbs = *p_cbs;
        p_provider->p_cbs_data = p_data;
    }
    vlc_mutex_unlock(&p_provider->lock);
}

static void
dialog_wait_interrupted(void *p_data)
{
    struct dialog_i11e_context *p_context = p_data;
    vlc_dialog_provider *p_provider = p_context->p_provider;
    vlc_dialog_id *p_id = p_context->p_id;

    vlc_mutex_lock(&p_provider->lock);
    dialog_cancel_locked(p_provider, p_id);
    vlc_mutex_unlock(&p_provider->lock);

    vlc_mutex_lock(&p_id->lock);
    vlc_cond_signal(&p_id->wait);
    vlc_mutex_unlock(&p_id->lock);
}

static int
dialog_wait(vlc_dialog_provider *p_provider, vlc_dialog_id *p_id,
            enum dialog_type i_type, struct dialog_answer *p_answer)
{
    struct dialog_i11e_context context = {
        .p_provider = p_provider,
        .p_id = p_id,
    };
    vlc_interrupt_register(dialog_wait_interrupted, &context);

    vlc_mutex_lock(&p_id->lock);
    /* Wait for the dialog to be dismissed, interrupted or answered */
    while (!p_id->b_cancelled && !p_id->b_answered)
        vlc_cond_wait(&p_id->wait, &p_id->lock);

    int i_ret;
    if (p_id->b_cancelled)
        i_ret = 0;
    else if (p_id->answer.i_type != i_type)
        i_ret = VLC_EGENERIC;
    else
    {
        i_ret = 1;
        memcpy(p_answer, &p_id->answer, sizeof(p_id->answer));
        memset(&p_id->answer, 0, sizeof(p_id->answer));
    }

    vlc_mutex_unlock(&p_id->lock);
    vlc_interrupt_unregister();

    vlc_mutex_lock(&p_provider->lock);
    dialog_remove_locked(p_provider, p_id);
    vlc_mutex_unlock(&p_provider->lock);
    return i_ret;
}

static int
dialog_display_error_va(vlc_dialog_provider *p_provider, const char *psz_title,
                        const char *psz_fmt, va_list ap)
{
    vlc_mutex_lock(&p_provider->lock);
    if (p_provider->cbs.pf_display_error == NULL)
    {
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_EGENERIC;
    }

    char *psz_text;
    if (vasprintf(&psz_text, psz_fmt, ap) == -1)
    {
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_ENOMEM;
    }

    p_provider->cbs.pf_display_error(psz_title, psz_text, p_provider->p_cbs_data);
    free(psz_text);
    vlc_mutex_unlock(&p_provider->lock);

    return VLC_SUCCESS;
}

int
vlc_dialog_display_error_va(vlc_object_t *p_obj, const char *psz_title,
                            const char *psz_fmt, va_list ap)
{
    assert(p_obj != NULL && psz_title != NULL && psz_fmt != NULL);
    int i_ret;
    vlc_dialog_provider *p_provider = get_dialog_provider(p_obj, true);

    if (p_provider != NULL)
        i_ret = dialog_display_error_va(p_provider, psz_title, psz_fmt, ap);
    else
        i_ret = VLC_EGENERIC;

    if (i_ret != VLC_SUCCESS)
    {
        msg_Err(p_obj, "%s", psz_title);
        msg_GenericVa(p_obj, VLC_MSG_ERR, psz_fmt, ap);
    }
    return i_ret;
}


#undef vlc_dialog_display_error
int
vlc_dialog_display_error(vlc_object_t *p_obj, const char *psz_title,
                         const char *psz_fmt, ...)
{
    assert(psz_fmt != NULL);
    va_list ap;
    va_start(ap, psz_fmt);
    int i_ret = vlc_dialog_display_error_va(p_obj, psz_title, psz_fmt, ap);
    va_end(ap);
    return i_ret;
}

static int
dialog_display_login_va(vlc_dialog_provider *p_provider, vlc_dialog_id **pp_id,
                        const char *psz_default_username, bool b_ask_store,
                        const char *psz_title, const char *psz_fmt, va_list ap)
{
    vlc_mutex_lock(&p_provider->lock);
    if (p_provider->cbs.pf_display_login == NULL
     || p_provider->cbs.pf_cancel == NULL)
    {
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_EGENERIC;
    }

    char *psz_text;
    if (vasprintf(&psz_text, psz_fmt, ap) == -1)
    {
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_ENOMEM;
    }

    vlc_dialog_id *p_id = dialog_add_locked(p_provider, VLC_DIALOG_LOGIN);
    if (p_id == NULL)
    {
        free(psz_text);
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_ENOMEM;
    }
    p_provider->cbs.pf_display_login(p_id, psz_title, psz_text,
                                     psz_default_username, b_ask_store,
                                     p_provider->p_cbs_data);
    free(psz_text);
    vlc_mutex_unlock(&p_provider->lock);
    *pp_id = p_id;

    return VLC_SUCCESS;
}

int
vlc_dialog_wait_login_va(vlc_object_t *p_obj,  char **ppsz_username,
                         char **ppsz_password, bool *p_store,
                         const char *psz_default_username,
                         const char *psz_title, const char *psz_fmt, va_list ap)
{
    assert(p_obj != NULL && ppsz_username != NULL && ppsz_password != NULL
        && psz_fmt != NULL && psz_title != NULL);

    vlc_dialog_provider *p_provider = get_dialog_provider(p_obj, true);
    if (p_provider == NULL)
        return VLC_EGENERIC;

    vlc_dialog_id *p_id;
    int i_ret = dialog_display_login_va(p_provider, &p_id, psz_default_username,
                                        p_store != NULL, psz_title, psz_fmt, ap);
    if (i_ret < 0 || p_id == NULL)
        return i_ret;

    struct dialog_answer answer;
    i_ret = dialog_wait(p_provider, p_id, VLC_DIALOG_LOGIN, &answer);
    if (i_ret <= 0)
        return i_ret;

    *ppsz_username = answer.u.login.psz_username;
    *ppsz_password = answer.u.login.psz_password;
    if (p_store != NULL)
        *p_store = answer.u.login.b_store;

    return 1;
}

#undef vlc_dialog_wait_login
int
vlc_dialog_wait_login(vlc_object_t *p_obj,  char **ppsz_username,
                      char **ppsz_password, bool *p_store,
                      const char *psz_default_username, const char *psz_title,
                      const char *psz_fmt, ...)
{
    assert(psz_fmt != NULL);
    va_list ap;
    va_start(ap, psz_fmt);
    int i_ret = vlc_dialog_wait_login_va(p_obj, ppsz_username, ppsz_password,
                                         p_store,psz_default_username,
                                         psz_title, psz_fmt, ap);
    va_end(ap);
    return i_ret;
}

static int
dialog_display_question_va(vlc_dialog_provider *p_provider, vlc_dialog_id **pp_id,
                           vlc_dialog_question_type i_type,
                           const char *psz_cancel, const char *psz_action1,
                           const char *psz_action2, const char *psz_title,
                           const char *psz_fmt, va_list ap)
{
    vlc_mutex_lock(&p_provider->lock);
    if (p_provider->cbs.pf_display_question == NULL
     || p_provider->cbs.pf_cancel == NULL)
    {
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_EGENERIC;
    }

    char *psz_text;
    if (vasprintf(&psz_text, psz_fmt, ap) == -1)
    {
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_ENOMEM;
    }

    vlc_dialog_id *p_id = dialog_add_locked(p_provider, VLC_DIALOG_QUESTION);
    if (p_id == NULL)
    {
        free(psz_text);
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_ENOMEM;
    }
    p_provider->cbs.pf_display_question(p_id, psz_title, psz_text,
                                        i_type, psz_cancel, psz_action1,
                                        psz_action2, p_provider->p_cbs_data);
    free(psz_text);
    vlc_mutex_unlock(&p_provider->lock);
    *pp_id = p_id;

    return VLC_SUCCESS;
}

int
vlc_dialog_wait_question_va(vlc_object_t *p_obj,
                            vlc_dialog_question_type i_type,
                            const char *psz_cancel, const char *psz_action1,
                            const char *psz_action2, const char *psz_title,
                            const char *psz_fmt, va_list ap)
{
    assert(p_obj != NULL && psz_fmt != NULL && psz_title != NULL
        && psz_cancel != NULL);

    vlc_dialog_provider *p_provider = get_dialog_provider(p_obj, true);
    if (p_provider == NULL)
        return VLC_EGENERIC;

    vlc_dialog_id *p_id;
    int i_ret = dialog_display_question_va(p_provider, &p_id, i_type,
                                           psz_cancel, psz_action1,
                                           psz_action2, psz_title, psz_fmt, ap);
    if (i_ret < 0 || p_id == NULL)
        return i_ret;

    struct dialog_answer answer;
    i_ret = dialog_wait(p_provider, p_id, VLC_DIALOG_QUESTION, &answer);
    if (i_ret <= 0)
        return i_ret;

    if (answer.u.question.i_action != 1 && answer.u.question.i_action != 2)
        return VLC_EGENERIC;

    return answer.u.question.i_action;
}

#undef vlc_dialog_wait_question
int
vlc_dialog_wait_question(vlc_object_t *p_obj,
                         vlc_dialog_question_type i_type,
                         const char *psz_cancel, const char *psz_action1,
                         const char *psz_action2, const char *psz_title,
                         const char *psz_fmt, ...)
{
    assert(psz_fmt != NULL);
    va_list ap;
    va_start(ap, psz_fmt);
    int i_ret = vlc_dialog_wait_question_va(p_obj, i_type, psz_cancel,
                                            psz_action1, psz_action2, psz_title,
                                            psz_fmt, ap);
    va_end(ap);
    return i_ret;
}

static int
display_progress_va(vlc_dialog_provider *p_provider, vlc_dialog_id **pp_id,
                    bool b_indeterminate, float f_position,
                    const char *psz_cancel, const char *psz_title,
                    const char *psz_fmt, va_list ap)
{
    vlc_mutex_lock(&p_provider->lock);
    if (p_provider->cbs.pf_display_progress == NULL
     || p_provider->cbs.pf_update_progress == NULL
     || p_provider->cbs.pf_cancel == NULL)
    {
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_EGENERIC;
    }

    char *psz_text;
    if (vasprintf(&psz_text, psz_fmt, ap) == -1)
    {
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_ENOMEM;
    }

    vlc_dialog_id *p_id = dialog_add_locked(p_provider, VLC_DIALOG_PROGRESS);
    if (p_id == NULL)
    {
        free(psz_text);
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_ENOMEM;
    }
    p_id->b_progress_indeterminate = b_indeterminate;
    p_id->psz_progress_text = psz_text;
    p_provider->cbs.pf_display_progress(p_id, psz_title, psz_text,
                                        b_indeterminate, f_position, psz_cancel,
                                        p_provider->p_cbs_data);
    vlc_mutex_unlock(&p_provider->lock);
    *pp_id = p_id;

    return VLC_SUCCESS;
}

vlc_dialog_id *
vlc_dialog_display_progress_va(vlc_object_t *p_obj, bool b_indeterminate,
                               float f_position, const char *psz_cancel,
                               const char *psz_title, const char *psz_fmt,
                               va_list ap)
{
    assert(p_obj != NULL && psz_title != NULL && psz_fmt != NULL);

    vlc_dialog_provider *p_provider = get_dialog_provider(p_obj, true);
    if (p_provider == NULL)
        return NULL;
    vlc_dialog_id *p_id;
    int i_ret = display_progress_va(p_provider, &p_id, b_indeterminate,
                                    f_position, psz_cancel, psz_title, psz_fmt,
                                    ap);
    return i_ret == VLC_SUCCESS ? p_id : NULL;
}

#undef vlc_dialog_display_progress
vlc_dialog_id *
vlc_dialog_display_progress(vlc_object_t *p_obj, bool b_indeterminate,
                            float f_position, const char *psz_cancel,
                            const char *psz_title, const char *psz_fmt, ...)
{
    assert(psz_fmt != NULL);
    va_list ap;
    va_start(ap, psz_fmt);
    vlc_dialog_id *p_id =
        vlc_dialog_display_progress_va(p_obj, b_indeterminate, f_position,
                                       psz_cancel, psz_title, psz_fmt, ap);
    va_end(ap);
    return p_id;
}

static int
dialog_update_progress(vlc_object_t *p_obj, vlc_dialog_id *p_id, float f_value,
                       char *psz_text)
{
    assert(p_obj != NULL && p_id != NULL);
    vlc_dialog_provider *p_provider = get_dialog_provider(p_obj, false);

    vlc_mutex_lock(&p_provider->lock);
    if (p_provider->cbs.pf_update_progress == NULL)
    {
        vlc_mutex_unlock(&p_provider->lock);
        free(psz_text);
        return VLC_EGENERIC;
    }

    if (p_id->b_progress_indeterminate)
    {
        vlc_mutex_unlock(&p_provider->lock);
        free(psz_text);
        return VLC_EGENERIC;
    }
    if (psz_text != NULL)
    {
        free(p_id->psz_progress_text);
        p_id->psz_progress_text = psz_text;
    }
    p_provider->cbs.pf_update_progress(p_id, f_value, p_id->psz_progress_text,
                                       p_provider->p_cbs_data);

    vlc_mutex_unlock(&p_provider->lock);
    return VLC_SUCCESS;
}

#undef vlc_dialog_update_progress
int
vlc_dialog_update_progress(vlc_object_t *p_obj, vlc_dialog_id *p_id,
                           float f_value)
{
    return dialog_update_progress(p_obj, p_id, f_value, NULL);
}

int
vlc_dialog_update_progress_text_va(vlc_object_t *p_obj, vlc_dialog_id *p_id,
                                   float f_value, const char *psz_fmt,
                                   va_list ap)
{
    assert(psz_fmt != NULL);

    char *psz_text;
    if (vasprintf(&psz_text, psz_fmt, ap) == -1)
        return VLC_ENOMEM;
    return dialog_update_progress(p_obj, p_id, f_value, psz_text);
}

#undef vlc_dialog_update_progress_text
int
vlc_dialog_update_progress_text(vlc_object_t *p_obj, vlc_dialog_id *p_id,
                                float f_value, const char *psz_fmt, ...)
{
    assert(psz_fmt != NULL);
    va_list ap;
    va_start(ap, psz_fmt);
    int i_ret = vlc_dialog_update_progress_text_va(p_obj, p_id, f_value,
                                                   psz_fmt, ap);
    va_end(ap);
    return i_ret;
}

#undef vlc_dialog_release
void
vlc_dialog_release(vlc_object_t *p_obj, vlc_dialog_id *p_id)
{
    assert(p_obj != NULL && p_id != NULL);
    vlc_dialog_provider *p_provider = get_dialog_provider(p_obj, false);

    vlc_mutex_lock(&p_provider->lock);
    dialog_cancel_locked(p_provider, p_id);
    dialog_remove_locked(p_provider, p_id);
    vlc_mutex_unlock(&p_provider->lock);
}

#undef vlc_dialog_is_cancelled
bool
vlc_dialog_is_cancelled(vlc_object_t *p_obj, vlc_dialog_id *p_id)
{
    (void) p_obj;
    assert(p_id != NULL);

    vlc_mutex_lock(&p_id->lock);
    bool b_cancelled = p_id->b_cancelled;
    vlc_mutex_unlock(&p_id->lock);
    return b_cancelled;
}

void
vlc_dialog_id_set_context(vlc_dialog_id *p_id, void *p_context)
{
    vlc_mutex_lock(&p_id->lock);
    p_id->p_context = p_context;
    vlc_mutex_unlock(&p_id->lock);
}

void *
vlc_dialog_id_get_context(vlc_dialog_id *p_id)
{
    vlc_mutex_lock(&p_id->lock);
    void *p_context = p_id->p_context;
    vlc_mutex_unlock(&p_id->lock);
    return p_context;
}

static int
dialog_id_post(vlc_dialog_id *p_id, struct dialog_answer *p_answer)
{
    vlc_mutex_lock(&p_id->lock);
    if (p_answer == NULL)
    {
        p_id->b_cancelled = true;
    }
    else
    {
        p_id->answer = *p_answer;
        p_id->b_answered = true;
    }
    p_id->i_refcount--;
    if (p_id->i_refcount > 0)
    {
        vlc_cond_signal(&p_id->wait);
        vlc_mutex_unlock(&p_id->lock);
    }
    else
    {
        vlc_mutex_unlock(&p_id->lock);
        dialog_id_release(p_id);
    }
    return VLC_SUCCESS;
}

int
vlc_dialog_id_post_login(vlc_dialog_id *p_id, const char *psz_username,
                         const char *psz_password, bool b_store)
{
    assert(p_id != NULL && psz_username != NULL && psz_password != NULL);

    struct dialog_answer answer = {
        .i_type = VLC_DIALOG_LOGIN,
        .u.login = {
            .b_store = b_store,
            .psz_username = strdup(psz_username),
            .psz_password = strdup(psz_password),
        },
    };
    if (answer.u.login.psz_username == NULL
     || answer.u.login.psz_password == NULL)
    {
        free(answer.u.login.psz_username);
        free(answer.u.login.psz_password);
        dialog_id_post(p_id, NULL);
        return VLC_ENOMEM;
    }

    return dialog_id_post(p_id, &answer);
}

int
vlc_dialog_id_post_action(vlc_dialog_id *p_id, int i_action)
{
    assert(p_id != NULL);

    struct dialog_answer answer = {
        .i_type = VLC_DIALOG_QUESTION,
        .u.question = { .i_action = i_action },
    };

    return dialog_id_post(p_id, &answer);
}

int
vlc_dialog_id_dismiss(vlc_dialog_id *p_id)
{
    return dialog_id_post(p_id, NULL);
}

#undef vlc_dialog_provider_set_ext_callback
void
vlc_dialog_provider_set_ext_callback(vlc_object_t *p_obj,
                                     vlc_dialog_ext_update_cb pf_update,
                                     void *p_data)
{
    assert(p_obj != NULL);
    vlc_dialog_provider *p_provider = get_dialog_provider(p_obj, false);

    vlc_mutex_lock(&p_provider->lock);

    p_provider->pf_ext_update = pf_update;
    p_provider->p_ext_data = p_data;

    vlc_mutex_unlock(&p_provider->lock);
}

#undef vlc_ext_dialog_update
int
vlc_ext_dialog_update(vlc_object_t *p_obj, extension_dialog_t *p_ext_dialog)
{
    assert(p_obj != NULL);
    vlc_dialog_provider *p_provider = get_dialog_provider(p_obj, false);

    vlc_mutex_lock(&p_provider->lock);
    if (p_provider->pf_ext_update == NULL)
    {
        vlc_mutex_unlock(&p_provider->lock);
        return VLC_EGENERIC;
    }
    p_provider->pf_ext_update(p_ext_dialog, p_provider->p_ext_data);
    vlc_mutex_unlock(&p_provider->lock);
    return VLC_SUCCESS;
}
