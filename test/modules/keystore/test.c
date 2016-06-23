/*****************************************************************************
 * test.c: test keystore module
 *****************************************************************************
 * Copyright © 2015-2016 VLC authors, VideoLAN and VideoLabs
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
#include <vlc_interrupt.h>
#include <vlc_fs.h>
#include <vlc_keystore.h>

#undef NDEBUG
#include <assert.h>

/*
 * Build and exec all keystore tests:
 * $ cd vlc/build-<name>/test
 * $ make test_modules_keystore
 * $ ./test_modules_keystore -a
 */

static const struct
{
    const char *psz_module;
    bool b_test_default;
    bool b_persistent;
} keystore_args[] =
{
    { "memory", true, false },
    { "file", true, true },
    /* Following keystores are tested only when asked explicitly by the tester
     * (with "-a" argv) */
    { "secret", false, true },
    { "kwallet", false, true },
    { "keychain", false, true }
};

static void
values_reinit(const char * ppsz_values[KEY_MAX])
{
    VLC_KEYSTORE_VALUES_INIT(ppsz_values);
    ppsz_values[KEY_REALM] = "libVLC TEST";
}

static unsigned int
ks_find(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX],
        const char* psz_cmp_secret)
{
    vlc_keystore_entry *p_entries;
    unsigned int i_entries = vlc_keystore_find(p_keystore, ppsz_values,
                                               &p_entries);
    for (unsigned int i = 0; i < i_entries; ++i)
    {
        vlc_keystore_entry *p_entry = &p_entries[i];
        assert(p_entry->p_secret[p_entry->i_secret_len - 1] == '\0');
        assert(strcmp((const char *)p_entry->p_secret, psz_cmp_secret) == 0);

        for (unsigned int j = 0; j < KEY_MAX; ++j)
        {
            const char *psz_value1 = ppsz_values[j];
            const char *psz_value2 = p_entry->ppsz_values[j];

            if (!psz_value1)
                continue;
            assert(psz_value2);
            assert(strcmp(psz_value1, psz_value2) == 0);
        }
    }
    if (i_entries > 0)
        vlc_keystore_release_entries(p_entries, i_entries);
    return i_entries;
}

static unsigned int
ks_remove(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX])
{
    return vlc_keystore_remove(p_keystore, ppsz_values);
}

static void
ks_store(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX],
         const uint8_t* p_secret, size_t i_secret_len)
{
    assert(vlc_keystore_store(p_keystore, ppsz_values,
                              (const uint8_t *)p_secret,
                              i_secret_len,
                              "libVLC TEST") == VLC_SUCCESS);
}

static void
test_module(const char *psz_module, bool b_test_all, bool b_persistent,
            int argc, const char * const *argv)
{
#define VALUES_INSERT(i_key, psz_value) ppsz_values[i_key] = psz_value
#define VALUES_REINIT() values_reinit(ppsz_values)
#define KS_FIND() do { \
    i_entries = ks_find(p_keystore, ppsz_values, psz_secret); \
} while (0)
#define KS_REMOVE() do { \
    i_entries = ks_remove(p_keystore, ppsz_values); \
} while(0)
#define KS_STORE() ks_store(p_keystore, ppsz_values, (const uint8_t *)psz_secret, -1)

    printf("\n== Testing %s keystore module ==\n\n", psz_module);

    printf("creating libvlc\n");
    libvlc_instance_t *p_libvlc = libvlc_new(argc, argv);
    assert(p_libvlc != NULL);

    vlc_interrupt_t *ctx = vlc_interrupt_create();
    assert(ctx != NULL);

    printf("creating %s keystore\n", psz_module);
    vlc_keystore *p_keystore = vlc_keystore_create(p_libvlc->p_libvlc_int);
    assert(p_keystore);

    const char *psz_secret = "libvlc test secret";
    unsigned int i_entries;
    const char *ppsz_values[KEY_MAX];

    printf("testing that there is no entry\n");
    VALUES_REINIT();
    VALUES_INSERT(KEY_PROTOCOL, "http");
    VALUES_INSERT(KEY_SERVER, "www.example.com");
    VALUES_INSERT(KEY_PATH, "/example/example.mkv");
    VALUES_INSERT(KEY_PORT, "88");
    VALUES_INSERT(KEY_USER, "user1");
    VALUES_INSERT(KEY_AUTHTYPE, "Basic");
    KS_FIND();
    assert(i_entries == 0);

    printf("testing adding an entry\n");
    KS_STORE();
    KS_FIND();
    assert(i_entries == 1);

    printf("testing that we can override the secret of a previous entry\n");
    psz_secret = "libvlc test secret NEW";
    KS_STORE();
    KS_FIND();
    assert(i_entries == 1);

    printf("testing adding an other entry\n");
    VALUES_INSERT(KEY_USER, "user2");
    KS_FIND();
    assert(i_entries == 0);
    KS_STORE();
    KS_FIND();
    assert(i_entries == 1);

    printf("testing finding the 2 previous entries\n");
    VALUES_INSERT(KEY_USER, NULL);
    KS_FIND();
    assert(i_entries == 2);

    printf("testing that we can't store 2 duplicate entries\n");
    VALUES_INSERT(KEY_USER, "user2");
    KS_STORE();
    VALUES_INSERT(KEY_USER, NULL);
    KS_FIND();
    assert(i_entries == 2);

    if (b_persistent)
    {
        printf("testing that entries are still present after a module unload\n");
        vlc_keystore_release(p_keystore);
        p_keystore = vlc_keystore_create(p_libvlc->p_libvlc_int);
        assert(p_keystore);
        KS_FIND();
        assert(i_entries == 2);
    }

    VALUES_REINIT();
    VALUES_INSERT(KEY_PROTOCOL, "smb");
    VALUES_INSERT(KEY_SERVER, "example");
    VALUES_INSERT(KEY_PATH, "/example.mkv");
    VALUES_INSERT(KEY_USER, "user1");

    if (b_persistent)
    {
        printf("testing adding a third entry from a second running instance\n");

        KS_FIND();
        assert(i_entries == 0);

        vlc_keystore *old_keystore = p_keystore;
        p_keystore = vlc_keystore_create(p_libvlc->p_libvlc_int);
        assert(p_keystore);

        KS_STORE();
        KS_FIND();
        assert(i_entries == 1);

        vlc_keystore_release(p_keystore);
        p_keystore = old_keystore;
        KS_FIND();
        assert(i_entries == 1);
    }
    else
    {
        printf("testing adding a third entry\n");
        KS_FIND();
        assert(i_entries == 0);
        KS_STORE();
        KS_FIND();
        assert(i_entries == 1);
    }


    printf("testing adding a fourth entry (without user/path)\n");
    VALUES_REINIT();
    VALUES_INSERT(KEY_PROTOCOL, "ftp");
    VALUES_INSERT(KEY_SERVER, "example.com");
    KS_FIND();
    assert(i_entries == 0);
    KS_STORE();
    KS_FIND();
    assert(i_entries == 1);

    printf("testing finding an entry only by its protocol\n");
    VALUES_REINIT();
    VALUES_INSERT(KEY_PROTOCOL, "smb");
    KS_FIND();
    assert(i_entries == 1);
    VALUES_REINIT();
    VALUES_INSERT(KEY_PROTOCOL, "http");
    KS_FIND();
    assert(i_entries == 2);
    VALUES_REINIT();
    VALUES_INSERT(KEY_PROTOCOL, "ftp");
    KS_FIND();
    assert(i_entries == 1);

    printf("testing finding all previous entries\n");
    VALUES_REINIT();
    KS_FIND();
    assert(i_entries == 4);

    if (b_test_all && b_persistent)
    {
        printf("\nPress ENTER to remove entries\n");
        getchar();
    }

    printf("testing removing entries that match user => user1\n");
    VALUES_REINIT();
    VALUES_INSERT(KEY_USER, "user1");
    KS_REMOVE();
    assert(i_entries == 2);

    printf("testing removing entries that match user => user2\n");
    VALUES_INSERT(KEY_USER, "user2");
    KS_REMOVE();
    assert(i_entries == 1);

    printf("testing removing entries that match protocol => ftp\n");
    VALUES_REINIT();
    VALUES_INSERT(KEY_PROTOCOL, "ftp");
    KS_REMOVE();
    assert(i_entries == 1);

    printf("testing that all entries are deleted\n");
    VALUES_REINIT();
    KS_FIND();
    assert(i_entries == 0);

    vlc_keystore_release(p_keystore);

    vlc_interrupt_destroy(ctx);

    libvlc_release(p_libvlc);
}

int
main(int i_argc, char *ppsz_argv[])
{
    /* If b_test_all is true, this test could pollute the developer´s keystores */
    bool b_test_all = i_argc > 1 && strcmp(ppsz_argv[1], "-a") == 0;

    if (!b_test_all)
        alarm(3);

    setenv("VLC_PLUGIN_PATH", "../modules", 1);

    /* Create a dummy libvlc to initialize module bank, needed by module_exists */
    libvlc_instance_t *p_libvlc = libvlc_new(0, NULL);
    assert(p_libvlc != NULL);

    for (unsigned int i = 0; i < sizeof(keystore_args)/sizeof(*keystore_args); ++i)
    {
        const char *psz_module = keystore_args[i].psz_module;

        if ((b_test_all || keystore_args[i].b_test_default)
         && module_exists(psz_module))
        {
            int i_vlc_argc = 1;
            char *ppsz_vlc_argv[2] = { 0 };
            int i_tmp_fd = -1;
            char psz_tmp_path[] = "/tmp/libvlc_XXXXXX";

            assert(asprintf(&ppsz_vlc_argv[0], "--keystore=%s,none",
                   psz_module) != -1);

            if (strcmp(psz_module, "file") == 0)
            {
                assert((i_tmp_fd = vlc_mkstemp(psz_tmp_path)) != -1);
                printf("plaintext tmp file: '%s'\n", psz_tmp_path);
                assert(asprintf(&ppsz_vlc_argv[1],
                       "--keystore-file=%s", psz_tmp_path) != -1);
                i_vlc_argc++;
            }

            test_module(psz_module, b_test_all, keystore_args[i].b_persistent,
                        i_vlc_argc, (const char * const *)ppsz_vlc_argv);

            if (i_tmp_fd != -1)
            {
                vlc_close(i_tmp_fd);
                unlink(psz_tmp_path);
            }
            free(ppsz_vlc_argv[0]);
            free(ppsz_vlc_argv[1]);
        }
        else
            printf("not testing %s\n", psz_module);
    }
    libvlc_release(p_libvlc);

    return 0;
}
