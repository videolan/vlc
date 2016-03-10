/*****************************************************************************
 * dialog.c: libvlc dialog API
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_dialog.h>

#include <vlc_common.h>
#include <vlc_dialog.h>

#include "libvlc_internal.h"

static libvlc_dialog_question_type
vlc_to_libvlc_dialog_question_type(vlc_dialog_question_type i_type)
{
    switch (i_type)
    {
    case VLC_DIALOG_QUESTION_NORMAL: return LIBVLC_DIALOG_QUESTION_NORMAL;
    case VLC_DIALOG_QUESTION_WARNING: return LIBVLC_DIALOG_QUESTION_WARNING;
    case VLC_DIALOG_QUESTION_CRITICAL: return LIBVLC_DIALOG_QUESTION_CRITICAL;
    default: vlc_assert_unreachable();
    }
}

static void
display_error_cb(void *p_data, const char *psz_title, const char *psz_text)
{
    libvlc_instance_t *p_instance = p_data;

    p_instance->dialog.cbs.pf_display_error(p_instance->dialog.data, psz_title,
                                            psz_text);
}

static void
display_login_cb(void *p_data, vlc_dialog_id *p_id, const char *psz_title,
                 const char *psz_text, const char *psz_default_username,
                 bool b_ask_store)
{
    libvlc_instance_t *p_instance = p_data;

    p_instance->dialog.cbs.pf_display_login(p_instance->dialog.data,
                                            (libvlc_dialog_id *) p_id,
                                            psz_title, psz_text,
                                            psz_default_username, b_ask_store);
}

static void
display_question_cb(void *p_data, vlc_dialog_id *p_id, const char *psz_title,
                    const char *psz_text, vlc_dialog_question_type i_type,
                    const char *psz_cancel, const char *psz_action1,
                    const char *psz_action2)
{
    libvlc_instance_t *p_instance = p_data;
    const libvlc_dialog_question_type i_ltype =
        vlc_to_libvlc_dialog_question_type(i_type);

    p_instance->dialog.cbs.pf_display_question(p_instance->dialog.data,
                                               (libvlc_dialog_id *) p_id,
                                               psz_title, psz_text, i_ltype,
                                               psz_cancel,
                                               psz_action1, psz_action2);
}

static void
display_progress_cb(void *p_data, vlc_dialog_id *p_id, const char *psz_title,
                    const char *psz_text, bool b_indeterminate,
                    float f_position, const char *psz_cancel)
{
    libvlc_instance_t *p_instance = p_data;

    p_instance->dialog.cbs.pf_display_progress(p_instance->dialog.data,
                                               (libvlc_dialog_id *) p_id,
                                               psz_title, psz_text,
                                               b_indeterminate, f_position,
                                               psz_cancel);
}

static void
cancel_cb(void *p_data, vlc_dialog_id *p_id)
{
    libvlc_instance_t *p_instance = p_data;
    p_instance->dialog.cbs.pf_cancel(p_instance->dialog.data,
                                     (libvlc_dialog_id *)p_id);
}

static void
update_progress_cb(void *p_data, vlc_dialog_id *p_id, float f_position,
                   const char *psz_text)
{
    libvlc_instance_t *p_instance = p_data;
    p_instance->dialog.cbs.pf_update_progress(p_instance->dialog.data,
                                              (libvlc_dialog_id *) p_id,
                                              f_position, psz_text);
}

void
libvlc_dialog_set_callbacks(libvlc_instance_t *p_instance,
                            const libvlc_dialog_cbs *p_cbs, void *p_data)
{
    libvlc_int_t *p_libvlc = p_instance->p_libvlc_int;

    vlc_mutex_lock(&p_instance->instance_lock);
    if (p_cbs != NULL)
    {
        const vlc_dialog_cbs dialog_cbs = {
            .pf_display_error = p_cbs->pf_display_error != NULL ?
                                display_error_cb : NULL,
            .pf_display_login = p_cbs->pf_display_login ?
                                display_login_cb : NULL,
            .pf_display_question = p_cbs->pf_display_question != NULL ?
                                   display_question_cb : NULL,
            .pf_display_progress = p_cbs->pf_display_progress != NULL ?
                                   display_progress_cb : NULL,
            .pf_cancel = p_cbs->pf_cancel != NULL ? cancel_cb : NULL,
            .pf_update_progress = p_cbs->pf_update_progress != NULL ?
                                  update_progress_cb : NULL,
        };

        p_instance->dialog.cbs = *p_cbs;
        p_instance->dialog.data = p_data;

        vlc_dialog_provider_set_callbacks(p_libvlc, &dialog_cbs, p_instance);
    }
    else
        vlc_dialog_provider_set_callbacks(p_libvlc, NULL, NULL);
    vlc_mutex_unlock(&p_instance->instance_lock);
}

void
libvlc_dialog_set_context(libvlc_dialog_id *p_id, void *p_context)
{
    vlc_dialog_id_set_context((vlc_dialog_id *)p_id, p_context);
}

void *
libvlc_dialog_get_context(libvlc_dialog_id *p_id)
{
    return vlc_dialog_id_get_context((vlc_dialog_id *)p_id);
}

int
libvlc_dialog_post_login(libvlc_dialog_id *p_id, const char *psz_username,
                         const char *psz_password, bool b_store)
{
    int i_ret = vlc_dialog_id_post_login((vlc_dialog_id *)p_id, psz_username,
                                         psz_password, b_store);
    return i_ret == VLC_SUCCESS ? 0 : -1;
}

int
libvlc_dialog_post_action(libvlc_dialog_id *p_id, int i_action)
{
    int i_ret = vlc_dialog_id_post_action((vlc_dialog_id *)p_id, i_action);
    return i_ret == VLC_SUCCESS ? 0 : -1;
}

int
libvlc_dialog_dismiss(libvlc_dialog_id *p_id)
{
    int i_ret = vlc_dialog_id_dismiss((vlc_dialog_id *)p_id);
    return i_ret == VLC_SUCCESS ? 0 : -1;
}
