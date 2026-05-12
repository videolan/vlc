/*****************************************************************************
 * dvdread_video.c : DVD-Video support for the dvdread input module
 *****************************************************************************
 * Copyright (C) 2001-2025 VLC authors and VideoLAN
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Saifelden Mohamed Ismail <saifeldenmi@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dvdread.h"
#include <limits.h>
#include <vlc_arrays.h>

extern void DvdReadESNew( demux_t *, int, int );
void DvdReadHandleDSI( demux_t *, uint8_t * );

static vlc_tick_t dvdtime_to_time( const dvd_time_t *dtime )
{
/* Macro to convert Binary Coded Decimal to Decimal */
#define BCD2D(__x__) (((__x__ & 0xf0) >> 4) * 10 + (__x__ & 0x0f))

    double f_fps, f_ms;

    int64_t sec = (int64_t)(BCD2D(dtime->hour)) * 60 * 60;
    sec += (int64_t)(BCD2D(dtime->minute)) * 60;
    sec += (int64_t)(BCD2D(dtime->second));

    switch((dtime->frame_u & 0xc0) >> 6)
    {
    case 1:
        f_fps = 25.0;
        break;
    case 3:
        f_fps = 29.97;
        break;
    default:
        f_fps = 2500.0;
        break;
    }
    f_ms = BCD2D(dtime->frame_u&0x3f) * 1000.0 / f_fps;
    return vlc_tick_from_sec(sec) + VLC_TICK_FROM_MS(f_ms);
}

/* total playback duration of the current title in vlc_tick_t
 * for dvd-video sums per-cell playback_time across the active cell range
 * so angle-restricted titles report the correct length
 * falls back to pgc playback_time for other disc types */
static vlc_tick_t DvdReadVideoTitleDuration( const demux_sys_t *p_sys )
{
    if( p_sys->p_cur_pgc && p_sys->p_cur_pgc->cell_playback &&
        p_sys->i_title_start_cell >= 0 &&
        p_sys->i_title_start_cell < p_sys->i_title_end_cell )
    {
        vlc_tick_t dur = 0;
        for( int ci = p_sys->i_title_start_cell; ci <= p_sys->i_title_end_cell; ci++ )
        {
            dur += dvdtime_to_time( &p_sys->p_cur_pgc->cell_playback[ci].playback_time );
        }
        return dur;
    }

    if( p_sys->p_cur_pgc )
        return dvdtime_to_time( &p_sys->p_cur_pgc->playback_time );
    return 0;
}

static vlc_tick_t DvdReadTitleLength( const demux_sys_t *p_sys )
{
    return DvdReadVideoTitleDuration( p_sys );
}

static bool DvdReadTimelineBase( const demux_sys_t *p_sys, vlc_tick_t *base )
{
    const pgc_t *p_pgc = p_sys->p_cur_pgc;
    if( !p_pgc || !p_pgc->cell_playback ||
        p_sys->i_title_start_cell < 0 || p_sys->i_cur_cell < 0 ||
        p_sys->i_title_start_cell >= p_pgc->nr_of_cells ||
        p_sys->i_cur_cell >= p_pgc->nr_of_cells ||
        p_sys->i_title_start_cell > p_sys->i_cur_cell )
        return false;

    vlc_tick_t cell_dvd_time = 0;
    for( int i = p_sys->i_title_start_cell; i < p_sys->i_cur_cell; i++ )
        cell_dvd_time += dvdtime_to_time( &p_pgc->cell_playback[i].playback_time );

    const cell_playback_t *p_cell = &p_pgc->cell_playback[p_sys->i_cur_cell];
    vlc_tick_t cell_duration = dvdtime_to_time( &p_cell->playback_time );
    const uint32_t cell_sectors = p_cell->last_sector - p_cell->first_sector + 1;

    if( cell_duration > 0 && cell_sectors > 0 )
    {
        int64_t within_sectors = p_sys->i_cur_block - (int64_t)p_cell->first_sector;

        if( within_sectors < 0 )
            within_sectors = 0;
        else if( within_sectors >= (int64_t)cell_sectors )
            within_sectors = (int64_t)cell_sectors - 1;

        vlc_tick_t scaled;
        if( !ckd_mul( &scaled, within_sectors, cell_duration ) )
            cell_dvd_time += scaled / cell_sectors;
    }

    *base = cell_dvd_time;
    return true;
}

static uint32_t DvdReadTimeToSeekOffset( const demux_sys_t *p_sys, vlc_tick_t time,
                                         uint32_t i_block_offset )
{
    VLC_UNUSED( p_sys );
    VLC_UNUSED( time );
    return i_block_offset;
}

static int DvdReadPsSource( void )
{
    return PS_SOURCE_VOB;
}

static void DvdReadAdjustAnchor( demux_sys_t *p_sys, block_t *pkt )
{
    VLC_UNUSED( p_sys );
    VLC_UNUSED( pkt );
}

/*****************************************************************************
 * DvdReadFindCell
 *****************************************************************************/
static void DvdReadFindCell( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    const pgc_t *p_pgc;
    int   pgc_id, pgn;
    int   i = 0;

    const cell_playback_t *cell = p_sys->p_cur_pgc->cell_playback;
    if( cell[p_sys->i_cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK )
    {
        p_sys->i_cur_cell += p_sys->i_angle - 1;

        while( cell[p_sys->i_cur_cell+i].block_mode != BLOCK_MODE_LAST_CELL )
        {
            i++;
        }
        p_sys->i_next_cell = p_sys->i_cur_cell + i + 1;
    }
    else
    {
        p_sys->i_next_cell = p_sys->i_cur_cell + 1;
    }

    if( p_sys->i_chapter + 1 >= p_sys->i_chapters ) return;

    pgc_id = p_sys->p_vts_file->vts_ptt_srpt->title[
                p_sys->i_ttn - 1].ptt[p_sys->i_chapter + 1].pgcn;
    pgn = p_sys->p_vts_file->vts_ptt_srpt->title[
              p_sys->i_ttn - 1].ptt[p_sys->i_chapter + 1].pgn;
    p_pgc = p_sys->p_vts_file->vts_pgcit->pgci_srp[pgc_id - 1].pgc;

    if( p_sys->i_cur_cell >= p_pgc->program_map[pgn - 1] - 1 )
    {
        p_sys->i_chapter++;

        if( p_sys->i_chapter < p_sys->i_chapters &&
            p_sys->cur_chapter != p_sys->i_chapter )
        {
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_chapter = p_sys->i_chapter;
        }
    }
}

/*****************************************************************************
 * DvdReadHandleDSI
 *****************************************************************************/
void DvdReadHandleDSI( demux_t *p_demux, uint8_t *p_data )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Check we are really on a DSI packet
     * http://www.mpucoder.com/DVD/dsi_pkt.html
     * Some think it's funny to fill with 0x42 */
    const uint8_t dsiheader[7] = { 0x00, 0x00, 0x01, 0xbf, 0x03, 0xfa, 0x01 };
    if(!memcmp(&p_data[DSI_START_BYTE-7], dsiheader, 7))
    {
        navRead_DSI( &p_sys->dsi_pack, &p_data[DSI_START_BYTE] );

        /*
         * Determine where we go next.  These values are the ones we mostly
        * care about.
        */
        p_sys->i_cur_block = p_sys->dsi_pack.dsi_gi.nv_pck_lbn;
        p_sys->i_pack_len = p_sys->dsi_pack.dsi_gi.vobu_ea;

        /*
        * If we're not at the end of this cell, we can determine the next
        * VOBU to display using the VOBU_SRI information section of the
        * DSI.  Using this value correctly follows the current angle,
        * avoiding the doubled scenes in The Matrix, and makes our life
        * really happy.
        */

        p_sys->i_next_vobu = p_sys->i_cur_block +
            ( p_sys->dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
    }
    else
    {
        /* resync after decoy/corrupted titles */
        msg_Warn(p_demux, "Invalid DSI packet in VOBU %d found, skipping Cell %d / %d",
                 p_sys->i_next_vobu, p_sys->i_cur_cell, p_sys->i_title_end_cell);
        p_sys->dsi_pack.vobu_sri.next_vobu = SRI_END_OF_CELL;
    }

    if( p_sys->dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL
        && p_sys->i_angle > 1 )
    {
        switch( ( p_sys->dsi_pack.sml_pbi.category & 0xf000 ) >> 12 )
        {
        case 0x4:
            /* Interleaved unit with no angle */
            if( p_sys->dsi_pack.sml_pbi.ilvu_sa != 0 )
            {
                p_sys->i_next_vobu = p_sys->i_cur_block +
                    p_sys->dsi_pack.sml_pbi.ilvu_sa;
                p_sys->i_pack_len = p_sys->dsi_pack.sml_pbi.ilvu_ea;
            }
            else
            {
                p_sys->i_next_vobu = p_sys->i_cur_block +
                    p_sys->dsi_pack.dsi_gi.vobu_ea + 1;
            }
            break;
        case 0x5:
            /* vobu is end of ilvu */
            if( p_sys->dsi_pack.sml_agli.data[p_sys->i_angle-1].address )
            {
                p_sys->i_next_vobu = p_sys->i_cur_block +
                    p_sys->dsi_pack.sml_agli.data[p_sys->i_angle-1].address;
                p_sys->i_pack_len = p_sys->dsi_pack.sml_pbi.ilvu_ea;

                break;
            }
            /* fall through */
        case 0x6:
            /* vobu is beginning of ilvu */
        case 0x9:
            /* next scr is 0 */
        case 0xa:
            /* entering interleaved section */
        case 0x8:
            /* non interleaved cells in interleaved section */
        default:
            p_sys->i_next_vobu = p_sys->i_cur_block +
                ( p_sys->dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
            break;
        }
    }
    else if( p_sys->dsi_pack.vobu_sri.next_vobu == SRI_END_OF_CELL )
    {
        p_sys->i_cur_cell = p_sys->i_next_cell;

        DvdReadResetCellTs( p_sys );

        /* End of title */
        if( p_sys->i_cur_cell >= p_sys->p_cur_pgc->nr_of_cells ) return;

        DvdReadFindCell( p_demux );

        p_sys->i_next_vobu =
            p_sys->p_cur_pgc->cell_playback[p_sys->i_cur_cell].first_sector;
    }


#if 0
    msg_Dbg( p_demux, "scr %d lbn 0x%02x vobu_ea %d vob_id %d c_id %d c_time %lld",
             p_sys->dsi_pack.dsi_gi.nv_pck_scr,
             p_sys->dsi_pack.dsi_gi.nv_pck_lbn,
             p_sys->dsi_pack.dsi_gi.vobu_ea,
             p_sys->dsi_pack.dsi_gi.vobu_vob_idn,
             p_sys->dsi_pack.dsi_gi.vobu_c_idn,
             dvdtime_to_time( &p_sys->dsi_pack.dsi_gi.c_eltm ) );

    msg_Dbg( p_demux, "cell duration: %lld",
             dvdtime_to_time( &p_sys->p_cur_pgc->cell_playback[p_sys->i_cur_cell].playback_time ) );

    msg_Dbg( p_demux, "cat 0x%02x ilvu_ea %d ilvu_sa %d size %d",
             p_sys->dsi_pack.sml_pbi.category,
             p_sys->dsi_pack.sml_pbi.ilvu_ea,
             p_sys->dsi_pack.sml_pbi.ilvu_sa,
             p_sys->dsi_pack.sml_pbi.size );

    msg_Dbg( p_demux, "next_vobu %d next_ilvu1 %d next_ilvu2 %d",
             p_sys->dsi_pack.vobu_sri.next_vobu & 0x7fffffff,
             p_sys->dsi_pack.sml_agli.data[ p_sys->i_angle - 1 ].address,
             p_sys->dsi_pack.sml_agli.data[ p_sys->i_angle ].address);
#endif
}

/*****************************************************************************
 * DvdReadSeek : Goes to a given position on the stream.
 *****************************************************************************
 * This one is used by the input and translate chronological position from
 * input to logical position on the device.
 *****************************************************************************/
static int DvdReadSeek( demux_t *p_demux, uint32_t i_block_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_chapter = 0;
    int i_cell = 0;
    uint32_t i_block;
    const pgc_t *p_pgc = p_sys->p_cur_pgc;
    const ifo_handle_t *p_vts = p_sys->p_vts_file;

    /* Find cell */
    i_block = i_block_offset;
    for( i_cell = p_sys->i_title_start_cell;
         i_cell <= p_sys->i_title_end_cell; i_cell++ )
    {
        if( i_block < p_pgc->cell_playback[i_cell].last_sector -
            p_pgc->cell_playback[i_cell].first_sector + 1 ) break;

        i_block -= (p_pgc->cell_playback[i_cell].last_sector -
            p_pgc->cell_playback[i_cell].first_sector + 1);
    }
    if( i_cell > p_sys->i_title_end_cell )
    {
        msg_Err( p_demux, "couldn't find cell for block %u", i_block_offset );
        return VLC_EGENERIC;
    }
    i_block += p_pgc->cell_playback[i_cell].first_sector;
    p_sys->i_title_offset = i_block_offset;

    /* Find chapter */
    for( i_chapter = 0; i_chapter < p_sys->i_chapters; i_chapter++ )
    {
        int pgc_id, pgn, i_tmp;

        pgc_id = p_vts->vts_ptt_srpt->title[
                    p_sys->i_ttn - 1].ptt[i_chapter].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[
                    p_sys->i_ttn - 1].ptt[i_chapter].pgn;

        i_tmp = p_vts->vts_pgcit->pgci_srp[pgc_id - 1].pgc->program_map[pgn-1];

        if( i_tmp > i_cell ) break;
    }

    if( i_chapter < p_sys->i_chapters &&
        p_sys->cur_chapter != i_chapter )
    {
        p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
        p_sys->cur_chapter = i_chapter;
    }

    /* Find vobu */
    /* see ifo_read.c / ifoRead_VOBU_ADMAP_internal for index count */
    if( unlikely( p_vts->vts_vobu_admap == NULL ||
                  p_vts->vts_vobu_admap->last_byte < VOBU_ADMAP_SIZE ) )
        return VLC_EGENERIC;

    size_t i_vobu = 1;
    const size_t i_vobu_sect_index_count =
            (p_vts->vts_vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE) /
            sizeof(*p_vts->vts_vobu_admap->vobu_start_sectors);
    if( unlikely( i_vobu_sect_index_count == 0 ) )
        return VLC_EGENERIC;
    for( size_t i=0; i<i_vobu_sect_index_count; i++ )
    {
        if( p_vts->vts_vobu_admap->vobu_start_sectors[i] > i_block )
            break;
        i_vobu = i + 1;
    }

#if 1
    int i_sub_cell = 1;
    /* Find sub_cell */
    /* need to check cell # <= vob count as cell table alloc only ensures:
     * info_length / sizeof(cell_adr_t) < c_adt->nr_of_vobs, see ifo_read.c */
    const uint32_t vobu_start_sector = p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu-1];
    for( int i = 0; i + 1<p_vts->vts_c_adt->nr_of_vobs; i++ )
    {
        const cell_adr_t *p_cell = &p_vts->vts_c_adt->cell_adr_table[i];
        if(p_cell->start_sector <= vobu_start_sector)
           i_sub_cell = i + 1;
    }

    /* i_vobu reaches count when no entry exceeds i_block, guard the debug peek */
    const size_t i_vobu_dbg_next = i_vobu < i_vobu_sect_index_count
        ? i_vobu : i_vobu_sect_index_count - 1;
    msg_Dbg( p_demux, "cell %d i_sub_cell %d chapter %d vobu %zu "
             "cell_sector %d vobu_sector %d sub_cell_sector %d",
             i_cell, i_sub_cell, i_chapter, i_vobu,
             p_sys->p_cur_pgc->cell_playback[i_cell].first_sector,
             p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu_dbg_next],
             p_vts->vts_c_adt->cell_adr_table[i_sub_cell - 1].start_sector);
#endif

    /* snap to vobu start, nav packet only at vobu head */
    const uint32_t vobu_start_sector_snap =
        p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu - 1];

    p_sys->i_cur_block = vobu_start_sector_snap;
    p_sys->i_next_vobu = vobu_start_sector_snap;
    /* len set by DvdReadHandleDSI from dsi */
    p_sys->i_pack_len = 0;
    p_sys->i_cur_cell = i_cell;
    p_sys->i_chapter = i_chapter;
    DvdReadFindCell( p_demux );

    DvdReadResetCellTs( p_sys );
    es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DvdReadSetArea: initialize input data for title x, chapter y.
 * It should be called for each user navigation request.
 *****************************************************************************
 * Take care that i_title and i_chapter start from 0.
 *****************************************************************************/
static int DvdReadSetArea( demux_t *p_demux, int i_title, int i_chapter,
                           int i_angle )
{
    VLC_UNUSED( i_angle );

    demux_sys_t *p_sys = p_demux->p_sys;
    int pgc_id = 0, pgn = 0;

    if( i_title >= 0 && i_title < p_sys->i_titles &&
        i_title != p_sys->i_title )
    {
        int i_start_cell, i_end_cell;

        if( p_sys->p_title != NULL )
        {
            DVDCloseFile( p_sys->p_title );
            p_sys->p_title = NULL;
        }
        p_sys->i_title = i_title;
        const ifo_handle_t *p_vmg = p_sys->p_vmg_file;

        /*
         *  We have to load all title information
         */
        msg_Dbg( p_demux, "open VTS %d, for title %d",
                 p_vmg->tt_srpt->title[i_title].title_set_nr, i_title + 1 );

        /* Ifo vts */
        if( p_sys->p_vts_file != NULL )
            ifoClose( p_sys->p_vts_file );
        if( !( p_sys->p_vts_file = ifoOpen( p_sys->p_dvdread,
               p_vmg->tt_srpt->title[i_title].title_set_nr ) ) )
        {
            msg_Err( p_demux, "fatal error in vts ifo" );
            return VLC_EGENERIC;
        }

        const ifo_handle_t *p_vts = p_sys->p_vts_file;

        /* Title position inside the selected vts */
        p_sys->i_ttn = p_vmg->tt_srpt->title[i_title].vts_ttn;

        /* Find title start/end */
        pgc_id = p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1].ptt[0].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1].ptt[0].pgn;
        const pgc_t *p_pgc =
            p_sys->p_cur_pgc = p_vts->vts_pgcit->pgci_srp[pgc_id - 1].pgc;

        if( p_pgc->cell_playback == NULL )
        {
            msg_Err( p_demux, "Invalid PGC (cell_playback_offset)" );
            return VLC_EGENERIC;
        }

        p_sys->i_title_start_cell =
            i_start_cell = p_pgc->program_map[pgn - 1] - 1;
        p_sys->i_title_start_block =
            p_pgc->cell_playback[i_start_cell].first_sector;

        p_sys->i_title_end_cell =
            i_end_cell = p_pgc->nr_of_cells - 1;
        p_sys->i_title_end_block =
            p_pgc->cell_playback[i_end_cell].last_sector;

        p_sys->i_title_offset = 0;

        p_sys->i_title_blocks = 0;
        for( int i = i_start_cell; i <= i_end_cell; i++ )
        {
            const uint32_t cell_blocks = p_pgc->cell_playback[i].last_sector -
                                         p_pgc->cell_playback[i].first_sector + 1;
            if(unlikely( cell_blocks == 0 || cell_blocks > INT_MAX ||
                 INT_MAX - p_sys->i_title_blocks < cell_blocks ))
                return VLC_EGENERIC;
            p_sys->i_title_blocks += cell_blocks;
        }

        msg_Dbg( p_demux, "title %d vts_title %d pgc %d pgn %d "
                 "start %d end %d blocks: %u",
                 i_title + 1, p_sys->i_ttn, pgc_id, pgn,
                 p_sys->i_title_start_block, p_sys->i_title_end_block,
                 p_sys->i_title_blocks );

        /*
         * Set properties for current chapter
         */
        p_sys->i_chapter = 0;
        p_sys->i_chapters =
            p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1].nr_of_ptts;

        pgc_id = p_vts->vts_ptt_srpt->title[
                    p_sys->i_ttn - 1].ptt[p_sys->i_chapter].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[
                    p_sys->i_ttn - 1].ptt[p_sys->i_chapter].pgn;

        p_pgc = p_vts->vts_pgcit->pgci_srp[pgc_id - 1].pgc;
        p_sys->i_pack_len = 0;
        p_sys->i_next_cell =
            p_sys->i_cur_cell = p_pgc->program_map[pgn - 1] - 1;
        DvdReadFindCell( p_demux );

        /* walk ptts accumulating per-cell durations to set each seekpoint's i_time_offset */
        if( p_sys->i_chapters > 0 )
        {
            input_title_t *p_title = p_sys->titles[i_title];
            if( p_title->seekpoint )
            {
                const int max_sp = __MIN( p_title->i_seekpoint,
                                            p_sys->i_chapters );
                const int max_ptt = p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1].nr_of_ptts;
                const pgc_t *p_pgc_seek = p_sys->p_cur_pgc;
                int cell = p_sys->i_title_start_cell;
                vlc_tick_t t = 0;
                for( int sj = 0; sj < max_sp && sj < max_ptt; sj++ )
                {
                    const uint16_t sj_pgc_id =
                        p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1]
                                             .ptt[sj].pgcn;
                    const uint16_t sj_pgn =
                        p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1]
                                             .ptt[sj].pgn;

                    /* cells are pgc-local, stop on pgc mismatch */
                    if( sj_pgc_id != pgc_id || sj_pgn == 0 )
                        break;
                    if( sj_pgn > p_pgc_seek->nr_of_programs )
                        break;

                    const int chapter_cell =
                        p_pgc_seek->program_map[sj_pgn - 1] - 1;
                    while( cell < chapter_cell &&
                           cell <= p_sys->i_title_end_cell )
                    {
                        t += dvdtime_to_time( &p_pgc_seek->cell_playback[cell]
                                                  .playback_time );
                        cell++;
                    }
                    p_title->seekpoint[sj]->i_time_offset = t;
                }
                p_title->i_length = DvdReadTitleLength( p_sys );
            }
        }

        DvdReadResetCellTs( p_sys );
        p_sys->i_next_vobu = p_sys->i_cur_block =
            p_pgc->cell_playback[p_sys->i_cur_cell].first_sector;

        /*
         * Angle management
         */
        p_sys->i_angles = p_vmg->tt_srpt->title[i_title].nr_of_angles;
        if( p_sys->i_angle > p_sys->i_angles ) p_sys->i_angle = 1;

        /*
         * We've got enough info, time to open the title set data.
         */
        if( !( p_sys->p_title = DVDOpenFile( p_sys->p_dvdread,
            p_vmg->tt_srpt->title[i_title].title_set_nr,
            DVD_READ_TITLE_VOBS ) ) )
        {
            msg_Err( p_demux, "cannot open title (VTS_%02d_1.VOB)",
                     p_vmg->tt_srpt->title[i_title].title_set_nr );
            return VLC_EGENERIC;
        }

        //IfoPrintTitle( p_demux );

        /*
         * Destroy obsolete ES by reinitializing program 0
         * and find all ES in title with ifo data
         */

        for( int i = 0; i < PS_TK_COUNT; i++ )
        {
            ps_track_t *tk = &p_sys->tk[i];
            if( tk->b_configured )
            {
                es_format_Clean( &tk->fmt );
                if( tk->es ) es_out_Del( p_demux->out, tk->es );
            }
            tk->b_configured = false;
        }

        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

        if( p_sys->cur_title != i_title )
        {
            p_sys->updates |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_title = i_title;
            p_sys->cur_chapter = 0;
        }

        /* TODO: re-add angles */


        DvdReadESNew( p_demux, 0xe0, 0 ); /* Video, FIXME ? */
        const video_attr_t *p_attr = &p_vts->vtsi_mat->vts_video_attr;
        int i_video_height = p_attr->video_format != 0 ? 576 : 480;
        int i_video_width;
        switch( p_attr->picture_size )
        {
        case 0:
            i_video_width = 720;
            break;
        case 1:
            i_video_width = 704;
            break;
        case 2:
            i_video_width = 352;
            break;
        default:
        case 3:
            i_video_width = 352;
            i_video_height /= 2;
            break;
        }
        switch( p_attr->display_aspect_ratio )
        {
        case 0:
            p_sys->i_sar_num = 4 * i_video_height;
            p_sys->i_sar_den = 3 * i_video_width;
            break;
        case 3:
            p_sys->i_sar_num = 16 * i_video_height;
            p_sys->i_sar_den =  9 * i_video_width;
            break;
        default:
            p_sys->i_sar_num = 0;
            p_sys->i_sar_den = 0;
            break;
        }

        /* Audio ES, in the order they appear in the .ifo */
        for( int i = 1; i <= p_vts->vtsi_mat->nr_of_vts_audio_streams; i++ )
        {
            int i_position = 0;
            uint16_t i_id;

            //IfoPrintAudio( p_demux, i );

            /* Audio channel is active if first byte is 0x80 */
            uint16_t i_audio_control = p_pgc->audio_control[i-1];
            if( i_audio_control & 0x8000 )
            {
                i_position = ( i_audio_control & 0x7F00 ) >> 8;

                msg_Dbg( p_demux, "audio position  %d", i_position );
                switch( p_vts->vtsi_mat->vts_audio_attr[i - 1].audio_format )
                {
                case 0x00: /* A52 */
                    i_id = (0x80 + i_position) | PS_PACKET_ID_MASK_VOB;
                    break;
                case 0x02:
                case 0x03: /* MPEG audio */
                    i_id = 0xc000 + i_position;
                    break;
                case 0x04: /* LPCM */
                    i_id = (0xa0 + i_position) | PS_PACKET_ID_MASK_VOB;
                    break;
                case 0x06: /* DTS */
                    i_id = (0x88 + i_position) | PS_PACKET_ID_MASK_VOB;
                    break;
                default:
                    i_id = 0;
                    msg_Err( p_demux, "unknown audio type %.2x",
                        p_vts->vtsi_mat->vts_audio_attr[i - 1].audio_format );
                }

                DvdReadESNew( p_demux, i_id, p_sys->p_vts_file->vtsi_mat->
                       vts_audio_attr[i - 1].lang_code );
            }
        }

        static_assert(sizeof(p_sys->clut) == sizeof(p_pgc->palette),
                      "mismatch CLUT size");
        memcpy( p_sys->clut, p_pgc->palette, sizeof( p_sys->clut ) );

        /* Sub Picture ES */
        for( int i = 1; i <= p_vts->vtsi_mat->nr_of_vts_subp_streams; i++ )
        {
            int i_position = 0;
            uint16_t i_id;
            uint32_t i_spu_control = p_pgc->subp_control[i-1];

            //IfoPrintSpu( p_sys, i );
            msg_Dbg( p_demux, "spu %d 0x%02x", i, i_spu_control );

            if( i_spu_control & 0x80000000 )
            {
                /*  there are several streams for one spu */
                if( p_vts->vtsi_mat->vts_video_attr.display_aspect_ratio )
                {
                    /* 16:9 */
                    switch( p_vts->vtsi_mat->vts_video_attr.permitted_df )
                    {
                    case 1: /* letterbox */
                        i_position = i_spu_control & 0xff;
                        break;
                    case 2: /* pan&scan */
                        i_position = ( i_spu_control >> 8 ) & 0xff;
                        break;
                    default: /* widescreen */
                        i_position = ( i_spu_control >> 16 ) & 0xff;
                        break;
                    }
                }
                else
                {
                    /* 4:3 */
                    i_position = ( i_spu_control >> 24 ) & 0x7F;
                }

                i_id = (0x20 + i_position) | PS_PACKET_ID_MASK_VOB;

                DvdReadESNew( p_demux, i_id, p_sys->p_vts_file->vtsi_mat->
                       vts_subp_attr[i - 1].lang_code );
            }
        }

    }
    else if( i_title != -1 && i_title != p_sys->i_title )

    {
        return VLC_EGENERIC; /* Couldn't set title */
    }

    /*
     * Chapter selection
     */

    if( i_chapter >= 0 && i_chapter < p_sys->i_chapters )
    {
        const ifo_handle_t *p_vts = p_sys->p_vts_file;
        pgc_id = p_vts->vts_ptt_srpt->title[
                     p_sys->i_ttn - 1].ptt[i_chapter].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[
                  p_sys->i_ttn - 1].ptt[i_chapter].pgn;

        const pgc_t *p_pgc =
            p_sys->p_cur_pgc = p_vts->vts_pgcit->pgci_srp[pgc_id - 1].pgc;
        if( p_pgc->cell_playback == NULL )
            return VLC_EGENERIC; /* Couldn't set chapter */

        p_sys->i_cur_cell = p_pgc->program_map[pgn - 1] - 1;
        p_sys->i_chapter = i_chapter;
        DvdReadFindCell( p_demux );

        p_sys->i_title_offset = 0;
        for( int i = p_sys->i_title_start_cell; i < p_sys->i_cur_cell; i++ )
        {
            p_sys->i_title_offset += p_pgc->cell_playback[i].last_sector -
                p_pgc->cell_playback[i].first_sector + 1;
        }

        p_sys->i_pack_len = 0;
        p_sys->i_next_vobu = p_sys->i_cur_block =
            p_pgc->cell_playback[p_sys->i_cur_cell].first_sector;

        if( p_sys->cur_chapter != i_chapter )
        {
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_chapter = i_chapter;
        }
    }
    else if( i_chapter != -1 )

    {
        return VLC_EGENERIC; /* Couldn't set chapter */
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DemuxTitles: get the titles/chapters or group/tracks structure
 *****************************************************************************/
static void DvdReadDemuxTitles( demux_t *p_demux, int *pi_angle )
{
    VLC_UNUSED( pi_angle );

    demux_sys_t *p_sys = p_demux->p_sys;
    input_title_t *t;
    seekpoint_t *s;

    const int32_t i_titles = p_sys->p_vmg_file->tt_srpt->nr_of_srpts;
    msg_Dbg( p_demux, "number of titles: %d", i_titles );

    for( int i = 0; i < i_titles; i++ )
    {
        const int32_t i_chapters = p_sys->p_vmg_file->tt_srpt->title[i].nr_of_ptts;

        msg_Dbg( p_demux, "title %d has %d chapters", i, i_chapters );

        t = vlc_input_title_New();
        if( unlikely( !t ) )
            return;

        for( int j = 0; j < __MAX( i_chapters, 1 ); j++ )
        {
            s = vlc_seekpoint_New();
            if( unlikely( !s ) )
            {
                vlc_input_title_Delete( t );
                return;
            }
            TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
        }
        TAB_APPEND( p_sys->i_titles, p_sys->titles, t );
    }
}

const dvdread_ops_t DvdReadVideoOps = {
    .set_area = DvdReadSetArea,
    .seek = DvdReadSeek,
    .find_cell = DvdReadFindCell,
    .title_length = DvdReadTitleLength,
    .demux_titles = DvdReadDemuxTitles,
    .timeline_base = DvdReadTimelineBase,
    .time_to_seek_offset = DvdReadTimeToSeekOffset,
    .ps_source = DvdReadPsSource,
    .adjust_anchor = DvdReadAdjustAnchor,
    .requires_vts = true,
};
