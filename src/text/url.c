/*****************************************************************************
 * url.c: URL related functions
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * Copyright (C) 2008-2012 Rémi Denis-Courmont
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

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef _WIN32
# include <io.h>
#endif

#include <vlc_common.h>
#include <vlc_memstream.h>
#include <vlc_url.h>
#include <vlc_fs.h>
#include <ctype.h>

char *vlc_uri_decode_duplicate (const char *str)
{
    char *buf = strdup (str);
    if (vlc_uri_decode (buf) == NULL)
    {
        free (buf);
        buf = NULL;
    }
    return buf;
}

char *vlc_uri_decode (char *str)
{
    char *in = str, *out = str;
    if (in == NULL)
        return NULL;

    char c;
    while ((c = *(in++)) != '\0')
    {
        if (c == '%')
        {
            char hex[3];

            if (!(hex[0] = *(in++)) || !(hex[1] = *(in++)))
                return NULL;
            hex[2] = '\0';
            *(out++) = strtoul (hex, NULL, 0x10);
        }
        else
            *(out++) = c;
    }
    *out = '\0';
    return str;
}

static bool isurisafe (int c)
{
    /* These are the _unreserved_ URI characters (RFC3986 §2.3) */
    return ((unsigned char)(c - 'a') < 26)
        || ((unsigned char)(c - 'A') < 26)
        || ((unsigned char)(c - '0') < 10)
        || (strchr ("-._~", c) != NULL);
}

static bool isurisubdelim(int c)
{
    return strchr("!$&'()*+,;=", c) != NULL;
}

static bool isurihex(int c)
{   /* Same as isxdigit() but does not depend on locale and unsignedness */
    return ((unsigned char)(c - '0') < 10)
        || ((unsigned char)(c - 'A') < 6)
        || ((unsigned char)(c - 'a') < 6);
}

static const char urihex[] = "0123456789ABCDEF";

static char *encode_URI_bytes (const char *str, size_t *restrict lenp)
{
    char *buf = malloc (3 * *lenp + 1);
    if (unlikely(buf == NULL))
        return NULL;

    char *out = buf;
    for (size_t i = 0; i < *lenp; i++)
    {
        unsigned char c = str[i];

        if (isurisafe (c))
            *(out++) = c;
        /* This is URI encoding, not HTTP forms:
         * Space is encoded as '%20', not '+'. */
        else
        {
            *(out++) = '%';
            *(out++) = urihex[c >> 4];
            *(out++) = urihex[c & 0xf];
        }
    }

    *lenp = out - buf;
    out = realloc (buf, *lenp + 1);
    return likely(out != NULL) ? out : buf;
}

char *vlc_uri_encode (const char *str)
{
    size_t len = strlen (str);
    char *ret = encode_URI_bytes (str, &len);
    if (likely(ret != NULL))
        ret[len] = '\0';
    return ret;
}

char *vlc_path2uri (const char *path, const char *scheme)
{
    if (path == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    if (scheme == NULL && !strcmp (path, "-"))
        return strdup ("fd://0"); // standard input
    /* Note: VLC cannot handle URI schemes without double slash after the
     * scheme name (such as mailto: or news:). */

    char *buf;

#ifdef __OS2__
    char p[strlen (path) + 1];

    for (buf = p; *path; buf++, path++)
        *buf = (*path == '/') ? DIR_SEP_CHAR : *path;
    *buf = '\0';

    path = p;
#endif

#if defined (_WIN32) || defined (__OS2__)
    /* Drive letter */
    if (isalpha ((unsigned char)path[0]) && (path[1] == ':'))
    {
        if (asprintf (&buf, "%s:///%c:", scheme ? scheme : "file",
                      path[0]) == -1)
            buf = NULL;
        path += 2;
# warning Drive letter-relative path not implemented!
        if (path[0] != DIR_SEP_CHAR)
        {
            errno = ENOTSUP;
            return NULL;
        }
    }
    else
    if (!strncmp (path, "\\\\", 2))
    {   /* Windows UNC paths */
        /* \\host\share\path -> file://host/share/path */
        int hostlen = strcspn (path + 2, DIR_SEP);

        if (asprintf (&buf, "file://%.*s", hostlen, path + 2) == -1)
            buf = NULL;
        path += 2 + hostlen;

        if (path[0] == '\0')
            return buf; /* Hostname without path */
    }
    else
#endif
    if (path[0] != DIR_SEP_CHAR)
    {   /* Relative path: prepend the current working directory */
        char *cwd, *ret;

        if ((cwd = vlc_getcwd ()) == NULL)
            return NULL;
        if (asprintf (&buf, "%s"DIR_SEP"%s", cwd, path) == -1)
            buf = NULL;

        free (cwd);
        ret = (buf != NULL) ? vlc_path2uri (buf, scheme) : NULL;
        free (buf);
        return ret;
    }
    else
    if (asprintf (&buf, "%s://", scheme ? scheme : "file") == -1)
        buf = NULL;
    if (buf == NULL)
        return NULL;

    /* Absolute file path */
    assert (path[0] == DIR_SEP_CHAR);
    do
    {
        size_t len = strcspn (++path, DIR_SEP);
        path += len;

        char *component = encode_URI_bytes (path - len, &len);
        if (unlikely(component == NULL))
        {
            free (buf);
            return NULL;
        }
        component[len] = '\0';

        char *uri;
        int val = asprintf (&uri, "%s/%s", buf, component);
        free (component);
        free (buf);
        if (unlikely(val == -1))
            return NULL;
        buf = uri;
    }
    while (*path);

    return buf;
}

char *vlc_uri2path (const char *url)
{
    char *ret = NULL;
    char *end;

    char *path = strstr (url, "://");
    if (path == NULL)
        return NULL; /* unsupported scheme or invalid syntax */

    end = memchr (url, '/', path - url);
    size_t schemelen = ((end != NULL) ? end : path) - url;
    path += 3; /* skip "://" */

    /* Remove request parameters and/or HTML anchor if present */
    end = path + strcspn (path, "?#");
    path = strndup (path, end - path);
    if (unlikely(path == NULL))
        return NULL; /* boom! */

    /* Decode path */
    vlc_uri_decode (path);

    if (schemelen == 4 && !strncasecmp (url, "file", 4))
    {
#if !defined (_WIN32) && !defined (__OS2__)
        /* Leading slash => local path */
        if (*path == '/')
            return path;
        /* Local path disguised as a remote one */
        if (!strncasecmp (path, "localhost/", 10))
            return memmove (path, path + 9, strlen (path + 9) + 1);
#else
        /* cannot start with a space */
        if (*path == ' ')
            goto out;
        for (char *p = strchr (path, '/'); p; p = strchr (p + 1, '/'))
            *p = '\\';

        /* Leading backslash => local path */
        if (*path == '\\')
            return memmove (path, path + 1, strlen (path + 1) + 1);
        /* Local path disguised as a remote one */
        if (!strncasecmp (path, "localhost\\", 10))
            return memmove (path, path + 10, strlen (path + 10) + 1);
        /* UNC path */
        if (*path && asprintf (&ret, "\\\\%s", path) == -1)
            ret = NULL;
#endif
        /* non-local path :-( */
    }
    else
    if (schemelen == 2 && !strncasecmp (url, "fd", 2))
    {
        int fd = strtol (path, &end, 0);

        if (*end)
            goto out;

#if !defined( _WIN32 ) && !defined( __OS2__ )
        switch (fd)
        {
            case 0:
                ret = strdup ("/dev/stdin");
                break;
            case 1:
                ret = strdup ("/dev/stdout");
                break;
            case 2:
                ret = strdup ("/dev/stderr");
                break;
            default:
                if (asprintf (&ret, "/dev/fd/%d", fd) == -1)
                    ret = NULL;
        }
#else
        /* XXX: Does this work on WinCE? */
        if (fd < 2)
            ret = strdup ("CON");
#endif
    }

out:
    free (path);
    return ret; /* unknown scheme */
}

static char *vlc_idna_to_ascii (const char *);

/* RFC3987 §3.1 */
static char *vlc_iri2uri(const char *iri)
{
    size_t a = 0, u = 0;

    for (size_t i = 0; iri[i] != '\0'; i++)
    {
        unsigned char c = iri[i];

        if (c < 128)
            a++;
        else
            u++;
    }

    if (unlikely((a + u) > (SIZE_MAX / 4)))
    {
        errno = ENOMEM;
        return NULL;
    }

    char *uri = malloc(a + 3 * u + 1), *p;
    if (unlikely(uri == NULL))
        return NULL;

    for (p = uri; *iri != '\0'; iri++)
    {
        unsigned char c = *iri;

        if (c < 128)
            *(p++) = c;
        else
        {
            *(p++) = '%';
            *(p++) = urihex[c >> 4];
            *(p++) = urihex[c & 0xf];
        }
    }

    *p = '\0';
    return uri;
}

static bool vlc_uri_component_validate(const char *str, const char *extras)
{
    assert(str != NULL);

    for (size_t i = 0; str[i] != '\0'; i++)
    {
        int c = str[i];

        if (isurisafe(c) || isurisubdelim(c))
            continue;
        if (strchr(extras, c) != NULL)
            continue;
        if (c == '%' && isurihex(str[i + 1]) && isurihex(str[i + 2]))
        {
            i += 2;
            continue;
        }
        return false;
    }
    return true;
}

static bool vlc_uri_host_validate(const char *str)
{
    return vlc_uri_component_validate(str, ":");
}

static bool vlc_uri_path_validate(const char *str)
{
    return vlc_uri_component_validate(str, "/@:");
}

static int vlc_UrlParseInner(vlc_url_t *restrict url, const char *str)
{
    url->psz_protocol = NULL;
    url->psz_username = NULL;
    url->psz_password = NULL;
    url->psz_host = NULL;
    url->i_port = 0;
    url->psz_path = NULL;
    url->psz_option = NULL;
    url->psz_buffer = NULL;
    url->psz_pathbuffer = NULL;

    if (str == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    char *buf = vlc_iri2uri(str);
    if (unlikely(buf == NULL))
        return -1;
    url->psz_buffer = buf;

    char *cur = buf, *next;
    int ret = 0;

    /* URI scheme */
    next = buf;
    while ((*next >= 'A' && *next <= 'Z') || (*next >= 'a' && *next <= 'z')
        || (*next >= '0' && *next <= '9') || memchr ("+-.", *next, 3) != NULL)
        next++;

    if (*next == ':')
    {
        *(next++) = '\0';
        url->psz_protocol = cur;
        cur = next;
    }

    /* Fragment */
    next = strchr(cur, '#');
    if (next != NULL)
    {
#if 0  /* TODO */
       *(next++) = '\0';
       url->psz_fragment = next;
#else
       *next = '\0';
#endif
    }

    /* Query parameters */
    next = strchr(cur, '?');
    if (next != NULL)
    {
        *(next++) = '\0';
        url->psz_option = next;
    }

    /* Authority */
    if (strncmp(cur, "//", 2) == 0)
    {
        cur += 2;

        /* Path */
        next = strchr(cur, '/');
        if (next != NULL)
        {
            *next = '\0'; /* temporary nul, reset to slash later */
            url->psz_path = next;
        }
        /*else
            url->psz_path = "/";*/

        /* User name */
        next = strrchr(cur, '@');
        if (next != NULL)
        {
            *(next++) = '\0';
            url->psz_username = cur;
            cur = next;

            /* Password (obsolete) */
            next = strchr(url->psz_username, ':');
            if (next != NULL)
            {
                *(next++) = '\0';
                url->psz_password = next;
                vlc_uri_decode(url->psz_password);
            }
            vlc_uri_decode(url->psz_username);
        }

        /* Host name */
        if (*cur == '[' && (next = strrchr(cur, ']')) != NULL)
        {   /* Try IPv6 numeral within brackets */
            *(next++) = '\0';
            url->psz_host = strdup(cur + 1);

            if (*next == ':')
                next++;
            else
                next = NULL;
        }
        else
        {
            next = strchr(cur, ':');
            if (next != NULL)
                *(next++) = '\0';

            url->psz_host = vlc_idna_to_ascii(vlc_uri_decode(cur));
        }

        if (url->psz_host == NULL)
            ret = -1;
        else
        if (!vlc_uri_host_validate(url->psz_host))
        {
            free(url->psz_host);
            url->psz_host = NULL;
            errno = EINVAL;
            ret = -1;
        }

        /* Port number */
        if (next != NULL && *next)
        {
            char* end;
            unsigned long port = strtoul(next, &end, 10);

            if (strchr("0123456789", *next) == NULL || *end || port > UINT_MAX)
            {
                errno = EINVAL;
                ret = -1;
            }

            url->i_port = port;
        }

        if (url->psz_path != NULL)
            *url->psz_path = '/'; /* restore leading slash */
    }
    else
    {
        url->psz_path = cur;
    }

    return ret;
}

int vlc_UrlParse(vlc_url_t *url, const char *str)
{
    int ret = vlc_UrlParseInner(url, str);

    if (url->psz_path != NULL && !vlc_uri_path_validate(url->psz_path))
    {
        url->psz_path = NULL;
        errno = EINVAL;
        ret = -1;
    }
    return ret;
}

static char *vlc_uri_fixup_inner(const char *str, const char *extras);

int vlc_UrlParseFixup(vlc_url_t *url, const char *str)
{
    int ret = vlc_UrlParseInner(url, str);

    static const char pathextras[] = "/@:";

    if (url->psz_path != NULL
     && !vlc_uri_component_validate(url->psz_path, pathextras))
    {
        url->psz_pathbuffer = vlc_uri_fixup_inner(url->psz_path, pathextras);
        if (url->psz_pathbuffer == NULL)
        {
            url->psz_path = NULL;
            errno = ENOMEM;
            ret = -1;
        }
        else
        {
            url->psz_path = url->psz_pathbuffer;
            assert(vlc_uri_path_validate(url->psz_path));
        }
    }
    return ret;
}

void vlc_UrlClean (vlc_url_t *restrict url)
{
    free (url->psz_host);
    free (url->psz_buffer);
    free (url->psz_pathbuffer);
}

/**
 * Merge paths
 *
 * See IETF RFC3986 section 5.2.3 for details.
 */
static char *vlc_uri_merge_paths(const char *base, const char *ref)
{
    char *str;
    int len;

    if (base == NULL)
        len = asprintf(&str, "/%s", ref);
    else
    {
        const char *end = strrchr(base, '/');

        if (end != NULL)
            end++;
        else
            end = base;

        len = asprintf(&str, "%.*s%s", (int)(end - base), base, ref);
    }

    if (unlikely(len == -1))
        str = NULL;
    return str;
}

/**
 * Remove dot segments
 *
 * See IETF RFC3986 section 5.2.4 for details.
 */
static char *vlc_uri_remove_dot_segments(char *str)
{
    char *input = str, *output = str;

    while (input[0] != '\0')
    {
        assert(output <= input);

        if (strncmp(input, "../", 3) == 0)
        {
            input += 3;
            continue;
        }
        if (strncmp(input, "./", 2) == 0)
        {
            input += 2;
            continue;
        }
        if (strncmp(input, "/./", 3) == 0)
        {
            input += 2;
            continue;
        }
        if (strcmp(input, "/.") == 0)
        {
            input[1] = '\0';
            continue;
        }
        if (strncmp(input, "/../", 4) == 0)
        {
            input += 3;
            output = memrchr(str, '/', output - str);
            if (output == NULL)
                output = str;
            continue;
        }
        if (strcmp(input, "/..") == 0)
        {
            input[1] = '\0';
            output = memrchr(str, '/', output - str);
            if (output == NULL)
                output = str;
            continue;
        }
        if (strcmp(input, ".") == 0)
        {
            input++;
            continue;
        }
        if (strcmp(input, "..") == 0)
        {
            input += 2;
            continue;
        }

        if (input[0] == '/')
            *(output++) = *(input++);

        size_t len = strcspn(input, "/");

        if (input != output)
            memmove(output, input, len);

        input += len;
        output += len;
    }

    output[0] = '\0';
    return str;
}

char *vlc_uri_compose(const vlc_url_t *uri)
{
    struct vlc_memstream stream;
    char *enc;

    vlc_memstream_open(&stream);

    if (uri->psz_protocol != NULL)
        vlc_memstream_printf(&stream, "%s:", uri->psz_protocol);

    if (uri->psz_host != NULL)
    {
        vlc_memstream_write(&stream, "//", 2);

        if (uri->psz_username != NULL)
        {
            enc = vlc_uri_encode(uri->psz_username);
            if (enc == NULL)
                goto error;

            vlc_memstream_puts(&stream, enc);
            free(enc);

            if (uri->psz_password != NULL)
            {
                enc = vlc_uri_encode(uri->psz_password);
                if (unlikely(enc == NULL))
                    goto error;

                vlc_memstream_printf(&stream, ":%s", enc);
                free(enc);
            }
            vlc_memstream_putc(&stream, '@');
        }

        const char *fmt;

        if (strchr(uri->psz_host, ':') != NULL)
            fmt = (uri->i_port != 0) ? "[%s]:%d" : "[%s]";
        else
            fmt = (uri->i_port != 0) ? "%s:%d" : "%s";
        /* No IDNA decoding here. Seems unnecessary, dangerous even. */
        vlc_memstream_printf(&stream, fmt, uri->psz_host, uri->i_port);
    }

    if (uri->psz_path != NULL)
        vlc_memstream_puts(&stream, uri->psz_path);
    if (uri->psz_option != NULL)
        vlc_memstream_printf(&stream, "?%s", uri->psz_option);
    /* NOTE: fragment not handled currently */

    if (vlc_memstream_close(&stream))
        return NULL;
    return stream.ptr;

error:
    if (vlc_memstream_close(&stream) == 0)
        free(stream.ptr);
    return NULL;
}

char *vlc_uri_resolve(const char *base, const char *ref)
{
    vlc_url_t base_uri, rel_uri;
    vlc_url_t tgt_uri;
    char *pathbuf = NULL, *ret = NULL;

    if (vlc_UrlParse(&rel_uri, ref))
    {
        vlc_UrlClean(&rel_uri);
        return NULL;
    }

    if (rel_uri.psz_protocol != NULL)
    {   /* Short circuit in case of absolute URI */
        vlc_UrlClean(&rel_uri);
        return strdup(ref);
    }

    vlc_UrlParse(&base_uri, base);

    /* RFC3986 section 5.2.2 */
    do
    {
        tgt_uri = rel_uri;
        tgt_uri.psz_protocol = base_uri.psz_protocol;

        if (rel_uri.psz_host != NULL)
            break;

        tgt_uri.psz_username = base_uri.psz_username;
        tgt_uri.psz_password = base_uri.psz_password;
        tgt_uri.psz_host = base_uri.psz_host;
        tgt_uri.i_port = base_uri.i_port;

        if (rel_uri.psz_path == NULL || rel_uri.psz_path[0] == '\0')
        {
            tgt_uri.psz_path = base_uri.psz_path;
            if (rel_uri.psz_option == NULL)
                tgt_uri.psz_option = base_uri.psz_option;
            break;
        }

        if (rel_uri.psz_path[0] == '/')
            break;

        pathbuf = vlc_uri_merge_paths(base_uri.psz_path, rel_uri.psz_path);
        if (unlikely(pathbuf == NULL))
            goto error;

        tgt_uri.psz_path = pathbuf;
    }
    while (0);

    if (tgt_uri.psz_path != NULL)
        vlc_uri_remove_dot_segments(tgt_uri.psz_path);

    ret = vlc_uri_compose(&tgt_uri);
error:
    free(pathbuf);
    vlc_UrlClean(&base_uri);
    vlc_UrlClean(&rel_uri);
    return ret;
}

static char *vlc_uri_fixup_inner(const char *str, const char *extras)
{
    assert(str && extras);

    bool encode_percent = false;
    for (size_t i = 0; str[i] != '\0'; i++)
        if (str[i] == '%' && !(isurihex(str[i+1]) && isurihex(str[i+2])))
        {
            encode_percent = true;
            break;
        }

    struct vlc_memstream stream;

    vlc_memstream_open(&stream);

    for (size_t i = 0; str[i] != '\0'; i++)
    {
        unsigned char c = str[i];

        if (isurisafe(c) || isurisubdelim(c) || (strchr(extras, c) != NULL)
         || (c == '%' && !encode_percent))
            vlc_memstream_putc(&stream, c);
        else
            vlc_memstream_printf(&stream, "%%%02hhX", c);
    }

    if (vlc_memstream_close(&stream))
        return NULL;
    return stream.ptr;
}

char *vlc_uri_fixup(const char *str)
{
    static const char extras[] = ":/?#[]@";

    /* Rule number one is do not change a (potentially) valid URI */
    if (vlc_uri_component_validate(str, extras))
        return strdup(str);

    return vlc_uri_fixup_inner(str, extras);
}

#if defined (HAVE_IDN)
# include <idna.h>
#elif defined (_WIN32)
# include <windows.h>
# include <vlc_charset.h>

# if (_WIN32_WINNT < _WIN32_WINNT_VISTA)
#  define IDN_ALLOW_UNASSIGNED 0x01
static int IdnToAscii(DWORD flags, LPCWSTR str, int len, LPWSTR buf, int size)
{
    HMODULE h = LoadLibrary(_T("Normaliz.dll"));
    if (h == NULL)
    {
        errno = ENOSYS;
        return 0;
    }

    int (WINAPI *IdnToAsciiReal)(DWORD, LPCWSTR, int, LPWSTR, int);
    int ret = 0;

    IdnToAsciiReal = GetProcAddress(h, "IdnToAscii");
    if (IdnToAsciiReal != NULL)
        ret = IdnToAsciiReal(flags, str, len, buf, size);
    else
        errno = ENOSYS;
    FreeLibrary(h);
    return ret;
}
# endif
#endif

/**
 * Converts a UTF-8 nul-terminated IDN to nul-terminated ASCII domain name.
 * \param idn UTF-8 Internationalized Domain Name to convert
 * \return a heap-allocated string or NULL on error.
 */
static char *vlc_idna_to_ascii (const char *idn)
{
#if defined (HAVE_IDN)
    char *adn;

    switch (idna_to_ascii_8z(idn, &adn, IDNA_ALLOW_UNASSIGNED))
    {
        case IDNA_SUCCESS:
            return adn;
        case IDNA_MALLOC_ERROR:
            errno = ENOMEM;
            return NULL;
        case IDNA_DLOPEN_ERROR:
            errno = ENOSYS;
            return NULL;
        default:
            errno = EINVAL;
            return NULL;
    }

#elif defined (_WIN32)
    char *ret = NULL;

    if (idn[0] == '\0')
        return strdup("");

    wchar_t *wide = ToWide (idn);
    if (wide == NULL)
        return NULL;

    int len = IdnToAscii (IDN_ALLOW_UNASSIGNED, wide, -1, NULL, 0);
    if (len == 0)
    {
        errno = EINVAL;
        goto error;
    }

    wchar_t *buf = vlc_alloc (len, sizeof (*buf));
    if (unlikely(buf == NULL))
        goto error;
    if (!IdnToAscii (IDN_ALLOW_UNASSIGNED, wide, -1, buf, len))
    {
        free (buf);
        errno = EINVAL;
        goto error;
    }
    ret = FromWide (buf);
    free (buf);
error:
    free (wide);
    return ret;

#else
    /* No IDN support, filter out non-ASCII domain names */
    for (const char *p = idn; *p; p++)
        if (((unsigned char)*p) >= 0x80)
        {
            errno = ENOSYS;
            return NULL;
        }

    return strdup (idn);

#endif
}
