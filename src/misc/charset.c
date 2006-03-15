/*****************************************************************************
 * charset.c: Locale's character encoding stuff.
 *****************************************************************************
 * See also unicode.c for Unicode to locale conversion helpers.
 *
 * Copyright (C) 2003-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Derk-Jan Hartman <thedj at users.sf.net>
 *          Christophe Massiot
 *          Rémi Denis-Courmont
 *
 * vlc_current_charset() an adaption of mp_locale_charset():
 *
 *  Copyright (C) 2001-2003 The Mape Project
 *  Written by Karel Zak  <zakkr@zf.jcu.cz>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <vlc/vlc.h>

#if !defined WIN32
# if HAVE_LANGINFO_CODESET
#  include <langinfo.h>
# else
#  if HAVE_SETLOCALE
#   include <locale.h>
#  endif
# endif
#elif defined WIN32
# include <windows.h>
#endif

#ifdef __APPLE__
#   include <errno.h>
#   include <string.h>
#endif

#include "charset.h"

typedef struct VLCCharsetAlias
{
    char *psz_alias, *psz_name;
} VLCCharsetAlias;

/*
 * The libcharset load all from external text file, but it's strange and
 * slow solution, we rather use array(s) compiled into source. In the
 * "good" libc this is not needful -- for example in linux.
 *
 * Please, put to this funtion exotic aliases only. The libc 'iconv' knows
 * a lot of basic aliases (check it first by iconv -l).
 *
 */
#if (defined OS2 || !HAVE_LANGINFO_CODESET) && !defined WIN32
static const char* vlc_encoding_from_language( const char *l )
{
    /* check for language (and perhaps country) codes */
    if (strstr(l, "zh_TW")) return "Big5";
    if (strstr(l, "zh_HK")) return "Big5HKSCS";   /* no MIME charset */
    if (strstr(l, "zh")) return "GB2312";
    if (strstr(l, "th")) return "TIS-620";
    if (strstr(l, "ja")) return "EUC-JP";
    if (strstr(l, "ko")) return "EUC-KR";
    if (strstr(l, "ru")) return "KOI8-R";
    if (strstr(l, "uk")) return "KOI8-U";
    if (strstr(l, "pl") || strstr(l, "hr") ||
        strstr(l, "hu") || strstr(l, "cs") ||
        strstr(l, "sk") || strstr(l, "sl")) return "ISO-8859-2";
    if (strstr(l, "eo") || strstr(l, "mt")) return "ISO-8859-3";
    if (strstr(l, "lt") || strstr(l, "la")) return "ISO-8859-4";
    if (strstr(l, "bg") || strstr(l, "be") ||
        strstr(l, "mk") || strstr(l, "uk")) return "ISO-8859-5";
    if (strstr(l, "ar")) return "ISO-8859-6";
    if (strstr(l, "el")) return "ISO-8859-7";
    if (strstr(l, "he") || strstr(l, "iw")) return "ISO-8859-8";
    if (strstr(l, "tr")) return "ISO-8859-9";
    if (strstr(l, "th")) return "ISO-8859-11";
    if (strstr(l, "lv")) return "ISO-8859-13";
    if (strstr(l, "cy")) return "ISO-8859-14";
    if (strstr(l, "et")) return "ISO-8859-15"; /* all latin1 could be iso15 as well */
    if (strstr(l, "ro")) return "ISO-8859-2";   /* or ISO-8859-16 */
    if (strstr(l, "am") || strstr(l, "vi")) return "UTF-8";
    /* We don't know. This ain't working go to default. */
    return "ISO-8859-1";
}
#endif

static const char* vlc_charset_aliases( const char *psz_name )
{
    VLCCharsetAlias     *a;

#if defined WIN32
    VLCCharsetAlias aliases[] =
    {
        { "CP936",      "GBK" },
        { "CP1361",     "JOHAB" },
        { "CP20127",    "ASCII" },
        { "CP20866",    "KOI8-R" },
        { "CP21866",    "KOI8-RU" },
        { "CP28591",    "ISO-8859-1" },
        { "CP28592",    "ISO-8859-2" },
        { "CP28593",    "ISO-8859-3" },
        { "CP28594",    "ISO-8859-4" },
        { "CP28595",    "ISO-8859-5" },
        { "CP28596",    "ISO-8859-6" },
        { "CP28597",    "ISO-8859-7" },
        { "CP28598",    "ISO-8859-8" },
        { "CP28599",    "ISO-8859-9" },
        { "CP28605",    "ISO-8859-15" },
        { NULL,         NULL }
    };
#elif SYS_AIX
    VLCCharsetAlias aliases[] =
    {
        { "IBM-850",    "CP850" },
        { "IBM-856",    "CP856" },
        { "IBM-921",    "ISO-8859-13" },
        { "IBM-922",    "CP922" },
        { "IBM-932",    "CP932" },
        { "IBM-943",    "CP943" },
        { "IBM-1046",   "CP1046" },
        { "IBM-1124",   "CP1124" },
        { "IBM-1129",   "CP1129" },
        { "IBM-1252",   "CP1252" },
        { "IBM-EUCCN",  "GB2312" },
        { "IBM-EUCJP",  "EUC-JP" },
        { "IBM-EUCKR",  "EUC-KR" },
        { "IBM-EUCTW",  "EUC-TW" },
        { NULL, NULL }
    };
#elif SYS_HPUX
    VLCCharsetAlias aliases[] =
    {
        { "ROMAN8",     "HP-ROMAN8" },
        { "ARABIC8",    "HP-ARABIC8" },
        { "GREEK8",     "HP-GREEK8" },
        { "HEBREW8",    "HP-HEBREW8" },
        { "TURKISH8",   "HP-TURKISH8" },
        { "KANA8",      "HP-KANA8" },
        { "HP15CN",     "GB2312" },
        { NULL, NULL }
    };
#elif SYS_IRIX
    VLCCharsetAlias aliases[] =
    {
        { "EUCCN",      "GB2312" },
        { NULL, NULL }
    };
#elif SYS_OSF
    VLCCharsetAlias aliases[] =
    {
        { "KSC5601",    "CP949" },
        { "SDECKANJI",  "EUC-JP" },
        { "TACTIS",     "TIS-620" },
        { NULL, NULL }
    };
#elif SYS_SOLARIS
    VLCCharsetAlias aliases[] =
    {
        { "646",        "ASCII" },
        { "CNS11643",   "EUC-TW" },
        { "5601",       "EUC-KR" },
        { "JOHAP92",    "JOHAB" },
        { "PCK",        "SHIFT_JIS" },
        { "2533",       "TIS-620" },
        { NULL, NULL }
    };
#elif SYS_BSD
    VLCCharsetAlias aliases[] =
    {
        { "646", " ASCII" },
        { "EUCCN", "GB2312" },
        { NULL, NULL }
    };
#else
    VLCCharsetAlias aliases[] = {{NULL, NULL}};
#endif

    for (a = aliases; a->psz_alias; a++)
        if (strcasecmp (a->psz_alias, psz_name) == 0)
            return a->psz_name;

    /* we return original name beacuse iconv() probably will know
     * something better about name if we don't know it :-) */
    return psz_name;
}

/* Returns charset from "language_COUNTRY.charset@modifier" string */
#if (defined OS2 || !HAVE_LANGINFO_CODESET) && !defined WIN32
static void vlc_encoding_from_locale( char *psz_locale, char *psz_charset )
{
    char *psz_dot = strchr( psz_locale, '.' );

    if( psz_dot != NULL )
    {
        const char *psz_modifier;

        psz_dot++;

        /* Look for the possible @... trailer and remove it, if any.  */
        psz_modifier = strchr( psz_dot, '@' );

        if( psz_modifier == NULL )
        {
            strcpy( psz_charset, psz_dot );
            return;
        }
        if( 0 < ( psz_modifier - psz_dot )
             && ( psz_modifier - psz_dot ) < 2 + 10 + 1 )
        {
            memcpy( psz_charset, psz_dot, psz_modifier - psz_dot );
            psz_charset[ psz_modifier - psz_dot ] = '\0';
            return;
        }
    }
    /* try language mapping */
    strcpy( psz_charset, vlc_encoding_from_language( psz_locale ) );
}
#endif

vlc_bool_t vlc_current_charset( char **psz_charset )
{
    const char *psz_codeset;

#if !(defined WIN32 || defined OS2 || defined __APPLE__)

# if HAVE_LANGINFO_CODESET
    /* Most systems support nl_langinfo( CODESET ) nowadays.  */
    psz_codeset = nl_langinfo( CODESET );
    if( !strcmp( psz_codeset, "ANSI_X3.4-1968" ) )
        psz_codeset = "ASCII";
# else
    /* On old systems which lack it, use setlocale or getenv.  */
    const char *psz_locale = NULL;
    char buf[2 + 10 + 1];

    /* But most old systems don't have a complete set of locales.  Some
     * (like SunOS 4 or DJGPP) have only the C locale.  Therefore we don't
     * use setlocale here; it would return "C" when it doesn't support the
     * locale name the user has set. Darwin's setlocale is broken. */
#  if HAVE_SETLOCALE && !__APPLE__
    psz_locale = setlocale( LC_ALL, NULL );
#  endif
    if( psz_locale == NULL || psz_locale[0] == '\0' )
    {
        psz_locale = getenv( "LC_ALL" );
        if( psz_locale == NULL || psz_locale[0] == '\0' )
        {
            psz_locale = getenv( "LC_CTYPE" );
            if( psz_locale == NULL || psz_locale[0] == '\0')
                psz_locale = getenv( "LANG" );
        }
    }

    /* On some old systems, one used to set locale = "iso8859_1". On others,
     * you set it to "language_COUNTRY.charset". Darwin only has LANG :( */
    vlc_encoding_from_locale( (char *)psz_locale, buf );
    psz_codeset =  buf;
# endif /* HAVE_LANGINFO_CODESET */

#elif defined __APPLE__

    /* Darwin is always using UTF-8 internally. */
    psz_codeset = "UTF-8";

#elif defined WIN32

    char buf[2 + 10 + 1];

    /* Woe32 has a function returning the locale's codepage as a number.  */
    sprintf( buf, "CP%u", GetACP() );
    psz_codeset = buf;

#elif defined OS2

    const char *psz_locale;
    char buf[2 + 10 + 1];
    ULONG cp[3];
    ULONG cplen;

    /* Allow user to override the codeset, as set in the operating system,
     * with standard language environment variables. */
    psz_locale = getenv( "LC_ALL" );
    if( psz_locale == NULL || psz_locale[0] == '\0' )
    {
        psz+locale = getenv( "LC_CTYPE" );
        if( psz_locale == NULL || locale[0] == '\0' )
            locale = getenv( "LANG" );
    }
    if( psz_locale != NULL && psz_locale[0] != '\0' )
        vlc_encoding_from_locale( psz_locale, buf );
        psz_codeset = buf;
    else
    {
        /* OS/2 has a function returning the locale's codepage as a number. */
        if( DosQueryCp( sizeof( cp ), cp, &cplen ) )
            psz_codeset = "";
        else
        {
            sprintf( buf, "CP%u", cp[0] );
            psz_codeset = buf;
        }
    }
#endif
    if( psz_codeset == NULL )
        /* The canonical name cannot be determined. */
        psz_codeset = "";
    else
        psz_codeset = vlc_charset_aliases( psz_codeset );

    /* Don't return an empty string.  GNU libc and GNU libiconv interpret
     * the empty string as denoting "the locale's character encoding",
     * thus GNU libiconv would call this function a second time. */
    if( psz_codeset[0] == '\0' )
    {
        /* Last possibility is 'CHARSET' enviroment variable */
        if( !( psz_codeset = getenv( "CHARSET" ) ) )
            psz_codeset = "ISO-8859-1";
    }

    if( psz_charset )
        *psz_charset = strdup(psz_codeset);

    if( !strcasecmp(psz_codeset, "UTF8") || !strcasecmp(psz_codeset, "UTF-8") )
        return VLC_TRUE;

    return VLC_FALSE;
}

char *__vlc_fix_readdir_charset( vlc_object_t *p_this, const char *psz_string )
{
#ifdef __APPLE__
    if ( p_this->p_libvlc->iconv_macosx != (vlc_iconv_t)-1 )
    {
        const char *psz_in = psz_string;
        size_t i_in = strlen(psz_in);
        size_t i_out = i_in * 2;
        char *psz_utf8 = malloc(i_out + 1);
        char *psz_out = psz_utf8;

        vlc_mutex_lock( &p_this->p_libvlc->iconv_lock );
        size_t i_ret = vlc_iconv( p_this->p_libvlc->iconv_macosx,
                                  &psz_in, &i_in, &psz_out, &i_out );
        vlc_mutex_unlock( &p_this->p_libvlc->iconv_lock );
        if( i_ret == (size_t)-1 || i_in )
        {
            msg_Warn( p_this,
                      "failed to convert \"%s\" from HFS+ charset (%s)",
                      psz_string, strerror(errno) );
            free( psz_utf8 );
            return strdup( psz_string );
        }

        *psz_out = '\0';
        return psz_utf8;
    }
#endif

    return strdup( psz_string );
}

/**
 * @return a fallback characters encoding to be used, given a locale.
 */
const char *FindFallbackEncoding( const char *locale )
{
    if( ( locale == NULL ) || ( strlen( locale ) < 2 ) )
        return "ASCII";

    switch( U16_AT( locale ) )
    {
        /*** The ISO-8859 series (anything but Asia) ***/
        /* Latin-1 Western-European languages (ISO-8859-1) */
        case 'aa':
        case 'af':
        case 'an':
        case 'br':
        case 'ca':
        case 'da':
        case 'de':
        case 'en':
        case 'es':
        case 'et':
        case 'eu':
        case 'fi':
        case 'fo':
        case 'fr':
        case 'ga':
        case 'gd':
        case 'gl':
        case 'gv':
        case 'id':
        case 'is':
        case 'it':
        case 'kl':
        case 'kw':
        case 'mg':
        case 'ms':
        case 'nb':
        case 'nl':
        case 'nn':
        case 'no':
        case 'oc':
        case 'om':
        case 'pt':
        case 'so':
        case 'sq':
        case 'st':
        case 'sv':
        case 'tl':
        case 'uz':
        case 'wa':
        case 'xh':
        case 'zu':
            /* Compatible Microsoft superset */
            return "CP1252";

        /* Latin-2 Slavic languages (ISO-8859-2) */
        case 'bs':
        case 'cs':
        case 'hr':
        case 'hu':
        case 'pl':
        case 'ro':
        case 'sk':
        case 'sl':
            /* CP1250 is more common, but incompatible */
            return "CP1250";

        /* Latin-3 Southern European languages (ISO-8859-3) */
        case 'eo':
        case 'mt':
        /*case 'tr': Turkish uses ISO-8859-9 instead */
            return "ISO-8859-3";

        /* Latin-4 North-European languages (ISO-8859-4) */
        /* All use Latin-1 or Latin-6 instead */

        /* Cyrillic alphabet languages (ISO-8859-5) */
        case 'be':
        case 'bg':
        case 'mk':
        case 'ru':
        case 'sr':
            /* KOI8, ISO-8859-5 and CP1251 are supposedly incompatible */
            return "CP1251";

        /* Arabic (ISO-8859-6) */
        case 'ar':
            /* FIXME: someone check if we should return CP1256
             * or ISO-8859-6 */
            /* CP1256 is(?) more common, but incompatible(?) */
            return "CP1256";

        /* Greek (ISO-8859-7) */
        case 'el':
            /* FIXME: someone check if we should return CP1253
            * or ISO-8859-7 */
            /* CP1253 is(?) more common and partially compatible */
            return "CP1253";

        /* Hebrew (ISO-8859-8) */
        case 'he':
        case 'iw':
        case 'yi':
            /* Compatible Microsoft superset */
            return "CP1255";

        /* Latin-5 Turkish (ISO-8859-9) */
        case 'tr':
        case 'ku':
            /* Compatible Microsoft superset */
            return "CP1254";

        /* Latin-6 “North-European” languages (ISO-8859-10) */
        /* It is so much north European that glibc only uses that for Luganda
         * which is spoken in Uganda... unless someone complains, I'm not
         * using this one; let's fallback to CP1252 here. */
        /* ISO-8859-11 does arguably not exist. Thai is handled below. */
        /* ISO-8859-12 really doesn't exist. */

        /* Latin-7 Baltic languages (ISO-8859-13) */
        case 'lt':
        case 'lv':
        case 'mi': /* FIXME: ??? that's in New Zealand, doesn't sound baltic */
            /* Compatible Microsoft superset */
            return "CP1257";

        /* Latin-8 Celtic languages (ISO-8859-14) */
        case 'cy':
            return "ISO-8859-14";

        /* Latin-9 (ISO-8859-15) -> see Latin-1 */
        /* Latin-10 (ISO-8859-16) does not seem to be used */

        /* KOI series */
        /* For Russian, we use CP1251 */
        case 'uk':
            return "KOI8-U";
        case 'tg':
            return "KOI8-T";

        /*** Asia ***/
        case 'jp': /* Japanese */
            /* Shift-JIS is way more common than EUC-JP */
            return "SHIFT-JIS";
        case 'ko': /* Korean */
            return "EUC-KR";
        case 'th': /* Thai */
            return "TIS-620";
        case 'vt': /* Vietnamese FIXME: infos needed */
            /* VISCII is probably a bad idea as it is not extended ASCII */
            /* glibc has TCVN5712-1, but I could find no infos on this one */
            return "CP1258";

        case 'kk': /* Kazakh FIXME: infos needed */
            return "PT154";

        case 'zh': /* Chinese, charset is country dependant */
            if( ( strlen( locale ) >= 5 ) && ( locale[2] != '_' ) )
                switch( U16_AT( locale + 3 ) )
                {
                    case 'HK': /* Hong Kong */
                        /* FIXME: use something else? */
                        return "BIG5-HKSCS";

                    case 'TW': /* Taiwan */
                        return "BIG5";
                }
            /* People's Republic of China */
            /* Singapore */
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

/**
 * GetFallbackEncoding() suggests an encoding to be used for non UTF-8
 * text files accord to the system's local settings. It is only a best
 * guess.
 */
const char *GetFallbackEncoding( void )
{
#if HAVE_SETLOCALE
    return FindFallbackEncoding( setlocale( LC_CTYPE, NULL ) );
#else
    return FindFallbackEncoding( NULL );
#endif
}

/**
 * There are two decimal separators in the computer world-wide locales:
 * dot (which is the american default), and comma (which is used in France,
 * the country with the most VLC developers, among others).
 *
 * i18n_strtod() has the same prototype as ANSI C strtod() but it accepts
 * either decimal separator when deserializing the string to a float number,
 * independant of the local computer setting.
 */
double i18n_strtod( const char *str, char **end )
{
    char *end_buf, e;
    double d;

    if( end == NULL )
        end = &end_buf;
    d = strtod( str, end );

    e = **end;
    if(( e == ',' ) || ( e == '.' ))
    {
        char dup[strlen( str ) + 1];
        strcpy( dup, str );

        if( dup == NULL )
            return d;

        dup[*end - str] = ( e == ',' ) ? '.' : ',';
        d = strtod( dup, end );
    }
    return d;
}

/**
 * i18n_atof() has the same prototype as ANSI C atof() but it accepts
 * either decimal separator when deserializing the string to a float number,
 * independant of the local computer setting.
 */
double i18n_atof( const char *str )
{
    return i18n_strtod( str, NULL );
}


/**
 * us_strtod() has the same prototype as ANSI C strtod() but it expects
 * a dot as decimal separator regardless of the system locale.
 */
double us_strtod( const char *str, char **end )
{
    char dup[strlen( str ) + 1], *ptr;
    double d;
    strcpy( dup, str );

    ptr = strchr( dup, ',' );
    if( ptr != NULL )
        *ptr = '\0';

    d = strtod( dup, &ptr );
    if( end != NULL )
        *end = (char *)&str[ptr - dup];

    return d;
}

/**
 * us_atof() has the same prototype as ANSI C atof() but it expects a dot
 * as decimal separator, regardless of the system locale.
 */
double us_atof( const char *str )
{
    return us_strtod( str, NULL );
}

