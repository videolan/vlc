/*****************************************************************************
 * dvd_ifo.c: Functions for ifo parsing
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ifo.c,v 1.17 2001/04/08 07:24:47 stef Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
 *
 * based on:
 *  - libifo by Thomas Mirlacher <dent@cosy.sbg.ac.at>
 *  - IFO structure documentation by Thomas Mirlacher, Björn Englund,
 *  Håkan Hjort
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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "common.h"

#include "intf_msg.h"
#include "dvd_ifo.h"
#include "dvd_udf.h"
#include "dvd_css.h"
#include "input_dvd.h"

/*
 * Local prototypes
 */
void            CommandRead     ( command_desc_t );
static int      ReadTitle       ( ifo_t * , title_t * );
static int      FreeTitle       ( title_t * );
static int      ReadUnitInf     ( ifo_t * , unit_inf_t * );
static int      FreeUnitInf     ( unit_inf_t * );
static int      ReadTitleUnit   ( ifo_t * , title_unit_t * );
static int      FreeTitleUnit   ( title_unit_t * );
static int      ReadVobuMap     ( ifo_t * , vobu_map_t * );
static int      FreeVobuMap     ( vobu_map_t * );
static int      ReadCellInf     ( ifo_t * , cell_inf_t * );
static int      FreeCellInf     ( cell_inf_t * );
static int      FreeTitleSet    ( vts_t * );

/*
 * Macros to process ifo files
 */
 
#define GET( p_field , i_len )                                              \
    {                                                                       \
        read( p_ifo->i_fd , (p_field) , (i_len) );                          \
/*fprintf(stderr, "Pos : %lld Val : %llx\n",                                  \
                                (long long)(p_ifo->i_pos - i_start),        \
                                (long long)*(p_field) );    */                \
        p_ifo->i_pos += i_len;                                              \
    }

#define GETC( p_field )                                                     \
    {                                                                       \
        read( p_ifo->i_fd , (p_field) , 1 );                                \
/*fprintf(stderr, "Pos : %lld Value : %d\n",                                  \
                                (long long)(p_ifo->i_pos - i_start),        \
                                          *(p_field) );*/                     \
        p_ifo->i_pos += 1;                                                  \
    }

#define GETS( p_field )                                                     \
    {                                                                       \
        read( p_ifo->i_fd , (p_field) , 2 );                                \
        *(p_field) = ntohs( *(p_field) );                                   \
/*fprintf(stderr, "Pos : %lld Value : %d\n",                                  \
                                (long long)(p_ifo->i_pos - i_start),        \
                                          *(p_field) );*/                     \
        p_ifo->i_pos += 2;                                                  \
    }

#define GETL( p_field )                                                     \
    {                                                                       \
        read( p_ifo->i_fd , (p_field) , 4 );                                \
        *(p_field) = ntohl( *(p_field) );                                   \
/*fprintf(stderr, "Pos : %lld Value : %d\n",                                  \
                                (long long)(p_ifo->i_pos - i_start),        \
                                          *(p_field) );*/                     \
        p_ifo->i_pos += 4;                                                  \
    }

#define GETLL( p_field )                                                    \
    {                                                                       \
        read( p_ifo->i_fd , (p_field) , 8 );                                \
        *(p_field) = ntoh64( *(p_field) );                                  \
/*fprintf(stderr, "Pos : %lld Value : %lld\n",                                \
                                (long long)(p_ifo->i_pos - i_start),        \
                                            *(p_field) );*/                   \
        p_ifo->i_pos += 8;                                                  \
    }

#define FLUSH( i_len )                                                      \
    {                                                                       \
/*fprintf(stderr, "Pos : %lld\n", (long long)(p_ifo->i_pos - i_start));*/       \
        p_ifo->i_pos = lseek( p_ifo->i_fd ,                               \
                              p_ifo->i_pos + (i_len), SEEK_SET );           \
    }



/*
 * IFO Management.
 */

/*****************************************************************************
 * IfoInit : Creates an ifo structure and prepares for parsing directly
 *           on DVD device. Then reads information from the management table.
 *****************************************************************************/
int IfoInit( ifo_t ** pp_ifo, int i_fd )
{
    ifo_t *             p_ifo;
    u64                 i_temp;
    u32                 i_lba;
    int                 i, j, k;
    off_t               i_start;

    
    p_ifo = malloc( sizeof(ifo_t) );
    if( p_ifo == NULL )
    {
        intf_ErrMsg( "ifo error: unable to allocate memory. aborting" );
        return -1;
    }

    *pp_ifo = p_ifo;

    /* if we are here the dvd device has already been opened */
    p_ifo->i_fd = i_fd;

    /* find the start sector of video information on the dvd */
    i_lba = UDFFindFile( i_fd, "/VIDEO_TS/VIDEO_TS.IFO");

    p_ifo->i_off = (off_t)(i_lba) * DVD_LB_SIZE;
    p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off, SEEK_SET );

    /*
     * read the video manager information table
     */
#define manager_inf     p_ifo->vmg.manager_inf
//fprintf( stderr, "VMGI\n" );

    GET( manager_inf.psz_id , 12 );
    manager_inf.psz_id[12] = '\0';
    GETL( &manager_inf.i_vmg_end_sector );
    FLUSH( 12 );
    GETL( &manager_inf.i_vmg_inf_end_sector );
    FLUSH( 1 );
    GETC( &manager_inf.i_spec_ver );
    GETL( &manager_inf.i_cat );
    GETS( &manager_inf.i_volume_nb );
    GETS( &manager_inf.i_volume );
    GETC( &manager_inf.i_disc_side );
    FLUSH( 19 );
    GETS( &manager_inf.i_title_set_nb );
    GET( manager_inf.ps_provider_id, 32 );
    GETLL( &manager_inf.i_pos_code );
    FLUSH( 24 );
    GETL( &manager_inf.i_vmg_inf_end_byte );
    GETL( &manager_inf.i_first_play_title_start_byte );
    FLUSH( 56 );
    GETL( &manager_inf.i_vob_start_sector );
    GETL( &manager_inf.i_title_inf_start_sector );
    GETL( &manager_inf.i_title_unit_start_sector );
    GETL( &manager_inf.i_parental_inf_start_sector );
    GETL( &manager_inf.i_vts_inf_start_sector );
    GETL( &manager_inf.i_text_data_start_sector );
    GETL( &manager_inf.i_cell_inf_start_sector );
    GETL( &manager_inf.i_vobu_map_start_sector );
    FLUSH( 32 );
//    GETS( &manager_inf.video_atrt );
    FLUSH(2);
    FLUSH( 1 );
    GETC( &manager_inf.i_audio_nb );
//fprintf( stderr, "vmgi audio nb : %d\n", manager_inf.i_audio_nb );
    for( i=0 ; i < 8 ; i++ )
    {
        GETLL( &i_temp );
    }
    FLUSH( 17 );
    GETC( &manager_inf.i_spu_nb );
//fprintf( stderr, "vmgi subpic nb : %d\n", manager_inf.i_subpic_nb );
    for( i=0 ; i < manager_inf.i_spu_nb ; i++ )
    {
        GET( &i_temp, 6 );
        /* FIXME : take care of endianness */
    }

    /*
     * read first play title.
     */
    p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                          manager_inf.i_first_play_title_start_byte,
                          SEEK_SET );
    if( ReadTitle( p_ifo, &p_ifo->vmg.title ) < 0)
    {
        return -1;
    }

    /*
     * fills the title information structure.
     */
#define title_inf       p_ifo->vmg.title_inf
    if( manager_inf.i_title_inf_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                              manager_inf.i_title_inf_start_sector *DVD_LB_SIZE,
                              SEEK_SET );
//fprintf( stderr, "title inf\n" );
    
        GETS( &title_inf.i_title_nb );
//fprintf( stderr, "title_inf: TTU nb %d\n", title_inf.i_title_nb );
        FLUSH( 2 );
        GETL( &title_inf.i_end_byte );
    
        /* parsing of title attributes */
        title_inf.p_attr = malloc( title_inf.i_title_nb *sizeof(title_attr_t) );
        if( title_inf.p_attr == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoInit" );
            return -1;
        }
    
        for( i = 0 ; i < title_inf.i_title_nb ; i++ )
        {
            GETC( &title_inf.p_attr[i].i_play_type );
            GETC( &title_inf.p_attr[i].i_angle_nb );
            GETS( &title_inf.p_attr[i].i_chapter_nb );
            GETS( &title_inf.p_attr[i].i_parental_id );
            GETC( &title_inf.p_attr[i].i_title_set_num );
            GETC( &title_inf.p_attr[i].i_title_num );
            GETL( &title_inf.p_attr[i].i_start_sector );
//fprintf( stderr, "title_inf: %d %d %d\n", ptr.p_tts[i].i_ptt_nb, ptr.p_tts[i].i_tts_nb,ptr.p_tts[i].i_vts_ttn );
        }
    }
    else
    {
        title_inf.p_attr = NULL;
    }
#undef title_inf

    /*
     * fills the title unit structure.
     */
    if( manager_inf.i_title_unit_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        manager_inf.i_title_unit_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        if( ReadTitleUnit( p_ifo, &p_ifo->vmg.title_unit ) < 0 )
        {
            return -1;
        }
    }

    /*
     * fills the structure about parental information.
     */
#define parental        p_ifo->vmg.parental_inf
    if( manager_inf.i_parental_inf_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        manager_inf.i_parental_inf_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        i_start = p_ifo->i_pos;

//fprintf( stderr, "PTL\n" );
    
        GETS( &parental.i_country_nb );
        GETS( &parental.i_vts_nb );
        GETL( &parental.i_end_byte );
        
        parental.p_parental_desc = malloc( parental.i_country_nb *
                                           sizeof(parental_desc_t) );
        if( parental.p_parental_desc == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoInit" );
            return -1;
        }

        for( i = 0 ; i < parental.i_country_nb ; i++ )
        {
            GET( parental.p_parental_desc[i].ps_country_code, 2 );
            FLUSH( 2 );
            GETS( &parental.p_parental_desc[i].i_parental_mask_start_byte );
            FLUSH( 2 );
        }

        parental.p_parental_mask = malloc( parental.i_country_nb *
                                           sizeof(parental_mask_t) );
        if( parental.p_parental_mask == NULL )
        {
            intf_ErrMsg( "ifo erro: out of memory in IfoInit" );
            return -1;
        }

        for( i = 0 ; i < parental.i_country_nb ; i++ )
        {
            p_ifo->i_pos = lseek( p_ifo->i_fd, i_start +
                       parental.p_parental_desc[i].i_parental_mask_start_byte,
                       SEEK_SET );
            for( j = 0 ; j < 8 ; j++ )
            {
                parental.p_parental_mask[i].ppi_mask[j] =
                            malloc( ( parental.i_vts_nb + 1 ) *sizeof(u16) );
                if( parental.p_parental_mask[i].ppi_mask[j] == NULL )
                {
                    intf_ErrMsg( "ifo error: out of memory in IfoInit" );
                    return -1;
                }        
                for( k = 0 ; k < parental.i_vts_nb + 1 ; k++ )
                {
                    GETS( &parental.p_parental_mask[i].ppi_mask[j][k] );
                }
            }
        }
    }
#undef parental

    /*
     * information and attributes about for each vts.
     */
#define vts_inf     p_ifo->vmg.vts_inf
    if( manager_inf.i_vts_inf_start_sector )
    {
        u64             i_temp;

        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        manager_inf.i_vts_inf_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        i_start = p_ifo->i_pos;
    
//fprintf( stderr, "VTS ATTR\n" );
    
        GETS( &vts_inf.i_vts_nb );
//fprintf( stderr, "VTS ATTR Nb: %d\n", vts_inf.i_vts_nb );
        FLUSH( 2 );
        GETL( &vts_inf.i_end_byte );
        vts_inf.pi_vts_attr_start_byte =
                            malloc( vts_inf.i_vts_nb *sizeof(u32) );
        if( vts_inf.pi_vts_attr_start_byte == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoInit" );
            return -1;
        }

        for( i = 0 ; i < vts_inf.i_vts_nb ; i++ )
        {
            GETL( &vts_inf.pi_vts_attr_start_byte[i] );
        }

        vts_inf.p_vts_attr = malloc( vts_inf.i_vts_nb *sizeof(vts_attr_t) );
        if( vts_inf.p_vts_attr == NULL )
        {
            intf_ErrMsg( "ifo erro: out of memory in IfoInit" );
            return -1;
        }

        for( i = 0 ; i < vts_inf.i_vts_nb ; i++ )
        {
            p_ifo->i_pos = lseek( p_ifo->i_fd, i_start +
                                    vts_inf.pi_vts_attr_start_byte[i],
                                    SEEK_SET );
            GETL( &vts_inf.p_vts_attr[i].i_end_byte );
            GETL( &vts_inf.p_vts_attr[i].i_cat_app_type );
    //        GETS( &vts_inf.p_vts_attr[i].vts_menu_video_attr );
            FLUSH(2);
            FLUSH( 1 );
            GETC( &vts_inf.p_vts_attr[i].i_vts_menu_audio_nb );
//fprintf( stderr, "m audio nb : %d\n", vts_inf.p_vts_vts_inf[i].i_vtsm_audio_nb );
            for( j = 0 ; j < 8 ; j++ )
            {
                GETLL( &i_temp );
            }
            FLUSH( 17 );
            GETC( &vts_inf.p_vts_attr[i].i_vts_menu_spu_nb );
//fprintf( stderr, "m subp nb : %d\n", vts_inf.p_vts_vts_inf[i].i_vtsm_subpic_nb );
            for( j = 0 ; j < 28 ; j++ )
            {
                GET( &i_temp, 6 );
                /* FIXME : Fix endianness issue here */
            }
            FLUSH( 2 );
    //        GETS( &vts_inf.p_vts_attr[i].vtstt_video_vts_inf );
            FLUSH(2);
            FLUSH( 1 );
            GETL( &vts_inf.p_vts_attr[i].i_vts_title_audio_nb );
//fprintf( stderr, "tt audio nb : %d\n", vts_inf.p_vts_vts_inf[i].i_vtstt_audio_nb );
            for( j = 0 ; j < 8 ; j++ )
            {
                GETLL( &i_temp );
            }
            FLUSH( 17 );
            GETC( &vts_inf.p_vts_attr[i].i_vts_title_spu_nb );
//fprintf( stderr, "tt subp nb : %d\n", vts_inf.p_vts_vts_inf[i].i_vtstt_subpic_nb );
            for( j=0 ; j<28/*vts_inf.p_vts_vts_inf[i].i_vtstt_subpic_nb*/ ; j++ )
            {
                GET( &i_temp, 6 );
                /* FIXME : Fix endianness issue here */
            }
        }
    }
#undef vts_inf

    /*
     * global cell map.
     */
    if( manager_inf.i_cell_inf_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        manager_inf.i_cell_inf_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        if( ReadCellInf( p_ifo, &p_ifo->vmg.cell_inf ) < 0 )
        {
            return -1;
        }
    }

    /*
     * global vob unit map.
     */
    if( manager_inf.i_vobu_map_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        manager_inf.i_vobu_map_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        if( ReadVobuMap( p_ifo, &p_ifo->vmg.vobu_map ) < 0 )
        {
            return -1;
        }
    }
#undef manager_inf

    p_ifo->vts.b_initialized = 0;

    intf_WarnMsg( 1, "ifo info: vmg initialized" );

    return 0;
}

/*****************************************************************************
 * IfoTitleSet: Parse vts*.ifo files to fill the Video Title Set structure.
 *****************************************************************************/
int IfoTitleSet( ifo_t * p_ifo )
{
    off_t       i_off;
    off_t       i_start;
    u64         i_temp;
    int         i, j;

    if( p_ifo->vts.b_initialized )
    {
        FreeTitleSet( &p_ifo->vts );
    }

    i_off =
    (off_t)( p_ifo->vmg.title_inf.p_attr[p_ifo->i_title-1].i_start_sector )
                   * DVD_LB_SIZE
                   + p_ifo->i_off;

    p_ifo->i_pos = lseek( p_ifo->i_fd, i_off, SEEK_SET );

    p_ifo->vts.i_pos = p_ifo->i_pos;

#define manager_inf p_ifo->vts.manager_inf
    /*
     * reads manager information
     */
//fprintf( stderr, "VTSI\n" );

    GET( manager_inf.psz_id , 12 );
    manager_inf.psz_id[12] = '\0';
    GETL( &manager_inf.i_end_sector );
    FLUSH( 12 );
    GETL( &manager_inf.i_inf_end_sector );
    FLUSH( 1 );
    GETC( &manager_inf.i_spec_ver );
    GETL( &manager_inf.i_cat );
    FLUSH( 90 );
    GETL( &manager_inf.i_inf_end_byte );
    FLUSH( 60 );
    GETL( &manager_inf.i_menu_vob_start_sector );
    GETL( &manager_inf.i_title_vob_start_sector );
    GETL( &manager_inf.i_title_inf_start_sector );
    GETL( &manager_inf.i_title_unit_start_sector );
    GETL( &manager_inf.i_menu_unit_start_sector );
    GETL( &manager_inf.i_time_inf_start_sector );
    GETL( &manager_inf.i_menu_cell_inf_start_sector );
    GETL( &manager_inf.i_menu_vobu_map_start_sector );
    GETL( &manager_inf.i_cell_inf_start_sector );
    GETL( &manager_inf.i_vobu_map_start_sector );
    FLUSH( 24 );
//    GETS( &manager_inf.m_video_atrt );
FLUSH(2);
    FLUSH( 1 );
    GETC( &manager_inf.i_menu_audio_nb );
    for( i=0 ; i<8 ; i++ )
    {
        GETLL( &i_temp );
    }
    FLUSH( 17 );
    GETC( &manager_inf.i_menu_spu_nb );
    for( i=0 ; i<28 ; i++ )
    {
        GET( &i_temp, 6 );
        /* FIXME : take care of endianness */
    }
    FLUSH( 2 );
//    GETS( &manager_inf.video_atrt );
FLUSH(2);
    FLUSH( 1 );
    GETC( &manager_inf.i_audio_nb );
//fprintf( stderr, "vtsi audio nb : %d\n", manager_inf.i_audio_nb );
    for( i=0 ; i<8 ; i++ )
    {
        GETLL( &i_temp );
//fprintf( stderr, "Audio %d: %llx\n", i, i_temp );
        i_temp >>= 8;
        manager_inf.p_audio_attr[i].i_bar = i_temp & 0xff;
        i_temp >>= 8;
        manager_inf.p_audio_attr[i].i_caption = i_temp & 0xff;
        i_temp >>= 8;
        manager_inf.p_audio_attr[i].i_foo = i_temp & 0xff;
        i_temp >>= 8;
        manager_inf.p_audio_attr[i].i_lang_code = i_temp & 0xffff;
        i_temp >>= 16;
        manager_inf.p_audio_attr[i].i_num_channels = i_temp & 0x7;
        i_temp >>= 3;
        manager_inf.p_audio_attr[i].i_test = i_temp & 0x1;
        i_temp >>= 1;
        manager_inf.p_audio_attr[i].i_sample_freq = i_temp & 0x3;
        i_temp >>= 2;
        manager_inf.p_audio_attr[i].i_quantization = i_temp & 0x3;
        i_temp >>= 2;
        manager_inf.p_audio_attr[i].i_appl_mode = i_temp & 0x3;
        i_temp >>= 2;
        manager_inf.p_audio_attr[i].i_type = i_temp & 0x3;
        i_temp >>= 2;
        manager_inf.p_audio_attr[i].i_multichannel_extension = i_temp & 0x1;
        i_temp >>= 1;
        manager_inf.p_audio_attr[i].i_coding_mode = i_temp & 0x7;
    }
    FLUSH( 17 );
    GETC( &manager_inf.i_spu_nb );
//fprintf( stderr, "vtsi subpic nb : %d\n", manager_inf.i_subpic_nb );
    for( i=0 ; i<manager_inf.i_spu_nb ; i++ )
    {
        GET( &i_temp, 6 );
        i_temp = hton64( i_temp ) >> 16;
//fprintf( stderr, "Subpic %d: %llx\n", i, i_temp );
        manager_inf.p_spu_attr[i].i_caption = i_temp & 0xff;
        i_temp >>= 16;
        manager_inf.p_spu_attr[i].i_lang_code = i_temp & 0xffff;
        i_temp >>= 16;
        manager_inf.p_spu_attr[i].i_prefix = i_temp & 0xffff;
    }

    /*
     * reads title information: set of pointers to title
     */
#define title_inf p_ifo->vts.title_inf
    if( manager_inf.i_title_inf_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->vts.i_pos +
                        manager_inf.i_title_inf_start_sector *DVD_LB_SIZE,
                        SEEK_SET );

        i_start = p_ifo->i_pos;
    
//fprintf( stderr, "VTS PTR\n" );
   
        GETS( &title_inf.i_title_nb );
//fprintf( stderr, "VTS title_inf nb: %d\n", title_inf.i_title_nb );
        FLUSH( 2 );
        GETL( &title_inf.i_end_byte );

        title_inf.pi_start_byte = malloc( title_inf.i_title_nb *sizeof(u32) );
        if( title_inf.pi_start_byte == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoTitleSet" );
            return -1;
        }

        for( i = 0 ; i < title_inf.i_title_nb ; i++ )
        {
            GETL( &title_inf.pi_start_byte[i] );
        }

        /* Parsing of tts */
        title_inf.p_title_start = malloc( title_inf.i_title_nb
                                         *sizeof(title_start_t) );
        if( title_inf.p_title_start == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoTitleSet" );
            return -1;
        }

        for( i = 0 ; i < title_inf.i_title_nb ; i++ )
        {
            p_ifo->i_pos = lseek( p_ifo->i_fd, i_start +
                            title_inf.pi_start_byte[i], SEEK_SET );

            GETS( &title_inf.p_title_start[i].i_program_chain_num );
            GETS( &title_inf.p_title_start[i].i_program_num );
//fprintf( stderr, "VTS %d title_inf Pgc: %d Prg: %d\n", i,title_inf.p_title_start[i].i_program_chain_num, title_inf.p_title_start[i].i_program_num );
        }
    }
#undef title_inf

    /*
     * menu unit information
     */
    if( manager_inf.i_menu_unit_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->vts.i_pos +
                        manager_inf.i_menu_unit_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        if( ReadTitleUnit( p_ifo, &p_ifo->vts.menu_unit ) < 0 )
        {
            return -1;
        }
    }

    /*
     * title unit information
     */
    if( manager_inf.i_title_unit_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->vts.i_pos +
                        manager_inf.i_title_unit_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        if( ReadUnitInf( p_ifo, &p_ifo->vts.title_unit ) < 0 )
        {
            return -1;
        }
    }

    /*
     * time map inforamtion
     */
#define time_inf p_ifo->vts.time_inf
    if( manager_inf.i_time_inf_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->vts.i_pos +
                        manager_inf.i_time_inf_start_sector *DVD_LB_SIZE,
                        SEEK_SET );

//fprintf( stderr, "TMAP\n" );

        GETS( &time_inf.i_nb );
        FLUSH( 2 );
        GETL( &time_inf.i_end_byte );

        time_inf.pi_start_byte = malloc( time_inf.i_nb *sizeof(u32) );
        if( time_inf.pi_start_byte == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoTitleSet" );
            return -1;
        }

        for( i = 0 ; i < time_inf.i_nb ; i++ )
        {    
            GETL( &time_inf.pi_start_byte[i] );
        }

        time_inf.p_time_map = malloc( time_inf.i_nb *sizeof(time_map_t) );
        if( time_inf.p_time_map == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoTitleSet" );
            return -1;
        }

        for( i = 0 ; i < time_inf.i_nb ; i++ )
        {    
            GETC( &time_inf.p_time_map[i].i_time_unit );
            FLUSH( 1 );
            GETS( &time_inf.p_time_map[i].i_entry_nb );

            time_inf.p_time_map[i].pi_sector =
                     malloc( time_inf.p_time_map[i].i_entry_nb *sizeof(u32) );
            if( time_inf.p_time_map[i].pi_sector == NULL )
            {
                intf_ErrMsg( "ifo error: out of memory in IfoTitleSet" );
                return -1;
            }

            for( j = 0 ; j < time_inf.p_time_map[i].i_entry_nb ; j++ )
            {
                GETL( &time_inf.p_time_map[i].pi_sector[j] );
            }
        }
    }
#undef time_inf

    if( manager_inf.i_menu_cell_inf_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->vts.i_pos +
                        manager_inf.i_menu_cell_inf_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        if( ReadCellInf( p_ifo, &p_ifo->vts.menu_cell_inf ) < 0 )
        {
            return -1;
        }
    }

    if( manager_inf.i_menu_vobu_map_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->vts.i_pos +
                        manager_inf.i_menu_vobu_map_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        if( ReadVobuMap( p_ifo, &p_ifo->vts.menu_vobu_map ) < 0 )
        {
            return -1;
        }
    }

    if( manager_inf.i_cell_inf_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->vts.i_pos +
                        manager_inf.i_cell_inf_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        if( ReadCellInf( p_ifo, &p_ifo->vts.cell_inf ) )
        {
            return -1;
        }
    }

    if( manager_inf.i_vobu_map_start_sector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->vts.i_pos +
                        manager_inf.i_vobu_map_start_sector *DVD_LB_SIZE,
                        SEEK_SET );
        if( ReadVobuMap( p_ifo, &p_ifo->vts.vobu_map ) )
        {
            return -1;
        }
    }

    intf_WarnMsg( 2, "ifo info: VTS %d initialized",
         p_ifo->vmg.title_inf.p_attr[p_ifo->i_title-1].i_title_set_num );

#undef manager_inf

    p_ifo->vts.b_initialized = 1;

    return 0;
}

/*****************************************************************************
 * FreeTitleSet : free all structures allocated by IfoTitleSet
 *****************************************************************************/
static int FreeTitleSet( vts_t * p_vts )
{
    int     i;

    if( p_vts->manager_inf.i_vobu_map_start_sector )
    {
        FreeVobuMap( &p_vts->vobu_map );
    }

    if( p_vts->manager_inf.i_cell_inf_start_sector )
    {
        FreeCellInf( &p_vts->cell_inf );
    }

    if( p_vts->manager_inf.i_menu_vobu_map_start_sector )
    {
        FreeVobuMap( &p_vts->menu_vobu_map );
    }

    if( p_vts->manager_inf.i_menu_cell_inf_start_sector )
    {
        FreeCellInf( &p_vts->menu_cell_inf );
    }

    if( p_vts->manager_inf.i_time_inf_start_sector )
    {
        for( i = 0 ; i < p_vts->time_inf.i_nb ; i++ )
        {
            free( p_vts->time_inf.p_time_map[i].pi_sector );
        }

        free( p_vts->time_inf.p_time_map );
        free( p_vts->time_inf.pi_start_byte );
    }

    if( p_vts->manager_inf.i_title_unit_start_sector )
    {
        FreeUnitInf( &p_vts->title_unit );
    }

    if( p_vts->manager_inf.i_menu_unit_start_sector )
    {
        FreeTitleUnit( &p_vts->menu_unit );
    }

    if( p_vts->manager_inf.i_title_inf_start_sector )
    {
        free( p_vts->title_inf.pi_start_byte );
        free( p_vts->title_inf.p_title_start );
    }

    p_vts->b_initialized = 0;
    
    return 0;
}

/*****************************************************************************
 * IfoEnd : Frees all the memory allocated to ifo structures
 *****************************************************************************/
void IfoEnd( ifo_t * p_ifo )
{
    int     i, j;

    FreeTitleSet( &p_ifo->vts );

    if( p_ifo->vmg.manager_inf.i_vobu_map_start_sector )
    {
        FreeVobuMap( &p_ifo->vmg.vobu_map );
    }

    if( p_ifo->vmg.manager_inf.i_cell_inf_start_sector )
    {
        FreeCellInf( &p_ifo->vmg.cell_inf );
    }

    if( p_ifo->vmg.manager_inf.i_vts_inf_start_sector )
    {
        free( p_ifo->vmg.vts_inf.p_vts_attr );
        free( p_ifo->vmg.vts_inf.pi_vts_attr_start_byte );
    }

    /* free parental information structures */
    if( p_ifo->vmg.manager_inf.i_parental_inf_start_sector )
    {
        for( i = 0 ; i < p_ifo->vmg.parental_inf.i_country_nb ; i++ )
        {
            for( j = 0 ; j < 8 ; j++ )
            {
                free( p_ifo->vmg.parental_inf.p_parental_mask[i].ppi_mask[j] );
            }
        }

        free( p_ifo->vmg.parental_inf.p_parental_mask );
        free( p_ifo->vmg.parental_inf.p_parental_desc );
    }

    if( p_ifo->vmg.manager_inf.i_title_unit_start_sector )
    {
        FreeTitleUnit( &p_ifo->vmg.title_unit );
    }

    if( p_ifo->vmg.manager_inf.i_title_inf_start_sector )
    {
        free( p_ifo->vmg.title_inf.p_attr );
    }

    FreeTitle( &p_ifo->vmg.title );

    free( p_ifo );

    return;
}
/*
 * Function common to Video Manager and Video Title set Processing
 */

/*****************************************************************************
 * ReadTitle : Fills the title structure.
 *****************************************************************************
 * Titles are logical stream units that correspond to a whole inside the dvd.
 * Several title can point to the same part of the physical DVD, and give
 * map to different anglesfor instance.
 *****************************************************************************/
#define GETCOMMAND( p_com )                                                 \
    {                                                                       \
        read( p_ifo->i_fd , (p_com) , 8 );                                  \
/*fprintf(stderr, "Pos : %lld Type : %d direct : %d cmd : %d dircmp : %d cmp : %d subcmd : %d v0 : %d v2 : %d v4 : %d\n",                                  \
                                (long long)(p_ifo->i_pos - i_start),        \
                                (int)((p_com)->i_type),                     \
                                (int)((p_com)->i_direct),                   \
                                (int)((p_com)->i_cmd),                      \
                                (int)((p_com)->i_dir_cmp),                  \
                                (int)((p_com)->i_cmp),                      \
                                (int)((p_com)->i_sub_cmd),                  \
                                (int)((p_com)->data.pi_16[0]),              \
                                (int)((p_com)->data.pi_16[1]),              \
                                (int)((p_com)->data.pi_16[2]));*/             \
/*        CommandRead( *(p_com) );*/                                            \
        p_ifo->i_pos += 8;                                                  \
    }

static int ReadTitle( ifo_t * p_ifo, title_t * p_title )
{
    off_t     i_start;
    int       i;

    i_start = p_ifo->i_pos;

//fprintf( stderr, "PGC\n" );

    FLUSH(2);
    GETC( &p_title->i_chapter_nb );
    GETC( &p_title->i_cell_nb );
//fprintf( stderr, "title: Prg: %d Cell: %d\n", pgc.i_prg_nb, pgc.i_cell_nb );
    GETL( &p_title->i_play_time );
    GETL( &p_title->i_prohibited_user_op );
    for( i=0 ; i<8 ; i++ )
    {
        GETS( &p_title->pi_audio_status[i] );
    }
    for( i=0 ; i<32 ; i++ )
    {
        GETL( &p_title->pi_subpic_status[i] );
    }
    GETS( &p_title->i_next_title_num );
    GETS( &p_title->i_prev_title_num );
    GETS( &p_title->i_go_up_title_num );
//fprintf( stderr, "title: Prev: %d Next: %d Up: %d\n",pgc.i_prev_pgc_nb ,pgc.i_next_pgc_nb, pgc.i_goup_pgc_nb );
    GETC( &p_title->i_still_time );
    GETC( &p_title->i_play_mode );
    for( i=0 ; i<16 ; i++ )
    {
        GETL( &p_title->pi_yuv_color[i] );
        /* FIXME : We have to erase the extra bit */
    }
    GETS( &p_title->i_command_start_byte );
    GETS( &p_title->i_chapter_map_start_byte );
    GETS( &p_title->i_cell_play_start_byte );
    GETS( &p_title->i_cell_pos_start_byte );

    /* parsing of command_t */
    if( p_title->i_command_start_byte )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd,
                              i_start + p_title->i_command_start_byte,
                              SEEK_SET );

        /* header */
        GETS( &p_title->command.i_pre_command_nb );
        GETS( &p_title->command.i_post_command_nb );
        GETS( &p_title->command.i_cell_command_nb );
        FLUSH( 2 );

        /* pre-title commands */
        if( p_title->command.i_pre_command_nb )
        {
            p_title->command.p_pre_command =
                           malloc( p_title->command.i_pre_command_nb
                                   *sizeof(command_desc_t) );

            if( p_title->command.p_pre_command == NULL )
            {
                intf_ErrMsg( "ifo error: out of memory in ReadTitle" );
                return -1;
            }

            for( i = 0 ; i < p_title->command.i_pre_command_nb ; i++ )
            {
                GETCOMMAND( &p_title->command.p_pre_command[i] );
            }
        }
        else
        {
            p_title->command.p_pre_command = NULL;
        }

        /* post-title commands */
        if( p_title->command.i_post_command_nb )
        {
            p_title->command.p_post_command =
                        malloc( p_title->command.i_post_command_nb
                                *sizeof(command_desc_t) );

            if( p_title->command.p_post_command == NULL )
            {
                intf_ErrMsg( "ifo error: out of memory in ReadTitle" );
                return -1;
            }

            for( i=0 ; i<p_title->command.i_post_command_nb ; i++ )
            {
                GETCOMMAND( &p_title->command.p_post_command[i] );
            }
        }
        else
        {
            p_title->command.p_post_command = NULL;
        }

        /* cell commands */
        if( p_title->command.i_cell_command_nb )
        {
            p_title->command.p_cell_command =
                        malloc( p_title->command.i_cell_command_nb
                                *sizeof(command_desc_t) );

            if( p_title->command.p_cell_command == NULL )
            {
                intf_ErrMsg( "ifo error: out of memory in ReadTitle" );
                return -1;
            }

            for( i=0 ; i<p_title->command.i_cell_command_nb ; i++ )
            {
                GETCOMMAND( &p_title->command.p_cell_command[i] );
            }
        }
        else
        {
            p_title->command.p_cell_command = NULL;
        }
    }

    /* parsing of chapter_map_t: it gives the entry cell for each chapter */
    if( p_title->i_chapter_map_start_byte )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd,
                              i_start + p_title->i_chapter_map_start_byte,
                              SEEK_SET );
        
        p_title->chapter_map.pi_start_cell =
                    malloc( p_title->i_chapter_nb *sizeof(chapter_map_t) );
        
        if( p_title->chapter_map.pi_start_cell == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in Read Title" );
            return -1;
        }

        GET( p_title->chapter_map.pi_start_cell, p_title->i_chapter_nb );
    }
    else
    {
        p_title->chapter_map.pi_start_cell = NULL; 
    }

    /* parsing of cell_play_t */
    if( p_title->i_cell_play_start_byte )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd,
                              i_start + p_title->i_cell_play_start_byte,
                              SEEK_SET );

        p_title->p_cell_play = malloc( p_title->i_cell_nb
                                       *sizeof(cell_play_t) );
    
        if( p_title->p_cell_play == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in ReadTitle" );
            return -1;
        }

        for( i = 0 ; i < p_title->i_cell_nb ; i++ )
        {
            GETS( &p_title->p_cell_play[i].i_category );
            GETC( &p_title->p_cell_play[i].i_still_time );
            GETC( &p_title->p_cell_play[i].i_command_nb );
            GETL( &p_title->p_cell_play[i].i_play_time );
            GETL( &p_title->p_cell_play[i].i_start_sector );
            GETL( &p_title->p_cell_play[i].i_first_ilvu_vobu_esector );
            GETL( &p_title->p_cell_play[i].i_last_vobu_start_sector );
            GETL( &p_title->p_cell_play[i].i_end_sector );
        }
    }

    /* Parsing of cell_pos_t */
    if( p_title->i_cell_pos_start_byte )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd,
                              i_start + p_title->i_cell_pos_start_byte,
                              SEEK_SET );

        p_title->p_cell_pos = malloc( p_title->i_cell_nb
                                      *sizeof(cell_pos_t) );

        if( p_title->p_cell_pos == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory" );
            return -1;
        }

        for( i = 0 ; i < p_title->i_cell_nb ; i++ )
        {
            GETS( &p_title->p_cell_pos[i].i_vob_id );
            FLUSH( 1 );
            GETC( &p_title->p_cell_pos[i].i_cell_id );
        }
    } 

    return 0;
}

/*****************************************************************************
 * FreeTitle: frees alla structure allocated by a call to ReadTitle
 *****************************************************************************/
static int FreeTitle( title_t * p_title )
{
    if( p_title->i_command_start_byte )
    {
        if( p_title->command.i_pre_command_nb )
        {
            free( p_title->command.p_pre_command );
        }

        if( p_title->command.i_post_command_nb )
        {
            free( p_title->command.p_post_command );
        }

        if( p_title->command.i_cell_command_nb )
        {
            free( p_title->command.p_cell_command );
        }

        if( p_title->i_chapter_map_start_byte )
        {
            free( p_title->chapter_map.pi_start_cell );
        }

        if( p_title->i_cell_play_start_byte )
        {
            free( p_title->p_cell_play );
        }

        if( p_title->i_cell_pos_start_byte )
        {
            free( p_title->p_cell_pos );
        }
    }

    return 0;
}

/*****************************************************************************
 * ReadUnitInf : Fills Menu Language Unit Table/ PGC Info Table
 *****************************************************************************/
static int ReadUnitInf( ifo_t * p_ifo, unit_inf_t * p_unit_inf )
{
    off_t           i_start;
    int             i;

    i_start = p_ifo->i_pos;
//fprintf( stderr, "Unit\n" );

    GETS( &p_unit_inf->i_title_nb );
    FLUSH( 2 );
    GETL( &p_unit_inf->i_end_byte );

    p_unit_inf->p_title =
            malloc( p_unit_inf->i_title_nb *sizeof(unit_title_t) );
    if( p_unit_inf->p_title == NULL )
    {
        intf_ErrMsg( "ifo error: out of memory in ReadUnit" );
        return -1;
    }

    for( i = 0 ; i < p_unit_inf->i_title_nb ; i++ )
    {
        GETC( &p_unit_inf->p_title[i].i_category_mask );
        GETC( &p_unit_inf->p_title[i].i_category );
        GETS( &p_unit_inf->p_title[i].i_parental_mask );
        GETL( &p_unit_inf->p_title[i].i_title_start_byte );
    }

    for( i = 0 ; i < p_unit_inf->i_title_nb ; i++ )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_start +
                              p_unit_inf->p_title[i].i_title_start_byte,
                              SEEK_SET );
//fprintf( stderr, "Unit: PGC %d\n", i );
        ReadTitle( p_ifo, &p_unit_inf->p_title[i].title );
    }

    return 0;
}

/*****************************************************************************
 * FreeUnitInf : frees a structure allocated by ReadUnit
 *****************************************************************************/ 
static int FreeUnitInf( unit_inf_t * p_unit_inf )
{
    if( p_unit_inf->p_title != NULL )
    {
        free( p_unit_inf->p_title );
    }

    return 0;
}


/*****************************************************************************
 * ReadTitleUnit: Fills the Title Unit structure.
 *****************************************************************************/
static int ReadTitleUnit( ifo_t * p_ifo, title_unit_t * p_title_unit )
{
    int             i;
    off_t           i_start = p_ifo->i_pos;

//fprintf( stderr, "Unit Table\n" );

    GETS( &p_title_unit->i_unit_nb );
    FLUSH( 2 );
    GETL( &p_title_unit->i_end_byte );

    p_title_unit->p_unit = malloc( p_title_unit->i_unit_nb *sizeof(unit_t) );
    if( p_title_unit->p_unit == NULL )
    {
        intf_ErrMsg( "ifo error: out of memory in ReadTitleUnit" );
        return -1;
    }

    for( i = 0 ; i < p_title_unit->i_unit_nb ; i++ )
    {
        GET( p_title_unit->p_unit[i].ps_lang_code, 2 );
        FLUSH( 1 );
        GETC( &p_title_unit->p_unit[i].i_existence_mask );
        GETL( &p_title_unit->p_unit[i].i_unit_inf_start_byte );
    }

    p_title_unit->p_unit_inf =
                malloc( p_title_unit->i_unit_nb *sizeof(unit_inf_t) );
    if( p_title_unit->p_unit_inf == NULL )
    {
        intf_ErrMsg( "ifo error: out of memory in ReadTitleUnit" );
        return -1;
    }

    for( i = 0 ; i < p_title_unit->i_unit_nb ; i++ )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_start +
                              p_title_unit->p_unit[i].i_unit_inf_start_byte,
                              SEEK_SET );
        ReadUnitInf( p_ifo, &p_title_unit->p_unit_inf[i] );
    }

    return 0;
}

/*****************************************************************************
 * FreeTitleUnit: frees a structure allocateed by ReadTitleUnit
 *****************************************************************************/
static int FreeTitleUnit( title_unit_t * p_title_unit )
{
    int     i;

    if( p_title_unit->p_unit_inf != NULL )
    {
        for( i = 0 ; i < p_title_unit->i_unit_nb ; i++ )
        {
            FreeUnitInf( &p_title_unit->p_unit_inf[i] );
        }

        free( p_title_unit->p_unit_inf );
    }

    return 0;
}

/*****************************************************************************
 * ReadCellInf : Fills the Cell Information structure.
 *****************************************************************************/
static int ReadCellInf( ifo_t * p_ifo, cell_inf_t * p_cell_inf )
{
    off_t           i_start;
    int             i;

    i_start = p_ifo->i_pos;
//fprintf( stderr, "CELL ADD\n" );

    GETS( &p_cell_inf->i_vob_nb );
    FLUSH( 2 );
    GETL( &p_cell_inf->i_end_byte );

    p_cell_inf->i_cell_nb =
        ( i_start + p_cell_inf->i_end_byte + 1 - p_ifo->i_pos )
        / sizeof(cell_map_t);

    p_cell_inf->p_cell_map =
                malloc( p_cell_inf->i_cell_nb *sizeof(cell_map_t) );
    if( p_cell_inf->p_cell_map == NULL )
    {
        intf_ErrMsg( "ifo error: out of memory in ReadCellInf" );
        return -1;
    }

    for( i = 0 ; i < p_cell_inf->i_cell_nb ; i++ )
    {
        GETS( &p_cell_inf->p_cell_map[i].i_vob_id );
        GETC( &p_cell_inf->p_cell_map[i].i_cell_id );
        FLUSH( 1 );
        GETL( &p_cell_inf->p_cell_map[i].i_start_sector );
        GETL( &p_cell_inf->p_cell_map[i].i_end_sector );
    }
    
    return 0;
}

/*****************************************************************************
 * FreeCellInf : frees structures allocated by ReadCellInf
 *****************************************************************************/
static int FreeCellInf( cell_inf_t * p_cell_inf )
{
    free( p_cell_inf->p_cell_map );

    return 0;
}

/*****************************************************************************
 * ReadVobuMap : Fills the VOBU Map structure.
 *****************************************************************************/
static int ReadVobuMap( ifo_t * p_ifo, vobu_map_t * p_vobu_map )
{
    off_t               i_start;
    int                 i, i_max;
    
    i_start = p_ifo->i_pos;
//fprintf( stderr, "VOBU ADMAP\n" );

    GETL( &p_vobu_map->i_end_byte );
    i_max = ( i_start + p_vobu_map->i_end_byte + 1 - p_ifo->i_pos )
            / sizeof(u32);

    p_vobu_map->pi_vobu_start_sector = malloc( i_max *sizeof(u32) );
    if( p_vobu_map->pi_vobu_start_sector == NULL )
    {
        intf_ErrMsg( "ifo error: out of memory in ReadVobuMap" );
        return -1;
    }

    for( i = 0 ; i < i_max ; i++ )
    {
        GETL( &p_vobu_map->pi_vobu_start_sector[i] );
    }

    return 0;
}

/*****************************************************************************
 * FreeVobuMap: frees structures allocated by ReadVobuMap
 *****************************************************************************/
static int FreeVobuMap( vobu_map_t * p_vobu_map )
{
    free( p_vobu_map->pi_vobu_start_sector );

    return 0;
}

/*
 * IFO virtual machine : a set of commands that give the
 * interactive behaviour of the dvd
 */
#if 0

#define OP_VAL_16(i) (ntoh16( com.data.pi_16[i]))
#define OP_VAL_8(i) ((com.data.pi_8[i]))

static char ifo_reg[][80]=
{
    "Menu_Language_Code",
    "Audio_Stream_#",
    "SubPicture_Stream_#",
    "Angle_#",
    "VTS_#",
    "VTS_Title_#",
    "PGC_#",
    "PTT_#",
    "Highlighted_Button_#",
    "Nav_Timer",
    "TimedPGC",
    "Karaoke_audio_mixing_mode",
    "Parental_mgmt_country_code",
    "Parental_Level",
    "Player_Video_Cfg",
    "Player_Audio_Cfg",
    "Audio_language_code_setting",
    "Audio_language_extension_code",
    "SPU_language_code_setting",
    "SPU_language_extension_code",
    "?Player_Regional_Code",
    "Reserved_21",
    "Reserved_22",
    "Reserved_23"
};

static char * IfoMath( char val )
{
    static char math_op[][10] =
    {
        "none",
        "=", 
        "<->",    // swap
        "+=",
        "-=",
        "*=",
        "/=",
        "%=",
        "rnd",    // rnd
        "&=",
        "|=",
        "^=",
        "??",    // invalid
        "??",    // invalid
        "??",    // invalid
        "??"    // invalid
    };

    return (char *) math_op[val & 0x0f];
}


char ifo_cmp[][10] =
{
    "none",
    "&&",
    "==",
    "!=",
    ">=",
    ">",
    "<",
    "<="
};

char ifo_parental[][10] =
{
    "0",
    "G",
    "2",
    "PG",
    "PG-13",
    "5",
    "R",
    "NC-17"
};

char ifo_menu_id[][80] =
{
    "-0-",
    "-1-",
    "Title (VTS menu)",
    "Root",
    "Sub-Picture",
    "Audio",
    "Angle",
    "Part of Title",
};

char * IfoMenuName( char index )
{
    return ifo_menu_id[index&0x07];
}

static void IfoRegister( u16 i_data, u8 i_direct)
{
    if( i_direct )
    {
        if( 0/*isalpha( i_data >> 8 & 0xff )*/ )
        {
            printf("'%c%c'", i_data>>8&0xff, i_data&0xff);
        }
        else
        {
            printf("0x%02x", i_data);
        }
    }
    else
    {
        if( i_data & 0x80 )
        {
            i_data &= 0x1f;

            if( i_data > 0x17 )
            {
                printf("s[ILL]");
            }
            else
            {
                printf("s[%s]", ifo_reg[i_data]);
            }
        }
        else
        {
            i_data &= 0x1f;

            if( i_data > 0xf )
            {
                printf("r[ILL]");
            }
            else
            {
                printf("r[0x%02x]", i_data);
            }
        }
    }
}

static void IfoAdvanced( u8 *pi_code )
{
    u8      i_cmd = pi_code[0];

    printf(" { ");

    if( pi_code[1]>>2 )
    {
        printf( " Highlight button %d; ", pi_code[1]>>2 );
    }

    if( i_cmd == 0xff )
    {
        printf( " Illegal " );
    }

    if( i_cmd == 0x00 )
    {
        printf( "ReSuME %d", pi_code[7] );
    }
    else if( ( i_cmd & 0x06) == 0x02 )
    {    // XX01Y
        printf ("Link to %s cell ", ( i_cmd & 0x01 ) ? "prev" : "next");
    }
    else
    {
        printf( "advanced (0x%02x) ", i_cmd );
    }
    printf(" } ");
}

static void IfoJmp( ifo_command_t com )
{

    printf ("jmp ");

    switch( com.i_sub_cmd )
    {
    case 0x01:
        printf( "Exit" );
        break;
    case 0x02:
        printf( "VTS 0x%02x", OP_VAL_8(3) );
        break;
    case 0x03:
        printf( "This VTS Title 0x%02x", OP_VAL_8(3) );
        break;
    case 0x05:
        printf( "This VTS Title 0x%02x Part 0x%04x",
                            OP_VAL_8(3),
                            OP_VAL_8(0)<<8|OP_VAL_8(1));
        break;
    case 0x06:
#if 0
            printf ("in SystemSpace ");
            switch (OP_VAL_8(3)>>4) {
                case 0x00:
                    printf ("to play first PGC");
                    break;
                case 0x01: {
                    printf ("to menu \"%s\"", decode_menuname (OP_VAL_8(3)));
                }
                    break;
                case 0x02:
                    printf ("to VTS 0x%02x and TTN 0x%02x", OP_VAL_8(1), OP_VAL_8(2));
                    break;
                case 0x03:
                    printf ("to VMGM PGC number 0x%02x", OP_VAL_8(0)<<8 | OP_VAL_8(1));
                    break;
                case 0x08:
                    printf ("vts 0x%02x lu 0x%02x menu \"%s\"", OP_VAL_8(2), OP_VAL_8(1), decode_menuname (OP_VAL_8(3)));
                    break;
#else
        switch( OP_VAL_8(3)>>6 )
        {
        case 0x00:
            printf( "to play first PGC" );
            break;                
        case 0x01:
            printf( "to VMG title menu (?)" );
            break;
        case 0x02:
            printf( "vts 0x%02x lu 0x%02x menu \"%s\"",
                            OP_VAL_8(2),
                            OP_VAL_8(1),
                            IfoMenuName( OP_VAL_8(3)&0xF ) );
            break;                
        case 0x03:
            printf( "vmg pgc 0x%04x (?)", ( OP_VAL_8(0)<<8) | OP_VAL_8(1) );
            break;
#endif
        }
        break;
    case 0x08:
#if 0
            switch(OP_VAL_8(3)>>4) {
                case 0x00:
                    printf ("system first pgc");
                    break;
                case 0x01:
                    printf ("system title menu");
                    break;
                case 0x02:
                    printf ("system menu \"%s\"", decode_menuname (OP_VAL_8(3)));
                    break;
                case 0x03:
                    printf ("system vmg pgc %02x ????", OP_VAL_8(0)<<8|OP_VAL_8(1));
                    break;
                case 0x08:
                    printf ("system lu 0x%02x menu \"%s\"", OP_VAL_8(2), decode_menuname (OP_VAL_8(3)));
                    break;
                case 0x0c:
                    printf ("system vmg pgc 0x%02x", OP_VAL_8(0)<<8|OP_VAL_8(1));
                    break;
            }
#else
        // OP_VAL_8(2) is number of cell
        // it is processed BEFORE switch
        // under some conditions, it is ignored
        // I don't understand exactly what it means
        printf( " ( spec cell 0x%02X ) ", OP_VAL_8(2) ); 

        switch( OP_VAL_8(3)>>6 )
        {
        case 0:
            printf( "to FP PGC" );
            break;
        case 1:
            printf( "to VMG root menu (?)" );
            break;
        case 2:
            printf( "to VTS menu \"%s\" (?)",
                    IfoMenuName(OP_VAL_8(3)&0xF) );
            break; 
        case 3:
            printf( "vmg pgc 0x%02x (?)", (OP_VAL_8(0)<<8)|OP_VAL_8(1) );
            break;
        }    
#endif
        break;
    }
}

static void IfoLnk( ifo_command_t com )
{
    u16     i_button=OP_VAL_8(4)>>2;

    printf ("lnk to ");

    switch( com.i_sub_cmd )
    {
    case 0x01:
        IfoAdvanced( &OP_VAL_8(4) );
        break;

    case 0x04:
        printf( "PGC 0x%02x", OP_VAL_16(2) );
        break; 

    case 0x05:
        printf( "PTT 0x%02x", OP_VAL_16(2) );
        break; 

    case 0x06:
        printf( "Program 0x%02x this PGC", OP_VAL_8(5) );
        break;

    case 0x07:
        printf( "Cell 0x%02x this PGC", OP_VAL_8(5) );
        break;
    default:
        return;
    }

    if( i_button )
    {
        printf( ", Highlight 0x%02x", OP_VAL_8(4)>>2 );
    }
            
}

void IfoSetSystem( ifo_command_t com )
{
    switch( com.i_cmd )
    {
    case 1: {
        int i;

        for( i=1; i<=3; i++ )
        {
            if( OP_VAL_8(i)&0x80 )
            {
                if( com.i_direct )
                {
                    printf ("s[%s] = 0x%02x;", ifo_reg[i], OP_VAL_8(i)&0xf);
                }
                else
                {
                    printf ("s[%s] = r[0x%02x];", ifo_reg[i], OP_VAL_8(i)&0xf);
                }
            }
        }
#if 0
                if(op->direct) {
                        if(OP_VAL_8(1]&0x80)
                                printf ("s[%s] = 0x%02x;", reg_name[1], OP_VAL_8(1]&0xf);
                        if(OP_VAL_8(2)&0x80)
//DENT: lwhat about 0x7f here ???
                                printf ("s[%s] = 0x%02x;", reg_name[2], OP_VAL_8(2)&0x7f);
                        if(OP_VAL_8(3)&0x80)
                                printf ("s[%s] = 0x%02x;", reg_name[3], OP_VAL_8(3)&0xf);
                } else {
                        if(OP_VAL_8(1)&0x80)
                                printf ("s[%s] = r[0x%02x];", reg_name[1], OP_VAL_8(1)&0xf);
                        if(OP_VAL_8(2)&0x80)
                                printf ("s[%s] = r[0x%02x];", reg_name[2], OP_VAL_8(2)&0xf);
                        if(OP_VAL_8(3)&0x80)
                                printf ("s[%s] = r[0x%02x];", reg_name[3], OP_VAL_8(3)&0xf);
                }
#endif
        }
        break;
    case 2:
        if( com.i_direct )
        {
            printf( "s[%s] = 0x%02x", ifo_reg[9], OP_VAL_16(0) );
        }
        else
        {
            printf( "s[%s] = r[0x%02x]", ifo_reg[9], OP_VAL_8(1)&0x0f );
        }

        printf( "s[%s] = (s[%s]&0x7FFF)|0x%02x", 
                        ifo_reg[10], ifo_reg[10], OP_VAL_16(1)&0x8000);
        break;
    case 3:
        if( com.i_direct )
        {
            printf( "r[0x%02x] = 0x%02x", OP_VAL_8(3)&0x0f, OP_VAL_16(0) );
        }
        else
        {
            printf ("r[r[0x%02x]] = r[0x%02x]",
                                    OP_VAL_8(3)&0x0f, OP_VAL_8(1)&0x0f);
        }
        break;
    case 4:
        //actually only bits 00011100 00011100 are set
        if( com.i_direct )
        {
            printf ("s[%s] = 0x%02x", ifo_reg[11], OP_VAL_16(1));
        }
        else
        {
            printf ("s[%s] = r[0x%02x]", ifo_reg[11], OP_VAL_8(3)&0x0f );
        }
        break;
    case 6:
        //actually,
        //s[%s]=(r[%s]&0x3FF) | (0x%02x << 0xA);
        //but it is way too ugly
        if( com.i_direct )
        {
            printf( "s[%s] = 0x%02x", ifo_reg[8], OP_VAL_8(2)>>2 );
        }
        else
        {
            printf( "s[%s] = r[0x%02x]", ifo_reg[8], OP_VAL_8(3)&0x0f );
        }
        break;
    default:
        printf ("unknown");
    }
}

static void IfoSet( ifo_command_t com )
{
    IfoRegister( OP_VAL_16(0), 0 );
    printf( " %s ", IfoMath( com.i_cmd ) );
    IfoRegister( OP_VAL_16(1), com.i_direct );
}

/*****************************************************************************
 * CommandRead : translates the command strings in ifo into command
 * structures.
 *****************************************************************************/
void CommandRead( ifo_command_t com )
{
    u8*     pi_code = (u8*)(&com);

    switch( com.i_type )
    {
    /* Goto */
    case 0:
        /* Main command */
        if( !pi_code[1] )
        {
            printf( "NOP\n" );
        }
        else
        {
            if( com.i_cmp )
            {
                printf ("if (r[0x%02x] %s ", OP_VAL_8(1)&0x0f,
                                             ifo_cmp[com.i_cmp]);
                IfoRegister (OP_VAL_16(1), com.i_dir_cmp);
                printf (") ");
            }
        
            /* Sub command */
            switch( com.i_sub_cmd )
            {
            case 1:
                printf( "goto Line 0x%02x", OP_VAL_16(2) );
                break;
        
            case 2:
                printf( "stop VM" );
                break;
        
            case 3:
                printf( "Set Parental Level To %s and goto Line 0x%02x",
                                     ifo_parental[OP_VAL_8(4)&0x7],
                                     OP_VAL_8(5) );
                break;
        
            default:
                printf( "Illegal" );
                break;
            }
        }
        break;

    /* Lnk */
    case 1:
        /* Main command */
        if( !pi_code[1] )
        {
            printf( "NOP\n" );
        }
        else
        {
            if( com.i_direct )
            {
                if( com.i_cmp )
                {
                    printf( "if (r[0x%02x] %s ", OP_VAL_8(4)&0x0f,
                                                 ifo_cmp[com.i_cmp] );
                    IfoRegister( OP_VAL_8(5), 0 );
                    printf( ") " );
                }

                /* Sub command */
                IfoJmp( com );
            }
            else
            {    
                if( com.i_cmp )
                {
                    printf( "if (r[0x%02x] %s ", OP_VAL_8(1)&0x0f,
                                                 ifo_cmp[com.i_cmp] );
                    IfoRegister( OP_VAL_16(1), com.i_dir_cmp );
                    printf( ") " );
                }

                /* Sub command */
                IfoLnk( com );
            }
        }
        break;

    /* SetSystem */
    case 2:
        if( !pi_code[1] )
        {
            IfoSetSystem( com );
        }
        else if( com.i_cmp && !com.i_sub_cmd )
        {
            printf ("if (r[0x%02x] %s ", OP_VAL_8(4)&0x0f, ifo_cmp[com.i_cmp]);
            IfoRegister( OP_VAL_8(5), 0 );
            printf (") ");
            IfoSetSystem( com );
        }
        else if( !com.i_cmp && com.i_sub_cmd )
        {
            printf( "if (" );
            IfoSetSystem( com );
            printf( ") " );
            IfoLnk( com );
        }
        else
        {
            printf("nop");
        }
        break;

    /* Set */
    case 3:
          if( ! pi_code[1] )
        {
            IfoSet( com );
        }
        else if( com.i_cmp && !com.i_sub_cmd )
        {
            printf ("if (r[0x%02x] %s ", OP_VAL_8(0)&0x0f, ifo_cmp[com.i_cmp]);
            IfoRegister( OP_VAL_16(2), com.i_dir_cmp );
            printf (") ");
            IfoSet( com );
        }
        else if( !com.i_cmp && com.i_sub_cmd )
        {
            printf ("if (");
            IfoSet( com );
            printf (") ");
            IfoLnk( com );
        }
        else
        {
            printf( "nop" );
        }
        break;

    /* 
     * math command on r[opcode[1]] and
     * direct?be2me_16(OP_VAL_8(0)):reg[OP_VAL_8(1)] is executed
     * ( unless command is swap; then r[opcode[1]] and r[OP_VAL_8(1)]
     * are swapped )
     * boolean operation cmp on r[opcode[1]] and
     * dir_cmp?be2me_16(OP_VAL_8(1)[1]):reg[OP_VAL_8(3)] is executed
     * on true result, buttons(c[6], c[7]) is called
     * problem is 'what is buttons()'
     */
    case 4:
        printf( "r[0x%X] ", pi_code[1] );
        printf( " %s ", IfoMath( com.i_cmd ) );
        if( com.i_cmd == 2 )
        {
            printf( "r[0x%X] ", OP_VAL_8(1) );
        }
        else
        {
            IfoRegister( OP_VAL_16(0), com.i_direct );
        }
        printf("; ");

        printf( "if ( r[%d] %s ", pi_code[1], ifo_cmp[com.i_cmp] );
        IfoRegister( OP_VAL_8(1), com.i_dir_cmp );
        printf( " )  then {" );
        IfoAdvanced( &OP_VAL_8(4) );
        printf( "}" );
        break;

    /*
     * opposite to case 4: boolean, math and buttons.
     */
    case 5:
    case 6:
        printf("if (");

        if( !com.i_direct && com.i_dir_cmp )
        {
            printf( "0x%X", OP_VAL_16(1) );
        }
        else
        {
            IfoRegister( OP_VAL_8(3), 0 );
            if( OP_VAL_8(3)&0x80 )
            {
                printf( "s[%s]", ifo_reg[OP_VAL_8(3)&0x1F] );
            }
            else
            {
                printf( "r[0x%X]", OP_VAL_8(3)&0x1F);
                    // 0x1F is either not a mistake,
                    // or Microsoft programmer's mistake!!!
            }
        }

        printf( " %s r[0x%X] ", ifo_cmp[com.i_cmp],
                                com.i_direct ? OP_VAL_8(2) : OP_VAL_8(1) );
           printf( " )  then {" );
        printf( "r[0x%X] ", pi_code[1] & 0xF );
        printf( " %s ", IfoMath( com.i_cmd ) );

        if( com.i_cmd == 0x02 )    // swap
        {
            printf("r[0x%X] ", OP_VAL_8(0)&0x1F);
        }
        else
        {
            if( com.i_direct )
            {
                printf( "0x%X", OP_VAL_16(0) );
            }
            else
            {
                if( OP_VAL_8(0) & 0x80 )
                {
                    printf("s[%s]", ifo_reg[OP_VAL_8(0) & 0x1F] );
                }
                else
                {
                    printf("r[0x%X]", OP_VAL_8(0) & 0x1F );
                }
            }
        }

        printf("; ");
        IfoAdvanced( &OP_VAL_8(4) );
        printf("}");

        break;

    default:
        printf( "Unknown Command\n" );
        break;
    }

    return;
}

/*****************************************************************************
 * CommandPrint : print in clear text (I hope so !) what a command does
 *****************************************************************************/
void CommandPrint( ifo_t ifo )
{
    return;
}

#endif
