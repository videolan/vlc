/*****************************************************************************
 * keystore.c: test vlc_credential API
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

#include "../../libvlc/test.h"
#include "../../../lib/libvlc_internal.h"

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interrupt.h>
#include <vlc_keystore.h>
#include <vlc_dialog.h>
#include <vlc_url.h>
#include <vlc_fs.h>

#include <assert.h>

struct cred
{
    const char *psz_user;
    const char *psz_pwd;
};

struct cred_res
{
    const char *psz_user;
    const char *psz_pwd;
    const char *psz_realm;
    const char *psz_authtype;
};

static const struct testcase
{
    bool        b_found;
    const char *psz_url;
    const char *psz_realm;
    const char *psz_authtype;
    struct cred_res result;
    struct cred opt;
    struct cred dialog;
    bool        b_dialog_store;
} testcases[] =
{
#define HTTP(path, realm) "http://"path, realm, "Basic"
#define SMB(path) "smb://"path, NULL, NULL
#define SFTP(path) "sftp://"path, NULL, NULL
#define WIPE_MEMORY_KEYSTORE \
    { false, NULL, NULL, NULL, {}, {}, {}, false }

    /* First tests use sftp protocol: no realm and results doesn't depend on
     * path */
    { true, SFTP("user1:pwd1@ex.com/testing/deprecated_url"),
      { "user1", "pwd1", NULL, NULL }, {} , {}, false },

    { true, SFTP("ex.com/testing/opt"),
      { "user1", "pwd1", NULL, NULL }, { "user1", "pwd1" }, {}, false },

    { true, SFTP("ex.com/testing/dial"),
      { "user1", "pwd1", NULL, NULL }, {}, { "user1", "pwd1" }, false },

    WIPE_MEMORY_KEYSTORE,

    { true, SFTP("user1@ex.com/testing/url_dial"),
      { "user1", "pwd1", NULL, NULL }, { NULL, NULL }, { NULL, "pwd1" }, false },

    WIPE_MEMORY_KEYSTORE,

    { true, SFTP("ex.com/testing/opt_dial"),
      { "user1", "pwd1", NULL, NULL }, { "user1", NULL }, { NULL, "pwd1" }, false },

    WIPE_MEMORY_KEYSTORE,

    { true, SFTP("WRONG_USER@ex.com/testing/url_opt_dial"),
      { "user1", "pwd1", NULL, NULL }, { "user1", NULL }, { NULL, "pwd1" }, false },

    WIPE_MEMORY_KEYSTORE,

    /* Order is important now since previously stored credentials could be
     * found by future tests */

    { true, SFTP("ex.com/testing/mem_ks_store"),
      { "user1", "pwd1", NULL, NULL }, {}, { "user1", "pwd1" }, false },

    { true, SFTP("ex.com/testing/mem_ks_find"),
      { "user1", "pwd1", NULL, NULL }, {}, {}, false },

    WIPE_MEMORY_KEYSTORE,

    { false, SFTP("ex.com/testing/mem_ks_find"),
      { "user1", "pwd1", NULL, NULL }, {}, {}, false },

    WIPE_MEMORY_KEYSTORE,

    /* Testing permanent keystore */

    { true, SFTP("ex.com/testing/ks_store"),
      { "user1", "pwd1", NULL, NULL }, {}, { "user1", "pwd1" }, true },

    WIPE_MEMORY_KEYSTORE,

    { true, SFTP("ex.com/testing/ks_find"),
      { "user1", "pwd1", NULL, NULL }, {}, {}, false },

    { true, SFTP("ex.com:2022/testing/ks_store"),
      { "user2", "pwd2", NULL, NULL }, {}, { "user2", "pwd2" }, true },

    WIPE_MEMORY_KEYSTORE,

    { true, SFTP("user1@ex.com/testing/ks_find"),
      { "user1", "pwd1", NULL, NULL }, {}, {}, false },

    { true, SFTP("user2@ex.com:2022/testing/ks_find"),
      { "user2", "pwd2", NULL, NULL }, {}, {}, false },

    { false, SFTP("user2@wrong_host.com:2022/testing/ks_find"),
      { "user2", "pwd2", NULL, NULL }, {}, {}, false },

    { false, SFTP("user2@ex.com/testing/ks_find"),
      { "user2", "pwd2", NULL, NULL }, {}, {}, false },

    { false, SMB("user2@ex.com:2022/testing/ks_find"),
      { "user2", "pwd2", NULL, NULL }, {}, {}, false },

    WIPE_MEMORY_KEYSTORE,

    { true, SFTP("ex.com/testing/opt_not_storing_ks"),
      { "user3", "pwd3", NULL, NULL }, { "user3", "pwd3" }, {}, true },

    WIPE_MEMORY_KEYSTORE,

    { false, SFTP("ex.com/testing/opt_not_storing_ks"),
      { "user3", "pwd3", NULL, NULL }, {}, {}, false },

    WIPE_MEMORY_KEYSTORE,

    /* Testing reusing http credentials rfc7617#2.2 */

    { true, HTTP("ex.com/testing/good_path/ks_store_realm", "Realm"),
      { "user4", "pwd4", "Realm", "Basic" }, {}, { "user4", "pwd4" }, true },

    { false, HTTP("ex.com/testing/good_path/ks_find_realm", "Wrong realm"),
      { "user4", "pwd4", "Wrong realm", "Basic" }, {}, {}, false },

    { true, HTTP("ex.com/testing/good_path/ks_find_realm", "Realm"),
      { "user4", "pwd4", "Realm", "Basic" }, {}, {}, false },

    { true, HTTP("ex.com/testing/good_path/another_path/ks_find_realm", "Realm"),
      { "user4", "pwd4", "Realm", "Basic" }, {}, {}, false },

    { false, HTTP("ex.com/testing/wrong_path/ks_find_realm", "Realm"),
      { "user4", "pwd4", "Realm", "Basic" }, {}, {}, false },

    /* Testing reusing smb credentials */

    { true, SMB("host/share/path1/path2/path3/ks_store"),
      { "user5", "pwd5", NULL, NULL }, {}, { "user5", "pwd5" }, false },

    { true, SMB("host/share/path4/ks_find"),
      { "user5", "pwd5", NULL, NULL }, {}, {}, false },

    { false, SMB("wrong_host/share/path4/ks_find"),
      { "user5", "pwd5", NULL, NULL }, {}, {}, false },

    { false, SMB("host/wrong_share/path4/ks_find"),
      { "user5", "pwd5", NULL, NULL }, {}, {}, false },

    WIPE_MEMORY_KEYSTORE,

    /* Testing smb realm split */

    { true, SMB("host/share/path1/ks_store"),
      { "user6", "pwd6", "domain", NULL }, {}, { "domain;user6", "pwd6" }, true },

    WIPE_MEMORY_KEYSTORE,

    { true, SMB("host/share/path1/ks_store"),
      { "user6", "pwd6", "domain", NULL }, {}, {}, false },

    { true, SMB("domain;user6@host/share/path1/ks_find"),
      { "user6", "pwd6", "domain", NULL }, {}, {}, false },

    { false, SMB("wrong_domain;user6@host/share/path1/ks_find"),
      { "user6", "pwd6", "wrong_domain", NULL }, {}, {}, false },

    WIPE_MEMORY_KEYSTORE,

    { false, "://invalid_url", NULL, NULL,
      { "user1", "pwd1", NULL, NULL }, {}, { "user1", "pwd1" }, false },

    { false, "/invalid_path", NULL, NULL,
      { "user1", "pwd1", NULL, NULL }, {}, { "user1", "pwd1" }, false },
};

struct dialog_ctx
{
    bool b_abort;
    const struct testcase *p_test;
};

static void
display_login_cb(void *p_data, vlc_dialog_id *p_id, const char *psz_title,
                 const char *psz_text, const char *psz_default_username,
                 bool b_ask_store)
{
    (void) psz_title;
    (void) psz_text;
    (void) psz_default_username;
    (void) b_ask_store;
    struct dialog_ctx *p_dialog_ctx = p_data;
    const struct testcase *p_testcase = p_dialog_ctx->p_test;

    const char *psz_user = p_testcase->dialog.psz_user != NULL ?
                           p_testcase->dialog.psz_user : psz_default_username;
    if (!p_dialog_ctx->b_abort && psz_user != NULL
     && p_testcase->dialog.psz_pwd != NULL)
    {
        vlc_dialog_id_post_login(p_id, psz_user, p_testcase->dialog.psz_pwd,
                                 p_testcase->b_dialog_store);
        p_dialog_ctx->b_abort = true;
    }
    else
        vlc_dialog_id_dismiss(p_id);
}

static void
cancel_cb(void *p_data, vlc_dialog_id *p_id)
{
    (void) p_data;
    vlc_dialog_id_dismiss(p_id);
}

static void
test(vlc_object_t *p_obj, unsigned int i_id, const struct testcase *p_test)
{
    printf("test(%u): url %s%s%s%s (%sexpected: %s:%s)\n", i_id, p_test->psz_url,
           p_test->psz_realm != NULL ? " (realm: " : "",
           p_test->psz_realm != NULL ? p_test->psz_realm : "",
           p_test->psz_realm != NULL ? ")" : "",
           p_test->b_found ? "" : "not ", p_test->result.psz_user,
           p_test->result.psz_pwd);

    const vlc_dialog_cbs cbs = {
        .pf_display_login = display_login_cb,
        .pf_cancel = cancel_cb,
    };
    struct dialog_ctx dialog_ctx = {
        .b_abort = false,
        .p_test = p_test,
    };
    vlc_dialog_provider_set_callbacks(p_obj, &cbs, &dialog_ctx);

    const char *psz_opt_user = NULL, *psz_opt_pwd = NULL;
    if (p_test->opt.psz_user != NULL)
    {
        psz_opt_user = "test-user";
        var_SetString(p_obj, psz_opt_user, p_test->opt.psz_user);
    }
    if (p_test->opt.psz_pwd != NULL)
    {
        psz_opt_pwd = "test-pwd";
        var_SetString(p_obj, psz_opt_pwd, p_test->opt.psz_pwd);
    }

    vlc_url_t url;
    vlc_UrlParse(&url, p_test->psz_url);

    vlc_credential credential;
    vlc_credential_init(&credential, &url);
    credential.psz_realm = p_test->psz_realm;
    credential.psz_authtype = p_test->psz_authtype;

    bool b_found = false;
    while (vlc_credential_get(&credential, p_obj, psz_opt_user, psz_opt_pwd,
                              "test authentication", "this a test"))
    {
        bool realm_match = !p_test->result.psz_realm
            || (credential.psz_realm
            && strcmp(credential.psz_realm, p_test->result.psz_realm) == 0);
        bool authtype_match = !p_test->result.psz_authtype
            || (credential.psz_authtype
            && strcmp(credential.psz_authtype, p_test->result.psz_authtype) == 0);

        if (realm_match && authtype_match
         && strcmp(credential.psz_username, p_test->result.psz_user) == 0
         && strcmp(credential.psz_password, p_test->result.psz_pwd) == 0)
        {
            b_found = true;
            break;
        }
    }
    assert(b_found == p_test->b_found);
    vlc_credential_store(&credential, p_obj);

    vlc_UrlClean(&url);
    vlc_credential_clean(&credential);

    vlc_dialog_provider_set_callbacks(p_obj, NULL, NULL);
}

static libvlc_instance_t *
create_libvlc(int i_vlc_argc, const char *const *ppsz_vlc_argv)
{
    libvlc_instance_t *p_libvlc = libvlc_new(i_vlc_argc, ppsz_vlc_argv);
    assert(p_libvlc != NULL);

    int i_ret;
    i_ret = var_Create(p_libvlc->p_libvlc_int, "test-user", VLC_VAR_STRING);
    assert(i_ret == VLC_SUCCESS);
    i_ret = var_Create(p_libvlc->p_libvlc_int, "test-pwd", VLC_VAR_STRING);
    assert(i_ret == VLC_SUCCESS);

    return p_libvlc;
}

int
main(void)
{
    test_init();

    printf("creating tmp plaintext keystore file\n");
    char psz_tmp_path[] = "/tmp/libvlc_XXXXXX";
    int i_tmp_fd = -1;
    i_tmp_fd = vlc_mkstemp(psz_tmp_path);
    assert(i_tmp_fd != -1);

    int i_vlc_argc = 4;
    const char *ppsz_vlc_argv[i_vlc_argc];
    ppsz_vlc_argv[0] = "--keystore";
    ppsz_vlc_argv[1] = "file_plaintext,none";
    ppsz_vlc_argv[2] = "--keystore-file";
    ppsz_vlc_argv[3] = psz_tmp_path;

    libvlc_instance_t *p_libvlc = create_libvlc(i_vlc_argc, ppsz_vlc_argv);

    for (unsigned int i = 0; i < sizeof(testcases)/sizeof(*testcases); ++i)
    {
        if (testcases[i].psz_url == NULL)
        {
            printf("test(%u): wiping memory keystore\n", i);
            libvlc_release(p_libvlc);
            p_libvlc = create_libvlc(i_vlc_argc, ppsz_vlc_argv);
        }
        else
            test(VLC_OBJECT(p_libvlc->p_libvlc_int), i, &testcases[i]);
    }

    libvlc_release(p_libvlc);
    vlc_close(i_tmp_fd);

    return 0;
}
