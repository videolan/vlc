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

#define LANG_CODE(a,b,c,d) { a,  b, c,  d },
#define LANG_CODE2(a,c)    { a, "", c, "" }, /* two param version (ISO-639-2/T only) */
#define LANG_CODE3(a,c,d)  { a, "", c,  d }, /* three param version (ISO-639-2/T&B only) */

static const iso639_lang_t p_languages[] =
{
    /* Definitions taken from GNU glibc */
    LANG_CODE  ( N_("Abkhazian"),                                         "ab", "abk", "abk" )
    LANG_CODE  ( N_("Afar"),                                              "aa", "aar", "aar" )
    LANG_CODE  ( N_("Afrikaans"),                                         "af", "afr", "afr" )
    LANG_CODE  ( N_("Albanian"),                                          "sq", "sqi", "alb" )
    LANG_CODE  ( N_("Amharic"),                                           "am", "amh", "amh" )
    LANG_CODE  ( N_("Arabic"),                                            "ar", "ara", "ara" )
    LANG_CODE  ( N_("Armenian"),                                          "hy", "hye", "arm" )
    LANG_CODE  ( N_("Assamese"),                                          "as", "asm", "asm" )
    LANG_CODE  ( N_("Avestan"),                                           "ae", "ave", "ave" )
    LANG_CODE  ( N_("Aymara"),                                            "ay", "aym", "aym" )
    LANG_CODE  ( N_("Azerbaijani"),                                       "az", "aze", "aze" )
    LANG_CODE  ( N_("Bashkir"),                                           "ba", "bak", "bak" )
    LANG_CODE  ( N_("Basque"),                                            "eu", "eus", "baq" )
    LANG_CODE  ( N_("Belarusian"),                                        "be", "bel", "bel" )
    LANG_CODE  ( N_("Bengali"),                                           "bn", "ben", "ben" )
    LANG_CODE  ( N_("Bihari"),                                            "bh", "bih", "bih" )
    LANG_CODE  ( N_("Bislama"),                                           "bi", "bis", "bis" )
    LANG_CODE  ( N_("Bosnian"),                                           "bs", "bos", "bos" )
    LANG_CODE  ( N_("Breton"),                                            "br", "bre", "bre" )
    LANG_CODE  ( N_("Bulgarian"),                                         "bg", "bul", "bul" )
    LANG_CODE  ( N_("Burmese"),                                           "my", "mya", "bur" )
    LANG_CODE  ( N_("Catalan"),                                           "ca", "cat", "cat" )
    LANG_CODE  ( N_("Chamorro"),                                          "ch", "cha", "cha" )
    LANG_CODE  ( N_("Chechen"),                                           "ce", "che", "che" )
    LANG_CODE  ( N_("Chichewa; Nyanja"),                                  "ny", "nya", "nya" )
    LANG_CODE  ( N_("Chinese"),                                           "zh", "zho", "chi" )
    LANG_CODE  ( N_("Church Slavic"),                                     "cu", "chu", "chu" )
    LANG_CODE  ( N_("Chuvash"),                                           "cv", "chv", "chv" )
    LANG_CODE  ( N_("Cornish"),                                           "kw", "cor", "cor" )
    LANG_CODE  ( N_("Corsican"),                                          "co", "cos", "cos" )
    LANG_CODE  ( N_("Croatian"),                                          "hr", "hrv", "scr" )
    LANG_CODE  ( N_("Czech"),                                             "cs", "ces", "cze" )
    LANG_CODE  ( N_("Danish"),                                            "da", "dan", "dan" )
    LANG_CODE  ( N_("Dutch"),                                             "nl", "nld", "dut" )
    LANG_CODE  ( N_("Dzongkha"),                                          "dz", "dzo", "dzo" )
    LANG_CODE  ( N_("English"),                                           "en", "eng", "eng" )
    LANG_CODE  ( N_("Esperanto"),                                         "eo", "epo", "epo" )
    LANG_CODE  ( N_("Estonian"),                                          "et", "est", "est" )
    LANG_CODE  ( N_("Faroese"),                                           "fo", "fao", "fao" )
    LANG_CODE  ( N_("Fijian"),                                            "fj", "fij", "fij" )
    LANG_CODE  ( N_("Finnish"),                                           "fi", "fin", "fin" )
    LANG_CODE  ( N_("French"),                                            "fr", "fra", "fre" )
    LANG_CODE  ( N_("Frisian"),                                           "fy", "fry", "fry" )
    LANG_CODE  ( N_("Gaelic (Scots)"),                                    "gd", "gla", "gla" )
    LANG_CODE  ( N_("Gallegan"),                                          "gl", "glg", "glg" )
    LANG_CODE  ( N_("Georgian"),                                          "ka", "kat", "geo" )
    LANG_CODE  ( N_("German"),                                            "de", "deu", "ger" )
    LANG_CODE  ( N_("Greek, Modern"),                                     "el", "ell", "gre" )
    LANG_CODE  ( N_("Guarani"),                                           "gn", "grn", "grn" )
    LANG_CODE  ( N_("Gujarati"),                                          "gu", "guj", "guj" )
    LANG_CODE  ( N_("Hebrew"),                                            "he", "heb", "heb" )
    LANG_CODE  ( N_("Herero"),                                            "hz", "her", "her" )
    LANG_CODE  ( N_("Hindi"),                                             "hi", "hin", "hin" )
    LANG_CODE  ( N_("Hiri Motu"),                                         "ho", "hmo", "hmo" )
    LANG_CODE  ( N_("Hungarian"),                                         "hu", "hun", "hun" )
    LANG_CODE  ( N_("Icelandic"),                                         "is", "isl", "ice" )
    LANG_CODE  ( N_("Indonesian"),                                        "id", "ind", "ind" )
    LANG_CODE  ( N_("Interlingua"),                                       "ia", "ina", "ina" )
    LANG_CODE  ( N_("Interlingue"),                                       "ie", "ile", "ile" )
    LANG_CODE  ( N_("Inuktitut"),                                         "iu", "iku", "iku" )
    LANG_CODE  ( N_("Inupiaq"),                                           "ik", "ipk", "ipk" )
    LANG_CODE  ( N_("Irish"),                                             "ga", "gle", "gle" )
    LANG_CODE  ( N_("Italian"),                                           "it", "ita", "ita" )
    LANG_CODE  ( N_("Japanese"),                                          "ja", "jpn", "jpn" )
    LANG_CODE  ( N_("Javanese"),                                          "jv", "jav", "jav" )
    LANG_CODE  ( N_("Greenlandic, Kalaallisut"),                          "kl", "kal", "kal" )
    LANG_CODE  ( N_("Kannada"),                                           "kn", "kan", "kan" )
    LANG_CODE  ( N_("Kashmiri"),                                          "ks", "kas", "kas" )
    LANG_CODE  ( N_("Kazakh"),                                            "kk", "kaz", "kaz" )
    LANG_CODE  ( N_("Khmer"),                                             "km", "khm", "khm" )
    LANG_CODE  ( N_("Kikuyu"),                                            "ki", "kik", "kik" )
    LANG_CODE  ( N_("Kinyarwanda"),                                       "rw", "kin", "kin" )
    LANG_CODE  ( N_("Kirghiz"),                                           "ky", "kir", "kir" )
    LANG_CODE  ( N_("Komi"),                                              "kv", "kom", "kom" )
    LANG_CODE  ( N_("Korean"),                                            "ko", "kor", "kor" )
    LANG_CODE  ( N_("Kuanyama"),                                          "kj", "kua", "kua" )
    LANG_CODE  ( N_("Kurdish"),                                           "ku", "kur", "kur" )
    LANG_CODE  ( N_("Lao"),                                               "lo", "lao", "lao" )
    LANG_CODE  ( N_("Latin"),                                             "la", "lat", "lat" )
    LANG_CODE  ( N_("Latvian"),                                           "lv", "lav", "lav" )
    LANG_CODE  ( N_("Lingala"),                                           "ln", "lin", "lin" )
    LANG_CODE  ( N_("Lithuanian"),                                        "lt", "lit", "lit" )
    LANG_CODE  ( N_("Letzeburgesch"),                                     "lb", "ltz", "ltz" )
    LANG_CODE  ( N_("Macedonian"),                                        "mk", "mkd", "mac" )
    LANG_CODE  ( N_("Malagasy"),                                          "mg", "mlg", "mlg" )
    LANG_CODE  ( N_("Malayalam"),                                         "ml", "mal", "mal" )
    LANG_CODE  ( N_("Malay"),                                             "ms", "msa", "may" )
    LANG_CODE  ( N_("Maltese"),                                           "mt", "mlt", "mlt" )
    LANG_CODE  ( N_("Manx"),                                              "gv", "glv", "glv" )
    LANG_CODE  ( N_("Maori"),                                             "mi", "mri", "mao" )
    LANG_CODE  ( N_("Marathi"),                                           "mr", "mar", "mar" )
    LANG_CODE  ( N_("Marshall"),                                          "mh", "mah", "mah" )
    LANG_CODE  ( N_("Moldavian"),                                         "mo", "mol", "mol" )
    LANG_CODE  ( N_("Mongolian"),                                         "mn", "mon", "mon" )
    LANG_CODE  ( N_("Nauru"),                                             "na", "nau", "nau" )
    LANG_CODE  ( N_("Navajo"),                                            "nv", "nav", "nav" )
    LANG_CODE  ( N_("Ndebele, North"),                                    "nd", "nde", "nde" )
    LANG_CODE  ( N_("Ndebele, South"),                                    "nr", "nbl", "nbl" )
    LANG_CODE  ( N_("Ndonga"),                                            "ng", "ndo", "ndo" )
    LANG_CODE  ( N_("Nepali"),                                            "ne", "nep", "nep" )
    LANG_CODE  ( N_("Northern Sami"),                                     "se", "sme", "sme" )
    LANG_CODE  ( N_("Norwegian Bokmaal"),                                 "nb", "nob", "nob" )
    LANG_CODE  ( N_("Norwegian Nynorsk"),                                 "nn", "nno", "nno" )
    LANG_CODE  ( N_("Norwegian"),                                         "no", "nor", "nor" )
    LANG_CODE  ( N_("Occitan; Provençal"),                                "oc", "oci", "oci" )
    LANG_CODE  ( N_("Oriya"),                                             "or", "ori", "ori" )
    LANG_CODE  ( N_("Oromo"),                                             "om", "orm", "orm" )
    LANG_CODE  ( N_("Ossetian; Ossetic"),                                 "os", "oss", "oss" )
    LANG_CODE  ( N_("Pali"),                                              "pi", "pli", "pli" )
    LANG_CODE  ( N_("Panjabi"),                                           "pa", "pan", "pan" )
    LANG_CODE  ( N_("Persian"),                                           "fa", "fas", "per" )
    LANG_CODE  ( N_("Polish"),                                            "pl", "pol", "pol" )
    LANG_CODE  ( N_("Portuguese"),                                        "pt", "por", "por" )
    LANG_CODE  ( N_("Pushto"),                                            "ps", "pus", "pus" )
    LANG_CODE  ( N_("Quechua"),                                           "qu", "que", "que" )
    LANG_CODE  ( N_("Raeto-Romance"),                                     "rm", "roh", "roh" )
    LANG_CODE  ( N_("Romanian"),                                          "ro", "ron", "rum" )
    LANG_CODE  ( N_("Rundi"),                                             "rn", "run", "run" )
    LANG_CODE  ( N_("Russian"),                                           "ru", "rus", "rus" )
    LANG_CODE  ( N_("Samoan"),                                            "sm", "smo", "smo" )
    LANG_CODE  ( N_("Sango"),                                             "sg", "sag", "sag" )
    LANG_CODE  ( N_("Sanskrit"),                                          "sa", "san", "san" )
    LANG_CODE  ( N_("Sardinian"),                                         "sc", "srd", "srd" )
    LANG_CODE  ( N_("Serbian"),                                           "sr", "srp", "scc" )
    LANG_CODE  ( N_("Shona"),                                             "sn", "sna", "sna" )
    LANG_CODE  ( N_("Sindhi"),                                            "sd", "snd", "snd" )
    LANG_CODE  ( N_("Sinhalese"),                                         "si", "sin", "sin" )
    LANG_CODE  ( N_("Slovak"),                                            "sk", "slk", "slo" )
    LANG_CODE  ( N_("Slovenian"),                                         "sl", "slv", "slv" )
    LANG_CODE  ( N_("Somali"),                                            "so", "som", "som" )
    LANG_CODE  ( N_("Sotho, Southern"),                                   "st", "sot", "sot" )
    LANG_CODE  ( N_("Spanish"),                                           "es", "spa", "spa" )
    LANG_CODE  ( N_("Sundanese"),                                         "su", "sun", "sun" )
    LANG_CODE  ( N_("Swahili"),                                           "sw", "swa", "swa" )
    LANG_CODE  ( N_("Swati"),                                             "ss", "ssw", "ssw" )
    LANG_CODE  ( N_("Swedish"),                                           "sv", "swe", "swe" )
    LANG_CODE  ( N_("Tagalog"),                                           "tl", "tgl", "tgl" )
    LANG_CODE  ( N_("Tahitian"),                                          "ty", "tah", "tah" )
    LANG_CODE  ( N_("Tajik"),                                             "tg", "tgk", "tgk" )
    LANG_CODE  ( N_("Tamil"),                                             "ta", "tam", "tam" )
    LANG_CODE  ( N_("Tatar"),                                             "tt", "tat", "tat" )
    LANG_CODE  ( N_("Telugu"),                                            "te", "tel", "tel" )
    LANG_CODE  ( N_("Thai"),                                              "th", "tha", "tha" )
    LANG_CODE  ( N_("Tibetan"),                                           "bo", "bod", "tib" )
    LANG_CODE  ( N_("Tigrinya"),                                          "ti", "tir", "tir" )
    LANG_CODE  ( N_("Tonga (Tonga Islands)"),                             "to", "ton", "ton" )
    LANG_CODE  ( N_("Tsonga"),                                            "ts", "tso", "tso" )
    LANG_CODE  ( N_("Tswana"),                                            "tn", "tsn", "tsn" )
    LANG_CODE  ( N_("Turkish"),                                           "tr", "tur", "tur" )
    LANG_CODE  ( N_("Turkmen"),                                           "tk", "tuk", "tuk" )
    LANG_CODE  ( N_("Twi"),                                               "tw", "twi", "twi" )
    LANG_CODE  ( N_("Uighur"),                                            "ug", "uig", "uig" )
    LANG_CODE  ( N_("Ukrainian"),                                         "uk", "ukr", "ukr" )
    LANG_CODE  ( N_("Urdu"),                                              "ur", "urd", "urd" )
    LANG_CODE  ( N_("Uzbek"),                                             "uz", "uzb", "uzb" )
    LANG_CODE  ( N_("Vietnamese"),                                        "vi", "vie", "vie" )
    LANG_CODE  ( N_("Volapuk"),                                           "vo", "vol", "vol" )
    LANG_CODE  ( N_("Welsh"),                                             "cy", "cym", "wel" )
    LANG_CODE  ( N_("Wolof"),                                             "wo", "wol", "wol" )
    LANG_CODE  ( N_("Xhosa"),                                             "xh", "xho", "xho" )
    LANG_CODE  ( N_("Yiddish"),                                           "yi", "yid", "yid" )
    LANG_CODE  ( N_("Yoruba"),                                            "yo", "yor", "yor" )
    LANG_CODE  ( N_("Zhuang"),                                            "za", "zha", "zha" )
    LANG_CODE  ( N_("Zulu"),                                              "zu", "zul", "zul" )

    /* Custom VLC additions */
    LANG_CODE  ( N_("On Screen Display"),                                 "od", "osd", "osd" )
    LANG_CODE3 ( N_("Original audio"),                                          "qaa", "qaa" )
    LANG_CODE  ( N_("Hebrew"),                                            "iw", "heb", "heb" ) /* for old DVDs */

    /* End marker */
    LANG_CODE  ( NULL, "", "", "" )
};
#undef LANG_CODE
#undef LANG_CODE2
#undef LANG_CODE3
