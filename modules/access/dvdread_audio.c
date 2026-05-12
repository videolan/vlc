/*****************************************************************************
 * dvdread_audio.c : DVD-Audio support for the dvdread input module
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

#ifdef DVDREAD_HAS_DVDAUDIO
typedef struct demux_sys_t demux_sys_t;

/* function different enough to warrant a seperate implementation*/
static int DvdAudioReadSetArea( demux_t *p_demux, int i_title, int i_track,
                         int i_angle )
{
    VLC_UNUSED( i_angle );

    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_title >= 0 && i_title < p_sys->i_titles &&
        i_title != p_sys->i_title )
    {
        if( p_sys->p_title != NULL )
        {
            DVDCloseFile( p_sys->p_title );
            p_sys->p_title = NULL;
        }
        p_sys->i_title = i_title;
        DvdReadResetCellTs( p_sys );

        /* reusing p_vmg variable for p_amg */
        const ifo_handle_t *p_vmg = p_sys->p_vmg_file;

        /*
         *  We have to load all title information
         */
        msg_Dbg( p_demux, "open ATS %d, for group %d",
                 p_vmg->info_table_second_sector->tracks_info[i_title].group_property, i_title+ 1 );

        /* Ifo ats */
        /* reusing p_vts variable for p_ats */
        if( p_sys->p_vts_file != NULL )
            ifoClose( p_sys->p_vts_file );
        if( !( p_sys->p_vts_file = ifoOpen( p_sys->p_dvdread,
               p_vmg->info_table_second_sector->tracks_info[i_title].group_property ) ) )
        {
            msg_Err( p_demux, "fatal error in ats ifo" );
            return VLC_EGENERIC;
        }

        const ifo_handle_t *p_vts = p_sys->p_vts_file;

        /* Title position inside the selected ats, i_title is the overall title number */
        p_sys->i_ttn = p_vmg->info_table_second_sector->tracks_info[i_title].title_property;

        const atsi_title_record_t *atsi_title_table=
            p_sys->p_title_table = &p_vts->atsi_title_table->atsi_title_row_tables[p_sys->i_ttn-1];

        p_sys->i_chapter = 0;
        p_sys->i_chapters =
            p_vts->atsi_title_table->atsi_title_row_tables[p_sys->i_ttn - 1].nr_pointer_records;
        if( p_sys->i_chapters <= 0 )
        {
            msg_Err( p_demux, "invalid chapter count for title %d", i_title );
            return VLC_EGENERIC;
        }

        /* there are no cells in dvd audio, bellow the start and end sectors of the title set. */
        p_sys->i_title_start_block = atsi_title_table->atsi_track_pointer_rows[0].start_sector;
        p_sys->i_title_end_block = atsi_title_table->atsi_track_pointer_rows[p_sys->i_chapters - 1].end_sector;

        p_sys->i_title_blocks = p_sys->i_title_end_block - p_sys->i_title_start_block + 1;
        msg_Dbg( p_demux, "title %d ttn %d start %d end %d blocks: %u",
                 i_title, p_sys->i_ttn,
                 p_sys->i_title_start_block, p_sys->i_title_end_block,
                 p_sys->i_title_blocks );

        if( p_sys->i_chapters > 0 )
        {
            input_title_t *p_title = p_sys->titles[i_title];
            if( p_title->seekpoint && p_sys->i_title_blocks > 0 )
            {
                const vlc_tick_t title_length = FROM_SCALE_NZ(
                    p_sys->p_title_table->length_pts );
                const uint32_t first_sector =
                    p_sys->p_title_table->atsi_track_pointer_rows[0].start_sector;

                p_title->i_length = title_length;

                const int max_sp = __MIN( p_title->i_seekpoint,
                                            p_sys->i_chapters );
                /* offset relative to title start */
                for( int sj = 0; sj < max_sp; sj++ )
                {
                    const uint32_t sj_sector =
                        p_sys->p_title_table->atsi_track_pointer_rows[sj].start_sector;
                    const uint32_t offset = sj_sector > first_sector
                        ? sj_sector - first_sector : 0;
                    if( title_length > 0 &&
                        offset > UINT64_MAX / (uint64_t)title_length )
                    {
                        msg_Err( p_demux, "chapter offset multiply overflow" );
                        return VLC_EGENERIC;
                    }
                    /* widen for the multiply, product fits uint64_t */
                    p_title->seekpoint[sj]->i_time_offset =
                        (vlc_tick_t)( (uint64_t)offset * title_length /
                                      p_sys->i_title_blocks );
                }
            }
        }

        /* The structure of DVD-A discs seems to be the following
         * each ATS IFO-> is a GROUP -> Contains multiple titles, Span across one or more AOBs -> each title contains multiple tracks or "Trackpoints",
         *
         * trackpoints are counted as tracks. They have records in the pointer table, and nr_pointer_records will include trackpoints. this is why it is different to nr_tracks*/

        /*
         * Set properties for current track, consider i_chapter as tracks/trackpoints
         */
       /*
         * We've got enough info, time to open the ATS AOB, indexed by "group_property"
         */
        if( !( p_sys->p_title = DVDOpenFile( p_sys->p_dvdread,
               p_vmg->info_table_second_sector->tracks_info[i_title].group_property,
               DVD_READ_TITLE_VOBS ) ) )
        {
            msg_Err( p_demux, "cannot open title (ATS_%02d_1.AOB)",
                     p_vmg->info_table_second_sector->tracks_info[i_title].group_property );
            return VLC_EGENERIC;
        }

        /* for now we are using the "second table, which includes no VOB tracks, assuming that if a user wants to open the video side he will select the standard DVD option"*/

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

    /*
     * Chapter selection
     */
    if( i_track >= 0 && i_track < p_sys->i_chapters )
    {
        p_sys->i_chapter = i_track;

        p_sys->i_title_offset =  p_sys->p_title_table->atsi_track_pointer_rows[i_track].start_sector
            - p_sys->p_title_table->atsi_track_pointer_rows[0].start_sector;

        msg_Dbg(p_demux, "Title Offset: %d", p_sys->i_title_offset);

        p_sys->i_pack_len = 0;
        /* current block relative to start of title*/
        p_sys->i_cur_block=p_sys->p_title_table->atsi_track_pointer_rows[i_track].start_sector;

        if( p_sys->cur_chapter != i_track)
        {
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_chapter = i_track;
        }
    }
    else if( i_track != -1 )
    {

        msg_Dbg( p_demux, "Couldn't set chapter" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int DvdAudioReadSeek( demux_t *p_demux, uint32_t i_block_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_chapter;
    uint32_t i_seek_blocks = 0;

    /* set pack length */
    /* set current block*/

    /* find current chapter */
    for ( i_chapter = 0 ; i_chapter < p_sys->i_chapters; i_chapter++ ) {
        uint32_t start = p_sys->p_title_table->atsi_track_pointer_rows[i_chapter].start_sector;
        uint32_t end = p_sys->p_title_table->atsi_track_pointer_rows[i_chapter].end_sector;
        uint32_t chapter_len = end - start + 1;

        if ( i_block_offset < i_seek_blocks + chapter_len )
            break;

        i_seek_blocks += chapter_len;
    }

    /* exit if i_chapter is invalid */
    if( i_chapter >= p_sys->i_chapters )
        return VLC_EGENERIC;

    if( p_sys->cur_chapter != i_chapter )
    {
        p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
        p_sys->cur_chapter = i_chapter;
    }

    const uint32_t start_sector =
        p_sys->p_title_table->atsi_track_pointer_rows[i_chapter].start_sector;
    /* i_block_offset title-relative, i_seek_blocks is chapter start */
    const uint32_t within_chapter = i_block_offset - i_seek_blocks;
    p_sys->i_cur_block = start_sector + within_chapter;

    if( p_sys->i_cur_block <= p_sys->i_title_end_block )
        p_sys->i_pack_len = p_sys->i_title_end_block - p_sys->i_cur_block + 1;
    else
        p_sys->i_pack_len = 0;
    p_sys->i_title_offset = i_block_offset;
    p_sys->i_chapter = i_chapter;
    DvdReadResetCellTs( p_sys );

    return VLC_SUCCESS;
}

static void DvdAudioReadFindCell( demux_t *p_demux )
{
    VLC_UNUSED( p_demux );
}

static vlc_tick_t DvdAudioReadTitleLength( const demux_sys_t *p_sys )
{
    return FROM_SCALE_NZ( p_sys->p_title_table->length_pts );
}

static bool DvdAudioReadTimelineBase( const demux_sys_t *p_sys, vlc_tick_t *base )
{
    if( !p_sys->p_title_table || p_sys->i_title_blocks == 0 )
        return false;

    const uint32_t first_sector =
        p_sys->p_title_table->atsi_track_pointer_rows[0].start_sector;
    const uint32_t current_sector = p_sys->i_cur_block > 0 ? (uint32_t)p_sys->i_cur_block : 0;
    const uint32_t current_offset = current_sector > first_sector ? current_sector - first_sector : 0;
    const vlc_tick_t title_length = FROM_SCALE_NZ( p_sys->p_title_table->length_pts );
    *base = (vlc_tick_t)( current_offset * title_length / p_sys->i_title_blocks );
    return true;
}

static uint32_t DvdAudioReadTimeToSeekOffset( const demux_sys_t *p_sys, vlc_tick_t time,
                                              uint32_t i_block_offset )
{
    VLC_UNUSED( p_sys );
    VLC_UNUSED( time );
    return i_block_offset;
}

static int DvdAudioReadPsSource( void )
{
    return PS_SOURCE_AOB;
}

static void DvdAudioReadAdjustAnchor( demux_sys_t *p_sys, block_t *pkt )
{
    VLC_UNUSED( p_sys );
    VLC_UNUSED( pkt );
}

static void DvdAudioReadDemuxTitles( demux_t *p_demux, int *pi_angle )
{
    VLC_UNUSED( pi_angle );

    demux_sys_t *p_sys = p_demux->p_sys;
    input_title_t *t = NULL;
    seekpoint_t *s;
    ifo_handle_t *p_ats_ifo = NULL;

    /* Find out number of titles/chapters */
    const int32_t i_titles = p_sys->p_vmg_file->info_table_second_sector->nr_of_titles;
    msg_Dbg( p_demux, "number of titles: %d", i_titles );

    for( int i = 0; i < i_titles; i++ )
    {
        const int32_t i_chapters =
            p_sys->p_vmg_file->info_table_second_sector->tracks_info[i].nr_chapters_in_title;
        msg_Dbg( p_demux, "title %d has %d chapters", i, i_chapters );

        t = vlc_input_title_New();
        if( unlikely( !t ) )
            return;

        const atsi_track_timestamp_t *p_track_ts = NULL;
        uint8_t i_track_ts = 0;
        const track_info_t * const p_track_info =
            &p_sys->p_vmg_file->info_table_second_sector->tracks_info[i];
        t->i_length = FROM_SCALE_NZ( p_track_info->len_audio_zone_pts );

        p_ats_ifo = NULL;
        p_ats_ifo = ifoOpen( p_sys->p_dvdread, p_track_info->group_property );
        if( p_ats_ifo != NULL && p_ats_ifo->atsi_title_table != NULL
         && p_track_info->title_property > 0
         && p_track_info->title_property <= p_ats_ifo->atsi_title_table->nr_titles )
        {
            const atsi_title_record_t * const p_title_rec =
                &p_ats_ifo->atsi_title_table->atsi_title_row_tables[p_track_info->title_property - 1];
            p_track_ts = p_title_rec->atsi_track_timestamp_rows;
            i_track_ts = p_title_rec->nr_tracks;
        }

        for( int j = 0; j < __MAX( i_chapters, 1 ); j++ )
        {
            s = vlc_seekpoint_New();
            if( unlikely( !s ) )
                goto fail;
            if( p_track_ts != NULL && j < i_track_ts )
                s->i_time_offset = FROM_SCALE_NZ( (uint64_t)p_track_ts[j].first_pts_of_track );
            TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
        }

        if( p_ats_ifo != NULL )
        {
            ifoClose( p_ats_ifo );
            p_ats_ifo = NULL;
        }

        TAB_APPEND( p_sys->i_titles, p_sys->titles, t );
        t = NULL;
    }
    return;

fail:
    if( p_ats_ifo != NULL )
        ifoClose( p_ats_ifo );
    if( t != NULL )
        vlc_input_title_Delete( t );
}

const dvdread_ops_t DvdAudioReadOps = {
    .set_area = DvdAudioReadSetArea,
    .seek = DvdAudioReadSeek,
    .find_cell = DvdAudioReadFindCell,
    .title_length = DvdAudioReadTitleLength,
    .demux_titles = DvdAudioReadDemuxTitles,
    .timeline_base = DvdAudioReadTimelineBase,
    .time_to_seek_offset = DvdAudioReadTimeToSeekOffset,
    .ps_source = DvdAudioReadPsSource,
    .adjust_anchor = DvdAudioReadAdjustAnchor,
    .requires_vts = true,
};

int OpenAudio( vlc_object_t *p_this )
{
    return OpenCommonDvdread( p_this, DVD_A, NULL );
}
#else
int OpenAudio( vlc_object_t *p_this )
{
    VLC_UNUSED( p_this );
    return VLC_ENOTSUP;
}
#endif
