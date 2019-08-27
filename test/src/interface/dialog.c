/*****************************************************************************
 * dialog.c: test VLC core dialogs
 *****************************************************************************
 * Copyright © 2016 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc/vlc.h>

#include "../../../lib/libvlc_internal.h"

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_dialog.h>
#include <vlc_interrupt.h>
#include <vlc_keystore.h>

#undef NDEBUG
#include <assert.h>

#define TITLE "VLC Dialogs test"

/*
 * Build and exec qt dialog test:
 * $ cd vlc/test
 * $ make test_src_interface_dialog
 * $ ./test_src_interface_dialog -a
 */

struct cb_answer
{
    bool b_dismiss;
    const char *psz_username;
    int i_action;
};



static void
display_error_cb(void *p_data, const char *psz_title, const char *psz_text)
{
    (void) p_data;
    printf("error message: title: '%s', text: '%s'\n", psz_title, psz_text);
}

static void
display_login_cb(void *p_data, vlc_dialog_id *p_id, const char *psz_title,
                 const char *psz_text, const char *psz_default_username,
                 bool b_ask_store)
{
    struct cb_answer *p_ans = p_data;
    printf("login dialog: title: '%s', text: '%s', "
           "default_username: '%s', b_ask_store: %d\n",
           psz_title, psz_text, psz_default_username, b_ask_store);

    if (p_ans->b_dismiss)
        vlc_dialog_id_dismiss(p_id);
    else if (p_ans->psz_username != NULL)
        vlc_dialog_id_post_login(p_id, p_ans->psz_username, "", false);
}

static void
display_question_cb(void *p_data, vlc_dialog_id *p_id, const char *psz_title,
                    const char *psz_text, vlc_dialog_question_type i_type,
                    const char *psz_cancel, const char *psz_action1,
                    const char *psz_action2)
{
    struct cb_answer *p_ans = p_data;
    printf("question dialog: title: '%s', text: '%s', "
           "type: %d, cancel: '%s', action1: '%s', action2: '%s'\n",
           psz_title, psz_text, i_type, psz_cancel, psz_action1, psz_action2);

    if (p_ans->b_dismiss)
        vlc_dialog_id_dismiss(p_id);
    else if (p_ans->i_action > 0)
        vlc_dialog_id_post_action(p_id, p_ans->i_action);
}

static void
display_progress_cb(void *p_data, vlc_dialog_id *p_id, const char *psz_title,
                    const char *psz_text, bool b_indeterminate,
                    float f_position, const char *psz_cancel)
{
    struct cb_answer *p_ans = p_data;
    printf("progress dialog: title: '%s', text: '%s', "
           "indeterminate: %d, position: %f, cancel: '%s'\n",
           psz_title, psz_text, b_indeterminate, f_position, psz_cancel);

    if (p_ans->b_dismiss)
        vlc_dialog_id_dismiss(p_id);
}

static void cancel_cb(void *p_data, vlc_dialog_id *p_id)
{
    (void) p_data;
    vlc_dialog_id_dismiss(p_id);
}

static void update_progress_cb(void *p_data, vlc_dialog_id *p_id, float f_position,
                               const char *psz_text)
{
    (void) p_id;
    (void) p_data;
    printf("update_progress: %f, text: %s\n", f_position, psz_text);
}

static inline void
set_answer(struct cb_answer *p_ans, bool b_dismiss, const char *psz_username,
           int i_action)
{
    if (p_ans != NULL)
    {
        p_ans->b_dismiss = b_dismiss;
        p_ans->psz_username = psz_username;
        p_ans->i_action = i_action;
    }
}

static void
test_dialogs(vlc_object_t *p_obj, struct cb_answer *p_ans,
             vlc_tick_t i_dialog_wait)
{
    set_answer(p_ans, false, NULL, 0);
    int i_ret = vlc_dialog_display_error(p_obj, TITLE,
                                         "%d/ Testing login...", 1);
    assert(i_ret == VLC_SUCCESS);

    vlc_dialog_id *p_id;
    char *psz_user, *psz_passwd;
    bool b_store;
    set_answer(p_ans, false, "Click OK", 0);
    i_ret = vlc_dialog_wait_login(p_obj, &psz_user, &psz_passwd,
                                  &b_store, "Click OK", TITLE, "Click OK");
    assert(i_ret == 1 && strcmp(psz_user, "Click OK") == 0);
    free(psz_user);
    free(psz_passwd);

    set_answer(p_ans, true, NULL, 0);
    i_ret = vlc_dialog_wait_login(p_obj, &psz_user, &psz_passwd,
                                  &b_store, "Click Cancel", TITLE, "Click Cancel");
    assert(i_ret == 0);

    set_answer(p_ans, false, NULL, 0);
    i_ret = vlc_dialog_display_error(p_obj, TITLE,
                                     "%d/ Testing question...", 2);
    assert(i_ret == VLC_SUCCESS);

    set_answer(p_ans, false, NULL, 1);
    i_ret = vlc_dialog_wait_question(p_obj,
                                     VLC_DIALOG_QUESTION_NORMAL,
                                     "Cancel", "Action1", "Action2", TITLE,
                                     "Click Action1");
    assert(i_ret == 1);

    set_answer(p_ans, false, NULL, 2);
    i_ret = vlc_dialog_wait_question(p_obj,
                                     VLC_DIALOG_QUESTION_NORMAL,
                                     "Cancel", "Action1", "Action2", TITLE,
                                     "Click Action2");
    assert(i_ret == 2);

    set_answer(p_ans, true, NULL, 0);
    i_ret = vlc_dialog_wait_question(p_obj,
                                     VLC_DIALOG_QUESTION_NORMAL,
                                     "Cancel", "Action1", "Action2", TITLE,
                                     "Click Cancel");
    assert(i_ret == 0);

    set_answer(p_ans, false, NULL, 0);
    i_ret = vlc_dialog_display_error(p_obj, TITLE,
                                     "%d/ Testing critical waiting error...", 3);
    assert(i_ret == VLC_SUCCESS);

    set_answer(p_ans, true, NULL, 0);
    i_ret = vlc_dialog_wait_question(p_obj,
                                     VLC_DIALOG_QUESTION_CRITICAL,
                                     "OK", NULL, NULL, TITLE,
                                     "Error");
    assert(i_ret == 0);

    set_answer(p_ans, false, NULL, 0);
    i_ret = vlc_dialog_display_error(p_obj, TITLE,
                                     "%d/ Testing progress dialog...", 4);
    assert(i_ret == VLC_SUCCESS);

    set_answer(p_ans, false, NULL, 0);
    p_id = vlc_dialog_display_progress(p_obj, true,
                                       0.5f /* should be ignored */,
                                       NULL, TITLE,
                                       "Indeterminate non cancellable dialog "
                                       "for %" PRId64 " us", i_dialog_wait);
    assert(p_id != NULL);
    vlc_tick_sleep(i_dialog_wait);
    vlc_dialog_release(p_obj, p_id);
    assert(i_ret == VLC_SUCCESS);

    set_answer(p_ans, true, NULL, 0);
    float f_position = 0.5f;
    p_id = vlc_dialog_display_progress(p_obj, true,
                                       f_position /* should be ignored */,
                                       "Cancel", TITLE,
                                       "Indeterminate cancellable dialog.\n"
                                       "Cancel It!");
    assert(p_id != NULL);
    while(!vlc_dialog_is_cancelled(p_obj, p_id))
        vlc_tick_sleep(i_dialog_wait / 30);
    vlc_dialog_release(p_obj, p_id);

    set_answer(p_ans, false, NULL, 0);
    p_id = vlc_dialog_display_progress(p_obj, false, f_position, NULL, TITLE,
                                       "Non cancellable dialog in progress");
    assert(p_id != NULL);
    while (f_position <= 1.0f)
    {
        vlc_tick_sleep(i_dialog_wait / 30);
        f_position += 0.02f;
        i_ret = vlc_dialog_update_progress(p_obj, p_id, f_position);
        assert(i_ret == VLC_SUCCESS);
    }
    vlc_dialog_release(p_obj, p_id);

    f_position = 0.5f;
    set_answer(p_ans, false, NULL, 0);
    p_id = vlc_dialog_display_progress(p_obj, false, f_position, NULL, TITLE,
                                       "Non cancellable dialog in progress.\n"
                                       "float value: %f", f_position);
    assert(p_id != NULL);
    while (f_position <= 1.0f)
    {
        vlc_tick_sleep(i_dialog_wait / 30);
        f_position += 0.02f;
        i_ret = vlc_dialog_update_progress_text(p_obj, p_id, f_position,
                                                "Non cancellable dialog in progress.\n"
                                                "float value: %f", f_position);
        assert(i_ret == VLC_SUCCESS);
    }
    vlc_dialog_release(p_obj, p_id);

    set_answer(p_ans, false, NULL, 0);
    i_ret = vlc_dialog_display_error(p_obj, TITLE,
                                     "%d/ Testing 2 modal dialogs at a time...", 5);

    assert(i_ret == VLC_SUCCESS);

    set_answer(p_ans, true, NULL, 0);
    p_id = vlc_dialog_display_progress(p_obj, true,
                                       f_position /* should be ignored */,
                                       "Cancel", TITLE,
                                       "Indeterminate cancellable dialog.\n"
                                       "Cancel It!");
    assert(p_id != NULL);

    set_answer(p_ans, true, NULL, 0);
    i_ret = vlc_dialog_wait_question(p_obj,
                                     VLC_DIALOG_QUESTION_CRITICAL,
                                     "OK", NULL, NULL, TITLE,
                                     "Error");
    assert(i_ret == 0);
    while(!vlc_dialog_is_cancelled(p_obj, p_id))
        vlc_tick_sleep(i_dialog_wait / 30);
    vlc_dialog_release(p_obj, p_id);
}

int
main(int i_argc, char *ppsz_argv[])
{
    bool b_test_all = i_argc > 1 && strcmp(ppsz_argv[1], "-a") == 0;

    if (!b_test_all)
        alarm(10);

    setenv("VLC_PLUGIN_PATH", "../modules", 1);
    setenv("VLC_LIB_PATH", "../modules", 1);

    libvlc_instance_t *p_libvlc = libvlc_new(0, NULL);
    assert(p_libvlc != NULL);

    printf("testing dialog callbacks\n");
    const vlc_dialog_cbs cbs = {
        .pf_display_error = display_error_cb,
        .pf_display_login = display_login_cb,
        .pf_display_question = display_question_cb,
        .pf_display_progress = display_progress_cb,
        .pf_cancel = cancel_cb,
        .pf_update_progress = update_progress_cb,
    };
    struct cb_answer ans = { 0 };
    vlc_dialog_provider_set_callbacks(p_libvlc->p_libvlc_int, &cbs, &ans);
    test_dialogs(VLC_OBJECT(p_libvlc->p_libvlc_int), &ans, 100000);
    vlc_dialog_provider_set_callbacks(p_libvlc->p_libvlc_int, NULL, NULL);

    libvlc_release(p_libvlc);

    if (b_test_all)
    {
        printf("testing Qt dialog callbacks\n");
        static const char *args[] = {
            "--no-qt-privacy-ask", /* avoid dialog that ask for privacy */
        };
        p_libvlc = libvlc_new(1, args);
        assert(p_libvlc != NULL);

        int i_ret = libvlc_InternalAddIntf(p_libvlc->p_libvlc_int, "qt");
        assert(i_ret == VLC_SUCCESS);
        test_dialogs(VLC_OBJECT(p_libvlc->p_libvlc_int), NULL, 3000000);

        libvlc_release(p_libvlc);
    }

    return 0;
}
