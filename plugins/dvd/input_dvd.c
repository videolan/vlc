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
 * $Id: input_dvd.c,v 1.32 2001/03/15 00:37:04 stef Exp $
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
#define MODULE_NAME dvd-css
#else /* HAVE_CSS */
#define MODULE_NAME dvd-nocss
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
static int  DVDCheckCSS ( struct input_thread_s * );
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
 * DVDCheckCSS: check the stream
 *****************************************************************************/
static int DVDCheckCSS( input_thread_t * p_input )
{
    return CSSTest( p_input->i_handle );
}


/*****************************************************************************
 * DVDFindSector: find cell index in adress map from index in
 * information table program map and give corresponding sectors.
 *****************************************************************************/
static int DVDFindSector( thread_dvd_data_t * p_dvd )
{
    pgc_t *              p_pgc;
    int                  i_cell;
    int                  i_index;

    p_pgc = &p_dvd->ifo.vts.pgci_ti.p_srp[p_dvd->i_vts_title-1].pgc;

    i_index = p_dvd->i_prg_cell - 1;

    do {
        if( i_index++ > p_dvd->i_prg_cell )
        {
            return -1;
        }
        
        i_cell = p_dvd->i_cell + 1;

        while( ( ( p_pgc->p_cell_pos_inf[i_index].i_vob_id !=
                   p_dvd->ifo.vts.c_adt.p_cell_inf[i_cell].i_vob_id ) ||
                 ( p_pgc->p_cell_pos_inf[i_index].i_cell_id !=
                   p_dvd->ifo.vts.c_adt.p_cell_inf[i_cell].i_cell_id ) ) &&
                 ( i_cell < ( p_dvd->ifo.vts.c_adt.i_cell_nb ) ) )
        {
            i_cell++;
        }

    } while( i_cell == ( p_dvd->ifo.vts.c_adt.i_cell_nb ) );

    p_dvd->i_cell = i_cell;
    p_dvd->i_prg_cell = i_index;

    /* Find start and end sectors of new cell */
    p_dvd->i_sector = MAX(
            p_dvd->ifo.vts.c_adt.p_cell_inf[i_cell].i_ssector,
            p_pgc->p_cell_play_inf[i_index].i_entry_sector );
    p_dvd->i_end_sector = MIN(
            p_dvd->ifo.vts.c_adt.p_cell_inf[i_cell].i_esector,
            p_pgc->p_cell_play_inf[i_index].i_lsector );

intf_WarnMsg( 3, "cell: %d index: %d sector1: %x sector2: %x end1: %x end2: %x", i_cell, i_index, p_dvd->ifo.vts.c_adt.p_cell_inf[i_cell].i_ssector, p_pgc->p_cell_play_inf[i_index].i_entry_sector, p_dvd->ifo.vts.c_adt.p_cell_inf[i_cell].i_esector,p_pgc->p_cell_play_inf[i_index].i_lsector  );

    return 0;
}

/*****************************************************************************
 * DVDChapterSelect: find the cell corresponding to requested chapter
 * When called to find chapter 1, also sets title size and end.
 *****************************************************************************/
static int DVDChapterSelect( thread_dvd_data_t * p_dvd, int i_chapter )
{
    pgc_t *              p_pgc;

    p_pgc = &p_dvd->ifo.vts.pgci_ti.p_srp[p_dvd->i_vts_title-1].pgc;

    /* Find cell index in Program chain for current chapter */
    p_dvd->i_prg_cell = p_pgc->prg_map.pi_entry_cell[i_chapter-1] - 1;
    p_dvd->i_cell = -1;

    /* Search for cell_index in cell adress_table */
    DVDFindSector( p_dvd );

    /* initialize  navigation parameters */
    p_dvd->i_sector = p_dvd->ifo.vts.c_adt.p_cell_inf[p_dvd->i_cell].i_ssector;
    p_dvd->i_end_sector =
                      p_dvd->ifo.vts.c_adt.p_cell_inf[p_dvd->i_cell].i_esector;

    /* start is : beginning of vts vobs + offset to vob x */
    p_dvd->i_start = p_dvd->i_title_start +
                     DVD_LB_SIZE * (off_t)( p_dvd->i_sector );

    /* Position the fd pointer on the right address */
    p_dvd->i_start = lseek( p_dvd->i_fd, p_dvd->i_start, SEEK_SET );

    p_dvd->i_chapter = i_chapter;

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
    int                  i_nb;
    u16                  i_id;
    u8                   i_ac3;
    u8                   i_mpeg;
    u8                   i_sub_pic;
    u8                   i;
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

        /* title number: it is not vts nb! */
        p_dvd->i_title = p_area->i_id;

        /* title position inside the selected vts */
        p_dvd->i_vts_title =
                    p_dvd->ifo.vmg.ptt_srpt.p_tts[p_dvd->i_title-1].i_vts_ttn;

        /* vts number */
        p_dvd->ifo.i_title = p_dvd->i_title;

        /* ifo vts */
        IfoReadVTS( &(p_dvd->ifo) );
        intf_WarnMsg( 2, "ifo info: vts initialized" );
    
        /* css title key for current vts */
        if( p_dvd->b_encrypted )
        {
            p_dvd->p_css->i_title =
                    p_dvd->ifo.vmg.ptt_srpt.p_tts[p_dvd->i_title-1].i_tts_nb;
            p_dvd->p_css->i_title_pos =
                    p_dvd->ifo.vts.i_pos +
                    p_dvd->ifo.vts.mat.i_tt_vobs_ssector * DVD_LB_SIZE;
            CSSGetKey( p_dvd->p_css );
            intf_WarnMsg( 2, "css info: vts key initialized" );
        }
    
        /*
         * Set selected title start and size
         */
        
        /* title set offset */
        p_dvd->i_title_start = p_dvd->ifo.vts.i_pos + DVD_LB_SIZE *
                      (off_t)( p_dvd->ifo.vts.mat.i_tt_vobs_ssector );

        /* last video cell */
        p_dvd->i_cell = -1;
        p_dvd->i_prg_cell = -1 +
            p_dvd->ifo.vts.pgci_ti.p_srp[p_dvd->i_vts_title-1].pgc.i_cell_nb;
        DVDFindSector( p_dvd );

        /* temporary hack to fix size in some dvds */
        if( p_dvd->i_cell >= p_dvd->ifo.vts.c_adt.i_cell_nb )
        {
            p_dvd->i_cell = p_dvd->ifo.vts.c_adt.i_cell_nb - 1;
        }

        p_dvd->i_size = DVD_LB_SIZE *
          (off_t)( p_dvd->ifo.vts.c_adt.p_cell_inf[p_dvd->i_cell].i_esector );

        DVDChapterSelect( p_dvd, 1 );

        p_dvd->i_size -= (off_t)( p_dvd->i_sector + 1 ) *DVD_LB_SIZE;

        lseek( p_input->i_handle, p_dvd->i_start, SEEK_SET );


        intf_WarnMsg( 2, "dvd info: title: %d", p_dvd->i_title );
        intf_WarnMsg( 2, "dvd info: vobstart at: %lld", p_dvd->i_start );
        intf_WarnMsg( 2, "dvd info: stream size: %lld", p_dvd->i_size );
        intf_WarnMsg( 2, "dvd info: number of chapters: %d",
                   p_dvd->ifo.vmg.ptt_srpt.p_tts[p_dvd->i_title-1].i_ptt_nb );

/*        intf_WarnMsg( 3, "last: %d index: %d", i_last_chapter, i_index );*/


/*        intf_WarnMsg( 3, "DVD: Cell: %d vob id: %d cell id: %d", i_end_cell, p_dvd->ifo.vts.c_adt.p_cell_inf[i_end_cell].i_vob_id, p_dvd->ifo.vts.c_adt.p_cell_inf[i_end_cell].i_cell_id );*/

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

            free( p_input->stream.pp_selected_es );
            p_input->stream.pp_selected_es = NULL;
            p_input->stream.i_selected_es_number = 0;
        }

        input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );

        p_es = NULL;

        /* ES 0 -> video MPEG2 */
        p_es = input_AddES( p_input, p_input->stream.pp_programs[0], 0xe0, 0 );
        p_es->i_stream_id = 0xe0;
        p_es->i_type = MPEG2_VIDEO_ES;
        input_SelectES( p_input, p_es );
        intf_WarnMsg( 1, "dvd info: video MPEG2 stream" );

    
        /* Audio ES, in the order they appear in .ifo */
        i_nb = p_dvd->ifo.vts.mat.i_audio_nb;
    
        i_ac3 = 0x7f;
        i_mpeg = 0xc0;

        for( i = 1 ; i <= i_nb ; i++ )
        {

#if 0
    fprintf( stderr, "Audio %d: %x %x %x %x %x %x %x %x %x %x %x %x\n", i,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_num_channels,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_coding_mode,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_multichannel_extension,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_type,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_appl_mode,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_foo,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_bar,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_appl_mode,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_quantization,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_sample_freq,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_lang_code,
            p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_caption );
#endif

            switch( p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_coding_mode )
            {
            case 0x00:              /* AC3 */
                i_id = ( ( i_ac3 + i ) << 8 ) | 0xbd;
                p_es = input_AddES( p_input,
                                    p_input->stream.pp_programs[0], i_id, 0 );
                p_es->i_stream_id = 0xbd;
                p_es->i_type = AC3_AUDIO_ES;
                p_es->b_audio = 1;
                strcpy( p_es->psz_desc, Language( hton16(
                    p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_lang_code ) ) ); 

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
                strcpy( p_es->psz_desc, Language( hton16(
                    p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_lang_code ) ) ); 

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
                         p_dvd->ifo.vts.mat.p_audio_atrt[i-1].i_coding_mode );
            }
        
        }
    
        /* Sub Picture ES */
        i_nb = p_dvd->ifo.vts.mat.i_subpic_nb;
    
        b_last = 0;
        i_sub_pic = 0x20;
        for( i = 1 ; i <= i_nb ; i++ )
        {
            if( !b_last )
            {
                i_id = ( i_sub_pic++ << 8 ) | 0xbd;
                p_es = input_AddES( p_input,
                                    p_input->stream.pp_programs[0], i_id, 0 );
                p_es->i_stream_id = 0xbd;
                p_es->i_type = DVD_SPU_ES;
                p_es->b_spu = 1;
                strcpy( p_es->psz_desc, Language( hton16(
                    p_dvd->ifo.vts.mat.p_subpic_atrt[i-1].i_lang_code ) ) ); 
                intf_WarnMsg( 1, "dvd info: spu stream %d %s\t(0x%x)",
                              i, p_es->psz_desc, i_id );
    
                /* The before the last spu has a 0x0 prefix */
                b_last =
                    ( p_dvd->ifo.vts.mat.p_subpic_atrt[i].i_prefix == 0 ); 
            }
        }

    } // i_title >= 0
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
        DVDChapterSelect( p_dvd, p_area->i_part );

        p_input->stream.p_selected_area->i_tell = p_dvd->i_start -
                                                  p_area->i_start;
        p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;

        intf_WarnMsg( 2, "dvd info: chapter %d start at: %lld",
                                    p_area->i_part, p_area->i_tell );
    }

    /* By default first audio stream and no spu */
    input_SelectES( p_input, p_input->stream.pp_es[1] );

    /* No PSM to read in DVD mode, we already have all information */
    p_input->stream.pp_programs[0]->b_is_ok = 1;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

/*****************************************************************************
 * DVDInit: initializes DVD structures
 *****************************************************************************/
static void DVDInit( input_thread_t * p_input )
{
    thread_dvd_data_t *  p_dvd;
    int                  i_title;
    int                  i_chapter;
    int                  i_audio;
    int                  i_spu;
    int                  i;

    if( (p_dvd = malloc( sizeof(thread_dvd_data_t) )) == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }

    p_input->p_plugin_data = (void *)p_dvd;
    p_input->p_method_data = NULL;

    p_dvd->i_fd = p_input->i_handle;

    p_dvd->i_block_once = 1;
    p_input->i_read_once = 8;

    p_dvd->b_encrypted = DVDCheckCSS( p_input );

    lseek( p_input->i_handle, 0, SEEK_SET );

    /* Reading structures initialisation */
    p_input->p_method_data =
//    DVDNetlistInit( 4096, 8192, 4096, DVD_LB_SIZE, p_dvd->i_block_once ); 
    DVDNetlistInit( 8192, 16384, 16384, DVD_LB_SIZE, p_dvd->i_block_once ); 
    intf_WarnMsg( 2, "dvd info: netlist initialized" );

    /* Ifo initialisation */
    p_dvd->ifo = IfoInit( p_input->i_handle );
    intf_WarnMsg( 2, "ifo info: vmg initialized" );

    /* CSS initialisation */
    if( p_dvd->b_encrypted )
    {
        p_dvd->p_css = CSSInit( p_input->i_handle );

        if( p_dvd->p_css == NULL )
        {
            intf_ErrMsg( "css error: fatal failure" );
            p_input->b_error = 1;
            return;
        }

        intf_WarnMsg( 2, "css info: initialized" );
    }

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

#define srpt p_dvd->ifo.vmg.ptt_srpt
    intf_WarnMsg( 2, "dvd info: number of titles: %d", srpt.i_ttu_nb );

#define area p_input->stream.pp_areas
    /* We start from 1 here since area 0 is reserved for video_ts.vob */
    for( i = 1 ; i <= srpt.i_ttu_nb ; i++ )
    {
        input_AddArea( p_input );

        /* Should not be so simple eventually :
         * are title Program Chains, or still something else ? */
        area[i]->i_id = i;

        /* Absolute start offset and size 
         * We can only set that with vts ifo, so we do it during the
         * first call to DVDSetArea */
        area[i]->i_start = 0;
        area[i]->i_size = 0;

        /* Number of chapter */
        area[i]->i_part_nb = srpt.p_tts[i-1].i_ptt_nb;
        area[i]->i_part = 1;

        /* Offset to vts_i_0.ifo */
        area[i]->i_plugin_data = p_dvd->ifo.i_off +
                               ( srpt.p_tts[i-1].i_ssector * DVD_LB_SIZE );
    }   
#undef area

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Get requested title - if none try to find one where is the movie */
    i_title = main_GetIntVariable( INPUT_TITLE_VAR, 1 );
    if( i_title <= 0 || i_title > srpt.i_ttu_nb )
    {
        i_title = 1;
    }
#undef srpt
    /* Get requested chapter - if none defaults to first one */
    i_chapter = main_GetIntVariable( INPUT_CHAPTER_VAR, 1 );
    if( i_chapter <= 0 )
    {
        i_chapter = 1;
    }

    p_input->stream.pp_areas[i_title]->i_part = i_chapter;
    DVDSetArea( p_input, p_input->stream.pp_areas[i_title] );

    /* For audio: first one if none or a not existing one specified */
    i_audio = main_GetIntVariable( INPUT_CHANNEL_VAR, 1 );
    if( i_audio <= 0 )
    {
        main_PutIntVariable( INPUT_CHANNEL_VAR, 1 );
        i_audio = 1;
    }

    input_ChangeES( p_input, p_input->stream.pp_es[i_audio], 1 );

    i_spu = main_GetIntVariable( INPUT_SUBTITLE_VAR, 0 );
    if( i_spu < 0 )
    {
        main_PutIntVariable( INPUT_CHANNEL_VAR, 1 );
        i_spu = 0;
    }

//    input_ChangeES( p_input, p_input->stream.pp_es[i_spu], 1 );


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

    CSSEnd( p_dvd->p_css );
//    IfoEnd( p_dvd->p_ifo ) );
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
    pgc_t *                 p_pgc;
    struct iovec *          p_vec;
    struct data_packet_s *  pp_data[p_input->i_read_once];
    u8 *                    pi_cur;
    int                     i_packet_size;
    int                     i_iovec;
    int                     i_packet;
    int                     i_pos;
    int                     i_read_bytes;
    int                     i_read_blocks;
    int                     i_chapter;
    off_t                   i_off;
    boolean_t               b_eof;

    p_dvd = (thread_dvd_data_t *)p_input->p_plugin_data;
    p_netlist = (dvd_netlist_t *)p_input->p_method_data;
    p_pgc = &p_dvd->ifo.vts.pgci_ti.p_srp[p_dvd->i_vts_title-1].pgc;

    /* Get an iovec pointer */
    if( ( p_vec = DVDGetiovec( p_netlist ) ) == NULL )
    {
        intf_ErrMsg( "DVD: read error" );
        return -1;
    }

    /* Get the position of the next cell if we're at cell end */
#if 1
    if( p_dvd->i_sector > p_dvd->i_end_sector )
    {
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
                       (off_t)( p_dvd->i_sector ) *DVD_LB_SIZE,
               SEEK_SET );

        i_chapter = 0;

        /* update chapter */
        while( p_pgc->prg_map.pi_entry_cell[i_chapter-1] - 1 <
               p_dvd->i_prg_cell )
        {
            i_chapter++;
        }

        p_dvd->i_chapter = i_chapter;

//        vlc_mutex_lock( &p_input->stream.stream_lock );

        p_input->stream.p_selected_area->i_tell = i_off -
                                    p_input->stream.p_selected_area->i_start;
        p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;

//        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

#endif
    /* Reads from DVD */
    i_read_bytes = readv( p_dvd->i_fd, p_vec, p_dvd->i_block_once );
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

            i_packet++;
            i_pos += i_packet_size + 6;
        }
    }

    pp_packets[i_packet] = NULL;

//    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.p_selected_area->i_tell +=
                                        p_dvd->i_block_once *DVD_LB_SIZE;
    b_eof = p_input->stream.p_selected_area->i_tell < p_dvd->i_size ? 0 : 1;

//    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( ( i_read_blocks == p_dvd->i_block_once ) && ( !b_eof ) )
    {
        return 0;
    }
    else
    {
        return 1;
    }
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
    pgc_t *                 p_pgc;
    off_t                   i_pos;
    int                     i_prg_cell;
    int                     i_cell;
    int                     i_chapter;
    
    p_dvd = ( thread_dvd_data_t * )p_input->p_plugin_data;
    p_pgc = &p_dvd->ifo.vts.pgci_ti.p_srp[p_dvd->i_vts_title-1].pgc;

    /* update navigation data */
    p_dvd->i_sector = i_off >> 11;

    i_prg_cell = 0;
    i_cell = 0;
    i_chapter = 1;

    /* parse vobu address map to find program cell */
    while( p_pgc->p_cell_play_inf[i_prg_cell].i_lsector < p_dvd->i_sector  )
    {
        i_prg_cell++;
    }
    /* parse cell address map to find cell */
    while( p_dvd->ifo.vts.c_adt.p_cell_inf[i_cell].i_esector <=
           p_dvd->i_sector + 1 )
    {
        i_cell++;
    }

    p_dvd->i_prg_cell = i_prg_cell;
    p_dvd->i_cell = i_cell - 1;             /* DVDFindSector will add one */

    /* check coherence of data */
    DVDFindSector( p_dvd );

    /* update chapter */
    while( p_pgc->prg_map.pi_entry_cell[i_chapter-1] - 1 < p_dvd->i_prg_cell )
    {
        i_chapter++;
    }

    p_dvd->i_chapter = i_chapter;

    /* we have to take care of offset of beginning of title */
    i_pos = i_off + p_input->stream.p_selected_area->i_start;

    /* with DVD, we have to be on a sector boundary */
    i_pos = i_pos & (~0x7ff);

    i_pos = lseek( p_dvd->i_fd, i_pos, SEEK_SET );

    p_input->stream.p_selected_area->i_tell = i_pos -
                                    p_input->stream.p_selected_area->i_start;
    p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;

    return;
}
