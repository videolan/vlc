/*****************************************************************************
 * dvd_ifo.c: Functions for ifo parsing
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ifo.c,v 1.3 2001/02/08 17:44:12 massiot Exp $
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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "common.h"

#include "intf_msg.h"
#include "dvd_ifo.h"
#include "input_dvd.h"

/*
 * IFO Management.
 */

/*****************************************************************************
 * IfoFindVMG : When reading directly on a device, finds the offset to the
 * beginning of video_ts.ifo.
 *****************************************************************************/
static int IfoFindVMG( ifo_t* p_ifo )
{
    char    psz_ifo_start[12] = "DVDVIDEO-VMG";
    char    psz_test[12];

    read( p_ifo->i_fd, psz_test, 12 );

    while( strncmp( psz_test, psz_ifo_start, 12 ) != 0 )
    {
        /* The start of ifo file is on a sector boundary */
        p_ifo->i_pos = lseek( p_ifo->i_fd,
                              p_ifo->i_pos + DVD_LB_SIZE,
                              SEEK_SET );
        read( p_ifo->i_fd, psz_test, 12 );
    }
    p_ifo->i_off = p_ifo->i_pos;

//fprintf( stderr, "VMG Off : %lld\n", (long long)(p_ifo->i_off) );

    return 0;
}

/*****************************************************************************
 * IfoFindVTS : beginning of vts_*.ifo.
 *****************************************************************************/
static int IfoFindVTS( ifo_t* p_ifo )
{
    char    psz_ifo_start[12] = "DVDVIDEO-VTS";
    char    psz_test[12];

    read( p_ifo->i_fd, psz_test, 12 );

    while( strncmp( psz_test, psz_ifo_start, 12 ) != 0 )
    {
        /* The start of ifo file is on a sector boundary */
        p_ifo->i_pos = lseek( p_ifo->i_fd,
                              p_ifo->i_pos + DVD_LB_SIZE,
                              SEEK_SET );
        read( p_ifo->i_fd, psz_test, 12 );
    }
    p_ifo->i_off = p_ifo->i_pos;

//fprintf( stderr, "VTS Off : %lld\n", (long long)(p_ifo->i_off) );

    return 0;
}

/*****************************************************************************
 * IfoInit : Creates an ifo structure and prepares for parsing directly
 * on DVD device.
 *****************************************************************************/
ifo_t IfoInit( int i_fd )
{
    ifo_t       ifo;
    
    /* If we are here the dvd device has already been opened */
    ifo.i_fd = i_fd;
    /* No data at the beginning of the disk
     * 512000 bytes is just another value :) */
    ifo.i_pos = lseek( ifo.i_fd, 250 *DVD_LB_SIZE, SEEK_SET );
    /* FIXME : use udf filesystem to find the beginning of the file */
    IfoFindVMG( &ifo );
    
    return ifo;
}

/*****************************************************************************
 * IfoEnd : Frees all the memory allocated to ifo structures
 *****************************************************************************/
void IfoEnd( ifo_t* p_ifo )
{
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
                                (int)((p_com)->i_v0),                       \
                                (int)((p_com)->i_v2),                       \
                                (int)((p_com)->i_v4) );           */          \
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
        if( pgc.p_cell_play_inf == NULL )
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
    int             i, i_max;
    off_t           i_start = p_ifo->i_pos;

//fprintf( stderr, "CELL ADD\n" );

    GETS( &c_adt.i_vob_nb );
    FLUSH( 2 );
    GETL( &c_adt.i_ebyte );
    i_max = ( i_start + c_adt.i_ebyte + 1 - p_ifo->i_pos ) / sizeof(cell_inf_t);
    c_adt.p_cell_inf = malloc( i_max *sizeof(cell_inf_t) );
    if( c_adt.p_cell_inf == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return c_adt;
    }
    for( i=0 ; i<i_max ; i++ )
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
    GETS( &mat.i_video_atrt );
    FLUSH( 1 );
    GETC( &mat.i_audio_nb );
//fprintf( stderr, "vmgi audio nb : %d\n", mat.i_audio_nb );
    for( i=0 ; i < 8 ; i++ )
    {
        GETLL( &mat.pi_audio_atrt[i] );
    }
    FLUSH( 17 );
    GETC( &mat.i_subpic_nb );
//fprintf( stderr, "vmgi subpic nb : %d\n", mat.i_subpic_nb );
    for( i=0 ; i < mat.i_subpic_nb ; i++ )
    {
        GET( &mat.pi_subpic_atrt[i], 6 );
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
    off_t           i_start = p_ifo->i_pos;

//fprintf( stderr, "VTS ATTR\n" );

    GETS( &atrt.i_vts_nb );
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
        GETS( &atrt.p_vts_atrt[i].i_vtsm_video_atrt );
        FLUSH( 1 );
        GETC( &atrt.p_vts_atrt[i].i_vtsm_audio_nb );
//fprintf( stderr, "m audio nb : %d\n", atrt.p_vts_atrt[i].i_vtsm_audio_nb );
        for( j=0 ; j<8 ; j++ )
        {
            GETLL( &atrt.p_vts_atrt[i].pi_vtsm_audio_atrt[j] );
        }
        FLUSH( 17 );
        GETC( &atrt.p_vts_atrt[i].i_vtsm_subpic_nb );
//fprintf( stderr, "m subp nb : %d\n", atrt.p_vts_atrt[i].i_vtsm_subpic_nb );
        for( j=0 ; j<28 ; j++ )
        {
            GET( &atrt.p_vts_atrt[i].pi_vtsm_subpic_atrt[j], 6 );
            /* FIXME : Fix endianness issue here */
        }
        FLUSH( 2 );
        GETS( &atrt.p_vts_atrt[i].i_vtstt_video_atrt );
        FLUSH( 1 );
        GETL( &atrt.p_vts_atrt[i].i_vtstt_audio_nb );
//fprintf( stderr, "tt audio nb : %d\n", atrt.p_vts_atrt[i].i_vtstt_audio_nb );
        for( j=0 ; j<8 ; j++ )
        {
            GETLL( &atrt.p_vts_atrt[i].pi_vtstt_audio_atrt[j] );
        }
        FLUSH( 17 );
        GETC( &atrt.p_vts_atrt[i].i_vtstt_subpic_nb );
//fprintf( stderr, "tt subp nb : %d\n", atrt.p_vts_atrt[i].i_vtstt_subpic_nb );
        for( j=0 ; j<28/*atrt.p_vts_atrt[i].i_vtstt_subpic_nb*/ ; j++ )
        {
            GET( &atrt.p_vts_atrt[i].pi_vtstt_subpic_atrt[j], 6 );
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
    GETS( &mat.i_m_video_atrt );
    FLUSH( 1 );
    GETC( &mat.i_m_audio_nb );
    for( i=0 ; i<8 ; i++ )
    {
        GETLL( &mat.pi_m_audio_atrt[i] );
    }
    FLUSH( 17 );
    GETC( &mat.i_m_subpic_nb );
    for( i=0 ; i<28 ; i++ )
    {
        GET( &mat.pi_m_subpic_atrt[i], 6 );
        /* FIXME : take care of endianness */
    }
    FLUSH( 2 );
    GETS( &mat.i_video_atrt );
    FLUSH( 1 );
    GETC( &mat.i_audio_nb );
//fprintf( stderr, "vtsi audio nb : %d\n", mat.i_audio_nb );
    for( i=0 ; i<8 ; i++ )
    {
        GETLL( &mat.pi_audio_atrt[i] );
    }
    FLUSH( 17 );
    GETC( &mat.i_subpic_nb );
//fprintf( stderr, "vtsi subpic nb : %d\n", mat.i_subpic_nb );
    for( i=0 ; i<mat.i_subpic_nb ; i++ )
    {
        GET( &mat.pi_subpic_atrt[i], 6 );
        /* FIXME : take care of endianness */
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

//fprintf( stderr, "PTR\n" );

    GETS( &ptr.i_ttu_nb );
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
 * ReadVTS : Parse vts*.ifo files to fill the Video Title Set structure.
 *****************************************************************************/
static vts_t ReadVTS( ifo_t* p_ifo )
{
    vts_t       vts;

    vts.i_pos = p_ifo->i_pos;

    vts.mat = ReadVTSInfMat( p_ifo );
    if( vts.mat.i_ptt_srpt_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vts.mat.i_ptt_srpt_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.ptt_srpt = ReadVTSTitlePointer( p_ifo );
    }
    if( vts.mat.i_m_pgci_ut_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vts.mat.i_m_pgci_ut_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.pgci_ut = ReadUnitTable( p_ifo );
    }
    if( vts.mat.i_pgcit_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vts.mat.i_pgcit_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.pgci_ti = ReadUnit( p_ifo );
    }
    if( vts.mat.i_tmap_ti_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vts.mat.i_tmap_ti_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.tmap_ti = ReadVTSTimeMap( p_ifo );
    }
    if( vts.mat.i_m_c_adt_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vts.mat.i_m_c_adt_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.m_c_adt = ReadCellInf( p_ifo );
    }
    if( vts.mat.i_m_vobu_admap_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vts.mat.i_m_vobu_admap_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.m_vobu_admap = ReadMap( p_ifo );
    }
    if( vts.mat.i_c_adt_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vts.mat.i_c_adt_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.c_adt = ReadCellInf( p_ifo );
    }
    if( vts.mat.i_vobu_admap_ssector )
    {
        p_ifo->i_pos = lseek( p_ifo->i_fd, p_ifo->i_off +
                        vts.mat.i_vobu_admap_ssector *DVD_LB_SIZE,
                        SEEK_SET );
        vts.vobu_admap = ReadMap( p_ifo );
    }

    return vts;
}

/*
 * DVD Information Management
 */

/*****************************************************************************
 * IfoRead : Function that fills structure and calls specified functions
 * to do it.
 *****************************************************************************/
void IfoRead( ifo_t* p_ifo )
{
    int     i;
    off_t   i_off;

    p_ifo->vmg = ReadVMG( p_ifo );
    p_ifo->p_vts = malloc( p_ifo->vmg.mat.i_tts_nb *sizeof(vts_t) );
    if( p_ifo->p_vts == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_ifo->b_error = 1;
        return;
    }
    for( i=0 ; i<1/*p_ifo->vmg.mat.i_tts_nb*/ ; i++ )
    {

        intf_WarnMsg( 3, "######### VTS %d #############\n", i+1 );

        i_off = p_ifo->vmg.ptt_srpt.p_tts[i].i_ssector *DVD_LB_SIZE;
        p_ifo->i_pos = lseek( p_ifo->i_fd, i_off, SEEK_SET );
        /* FIXME : use udf filesystem to avoid this */
        IfoFindVTS( p_ifo );
        p_ifo->p_vts[i] = ReadVTS( p_ifo );
    }
    return; 
}

/*
 * IFO virtual machine : a set of commands that give the behaviour of the dvd
 */
#if 0
/*****************************************************************************
 * CommandRead : translates the command strings in ifo into command
 * structures.
 *****************************************************************************/
void CommandRead( ifo_command_t com )
{
    u8*     pi_code = (u8*)(&com);

    switch( com.i_type )
    {
        case 0:                                     /* Goto */
            if( !pi_code[1] )
            {
                fprintf( stderr, "NOP\n" );
            }
            else if( cmd.i_cmp )
            {
                
            }
            break;
        case 1:                                     /* Lnk */
            break;
        case 2:                                     /* SetSystem */
            break;
        case 3:                                     /* Set */
            break;
        case 4:                                     /* */
            break;
        case 5:                                     /* */
            break;
        case 6:                                     /* */
            break;
        default:
            fprintf( stderr, "Unknown Command\n" );
            break;
    }

    return;
}

/*****************************************************************************
 * IfoGoto
 *****************************************************************************/
static void IfoGoto( ifo_command_t cmd )
{
    

    return;
}

/*****************************************************************************
 * IfoLnk
 *****************************************************************************/
static void IfoLnk( ifo_t* p_ifo )
{
    return;
}

/*****************************************************************************
 * IfoJmp
 *****************************************************************************/
static void IfoJmp( ifo_t* p_ifo )
{
    return;
}
#endif
