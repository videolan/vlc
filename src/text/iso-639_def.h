/*****************************************************************************
 * iso_lang.h: languages codes and abbreviations
 *****************************************************************************
 * Copyright (C) 1998-2004 VLC authors and VideoLAN
 * $Id$
 *
 * This is used in iso_lang.cpp and is taken from the GNU glibc 2.2.5
 * tarball. It has been partially completed with native language names.
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr>
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

/* Define the languages codes and abbreviations according to ISO 639-[12].
 * Format is:( "English name", 639-1-code, 639-2/T-code, 639-2/B-code)
 * If you find something missing or wrong contact <bug-glibc@gnu.org> */

/* The following spellings were converted to pure ASCII. They should be
 * "translated" back to Unicode in the po/en*.po files:
 *  Provençal -> Provencal
 *  Volapük -> Volapuk
 *  Bokmål -> Bokmaal
 */

static const iso639_lang_t p_languages[] =
{
    { N_( "Afar" ),             "",            "aa", "aar", "aar" },
    { N_( "Abkhazian" ),        "",            "ab", "abk", "abk" },
    { N_( "Afrikaans" ),        "",            "af", "afr", "afr" },
    { N_( "Albanian" ),         "",            "sq", "sqi", "alb" },
    { N_( "Amharic" ),          "",            "am", "amh", "amh" },
    { N_( "Arabic" ),           "ﻉﺮﺒﻳ",        "ar", "ara", "ara" },
    { N_( "Armenian" ),         "",            "hy", "hye", "arm" },
    { N_( "Assamese" ),         "",            "as", "asm", "asm" },
    { N_( "Avestan" ),          "",            "ae", "ave", "ave" },
    { N_( "Aymara" ),           "",            "ay", "aym", "aym" },
    { N_( "Azerbaijani" ),      "",            "az", "aze", "aze" },
    { N_( "Bashkir" ),          "",            "ba", "bak", "bak" },
    { N_( "Basque" ),           "",            "eu", "eus", "baq" },
    { N_( "Belarusian" ),       "",            "be", "bel", "bel" },
    { N_( "Bengali" ),          "বাংলা",         "bn", "ben", "ben" },
    { N_( "Bihari" ),           "",            "bh", "bih", "bih" },
    { N_( "Bislama" ),          "",            "bi", "bis", "bis" },
    { N_( "Bosnian" ),          "",            "bs", "bos", "bos" },
    { N_( "Breton" ),           "",            "br", "bre", "bre" },
    { N_( "Bulgarian" ),        "български език", "bg", "bul", "bul" },
    { N_( "Burmese" ),          "",            "my", "mya", "bur" },
    { N_( "Catalan" ),          "Català",      "ca", "cat", "cat" },
    { N_( "Chamorro" ),         "",            "ch", "cha", "cha" },
    { N_( "Chechen" ),          "",            "ce", "che", "che" },
    { N_( "Chinese" ),          "简体中文",    "zh", "zho", "chi" },
    { N_( "Church Slavic" ),    "",            "cu", "chu", "chu" },
    { N_( "Chuvash" ),          "",            "cv", "chv", "chv" },
    { N_( "Cornish" ),          "",            "kw", "cor", "cor" },
    { N_( "Corsican" ),         "",            "co", "cos", "cos" },
    { N_( "Czech" ),            "Čeština",     "cs", "ces", "cze" },
    { N_( "Danish" ),           "Dansk",       "da", "dan", "dan" },
    { N_( "Dutch" ),            "Nederlands",  "nl", "nld", "dut" },
    { N_( "Dzongkha" ),         "",            "dz", "dzo", "dzo" },
    { N_( "English" ),          "English",     "en", "eng", "eng" },
    { N_( "Esperanto" ),        "",            "eo", "epo", "epo" },
    { N_( "Estonian" ),         "",            "et", "est", "est" },
    { N_( "Faroese" ),          "",            "fo", "fao", "fao" },
    { N_( "Fijian" ),           "",            "fj", "fij", "fij" },
    { N_( "Finnish" ),          "Suomi",       "fi", "fin", "fin" },
    { N_( "French" ),           "Français",    "fr", "fra", "fre" },
    { N_( "Frisian" ),          "",            "fy", "fry", "fry" },
    { N_( "Georgian" ),         "ქართული",     "ka", "kat", "geo" },
    { N_( "German" ),           "Deutsch",     "de", "deu", "ger" },
    { N_( "Gaelic (Scots)" ),   "",            "gd", "gla", "gla" },
    { N_( "Irish" ),            "",            "ga", "gle", "gle" },
    { N_( "Gallegan" ),         "Galego",      "gl", "glg", "glg" },
    { N_( "Manx" ),             "",            "gv", "glv", "glv" },
    { N_( "Greek, Modern ()" ), "",            "el", "gre", "ell" },
    { N_( "Guarani" ),          "",            "gn", "grn", "grn" },
    { N_( "Gujarati" ),         "",            "gu", "guj", "guj" },
    { N_( "Hebrew" ),           "עברית",       "he", "heb", "heb" },
    { N_( "Herero" ),           "",            "hz", "her", "her" },
    { N_( "Hindi" ),            "",            "hi", "hin", "hin" },
    { N_( "Hiri Motu" ),        "",            "ho", "hmo", "hmo" },
    { N_( "Hungarian" ),        "Magyar",      "hu", "hun", "hun" },
    { N_( "Icelandic" ),        "Islenska",    "is", "isl", "ice" },
    { N_( "Inuktitut" ),        "",            "iu", "iku", "iku" },
    { N_( "Interlingue" ),      "",            "ie", "ile", "ile" },
    { N_( "Interlingua" ),      "",            "ia", "ina", "ina" },
    { N_( "Indonesian" ),       "Bahasa Indonesia", "id", "ind", "ind" },
    { N_( "Inupiaq" ),          "",            "ik", "ipk", "ipk" },
    { N_( "Italian" ),          "Italiano",    "it", "ita", "ita" },
    { N_( "Javanese" ),         "",            "jv", "jaw", "jav" },
    { N_( "Japanese" ),         "日本語",      "ja", "jpn", "jpn" },
    { N_( "Kalaallisut (Greenlandic)" ), "",   "kl", "kal", "kal" },
    { N_( "Kannada" ),          "",            "kn", "kan", "kan" },
    { N_( "Kashmiri" ),         "",            "ks", "kas", "kas" },
    { N_( "Kazakh" ),           "",            "kk", "kaz", "kaz" },
    { N_( "Khmer" ),            "",            "km", "khm", "khm" },
    { N_( "Kikuyu" ),           "",            "ki", "kik", "kik" },
    { N_( "Kinyarwanda" ),      "",            "rw", "kin", "kin" },
    { N_( "Kirghiz" ),          "",            "ky", "kir", "kir" },
    { N_( "Komi" ),             "",            "kv", "kom", "kom" },
    { N_( "Korean" ),           "한국어",      "ko", "kor", "kor" },
    { N_( "Kuanyama" ),         "",            "kj", "kua", "kua" },
    { N_( "Kurdish" ),          "",            "ku", "kur", "kur" },
    { N_( "Lao" ),              "",            "lo", "lao", "lao" },
    { N_( "Latin" ),            "",            "la", "lat", "lat" },
    { N_( "Latvian" ),          "",            "lv", "lav", "lav" },
    { N_( "Lingala" ),          "",            "ln", "lin", "lin" },
    { N_( "Lithuanian" ),       "",            "lt", "lit", "lit" },
    { N_( "Letzeburgesch" ),    "",            "lb", "ltz", "ltz" },
    { N_( "Macedonian" ),       "",            "mk", "mkd", "mac" },
    { N_( "Marshall" ),         "",            "mh", "mah", "mah" },
    { N_( "Malayalam" ),        "",            "ml", "mal", "mal" },
    { N_( "Maori" ),            "",            "mi", "mri", "mao" },
    { N_( "Marathi" ),          "",            "mr", "mar", "mar" },
    { N_( "Malay" ),            "Melayu",      "ms", "msa", "may" },
    { N_( "Malagasy" ),         "",            "mg", "mlg", "mlg" },
    { N_( "Maltese" ),          "",            "mt", "mlt", "mlt" },
    { N_( "Moldavian" ),        "",            "mo", "mol", "mol" },
    { N_( "Mongolian" ),        "",            "mn", "mon", "mon" },
    { N_( "Nauru" ),            "",            "na", "nau", "nau" },
    { N_( "Navajo" ),           "",            "nv", "nav", "nav" },
    { N_( "Ndebele, South" ),   "",            "nr", "nbl", "nbl" },
    { N_( "Ndebele, North" ),   "",            "nd", "nde", "nde" },
    { N_( "Ndonga" ),           "",            "ng", "ndo", "ndo" },
    { N_( "Nepali" ),           "",            "ne", "nep", "nep" },
    { N_( "Norwegian" ),        "Norsk",       "no", "nor", "nor" },
    { N_( "Norwegian Nynorsk" ), "",           "nn", "nno", "nno" },
    { N_( "Norwegian Bokmaal" ), "",           "nb", "nob", "nob" },
    { N_( "Chichewa; Nyanja" ), "",            "ny", "nya", "nya" },
    { N_( "Occitan (post 1500); Provencal" ), "Occitan", "oc", "oci", "oci" },
    { N_( "Oriya" ),            "",            "or", "ori", "ori" },
    { N_( "Oromo" ),            "",            "om", "orm", "orm" },
    { N_( "On Screen Display" ),"On Screen Display", "od", "osd", "osd" },
    { N_( "Ossetian; Ossetic" ), "",           "os", "oss", "oss" },
    { N_( "Panjabi" ),          "ਪੰਜਾਬੀ",         "pa", "pan", "pan" },
    { N_( "Persian" ),          "ﻑﺍﺮﺳی",       "fa", "fas", "per" },
    { N_( "Pali" ),             "",            "pi", "pli", "pli" },
    { N_( "Polish" ),           "Polski",      "pl", "pol", "pol" },
    { N_( "Portuguese" ),       "Português",   "pt", "por", "por" },
    { N_( "Pushto" ),           "",            "ps", "pus", "pus" },
    { N_( "Quechua" ),          "",            "qu", "que", "que" },
    { N_( "Original audio" ),   "",            "",   "qaa", "qaa" },
    { N_( "Raeto-Romance" ),    "",            "rm", "roh", "roh" },
    { N_( "Romanian" ),         "Română",      "ro", "ron", "rum" },
    { N_( "Rundi" ),            "",            "rn", "run", "run" },
    { N_( "Russian" ),          "Русский",     "ru", "rus", "rus" },
    { N_( "Sango" ),            "",            "sg", "sag", "sag" },
    { N_( "Sanskrit" ),         "",            "sa", "san", "san" },
    { N_( "Serbian" ),          "српски",      "sr", "srp", "scc" },
    { N_( "Croatian" ),         "Hrvatski",    "hr", "hrv", "scr" },
    { N_( "Sinhalese" ),        "",            "si", "sin", "sin" },
    { N_( "Slovak" ),           "Slovensky",   "sk", "slk", "slo" },
    { N_( "Slovenian" ),        "slovenščina", "sl", "slv", "slv" },
    { N_( "Northern Sami" ),    "",            "se", "sme", "sme" },
    { N_( "Samoan" ),           "",            "sm", "smo", "smo" },
    { N_( "Shona" ),            "",            "sn", "sna", "sna" },
    { N_( "Sindhi" ),           "",            "sd", "snd", "snd" },
    { N_( "Somali" ),           "",            "so", "som", "som" },
    { N_( "Sotho, Southern" ),  "",            "st", "sot", "sot" },
    { N_( "Spanish" ),          "Español",     "es", "spa", "spa" },
    { N_( "Sardinian" ),        "",            "sc", "srd", "srd" },
    { N_( "Swati" ),            "",            "ss", "ssw", "ssw" },
    { N_( "Sundanese" ),        "",            "su", "sun", "sun" },
    { N_( "Swahili" ),          "",            "sw", "swa", "swa" },
    { N_( "Swedish" ),          "Svenska",     "sv", "swe", "swe" },
    { N_( "Tahitian" ),         "",            "ty", "tah", "tah" },
    { N_( "Tamil" ),            "",            "ta", "tam", "tam" },
    { N_( "Tatar" ),            "",            "tt", "tat", "tat" },
    { N_( "Telugu" ),           "",            "te", "tel", "tel" },
    { N_( "Tajik" ),            "",            "tg", "tgk", "tgk" },
    { N_( "Tagalog" ),          "",            "tl", "tgl", "tgl" },
    { N_( "Thai" ),             "",            "th", "tha", "tha" },
    { N_( "Tibetan" ),          "",            "bo", "bod", "tib" },
    { N_( "Tigrinya" ),         "",            "ti", "tir", "tir" },
    { N_( "Tonga (Tonga Islands)" ), "",       "to", "ton", "ton" },
    { N_( "Tswana" ),           "",            "tn", "tsn", "tsn" },
    { N_( "Tsonga" ),           "",            "ts", "tso", "tso" },
    { N_( "Turkish" ),          "Türkçe",      "tr", "tur", "tur" },
    { N_( "Turkmen" ),          "",            "tk", "tuk", "tuk" },
    { N_( "Twi" ),              "",            "tw", "twi", "twi" },
    { N_( "Uighur" ),           "",            "ug", "uig", "uig" },
    { N_( "Ukrainian" ),        "українська мова", "uk", "ukr", "ukr" },
    { N_( "Urdu" ),             "",            "ur", "urd", "urd" },
    { N_( "Uzbek" ),            "",            "uz", "uzb", "uzb" },
    { N_( "Vietnamese" ),       "",            "vi", "vie", "vie" },
    { N_( "Volapuk" ),          "",            "vo", "vol", "vol" },
    { N_( "Welsh" ),            "",            "cy", "cym", "wel" },
    { N_( "Wolof" ),            "",            "wo", "wol", "wol" },
    { N_( "Xhosa" ),            "",            "xh", "xho", "xho" },
    { N_( "Yiddish" ),          "",            "yi", "yid", "yid" },
    { N_( "Yoruba" ),           "",            "yo", "yor", "yor" },
    { N_( "Zhuang" ),           "",            "za", "zha", "zha" },
    { N_( "Zulu" ),             "",            "zu", "zul", "zul" },

    /* XXX psz_eng_name MUST be NULL on the last item as it is the stop condition */
    { NULL,                   NULL,            "",   "",    "" }
};

