/*****************************************************************************
 * textdomain.c : Modules text domain management
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
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
#include "modules/modules.h"

#ifdef ENABLE_NLS
# include <libintl.h>
# include <vlc_charset.h>
#endif

int vlc_bindtextdomain (const char *domain)
{
#if defined (ENABLE_NLS)
    /* Specify where to find the locales for current domain */
    char *upath = config_GetSysPath(VLC_LOCALE_DIR, NULL);
    if (unlikely(upath == NULL))
        return -1;

    char *lpath = ToLocale(upath);
    if (lpath == NULL || bindtextdomain (domain, lpath) == NULL)
    {
        LocaleFree(lpath);
        fprintf (stderr, "%s: text domain not found in %s\n", domain, upath);
        free (upath);
        return -1;
    }
    LocaleFree(lpath);
    free (upath);

    /* LibVLC wants all messages in UTF-8.
     * Unfortunately, we cannot ask UTF-8 for strerror_r(), strsignal_r()
     * and other functions that are not part of our text domain.
     */
    if (bind_textdomain_codeset (PACKAGE_NAME, "UTF-8") == NULL)
    {
        fprintf (stderr, "%s: UTF-8 encoding not available\n", domain);
        // Unbinds the text domain to avoid broken encoding
        bindtextdomain (PACKAGE_NAME, "/DOES_NOT_EXIST");
        return -1;
    }

    /* LibVLC does NOT set the default textdomain, since it is a library.
     * This could otherwise break programs using LibVLC (other than VLC).
     * textdomain (PACKAGE_NAME);
     */

#else /* !ENABLE_NLS */
    (void)domain;
#endif

    return 0;
}

/**
 * In-tree plugins share their gettext domain with LibVLC.
 */
const char *vlc_gettext(const char *msgid)
{
#ifdef ENABLE_NLS
    if (likely(*msgid))
        return dgettext (PACKAGE_NAME, msgid);
#endif
    return msgid;
}

const char *vlc_ngettext(const char *msgid, const char *plural,
                         unsigned long n)
{
#ifdef ENABLE_NLS
    if (likely(*msgid))
        return dngettext (PACKAGE_NAME, msgid, plural, n);
#endif
    return ((n == 1) ? msgid : plural);
}
