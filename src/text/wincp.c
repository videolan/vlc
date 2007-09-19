/*****************************************************************************
 * wincp.c: Guessing "local" ANSI code page on Microsoft Windows®
 *****************************************************************************
 *
 * Copyright © 2006-2007 Rémi Denis-Courmont
 * $Id$
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*** We need your help to complete this file!! Look for FIXME ***/

#include <vlc/vlc.h>

#ifndef WIN32
# include <locale.h>
#else
# include <windows.h>
#endif

#ifdef __APPLE__
#   include <errno.h>
#   include <string.h>
#endif

#include <vlc_charset.h>


#ifndef WIN32 /* should work on Win32, but useless */
static inline int locale_match (const char *tab, const char *locale)
{
    for (;*tab; tab += 2)
        if (memcmp (tab, locale, 2) == 0)
            return 0;
    return 1;
}


/**
 * @return a fallback characters encoding to be used, given a locale.
 */
static const char *FindFallbackEncoding (const char *locale)
{
    if ((locale == NULL) || (strlen (locale) < 2)
     || !strcasecmp (locale, "POSIX"))
        return "ASCII";


    /*** The ISO-8859 series (anything but Asia) ***/
    // Latin-1 Western-European languages (ISO-8859-1)
    static const char western[] =
        "aa" "af" "an" "br" "ca" "da" "de" "en" "es" "et" "eu" "fi" "fo" "fr"
        "ga" "gd" "gl" "gv" "id" "is" "it" "kl" "kw" "mg" "ms" "nb" "nl" "nn"
        "no" "oc" "om" "pt" "so" "sq" "st" "sv" "tl" "uz" "wa" "xh" "zu"
        "eo" "mt" "cy";
    if (!locale_match (western, locale))
        return "CP1252"; // Compatible Microsoft superset

    // Latin-2 Slavic languages (ISO-8859-2)
    static const char slavic[] = "bs" "cs" "hr" "hu" "pl" "ro" "sk" "sl";
    if (!locale_match (slavic, locale))
        return "CP1250"; // CP1250 is more common, but incompatible

    // Latin-3 Southern European languages (ISO-8859-3)
    // "eo" and "mt" -> Latin-1 instead, I presume(?).
    // "tr" -> ISO-8859-9 instead

    // Latin-4 North-European languages (ISO-8859-4)
    // -> Latin-1 instead

    /* Cyrillic alphabet languages (ISO-8859-5) */
    static const char cyrillic[] = "be" "bg" "mk" "ru" "sr";
    if (!locale_match (cyrillic, locale))
        return "CP1251"; // KOI8, ISO-8859-5 and CP1251 are incompatible(?)

    /* Arabic (ISO-8859-6) */
    if (!locale_match ("ar", locale))
        // FIXME: someone check if we should return CP1256 or ISO-8859-6
        return "CP1256"; // CP1256 is(?) more common, but incompatible(?)

    /* Greek (ISO-8859-7) */
    if (!locale_match ("el", locale))
        // FIXME: someone check if we should return CP1253 or ISO-8859-7
        return "CP1253"; // CP1253 is(?) more common and less incompatible

    /* Hebrew (ISO-8859-8) */
    if (!locale_match ("he" "iw" "yi", locale))
        return "CP1255"; // Compatible Microsoft superset

    /* Latin-5 Turkish (ISO-8859-9) */
    if (!locale_match ("tr" "ku", locale))
        return "CP1254"; // Compatible Microsoft superset

    /* Latin-6 “North-European” languages (ISO-8859-10) */
    /* It is so much north European that glibc only uses that for Luganda
     * which is spoken in Uganda... unless someone complains, I'm not
     * using this one; let's fallback to CP1252 here. */

    // ISO-8859-11 does arguably not exist. Thai is handled below.

    // ISO-8859-12 really doesn't exist.

    // Latin-7 Baltic languages (ISO-8859-13)
    if (!locale_match ("lt" "lv" "mi", locale))
        // FIXME: mi = New Zealand, doesn't sound baltic!
        return "CP1257"; // Compatible Microsoft superset

    // Latin-8 Celtic languages (ISO-8859-14)
    // "cy" -> use Latin-1 instead (most likely English or French)

    // Latin-9 (ISO-8859-15) -> see Latin-1

    // Latin-10 (ISO-8859-16) does not seem to be used

    /*** KOI series ***/
    // For Russian, we use CP1251
    if (!locale_match ("uk", locale))
        return "KOI8-U";

    if (!locale_match ("tg", locale))
        return "KOI8-T";

    /*** Asia ***/
    // Japanese
    if (!locale_match ("jp", locale))
        return "SHIFT-JIS"; // Shift-JIS is way more common than EUC-JP

    // Korean
    if (!locale_match ("ko", locale))
        return "EUC-KR";

    // Thai
    if (!locale_match ("th", locale))
        return "TIS-620";

    // Vietnamese (FIXME: more infos needed)
    if (!locale_match ("vt", locale))
        /* VISCII is probably a bad idea as it is not extended ASCII */
        /* glibc has TCVN5712-1 */
        return "CP1258";

    /* Kazakh (FIXME: more infos needed) */
    if (!locale_match ("kk", locale))
        return "PT154";

    // Chinese. The politically incompatible character sets.
    if (!locale_match ("zh", locale))
    {
        if ((strlen (locale) >= 5) && (locale[2] != '_'))
            locale += 3;

        // Hong Kong
        if (!locale_match ("HK", locale))
            return "BIG5-HKSCS"; /* FIXME: use something else? */

        // Taiwan island
        if (!locale_match ("TW", locale))
            return "BIG5";

        // People's Republic of China and Singapore
        /*
         * GB18030 can represent any Unicode code point
         * (like UTF-8), while remaining compatible with GBK
         * FIXME: is it compatible with GB2312? if not, should we
         * use GB2312 instead?
         */
        return "GB18030";
    }

    return "ASCII";
}
#endif

/**
 * GetFallbackEncoding() suggests an encoding to be used for non UTF-8
 * text files accord to the system's local settings. It is only a best
 * guess.
 */
const char *GetFallbackEncoding( void )
{
#ifndef WIN32
    const char *psz_lang;

    /* Some systems (like Darwin, SunOS 4 or DJGPP) have only the C locale.
     * Therefore we don't use setlocale here; it would return "C". */
#  if defined (HAVE_SETLOCALE) && !defined ( __APPLE__)
    psz_lang = setlocale (LC_ALL, NULL);
    if (psz_lang == NULL)
#  endif
    {
        psz_lang = getenv ("LC_ALL");
        if ((psz_lang == NULL) || !*psz_lang)
        {
            psz_lang = getenv ("LC_CTYPE");
            if ((psz_lang == NULL))
                psz_lang = getenv ("LANG");
        }
    }

    return FindFallbackEncoding (psz_lang);
#else
    static char buf[16] = "";
    if (buf[0] == 0)
        snprintf (buf, sizeof (buf), "CP%u", GetACP ());
    return buf;
#endif
}
