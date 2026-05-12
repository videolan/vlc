/*****************************************************************************
 * dvdread_vr.c : DVD-VideoRecording support for the dvdread input module
 *****************************************************************************
 * Copyright (C) 2024-2025 VLC authors and VideoLAN
 *
 * Authors: Saifelden Mohamed Ismail <saifeldenmi@gmail.com>
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
#include <vlc_arrays.h>
#include <vlc_charset.h>

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
typedef struct demux_sys_t demux_sys_t;
#define DVDVR_FALLBACK_SECTORS_PER_VOBU 512
#define DVDVR_EARLY_SNAP_VOBU_WINDOW 4

/* estimated sector span of a vr program's vobu map
 * dvd-vr has no flat vobu address map like VOBU_ADMAP in dvd-video
 * instead time_info entries store vobu_adr as sector offset and vobu_entn
 * as vobu index so the last entry gives a sectors-per-vobu ratio
 * falls back to a per-program or constant ratio when no usable entries exist
 * returns 0 when the map has no VOBUs */
uint32_t DvdVRGetProgramSectorSpan( const demux_sys_t *p_sys,
                                    const vobu_map_t *map )
{
    if( map->nr_of_time_info > 0 )
    {
        const uint32_t last_adr = map->time_infos[map->nr_of_time_info - 1].vobu_adr;
        const uint32_t last_entn = map->time_infos[map->nr_of_time_info - 1].vobu_entn;
        if( last_entn > 1 )
            return (uint32_t)( (uint64_t)last_adr * map->nr_of_vobu_info /
                               ( last_entn - 1 ) );
    }

    uint32_t sects_per_vobu = DVDVR_FALLBACK_SECTORS_PER_VOBU;
    for( int p = 0; p < p_sys->pgc_gi->nr_of_programs; p++ )
    {
        const vobu_map_t *m = &p_sys->pgc_gi->pgi[p].map;
        if( m->nr_of_time_info > 0 &&
            m->time_infos[m->nr_of_time_info - 1].vobu_entn > 1 )
        {
            sects_per_vobu = m->time_infos[m->nr_of_time_info - 1].vobu_adr /
                             ( m->time_infos[m->nr_of_time_info - 1].vobu_entn - 1 );
            break;
        }
    }

    return map->nr_of_vobu_info * sects_per_vobu;
}

static vlc_tick_t DvdVRProgramDuration( const pgi_t *pgi )
{
    if( pgi->header.vob_v_e_ptm.ptm <= pgi->header.vob_v_s_ptm.ptm )
        return 0;

    const uint32_t delta_ptm =
        pgi->header.vob_v_e_ptm.ptm - pgi->header.vob_v_s_ptm.ptm;
    return FROM_SCALE_NZ( delta_ptm );
}

/* vobu_adr walked in order by callers, must be monotone */
static bool DvdVRTimeInfosSane( const vobu_map_t *map, uint32_t total_sectors )
{
    if( map->nr_of_time_info == 0 || total_sectors == 0 )
        return false;

    uint32_t prev = 0;
    for( int i = 0; i < map->nr_of_time_info; i++ )
    {
        const uint32_t adr = map->time_infos[i].vobu_adr;
        if( adr >= total_sectors )
            return false;
        if( adr < prev )
            return false;
        prev = adr;
    }
    return true;
}

static uint32_t DvdVRReadTimeToVobuOffset( const demux_sys_t *p_sys, vlc_tick_t t )
{
    if( p_sys->type != DVD_VR || !p_sys->pgc_gi || !p_sys->ud_pgcit ||
        p_sys->i_title_start_cell < 0 || p_sys->i_title_end_cell <= p_sys->i_title_start_cell )
        return 0;

    vlc_tick_t acc = 0;
    uint32_t vobu_acc = 0;

    for( int ci = p_sys->i_title_start_cell; ci < p_sys->i_title_end_cell; ci++ )
    {
        uint16_t srpn = p_sys->ud_pgcit->m_c_gi[ci].m_vobi_srpn;
        if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
            return vobu_acc;

        const pgi_t *pgi = &p_sys->pgc_gi->pgi[srpn - 1];
        const uint32_t vobus = pgi->map.nr_of_vobu_info;
        if( vobus == 0 )
            continue;

        const vlc_tick_t dur = DvdVRProgramDuration( pgi );
        if( dur == 0 )
            continue;
        if( acc + dur > t )
        {
            const vlc_tick_t within = t - acc;
            const uint32_t within_vobu = (uint32_t)(within * vobus / dur);
            return vobu_acc + within_vobu;
        }

        acc += dur;
        vobu_acc += vobus;
    }

    return p_sys->i_title_blocks > 0 ? p_sys->i_title_blocks - 1 : 0;
}

static int DvdVRReadSetArea( demux_t *p_demux, int i_title, int i_chapter,
                      int i_angle )
{
    VLC_UNUSED( i_angle );
    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_title >= 0 && (unsigned)i_title >= p_sys->ud_pgcit->nr_of_pgci )
        return VLC_EGENERIC;

    /* user made title selection */
    if( i_title >= 0 && i_title < p_sys->i_titles &&
        i_title != p_sys->i_title )
    {
        int i_start_cell, i_end_cell;

        if( p_sys->p_title != NULL )
            DVDCloseFile( p_sys->p_title );
        if( !( p_sys->p_title = DVDOpenFile( p_sys->p_dvdread, 0, DVD_READ_TITLE_VOBS ) ) )
        {
            msg_Err( p_demux, "cannot open VR_MOVIE.VRO");
            return VLC_EGENERIC;
        }

        uint16_t fpid = p_sys->ud_pgcit->ud_pgci_items[i_title].first_prog_id;
        if( fpid == 0 || fpid == 0xFFFF
            || fpid > p_sys->pgc_gi->nr_of_programs )
        {
            msg_Warn( p_demux, "invalid first_prog_id %u for title %d",
                      fpid, i_title );
            return VLC_EGENERIC;
        }
        i_start_cell = p_sys->i_title_start_cell = fpid - 1;

        p_sys->i_cur_cell = i_start_cell;
        i_end_cell = p_sys->i_title_end_cell = p_sys->ud_pgcit->ud_pgci_items[i_title].first_prog_id - 1 +
           p_sys->ud_pgcit->ud_pgci_items[i_title].nr_of_programs;

        if( i_end_cell > p_sys->pgc_gi->nr_of_programs )
        {
            msg_Warn( p_demux, "invalid end cell %d for title %d",
                      i_end_cell, i_title );
            return VLC_EGENERIC;
        }

        p_sys->i_chapters = 0;
        for (int c = i_start_cell; c < i_end_cell; c++)
        {
            m_c_gi_t *cell = &p_sys->ud_pgcit->m_c_gi[c];
            p_sys->i_chapters += cell->c_epi_n ? cell->c_epi_n : 1;
        }

        /* which program we should search for our timestamp */
        uint16_t m_vobi_srpn =  p_sys->ud_pgcit->m_c_gi[i_start_cell].m_vobi_srpn;
        if( m_vobi_srpn == 0 || m_vobi_srpn > p_sys->pgc_gi->nr_of_programs )
        {
            msg_Err( p_demux, "invalid m_vobi_srpn %u for cell %d",
                     m_vobi_srpn, i_start_cell );
            return VLC_EGENERIC;
        }

        /* find the corresponding address */
        p_sys->p_cur_pgi = &p_sys->pgc_gi->pgi[m_vobi_srpn - 1];

        uint32_t vob_offset = p_sys->p_cur_pgi->map.vob_offset;

        p_sys->i_cur_block = vob_offset;

        p_sys->i_title_blocks = 0;
        for( int c = i_start_cell; c < i_end_cell; c++ )
        {
            uint16_t srpn = p_sys->ud_pgcit->m_c_gi[c].m_vobi_srpn;
            if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
            {
                msg_Warn( p_demux, "invalid m_vobi_srpn %" PRIu16 " in title %d",
                          srpn, i_title );
                return VLC_EGENERIC;
            }
            p_sys->i_title_blocks += p_sys->pgc_gi->pgi[srpn - 1].map.nr_of_vobu_info;
        }
        p_sys->i_title_offset = 0;
        p_sys->i_pack_len = 0;
        DvdReadResetCellTs( p_sys );

        p_sys->i_title = i_title;

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

        if( p_sys->cur_title != i_title)
        {
            p_sys->updates |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_title = i_title;
            p_sys->cur_chapter = 0;
        }

    }
    else if( i_title != -1 && i_title != p_sys->i_title )
    {
        return VLC_EGENERIC; /* Couldn't set title */
    }

    /* Search for chapter */
    if( i_chapter >= 0 && i_chapter < p_sys->i_chapters )
    {
        int i, j;
        int accum_chapter = 0;
        uint32_t chapter_ptm;
        uint32_t chapter_program;
        int found = 0;

        /* find the correct entry point */
        for (i = p_sys->i_title_start_cell;
            i < p_sys->i_title_end_cell && !found; i++)
        {
            int n_ep = p_sys->ud_pgcit->m_c_gi[i].c_epi_n;
            if( n_ep == 0 )
            {
                /* cell has no explicit entry points — treat as one chapter */
                if( accum_chapter == i_chapter )
                {
                    chapter_ptm = p_sys->ud_pgcit->m_c_gi[i].c_v_s_ptm.ptm;
                    chapter_program = p_sys->ud_pgcit->m_c_gi[i].m_vobi_srpn;
                    found = 1;
                    break;
                }
                accum_chapter++;
            }
            else
            {
                for (j = 0; j < n_ep; j++)
                {
                    if (accum_chapter == i_chapter)
                    {
                        chapter_ptm = p_sys->ud_pgcit->m_c_gi[i].m_c_epi[j].ep_ptm.ptm;
                        chapter_program = p_sys->ud_pgcit->m_c_gi[i].m_vobi_srpn;
                        found = 1;
                        break;
                    }
                    accum_chapter++;
                }
            }
        }
        if (!found)
            return VLC_EGENERIC;

        if( chapter_program == 0 || chapter_program > p_sys->pgc_gi->nr_of_programs )
            return VLC_EGENERIC;

        const pgi_t *chapter_pgi = &p_sys->pgc_gi->pgi[chapter_program - 1];
        const vobu_map_t *map = &chapter_pgi->map;

        const int64_t relative_ptm = (int64_t)chapter_ptm -
            (int64_t)chapter_pgi->header.vob_v_s_ptm.ptm;
        const int64_t total_pts = (int64_t)chapter_pgi->header.vob_v_e_ptm.ptm -
            (int64_t)chapter_pgi->header.vob_v_s_ptm.ptm;
        const uint32_t total_sectors = DvdVRGetProgramSectorSpan( p_sys, map );

        uint32_t within_vobu = 0;
        if( total_pts > 0 && relative_ptm >= 0 && total_sectors > 0 )
        {
            if( map->nr_of_vobu_info > 0 )
                within_vobu = (uint32_t)( relative_ptm *
                              map->nr_of_vobu_info / total_pts );
        }
        if( map->nr_of_vobu_info > 0 && within_vobu >= map->nr_of_vobu_info )
            within_vobu = map->nr_of_vobu_info - 1;

        /* time_infos sparse, snap to entry only near program start */
        uint32_t snap_sector = 0;
        if( within_vobu < DVDVR_EARLY_SNAP_VOBU_WINDOW &&
            DvdVRTimeInfosSane( map, total_sectors ) )
        {
            int idx = 0;
            while( idx + 1 < map->nr_of_time_info &&
                   map->time_infos[idx + 1].vobu_entn <= within_vobu )
                idx++;
            snap_sector = map->time_infos[idx].vobu_adr;
        }

        const uint32_t chapter_offset = map->vob_offset + snap_sector;
        p_sys->i_cur_block = chapter_offset;
        p_sys->i_pack_len = total_sectors > snap_sector ? total_sectors - snap_sector : 1;
        p_sys->i_chapter = i_chapter;
        p_sys->cur_chapter = i_chapter;
        DvdReadResetCellTs( p_sys );
        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

        p_sys->i_cur_cell = -1;
        for( int ci = p_sys->i_title_start_cell;
             ci < p_sys->i_title_end_cell; ci++ )
        {
            if( p_sys->ud_pgcit->m_c_gi[ci].m_vobi_srpn == chapter_program )
            {
                p_sys->i_cur_cell = ci;
                break;
            }
        }
        if( p_sys->i_cur_cell < p_sys->i_title_start_cell )
            return VLC_EGENERIC;

        p_sys->i_title_offset = 0;
        for( int ci = p_sys->i_title_start_cell;
             ci < p_sys->i_cur_cell; ci++ )
        {
            uint16_t s = p_sys->ud_pgcit->m_c_gi[ci].m_vobi_srpn;
            p_sys->i_title_offset += p_sys->pgc_gi->pgi[s - 1].map.nr_of_vobu_info;
        }
        /* add within-program vobu offset for position accuracy */
        p_sys->i_title_offset += within_vobu;

        return VLC_SUCCESS;
    }
    else if( i_chapter != -1 )
    {
        return VLC_EGENERIC; /* Couln't set chapter */
    }

    return VLC_SUCCESS;
}

static void DvdVRFindCell( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->i_next_cell = p_sys->i_cur_cell + 1;

    if( p_sys->i_cur_cell >= p_sys->i_title_end_cell )
        return;

    /* labelled as entry point in dvd-vr */
    int chapter = 0;

    /* see if we've reached the next entry point */
    ud_pgci_t *psi = &p_sys->ud_pgcit->ud_pgci_items[p_sys->i_title];
    int cell_base = psi->first_prog_id - 1;

    for( int i = 0; i < psi->nr_of_programs; i++ )
    {
        int cell_idx = cell_base + i;
        m_c_gi_t *cell = &p_sys->ud_pgcit->m_c_gi[cell_idx];

        if ( cell_idx == p_sys->i_cur_cell )
            break;

        /* entry points are chapters, count cell as 1 if no entry points */
        chapter += cell->c_epi_n ? cell->c_epi_n : 1;

    }

    if( chapter >= p_sys->i_chapters )
        chapter = p_sys->i_chapters > 0 ? p_sys->i_chapters - 1 : 0;

    if( chapter != p_sys->i_chapter )
    {
        p_sys->i_chapter = chapter;
        if( p_sys->cur_chapter != chapter )
        {
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_chapter = chapter;
        }
    }

}

static vlc_tick_t DVDVRGetTitleLength( pgc_gi_t *pgc_gi, ud_pgcit_t *ud_pgcit, int program)
{
    if( program < 0 || (unsigned)program >= ud_pgcit->nr_of_pgci )
        return 0;

    uint64_t length_ptm = 0;
    uint16_t first_prog_id = ud_pgcit->ud_pgci_items[program].first_prog_id;
    uint16_t nr_of_programs = ud_pgcit->ud_pgci_items[program].nr_of_programs;

    if( first_prog_id == 0 || first_prog_id > pgc_gi->nr_of_programs ||
        nr_of_programs == 0 ||
        first_prog_id - 1 + nr_of_programs > pgc_gi->nr_of_programs )
        return 0;

    for ( int i = 0; i < nr_of_programs; i++ )
        length_ptm += pgc_gi->pgi[first_prog_id -1 + i].header.vob_v_e_ptm.ptm
            - pgc_gi->pgi[first_prog_id -1 + i].header.vob_v_s_ptm.ptm;

    return FROM_SCALE_NZ( length_ptm );
}

static int DvdVRReadSeek( demux_t *p_demux, uint32_t i_block_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_chapter = 0;
    uint32_t vobu_accum = 0;
    bool found_cell = false;

    int cell_base = p_sys->ud_pgcit->ud_pgci_items[p_sys->i_title].first_prog_id - 1;
    int nr = p_sys->ud_pgcit->ud_pgci_items[p_sys->i_title].nr_of_programs;

    for( int c = 0; c < nr; c++ )
    {
        int cell_idx = cell_base + c;
        uint16_t srpn = p_sys->ud_pgcit->m_c_gi[cell_idx].m_vobi_srpn;
        if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
            return VLC_EGENERIC;
        uint16_t cell_vobus = p_sys->pgc_gi->pgi[srpn - 1].map.nr_of_vobu_info;

        if( i_block_offset < vobu_accum + cell_vobus )
        {
            p_sys->i_cur_cell = cell_idx;
            found_cell = true;
            break;
        }
        vobu_accum += cell_vobus;

        i_chapter += p_sys->ud_pgcit->m_c_gi[cell_idx].c_epi_n
                   ? p_sys->ud_pgcit->m_c_gi[cell_idx].c_epi_n : 1;
    }

    /* seek past title end: snap to last cell */
    if( !found_cell && nr > 0 )
    {
        const int last_cell_idx = cell_base + nr - 1;
        uint16_t srpn = p_sys->ud_pgcit->m_c_gi[last_cell_idx].m_vobi_srpn;
        if( unlikely( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs ) )
            return VLC_EGENERIC;

        const uint16_t last_vobus = p_sys->pgc_gi->pgi[srpn - 1].map.nr_of_vobu_info;
        p_sys->i_cur_cell = last_cell_idx;
        if( last_vobus > 0 && vobu_accum >= last_vobus )
            vobu_accum -= last_vobus;
        else
            vobu_accum = 0;
    }

    if( i_chapter < p_sys->i_chapters &&
        p_sys->cur_chapter != i_chapter )
    {
        p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
        p_sys->cur_chapter = i_chapter;
    }

    if( p_sys->i_cur_cell < cell_base || p_sys->i_cur_cell >= cell_base + nr )
        return VLC_EGENERIC;

    uint16_t cur_srpn = p_sys->ud_pgcit->m_c_gi[p_sys->i_cur_cell].m_vobi_srpn;
    if( unlikely( cur_srpn == 0 || cur_srpn > p_sys->pgc_gi->nr_of_programs ) )
        return VLC_EGENERIC;

    const vobu_map_t *map = &p_sys->pgc_gi->pgi[cur_srpn - 1].map;
    uint32_t within_program = i_block_offset - vobu_accum;
    const uint32_t total_sectors = DvdVRGetProgramSectorSpan( p_sys, map );
    if( map->nr_of_vobu_info > 0 && within_program >= map->nr_of_vobu_info )
        within_program = map->nr_of_vobu_info - 1;

    uint32_t raw_sector = 0;
    if( total_sectors > 0 && map->nr_of_vobu_info > 0 )
    {
        uint64_t scaled_sector = 0;
        if( !ckd_mul( &scaled_sector, (uint64_t)within_program,
                      (uint64_t)total_sectors ) )
            raw_sector = (uint32_t)( scaled_sector / map->nr_of_vobu_info );
    }

    /* time_infos sparse, snap to entry only near program start */
    uint32_t sector = raw_sector;
    if( within_program < DVDVR_EARLY_SNAP_VOBU_WINDOW &&
        DvdVRTimeInfosSane( map, total_sectors ) )
    {
        int idx = 0;
        while( idx + 1 < map->nr_of_time_info &&
               map->time_infos[idx + 1].vobu_adr <= raw_sector )
            idx++;
        sector = map->time_infos[idx].vobu_adr;
    }

    if( sector >= total_sectors )
        sector = total_sectors > 0 ? total_sectors - 1 : 0;

    p_sys->i_cur_block = map->vob_offset + sector;
    p_sys->i_pack_len = total_sectors > sector ? total_sectors - sector : 1;
    p_sys->i_title_offset = i_block_offset;
    p_sys->i_chapter = i_chapter;
    DvdReadResetCellTs( p_sys );
    es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

    return VLC_SUCCESS;
}

static const char* ParseTxtEncoding( uint8_t txt_encoding )
{
    const char* charset = "Unknown";

    switch (txt_encoding) {
    case 0x00: charset = "ASCII"; break;
    case 0x01: charset = "ISO646-JP"; break;
    case 0x10: charset = "JIS_C6220-1969-RO"; break;
    case 0x11: charset = "ISO_8859-1"; break;
    case 0x12: charset = "SHIFT_JIS"; break;
    default:
        charset = "ISO_8859-15";
        break;
    }

    return charset;
}

static vlc_tick_t DvdVRReadTitleLength( const demux_sys_t *p_sys )
{
    if( p_sys->cur_title < 0 || p_sys->cur_title >= p_sys->i_titles )
        return 0;
    return p_sys->titles[p_sys->cur_title]->i_length;
}

static bool DvdVRReadTimelineBase( const demux_sys_t *p_sys, vlc_tick_t *base )
{
    if( p_sys->i_title_start_cell < 0 || p_sys->i_cur_cell < 0 ||
        p_sys->i_title_start_cell > p_sys->i_cur_cell ||
        p_sys->i_cur_cell >= p_sys->i_title_end_cell )
        return false;

    vlc_tick_t cell_dvd_time = 0;
    for( int ci = p_sys->i_title_start_cell;
         ci < p_sys->i_cur_cell && ci < p_sys->i_title_end_cell; ci++ )
    {
        uint16_t srpn = p_sys->ud_pgcit->m_c_gi[ci].m_vobi_srpn;
        if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
            return false;
        const pgi_t *scan_pgi = &p_sys->pgc_gi->pgi[srpn - 1];
        const vlc_tick_t scan_dur = DvdVRProgramDuration( scan_pgi );
        if( scan_dur <= 0 )
            return false;
        cell_dvd_time += scan_dur;
    }

    uint16_t srpn = p_sys->ud_pgcit->m_c_gi[p_sys->i_cur_cell].m_vobi_srpn;
    if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
        return false;

    const pgi_t *pgi = &p_sys->pgc_gi->pgi[srpn - 1];
    const vlc_tick_t program_duration = DvdVRProgramDuration( pgi );
    if( program_duration <= 0 )
        return false;
    const uint32_t total_sectors = DvdVRGetProgramSectorSpan( p_sys, &pgi->map );
    if( total_sectors > 0 )
    {
        int64_t within_sectors = (int64_t)p_sys->i_cur_block - pgi->map.vob_offset;
        if( within_sectors < 0 )
            within_sectors = 0;
        else if( within_sectors >= (int64_t)total_sectors )
            within_sectors = (int64_t)total_sectors - 1;

        vlc_tick_t scaled;
        if( !ckd_mul( &scaled, within_sectors, program_duration ) )
            cell_dvd_time += scaled / total_sectors;
    }

    *base = cell_dvd_time;
    return true;
}

static uint32_t DvdVRReadTimeToSeekOffset( const demux_sys_t *p_sys, vlc_tick_t time,
                                           uint32_t i_block_offset )
{
    VLC_UNUSED( i_block_offset );
    return DvdVRReadTimeToVobuOffset( p_sys, time );
}

static int DvdVRReadPsSource( void )
{
    return PS_SOURCE_VOB;
}

static void DvdVRReadAdjustAnchor( demux_sys_t *p_sys, block_t *pkt )
{
    if( p_sys->cell_ts.dvd == VLC_TICK_INVALID || p_sys->cell_ts.ps == VLC_TICK_INVALID )
        return;

    vlc_tick_t anchor = p_sys->cell_ts.ps;
    if( pkt->i_dts != VLC_TICK_INVALID && pkt->i_dts < anchor )
        anchor = pkt->i_dts;
    if( pkt->i_pts != VLC_TICK_INVALID && pkt->i_pts < anchor )
        anchor = pkt->i_pts;
    if( anchor < p_sys->cell_ts.ps )
        p_sys->cell_ts.ps = anchor;
}

static void DvdVRReadDemuxTitles( demux_t *p_demux, int *pi_angle )
{
    VLC_UNUSED( pi_angle );

    demux_sys_t *p_sys = p_demux->p_sys;
    input_title_t *t;
    seekpoint_t *s;

    const char *disc_charset = ParseTxtEncoding( p_sys->p_vmg_file->rtav_vmgi->txt_encoding );
    const int32_t i_titles = p_sys->ud_pgcit->nr_of_pgci;
    msg_Dbg( p_demux, "number of titles: %d", i_titles );

    for( int i = 0; i < i_titles; i++ )
    {
        int32_t i_chapters = 0;
        uint16_t fpid = p_sys->ud_pgcit->ud_pgci_items[i].first_prog_id;
        if( fpid == 0 || fpid == 0xFFFF || fpid > p_sys->pgc_gi->nr_of_programs )
            continue;

        /* chapters are entry points across all cells */
        int cell_base = fpid - 1;
        int nr = p_sys->ud_pgcit->ud_pgci_items[i].nr_of_programs;
        for( int c = 0; c < nr; c++ )
        {
            m_c_gi_t *cell = &p_sys->ud_pgcit->m_c_gi[cell_base + c];
            i_chapters += cell->c_epi_n ? cell->c_epi_n : 1;
        }

        msg_Dbg( p_demux, "title %d has %d chapters", i, i_chapters );

        t = vlc_input_title_New();
        if( unlikely( !t ) )
            return;

        /* ep_ptm disc-global, subtract title start for relative offset */
        vlc_tick_t title_base = 0;
        if( nr > 0 )
            title_base = FROM_SCALE_NZ( p_sys->ud_pgcit->m_c_gi[cell_base].c_v_s_ptm.ptm );

        char *converted_title = FromCharset(
            disc_charset,
            p_sys->ud_pgcit->ud_pgci_items[i].title,
            strnlen( p_sys->ud_pgcit->ud_pgci_items[i].title,
                     sizeof( p_sys->ud_pgcit->ud_pgci_items[i].title ) )
        );
        if( converted_title && *converted_title )
            t->psz_name = converted_title;
        else
        {
            free( converted_title );
            t->psz_name = strndup( p_sys->ud_pgcit->ud_pgci_items[i].label,
                                   sizeof( p_sys->ud_pgcit->ud_pgci_items[i].label ) );
        }
        t->i_length = DVDVRGetTitleLength( p_sys->pgc_gi, p_sys->ud_pgcit, i );

        /* create one seekpoint per entry point */
        for( int c = 0; c < nr; c++ )
        {
            m_c_gi_t *cell = &p_sys->ud_pgcit->m_c_gi[cell_base + c];
            if( cell->c_epi_n > 0 )
            {
                for( int ep = 0; ep < cell->c_epi_n; ep++ )
                {
                    s = vlc_seekpoint_New();
                    if( unlikely( !s ) )
                        goto fail;

                    vlc_tick_t ep_time =
                        FROM_SCALE_NZ( cell->m_c_epi[ep].ep_ptm.ptm ) - title_base;
                    s->i_time_offset = ep_time > 0 ? ep_time : 0;
                    TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
                }
            }
            else
            {
                s = vlc_seekpoint_New();
                if( unlikely( !s ) )
                    goto fail;

                vlc_tick_t cell_time = FROM_SCALE_NZ( cell->c_v_s_ptm.ptm ) - title_base;
                s->i_time_offset = cell_time > 0 ? cell_time : 0;
                TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
            }
        }

        TAB_APPEND( p_sys->i_titles, p_sys->titles, t );
    }
    return;

fail:
    vlc_input_title_Delete( t );
}

const dvdread_ops_t DvdVRReadOps = {
    .set_area = DvdVRReadSetArea,
    .seek = DvdVRReadSeek,
    .find_cell = DvdVRFindCell,
    .title_length = DvdVRReadTitleLength,
    .demux_titles = DvdVRReadDemuxTitles,
    .timeline_base = DvdVRReadTimelineBase,
    .time_to_seek_offset = DvdVRReadTimeToSeekOffset,
    .ps_source = DvdVRReadPsSource,
    .adjust_anchor = DvdVRReadAdjustAnchor,
    .requires_vts = false,
};

int OpenVideoRecording( vlc_object_t *p_this )
{
    return OpenCommonDvdread( p_this, DVD_VR, NULL );
}
#else
int OpenVideoRecording( vlc_object_t *p_this )
{
    VLC_UNUSED( p_this );
    return VLC_ENOTSUP;
}
#endif
