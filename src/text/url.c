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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef WIN32
# include <io.h>
#endif

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_fs.h>

/**
 * Decodes an encoded URI component. See also decode_URI().
 * \return decoded string allocated on the heap, or NULL on error.
 */
char *decode_URI_duplicate (const char *str)
{
    char *buf = strdup (str);
    decode_URI (buf);
    return buf;
}

/**
 * Decodes an encoded URI component in place.
 * <b>This function does NOT decode entire URIs.</b> Instead, it decodes one
 * component at a time (e.g. host name, directory, file name).
 * Decoded URIs do not exist in the real world (see RFC3986 §2.4).
 * Complete URIs are always "encoded" (or they are syntaxically invalid).
 *
 * Note that URI encoding is different from Javascript escaping. Especially,
 * white spaces and Unicode non-ASCII code points are encoded differently.
 *
 * \param str nul-terminated URI component to decode
 * \return str on success, NULL if it was not properly encoded
 */
char *decode_URI (char *str)
{
    char *in = str, *out = str;
    if (in == NULL)
        return NULL;

    signed char c;
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
        if (c >= 32)
            *(out++) = c;
        else
            /* Inserting non-ASCII or non-printable characters is unsafe,
             * and no sane browser will send these unencoded */
            *(out++) = '?';
    }
    *out = '\0';
    return str;
}

static inline bool isurisafe (int c)
{
    /* These are the _unreserved_ URI characters (RFC3986 §2.3) */
    return ((unsigned char)(c - 'a') < 26)
        || ((unsigned char)(c - 'A') < 26)
        || ((unsigned char)(c - '0') < 10)
        || (strchr ("-._~", c) != NULL);
}

static char *encode_URI_bytes (const char *str, size_t *restrict lenp)
{
    char *buf = malloc (3 * *lenp + 1);
    if (unlikely(buf == NULL))
        return NULL;

    char *out = buf;
    for (size_t i = 0; i < *lenp; i++)
    {
        static const char hex[16] = "0123456789ABCDEF";
        unsigned char c = str[i];

        if (isurisafe (c))
            *(out++) = c;
        /* This is URI encoding, not HTTP forms:
         * Space is encoded as '%20', not '+'. */
        else
        {
            *(out++) = '%';
            *(out++) = hex[c >> 4];
            *(out++) = hex[c & 0xf];
        }
    }

    *lenp = out - buf;
    out = realloc (buf, *lenp + 1);
    return likely(out != NULL) ? out : buf;
}

/**
 * Encodes a URI component (RFC3986 §2).
 *
 * @param str nul-terminated UTF-8 representation of the component.
 * @note Obviously, a URI containing nul bytes cannot be passed.
 * @return encoded string (must be free()'d), or NULL for ENOMEM.
 */
char *encode_URI_component (const char *str)
{
    size_t len = strlen (str);
    char *ret = encode_URI_bytes (str, &len);
    if (likely(ret != NULL))
        ret[len] = '\0';
    return ret;
}

/**
 * Builds a URL representation from a local file path.
 * If already a URI, return a copy of the string.
 * @param path path to convert (or URI to copy)
 * @param scheme URI scheme to use (default is auto: "file", "fd" or "smb")
 * @return a nul-terminated URI string (use free() to release it),
 * or NULL in case of error
 */
char *make_URI (const char *path, const char *scheme)
{
    if (path == NULL)
        return NULL;
    if (scheme == NULL && !strcmp (path, "-"))
        return strdup ("fd://0"); // standard input
    if (strstr (path, "://") != NULL)
        return strdup (path); /* Already a URI */
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

#if defined( WIN32 ) || defined( __OS2__ )
    /* Drive letter */
    if (isalpha ((unsigned char)path[0]) && (path[1] == ':'))
    {
        if (asprintf (&buf, "%s:///%c:", scheme ? scheme : "file",
                      path[0]) == -1)
            buf = NULL;
        path += 2;
# warning Drive letter-relative path not implemented!
        if (path[0] != DIR_SEP_CHAR)
            return NULL;
    }
    else
#endif
    if (!strncmp (path, "\\\\", 2))
    {   /* Windows UNC paths */
#if !defined( WIN32 ) && !defined( __OS2__ )
        if (scheme != NULL)
            return NULL; /* remote files not supported */

        /* \\host\share\path -> smb://host/share/path */
        if (strchr (path + 2, '\\') != NULL)
        {   /* Convert backslashes to slashes */
            char *dup = strdup (path);
            if (dup == NULL)
                return NULL;
            for (size_t i = 2; dup[i]; i++)
                if (dup[i] == '\\')
                    dup[i] = DIR_SEP_CHAR;

            char *ret = make_URI (dup, scheme);
            free (dup);
            return ret;
        }
# define SMB_SCHEME "smb"
#else
        /* \\host\share\path -> file://host/share/path */
# define SMB_SCHEME "file"
#endif
        size_t hostlen = strcspn (path + 2, DIR_SEP);

        buf = malloc (sizeof (SMB_SCHEME) + 3 + hostlen);
        if (buf != NULL)
            snprintf (buf, sizeof (SMB_SCHEME) + 3 + hostlen,
                      SMB_SCHEME"://%s", path + 2);
        path += 2 + hostlen;

        if (path[0] == '\0')
            return buf; /* Hostname without path */
    }
    else
    if (path[0] != DIR_SEP_CHAR)
    {   /* Relative path: prepend the current working directory */
        char *cwd, *ret;

        if ((cwd = vlc_getcwd ()) == NULL)
            return NULL;
        if (asprintf (&buf, "%s"DIR_SEP"%s", cwd, path) == -1)
            buf = NULL;

        free (cwd);
        ret = (buf != NULL) ? make_URI (buf, scheme) : NULL;
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

/**
 * Tries to convert a URI to a local (UTF-8-encoded) file path.
 * @param url URI to convert
 * @return NULL on error, a nul-terminated string otherwise
 * (use free() to release it)
 */
char *make_path (const char *url)
{
    char *ret = NULL;
    char *end;

    char *path = strstr (url, "://");
    if (path == NULL)
        return NULL; /* unsupported scheme or invalid syntax */

    end = memchr (url, '/', path - url);
    size_t schemelen = ((end != NULL) ? end : path) - url;
    path += 3; /* skip "://" */

    /* Remove HTML anchor if present */
    end = strchr (path, '#');
    if (end)
        path = strndup (path, end - path);
    else
        path = strdup (path);
    if (unlikely(path == NULL))
        return NULL; /* boom! */

    /* Decode path */
    decode_URI (path);

    if (schemelen == 4 && !strncasecmp (url, "file", 4))
    {
#if (!defined (WIN32) && !defined (__OS2__)) || defined (UNDER_CE)
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

#if !defined( WIN32 ) && !defined( __OS2__ )
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
        else
            ret = NULL;
#endif
    }

out:
    free (path);
    return ret; /* unknown scheme */
}
