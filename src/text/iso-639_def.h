/*****************************************************************************
 * iso-639_def.h: language codes and abbreviations
 *****************************************************************************
 * Copyright (C) 1998-2004 VLC authors and VideoLAN
 *
 * Definitions taken from GNU glibc.
 *
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

/* This table defines ISO-639-1, ISO-639-2T and ISO-639-2B languages codes and
 * their mappings to descriptive labels.
 *
 * The entries have been copied in bulk from the set defined in glibc, with
 * minimal adjustments. Corrections or additions, unless they pertain to custom
 * VLC adjustments, should generally be discussed with the glibc developers,
 * then updated here subsequently if accepted by glibc.
 *
 * The glibc ordering should be preserved to avoid making future bulk updates
 * harder. (Rare VLC additions belong at the end).
 */

static const iso639_lang_t p_languages[] =
{
    /* Definitions taken from GNU glibc */
    { N_( "Abkhazian" ),                      "ab", "abk", "abk" },
    { N_( "Afar" ),                           "aa", "aar", "aar" },
    { N_( "Afrikaans" ),                      "af", "afr", "afr" },
    { N_( "Albanian" ),                       "sq", "sqi", "alb" },
    { N_( "Amharic" ),                        "am", "amh", "amh" },
    { N_( "Arabic" ),                         "ar", "ara", "ara" },
    { N_( "Armenian" ),                       "hy", "hye", "arm" },
    { N_( "Assamese" ),                       "as", "asm", "asm" },
    { N_( "Avestan" ),                        "ae", "ave", "ave" },
    { N_( "Aymara" ),                         "ay", "aym", "aym" },
    { N_( "Azerbaijani" ),                    "az", "aze", "aze" },
    { N_( "Bashkir" ),                        "ba", "bak", "bak" },
    { N_( "Basque" ),                         "eu", "eus", "baq" },
    { N_( "Belarusian" ),                     "be", "bel", "bel" },
    { N_( "Bengali" ),                        "bn", "ben", "ben" },
    { N_( "Bihari" ),                         "bh", "bih", "bih" },
    { N_( "Bislama" ),                        "bi", "bis", "bis" },
    { N_( "Bosnian" ),                        "bs", "bos", "bos" },
    { N_( "Breton" ),                         "br", "bre", "bre" },
    { N_( "Bulgarian" ),                      "bg", "bul", "bul" },
    { N_( "Burmese" ),                        "my", "mya", "bur" },
    { N_( "Catalan" ),                        "ca", "cat", "cat" },
    { N_( "Chamorro" ),                       "ch", "cha", "cha" },
    { N_( "Chechen" ),                        "ce", "che", "che" },
    { N_( "Chichewa; Nyanja" ),               "ny", "nya", "nya" },
    { N_( "Chinese" ),                        "zh", "zho", "chi" },
    { N_( "Church Slavic" ),                  "cu", "chu", "chu" },
    { N_( "Chuvash" ),                        "cv", "chv", "chv" },
    { N_( "Cornish" ),                        "kw", "cor", "cor" },
    { N_( "Corsican" ),                       "co", "cos", "cos" },
    { N_( "Croatian" ),                       "hr", "hrv", "scr" },
    { N_( "Czech" ),                          "cs", "ces", "cze" },
    { N_( "Danish" ),                         "da", "dan", "dan" },
    { N_( "Dutch" ),                          "nl", "nld", "dut" },
    { N_( "Dzongkha" ),                       "dz", "dzo", "dzo" },
    { N_( "English" ),                        "en", "eng", "eng" },
    { N_( "Esperanto" ),                      "eo", "epo", "epo" },
    { N_( "Estonian" ),                       "et", "est", "est" },
    { N_( "Faroese" ),                        "fo", "fao", "fao" },
    { N_( "Fijian" ),                         "fj", "fij", "fij" },
    { N_( "Finnish" ),                        "fi", "fin", "fin" },
    { N_( "French" ),                         "fr", "fra", "fre" },
    { N_( "Frisian" ),                        "fy", "fry", "fry" },
    { N_( "Gaelic (Scots)" ),                 "gd", "gla", "gla" },
    { N_( "Gallegan" ),                       "gl", "glg", "glg" },
    { N_( "Georgian" ),                       "ka", "kat", "geo" },
    { N_( "German" ),                         "de", "deu", "ger" },
    { N_( "Greek, Modern" ),                  "el", "ell", "gre" },
    { N_( "Guarani" ),                        "gn", "grn", "grn" },
    { N_( "Gujarati" ),                       "gu", "guj", "guj" },
    { N_( "Hebrew" ),                         "he", "heb", "heb" },
    { N_( "Herero" ),                         "hz", "her", "her" },
    { N_( "Hindi" ),                          "hi", "hin", "hin" },
    { N_( "Hiri Motu" ),                      "ho", "hmo", "hmo" },
    { N_( "Hungarian" ),                      "hu", "hun", "hun" },
    { N_( "Icelandic" ),                      "is", "isl", "ice" },
    { N_( "Indonesian" ),                     "id", "ind", "ind" },
    { N_( "Interlingua" ),                    "ia", "ina", "ina" },
    { N_( "Interlingue" ),                    "ie", "ile", "ile" },
    { N_( "Inuktitut" ),                      "iu", "iku", "iku" },
    { N_( "Inupiaq" ),                        "ik", "ipk", "ipk" },
    { N_( "Irish" ),                          "ga", "gle", "gle" },
    { N_( "Italian" ),                        "it", "ita", "ita" },
    { N_( "Japanese" ),                       "ja", "jpn", "jpn" },
    { N_( "Javanese" ),                       "jv", "jav", "jav" },
    { N_( "Greenlandic, Kalaallisut" ),       "kl", "kal", "kal" },
    { N_( "Kannada" ),                        "kn", "kan", "kan" },
    { N_( "Kashmiri" ),                       "ks", "kas", "kas" },
    { N_( "Kazakh" ),                         "kk", "kaz", "kaz" },
    { N_( "Khmer" ),                          "km", "khm", "khm" },
    { N_( "Kikuyu" ),                         "ki", "kik", "kik" },
    { N_( "Kinyarwanda" ),                    "rw", "kin", "kin" },
    { N_( "Kirghiz" ),                        "ky", "kir", "kir" },
    { N_( "Komi" ),                           "kv", "kom", "kom" },
    { N_( "Korean" ),                         "ko", "kor", "kor" },
    { N_( "Kuanyama" ),                       "kj", "kua", "kua" },
    { N_( "Kurdish" ),                        "ku", "kur", "kur" },
    { N_( "Lao" ),                            "lo", "lao", "lao" },
    { N_( "Latin" ),                          "la", "lat", "lat" },
    { N_( "Latvian" ),                        "lv", "lav", "lav" },
    { N_( "Lingala" ),                        "ln", "lin", "lin" },
    { N_( "Lithuanian" ),                     "lt", "lit", "lit" },
    { N_( "Letzeburgesch" ),                  "lb", "ltz", "ltz" },
    { N_( "Macedonian" ),                     "mk", "mkd", "mac" },
    { N_( "Malagasy" ),                       "mg", "mlg", "mlg" },
    { N_( "Malayalam" ),                      "ml", "mal", "mal" },
    { N_( "Malay" ),                          "ms", "msa", "may" },
    { N_( "Maltese" ),                        "mt", "mlt", "mlt" },
    { N_( "Manx" ),                           "gv", "glv", "glv" },
    { N_( "Maori" ),                          "mi", "mri", "mao" },
    { N_( "Marathi" ),                        "mr", "mar", "mar" },
    { N_( "Marshall" ),                       "mh", "mah", "mah" },
    { N_( "Moldavian" ),                      "mo", "mol", "mol" },
    { N_( "Mongolian" ),                      "mn", "mon", "mon" },
    { N_( "Nauru" ),                          "na", "nau", "nau" },
    { N_( "Navajo" ),                         "nv", "nav", "nav" },
    { N_( "Ndebele, North" ),                 "nd", "nde", "nde" },
    { N_( "Ndebele, South" ),                 "nr", "nbl", "nbl" },
    { N_( "Ndonga" ),                         "ng", "ndo", "ndo" },
    { N_( "Nepali" ),                         "ne", "nep", "nep" },
    { N_( "Northern Sami" ),                  "se", "sme", "sme" },
    { N_( "Norwegian Bokmaal" ),              "nb", "nob", "nob" },
    { N_( "Norwegian Nynorsk" ),              "nn", "nno", "nno" },
    { N_( "Norwegian" ),                      "no", "nor", "nor" },
    { N_( "Occitan; Provençal" ),             "oc", "oci", "oci" },
    { N_( "Oriya" ),                          "or", "ori", "ori" },
    { N_( "Oromo" ),                          "om", "orm", "orm" },
    { N_( "Ossetian; Ossetic" ),              "os", "oss", "oss" },
    { N_( "Pali" ),                           "pi", "pli", "pli" },
    { N_( "Panjabi" ),                        "pa", "pan", "pan" },
    { N_( "Persian" ),                        "fa", "fas", "per" },
    { N_( "Polish" ),                         "pl", "pol", "pol" },
    { N_( "Portuguese" ),                     "pt", "por", "por" },
    { N_( "Pushto" ),                         "ps", "pus", "pus" },
    { N_( "Quechua" ),                        "qu", "que", "que" },
    { N_( "Raeto-Romance" ),                  "rm", "roh", "roh" },
    { N_( "Romanian" ),                       "ro", "ron", "rum" },
    { N_( "Rundi" ),                          "rn", "run", "run" },
    { N_( "Russian" ),                        "ru", "rus", "rus" },
    { N_( "Samoan" ),                         "sm", "smo", "smo" },
    { N_( "Sango" ),                          "sg", "sag", "sag" },
    { N_( "Sanskrit" ),                       "sa", "san", "san" },
    { N_( "Sardinian" ),                      "sc", "srd", "srd" },
    { N_( "Serbian" ),                        "sr", "srp", "scc" },
    { N_( "Shona" ),                          "sn", "sna", "sna" },
    { N_( "Sindhi" ),                         "sd", "snd", "snd" },
    { N_( "Sinhalese" ),                      "si", "sin", "sin" },
    { N_( "Slovak" ),                         "sk", "slk", "slo" },
    { N_( "Slovenian" ),                      "sl", "slv", "slv" },
    { N_( "Somali" ),                         "so", "som", "som" },
    { N_( "Sotho, Southern" ),                "st", "sot", "sot" },
    { N_( "Spanish" ),                        "es", "spa", "spa" },
    { N_( "Sundanese" ),                      "su", "sun", "sun" },
    { N_( "Swahili" ),                        "sw", "swa", "swa" },
    { N_( "Swati" ),                          "ss", "ssw", "ssw" },
    { N_( "Swedish" ),                        "sv", "swe", "swe" },
    { N_( "Tagalog" ),                        "tl", "tgl", "tgl" },
    { N_( "Tahitian" ),                       "ty", "tah", "tah" },
    { N_( "Tajik" ),                          "tg", "tgk", "tgk" },
    { N_( "Tamil" ),                          "ta", "tam", "tam" },
    { N_( "Tatar" ),                          "tt", "tat", "tat" },
    { N_( "Telugu" ),                         "te", "tel", "tel" },
    { N_( "Thai" ),                           "th", "tha", "tha" },
    { N_( "Tibetan" ),                        "bo", "bod", "tib" },
    { N_( "Tigrinya" ),                       "ti", "tir", "tir" },
    { N_( "Tonga (Tonga Islands)" ),          "to", "ton", "ton" },
    { N_( "Tsonga" ),                         "ts", "tso", "tso" },
    { N_( "Tswana" ),                         "tn", "tsn", "tsn" },
    { N_( "Turkish" ),                        "tr", "tur", "tur" },
    { N_( "Turkmen" ),                        "tk", "tuk", "tuk" },
    { N_( "Twi" ),                            "tw", "twi", "twi" },
    { N_( "Uighur" ),                         "ug", "uig", "uig" },
    { N_( "Ukrainian" ),                      "uk", "ukr", "ukr" },
    { N_( "Urdu" ),                           "ur", "urd", "urd" },
    { N_( "Uzbek" ),                          "uz", "uzb", "uzb" },
    { N_( "Vietnamese" ),                     "vi", "vie", "vie" },
    { N_( "Volapuk" ),                        "vo", "vol", "vol" },
    { N_( "Welsh" ),                          "cy", "cym", "wel" },
    { N_( "Wolof" ),                          "wo", "wol", "wol" },
    { N_( "Xhosa" ),                          "xh", "xho", "xho" },
    { N_( "Yiddish" ),                        "yi", "yid", "yid" },
    { N_( "Yoruba" ),                         "yo", "yor", "yor" },
    { N_( "Zhuang" ),                         "za", "zha", "zha" },
    { N_( "Zulu" ),                           "zu", "zul", "zul" },

    /* Custom VLC additions */
    { N_( "On Screen Display" ),              "od", "osd", "osd" },
    { N_( "Original audio" ),                 "",   "qaa", "qaa" },
    { N_( "Hebrew" ),                         "iw", "heb", "heb" }, /* for old DVDs */

    /* End marker */
    { NULL,                                   "",   "",    "" }
};

