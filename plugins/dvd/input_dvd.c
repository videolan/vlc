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
 * $Id: input_dvd.c,v 1.20 2001/02/22 08:59:54 stef Exp $
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
#include <unistd.h>
#include <netinet/in.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <string.h>
#include <errno.h>
#include <malloc.h>

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
#include "input_netlist.h"

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
} lang_tbl[] =
    {

    /* The ISO 639 language codes.
     * Language names with * prefix are not spelled in their own language 
     */
    	{"  ", "Not Specified"},
    	{"aa", "*Afar"},
    	{"ab", "*Abkhazian"},
    	{"af", "*Afrikaans"},
    	{"am", "*Amharic"},
    	{"ar", "*Arabic"},
    	{"as", "*Assamese"},
    	{"ay", "*Aymara"},
    	{"az", "*Azerbaijani"},
    	{"ba", "*Bashkir"},
    	{"be", "*Byelorussian"},
    	{"bg", "*Bulgarian"},
    	{"bh", "*Bihari"},
    	{"bi", "*Bislama"},
    	{"bn", "*Bengali; Bangla"},
    	{"bo", "*Tibetan"},
    	{"br", "*Breton"},
    	{"ca", "*Catalan"},
    	{"co", "*Corsican"},
    	{"cs", "*Czech(Ceske)"},
    	{"cy", "*Welsh"},
    	{"da", "Dansk"},
    	{"de", "Deutsch"},
    	{"dz", "*Bhutani"},
    	{"el", "*Greek"},
    	{"en", "English"},
    	{"eo", "*Esperanto"},
    	{"es", "Espanol"},
    	{"et", "*Estonian"},
    	{"eu", "*Basque"},
    	{"fa", "*Persian"},
    	{"fi", "Suomi"},
    	{"fj", "*Fiji"},
    	{"fo", "*Faroese"},
    	{"fr", "Francais"},
    	{"fy", "*Frisian"},
    	{"ga", "*Irish"},
    	{"gd", "*Scots Gaelic"},
    	{"gl", "*Galician"},
    	{"gn", "*Guarani"},
    	{"gu", "*Gujarati"},
    	{"ha", "*Hausa"},
    	{"he", "*Hebrew"},				// formerly iw
    	{"hi", "*Hindi"},
    	{"hr", "Hrvatski"},				// Croatian
    	{"hu", "Magyar"},
    	{"hy", "*Armenian"},
    	{"ia", "*Interlingua"},
    	{"id", "*Indonesian"},				// formerly in
    	{"ie", "*Interlingue"},
    	{"ik", "*Inupiak"},
    	{"in", "*Indonesian"},				// replaced by id
    	{"is", "Islenska"},
    	{"it", "Italiano"},
    	{"iu", "*Inuktitut"},
    	{"iw", "*Hebrew"},				// replaced by he
    	{"ja", "*Japanese"},
    	{"ji", "*Yiddish"},				// replaced by yi
    	{"jw", "*Javanese"},
    	{"ka", "*Georgian"},
    	{"kk", "*Kazakh"},
    	{"kl", "*Greenlandic"},
    	{"km", "*Cambodian"},
    	{"kn", "*Kannada"},
    	{"ko", "*Korean"},
    	{"ks", "*Kashmiri"},
    	{"ku", "*Kurdish"},
    	{"ky", "*Kirghiz"},
    	{"la", "*Latin"},
    	{"ln", "*Lingala"},
    	{"lo", "*Laothian"},
    	{"lt", "*Lithuanian"},
    	{"lv", "*Latvian, Lettish"},
    	{"mg", "*Malagasy"},
    	{"mi", "*Maori"},
    	{"mk", "*Macedonian"},
    	{"ml", "*Malayalam"},
    	{"mn", "*Mongolian"},
    	{"mo", "*Moldavian"},
    	{"mr", "*Marathi"},
    	{"ms", "*Malay"},
    	{"mt", "*Maltese"},
    	{"my", "*Burmese"},
    	{"na", "*Nauru"},
    	{"ne", "*Nepali"},
    	{"nl", "Nederlands"},
    	{"no", "Norsk"},
    	{"oc", "*Occitan"},
    	{"om", "*(Afan) Oromo"},
    	{"or", "*Oriya"},
    	{"pa", "*Punjabi"},
    	{"pl", "*Polish"},
    	{"ps", "*Pashto, Pushto"},
    	{"pt", "Portugues"},
    	{"qu", "*Quechua"},
    	{"rm", "*Rhaeto-Romance"},
    	{"rn", "*Kirundi"},
    	{"ro", "*Romanian"},
    	{"ru", "*Russian"},
    	{"rw", "*Kinyarwanda"},
    	{"sa", "*Sanskrit"},
    	{"sd", "*Sindhi"},
    	{"sg", "*Sangho"},
    	{"sh", "*Serbo-Croatian"},
    	{"si", "*Sinhalese"},
    	{"sk", "*Slovak"},
    	{"sl", "*Slovenian"},
    	{"sm", "*Samoan"},
    	{"sn", "*Shona"},
    	{"so", "*Somali"},
    	{"sq", "*Albanian"},
    	{"sr", "*Serbian"},
    	{"ss", "*Siswati"},
    	{"st", "*Sesotho"},
    	{"su", "*Sundanese"},
    	{"sv", "Svenska"},
    	{"sw", "*Swahili"},
    	{"ta", "*Tamil"},
    	{"te", "*Telugu"},
    	{"tg", "*Tajik"},
    	{"th", "*Thai"},
    	{"ti", "*Tigrinya"},
    	{"tk", "*Turkmen"},
    	{"tl", "*Tagalog"},
    	{"tn", "*Setswana"},
    	{"to", "*Tonga"},
    	{"tr", "*Turkish"},
    	{"ts", "*Tsonga"},
    	{"tt", "*Tatar"},
    	{"tw", "*Twi"},
    	{"ug", "*Uighur"},
    	{"uk", "*Ukrainian"},
    	{"ur", "*Urdu"},
    	{"uz", "*Uzbek"},
    	{"vi", "*Vietnamese"},
    	{"vo", "*Volapuk"},
    	{"wo", "*Wolof"},
    	{"xh", "*Xhosa"},
    	{"yi", "*Yiddish"},				// formerly ji
    	{"yo", "*Yoruba"},
    	{"za", "*Zhuang"},
    	{"zh", "*Chinese"},
    	{"zu", "*Zulu"},
    	{"\0", ""}
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
static int  DVDSetArea  ( struct input_thread_s *, int, int, int, int );
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
    input.pf_new_packet       = input_NetlistNewPacket;
    input.pf_new_pes          = input_NetlistNewPES;
    input.pf_delete_packet    = input_NetlistDeletePacket;
    input.pf_delete_pes       = input_NetlistDeletePES;
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
        return( 999 );
    }

    if( ( strlen(psz_name) > 4 ) && !strncasecmp( psz_name, "dvd:", 4 ) )
    {
        /* If the user specified "dvd:" then it's probably a DVD */
        i_score = 100;
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
 * DVDChapterSelect: find the cell corresponding to requested chapter
 *****************************************************************************/
static int DVDChapterSelect( thread_dvd_data_t * p_dvd, int i_chapter )
{
    pgc_t *              p_pgc;
    int                  i_start_cell;
    int                  i_end_cell;
    int                  i_index;
    int                  i_cell;

    p_pgc = &p_dvd->ifo.vts.pgci_ti.p_srp[0].pgc;

    /* Find cell index in Program chain for current chapter */
    i_index = p_pgc->prg_map.pi_entry_cell[i_chapter-1] - 1;

    /* Search for cell_index in cell adress_table */
    i_cell = 0;
    while( p_pgc->p_cell_pos_inf[i_index].i_vob_id >
           p_dvd->ifo.vts.c_adt.p_cell_inf[i_cell].i_vob_id )
    {
        i_cell++;
    }
    while( p_pgc->p_cell_pos_inf[i_index].i_cell_id >
           p_dvd->ifo.vts.c_adt.p_cell_inf[i_cell].i_cell_id )
    {
        i_cell++;
    }

    i_start_cell = i_cell;

    p_dvd->i_start = p_dvd->ifo.vts.i_pos + DVD_LB_SIZE *
            (off_t)( p_dvd->ifo.vts.mat.i_tt_vobs_ssector +
                     p_dvd->ifo.vts.c_adt.p_cell_inf[i_start_cell].i_ssector );

    if( i_chapter == 1 )
    {
        i_end_cell = i_start_cell + p_pgc->i_cell_nb - 1;
        p_dvd->i_size = (off_t)DVD_LB_SIZE *
            ( p_dvd->ifo.vts.c_adt.p_cell_inf[i_end_cell].i_esector -
              p_dvd->ifo.vts.c_adt.p_cell_inf[i_start_cell].i_ssector + 1 );
        p_dvd->i_chapter_nb = p_pgc->i_cell_nb;
        intf_WarnMsg( 3, "DVD: Start cell: %d End Cell: %d",
                                            i_start_cell, i_end_cell );
    }

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
static int DVDSetArea( input_thread_t * p_input,
                       int i_title, int i_chapter,
                       int i_audio, int i_spu )
{
    thread_dvd_data_t *  p_method;
    es_descriptor_t *    p_es;
    int                  i_index;
    int                  i_nb;
    u16                  i_id;
    u8                   i_ac3;
    u8                   i_mpeg;
    u8                   i_sub_pic;
    u8                   i;
    boolean_t            b_last;
    
    p_method = (thread_dvd_data_t*)p_input->p_plugin_data;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( i_title >= 0 )
    {
        /*
         *  We have to load all title information
         */

        /* Change the default area */
        p_input->stream.p_selected_area = p_input->stream.pp_areas[i_title];

        /* Ifo VTS, and CSS reading */
        p_method->ifo.i_title = i_title;
        IfoReadVTS( &(p_method->ifo) );
        intf_WarnMsg( 2, "Ifo: VTS initialized" );
    
        if( p_method->b_encrypted )
        {
            p_method->css.i_title = i_title;
            p_method->css.i_title_pos =
                    p_method->ifo.vts.i_pos +
                    p_method->ifo.vts.mat.i_tt_vobs_ssector * DVD_LB_SIZE;
            CSSGetKey( &(p_method->css) );
            intf_WarnMsg( 2, "CSS: VTS key initialized" );
        }
    
        /*
         * Set selected title start and size
         */
        DVDChapterSelect( p_method, 1 );
    
        /* start is : beginning of vts + offset to vobs + offset to vob x */
            
        p_method->i_start =
                    lseek( p_input->i_handle, p_method->i_start, SEEK_SET );

        intf_WarnMsg( 2, "DVD: vobstart at: %lld", p_method->i_start );
        intf_WarnMsg( 2, "DVD: stream size: %lld", p_method->i_size );
        intf_WarnMsg( 2, "DVD: number of chapters: %lld",
                                                   p_method->i_chapter_nb );

        /* Area definition */
        p_input->stream.p_selected_area->i_start = p_method->i_start;
        p_input->stream.p_selected_area->i_size = p_method->i_size;
        p_input->stream.p_selected_area->i_part_nb = p_method->i_chapter_nb;

        /*
         * Destroy obsolete ES by reinitializing program 0
         * and find all ES in title with ifo data
         */
        if( p_input->stream.pp_programs != NULL )
        {
            input_DelProgram( p_input, p_input->stream.pp_programs[0] );
        }

        input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );

        p_es = NULL;

        /* ES 0 -> video MPEG2 */
        p_es = input_AddES( p_input, p_input->stream.pp_programs[0], 0xe0, 0 );
        p_es->i_stream_id = 0xe0;
        p_es->i_type = MPEG2_VIDEO_ES;
        input_SelectES( p_input, p_es );
        intf_WarnMsg( 1, "DVD: Video MPEG2 stream" );
    
        /* Audio ES, in the order they appear in .ifo */
        i_nb = p_method->ifo.vts.mat.i_audio_nb;
    
        i_ac3 = 0x80;
        i_mpeg = 0xc0;

        for( i = 1 ; i <= i_nb ; i++ )
        {

#if 0
    fprintf( stderr, "Audio %d: %x %x %x %x %x %x\n", i,
            p_method->ifo.vts.mat.p_audio_atrt[i].i_coding_mode,
            p_method->ifo.vts.mat.p_audio_atrt[i].i_multichannel_extension,
            p_method->ifo.vts.mat.p_audio_atrt[i].i_type,
            p_method->ifo.vts.mat.p_audio_atrt[i].i_appl_mode,
            p_method->ifo.vts.mat.p_audio_atrt[i].i_foo,
            p_method->ifo.vts.mat.p_audio_atrt[i].i_bar );
#endif

            switch( p_method->ifo.vts.mat.p_audio_atrt[i].i_coding_mode )
            {
            case 0x00:              /* AC3 */
                i_id = ( ( 0x7f + i ) << 8 ) | 0xbd;
                p_es = input_AddES( p_input,
                                    p_input->stream.pp_programs[0], i_id, 0 );
                p_es->i_stream_id = 0xbd;
                p_es->i_type = AC3_AUDIO_ES;
                p_es->b_audio = 1;
                strcpy( p_es->psz_desc, Language( hton16(
                    p_method->ifo.vts.mat.p_audio_atrt[i-1].i_lang_code ) ) ); 

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
                    p_method->ifo.vts.mat.p_audio_atrt[i-1].i_lang_code ) ) ); 

                break;
            case 0x04:              /* LPCM */
                i_id = 0;
                intf_ErrMsg( "DVD: LPCM audio not handled yet" );
                break;
            case 0x06:              /* DTS */
                i_id = 0;
                intf_ErrMsg( "DVD: DTS audio not handled yet" );
                break;
            default:
                i_id = 0;
                intf_ErrMsg( "DVD: unkown audio" );
            }
        
            intf_WarnMsg( 1, "DVD: Audio stream %d %s\t(0x%x)",
                          i, p_es->psz_desc, i_id );
        }
    
        /* Sub Picture ES */
        i_nb = p_method->ifo.vts.mat.i_subpic_nb;
    
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
                strcpy( p_es->psz_desc, Language( hton16(
                    p_method->ifo.vts.mat.p_subpic_atrt[i-1].i_lang_code ) ) ); 
                intf_WarnMsg( 1, "DVD: SPU stream %d %s\t(0x%x)",
                              i, p_es->psz_desc, i_id );
    
                /* The before the last spu has a 0x0 prefix */
                b_last =
                    ( p_method->ifo.vts.mat.p_subpic_atrt[i].i_prefix == 0 ); 
            }
        }

    } // i_title >= 0

    /*
     * Select requested ES
     */
    if( ( i_audio >= 0 ) || ( i_title >= 0 ) )
    {

        /* Audio: we check it is in the range and
         * default it to the first if not */
        if( i_audio > p_method->ifo.vts.mat.i_audio_nb )
        {
            i_audio = 1;
        }

        p_es = p_input->stream.pp_programs[0]->pp_es[i_audio];

        /* We can only have one audio channel */
        /* Look for a preselected one */
        i_index = -1;
        for( i = 0 ; i < p_input->stream.i_selected_es_number ; i++ )
        {
            if( p_input->stream.pp_selected_es[i]->b_audio )
            {
                i_index = i;
                break;
            }
        }

        if( i_index != -1 )
        {
            
            if( p_input->stream.pp_selected_es[i_index] != p_es )
            {
                input_UnSelectES( p_input,
                                  p_input->stream.pp_selected_es[i_index] );
                input_SelectES( p_input, p_es );
                intf_WarnMsg( 1, "DVD: Audio %d selected -> %s (0x%x)",
                              i_audio, p_es->psz_desc, p_es->i_id );
            }
        }
        else
        {
            input_SelectES( p_input, p_es );
            intf_WarnMsg( 1, "DVD: Audio %d selected -> %s (0x%x)",
                          i_audio, p_es->psz_desc, p_es->i_id );
        }
    }

    if( ( i_spu >= 0 ) || ( i_title >= 0 ) )
    {
        /* For spu: no one if none or a not existed one requested */
        if( ( i_spu <= p_method->ifo.vts.mat.i_subpic_nb ) && ( i_spu > 0 ) )
        {
            input_SelectES( p_input, ( p_es = p_input->stream.pp_programs[0]->
                pp_es[ i_spu + p_method->ifo.vts.mat.i_audio_nb ] ) );
    
            intf_WarnMsg( 1, "DVD: SPU   %d selected -> %s (0x%x)",
                          i_spu, p_es->psz_desc, p_es->i_id );
        }
    }

    /*
     * Chapter selection
     */

    if( ( i_chapter > 0 ) &&
        ( i_chapter <= p_input->stream.p_selected_area->i_part_nb ) )
    {
        DVDChapterSelect( p_method, i_chapter );

        p_input->stream.p_selected_area->i_part = i_chapter; 

        DVDSeek( p_input, p_method->i_start -
                          p_input->stream.p_selected_area->i_start );

        intf_WarnMsg( 2, "DVD: Chapter %d start at: %lld", i_chapter,
                                    p_input->stream.p_selected_area->i_tell );
    }

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
    thread_dvd_data_t *  p_method;
    int                  i_title;
    int                  i_chapter;
    int                  i_audio;
    int                  i_spu;
    int                  i;

    if( (p_method = malloc( sizeof(thread_dvd_data_t) )) == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }

    p_input->p_plugin_data = (void *)p_method;
    p_input->p_method_data = NULL;

    p_method->i_fd = p_input->i_handle;
    /* FIXME: read several packets once */
    p_method->i_read_once = 1; 
    p_method->b_encrypted = DVDCheckCSS( p_input );

    lseek( p_input->i_handle, 0, SEEK_SET );

    /* Reading structures initialisation */
    input_NetlistInit( p_input, 4096, 4096, DVD_LB_SIZE,
                       p_method->i_read_once ); 
    intf_WarnMsg( 2, "DVD: Netlist initialized" );

    /* Ifo initialisation */
    p_method->ifo = IfoInit( p_input->i_handle );
    intf_WarnMsg( 2, "Ifo: VMG initialized" );

    /* CSS initialisation */
    if( p_method->b_encrypted )
    {
        p_method->css = CSSInit( p_input->i_handle );

        if( ( p_input->b_error = p_method->css.b_error ) )
        {
            intf_ErrMsg( "CSS fatal error" );
            return;
        }
        intf_WarnMsg( 2, "CSS: initialized" );
    }

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* FIXME: We consider here that one title is one title set
     * it is not true !!! */

    intf_WarnMsg( 2, "DVD: Number of titles: %d\n",
                  p_method->ifo.vmg.mat.i_tts_nb );

#define area p_input->stream.pp_areas
    /* We start from 1 here since area 0 is reserved for video_ts.vob */
    for( i = 1 ; i <= p_method->ifo.vmg.mat.i_tts_nb ; i++ )
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
        area[i]->i_part_nb = 0;
        area[i]->i_part = 1;
        /* Offset to vts_i_0.ifo */
        area[i]->i_plugin_data = p_method->ifo.i_off +
            ( p_method->ifo.vmg.ptt_srpt.p_tts[i-1].i_ssector * DVD_LB_SIZE );
    }   
#undef area

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Get requested title - if none try to find one where is the movie */
    i_title = main_GetIntVariable( INPUT_TITLE_VAR,
                              p_method->ifo.vmg.ptt_srpt.p_tts[0].i_tts_nb );
    if( i_title <= 0 || i_title >= p_method->ifo.vmg.mat.i_tts_nb )
    {
        i_title = p_method->ifo.vmg.ptt_srpt.p_tts[0].i_tts_nb;
    }

    /* Get requested chapter - if none defaults to first one */
    i_chapter = main_GetIntVariable( INPUT_CHAPTER_VAR, 1 );
    if( i_chapter <= 0 )
    {
        i_chapter = 1;
    }

    /* For audio: first one if none or a not existing one specified */
    i_audio = main_GetIntVariable( INPUT_CHANNEL_VAR, 1 );
    if( i_audio <= 0 )
    {
        main_PutIntVariable( INPUT_CHANNEL_VAR, 1 );
        i_audio = 1;
    }

    i_spu = main_GetIntVariable( INPUT_SUBTITLE_VAR, 0 );

    DVDSetArea( p_input, i_title, i_chapter, i_audio, i_spu );

    return;
}

/*****************************************************************************
 * DVDEnd: frees unused data
 *****************************************************************************/
static void DVDEnd( input_thread_t * p_input )
{
    /* FIXME: check order of calls */
//    CSSEnd( p_input );
//    IfoEnd( (ifo_t*)(&p_input->p_plugin_data->ifo ) );
    free( p_input->stream.p_demux_data );
    free( p_input->p_plugin_data );
    input_NetlistEnd( p_input );
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
    thread_dvd_data_t *     p_method;
    netlist_t *             p_netlist;
    struct iovec *          p_vec;
    struct data_packet_s *  p_data;
    u8 *                    pi_cur;
    int                     i_packet_size;
    int                     i_packet;
    int                     i_pos;
    int                     i;
    boolean_t               b_first_packet;

    p_method = ( thread_dvd_data_t * ) p_input->p_plugin_data;
    p_netlist = ( netlist_t * ) p_input->p_method_data;

    /* Get an iovec pointer */
    if( ( p_vec = input_NetlistGetiovec( p_netlist ) ) == NULL )
    {
        intf_ErrMsg( "DVD: read error" );
        return -1;
    }

    /* Reads from DVD */
    readv( p_input->i_handle, p_vec, p_method->i_read_once );

    if( p_method->b_encrypted )
    {
        for( i=0 ; i<p_method->i_read_once ; i++ )
        {
            CSSDescrambleSector( p_method->css.pi_title_key, 
                                 p_vec[i].iov_base );
            ((u8*)(p_vec[i].iov_base))[0x14] &= 0x8F;
        }
    }

    /* Update netlist indexes */
    input_NetlistMviovec( p_netlist, p_method->i_read_once, &p_data );

    i_packet = 0;
    /* Read headers to compute payload length */
    for( i = 0 ; i < p_method->i_read_once ; i++ )
    {
        i_pos = 0;
        b_first_packet = 1;
        while( i_pos < p_netlist->i_buffer_size )
        {
            pi_cur = (u8*)(p_vec[i].iov_base + i_pos);
            /*default header */
            if( U32_AT( pi_cur ) != 0x1BA )
            {
                /* That's the case for all packets, except pack header. */
                i_packet_size = U16_AT( pi_cur + 4 );
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
            }
            if( b_first_packet )
            {
                p_data->b_discard_payload = 0;
                b_first_packet = 0;
            }
            else
            { 
                p_data = input_NetlistNewPacket( p_netlist ,
                                                 i_packet_size + 6 );
                memcpy( p_data->p_buffer,
                        p_vec[i].iov_base + i_pos , i_packet_size + 6 );
            }

            p_data->p_payload_end = p_data->p_payload_start + i_packet_size + 6;
            pp_packets[i_packet] = p_data;
            i_packet++;
            i_pos += i_packet_size + 6;
        }
    }
    pp_packets[i_packet] = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_tell +=
                                        p_method->i_read_once *DVD_LB_SIZE;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( 0 );
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
 *****************************************************************************/
static void DVDSeek( input_thread_t * p_input, off_t i_off )
{
    thread_dvd_data_t *     p_method;
    off_t                   i_pos;
    
    p_method = ( thread_dvd_data_t * )p_input->p_plugin_data;

    /* We have to take care of offset of beginning of title */
    i_pos = i_off + p_input->stream.p_selected_area->i_start;

    /* With DVD, we have to be on a sector boundary */
    i_pos = i_pos & (~0x7ff);

    i_pos = lseek( p_input->i_handle, i_pos, SEEK_SET );

    p_input->stream.p_selected_area->i_tell = i_pos -
                                    p_input->stream.p_selected_area->i_start;

    return;
}



