/*****************************************************************************
 * dvd_ifo.c: Functions for ifo parsing
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ifo.c,v 1.36 2001/08/06 13:28:00 sam Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          German Tischler <tanis@gaspode.franken.de>
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

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#include <string.h>
#include <fcntl.h>

#ifdef GOD_DAMN_DMCA
#   include "dummy_dvdcss.h"
#else
#   include <videolan/dvdcss.h>
#endif

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "input_dvd.h"
#include "dvd_ifo.h"
#include "dvd_udf.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
void            CommandRead     ( command_desc_t );
static int      ReadTitle       ( ifo_t * , title_t *, int, int );
static int      FreeTitle       ( title_t * );
static int      ReadUnitInf     ( ifo_t * , unit_inf_t *, int, int );
static int      FreeUnitInf     ( unit_inf_t * );
static int      ReadTitleUnit   ( ifo_t * , title_unit_t *, int );
static int      FreeTitleUnit   ( title_unit_t * );
static int      ReadVobuMap     ( ifo_t * , vobu_map_t *, int );
static int      FreeVobuMap     ( vobu_map_t * );
static int      ReadCellInf     ( ifo_t * , cell_inf_t *, int );
static int      FreeCellInf     ( cell_inf_t * );
static int      FreeTitleSet    ( vts_t * );

static u8*      FillBuffer      ( ifo_t *, u8 *, int );
static u8       ReadByte        ( ifo_t *, u8 *, u8 ** );
static void     ReadBytes       ( ifo_t *, u8 *, u8 **, u8 *, int );
static void     DumpBytes       ( ifo_t *, u8 *, u8 **, int );
static u16      ReadWord        ( ifo_t *, u8 *, u8 ** );
static u32      ReadDouble      ( ifo_t *, u8 *, u8 ** );
static u64      ReadQuad        ( ifo_t *, u8 *, u8 ** );

/*
 * IFO Management.
 */

/*****************************************************************************
 * IfoCreate : Creates an ifo structure and prepares for parsing directly
 *             on DVD device
 *****************************************************************************/
int IfoCreate( thread_dvd_data_t * p_dvd )
{
    p_dvd->p_ifo = malloc( sizeof(ifo_t) );
    if( p_dvd->p_ifo == NULL )
    {
        intf_ErrMsg( "ifo error: unable to allocate memory. aborting" );
        return -1;
    }

    /* if we are here the dvd device has already been opened */
    p_dvd->p_ifo->dvdhandle = p_dvd->dvdhandle;

    return 0;
}

/*****************************************************************************
 * IfoInit : Reads information from the management table.
 *****************************************************************************/
int IfoInit( ifo_t * p_ifo )
{
    u8                  p_buf[DVD_LB_SIZE];
    u8*                 p_tmp;
    u64                 i_temp;
    int                 i, j, k;
    int                 i_start;

    /* find the start sector of video information on the dvd */
    p_ifo->i_start = UDFFindFile( p_ifo->dvdhandle, "/VIDEO_TS/VIDEO_TS.IFO" );
    if( !p_ifo->i_start ) return -1;

    p_tmp = FillBuffer( p_ifo, p_buf, p_ifo->i_start );
    //i_start = p_ifo->i_pos;

    /*
     * read the video manager information table
     */
#define MGINF     p_ifo->vmg.manager_inf
    //fprintf( stderr, "VMGI\n" );

    ReadBytes( p_ifo, p_buf, &p_tmp, MGINF.psz_id, 12 );
    MGINF.psz_id[12] = '\0';
    MGINF.i_vmg_end_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 12 );
    MGINF.i_vmg_inf_end_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
    MGINF.i_spec_ver = ReadByte( p_ifo, p_buf, &p_tmp );
    MGINF.i_cat = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_volume_nb = ReadWord( p_ifo, p_buf, &p_tmp );
    MGINF.i_volume = ReadWord( p_ifo, p_buf, &p_tmp );
    MGINF.i_disc_side = ReadByte( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 19 );
    MGINF.i_title_set_nb = ReadWord( p_ifo, p_buf, &p_tmp );
    ReadBytes( p_ifo, p_buf, &p_tmp, MGINF.ps_provider_id, 32 );
    MGINF.i_pos_code = ReadQuad( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 24 );
    MGINF.i_vmg_inf_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_first_play_title_start_byte = ReadDouble( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 56 );
    MGINF.i_vob_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_title_inf_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_title_unit_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_parental_inf_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_vts_inf_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_text_data_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_cell_inf_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_vobu_map_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 32 );
    DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
    DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
    MGINF.i_audio_nb = ReadByte( p_ifo, p_buf, &p_tmp );
    //fprintf( stderr, "vmgi audio nb : %d\n", MGINF.i_audio_nb );

    for( i = 0 ; i < 8 ; i++ )
    {
        i_temp = ReadQuad( p_ifo, p_buf, &p_tmp );
    }

    DumpBytes( p_ifo, p_buf, &p_tmp, 17 );
    MGINF.i_spu_nb = ReadByte( p_ifo, p_buf, &p_tmp );
    //fprintf( stderr, "vmgi subpic nb : %d\n", MGINF.i_spu_nb );

    for( i = 0 ; i < MGINF.i_spu_nb ; i++ )
    {
        ReadBytes( p_ifo, p_buf, &p_tmp, (u8*)(&i_temp), 6 );
        /* FIXME : take care of endianness */
    }

    /*
     * read first play title.
     */
    //fprintf(stderr,"readtitle %i\n", MGINF.i_first_play_title_start_byte & 0x7ff );
    if( ReadTitle( p_ifo, &p_ifo->vmg.title, p_ifo->i_start
                           + OFF2LB( MGINF.i_first_play_title_start_byte ),
                   MGINF.i_first_play_title_start_byte & 0x7ff ) < 0 )
    {
        return -1;
    }

    /*
     * fills the title information structure.
     */
#define TITINF       p_ifo->vmg.title_inf
    if( MGINF.i_title_inf_start_sector )
    {
        p_tmp = FillBuffer( p_ifo, p_buf,
                        p_ifo->i_start + MGINF.i_title_inf_start_sector );
        //fprintf( stderr, "title inf %d\n", p_ifo->i_pos );
    
        TITINF.i_title_nb = ReadWord( p_ifo, p_buf, &p_tmp );
        //fprintf( stderr, "title_inf: TTU nb %d\n", TITINF.i_title_nb );
        DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
        TITINF.i_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );
    
        /* parsing of title attributes */
        TITINF.p_attr = malloc( TITINF.i_title_nb *sizeof(title_attr_t) );
        if( TITINF.p_attr == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoInit" );
            return -1;
        }
    
        for( i = 0 ; i < TITINF.i_title_nb ; i++ )
        {
            TITINF.p_attr[i].i_play_type = ReadByte( p_ifo, p_buf, &p_tmp );
            TITINF.p_attr[i].i_angle_nb = ReadByte( p_ifo, p_buf, &p_tmp );
            TITINF.p_attr[i].i_chapter_nb = ReadWord( p_ifo, p_buf, &p_tmp );
            TITINF.p_attr[i].i_parental_id = ReadWord( p_ifo, p_buf, &p_tmp );
            TITINF.p_attr[i].i_title_set_num = ReadByte( p_ifo, p_buf, &p_tmp );
            TITINF.p_attr[i].i_title_num = ReadByte( p_ifo, p_buf, &p_tmp );
            TITINF.p_attr[i].i_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
            //fprintf( stderr, "title_inf: %d %d %d\n", TITINF.p_attr[i].i_chapter_nb, TITINF.p_attr[i].i_title_set_num, TITINF.p_attr[i].i_title_num );
        }
    }
    else
    {
        TITINF.p_attr = NULL;
    }
#undef TITINF

    /*
     * fills the title unit structure.
     */
    if( MGINF.i_title_unit_start_sector )
    {
        if( ReadTitleUnit( p_ifo, &p_ifo->vmg.title_unit, p_ifo->i_start
                            + MGINF.i_title_unit_start_sector ) < 0 )
        {
            return -1;
        }
    }

    /*
     * fills the structure about parental information.
     */
#define PARINF        p_ifo->vmg.parental_inf
    if( MGINF.i_parental_inf_start_sector )
    {
        p_tmp = FillBuffer( p_ifo, p_buf, p_ifo->i_start +
                                MGINF.i_parental_inf_start_sector );
        i_start = p_ifo->i_pos;

        //fprintf( stderr, "PTL\n" );
    
        PARINF.i_country_nb = ReadWord( p_ifo, p_buf, &p_tmp );
        PARINF.i_vts_nb = ReadWord( p_ifo, p_buf, &p_tmp );
        PARINF.i_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );
        
        PARINF.p_parental_desc = malloc( PARINF.i_country_nb
                                          * sizeof(parental_desc_t) );
        if( PARINF.p_parental_desc == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoInit" );
            return -1;
        }

        for( i = 0 ; i < PARINF.i_country_nb ; i++ )
        {
            ReadBytes( p_ifo, p_buf, &p_tmp,
                       PARINF.p_parental_desc[i].ps_country_code, 2 );
            DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
            PARINF.p_parental_desc[i].i_parental_mask_start_byte =
                                            ReadWord( p_ifo, p_buf, &p_tmp );
            DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
        }

        PARINF.p_parental_mask = malloc( PARINF.i_country_nb
                                          * sizeof(parental_mask_t) );
        if( PARINF.p_parental_mask == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoInit" );
            return -1;
        }

        for( i = 0 ; i < PARINF.i_country_nb ; i++ )
        {
            p_tmp = FillBuffer( p_ifo, p_buf, i_start + OFF2LB(
                      PARINF.p_parental_desc[i].i_parental_mask_start_byte ) )
              + (PARINF.p_parental_desc[i].i_parental_mask_start_byte & 0x7ff);

            for( j = 0 ; j < 8 ; j++ )
            {
                PARINF.p_parental_mask[i].ppi_mask[j] =
                            malloc( ( PARINF.i_vts_nb + 1 ) * sizeof(u16) );

                if( PARINF.p_parental_mask[i].ppi_mask[j] == NULL )
                {
                    intf_ErrMsg( "ifo error: out of memory in IfoInit" );
                    return -1;
                }

                for( k = 0 ; k < PARINF.i_vts_nb + 1 ; k++ )
                {
                    PARINF.p_parental_mask[i].ppi_mask[j][k] =
                                            ReadWord( p_ifo, p_buf, &p_tmp );
                }
            }
        }
    }
#undef PARINF

    /*
     * information and attributes about for each vts.
     */
#define VTSINF     p_ifo->vmg.vts_inf
    if( MGINF.i_vts_inf_start_sector )
    {
        u64             i_temp;

        p_tmp = FillBuffer( p_ifo, p_buf, p_ifo->i_start +
                                MGINF.i_vts_inf_start_sector );
        i_start = p_ifo->i_pos;
    
        //fprintf( stderr, "VTS ATTR\n" );
    
        VTSINF.i_vts_nb = ReadWord( p_ifo, p_buf, &p_tmp );;
        //fprintf( stderr, "VTS ATTR Nb: %d\n", VTSINF.i_vts_nb );
        DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
        VTSINF.i_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );
        VTSINF.pi_vts_attr_start_byte =
                            malloc( VTSINF.i_vts_nb * sizeof(u32) );
        if( VTSINF.pi_vts_attr_start_byte == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoInit" );
            return -1;
        }

        for( i = 0 ; i < VTSINF.i_vts_nb ; i++ )
        {
            VTSINF.pi_vts_attr_start_byte[i] =
                                      ReadDouble( p_ifo, p_buf, &p_tmp );
        }

        VTSINF.p_vts_attr = malloc( VTSINF.i_vts_nb * sizeof(vts_attr_t) );
        if( VTSINF.p_vts_attr == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoInit" );
            return -1;
        }

        for( i = 0 ; i < VTSINF.i_vts_nb ; i++ )
        {
            p_tmp = FillBuffer( p_ifo, p_buf, i_start +
                                OFF2LB( VTSINF.pi_vts_attr_start_byte[i] ) )
                     + ( VTSINF.pi_vts_attr_start_byte[i] & 0x7ff );

            VTSINF.p_vts_attr[i].i_end_byte =
                                ReadDouble( p_ifo, p_buf, &p_tmp );
            VTSINF.p_vts_attr[i].i_cat_app_type =
                                ReadDouble( p_ifo, p_buf, &p_tmp );
            DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
            DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
            VTSINF.p_vts_attr[i].i_vts_menu_audio_nb =
                                ReadByte( p_ifo, p_buf, &p_tmp );
            //fprintf( stderr, "m audio nb : %d\n", VTSINF.p_vts_attr[i].i_vts_menu_audio_nb );

            for( j = 0 ; j < 8 ; j++ )
            {
                i_temp = ReadQuad( p_ifo, p_buf, &p_tmp );
            }

            DumpBytes( p_ifo, p_buf, &p_tmp, 17 );
            VTSINF.p_vts_attr[i].i_vts_menu_spu_nb =
                                ReadByte( p_ifo, p_buf, &p_tmp );
            //fprintf( stderr, "m subp nb : %d\n", VTSINF.p_vts_attr[i].i_vts_menu_spu_nb );

            for( j = 0 ; j < 28 ; j++ )
            {
                /* FIXME : Fix endianness issue here */
                ReadBytes( p_ifo, p_buf, &p_tmp, (u8*)(&i_temp), 6 );
            }

            DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
            DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
            DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
            VTSINF.p_vts_attr[i].i_vts_title_audio_nb =
                                ReadDouble( p_ifo, p_buf, &p_tmp );
            //fprintf( stderr, "tt audio nb : %d\n", VTSINF.p_vts_attr[i].i_vts_title_audio_nb );

            for( j = 0 ; j < 8 ; j++ )
            {
                i_temp = ReadQuad( p_ifo, p_buf, &p_tmp );;
            }

            DumpBytes( p_ifo, p_buf, &p_tmp, 17 );
            VTSINF.p_vts_attr[i].i_vts_title_spu_nb =
                                ReadByte( p_ifo, p_buf, &p_tmp );
            //fprintf( stderr, "tt subp nb : %d\n", VTSINF.p_vts_attr[i].i_vts_title_spu_nb );

            for( j = 0 ; j < 28 /*VTSINF.p_vts_vts_inf[i].i_vtstt_subpic_nb*/ ; j++ )
            {
                /* FIXME : Fix endianness issue here */
                ReadBytes( p_ifo, p_buf, &p_tmp, (u8*)(&i_temp), 6 );
            }
        }
    }
#undef VTSINF

    /*
     * global cell map.
     */
    if( MGINF.i_cell_inf_start_sector )
    {
        if( ReadCellInf( p_ifo, &p_ifo->vmg.cell_inf, p_ifo->i_start +
                         MGINF.i_cell_inf_start_sector ) < 0 )
        {
            return -1;
        }
    }

    /*
     * global vob unit map.
     */
    if( MGINF.i_vobu_map_start_sector )
    {
        if( ReadVobuMap( p_ifo, &p_ifo->vmg.vobu_map, p_ifo->i_start +
                         MGINF.i_vobu_map_start_sector ) < 0 )
        {
            return -1;
        }
    }
#undef MGINF

    p_ifo->vts.b_initialized = 0;

    intf_WarnMsg( 2, "ifo info: vmg initialized" );

    return 0;
}

/*****************************************************************************
 * IfoTitleSet: Parse vts*.ifo files to fill the Video Title Set structure.
 *****************************************************************************/
int IfoTitleSet( ifo_t * p_ifo )
{
    u8          p_buf[DVD_LB_SIZE];
    u8 *        p_tmp;
    int         i_off;
    int         i_start;
    u64         i_temp;
    u16         i_short;
    int         i, j;

    if( p_ifo->vts.b_initialized )
    {
        FreeTitleSet( &p_ifo->vts );
    }

    i_off = p_ifo->vmg.title_inf.p_attr[p_ifo->i_title-1].i_start_sector
                   + p_ifo->i_start;

    //fprintf(stderr, "offset: %d\n" , i_off );

    p_tmp = FillBuffer( p_ifo, p_buf, i_off );
    //i_start = p_ifo->i_pos;
    p_ifo->vts.i_pos = p_ifo->i_pos;

#define MGINF p_ifo->vts.manager_inf

    /*
     * read manager information
     */
    //fprintf( stderr, "VTSI\n" );

    ReadBytes( p_ifo, p_buf, &p_tmp, MGINF.psz_id , 12 );
    MGINF.psz_id[12] = '\0';
    MGINF.i_end_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 12 );
    MGINF.i_inf_end_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
    MGINF.i_spec_ver = ReadByte( p_ifo, p_buf, &p_tmp );
    MGINF.i_cat = ReadDouble( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 90 );
    MGINF.i_inf_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 60 );
    MGINF.i_menu_vob_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_title_vob_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_title_inf_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_title_unit_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_menu_unit_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_time_inf_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_menu_cell_inf_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_menu_vobu_map_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_cell_inf_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    MGINF.i_vobu_map_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 24 );
    DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
    DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
    MGINF.i_menu_audio_nb = ReadByte( p_ifo, p_buf, &p_tmp );

    for( i = 0 ; i < 8 ; i++ )
    {
        i_temp = ReadQuad( p_ifo, p_buf, &p_tmp );
    }

    DumpBytes( p_ifo, p_buf, &p_tmp, 17 );
    MGINF.i_menu_spu_nb = ReadByte( p_ifo, p_buf, &p_tmp );

    for( i = 0 ; i < 28 ; i++ )
    {
        /* FIXME : take care of endianness */
        ReadBytes( p_ifo, p_buf, &p_tmp, (u8*)(&i_temp), 6 );
    }

    DumpBytes( p_ifo, p_buf, &p_tmp, 2 );

    i_short = ReadWord( p_ifo, p_buf, &p_tmp );
    i_short >>= 2;
    MGINF.video_attr.i_mode = i_short & 0x1;
    i_short >>= 1;
    MGINF.video_attr.i_letterboxed = i_short & 0x1;
    i_short >>= 1;
    MGINF.video_attr.i_source_res = i_short & 0x3;
    i_short >>= 2;
    MGINF.video_attr.i_line21_2 = i_short & 0x1;
    i_short >>= 1;
    MGINF.video_attr.i_line21_1 = i_short & 0x1;
    i_short >>= 1;
    MGINF.video_attr.i_perm_displ = i_short & 0x3;
    i_short >>= 2;
    MGINF.video_attr.i_ratio = i_short & 0x3;
    i_short >>= 2;
    MGINF.video_attr.i_system = i_short & 0x3;
    i_short >>= 2;
    MGINF.video_attr.i_compression = i_short & 0x3;

    DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
    MGINF.i_audio_nb = ReadByte( p_ifo, p_buf, &p_tmp );
    //fprintf( stderr, "vtsi audio nb : %d\n", MGINF.i_audio_nb );

    for( i = 0 ; i < 8 ; i++ )
    {
        i_temp = ReadQuad( p_ifo, p_buf, &p_tmp );
        //fprintf( stderr, "Audio %d: %llx\n", i, i_temp );
        i_temp >>= 8;
        MGINF.p_audio_attr[i].i_bar = i_temp & 0xff;
        i_temp >>= 8;
        MGINF.p_audio_attr[i].i_caption = i_temp & 0xff;
        i_temp >>= 8;
        MGINF.p_audio_attr[i].i_foo = i_temp & 0xff;
        i_temp >>= 8;
        MGINF.p_audio_attr[i].i_lang_code = i_temp & 0xffff;
        i_temp >>= 16;
        MGINF.p_audio_attr[i].i_num_channels = i_temp & 0x7;
        i_temp >>= 3;
        MGINF.p_audio_attr[i].i_test = i_temp & 0x1;
        i_temp >>= 1;
        MGINF.p_audio_attr[i].i_sample_freq = i_temp & 0x3;
        i_temp >>= 2;
        MGINF.p_audio_attr[i].i_quantization = i_temp & 0x3;
        i_temp >>= 2;
        MGINF.p_audio_attr[i].i_appl_mode = i_temp & 0x3;
        i_temp >>= 2;
        MGINF.p_audio_attr[i].i_type = i_temp & 0x3;
        i_temp >>= 2;
        MGINF.p_audio_attr[i].i_multichannel_extension = i_temp & 0x1;
        i_temp >>= 1;
        MGINF.p_audio_attr[i].i_coding_mode = i_temp & 0x7;
    }

    DumpBytes( p_ifo, p_buf, &p_tmp, 17 );
    MGINF.i_spu_nb = ReadByte( p_ifo, p_buf, &p_tmp );
    //fprintf( stderr, "vtsi subpic nb : %d\n", MGINF.i_spu_nb );

    for( i=0 ; i<MGINF.i_spu_nb ; i++ )
    {
        ReadBytes( p_ifo, p_buf, &p_tmp, (u8*)(&i_temp), 6 );
        i_temp = hton64( i_temp ) >> 16;
        //fprintf( stderr, "Subpic %d: %llx\n", i, i_temp );
        MGINF.p_spu_attr[i].i_caption = i_temp & 0xff;
        i_temp >>= 8;
        MGINF.p_spu_attr[i].i_foo = i_temp & 0xff;
        i_temp >>= 8;
        MGINF.p_spu_attr[i].i_lang_code = i_temp & 0xffff;
        i_temp >>= 16;
        MGINF.p_spu_attr[i].i_prefix = i_temp & 0xffff;
    }

    /*
     * read title information: set of pointers to title
     */
#define TITINF p_ifo->vts.title_inf
    if( MGINF.i_title_inf_start_sector )
    {
        p_tmp = FillBuffer( p_ifo, p_buf, p_ifo->vts.i_pos +
                                          MGINF.i_title_inf_start_sector );

        i_start = p_ifo->i_pos;
    
        //fprintf( stderr, "VTS PTR\n" );
   
        TITINF.i_title_nb = ReadWord( p_ifo, p_buf, &p_tmp );
        //fprintf( stderr, "VTS title_inf nb: %d\n", TITINF.i_title_nb );
        DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
        TITINF.i_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );

        TITINF.pi_start_byte = malloc( TITINF.i_title_nb * sizeof(u32) );
        if( TITINF.pi_start_byte == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoTitleSet" );
            return -1;
        }

        for( i = 0 ; i < TITINF.i_title_nb ; i++ )
        {
            TITINF.pi_start_byte[i] = ReadDouble( p_ifo, p_buf, &p_tmp );
        }

        /* Parsing of tts */
        TITINF.p_title_start = malloc( TITINF.i_title_nb
                                         * sizeof(title_start_t) );
        if( TITINF.p_title_start == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoTitleSet" );
            return -1;
        }

        for( i = 0 ; i < TITINF.i_title_nb ; i++ )
        {
            p_tmp = FillBuffer( p_ifo, p_buf, i_start +
                                OFF2LB( TITINF.pi_start_byte[i] ) )
                     + (TITINF.pi_start_byte[i] & 0x7ff);

            TITINF.p_title_start[i].i_title_id =
                                   ReadWord( p_ifo, p_buf, &p_tmp );
            TITINF.p_title_start[i].i_chapter =
                                   ReadWord( p_ifo, p_buf, &p_tmp );
        }
    }
#undef TITINF

    /*
     * menu unit information
     */
    if( MGINF.i_menu_unit_start_sector )
    {
        if( ReadTitleUnit( p_ifo, &p_ifo->vts.menu_unit, p_ifo->vts.i_pos +
                     MGINF.i_menu_unit_start_sector ) < 0 )
        {
            return -1;
        }
    }

    /*
     * title unit information
     */
    if( MGINF.i_title_unit_start_sector )
    {
        if( ReadUnitInf( p_ifo, &p_ifo->vts.title_unit, p_ifo->vts.i_pos +
                         MGINF.i_title_unit_start_sector, 0  ) < 0 )
        {
            return -1;
        }
    }

    /*
     * time map information
     */
#define TIMINF p_ifo->vts.time_inf
    if( MGINF.i_time_inf_start_sector )
    {
        u8      p_buf[DVD_LB_SIZE];

        p_tmp = FillBuffer( p_ifo, p_buf, p_ifo->vts.i_pos +
                                          MGINF.i_time_inf_start_sector );

        //fprintf( stderr, "TMAP\n" );

        TIMINF.i_nb = ReadWord( p_ifo, p_buf, &p_tmp );;
        DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
        TIMINF.i_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );

        TIMINF.pi_start_byte = malloc( TIMINF.i_nb * sizeof(u32) );
        if( TIMINF.pi_start_byte == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoTitleSet" );
            return -1;
        }

        for( i = 0 ; i < TIMINF.i_nb ; i++ )
        {    
            TIMINF.pi_start_byte[i] = ReadDouble( p_ifo, p_buf, &p_tmp );
        }

        TIMINF.p_time_map = malloc( TIMINF.i_nb * sizeof(time_map_t) );
        if( TIMINF.p_time_map == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in IfoTitleSet" );
            return -1;
        }

        for( i = 0 ; i < TIMINF.i_nb ; i++ )
        {    
            TIMINF.p_time_map[i].i_time_unit = ReadByte( p_ifo, p_buf, &p_tmp );
            DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
            TIMINF.p_time_map[i].i_entry_nb = ReadWord( p_ifo, p_buf, &p_tmp );

            TIMINF.p_time_map[i].pi_sector =
                     malloc( TIMINF.p_time_map[i].i_entry_nb * sizeof(u32) );
            if( TIMINF.p_time_map[i].pi_sector == NULL )
            {
                intf_ErrMsg( "ifo error: out of memory in IfoTitleSet" );
                return -1;
            }

            for( j = 0 ; j < TIMINF.p_time_map[i].i_entry_nb ; j++ )
            {
                TIMINF.p_time_map[i].pi_sector[j] =
                                        ReadDouble( p_ifo, p_buf, &p_tmp );
            }
        }
    }
#undef TIMINF

    if( MGINF.i_menu_cell_inf_start_sector
         && ReadCellInf( p_ifo, &p_ifo->vts.menu_cell_inf, p_ifo->vts.i_pos +
                         MGINF.i_menu_cell_inf_start_sector ) < 0 )
    {
        return -1;
    }

    if( MGINF.i_menu_vobu_map_start_sector
         && ReadVobuMap( p_ifo, &p_ifo->vts.menu_vobu_map, p_ifo->vts.i_pos +
                         MGINF.i_menu_vobu_map_start_sector ) < 0 )
    {
        return -1;
    }

    if( MGINF.i_cell_inf_start_sector
         && ReadCellInf( p_ifo, &p_ifo->vts.cell_inf, p_ifo->vts.i_pos +
                         MGINF.i_cell_inf_start_sector ) )
    {
        return -1;
    }

    if( MGINF.i_vobu_map_start_sector
         && ReadVobuMap( p_ifo, &p_ifo->vts.vobu_map, p_ifo->vts.i_pos +
                         MGINF.i_vobu_map_start_sector ) )
    {
        return -1;
    }
#undef MGINF

    intf_WarnMsg( 2, "ifo info: vts %d initialized",
         p_ifo->vmg.title_inf.p_attr[p_ifo->i_title-1].i_title_set_num );

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
 * IfoDestroy : Frees all the memory allocated to ifo structures
 *****************************************************************************/
void IfoDestroy( ifo_t * p_ifo )
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
static int ReadTitle( ifo_t * p_ifo, title_t * p_title, int i_block, int i_bytes )
{
    u8          p_buf[DVD_LB_SIZE];
    u8 *        p_tmp;
    int         i_start;
    u16         i_audio;
    u32         i_spu;
    int         i;

    p_tmp = FillBuffer( p_ifo, p_buf, i_block ) + i_bytes;

    i_start = p_ifo->i_pos;

    //fprintf( stderr, "PGC @ %d + %d\n", p_ifo->i_pos, i_bytes );

    DumpBytes( p_ifo, p_buf, &p_tmp, 2);
    p_title->i_chapter_nb = ReadByte( p_ifo, p_buf, &p_tmp );
    p_title->i_cell_nb = ReadByte( p_ifo, p_buf, &p_tmp );
    //fprintf( stderr, "title: Prg: %d Cell: %d\n",p_title->i_chapter_nb,p_title->i_cell_nb  );
    p_title->i_play_time = ReadDouble( p_ifo, p_buf, &p_tmp );
    p_title->i_prohibited_user_op = ReadDouble( p_ifo, p_buf, &p_tmp );

    for( i = 0 ; i < 8 ; i++ )
    {
        i_audio = ReadWord( p_ifo, p_buf, &p_tmp );
        p_title->pi_audio_status[i].i_foo = i_audio & 0xff;
        i_audio >>= 8;
        p_title->pi_audio_status[i].i_position = i_audio & 0x07;
        i_audio >>= 7;
        p_title->pi_audio_status[i].i_available = i_audio;
    }

    for( i = 0 ; i < 32 ; i++ )
    {
        i_spu = ReadDouble( p_ifo, p_buf, &p_tmp );
        p_title->pi_spu_status[i].i_position_pan = i_spu & 0x1f;
        i_spu >>= 8;
        p_title->pi_spu_status[i].i_position_letter = i_spu & 0x1f;
        i_spu >>= 8;
        p_title->pi_spu_status[i].i_position_wide = i_spu & 0x1f;
        i_spu >>= 8;
        p_title->pi_spu_status[i].i_position_43 = i_spu & 0x1f;
        i_spu >>= 7;
        p_title->pi_spu_status[i].i_available = i_spu;
    }

    p_title->i_next_title_num = ReadWord( p_ifo, p_buf, &p_tmp );
    p_title->i_prev_title_num = ReadWord( p_ifo, p_buf, &p_tmp );
    p_title->i_go_up_title_num = ReadWord( p_ifo, p_buf, &p_tmp );
    p_title->i_still_time = ReadByte( p_ifo, p_buf, &p_tmp );
    p_title->i_play_mode = ReadByte( p_ifo, p_buf, &p_tmp );

    for( i = 0 ; i < 16 ; i++ )
    {
        /* FIXME : We have to erase the extra bit */
        p_title->pi_yuv_color[i] = ReadDouble( p_ifo, p_buf, &p_tmp );
    }

    p_title->i_command_start_byte = ReadWord( p_ifo, p_buf, &p_tmp );
    p_title->i_chapter_map_start_byte = ReadWord( p_ifo, p_buf, &p_tmp );
    p_title->i_cell_play_start_byte = ReadWord( p_ifo, p_buf, &p_tmp );
    p_title->i_cell_pos_start_byte = ReadWord( p_ifo, p_buf, &p_tmp );

    /* parsing of command_t */
    if( p_title->i_command_start_byte )
    {
        p_tmp = FillBuffer( p_ifo, p_buf, i_start +
                            OFF2LB( p_title->i_command_start_byte + i_bytes ) )
                 + ( (p_title->i_command_start_byte + i_bytes) & 0x7ff );

        /* header */
        p_title->command.i_pre_command_nb = ReadWord( p_ifo, p_buf, &p_tmp );
        p_title->command.i_post_command_nb = ReadWord( p_ifo, p_buf, &p_tmp );
        p_title->command.i_cell_command_nb = ReadWord( p_ifo, p_buf, &p_tmp );
        DumpBytes( p_ifo, p_buf, &p_tmp, 2 );

        /* pre-title commands */
        if( p_title->command.i_pre_command_nb )
        {
            p_title->command.p_pre_command =
                           malloc( p_title->command.i_pre_command_nb
                                    * sizeof(command_desc_t) );

            if( p_title->command.p_pre_command == NULL )
            {
                intf_ErrMsg( "ifo error: out of memory in ReadTitle" );
                return -1;
            }

            for( i = 0 ; i < p_title->command.i_pre_command_nb ; i++ )
            {
                p_title->command.p_pre_command[i] =
                                        ReadQuad( p_ifo, p_buf, &p_tmp );
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
                                 * sizeof(command_desc_t) );

            if( p_title->command.p_post_command == NULL )
            {
                intf_ErrMsg( "ifo error: out of memory in ReadTitle" );
                return -1;
            }

            for( i = 0 ; i < p_title->command.i_post_command_nb ; i++ )
            {
                p_title->command.p_post_command[i] =
                                        ReadQuad( p_ifo, p_buf, &p_tmp );
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
                                 * sizeof(command_desc_t) );

            if( p_title->command.p_cell_command == NULL )
            {
                intf_ErrMsg( "ifo error: out of memory in ReadTitle" );
                return -1;
            }

            for( i = 0 ; i < p_title->command.i_cell_command_nb ; i++ )
            {
                p_title->command.p_cell_command[i] =
                                        ReadQuad( p_ifo, p_buf, &p_tmp );
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
        p_ifo->i_pos = dvdcss_seek( p_ifo->dvdhandle,
               OFF2LB( i_start + p_title->i_chapter_map_start_byte ) );
        
        p_title->chapter_map.pi_start_cell =
                    malloc( p_title->i_chapter_nb * sizeof(chapter_map_t) );
        
        if( p_title->chapter_map.pi_start_cell == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in Read Title" );
            return -1;
        }

        ReadBytes( p_ifo, p_buf, &p_tmp, p_title->chapter_map.pi_start_cell,
                   p_title->i_chapter_nb );
    }
    else
    {
        p_title->chapter_map.pi_start_cell = NULL; 
    }

    /* parsing of cell_play_t */
    if( p_title->i_cell_play_start_byte )
    {
        p_tmp = FillBuffer( p_ifo, p_buf, i_start +
                            OFF2LB( p_title->i_cell_play_start_byte+i_bytes ) )
                 + ( (p_title->i_cell_play_start_byte+i_bytes) & 0x7ff );

        p_title->p_cell_play = malloc( p_title->i_cell_nb
                                        * sizeof(cell_play_t) );
    
        if( p_title->p_cell_play == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory in ReadTitle" );
            return -1;
        }

        for( i = 0 ; i < p_title->i_cell_nb ; i++ )
        {
#define PLAY p_title->p_cell_play[i]
            PLAY.i_category = ReadWord( p_ifo, p_buf, &p_tmp );
            PLAY.i_still_time = ReadByte( p_ifo, p_buf, &p_tmp );
            PLAY.i_command_nb = ReadByte( p_ifo, p_buf, &p_tmp );
            PLAY.i_play_time = ReadDouble( p_ifo, p_buf, &p_tmp );
            PLAY.i_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
            PLAY.i_first_ilvu_vobu_esector = ReadDouble( p_ifo, p_buf, &p_tmp );
            PLAY.i_last_vobu_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
            PLAY.i_end_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
#undef PLAY
        }
    }

    /* Parsing of cell_pos_t */
    if( p_title->i_cell_pos_start_byte )
    {
        p_tmp = FillBuffer( p_ifo, p_buf, i_start +
                            OFF2LB( p_title->i_cell_pos_start_byte + i_bytes ) )
                 + ( (p_title->i_cell_pos_start_byte + i_bytes) & 0x7ff );

        p_title->p_cell_pos = malloc( p_title->i_cell_nb
                                       * sizeof(cell_pos_t) );

        if( p_title->p_cell_pos == NULL )
        {
            intf_ErrMsg( "ifo error: out of memory" );
            return -1;
        }

        for( i = 0 ; i < p_title->i_cell_nb ; i++ )
        {
            p_title->p_cell_pos[i].i_vob_id = ReadWord( p_ifo, p_buf, &p_tmp );
            DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
            p_title->p_cell_pos[i].i_cell_id = ReadByte( p_ifo, p_buf, &p_tmp );
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
static int ReadUnitInf( ifo_t * p_ifo, unit_inf_t * p_unit_inf,
                        int i_block, int i_bytes )
{
    u8              p_buf[DVD_LB_SIZE];
    u8 *            p_tmp;
    int             i_start;
    int             i;

    p_tmp = FillBuffer( p_ifo, p_buf, i_block ) + i_bytes;

    i_start = p_ifo->i_pos;
    //fprintf( stderr, "Unit\n" );

    p_unit_inf->i_title_nb = ReadWord( p_ifo, p_buf, &p_tmp );
    //fprintf( stderr, "Unit nb: %d\n", p_unit_inf->i_title_nb );
    DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
    p_unit_inf->i_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );

    p_unit_inf->p_title =
            malloc( p_unit_inf->i_title_nb * sizeof(unit_title_t) );
    if( p_unit_inf->p_title == NULL )
    {
        intf_ErrMsg( "ifo error: out of memory in ReadUnit" );
        return -1;
    }

    for( i = 0 ; i < p_unit_inf->i_title_nb ; i++ )
    {
#define TITLE p_unit_inf->p_title[i]
        TITLE.i_category_mask = ReadByte( p_ifo, p_buf, &p_tmp );
        TITLE.i_category = ReadByte( p_ifo, p_buf, &p_tmp );
        //fprintf( stderr, "cat mask %d: %x cat %x\n", i, TITLE.i_category_mask, TITLE.i_category );
        TITLE.i_parental_mask = ReadWord( p_ifo, p_buf, &p_tmp );
        TITLE.i_title_start_byte = ReadDouble( p_ifo, p_buf, &p_tmp );
#undef TITLE
    }

    for( i = 0 ; i < p_unit_inf->i_title_nb ; i++ )
    {
        //fprintf( stderr, "Unit: PGC %d @ %d\n", i, p_ifo->i_pos );
        ReadTitle( p_ifo, &p_unit_inf->p_title[i].title, i_start +
            OFF2LB( p_unit_inf->p_title[i].i_title_start_byte + i_bytes ),
          (p_unit_inf->p_title[i].i_title_start_byte+i_bytes) & 0x7ff );
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
static int ReadTitleUnit( ifo_t * p_ifo, title_unit_t * p_title_unit,
                          int i_block )
{
    u8              p_buf[DVD_LB_SIZE];
    u8 *            p_tmp;
    int             i;
    int             i_start;

    p_tmp = FillBuffer( p_ifo, p_buf, i_block );
    i_start = p_ifo->i_pos;
    //fprintf( stderr, "Unit Table\n" );

    p_title_unit->i_unit_nb = ReadWord( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
    p_title_unit->i_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );

    //fprintf(stderr, "Unit: nb %d end %d\n", p_title_unit->i_unit_nb, p_title_unit->i_end_byte );

    p_title_unit->p_unit = malloc( p_title_unit->i_unit_nb * sizeof(unit_t) );
    if( p_title_unit->p_unit == NULL )
    {
        intf_ErrMsg( "ifo error: out of memory in ReadTitleUnit" );
        return -1;
    }

    for( i = 0 ; i < p_title_unit->i_unit_nb ; i++ )
    {
        //ReadBytes( p_ifo, p_buf, &p_tmp, p_title_unit->p_unit[i].ps_lang_code, 2 );
        p_title_unit->p_unit[i].i_lang_code = ReadWord( p_ifo, p_buf, &p_tmp );
        //fprintf( stderr, "lang %d %x\n", i,p_title_unit->p_unit[i].i_lang_code );
        DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
        p_title_unit->p_unit[i].i_existence_mask =
                                           ReadByte( p_ifo, p_buf, &p_tmp );
        p_title_unit->p_unit[i].i_unit_inf_start_byte =
                                           ReadDouble( p_ifo, p_buf, &p_tmp );
    }

    p_title_unit->p_unit_inf =
                malloc( p_title_unit->i_unit_nb * sizeof(unit_inf_t) );
    if( p_title_unit->p_unit_inf == NULL )
    {
        intf_ErrMsg( "ifo error: out of memory in ReadTitleUnit" );
        return -1;
    }

    for( i = 0 ; i < p_title_unit->i_unit_nb ; i++ )
    {
        ReadUnitInf( p_ifo, &p_title_unit->p_unit_inf[i], i_start +
                      OFF2LB( p_title_unit->p_unit[i].i_unit_inf_start_byte ),
                     p_title_unit->p_unit[i].i_unit_inf_start_byte & 0x7ff );
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
static int ReadCellInf( ifo_t * p_ifo, cell_inf_t * p_cell_inf, int i_block )
{
    u8              p_buf[DVD_LB_SIZE];
    u8 *            p_tmp;
    int             i_start;
    int             i;

    p_tmp = FillBuffer( p_ifo, p_buf, i_block );
    i_start = p_ifo->i_pos;
    //fprintf( stderr, "CELL ADD\n" );

    p_cell_inf->i_vob_nb = ReadWord( p_ifo, p_buf, &p_tmp );
    DumpBytes( p_ifo, p_buf, &p_tmp, 2 );
    p_cell_inf->i_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );

    p_cell_inf->i_cell_nb = (p_cell_inf->i_end_byte/* - 7*/) / sizeof(cell_map_t);

    //fprintf( stderr, "Cell inf: vob %d end %d cell %d\n", p_cell_inf->i_vob_nb, p_cell_inf->i_end_byte,  p_cell_inf->i_cell_nb );

    p_cell_inf->p_cell_map =
                malloc( p_cell_inf->i_cell_nb *sizeof(cell_map_t) );
    if( p_cell_inf->p_cell_map == NULL )
    {
        intf_ErrMsg( "ifo error: out of memory in ReadCellInf" );
        return -1;
    }

    for( i = 0 ; i < p_cell_inf->i_cell_nb ; i++ )
    {
#define MAP p_cell_inf->p_cell_map[i]
        MAP.i_vob_id = ReadWord( p_ifo, p_buf, &p_tmp );
        MAP.i_cell_id = ReadByte( p_ifo, p_buf, &p_tmp );
        DumpBytes( p_ifo, p_buf, &p_tmp, 1 );
        MAP.i_start_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
        //fprintf(stderr, "sector[%d] %d (%d)\n", i,ntohl(*(u32*)(p_tmp)), p_ifo->i_pos);
        MAP.i_end_sector = ReadDouble( p_ifo, p_buf, &p_tmp );
#undef MAP
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
static int ReadVobuMap( ifo_t * p_ifo, vobu_map_t * p_vobu_map, int i_block )
{
    u8                  p_buf[DVD_LB_SIZE];
    u8 *                p_tmp;
    int                 i_start;
    int                 i, i_max;
    
    p_tmp = FillBuffer( p_ifo, p_buf, i_block );
    i_start = p_ifo->i_pos;
    //fprintf( stderr, "VOBU ADMAP\n" );

    p_vobu_map->i_end_byte = ReadDouble( p_ifo, p_buf, &p_tmp );
    i_max = ( i_start + p_vobu_map->i_end_byte + 1 - p_ifo->i_pos )
             / sizeof(u32);

    p_vobu_map->pi_vobu_start_sector = malloc( i_max * sizeof(u32) );
    if( p_vobu_map->pi_vobu_start_sector == NULL )
    {
        intf_ErrMsg( "ifo error: out of memory in ReadVobuMap" );
        return -1;
    }

    for( i = 0 ; i < i_max ; i++ )
    {
        p_vobu_map->pi_vobu_start_sector[i] = ReadDouble( p_ifo, p_buf, &p_tmp );
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

/*****************************************************************************
 * ReadByte and so
 *****************************************************************************/
static u8* FillBuffer( ifo_t* p_ifo, u8* p_buf, int i_pos )
{
    p_ifo->i_pos = dvdcss_seek( p_ifo->dvdhandle, i_pos );
    dvdcss_read( p_ifo->dvdhandle, p_buf, 1, DVDCSS_NOFLAGS );

    return p_buf;
}

static void ReadBytes( ifo_t* p_ifo, u8* p_buf, u8** pp_tmp,
                                     u8* pi_dest, int i_nb )
{
    if( i_nb > DVD_LB_SIZE )
    {
        intf_ErrMsg( "ifo error: excessive ReadBytes call (%i)", i_nb );
    }

    if( *pp_tmp + i_nb >= p_buf + DVD_LB_SIZE )
    {
        int i_spare = (int)( (p_buf + DVD_LB_SIZE) - *pp_tmp );

        /* Copy the bytes remaining in the current buffer */
        memcpy( pi_dest, *pp_tmp, i_spare );
        pi_dest += i_spare;
        i_nb -= i_spare;

        /* Load the next buffer */
        *pp_tmp = FillBuffer( p_ifo, p_buf, p_ifo->i_pos + 1 );
    }

    memcpy( pi_dest, *pp_tmp, i_nb );
    *pp_tmp += i_nb;

    return;
}

static void DumpBytes( ifo_t* p_ifo, u8* p_buf, u8** pp_tmp, int i_nb )
{
    if( i_nb > DVD_LB_SIZE )
    {
        intf_ErrMsg( "ifo error: excessive DumpBytes call (%i)", i_nb );
    }

    *pp_tmp += i_nb;

    if( *pp_tmp >= p_buf + DVD_LB_SIZE )
    {
        /* If we went too far, load the next buffer */
        *pp_tmp = FillBuffer( p_ifo, p_buf, p_ifo->i_pos + 1 )
                   + (int)( (*pp_tmp) - (p_buf - DVD_LB_SIZE) );
    }

    return;
}

#define ADDBYTE \
    if( *pp_tmp >= p_buf + DVD_LB_SIZE ) \
    { \
        *pp_tmp = FillBuffer( p_ifo, p_buf, p_ifo->i_pos + 1 ); \
    } \
    i_ret <<= 8; i_ret |= **pp_tmp; (*pp_tmp)++;

static u8 ReadByte( ifo_t * p_ifo, u8* p_buf, u8** pp_tmp )
{
    u8     i_ret = 0;
    ADDBYTE;
    return i_ret;
}

static u16 ReadWord( ifo_t * p_ifo, u8* p_buf, u8** pp_tmp )
{
    u16    i_ret = 0;
    ADDBYTE; ADDBYTE;
    return i_ret;
}

static u32 ReadDouble( ifo_t * p_ifo, u8* p_buf, u8** pp_tmp )
{
    u32    i_ret = 0;
    ADDBYTE; ADDBYTE; ADDBYTE; ADDBYTE;
    return i_ret;
}

static u64 ReadQuad( ifo_t * p_ifo, u8* p_buf, u8** pp_tmp )
{
    u64    i_ret = 0;
    ADDBYTE; ADDBYTE; ADDBYTE; ADDBYTE; ADDBYTE; ADDBYTE; ADDBYTE; ADDBYTE;
    return i_ret;
}

