/*****************************************************************************
 * dvd_summary.c: set of functions to print options of selected title
 * found in .ifo.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_summary.c,v 1.8 2001/08/06 13:28:00 sam Exp $
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

#define MODULE_NAME dvd
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if !defined( WIN32 )
#include <netinet/in.h>
#endif

#include <fcntl.h>
#include <sys/types.h>

#include <string.h>
#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif
#include <errno.h>

#ifdef GOD_DAMN_DMCA
#   include "dummy_dvdcss.h"
#else
#   include <videolan/dvdcss.h>
#endif

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "intf_msg.h"

#include "main.h"

#include "input_dvd.h"
#include "dvd_ifo.h"

#include "debug.h"

#include "modules.h"
#include "modules_export.h"

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

/*
 * Local tools to decode some data in ifo
 */

/*****************************************************************************
 * IfoLanguage: gives the long language name from the two-letters
 *              ISO-639 code
 *****************************************************************************/
char * IfoLanguage( u16 i_code )
{
    int     i = 0;

    while( memcmp( lang_tbl[i].p_code, &i_code, 2 ) &&
           lang_tbl[i].p_lang_long[0] )
    {
        i++;
    }

    return lang_tbl[i].p_lang_long;
}


/****************************************************************************
 * IfoPrintTitle
 ****************************************************************************/
void IfoPrintTitle( thread_dvd_data_t * p_dvd )
{
    intf_WarnMsg( 5, "dvd info: title: %d", p_dvd->i_title );
    intf_WarnMsg( 5, "    vobstart at: %d blocks", p_dvd->i_start );
    intf_WarnMsg( 5, "    stream size: %d blocks", p_dvd->i_size );
    intf_WarnMsg( 5, "    number of chapters: %d", p_dvd->i_chapter_nb );
    intf_WarnMsg( 5, "    number of angles: %d", p_dvd->i_angle_nb );
}

/****************************************************************************
 * IfoPrintVideo
 ****************************************************************************/
#define video p_dvd->p_ifo->vts.manager_inf.video_attr
void IfoPrintVideo( thread_dvd_data_t * p_dvd )
{
    char     psz_ratio[12];
    char     psz_perm_displ[4][23] =
             {
                "pan-scan & letterboxed",
                "pan-scan",
                "letterboxed",
                "not specified"
             };
    char     psz_source_res[4][28] =
             {
                "720x480 ntsc or 720x576 pal",
                "704x480 ntsc or 704x576 pal",
                "352x480 ntsc or 352x576 pal",
                "352x240 ntsc or 352x288 pal"
             };

 
    switch( video.i_ratio )
    {
    case 0:
        sprintf( psz_ratio, "4:3" );
        break;
    case 3:
        sprintf( psz_ratio, "16:9" );
        break;
    default:
        sprintf( psz_ratio, "undef" );
        break;
    }

    intf_WarnMsg( 5, "dvd info: video" );
    intf_WarnMsg( 5, "    compression: mpeg-%d", video.i_compression+1 );
    intf_WarnMsg( 5, "    tv system: %s Hz",
                     video.i_system ? "pal 625/50" : "ntsc 525/60" );
    intf_WarnMsg( 5, "    aspect ratio: %s", psz_ratio );
    intf_WarnMsg( 5, "    display mode: %s",
                     psz_perm_displ[video.i_perm_displ] );
    intf_WarnMsg( 5, "    line21-1: %s",
                     video.i_line21_1 ? "data present in GOP" : "" );
    intf_WarnMsg( 5, "    line21-2: %s",
                     video.i_line21_2 ? "data present in GOP" : "" );
    intf_WarnMsg( 5, "    source res: %s",
                     psz_source_res[video.i_source_res] );
    intf_WarnMsg( 5, "    letterboxed: %s",
                     video.i_letterboxed ? "yes" : "no" );
    intf_WarnMsg( 5, "    mode: %s",
                     video.i_mode ? "film (625/50 only)" : "camera");
}
#undef video

/****************************************************************************
 * IfoPrintAudio
 ****************************************************************************/
#define audio p_dvd->p_ifo->vts.manager_inf.p_audio_attr[i-1]
#define audio_status \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_title_id-1].title.pi_audio_status[i-1]
void IfoPrintAudio( thread_dvd_data_t * p_dvd, int i )
{

    if( audio_status.i_available )
    {
        char    ppsz_mode[7][9] =
                { "ac3", "unknown", "mpeg-1", "mpeg-2", "lpcm", "sdds", "dts" };
        char    ppsz_appl_mode[3][15] =
                { "not specified", "karaoke", "surround sound" };
        char    psz_caption[25];
        char    ppsz_quant[4][10] =
                { "16 bits", "20 bits", "24 bits", "drc" };
    
        intf_WarnMsg( 5, "dvd info: audio %d" , i );
        intf_WarnMsg( 5, "    language: %s", 
                         IfoLanguage( hton16( audio.i_lang_code ) ) );
        intf_WarnMsg( 5, "    mode: %s", ppsz_mode[audio.i_coding_mode & 0x7] );
        intf_WarnMsg( 5, "    channel(s): %d %s",
                         audio.i_num_channels + 1,
                         audio.i_multichannel_extension ? "ext." : "" );
        intf_WarnMsg( 5, "    sampling: %d Hz",
                         audio.i_sample_freq ? 96000 : 48000 );
        intf_WarnMsg( 5, "    appl_mode: %s",
                         ppsz_appl_mode[audio.i_appl_mode & 0x2] );
        switch( audio.i_caption )
        {
        case 1:
            sprintf( psz_caption, "normal caption" );
            break;
        case 3:
            sprintf( psz_caption, "directors comments" );
            break;
        default:
            sprintf( psz_caption, " " );
            break;
        }
        intf_WarnMsg( 5, "    caption: %s", psz_caption );
        intf_WarnMsg( 5, "    quantization: %s",
                         ppsz_quant[audio.i_quantization & 0x3] );
    
        intf_WarnMsg( 5, "    status: %x", audio_status.i_position );
    }


}
#undef audio_status
#undef audio

/****************************************************************************
 * IfoPrintSpu
 ****************************************************************************/
#define spu p_dvd->p_ifo->vts.manager_inf.p_spu_attr[i-1]
#define spu_status \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_title_id-1].title.pi_spu_status[i-1]

void IfoPrintSpu( thread_dvd_data_t * p_dvd, int i )
{
    if( spu_status.i_available )
    {
        intf_WarnMsg( 5, "dvd info: spu %d", i );
        intf_WarnMsg( 5, "    caption: %d", spu.i_caption );
        intf_WarnMsg( 5, "    language: %s",
                         IfoLanguage( hton16( spu.i_lang_code ) ) );
        intf_WarnMsg( 5, "    prefix: %x", spu.i_prefix );

        intf_WarnMsg( 5, "    status: 4:3 %x wide %x letter %x pan %x",
            spu_status.i_position_43,
            spu_status.i_position_wide,
            spu_status.i_position_letter,
            spu_status.i_position_pan );
    }

}
#undef spu_status
#undef spu
