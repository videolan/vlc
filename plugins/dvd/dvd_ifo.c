/*****************************************************************************
 * dvd_ifo.c: Functions for ifo parsing
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ifo.c,v 1.14 2001/02/22 08:44:45 stef Exp $
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
#include <stdio.h>
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
static vmg_t ReadVMG    ( ifo_t* );
void         CommandRead( ifo_command_t );

/*
 * IFO Management.
 */

/*****************************************************************************
 * IfoInit : Creates an ifo structure and prepares for parsing directly
 * on DVD device.
 *****************************************************************************/
ifo_t IfoInit( int i_fd )
{
    ifo_t       ifo;
    u32         i_lba;
    
    /* If we are here the dvd device has already been opened */
    ifo.i_fd = i_fd;

    i_lba = UDFFindFile( i_fd, "/VIDEO_TS/VIDEO_TS.IFO");

    ifo.i_off = (off_t)(i_lba) * DVD_LB_SIZE;
    ifo.i_pos = lseek( ifo.i_fd, ifo.i_off, SEEK_SET );

    /* Video Manager Initialization */
    intf_WarnMsg( 2, "ifo: initializing VMG" );
    ifo.vmg = ReadVMG( &ifo );

    return ifo;
}

/*****************************************************************************
 * IfoEnd : Frees all the memory allocated to ifo structures
 *****************************************************************************/
void IfoEnd( ifo_t* p_ifo )
{
#if 0
    int     i,j;

    /* Free structures from video title sets */
    for( j=0 ; j<p_ifo->vmg.mat.i_tts_nb ; j++ )
    {
        free( p_ifo->p_vts[j].vobu_admap.pi_vobu_ssector );
        free( p_ifo->p_vts[j].c_adt.p_cell_inf );
        free( p_ifo->p_vts[j].m_vobu_admap.pi_vobu_ssector );
        free( p_ifo->p_vts[j].m_c_adt.p_cell_inf );
        for( i=0 ; i<p_ifo->p_vts[j].tmap_ti.i_nb ; i++ )
        {
            free( p_ifo->p_vts[j].tmap_ti.p_tmap[i].pi_sector );
        }
        free( p_ifo->p_vts[j].tmap_ti.pi_sbyte );
        free( p_ifo->p_vts[j].tmap_ti.p_tmap );
        free( p_ifo->p_vts[j].pgci_ti.p_srp );
        for( i=0 ; i<p_ifo->p_vts[j].pgci_ut.i_lu_nb ; i++ )
        {
            free( p_ifo->p_vts[j].pgci_ut.p_pgci_inf[i].p_srp );
        }
        free( p_ifo->p_vts[j].pgci_ut.p_pgci_inf );
        free( p_ifo->p_vts[j].pgci_ut.p_lu );
    }

    free( p_ifo->p_vts );

    /* Free structures from video manager */
    free( p_ifo->vmg.vobu_admap.pi_vobu_ssector );
    free( p_ifo->vmg.c_adt.p_cell_inf );
    for( i=0 ; i<p_ifo->vmg.pgci_ut.i_lu_nb ; i++ )
    {
        free( p_ifo->vmg.pgci_ut.p_pgci_inf[i].p_srp );
    }
    free( p_ifo->vmg.pgci_ut.p_pgci_inf );
    free( p_ifo->vmg.pgci_ut.p_lu );
    for( i=1 ; i<=8 ; i++ )
    {
        free( p_ifo->vmg.ptl_mait.p_ptl_mask->ppi_ptl_mask[i] );
    }
    free( p_ifo->vmg.ptl_mait.p_ptl_desc );
    free( p_ifo->vmg.ptl_mait.p_ptl_mask );
    free( p_ifo->vmg.vts_atrt.pi_vts_atrt_sbyte );
    free( p_ifo->vmg.vts_atrt.p_vts_atrt );
    free( p_ifo->vmg.pgc.p_cell_pos_inf );
    free( p_ifo->vmg.pgc.p_cell_play_inf );
    free( p_ifo->vmg.pgc.prg_map.pi_entry_cell );
    free( p_ifo->vmg.pgc.com_tab.p_cell_com );
    free( p_ifo->vmg.pgc.com_tab.p_post_com );
    free( p_ifo->vmg.pgc.com_tab.p_pre_com );
#endif
    return;
}

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
 * Function common to Video Manager and Video Title set Processing
 */

/*****************************************************************************
 * ReadPGC : Fills the Program Chain structure.
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

static pgc_t ReadPGC( ifo_t* p_ifo )
{
    pgc_t   pgc;
    int     i;
    off_t   i_start = p_ifo->i_pos;

//fprintf( stderr, "PGC\n" );

    FLUSH(2);
    GETC( &pgc.i_prg_nb );
    GETC( &pgc.i_cell_nb );
//fprintf( stderr, "PGC: Prg: %d Cell: %d\n", pgc.i_prg_nb, pgc.i_cell_nb );
    GETL( &pgc.i_play_time );
    GETL( &pgc.i_prohibited_user_op );
    for( i=0 ; i<8 ; i++ )
    {
        GETS( &pgc.pi_audio_status[i] );
    }
    for( i=0 ; i<32 ; i++ )
    {
        GETL( &pgc.pi_subpic_status[i] );
    }
    GETS( &pgc.i_next_pgc_nb );
    GETS( &pgc.i_prev_pgc_nb );
    GETS( &pgc.i_goup_pgc_nb );
//fprintf( stderr, "PGC: Prev: %d Next: %d Up: %d\n",pgc.i_prev_pgc_nb ,pgc.i_next_pgc_nb, pgc.i_goup_pgc_nb );
    GETC( &pgc.i_still_time );
    GETC( &pgc.i_play_mode );
    for( i=0 ; i<16 ; i++ )
    {
        GETL( &pgc.pi_yuv_color[i] );
        /* FIXME : We have to erase the extra bit */
    }
    GETS( &pgc.i_com_tab_sbyte );
    GETS( &pgc.i_prg_map_sbyte );
    GETS( &pgc.i_cell_play_inf_sbyte );
    GETS( &pgc.i_cell_pos_inf_sbyte );

    /* Parsing of pgc_com_tab_t */
    if( pgc.i_com_tab_sbyte )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_start
                            + pgc.i_com_tab_sbyte, SEEK_SET );
        GETS( &pgc.com_tab.i_pre_com_nb );
        GETS( &pgc.com_tab.i_post_com_nb );
        GETS( &pgc.com_tab.i_cell_com_nb );
        FLUSH( 2 );
        if( pgc.com_tab.i_pre_com_nb )
        {
            pgc.com_tab.p_pre_com =
                      malloc(pgc.com_tab.i_pre_com_nb *sizeof(ifo_command_t));
            if( pgc.com_tab.p_pre_com == NULL )
            {
                intf_ErrMsg( "Out of memory" );
                p_ifo->b_error = 1;
                return pgc;
            }
            for( i=0 ; i<pgc.com_tab.i_pre_com_nb ; i++ )
            {
                GETCOMMAND( &pgc.com_tab.p_pre_com[i] );
            }
        }
        if( pgc.com_tab.i_post_com_nb )
        {
            pgc.com_tab.p_post_com =
                      malloc(pgc.com_tab.i_post_com_nb *sizeof(ifo_command_t));
            if( pgc.com_tab.p_post_com == NULL )
            {
                intf_ErrMsg( "Out of memory" );
                p_ifo->b_error = 1;
                return pgc;
            }
            for( i=0 ; i<pgc.com_tab.i_post_com_nb ; i++ )
            {
                GETCOMMAND( &pgc.com_tab.p_post_com[i] );
            }
        }
        if( pgc.com_tab.i_cell_com_nb )
        {
            pgc.com_tab.p_cell_com =
                      malloc(pgc.com_tab.i_cell_com_nb *sizeof(ifo_command_t));
            if( pgc.com_tab.p_cell_com == NULL )
            {
                intf_ErrMsg( "Out of memory" );
                p_ifo->b_error = 1;
                return pgc;
            }
            for( i=0 ; i<pgc.com_tab.i_cell_com_nb ; i++ )
            {
                GETCOMMAND( &pgc.com_tab.p_cell_com[i] );
            }
        }
    }
    /* Parsing of pgc_prg_map_t */
    if( pgc.i_prg_map_sbyte )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_start
                            + pgc.i_prg_map_sbyte, SEEK_SET );
        pgc.prg_map.pi_entry_cell = malloc( pgc.i_prg_nb *sizeof(u8) );
        if( pgc.prg_map.pi_entry_cell == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            p_ifo->b_error = 1;
            return pgc;
        }
        GET( pgc.prg_map.pi_entry_cell, pgc.i_prg_nb );
        /* FIXME : check endianness here */
    }
    /* Parsing of cell_play_inf_t */
    if( pgc.i_cell_play_inf_sbyte )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_start
                            + pgc.i_cell_play_inf_sbyte, SEEK_SET );
        pgc.p_cell_play_inf = malloc( pgc.i_cell_nb *sizeof(cell_play_inf_t) );
        if( pgc.p_cell_play_inf == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            p_ifo->b_error = 1;
            return pgc;
        }
        for( i=0 ; i<pgc.i_cell_nb ; i++ )
        {
            GETS( &pgc.p_cell_play_inf[i].i_cat );
            GETC( &pgc.p_cell_play_inf[i].i_still_time );
            GETC( &pgc.p_cell_play_inf[i].i_com_nb );
            GETL( &pgc.p_cell_play_inf[i].i_play_time );
            GETL( &pgc.p_cell_play_inf[i].i_entry_sector );
            GETL( &pgc.p_cell_play_inf[i].i_first_ilvu_vobu_esector );
            GETL( &pgc.p_cell_play_inf[i].i_lvobu_ssector );
            GETL( &pgc.p_cell_play_inf[i].i_lsector );
        }
    }
    /* Parsing of cell_pos_inf_map */
    if( pgc.i_cell_pos_inf_sbyte )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_start
                            + pgc.i_cell_pos_inf_sbyte, SEEK_SET );
        pgc.p_cell_pos_inf = malloc( pgc.i_cell_nb *sizeof(cell_pos_inf_t) );
        if( pgc.p_cell_pos_inf == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            p_ifo->b_error = 1;
            return pgc;
        }
        for( i=0 ; i<pgc.i_cell_nb ; i++ )
        {
            GETS( &pgc.p_cell_pos_inf[i].i_vob_id );
            FLUSH( 1 );
            GETC( &pgc.p_cell_pos_inf[i].i_cell_id );
        }
    } 

    return pgc;
}

/*****************************************************************************
 * ReadUnit : Fills Menu Language Unit Table/ PGC Info Table
 *****************************************************************************/
static pgci_inf_t ReadUnit( ifo_t* p_ifo )
{
    pgci_inf_t      inf;
    int             i;
    off_t           i_start = p_ifo->i_pos;

//fprintf( stderr, "Unit\n" );

    GETS( &inf.i_srp_nb );
    FLUSH( 2 );
    GETL( &inf.i_lu_ebyte );
    inf.p_srp = malloc( inf.i_srp_nb *sizeof(pgci_srp_t) );
    if( inf.p_srp == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return inf;
    }
    for( i=0 ; i<inf.i_srp_nb ; i++ )
    {
        GETC( &inf.p_srp[i].i_pgc_cat_mask );
        GETC( &inf.p_srp[i].i_pgc_cat );
        GETS( &inf.p_srp[i].i_par_mask );
        GETL( &inf.p_srp[i].i_pgci_sbyte );
    }
    for( i=0 ; i<inf.i_srp_nb ; i++ )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd,
                         i_start + inf.p_srp[i].i_pgci_sbyte,
                         SEEK_SET );
//fprintf( stderr, "Unit: PGC %d\n", i );
        inf.p_srp[i].pgc = ReadPGC( p_ifo );
    }

    return inf;
}

/*****************************************************************************
 * ReadUnitTable : Fills the PGCI Unit structure.
 *****************************************************************************/
static pgci_ut_t ReadUnitTable( ifo_t* p_ifo )
{
    pgci_ut_t       pgci;
    int             i;
    off_t           i_start = p_ifo->i_pos;

//fprintf( stderr, "Unit Table\n" );

    GETS( &pgci.i_lu_nb );
    FLUSH( 2 );
    GETL( &pgci.i_ebyte );
    pgci.p_lu = malloc( pgci.i_lu_nb *sizeof(pgci_lu_t) );
    if( pgci.p_lu == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return pgci;
    }
    for( i=0 ; i<pgci.i_lu_nb ; i++ )
    {
        GET( pgci.p_lu[i].ps_lang_code, 2 );
        FLUSH( 1 );
        GETC( &pgci.p_lu[i].i_existence_mask );
        GETL( &pgci.p_lu[i].i_lu_sbyte );
    }
    pgci.p_pgci_inf = malloc( pgci.i_lu_nb *sizeof(pgci_inf_t) );
    if( pgci.p_pgci_inf == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return pgci;
    }
    for( i=0 ; i<pgci.i_lu_nb ; i++ )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_start +
                                pgci.p_lu[i].i_lu_sbyte,
                                SEEK_SET );
        pgci.p_pgci_inf[i] = ReadUnit( p_ifo );
    }

    return pgci;
}

/*****************************************************************************
 * ReadCellInf : Fills the Cell Information structure.
 *****************************************************************************/
static c_adt_t ReadCellInf( ifo_t* p_ifo )
{
    c_adt_t         c_adt;
    off_t           i_start = p_ifo->i_pos;
    int             i;

//fprintf( stderr, "CELL ADD\n" );

    GETS( &c_adt.i_vob_nb );
    FLUSH( 2 );
    GETL( &c_adt.i_ebyte );
    c_adt.i_cell_nb =
        ( i_start + c_adt.i_ebyte + 1 - p_ifo->i_pos ) / sizeof(cell_inf_t);
    c_adt.p_cell_inf = malloc( c_adt.i_cell_nb *sizeof(cell_inf_t) );
    if( c_adt.p_cell_inf == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return c_adt;
    }
    for( i = 0 ; i < c_adt.i_cell_nb ; i++ )
    {
        GETS( &c_adt.p_cell_inf[i].i_vob_id );
        GETC( &c_adt.p_cell_inf[i].i_cell_id );
        FLUSH( 1 );
        GETL( &c_adt.p_cell_inf[i].i_ssector );
        GETL( &c_adt.p_cell_inf[i].i_esector );
    }
    
    return c_adt;
}

/*****************************************************************************
 * ReadMap : Fills the VOBU Map structure.
 *****************************************************************************/
static vobu_admap_t ReadMap( ifo_t* p_ifo )
{
    vobu_admap_t        map;
    int                 i, i_max;
    off_t               i_start = p_ifo->i_pos;
    
//fprintf( stderr, "VOBU ADMAP\n" );

    GETL( &map.i_ebyte );
    i_max = ( i_start + map.i_ebyte + 1 - p_ifo->i_pos ) / sizeof(u32);
    map.pi_vobu_ssector = malloc( i_max *sizeof(u32) );
    for( i=0 ; i<i_max ; i++ )
    {
        GETL( &map.pi_vobu_ssector[i] );
    }

    return map;
}
 
/*
 * Video Manager Information Processing.
 * This is what is contained in video_ts.ifo.
 */

/*****************************************************************************
 * ReadVMGInfMat : Fills the Management Information structure.
 *****************************************************************************/
static vmgi_mat_t ReadVMGInfMat( ifo_t* p_ifo )
{
    vmgi_mat_t  mat;
    u64         i_temp;
    int         i;
//    off_t     i_start = p_ifo->i_pos;

//fprintf( stderr, "VMGI\n" );

    GET( mat.psz_id , 12 );
    mat.psz_id[12] = '\0';
    GETL( &mat.i_lsector );
    FLUSH( 12 );
    GETL( &mat.i_i_lsector );
    FLUSH( 1 );
    GETC( &mat.i_spec_ver );
    GETL( &mat.i_cat );
    GETS( &mat.i_vol_nb );
    GETS( &mat.i_vol );
    GETC( &mat.i_disc_side );
    FLUSH( 19 );
    GETS( &mat.i_tts_nb );
    GET( mat.ps_provider_id, 32 );
    GETLL( &mat.i_pos_code );
    FLUSH( 24 );
    GETL( &mat.i_i_mat_ebyte );
    GETL( &mat.i_fp_pgc_sbyte );
    FLUSH( 56 );
    GETL( &mat.i_vobs_ssector );
    GETL( &mat.i_ptt_srpt_ssector );
    GETL( &mat.i_pgci_ut_ssector );
    GETL( &mat.i_ptl_mait_ssector );
    GETL( &mat.i_vts_atrt_ssector );
    GETL( &mat.i_txtdt_mg_ssector );
    GETL( &mat.i_c_adt_ssector );
    GETL( &mat.i_vobu_admap_ssector );
    FLUSH( 32 );
//    GETS( &mat.video_atrt );
FLUSH(2);
    FLUSH( 1 );
    GETC( &mat.i_audio_nb );
//fprintf( stderr, "vmgi audio nb : %d\n", mat.i_audio_nb );
    for( i=0 ; i < 8 ; i++ )
    {
        GETLL( &i_temp );
    }
    FLUSH( 17 );
    GETC( &mat.i_subpic_nb );
//fprintf( stderr, "vmgi subpic nb : %d\n", mat.i_subpic_nb );
    for( i=0 ; i < mat.i_subpic_nb ; i++ )
    {
        GET( &i_temp, 6 );
        /* FIXME : take care of endianness */
    }

    return mat;
}

/*****************************************************************************
 * ReadVMGTitlePointer : Fills the Part Of Title Search Pointer structure.
 *****************************************************************************/
static vmg_ptt_srpt_t ReadVMGTitlePointer( ifo_t* p_ifo )
{
    vmg_ptt_srpt_t  ptr;
    int             i;
//    off_t           i_start = p_ifo->i_pos;

//fprintf( stderr, "PTR\n" );

    GETS( &ptr.i_ttu_nb );
//fprintf( stderr, "PTR: TTU nb %d\n", ptr.i_ttu_nb );
    FLUSH( 2 );
    GETL( &ptr.i_ebyte );
    /* Parsing of tts */
    ptr.p_tts = malloc( ptr.i_ttu_nb *sizeof(tts_t) );
    if( ptr.p_tts == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return ptr;
    }
    for( i=0 ; i<ptr.i_ttu_nb ; i++ )
    {
        GETC( &ptr.p_tts[i].i_play_type );
        GETC( &ptr.p_tts[i].i_angle_nb );
        GETS( &ptr.p_tts[i].i_ptt_nb );
        GETS( &ptr.p_tts[i].i_parental_id );
        GETC( &ptr.p_tts[i].i_tts_nb );
        GETC( &ptr.p_tts[i].i_vts_ttn );
        GETL( &ptr.p_tts[i].i_ssector );
//fprintf( stderr, "PTR: %d %d %d\n", ptr.p_tts[i].i_ptt_nb, ptr.p_tts[i].i_tts_nb,ptr.p_tts[i].i_vts_ttn );
    }

    return ptr;
}

/*****************************************************************************
 * ReadParentalInf : Fills the Parental Management structure.
 *****************************************************************************/
static vmg_ptl_mait_t ReadParentalInf( ifo_t* p_ifo )
{
    vmg_ptl_mait_t  par;
    int             i, j, k;
    off_t           i_start = p_ifo->i_pos;

//fprintf( stderr, "PTL\n" );

    GETS( &par.i_country_nb );
    GETS( &par.i_vts_nb );
    GETL( &par.i_ebyte );
    par.p_ptl_desc = malloc( par.i_country_nb *sizeof(vmg_ptl_mai_desc_t) );
    if( par.p_ptl_desc == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return par;
    }
    for( i=0 ; i<par.i_country_nb ; i++ )
    {
        GET( par.p_ptl_desc[i].ps_country_code, 2 );
        FLUSH( 2 );
        GETS( &par.p_ptl_desc[i].i_ptl_mai_sbyte );
        FLUSH( 2 );
    }
    par.p_ptl_mask = malloc( par.i_country_nb *sizeof(vmg_ptl_mask_t) );
    if( par.p_ptl_mask == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return par;
    }
    for( i=0 ; i<par.i_country_nb ; i++ )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_start +
                         par.p_ptl_desc[i].i_ptl_mai_sbyte, SEEK_SET );
        for( j=1 ; j<=8 ; j++ )
        {
            par.p_ptl_mask[i].ppi_ptl_mask[j] =
                                    malloc( par.i_vts_nb *sizeof(u16) );
            if( par.p_ptl_mask[i].ppi_ptl_mask[j] == NULL )
            {
                intf_ErrMsg( "Out of memory" );
                p_ifo->b_error = 1;
                return par;
            }        
            for( k=0 ; k<par.i_vts_nb ; k++ )
            {
                GETS( &par.p_ptl_mask[i].ppi_ptl_mask[j][k] );
            }
        }
    }

    return par;
}

/*****************************************************************************
 * ReadVTSAttr : Fills the structure about VTS attributes.
 *****************************************************************************/
static vmg_vts_atrt_t ReadVTSAttr( ifo_t* p_ifo )
{
    vmg_vts_atrt_t  atrt;
    int             i, j;
    u64             i_temp;
    off_t           i_start = p_ifo->i_pos;

//fprintf( stderr, "VTS ATTR\n" );

    GETS( &atrt.i_vts_nb );
//fprintf( stderr, "VTS ATTR Nb: %d\n", atrt.i_vts_nb );
    FLUSH( 2 );
    GETL( &atrt.i_ebyte );
    atrt.pi_vts_atrt_sbyte = malloc( atrt.i_vts_nb *sizeof(u32) );
    if( atrt.pi_vts_atrt_sbyte == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return atrt;
    }
    for( i=0 ; i<atrt.i_vts_nb ; i++ )
    {
        GETL( &atrt.pi_vts_atrt_sbyte[i] );
    }
    atrt.p_vts_atrt = malloc( atrt.i_vts_nb *sizeof(vts_atrt_t) );
    if( atrt.p_vts_atrt == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return atrt;
    }
    for( i=0 ; i<atrt.i_vts_nb ; i++ )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_start +
                                atrt.pi_vts_atrt_sbyte[i],
                                SEEK_SET );
        GETL( &atrt.p_vts_atrt[i].i_ebyte );
        GETL( &atrt.p_vts_atrt[i].i_cat_app_type );
//        GETS( &atrt.p_vts_atrt[i].vtsm_video_atrt );
FLUSH(2);
        FLUSH( 1 );
        GETC( &atrt.p_vts_atrt[i].i_vtsm_audio_nb );
//fprintf( stderr, "m audio nb : %d\n", atrt.p_vts_atrt[i].i_vtsm_audio_nb );
        for( j=0 ; j<8 ; j++ )
        {
            GETLL( &i_temp );
        }
        FLUSH( 17 );
        GETC( &atrt.p_vts_atrt[i].i_vtsm_subpic_nb );
//fprintf( stderr, "m subp nb : %d\n", atrt.p_vts_atrt[i].i_vtsm_subpic_nb );
        for( j=0 ; j<28 ; j++ )
        {
            GET( &i_temp, 6 );
            /* FIXME : Fix endianness issue here */
        }
        FLUSH( 2 );
//        GETS( &atrt.p_vts_atrt[i].vtstt_video_atrt );
FLUSH(2);
        FLUSH( 1 );
        GETL( &atrt.p_vts_atrt[i].i_vtstt_audio_nb );
//fprintf( stderr, "tt audio nb : %d\n", atrt.p_vts_atrt[i].i_vtstt_audio_nb );
        for( j=0 ; j<8 ; j++ )
        {
            GETLL( &i_temp );
        }
        FLUSH( 17 );
        GETC( &atrt.p_vts_atrt[i].i_vtstt_subpic_nb );
//fprintf( stderr, "tt subp nb : %d\n", atrt.p_vts_atrt[i].i_vtstt_subpic_nb );
        for( j=0 ; j<28/*atrt.p_vts_atrt[i].i_vtstt_subpic_nb*/ ; j++ )
        {
            GET( &i_temp, 6 );
            /* FIXME : Fix endianness issue here */
        }
    }

    return atrt;
}
                           
/*****************************************************************************
 * ReadVMG : Parse video_ts.ifo file to fill the Video Manager structure.
 *****************************************************************************/
static vmg_t ReadVMG( ifo_t* p_ifo )
{
    vmg_t       vmg;

    p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off, SEEK_SET);
    vmg.mat = ReadVMGInfMat( p_ifo );
    p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off + 
                              vmg.mat.i_fp_pgc_sbyte, SEEK_SET );
    vmg.pgc = ReadPGC( p_ifo );
    if( vmg.mat.i_ptt_srpt_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vmg.mat.i_ptt_srpt_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vmg.ptt_srpt = ReadVMGTitlePointer( p_ifo );
    }
    if( vmg.mat.i_pgci_ut_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vmg.mat.i_pgci_ut_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vmg.pgci_ut = ReadUnitTable( p_ifo );
    }
    if( vmg.mat.i_ptl_mait_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vmg.mat.i_ptl_mait_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vmg.ptl_mait = ReadParentalInf( p_ifo );
    }
    if( vmg.mat.i_vts_atrt_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vmg.mat.i_vts_atrt_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vmg.vts_atrt = ReadVTSAttr( p_ifo );
    }
    if( vmg.mat.i_c_adt_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vmg.mat.i_c_adt_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vmg.c_adt = ReadCellInf( p_ifo );
    }
    if( vmg.mat.i_vobu_admap_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vmg.mat.i_vobu_admap_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vmg.vobu_admap = ReadMap( p_ifo );
    }
    return vmg;
}

/*
 * Video Title Set Information Processing.
 * This is what is contained in vts_*.ifo.
 */

/*****************************************************************************
 * ReadVTSInfMat : Fills the Title Set Information structure.
 *****************************************************************************/
static vtsi_mat_t ReadVTSInfMat( ifo_t* p_ifo )
{
    vtsi_mat_t  mat;
    int         i;
    u64         i_temp;
//    off_t       i_start = p_ifo->i_pos;

//fprintf( stderr, "VTSI\n" );

    GET( mat.psz_id , 12 );
    mat.psz_id[12] = '\0';
    GETL( &mat.i_lsector );
    FLUSH( 12 );
    GETL( &mat.i_i_lsector );
    FLUSH( 1 );
    GETC( &mat.i_spec_ver );
    GETL( &mat.i_cat );
    FLUSH( 90 );
    GETL( &mat.i_mat_ebyte );
    FLUSH( 60 );
    GETL( &mat.i_m_vobs_ssector );
    GETL( &mat.i_tt_vobs_ssector );
    GETL( &mat.i_ptt_srpt_ssector );
    GETL( &mat.i_pgcit_ssector );
    GETL( &mat.i_m_pgci_ut_ssector );
    GETL( &mat.i_tmap_ti_ssector );
    GETL( &mat.i_m_c_adt_ssector );
    GETL( &mat.i_m_vobu_admap_ssector );
    GETL( &mat.i_c_adt_ssector );
    GETL( &mat.i_vobu_admap_ssector );
    FLUSH( 24 );
//    GETS( &mat.m_video_atrt );
FLUSH(2);
    FLUSH( 1 );
    GETC( &mat.i_m_audio_nb );
    for( i=0 ; i<8 ; i++ )
    {
        GETLL( &i_temp );
    }
    FLUSH( 17 );
    GETC( &mat.i_m_subpic_nb );
    for( i=0 ; i<28 ; i++ )
    {
        GET( &i_temp, 6 );
        /* FIXME : take care of endianness */
    }
    FLUSH( 2 );
//    GETS( &mat.video_atrt );
FLUSH(2);
    FLUSH( 1 );
    GETC( &mat.i_audio_nb );
//fprintf( stderr, "vtsi audio nb : %d\n", mat.i_audio_nb );
    for( i=0 ; i<8 ; i++ )
    {
        GETLL( &i_temp );
//fprintf( stderr, "Audio %d: %llx\n", i, i_temp );
        i_temp >>= 32;
        mat.p_audio_atrt[i].i_lang_code = i_temp & 0xffff;
        i_temp >>= 16;
        mat.p_audio_atrt[i].i_num_channels = i_temp & 0x7;
        i_temp >>= 4;
        mat.p_audio_atrt[i].i_sample_freq = i_temp & 0x3;
        i_temp >>= 2;
        mat.p_audio_atrt[i].i_quantization = i_temp & 0x3;
        i_temp >>= 2;
        mat.p_audio_atrt[i].i_appl_mode = i_temp & 0x3;
        i_temp >>= 2;
        mat.p_audio_atrt[i].i_type = i_temp & 0x3;
        i_temp >>= 2;
        mat.p_audio_atrt[i].i_multichannel_extension = i_temp & 0x1;
        i_temp >>= 1;
        mat.p_audio_atrt[i].i_coding_mode = i_temp & 0x7;
    }
    FLUSH( 17 );
    GETC( &mat.i_subpic_nb );
//fprintf( stderr, "vtsi subpic nb : %d\n", mat.i_subpic_nb );
    for( i=0 ; i<mat.i_subpic_nb ; i++ )
    {
        GET( &i_temp, 6 );
        i_temp = hton64( i_temp ) >> 16;
//fprintf( stderr, "Subpic %d: %llx\n", i, i_temp );
        mat.p_subpic_atrt[i].i_caption = i_temp & 0xff;
        i_temp >>= 16;
        mat.p_subpic_atrt[i].i_lang_code = i_temp & 0xffff;
        i_temp >>= 16;
        mat.p_subpic_atrt[i].i_prefix = i_temp & 0xffff;
    }

    return mat;
}

/*****************************************************************************
 * ReadVTSTitlePointer : Fills the Part Of Title Search Pointer structure.
 *****************************************************************************/
static vts_ptt_srpt_t ReadVTSTitlePointer( ifo_t* p_ifo )
{
    vts_ptt_srpt_t  ptr;
    int             i;
    off_t           i_start = p_ifo->i_pos;

//fprintf( stderr, "VTS PTR\n" );

    GETS( &ptr.i_ttu_nb );
//fprintf( stderr, "VTS PTR nb: %d\n", ptr.i_ttu_nb );
    FLUSH( 2 );
    GETL( &ptr.i_ebyte );
    ptr.pi_ttu_sbyte = malloc( ptr.i_ttu_nb *sizeof(u32) );
    if( ptr.pi_ttu_sbyte == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return ptr;
    }
    for( i=0 ; i<ptr.i_ttu_nb ; i++ )
    {
        GETL( &ptr.pi_ttu_sbyte[i] );
    }
    /* Parsing of tts */
    ptr.p_ttu = malloc( ptr.i_ttu_nb *sizeof(ttu_t) );
    if( ptr.p_ttu == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return ptr;
    }
    for( i=0 ; i<ptr.i_ttu_nb ; i++ )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_start +
                        ptr.pi_ttu_sbyte[i], SEEK_SET );
        GETS( &ptr.p_ttu[i].i_pgc_nb );
        GETS( &ptr.p_ttu[i].i_prg_nb );
//fprintf( stderr, "VTS %d PTR Pgc: %d Prg: %d\n", i,ptr.p_ttu[i].i_pgc_nb, ptr.p_ttu[i].i_prg_nb );
    }

    return ptr;
}

/*****************************************************************************
 * ReadVTSTimeMap : Fills the time map table
 *****************************************************************************/
static vts_tmap_ti_t ReadVTSTimeMap( ifo_t* p_ifo )
{
    vts_tmap_ti_t   tmap;
    int             i,j;
//    off_t           i_start = p_ifo->i_pos;

//fprintf( stderr, "TMAP\n" );

    GETS( &tmap.i_nb );
    FLUSH( 2 );
    GETL( &tmap.i_ebyte );
    tmap.pi_sbyte = malloc( tmap.i_nb *sizeof(u32) );
    if( tmap.pi_sbyte == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return tmap;
    }
    for( i=0 ; i<tmap.i_nb ; i++ )
    {    
        GETL( &tmap.pi_sbyte[i] );
    }
    tmap.p_tmap = malloc( tmap.i_nb *sizeof(tmap_t) );
    if( tmap.p_tmap == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return tmap;
    }
    for( i=0 ; i<tmap.i_nb ; i++ )
    {    
        GETC( &tmap.p_tmap[i].i_time_unit );
        FLUSH( 1 );
        GETS( &tmap.p_tmap[i].i_entry_nb );
        tmap.p_tmap[i].pi_sector =
                    malloc( tmap.p_tmap[i].i_entry_nb *sizeof(u32) );
        if( tmap.p_tmap[i].pi_sector == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            p_ifo->b_error = 1;
            return tmap;
        }
        for( j=0 ; j<tmap.p_tmap[i].i_entry_nb ; j++ )
        {
            GETL( &tmap.p_tmap[i].pi_sector[j] );
        }
    }

    return tmap;
}
    

/*****************************************************************************
 * IfoReadVTS : Parse vts*.ifo files to fill the Video Title Set structure.
 *****************************************************************************/
int IfoReadVTS( ifo_t* p_ifo )
{
    vts_t       vts;
    off_t       i_off;
    int         i_title;

    intf_WarnMsg( 2, "ifo: initializing VTS %d", p_ifo->i_title );

    i_title = p_ifo->i_title;
    i_off = (off_t)( p_ifo->vmg.ptt_srpt.p_tts[i_title-1].i_ssector )
                   * DVD_LB_SIZE
                   + p_ifo->i_off;

    p_ifo->i_pos = lseek( p_ifo->i_fd, i_off, SEEK_SET );

    vts.i_pos = p_ifo->i_pos;

    vts.mat = ReadVTSInfMat( p_ifo );
    if( vts.mat.i_ptt_srpt_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, vts.i_pos +
                        vts.mat.i_ptt_srpt_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.ptt_srpt = ReadVTSTitlePointer( p_ifo );
    }
    if( vts.mat.i_m_pgci_ut_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, vts.i_pos +
                        vts.mat.i_m_pgci_ut_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.pgci_ut = ReadUnitTable( p_ifo );
    }
    if( vts.mat.i_pgcit_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, vts.i_pos +
                        vts.mat.i_pgcit_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.pgci_ti = ReadUnit( p_ifo );
    }
    if( vts.mat.i_tmap_ti_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, vts.i_pos +
                        vts.mat.i_tmap_ti_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.tmap_ti = ReadVTSTimeMap( p_ifo );
    }
    if( vts.mat.i_m_c_adt_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, vts.i_pos +
                        vts.mat.i_m_c_adt_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.m_c_adt = ReadCellInf( p_ifo );
    }
    if( vts.mat.i_m_vobu_admap_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, vts.i_pos +
                        vts.mat.i_m_vobu_admap_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.m_vobu_admap = ReadMap( p_ifo );
    }
    if( vts.mat.i_c_adt_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, vts.i_pos +
                        vts.mat.i_c_adt_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.c_adt = ReadCellInf( p_ifo );
    }
    if( vts.mat.i_vobu_admap_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, vts.i_pos +
                        vts.mat.i_vobu_admap_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.vobu_admap = ReadMap( p_ifo );
    }

    p_ifo->vts = vts;

    return 0;
}

/*
 * DVD Information Management
 */
#if 0
/*****************************************************************************
 * IfoRead : Function that fills structure and calls specified functions
 * to do it.
 *****************************************************************************/
void IfoRead( ifo_t* p_ifo )
{
    int     i;
    off_t   i_off;

    /* Video Title Sets initialization */
    p_ifo->p_vts = malloc( p_ifo->vmg.mat.i_tts_nb *sizeof(vts_t) );
    if( p_ifo->p_vts == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return;
    }

    for( i=0 ; i<p_ifo->vmg.mat.i_tts_nb ; i++ )
    {

        intf_WarnMsg( 2, "ifo: initializing VTS %d", i+1 );

        i_off = (off_t)( p_ifo->vmg.ptt_srpt.p_tts[i].i_ssector ) *DVD_LB_SIZE
                       + p_ifo->i_off;

        p_ifo->i_pos = lseek( p_ifo->i_fd, i_off, SEEK_SET );

        /* FIXME : I really don't know why udf find file
         * does not give the exact beginning of file */

        p_ifo->p_vts[i] = ReadVTS( p_ifo );

    }

    return; 
}
#endif
/*
 * IFO virtual machine : a set of commands that give the
 * interactive behaviour of the dvd
 */
#if 1

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
