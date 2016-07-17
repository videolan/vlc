/*****************************************************************************
 * url.c: Test for url encoding/decoding stuff
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
 * $Id$
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

#include <vlc_common.h>
#include "vlc_url.h"
#include "vlc_strings.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef char * (*conv_t) (const char *);

static void test (conv_t f, const char *in, const char *out)
{
    char *res;

    if (out != NULL)
       printf ("\"%s\" -> \"%s\" ?\n", in, out);
    else
       printf ("\"%s\" -> NULL ?\n", in);
    res = f (in);
    if (res == NULL)
    {
        if (out == NULL)
            return; /* good: NULL -> NULL */
        puts (" ERROR: got NULL");
        exit (2);
    }
    if (out == NULL || strcmp (res, out))
    {
        printf (" ERROR: got \"%s\"\n", res);
        exit (2);
    }

    free (res);
}

static inline void test_decode (const char *in, const char *out)
{
    test (vlc_uri_decode_duplicate, in, out);
}

static inline void test_b64 (const char *in, const char *out)
{
    test (vlc_b64_encode, in, out);
}

static char *make_URI_def (const char *in)
{
    return vlc_path2uri (in, NULL);
}

static inline void test_path (const char *in, const char *out)
{
    test (make_URI_def, in, out);
}

static inline void test_current_directory_path (const char *in, const char *cwd, const char *out)
{
    char *expected_result;
    int val = asprintf (&expected_result, "file://%s/%s", cwd, out);
    assert (val != -1);

    test (make_URI_def, in, expected_result);
    free(expected_result);
}

static void test_url_parse(const char* in, const char* protocol, const char* user,
                           const char* pass, const char* host, unsigned i_port,
                           const char* path, const char* option )
{
#define CHECK( a, b ) \
    if (a == NULL) \
        assert(b == NULL); \
    else \
        assert(b != NULL && !strcmp((a), (b)))

    vlc_url_t url;
    vlc_UrlParse( &url, in );
    CHECK( url.psz_protocol, protocol );
    CHECK( url.psz_username, user );
    CHECK( url.psz_password, pass );
    CHECK( url.psz_host, host );
    CHECK( url.psz_path, path );
    assert( url.i_port == i_port );
    CHECK( url.psz_option, option );

    vlc_UrlClean( &url );

#undef CHECK
}

static void test_url_resolve(const char *base, const char *reference,
                             const char *expected)
{
    fprintf(stderr, "(%s) \"%s\" -> \"%s\" ?\n", base, reference, expected);

    char *result = vlc_uri_resolve(base, reference);
    assert(result != NULL);
    if (strcmp(result, expected))
    {
        fprintf(stderr, " ERROR: got \"%s\"\n", result);
        abort();
    }
    free(result);
}

static void test_rfc3986(const char *reference, const char *expected)
{
    test_url_resolve("http://a/b/c/d;p?q", reference, expected);
}

int main (void)
{
    int val;

    (void)setvbuf (stdout, NULL, _IONBF, 0);
    test_decode ("this_should_not_be_modified_1234",
                 "this_should_not_be_modified_1234");

    test_decode ("This%20should%20be%20modified%201234!",
                 "This should be modified 1234!");

    test_decode ("%7E", "~");

    /* tests with invalid input */
    test_decode ("%", NULL);
    test_decode ("%2", NULL);
    test_decode ("%0000", "");

    /* Non-ASCII tests */
    test_decode ("T%C3%a9l%c3%A9vision %e2%82%Ac", "Télévision €");
    test_decode ("T%E9l%E9vision", "T\xe9l\xe9vision");

    /* Base 64 tests */
    test_b64 ("", "");
    test_b64 ("f", "Zg==");
    test_b64 ("fo", "Zm8=");
    test_b64 ("foo", "Zm9v");
    test_b64 ("foob", "Zm9vYg==");
    test_b64 ("fooba", "Zm9vYmE=");
    test_b64 ("foobar", "Zm9vYmFy");

    /* Path test */
#ifndef _WIN32
    test_path ("/", "file:///");
    test_path ("/home/john/", "file:///home/john/");
    test_path ("/home/john//too///many//slashes",
               "file:///home/john//too///many//slashes");
    test_path ("/home/john/music.ogg", "file:///home/john/music.ogg");
#else
    test_path ("C:\\", "file:///C:/");
    test_path ("C:\\Users\\john\\", "file:///C:/Users/john/");
    test_path ("C:\\Users\\john\\music.ogg",
               "file:///C:/Users/john/music.ogg");
    test_path ("\\\\server\\share\\dir\\file.ext",
               "file://server/share/dir/file.ext");
#endif

    /*int fd = open (".", O_RDONLY);
    assert (fd != -1);*/
    val = chdir ("/tmp");
    assert (val != -1);

    char buf[256];
    char * tmpdir;
    tmpdir = getcwd(buf, sizeof(buf)/sizeof(*buf));
    assert (tmpdir);

#ifndef _WIN32 /* FIXME: deal with anti-slashes */
    test_current_directory_path ("movie.ogg", tmpdir, "movie.ogg");
    test_current_directory_path (".", tmpdir, ".");
    test_current_directory_path ("", tmpdir, "");
#endif

    /*val = fchdir (fd);
    assert (val != -1);*/

    /* URI to path tests */
#define test( a, b ) test (vlc_uri2path, a, b)
    test ("mailto:john@example.com", NULL);
    test ("http://www.example.com/file.html#ref", NULL);
    test ("file://", NULL);
#ifndef _WIN32
    test ("file:///", "/");
    test ("file://localhost/home/john/music%2Eogg", "/home/john/music.ogg");
    test ("file://localhost/home/john/text#ref", "/home/john/text");
    test ("file://localhost/home/john/text?name=value", "/home/john/text");
    test ("file://localhost/home/john/text?name=value#ref", "/home/john/text");
    test ("file://?name=value", NULL);
    test ("file:///?name=value", "/");
    test ("fd://0foobar", NULL);
    test ("fd://0#ref", "/dev/stdin");
    test ("fd://1", "/dev/stdout");
    test ("fd://12345", "/dev/fd/12345");
#else
    test ("file:///C:", "C:");
    test ("file:///C:/Users/john/music%2Eogg", "C:\\Users\\john\\music.ogg");
    test ("file://server/share/dir/file%2Eext",
          "\\\\server\\share\\dir\\file.ext");
    test ("file:///C:/Users/john/text#ref", "C:\\Users\\john\\text");
    test ("file:///C:/Users/john/text?name=value", "C:\\Users\\john\\text");
    test ("file:///C:/Users/john/text?name=value#ref",
          "C:\\Users\\john\\text");
    test ("file://?name=value", NULL);
    test ("file:///C:?name=value", "C:");
    test ("fd://0foobar", NULL);
    test ("fd://0#ref", "CON");
    test ("fd://1", "CON");
    test ("fd://12345", NULL);
#endif
#undef test

    test_url_parse("http://example.com", "http", NULL, NULL, "example.com", 0,
                   NULL, NULL);
    test_url_parse("http://example.com/", "http", NULL, NULL, "example.com", 0,
                   "/", NULL);
    test_url_parse("http://[2001:db8::1]", "http", NULL, NULL, "2001:db8::1",
                   0, NULL, NULL);
    test_url_parse("protocol://john:doe@1.2.3.4:567", "protocol", "john", "doe", "1.2.3.4", 567, NULL, NULL);
    test_url_parse("http://a.b/?opt=val", "http", NULL, NULL, "a.b", 0, "/", "opt=val");
    test_url_parse("p://u:p@host:123/a/b/c?o=v", "p", "u", "p", "host", 123, "/a/b/c", "o=v");
    test_url_parse("p://?o=v", "p", NULL, NULL, "", 0, NULL, "o=v");
    test_url_parse("p://h?o=v", "p", NULL, NULL, "h", 0, NULL, "o=v");
    test_url_parse("p://h:123?o=v", "p", NULL, NULL, "h", 123, NULL, "o=v");
    test_url_parse("p://u:p@h:123?o=v", "p", "u", "p", "h", 123, NULL, "o=v");
    test_url_parse("p://white%20spaced", "p", NULL, NULL, "white%20spaced", 0,
                   NULL, NULL);
    test_url_parse("p://h/white%20spaced", "p", NULL, NULL, "h", 0,
                   "/white%20spaced", NULL);
    /* Relative URIs */
    test_url_parse("//example.com", NULL, NULL, NULL, "example.com", 0,
                   NULL, NULL);
    test_url_parse("/file", NULL, NULL, NULL, NULL, 0, "/file", NULL);
    test_url_parse("?opt=val", NULL, NULL, NULL, NULL, 0, "", "opt=val");
    test_url_parse("/f?o=v", NULL, NULL, NULL, NULL, 0, "/f", "o=v");
    test_url_parse("//example.com/file", NULL, NULL, NULL, "example.com", 0,
                   "/file", NULL);
    test_url_parse("//example.com?opt=val", NULL, NULL, NULL, "example.com", 0,
                   NULL, "opt=val");
    test_url_parse("//example.com/f?o=v", NULL, NULL, NULL, "example.com", 0,
                   "/f", "o=v");
    /* Invalid URIs */
    test_url_parse("p://G a r b a g e", "p", NULL, NULL, NULL, 0, NULL, NULL);
    test_url_parse("p://h/G a r b a g e", "p", NULL, NULL, "h", 0, NULL, NULL);

    /* Reference test cases for reference URI resolution */
    static const char *rfc3986_cases[] =
    {
        "g:h",           "g:h",
        "g",             "http://a/b/c/g",
        "./g",           "http://a/b/c/g",
        "g/",            "http://a/b/c/g/",
        "/g",            "http://a/g",
        "//g",           "http://g",
        "?y",            "http://a/b/c/d;p?y",
        "g?y",           "http://a/b/c/g?y",
        //"#s",            "http://a/b/c/d;p?q#s",
        //"g#s",           "http://a/b/c/g#s",
        //"g?y#s",         "http://a/b/c/g?y#s",
        ";x",            "http://a/b/c/;x",
        "g;x",           "http://a/b/c/g;x",
        //"g;x?y#s",       "http://a/b/c/g;x?y#s",
        "",              "http://a/b/c/d;p?q",
        ".",             "http://a/b/c/",
        "./",            "http://a/b/c/",
        "..",            "http://a/b/",
        "../",           "http://a/b/",
        "../g",          "http://a/b/g",
        "../..",         "http://a/",
        "../../",        "http://a/",
        "../../g",       "http://a/g",

        "../../../g",    "http://a/g",
        "../../../../g", "http://a/g",

        "/./g",          "http://a/g",
        "/../g",         "http://a/g",
        "g.",            "http://a/b/c/g.",
        ".g",            "http://a/b/c/.g",
        "g..",           "http://a/b/c/g..",
        "..g",           "http://a/b/c/..g",

        "./../g",        "http://a/b/g",
        "./g/.",         "http://a/b/c/g/",
        "g/./h",         "http://a/b/c/g/h",
        "g/../h",        "http://a/b/c/h",
        "g;x=1/./y",     "http://a/b/c/g;x=1/y",
        "g;x=1/../y",    "http://a/b/c/y",

        "g?y/./x",       "http://a/b/c/g?y/./x",
        "g?y/../x",      "http://a/b/c/g?y/../x",
        //"g#s/./x",       "http://a/b/c/g#s/./x",
        //"g#s/../x",      "http://a/b/c/g#s/../x",
    };

    for (size_t i = 0; i < ARRAY_SIZE(rfc3986_cases); i += 2)
        test_rfc3986(rfc3986_cases[i], rfc3986_cases[i + 1]);

    return 0;
}
