/*****************************************************************************
 * iso_lang.c: function to decode language code (in dvd or a52 for instance).
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: iso_lang.c,v 1.3 2001/12/30 07:09:56 sam Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdio.h>

#include <videolan/vlc.h>

/*****************************************************************************
 * Local tables
 *****************************************************************************/
static struct
{
    char    p_code[3];
    char    p_lang_long[20];
}

lang_tbl[] =
{
    /* The ISO 639 language codes.
     * Language names with * prefix are not spelled in their own language 
     */
    { "  ", "Not Specified" },
    { "aa", "*Afar" },
    { "ab", "*Abkhazian" },
    { "af", "*Afrikaans" },
    { "am", "*Amharic" },
    { "ar", "*Arabic" },
    { "as", "*Assamese" },
    { "ay", "*Aymara" },
    { "az", "*Azerbaijani" },
    { "ba", "*Bashkir" },
    { "be", "*Byelorussian" },
    { "bg", "*Bulgarian" },
    { "bh", "*Bihari" },
    { "bi", "*Bislama" },
    { "bn", "*Bengali; Bangla" },
    { "bo", "*Tibetan" },
    { "br", "*Breton" },
    { "ca", "*Catalan" },
    { "co", "*Corsican" },
    { "cs", "*Czech(Ceske)" },
    { "cy", "*Welsh" },
    { "da", "Dansk" },
    { "de", "Deutsch" },
    { "dz", "*Bhutani" },
    { "el", "*Greek" },
    { "en", "English" },
    { "eo", "*Esperanto" },
    { "es", "Espanol" },
    { "et", "*Estonian" },
    { "eu", "*Basque" },
    { "fa", "*Persian" },
    { "fi", "Suomi" },
    { "fj", "*Fiji" },
    { "fo", "*Faroese" },
    { "fr", "Francais" },
    { "fy", "*Frisian" },
    { "ga", "*Irish" },
    { "gd", "*Scots Gaelic" },
    { "gl", "*Galician" },
    { "gn", "*Guarani" },
    { "gu", "*Gujarati" },
    { "ha", "*Hausa" },
    { "he", "*Hebrew" },                                      /* formerly iw */
    { "hi", "*Hindi" },
    { "hr", "Hrvatski" },                                        /* Croatian */
    { "hu", "Magyar" },
    { "hy", "*Armenian" },
    { "ia", "*Interlingua" },
    { "id", "*Indonesian" },                                  /* formerly in */
    { "ie", "*Interlingue" },
    { "ik", "*Inupiak" },
    { "in", "*Indonesian" },                               /* replaced by id */
    { "is", "Islenska" },
    { "it", "Italiano" },
    { "iu", "*Inuktitut" },
    { "iw", "*Hebrew" },                                   /* replaced by he */
    { "ja", "*Japanese" },
    { "ji", "*Yiddish" },                                  /* replaced by yi */
    { "jw", "*Javanese" },
    { "ka", "*Georgian" },
    { "kk", "*Kazakh" },
    { "kl", "*Greenlandic" },
    { "km", "*Cambodian" },
    { "kn", "*Kannada" },
    { "ko", "*Korean" },
    { "ks", "*Kashmiri" },
    { "ku", "*Kurdish" },
    { "ky", "*Kirghiz" },
    { "la", "*Latin" },
    { "ln", "*Lingala" },
    { "lo", "*Laothian" },
    { "lt", "*Lithuanian" },
    { "lv", "*Latvian, Lettish" },
    { "mg", "*Malagasy" },
    { "mi", "*Maori" },
    { "mk", "*Macedonian" },
    { "ml", "*Malayalam" },
    { "mn", "*Mongolian" },
    { "mo", "*Moldavian" },
    { "mr", "*Marathi" },
    { "ms", "*Malay" },
    { "mt", "*Maltese" },
    { "my", "*Burmese" },
    { "na", "*Nauru" },
    { "ne", "*Nepali" },
    { "nl", "Nederlands" },
    { "no", "Norsk" },
    { "oc", "*Occitan" },
    { "om", "*(Afan) Oromo" },
    { "or", "*Oriya" },
    { "pa", "*Punjabi" },
    { "pl", "*Polish" },
    { "ps", "*Pashto, Pushto" },
    { "pt", "Portugues" },
    { "qu", "*Quechua" },
    { "rm", "*Rhaeto-Romance" },
    { "rn", "*Kirundi" },
    { "ro", "*Romanian"  },
    { "ru", "*Russian" },
    { "rw", "*Kinyarwanda" },
    { "sa", "*Sanskrit" },
    { "sd", "*Sindhi" },
    { "sg", "*Sangho" },
    { "sh", "*Serbo-Croatian" },
    { "si", "*Sinhalese" },
    { "sk", "*Slovak" },
    { "sl", "*Slovenian" },
    { "sm", "*Samoan" },
    { "sn", "*Shona"  },
    { "so", "*Somali" },
    { "sq", "*Albanian" },
    { "sr", "*Serbian" },
    { "ss", "*Siswati" },
    { "st", "*Sesotho" },
    { "su", "*Sundanese" },
    { "sv", "Svenska" },
    { "sw", "*Swahili" },
    { "ta", "*Tamil" },
    { "te", "*Telugu" },
    { "tg", "*Tajik" },
    { "th", "*Thai" },
    { "ti", "*Tigrinya" },
    { "tk", "*Turkmen" },
    { "tl", "*Tagalog" },
    { "tn", "*Setswana" },
    { "to", "*Tonga" },
    { "tr", "*Turkish" },
    { "ts", "*Tsonga" },
    { "tt", "*Tatar" },
    { "tw", "*Twi" },
    { "ug", "*Uighur" },
    { "uk", "*Ukrainian" },
    { "ur", "*Urdu" },
    { "uz", "*Uzbek" },
    { "vi", "*Vietnamese" },
    { "vo", "*Volapuk" },
    { "wo", "*Wolof" },
    { "xh", "*Xhosa" },
    { "yi", "*Yiddish" },                                     /* formerly ji */
    { "yo", "*Yoruba" },
    { "za", "*Zhuang" },
    { "zh", "*Chinese" },
    { "zu", "*Zulu" },
    { "\0", "" }
};

/*****************************************************************************
 * DecodeLanguage: gives the long language name from the two-letters
 *                 ISO-639 code
 *****************************************************************************/
char * DecodeLanguage( u16 i_code )
{
    int     i = 0;

    while( memcmp( lang_tbl[i].p_code, &i_code, 2 ) &&
           lang_tbl[i].p_lang_long[0] )
    {
        i++;
    }

    return lang_tbl[i].p_lang_long;
}

