/*****************************************************************************
 * input_dvd.c: DVD raw reading plugin.
 * ---
 * This plugins should handle all the known specificities of the DVD format,
 * especially the 2048 bytes logical block size.
 * It depends on:
 *  -input_netlist used to read packets
 *  -dvd_ifo for ifo parsing and analyse
 *  -dvd_css for unscrambling
 *  -dvd_udf to find files
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input_dvd.c,v 1.46 2001/04/15 04:19:57 sam Exp $
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
#include "defs.h"

#ifdef HAVE_CSS
#define MODULE_NAME dvd
#else /* HAVE_CSS */
#define MODULE_NAME dvdnocss
#endif /* HAVE_CSS */
#include "modules_inner.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <string.h>
#include <errno.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "intf_msg.h"

#include "main.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"

#include "dvd_netlist.h"
#include "dvd_ifo.h"
#include "dvd_css.h"
#include "input_dvd.h"
#include "mpeg_system.h"

#include "debug.h"

#include "modules.h"

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
 * Local prototypes
 *****************************************************************************/
static int  DVDProbe    ( probedata_t *p_data );
static int  DVDRead     ( struct input_thread_s *, data_packet_t ** );
static void DVDInit     ( struct input_thread_s * );
static void DVDEnd      ( struct input_thread_s * );
static void DVDSeek     ( struct input_thread_s *, off_t );
static int  DVDSetArea  ( struct input_thread_s *, struct input_area_s * );
static int  DVDRewind   ( struct input_thread_s * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list )
{
#define input p_function_list->functions.input
    p_function_list->pf_probe = DVDProbe;
    input.pf_init             = DVDInit;
    input.pf_open             = input_FileOpen;
    input.pf_close            = input_FileClose;
    input.pf_end              = DVDEnd;
    input.pf_read             = DVDRead;
    input.pf_set_area         = DVDSetArea;
    input.pf_demux            = input_DemuxPS;
    input.pf_new_packet       = DVDNewPacket;
    input.pf_new_pes          = DVDNewPES;
    input.pf_delete_packet    = DVDDeletePacket;
    input.pf_delete_pes       = DVDDeletePES;
    input.pf_rewind           = DVDRewind;
    input.pf_seek             = DVDSeek;
#undef input
}

/*
 * Local tools to decode some data in ifo
 */

/*****************************************************************************
 * Language: gives the long language name from the two-letters ISO-639 code
 *****************************************************************************/
static char * Language( u16 i_code )
{
    int     i = 0;

    while( memcmp( lang_tbl[i].p_code, &i_code, 2 ) &&
           lang_tbl[i].p_lang_long[0] )
    {
        i++;
    }

    return lang_tbl[i].p_lang_long;
}

/*
 * Data reading functions
 */

/*****************************************************************************
 * DVDProbe: verifies that the stream is a PS stream
 *****************************************************************************/
static int DVDProbe( probedata_t *p_data )
{
    input_thread_t * p_input = (input_thread_t *)p_data;

    char * psz_name = p_input->p_source;
    int i_handle;
    int i_score = 5;

    if( TestMethod( INPUT_METHOD_VAR, "dvd" ) )
    {
#ifdef HAVE_CSS
        return( 999 );
#else /* HAVE_CSS */
        return( 998 );
#endif /* HAVE_CSS */
    }

    if( ( strlen(psz_name) > 4 ) && !strncasecmp( psz_name, "dvd:", 4 ) )
    {
        /* If the user specified "dvd:" then it's probably a DVD */
#ifdef HAVE_CSS
        i_score = 100;
#else /* HAVE_CSS */
        i_score = 90;
#endif /* HAVE_CSS */
        psz_name += 4;
    }

    i_handle = open( psz_name, 0 );
    if( i_handle == -1 )
    {
        return( 0 );
    }
    close( i_handle );

    return( i_score );
}

/*****************************************************************************
 * DVDFindCell: adjust the title cell index with the program cell
 *****************************************************************************/
static int DVDFindCell( thread_dvd_data_t * p_dvd )
{
    int                 i_cell;
    int                 i_index;

#define title \
        p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_program_chain-1].title
#define cell  p_dvd->p_ifo->vts.cell_inf

    i_cell = p_dvd->i_cell;
    i_index = p_dvd->i_prg_cell;

    while( ( ( title.p_cell_pos[i_index].i_vob_id !=
                   cell.p_cell_map[i_cell].i_vob_id ) ||
      ( title.p_cell_pos[i_index].i_cell_id !=
                   cell.p_cell_map[i_cell].i_cell_id ) ) &&
           ( i_cell < cell.i_cell_nb ) )
    {
        i_cell++;
    }
/*
intf_WarnMsg( 1, "FindCell: i_cell %d i_index %d found %d nb %d",
                    p_dvd->i_cell,
                    p_dvd->i_prg_cell,
                    i_cell,
                    cell.i_cell_nb );
*/
    if( i_cell == cell.i_cell_nb )
    {
        intf_ErrMsg( "dvd error: can't find cell" );
        return -1;
    }
    else
    {
        p_dvd->i_cell = i_cell;
        return 0;
    }
#undef title
#undef cell
}

/*****************************************************************************
 * DVDFindSector: find cell index in adress map from index in
 * information table program map and give corresponding sectors.
 *****************************************************************************/
static int DVDFindSector( thread_dvd_data_t * p_dvd )
{
#define title \
        p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_program_chain-1].title

    if( p_dvd->i_sector > title.p_cell_play[p_dvd->i_prg_cell].i_end_sector )
    {
        p_dvd->i_prg_cell++;
    }

    if( DVDFindCell( p_dvd ) < 0 )
    {
        intf_ErrMsg( "dvd error: can't find sector" );
        return -1;
    }

    /* Find start and end sectors of new cell */
    p_dvd->i_sector = MAX(
         p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_start_sector,
         title.p_cell_play[p_dvd->i_prg_cell].i_start_sector );
    p_dvd->i_end_sector = MIN(
         p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_end_sector,
         title.p_cell_play[p_dvd->i_prg_cell].i_end_sector );

/*    intf_WarnMsg( 1, "cell: %d sector1: 0x%x end1: 0x%x\n"
                     "index: %d sector2: 0x%x end2: 0x%x", 
        p_dvd->i_cell,
        p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_start_sector,
        p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_end_sector,
        p_dvd->i_prg_cell,
        title.p_cell_play[p_dvd->i_prg_cell].i_start_sector,
        title.p_cell_play[p_dvd->i_prg_cell].i_end_sector );
*/
#undef title

    return 0;
}

/*****************************************************************************
 * DVDChapterSelect: find the cell corresponding to requested chapter
 * When called to find chapter 1, also sets title size and end.
 *****************************************************************************/
static int DVDChapterSelect( thread_dvd_data_t * p_dvd, int i_chapter )
{
#define title \
        p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_program_chain-1].title

    /* Find cell index in Program chain for current chapter */
    p_dvd->i_prg_cell = title.chapter_map.pi_start_cell[i_chapter-1] - 1;
    p_dvd->i_cell = 0;
    p_dvd->i_sector = 0;

    /* Search for cell_index in cell adress_table and initialize start sector */
    if( DVDFindSector( p_dvd ) < 0 )
    {
        intf_ErrMsg( "dvd error: can't select chapter" );
        return -1;
    }

    /* start is : beginning of vts vobs + offset to vob x */
    p_dvd->i_start = p_dvd->i_title_start +
                     DVD_LB_SIZE * (off_t)( p_dvd->i_sector );

    /* Position the fd pointer on the right address */
    p_dvd->i_start = lseek( p_dvd->i_fd, p_dvd->i_start, SEEK_SET );

    p_dvd->i_chapter = i_chapter;
#undef title
    return 0;
}

/*****************************************************************************
 * DVDSetArea: initialize input data for title x, chapter y.
 * It should be called for each user navigation request, and to change
 * audio or sub-picture streams.
 * ---
 * Take care that i_title starts from 0 (vmg) and i_chapter start from 1.
 * i_audio, i_spu start from 1 ; 0 means off.
 * A negative value for an argument means it does not change
 *****************************************************************************/
static int DVDSetArea( input_thread_t * p_input, input_area_t * p_area )
{
    thread_dvd_data_t *  p_dvd;
    es_descriptor_t *    p_es;
    int                  i_audio;
    int                  i_spu;
    u16                  i_id;
    u8                   i_ac3;
    u8                   i_mpeg;
    u8                   i_sub_pic;
    u8                   i;
    int                  j;
    boolean_t            b_last;

    p_dvd = (thread_dvd_data_t*)p_input->p_plugin_data;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( p_area != p_input->stream.p_selected_area )
    {

        /*
         *  We have to load all title information
         */
        /* Change the default area */
        p_input->stream.p_selected_area =
                    p_input->stream.pp_areas[p_area->i_id];

        /* title number: it is not vts nb!,
         * it is what appears in the interface list */
        p_dvd->i_title = p_area->i_id;
        p_dvd->p_ifo->i_title = p_dvd->i_title;

        /* ifo vts */
        if( IfoTitleSet( p_dvd->p_ifo ) < 0 )
        {
            intf_ErrMsg( "dvd error: fatal error in vts ifo" );
            free( p_dvd );
            p_input->b_error = 1;
            return -1;
        }

#define vmg p_dvd->p_ifo->vmg
#define vts p_dvd->p_ifo->vts
        /* title position inside the selected vts */
        p_dvd->i_vts_title =
                    vmg.title_inf.p_attr[p_dvd->i_title-1].i_title_num;
        p_dvd->i_program_chain =
          vts.title_inf.p_title_start[p_dvd->i_vts_title-1].i_program_chain_num;

/*        intf_WarnMsg( 1, "dvd: title %d vts_title %d pgc %d",
                        p_dvd->i_title,
                        p_dvd->i_vts_title,
                        p_dvd->i_program_chain );
*/
        /* css title key for current vts */
        if( p_dvd->b_encrypted )
        {
            /* this one is vts number */
            p_dvd->p_css->i_title =
                    vmg.title_inf.p_attr[p_dvd->i_title-1].i_title_set_num;
            p_dvd->p_css->i_title_pos =
                    vts.i_pos +
                    vts.manager_inf.i_title_vob_start_sector * DVD_LB_SIZE;

            j = CSSGetKey( p_input->i_handle, p_dvd->p_css );
            if( j < 0 )
            {
                intf_ErrMsg( "dvd error: fatal error in vts css key" );
                free( p_dvd );
                p_input->b_error = 1;
                return -1;
            }
            else if( j > 0 )
            {
                intf_ErrMsg( "dvd error: css decryption unavailable" );
                free( p_dvd );
                p_input->b_error = 1;
                return -1;
            }
        }
    
        /*
         * Set selected title start and size
         */
        
        /* title set offset */
        p_dvd->i_title_start = vts.i_pos + DVD_LB_SIZE *
                      (off_t)( vts.manager_inf.i_title_vob_start_sector );

        /* last video cell */
        p_dvd->i_cell = 0;
        p_dvd->i_prg_cell = -1 +
            vts.title_unit.p_title[p_dvd->i_program_chain-1].title.i_cell_nb;

        if( DVDFindCell( p_dvd ) < 0 )
        {
            intf_ErrMsg( "dvd error: can't find title end" );
            p_input->b_error = 1;
            return -1;
        }

        /* temporary hack to fix size in some dvds */
        if( p_dvd->i_cell >= vts.cell_inf.i_cell_nb )
        {
            p_dvd->i_cell = vts.cell_inf.i_cell_nb - 1;
        }

        p_dvd->i_sector = 0;
        p_dvd->i_size = DVD_LB_SIZE *
          (off_t)( vts.cell_inf.p_cell_map[p_dvd->i_cell].i_end_sector );

        if( DVDChapterSelect( p_dvd, 1 ) < 0 )
        {
            intf_ErrMsg( "dvd error: can't find first chapter" );
            p_input->b_error = 1;
            return -1;
        }

        p_dvd->i_size -= (off_t)( p_dvd->i_sector + 1 ) *DVD_LB_SIZE;

        intf_WarnMsg( 2, "dvd info: title: %d", p_dvd->i_title );
        intf_WarnMsg( 2, "dvd info: vobstart at: %lld", p_dvd->i_start );
        intf_WarnMsg( 2, "dvd info: stream size: %lld", p_dvd->i_size );
        intf_WarnMsg( 2, "dvd info: number of chapters: %d",
                   vmg.title_inf.p_attr[p_dvd->i_title-1].i_chapter_nb );

        /* Area definition */
        p_input->stream.p_selected_area->i_start = p_dvd->i_start;
        p_input->stream.p_selected_area->i_size = p_dvd->i_size;

        /*
         * Destroy obsolete ES by reinitializing program 0
         * and find all ES in title with ifo data
         */
        if( p_input->stream.pp_programs != NULL )
        {
            /* We don't use input_EndStream here since
             * we keep area structures */

            for( i = 0 ; i < p_input->stream.i_selected_es_number ; i++ )
            {
                input_UnselectES( p_input, p_input->stream.pp_selected_es[i] );
            }

            input_DelProgram( p_input, p_input->stream.pp_programs[0] );

            p_input->stream.pp_selected_es = NULL;
            p_input->stream.i_selected_es_number = 0;
        }

        input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );

        /* No PSM to read in DVD mode, we already have all information */
        p_input->stream.pp_programs[0]->b_is_ok = 1;
        p_input->stream.pp_programs[0]->i_synchro_state = SYNCHRO_START;

        p_es = NULL;

        /* ES 0 -> video MPEG2 */
        p_es = input_AddES( p_input, p_input->stream.pp_programs[0], 0xe0, 0 );
        p_es->i_stream_id = 0xe0;
        p_es->i_type = MPEG2_VIDEO_ES;
        p_es->i_cat = VIDEO_ES;
        intf_WarnMsg( 1, "dvd info: video mpeg2 stream" );
        if( p_main->b_video )
        {
            input_SelectES( p_input, p_es );
        }

        /* Audio ES, in the order they appear in .ifo */
            
        i_ac3 = 0x7f;
        i_mpeg = 0xc0;

        for( i = 1 ; i <= vts.manager_inf.i_audio_nb ; i++ )
        {

#ifdef DEBUG
        intf_WarnMsg( 1, "Audio %d: %x %x %x %x %x %x %x %x %x %x %x %x", i,
            vts.manager_inf.p_audio_attr[i-1].i_num_channels,
            vts.manager_inf.p_audio_attr[i-1].i_coding_mode,
            vts.manager_inf.p_audio_attr[i-1].i_multichannel_extension,
            vts.manager_inf.p_audio_attr[i-1].i_type,
            vts.manager_inf.p_audio_attr[i-1].i_appl_mode,
            vts.manager_inf.p_audio_attr[i-1].i_foo,
            vts.manager_inf.p_audio_attr[i-1].i_test,
            vts.manager_inf.p_audio_attr[i-1].i_bar,
            vts.manager_inf.p_audio_attr[i-1].i_quantization,
            vts.manager_inf.p_audio_attr[i-1].i_sample_freq,
            vts.manager_inf.p_audio_attr[i-1].i_lang_code,
            vts.manager_inf.p_audio_attr[i-1].i_caption );
#endif

            switch( vts.manager_inf.p_audio_attr[i-1].i_coding_mode )
            {
            case 0x00:              /* AC3 */
                i_id = ( ( i_ac3 + i ) << 8 ) | 0xbd;
                p_es = input_AddES( p_input,
                                    p_input->stream.pp_programs[0], i_id, 0 );
                p_es->i_stream_id = 0xbd;
                p_es->i_type = AC3_AUDIO_ES;
                p_es->b_audio = 1;
                p_es->i_cat = AUDIO_ES;
                strcpy( p_es->psz_desc, Language( hton16(
                    vts.manager_inf.p_audio_attr[i-1].i_lang_code ) ) ); 
                strcat( p_es->psz_desc, " (ac3)" );

                intf_WarnMsg( 1, "dvd info: audio stream %d %s\t(0x%x)",
                              i, p_es->psz_desc, i_id );

                break;
            case 0x02:
            case 0x03:              /* MPEG audio */
                i_id = 0xbf + i;
                p_es = input_AddES( p_input,
                                    p_input->stream.pp_programs[0], i_id, 0 );
                p_es->i_stream_id = i_id;
                p_es->i_type = MPEG2_AUDIO_ES;
                p_es->b_audio = 1;
                p_es->i_cat = AUDIO_ES;
                strcpy( p_es->psz_desc, Language( hton16(
                    vts.manager_inf.p_audio_attr[i-1].i_lang_code ) ) ); 
                strcat( p_es->psz_desc, " (mpeg)" );

                intf_WarnMsg( 1, "dvd info: audio stream %d %s\t(0x%x)",
                              i, p_es->psz_desc, i_id );

                break;
            case 0x04:              /* LPCM */
                i_id = 0;
                intf_ErrMsg( "dvd warning: LPCM audio not handled yet" );
                break;
            case 0x06:              /* DTS */
                i_id = 0;
                i_ac3--;
                intf_ErrMsg( "dvd warning: DTS audio not handled yet" );
                break;
            default:
                i_id = 0;
                intf_ErrMsg( "dvd warning: unknown audio type %.2x",
                         vts.manager_inf.p_audio_attr[i-1].i_coding_mode );
            }
        
        }
    
        /* Sub Picture ES */
           
        b_last = 0;
        i_sub_pic = 0x20;
        for( i = 1 ; i <= vts.manager_inf.i_spu_nb; i++ )
        {
            if( !b_last )
            {
                i_id = ( i_sub_pic++ << 8 ) | 0xbd;
                p_es = input_AddES( p_input,
                                    p_input->stream.pp_programs[0], i_id, 0 );
                p_es->i_stream_id = 0xbd;
                p_es->i_type = DVD_SPU_ES;
                p_es->i_cat = SPU_ES;
                strcpy( p_es->psz_desc, Language( hton16(
                    vts.manager_inf.p_spu_attr[i-1].i_lang_code ) ) ); 
                intf_WarnMsg( 1, "dvd info: spu stream %d %s\t(0x%x)",
                              i, p_es->psz_desc, i_id );
    
                /* The before the last spu has a 0x0 prefix */
                b_last =
                    ( vts.manager_inf.p_spu_attr[i].i_prefix == 0 ); 
            }
        }

        if( p_main->b_audio )
        {
            /* For audio: first one if none or a not existing one specified */
            i_audio = main_GetIntVariable( INPUT_CHANNEL_VAR, 1 );
            if( i_audio < 0 || i_audio > vts.manager_inf.i_audio_nb )
            {
                main_PutIntVariable( INPUT_CHANNEL_VAR, 1 );
                i_audio = 1;
            }
            if( i_audio > 0 && vts.manager_inf.i_audio_nb > 0 )
            {
                input_SelectES( p_input, p_input->stream.pp_es[i_audio] );
            }
        }

        if( p_main->b_video )
        {
            /* for spu, default is none */
            i_spu = main_GetIntVariable( INPUT_SUBTITLE_VAR, 0 );
            if( i_spu < 0 || i_spu > vts.manager_inf.i_spu_nb )
            {
                main_PutIntVariable( INPUT_SUBTITLE_VAR, 0 );
                i_spu = 0;
            }
            if( i_spu > 0 && vts.manager_inf.i_spu_nb > 0 )
            {
                i_spu += vts.manager_inf.i_audio_nb;
                input_SelectES( p_input, p_input->stream.pp_es[i_spu] );
            }
        }
    } /* i_title >= 0 */
    else
    {
        p_area = p_input->stream.p_selected_area;
    }

    /*
     * Chapter selection
     */

    if( ( p_area->i_part > 0 ) &&
        ( p_area->i_part <= p_area->i_part_nb ) )
    {
        if( DVDChapterSelect( p_dvd, p_area->i_part ) < 0 )
        {
            intf_ErrMsg( "dvd error: can't set chapter in area" );
            p_input->b_error = 1;
            return -1;
        }

        p_input->stream.p_selected_area->i_tell = p_dvd->i_start -
                                                  p_area->i_start;
        p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;

        intf_WarnMsg( 2, "dvd info: chapter %d start at: %lld",
                                    p_area->i_part, p_area->i_tell );
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );
#undef vts
#undef vmg

    return 0;
}

/*****************************************************************************
 * DVDInit: initializes DVD structures
 *****************************************************************************/
static void DVDInit( input_thread_t * p_input )
{
    thread_dvd_data_t *  p_dvd;
    input_area_t *       p_area;
    int                  i_title;
    int                  i_chapter;
    int                  i;

    /* I don't want DVDs to start playing immediately */
//    p_input->stream.i_new_status = PAUSE_S;

    p_dvd = malloc( sizeof(thread_dvd_data_t) );
    if( p_dvd == NULL )
    {
        intf_ErrMsg( "dvd error: out of memory" );
        p_input->b_error = 1;
        return;
    }

    p_input->p_plugin_data = (void *)p_dvd;
    p_input->p_method_data = NULL;

    p_dvd->i_fd = p_input->i_handle;

    /* reading several block once seems to cause lock-up
     * when using input_ToggleES
     * who wrote thez damn buggy piece of shit ??? --stef */
    p_dvd->i_block_once = 1;//32;
    p_input->i_read_once = 8;//128;

    i = CSSTest( p_input->i_handle );

    if( i < 0 )
    {
        free( p_dvd );
        p_input->b_error = 1;
        return;
    }

    p_dvd->b_encrypted = i;

    lseek( p_input->i_handle, 0, SEEK_SET );

    /* Reading structures initialisation */
    p_input->p_method_data =
        DVDNetlistInit( 2048, 8192, 2048, DVD_LB_SIZE, p_dvd->i_block_once );
    intf_WarnMsg( 2, "dvd info: netlist initialized" );

    /* Ifo allocation & initialisation */
    if( IfoCreate( p_dvd ) < 0 )
    {
        intf_ErrMsg( "dvd error: allcation error in ifo" );
        p_input->b_error = 1;
        return;
    }

    if( IfoInit( p_dvd->p_ifo ) < 0 )
    {
        intf_ErrMsg( "dvd error: fatal failure in ifo" );
        free( p_dvd );
        p_input->b_error = 1;
        return;
    }

    /* CSS initialisation */
    if( p_dvd->b_encrypted )
    {
        p_dvd->p_css = malloc( sizeof(css_t) );
        if( p_dvd->p_css == NULL )
        {
            intf_ErrMsg( "dvd error: couldn't create css structure" );
            free( p_dvd );
            p_input->b_error = 1;
            return;
        }

        p_dvd->p_css->i_agid = 0;

        if( CSSInit( p_input->i_handle, p_dvd->p_css ) < 0 )
        {
            intf_ErrMsg( "dvd error: fatal failure in css" );
            free( p_dvd->p_css );
            free( p_dvd );
            p_input->b_error = 1;
            return;
        }

        intf_WarnMsg( 2, "dvd info: css initialized" );
    }

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

    /* disc input method */
    p_input->stream.i_method = INPUT_METHOD_DVD;

#define title_inf p_dvd->p_ifo->vmg.title_inf
    intf_WarnMsg( 2, "dvd info: number of titles: %d", title_inf.i_title_nb );

#define area p_input->stream.pp_areas
    /* We start from 1 here since the default area 0
     * is reserved for video_ts.vob */
    for( i = 1 ; i <= title_inf.i_title_nb ; i++ )
    {
        input_AddArea( p_input );

        /* Titles are Program Chains */
        area[i]->i_id = i;

        /* Absolute start offset and size 
         * We can only set that with vts ifo, so we do it during the
         * first call to DVDSetArea */
        area[i]->i_start = 0;
        area[i]->i_size = 0;

        /* Number of chapters */
        area[i]->i_part_nb = title_inf.p_attr[i-1].i_chapter_nb;
        area[i]->i_part = 1;

        /* Offset to vts_i_0.ifo */
        area[i]->i_plugin_data = p_dvd->p_ifo->i_off +
                       ( title_inf.p_attr[i-1].i_start_sector * DVD_LB_SIZE );
    }   
#undef area

    /* Get requested title - if none try the first title */
    i_title = main_GetIntVariable( INPUT_TITLE_VAR, 1 );
    if( i_title <= 0 || i_title > title_inf.i_title_nb )
    {
        i_title = 1;
    }
#undef title_inf
    /* Get requested chapter - if none defaults to first one */
    i_chapter = main_GetIntVariable( INPUT_CHAPTER_VAR, 1 );
    if( i_chapter <= 0 )
    {
        i_chapter = 1;
    }

    p_input->stream.pp_areas[i_title]->i_part = i_chapter;

    p_area = p_input->stream.pp_areas[i_title];

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* set title, chapter, audio and subpic */
    DVDSetArea( p_input, p_area );

    return;
}

/*****************************************************************************
 * DVDEnd: frees unused data
 *****************************************************************************/
static void DVDEnd( input_thread_t * p_input )
{
    thread_dvd_data_t *     p_dvd;
    dvd_netlist_t *         p_netlist;

    p_dvd = (thread_dvd_data_t*)p_input->p_plugin_data;
    p_netlist = (dvd_netlist_t *)p_input->p_method_data;

    if( p_dvd->b_encrypted )
    {
        free( p_dvd->p_css );
    }

    IfoDestroy( p_dvd->p_ifo );
    free( p_dvd );
    DVDNetlistEnd( p_netlist );
}

/*****************************************************************************
 * DVDRead: reads data packets into the netlist.
 *****************************************************************************
 * Returns -1 in case of error, 0 if everything went well, and 1 in case of
 * EOF.
 *****************************************************************************/
static int DVDRead( input_thread_t * p_input,
                    data_packet_t ** pp_packets )
{
    thread_dvd_data_t *     p_dvd;
    dvd_netlist_t *         p_netlist;
    struct iovec *          p_vec;
    struct data_packet_s *  pp_data[p_input->i_read_once];
    u8 *                    pi_cur;
    int                     i_block_once;
    int                     i_packet_size;
    int                     i_iovec;
    int                     i_packet;
    int                     i_pos;
    int                     i_read_bytes;
    int                     i_read_blocks;
    off_t                   i_off;
    boolean_t               b_eof;

    p_dvd = (thread_dvd_data_t *)p_input->p_plugin_data;
    p_netlist = (dvd_netlist_t *)p_input->p_method_data;
#define title \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_program_chain-1].title

    /* Get an iovec pointer */
    if( ( p_vec = DVDGetiovec( p_netlist ) ) == NULL )
    {
        intf_ErrMsg( "dvd error: can't get iovec" );
        return -1;
    }

    i_block_once = p_dvd->i_end_sector - p_dvd->i_sector + 1;

    /* Get the position of the next cell if we're at cell end */
    if( i_block_once <= 0 )
    {
        p_dvd->i_cell++;

        /* Find cell index in adress map */
        if( DVDFindSector( p_dvd ) < 0 )
        {
            pp_packets[0] = NULL;
            intf_ErrMsg( "dvd error: can't find next cell" );
            return 1;
        }

        /* Position the fd pointer on the right address */
        i_off = lseek( p_dvd->i_fd,
                       p_dvd->i_title_start +
                       (off_t)( p_dvd->i_sector ) *DVD_LB_SIZE, SEEK_SET );

        /* update chapter : it will be easier when we have navigation
         * ES support */
        if( title.chapter_map.pi_start_cell[p_dvd->i_chapter-1] <=
            p_dvd->i_prg_cell )
        {
            p_dvd->i_chapter++;
        }

        vlc_mutex_lock( &p_input->stream.stream_lock );

        p_input->stream.p_selected_area->i_tell = i_off -
                                    p_input->stream.p_selected_area->i_start;
        p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;

        /* the synchro has to be reinitialized when we change cell */
        p_input->stream.pp_programs[0]->i_synchro_state = SYNCHRO_START;

        vlc_mutex_unlock( &p_input->stream.stream_lock );

        i_block_once = p_dvd->i_end_sector - p_dvd->i_sector + 1;
    }

    /* the number of blocks read is the maw between the requested
     * value and the leaving block in the cell */
    if( i_block_once > p_dvd->i_block_once )
    {
        i_block_once = p_dvd->i_block_once;
    }
//intf_WarnMsg( 2, "Sector: 0x%x Read: %d Chapter: %d", p_dvd->i_sector, i_block_once, p_dvd->i_chapter );

    p_netlist->i_read_once = i_block_once;

    /* Reads from DVD */
    i_read_bytes = readv( p_dvd->i_fd, p_vec, i_block_once );
    i_read_blocks = ( i_read_bytes + 0x7ff ) >> 11;

    /* Update netlist indexes */
    DVDMviovec( p_netlist, i_read_blocks, pp_data );

    /* Update global position */
    p_dvd->i_sector += i_read_blocks;

    i_packet = 0;

    /* Read headers to compute payload length */
    for( i_iovec = 0 ; i_iovec < i_read_blocks ; i_iovec++ )
    {
        if( p_dvd->b_encrypted )
        {
            CSSDescrambleSector( p_dvd->p_css->pi_title_key, 
                                 p_vec[i_iovec].iov_base );
            ((u8*)(p_vec[i_iovec].iov_base))[0x14] &= 0x8F;
        }

        i_pos = 0;

        while( i_pos < p_netlist->i_buffer_size )
        {
            pi_cur = (u8*)(p_vec[i_iovec].iov_base + i_pos);

            /*default header */
            if( U32_AT( pi_cur ) != 0x1BA )
            {
                /* That's the case for all packets, except pack header. */
                i_packet_size = U16_AT( pi_cur + 4 );
                pp_packets[i_packet] = DVDNewPtr( p_netlist );
            }
            else
            {
                /* Pack header. */
                if( ( pi_cur[4] & 0xC0 ) == 0x40 )
                {
                    /* MPEG-2 */
                    i_packet_size = 8;
                }
                else if( ( pi_cur[4] & 0xF0 ) == 0x20 )
                {
                    /* MPEG-1 */
                    i_packet_size = 6;
                }
                else
                {
                    intf_ErrMsg( "Unable to determine stream type" );
                    return( -1 );
                }

                pp_packets[i_packet] = pp_data[i_iovec];

            }

            (*pp_data[i_iovec]->pi_refcount)++;

            pp_packets[i_packet]->pi_refcount = pp_data[i_iovec]->pi_refcount;

            pp_packets[i_packet]->p_buffer = pp_data[i_iovec]->p_buffer;

            pp_packets[i_packet]->p_payload_start =
                    pp_packets[i_packet]->p_buffer + i_pos;

            pp_packets[i_packet]->p_payload_end =
                    pp_packets[i_packet]->p_payload_start + i_packet_size + 6;

            pp_packets[i_packet]->p_next = NULL;
            pp_packets[i_packet]->b_discard_payload = 0;

            i_packet++;
            i_pos += i_packet_size + 6;
        }
    }

    pp_packets[i_packet] = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.p_selected_area->i_tell += i_read_bytes;
    b_eof = !( p_input->stream.p_selected_area->i_tell < p_dvd->i_size );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( ( i_read_blocks == i_block_once ) && ( !b_eof ) )
    {
        return 0;
    }
    else
    {
        return 1;
    }
#undef title
}


/*****************************************************************************
 * DVDRewind : reads a stream backward
 *****************************************************************************/
static int DVDRewind( input_thread_t * p_input )
{
    return( -1 );
}

/*****************************************************************************
 * DVDSeek : Goes to a given position on the stream ; this one is used by the 
 * input and translate chronological position from input to logical postion
 * on the device
 * ---
 * The lock should be taken before calling this function.
 *****************************************************************************/
static void DVDSeek( input_thread_t * p_input, off_t i_off )
{
    thread_dvd_data_t *     p_dvd;
    off_t                   i_pos;
    int                     i_prg_cell;
    int                     i_cell;
    int                     i_chapter;
    
    p_dvd = ( thread_dvd_data_t * )p_input->p_plugin_data;
#define title \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_program_chain-1].title

    /* we have to take care of offset of beginning of title */
    i_pos = i_off + p_input->stream.p_selected_area->i_start
                  - p_dvd->i_title_start;

    /* update navigation data */
    p_dvd->i_sector = i_pos >> 11;

    i_prg_cell = 0;
    i_chapter = 1;

    /* parse vobu address map to find program cell */
    while( title.p_cell_play[i_prg_cell].i_end_sector < p_dvd->i_sector  )
    {
        i_prg_cell++;
    }

    p_dvd->i_prg_cell = i_prg_cell;
    p_dvd->i_cell = 0;

    /* Find first title cell which is inside program cell */
    if( DVDFindCell( p_dvd ) < 0 )
    {
        intf_ErrMsg( "dvd error: cell seeking failed" );
        p_input->b_error = 1;
        return;
    }

    i_cell = p_dvd->i_cell;

#define cell p_dvd->p_ifo->vts.cell_inf.p_cell_map[i_cell]
    /* parse cell address map to find title cell containing sector */
    while( cell.i_end_sector < p_dvd->i_sector )
    {
        i_cell++;
    }

    p_dvd->i_cell = i_cell;

    p_dvd->i_end_sector = MIN(
            cell.i_end_sector,
            title.p_cell_play[p_dvd->i_prg_cell].i_end_sector );

    /* update chapter */
    while( title.chapter_map.pi_start_cell[i_chapter-1] <= p_dvd->i_prg_cell )
    {
        i_chapter++;
    }

    p_dvd->i_chapter = i_chapter;
    p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;

    p_input->stream.p_selected_area->i_tell =
                lseek( p_dvd->i_fd, p_dvd->i_title_start +
                       (off_t)( p_dvd->i_sector ) *DVD_LB_SIZE, SEEK_SET ) -
                p_input->stream.p_selected_area->i_start;

    intf_WarnMsg( 1, "Program Cell: %d Cell: %d Chapter: %d",
                     i_prg_cell, i_cell, p_dvd->i_chapter );

    return;
}
