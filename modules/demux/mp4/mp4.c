/*****************************************************************************
 * mp4.c : MP4 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: mp4.c,v 1.22 2003/04/22 08:51:28 fenrir Exp $
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include <errno.h>
#include <sys/types.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include "codecs.h"
#include "libmp4.h"
#include "mp4.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int    MP4Init    ( vlc_object_t * );
static void __MP4End     ( vlc_object_t * );
static int    MP4Demux   ( input_thread_t * );

/* New input could have something like that... */
static int   MP4Seek     ( input_thread_t *, mtime_t );

#define MP4End(a) __MP4End(VLC_OBJECT(a))

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MP4 demuxer") );
    set_capability( "demux", 242 );
    set_callbacks( MP4Init, __MP4End );
vlc_module_end();

/*****************************************************************************
 * Declaration of local function
 *****************************************************************************/

static void MP4_TrackCreate ( input_thread_t *,
                              track_data_mp4_t *,
                              MP4_Box_t  * );
static void MP4_TrackDestroy( input_thread_t *,
                              track_data_mp4_t * );

static int  MP4_TrackSelect ( input_thread_t *,
                              track_data_mp4_t *,
                              mtime_t );
static void MP4_TrackUnselect(input_thread_t *,
                              track_data_mp4_t * );

static int  MP4_TrackSeek   ( input_thread_t *,
                              track_data_mp4_t *,
                              mtime_t );

static uint64_t MP4_GetTrackPos( track_data_mp4_t * );
static int  MP4_TrackSampleSize( track_data_mp4_t * );
static int  MP4_TrackNextSample( input_thread_t *,
                                 track_data_mp4_t * );

#define MP4_Set4BytesLE( p, dw ) \
    *((uint8_t*)p)   = ( (dw)&0xff ); \
    *((uint8_t*)p+1) = ( ((dw)>> 8)&0xff ); \
    *((uint8_t*)p+2) = ( ((dw)>>16)&0xff ); \
    *((uint8_t*)p+3) = ( ((dw)>>24)&0xff )

#define MP4_Set2BytesLE( p, dw ) \
    *((uint8_t*)p) = ( (dw)&0xff ); \
    *((uint8_t*)p+1) = ( ((dw)>> 8)&0xff )

#define FREE( p ) \
    if( p ) { free( p ); (p) = NULL;}

/*****************************************************************************
 * MP4Init: check file and initializes MP4 structures
 *****************************************************************************/
static int MP4Init( vlc_object_t * p_this )
{
    input_thread_t  *p_input = (input_thread_t *)p_this;
    uint8_t         *p_peek;

    demux_sys_t     *p_demux;

    MP4_Box_t       *p_ftyp;


    MP4_Box_t       *p_mvhd;
    MP4_Box_t       *p_trak;

    unsigned int    i;
    /* I need to seek */
    if( !p_input->stream.b_seekable )
    {
        msg_Warn( p_input, "MP4 plugin discarded (unseekable)" );
        return( -1 );

    }
    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE ;
    }

    p_input->pf_demux = MP4Demux;

    /* a little test to see if it could be a mp4 */
    if( input_Peek( p_input, &p_peek, 8 ) < 8 )
    {
        msg_Warn( p_input, "MP4 plugin discarded (cannot peek)" );
        return( -1 );
    }


    switch( VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] ) )
    {
        case( FOURCC_ftyp ):
        case( FOURCC_moov ):
        case( FOURCC_moof ):
        case( FOURCC_mdat ):
        case( FOURCC_udta ):
        case( FOURCC_free ):
        case( FOURCC_skip ):
        case( FOURCC_wide ):
            break;
         default:
            msg_Warn( p_input, "MP4 plugin discarded (not a valid file)" );
            return( -1 );
    }

    /* create our structure that will contains all data */
    if( !( p_input->p_demux_data =
                p_demux = malloc( sizeof( demux_sys_t ) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }
    memset( p_demux, 0, sizeof( demux_sys_t ) );
    p_input->p_demux_data = p_demux;


    /* Now load all boxes ( except raw data ) */
    if( !MP4_BoxGetRoot( p_input, &p_demux->box_root ) )
    {
        msg_Warn( p_input, "MP4 plugin discarded (not a valid file)" );
        return( -1 );
    }

    MP4_BoxDumpStructure( p_input, &p_demux->box_root );

    if( ( p_ftyp = MP4_BoxGet( &p_demux->box_root, "/ftyp" ) ) )
    {
        switch( p_ftyp->data.p_ftyp->i_major_brand )
        {
            case( FOURCC_isom ):
                msg_Dbg( p_input,
                         "ISO Media file (isom) version %d.",
                         p_ftyp->data.p_ftyp->i_minor_version );
                break;
            default:
                msg_Dbg( p_input,
                         "unrecognized major file specification (%4.4s).",
                          (char*)&p_ftyp->data.p_ftyp->i_major_brand );
                break;
        }
    }
    else
    {
        msg_Dbg( p_input, "file type box missing (assuming ISO Media file)" );
    }

    /* the file need to have one moov box */
    if( MP4_BoxCount( &p_demux->box_root, "/moov" ) != 1 )
    {
        msg_Err( p_input,
                 "MP4 plugin discarded (%d moov boxes)",
                 MP4_BoxCount( &p_demux->box_root, "/moov" ) );
//        MP4End( p_input );
//        return( -1 );
    }

    if( !(p_mvhd = MP4_BoxGet( &p_demux->box_root, "/moov/mvhd" ) ) )
    {
        msg_Err( p_input, "cannot find /moov/mvhd" );
        MP4End( p_input );
        return( -1 );
    }
    else
    {
        p_demux->i_timescale = p_mvhd->data.p_mvhd->i_timescale;
        p_demux->i_duration = p_mvhd->data.p_mvhd->i_duration;
    }

    if( !( p_demux->i_tracks =
                MP4_BoxCount( &p_demux->box_root, "/moov/trak" ) ) )
    {
        msg_Err( p_input, "cannot find any /moov/trak" );
        MP4End( p_input );
        return( -1 );
    }
    msg_Dbg( p_input, "find %d track%c",
                        p_demux->i_tracks,
                        p_demux->i_tracks ? 's':' ' );

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        MP4End( p_input );
        return( -1 );
    }
    /* Needed to create program _before_ MP4_TrackCreate */
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        MP4End( p_input );
        return( -1 );
    }
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    /* XXX beurk and beurk, see MP4Demux and MP4Seek */
    if( p_demux->i_duration/p_demux->i_timescale > 0 )
    {
        p_input->stream.i_mux_rate =
            p_input->stream.p_selected_area->i_size / 50 /
            ( p_demux->i_duration / p_demux->i_timescale );
    }
    else
    {
        p_input->stream.i_mux_rate = 0;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );


    /* allocate memory */
    p_demux->track = calloc( p_demux->i_tracks, sizeof( track_data_mp4_t ) );

    /* now process each track and extract all usefull informations */
    for( i = 0; i < p_demux->i_tracks; i++ )
    {
        p_trak = MP4_BoxGet( &p_demux->box_root, "/moov/trak[%d]", i );
        MP4_TrackCreate( p_input, &p_demux->track[i], p_trak );

        if( p_demux->track[i].b_ok )
        {
            char *psz_cat;
            switch( p_demux->track[i].i_cat )
            {
                case( VIDEO_ES ):
                    psz_cat = "video";
                    break;
                case( AUDIO_ES ):
                    psz_cat = "audio";
                    break;
                default:
                    psz_cat = "unknown";
                    break;
            }

            msg_Dbg( p_input, "adding track[Id 0x%x] %s (%s) language %c%c%c",
                            p_demux->track[i].i_track_ID,
                            psz_cat,
                            p_demux->track[i].b_enable ? "enable":"disable",
                            p_demux->track[i].i_language[0],
                            p_demux->track[i].i_language[1],
                            p_demux->track[i].i_language[2] );
        }
        else
        {
            msg_Dbg( p_input, "ignoring track[Id 0x%x]", p_demux->track[i].i_track_ID );
        }

    }

    for( i = 0; i < p_demux->i_tracks; i++ )
    {
        /* start decoder for this track if enable by default*/
        if( p_demux->track[i].b_ok && p_demux->track[i].b_enable )
        {
            MP4_TrackSelect( p_input, &p_demux->track[i], 0 );
        }
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( 0 );

}

/*****************************************************************************
 * MP4Demux: read packet and send them to decoders
 *****************************************************************************
 * TODO check for newly selected track (ie audio upt to now )
 *****************************************************************************/
static int MP4Demux( input_thread_t *p_input )
{
    demux_sys_t *p_demux = p_input->p_demux_data;
    unsigned int i_track;


    unsigned int i_track_selected;
    vlc_bool_t   b_video;
    vlc_bool_t   b_play_audio;

    /* check for newly selected/unselected track */
    for( i_track = 0, i_track_selected = 0, b_video = VLC_FALSE;
            i_track <  p_demux->i_tracks; i_track++ )
    {
#define track   p_demux->track[i_track]
        if( track.b_selected && track.i_sample >= track.i_sample_count )
        {
            msg_Warn( p_input, "track[0x%x] will be disabled", track.i_track_ID );
            MP4_TrackUnselect( p_input, &track );
        }
        else if( track.b_ok )
        {
            if( track.b_selected && track.p_es->p_decoder_fifo == NULL )
            {
                MP4_TrackUnselect( p_input, &track );
            }
            else if( !track.b_selected && track.p_es->p_decoder_fifo != NULL )
            {
                MP4_TrackSelect( p_input, &track, MP4_GetMoviePTS( p_demux ) );
            }

            if( track.b_selected )
            {
                i_track_selected++;

                if( track.i_cat == VIDEO_ES )
                {
                    b_video = VLC_TRUE;
                }
            }
        }
#undef  track
    }

    if( i_track_selected <= 0 )
    {
        msg_Warn( p_input, "no track selected, exiting..." );
        return( 0 );
    }


    /* XXX beurk, beuRK and BEURK,
       but only way I've found to detect seek from interface */
    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        mtime_t i_date;

        /* first wait for empty buffer, arbitrary time FIXME */
        msleep( DEFAULT_PTS_DELAY );
        /* *** calculate new date *** */

        i_date = (mtime_t)1000000 *
                 (mtime_t)p_demux->i_duration /
                 (mtime_t)p_demux->i_timescale *
                 (mtime_t)MP4_TellAbsolute( p_input ) /
                 (mtime_t)p_input->stream.p_selected_area->i_size;
        MP4Seek( p_input, i_date );
    }

    /* first wait for the good time to read a packet */
    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_demux->i_pcr );


    /* update pcr XXX in mpeg scale so in 90000 unit/s */
    p_demux->i_pcr = MP4_GetMoviePTS( p_demux ) * 9 / 100;


    /* we will read 100ms for each stream so ...*/
    p_demux->i_time += __MAX( p_demux->i_timescale / 10 , 1 );


    /* *** send audio data to decoder if rate == DEFAULT_RATE or no video *** */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( p_input->stream.control.i_rate == DEFAULT_RATE || !b_video )
    {
        b_play_audio = VLC_TRUE;
    }
    else
    {
        b_play_audio = VLC_FALSE;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    for( i_track = 0; i_track < p_demux->i_tracks; i_track++ )
    {
#define track p_demux->track[i_track]
        if( !track.b_ok ||
            !track.b_selected ||
            MP4_GetTrackPTS( &track ) >= MP4_GetMoviePTS( p_demux ) )
        {
            continue;
        }
        while( MP4_GetTrackPTS( &track ) < MP4_GetMoviePTS( p_demux ) )
        {

            if( !b_play_audio && track.i_cat == AUDIO_ES )
            {
                if( MP4_TrackNextSample( p_input, &track ) )
                {
                    break;
                }
            }
            else
            {
                size_t i_size;
                off_t i_pos;

                data_packet_t *p_data;
                pes_packet_t *p_pes;

                /* caculate size and position for this sample */
                i_size = MP4_TrackSampleSize( &track );

                i_pos  = MP4_GetTrackPos( &track );

                //msg_Dbg( p_input, "stream %d size=%6d pos=%8lld",  i_track, i_size, i_pos );

                /* go,go go ! */
                if( ! MP4_SeekAbsolute( p_input, i_pos ) )
                {
                    msg_Warn( p_input, "track[0x%x] will be disabled (eof?)", track.i_track_ID );
                    MP4_TrackUnselect( p_input, &track );
                    break;
                }


                /* now create a pes */
                if( !(p_pes = input_NewPES( p_input->p_method_data ) ) )
                {
                    break;
                }
                /* and a data packet for the data */
                if( !(p_data = input_NewPacket( p_input->p_method_data, i_size ) ) )
                {
                    input_DeletePES( p_input->p_method_data, p_pes );
                    break;
                }
                p_data->p_payload_end = p_data->p_payload_start + i_size;

                /* initialisation of all the field */
                p_pes->i_dts = p_pes->i_pts = 0;
                p_pes->p_first = p_pes->p_last  = p_data;
                p_pes->i_nb_data = 1;
                p_pes->i_pes_size = i_size;
                if( i_size > 0 )
                {
                    if( !MP4_ReadData( p_input, p_data->p_payload_start, i_size ) )
                    {
                        input_DeletePES( p_input->p_method_data, p_pes );

                        msg_Warn( p_input, "track[0x%x] will be disabled (eof?)", track.i_track_ID );
                        MP4_TrackUnselect( p_input, &track );
                        break;
                    }
                }

                p_pes->i_dts =
                    p_pes->i_pts = input_ClockGetTS( p_input,
                                                     p_input->stream.p_selected_program,
                                                     MP4_GetTrackPTS( &track ) * 9/100);

                if( track.p_es->p_decoder_fifo )
                {
                    input_DecodePES( track.p_es->p_decoder_fifo, p_pes );
                }
                else
                {
                    input_DeletePES( p_input->p_method_data, p_pes );
                }

                if( MP4_TrackNextSample( p_input, &track ) )
                {
                    break;
                }
            }
        }
#undef track
    }

    return( 1 );
}
/*****************************************************************************
 * MP4Seek: Got to i_date
 ******************************************************************************/
static int   MP4Seek     ( input_thread_t *p_input, mtime_t i_date )
{
    demux_sys_t *p_demux = p_input->p_demux_data;
    unsigned int i_track;
    /* First update update global time */
    p_demux->i_time = i_date * p_demux->i_timescale / 1000000;
    p_demux->i_pcr  = i_date* 9 / 100;

    /* Now for each stream try to go to this time */
    for( i_track = 0; i_track < p_demux->i_tracks; i_track++ )
    {
#define track p_demux->track[i_track]
        if( track.b_ok && track.b_selected )
        {
            MP4_TrackSeek( p_input, &track, i_date );
        }
#undef  track
    }
    return( 1 );
}

/*****************************************************************************
 * MP4End: frees unused data
 *****************************************************************************/
static void __MP4End ( vlc_object_t * p_this )
{
    unsigned int i_track;
    input_thread_t *  p_input = (input_thread_t *)p_this;
    demux_sys_t *p_demux = p_input->p_demux_data;

    msg_Dbg( p_input, "freeing all memory" );
    MP4_BoxFree( p_input, &p_demux->box_root );
    for( i_track = 0; i_track < p_demux->i_tracks; i_track++ )
    {
        MP4_TrackDestroy( p_input, &p_demux->track[i_track] );
    }
    FREE( p_demux->track );

    FREE( p_input->p_demux_data );
}


/****************************************************************************
 * Local functions, specific to vlc
 ****************************************************************************/

/* now create basic chunk data, the rest will be filled by MP4_CreateSamplesIndex */
static int TrackCreateChunksIndex( input_thread_t *p_input,
                                   track_data_mp4_t *p_demux_track )
{
    MP4_Box_t *p_co64; /* give offset for each chunk, same for stco and co64 */
    MP4_Box_t *p_stsc;

    unsigned int i_chunk;
    unsigned int i_index, i_last;

    if( ( !(p_co64 = MP4_BoxGet( p_demux_track->p_stbl, "stco" ) )&&
          !(p_co64 = MP4_BoxGet( p_demux_track->p_stbl, "co64" ) ) )||
        ( !(p_stsc = MP4_BoxGet( p_demux_track->p_stbl, "stsc" ) ) ))
    {
        return( VLC_EGENERIC );
    }

    p_demux_track->i_chunk_count = p_co64->data.p_co64->i_entry_count;
    if( !p_demux_track->i_chunk_count )
    {
        msg_Warn( p_input, "no chunk defined" );
        return( VLC_EGENERIC );
    }
    p_demux_track->chunk = calloc( p_demux_track->i_chunk_count,
                                   sizeof( chunk_data_mp4_t ) );

    /* first we read chunk offset */
    for( i_chunk = 0; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
    {
        p_demux_track->chunk[i_chunk].i_offset =
                p_co64->data.p_co64->i_chunk_offset[i_chunk];
    }

    /* now we read index for SampleEntry( soun vide mp4a mp4v ...)
        to be used for the sample XXX begin to 1
        We construct it begining at the end */
    i_last = p_demux_track->i_chunk_count; /* last chunk proceded */
    i_index = p_stsc->data.p_stsc->i_entry_count;
    if( !i_index )
    {
        msg_Warn( p_input, "cannot read chunk table or table empty" );
        return( VLC_EGENERIC );
    }

    while( i_index )
    {
        i_index--;
        for( i_chunk = p_stsc->data.p_stsc->i_first_chunk[i_index] - 1;
                i_chunk < i_last; i_chunk++ )
        {
            p_demux_track->chunk[i_chunk].i_sample_description_index =
                    p_stsc->data.p_stsc->i_sample_description_index[i_index];
            p_demux_track->chunk[i_chunk].i_sample_count =
                    p_stsc->data.p_stsc->i_samples_per_chunk[i_index];
        }
        i_last = p_stsc->data.p_stsc->i_first_chunk[i_index] - 1;
    }

    p_demux_track->chunk[0].i_sample_first = 0;
    for( i_chunk = 1; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
    {
        p_demux_track->chunk[i_chunk].i_sample_first =
            p_demux_track->chunk[i_chunk-1].i_sample_first +
                p_demux_track->chunk[i_chunk-1].i_sample_count;
    }

    msg_Dbg( p_input,
             "track[Id 0x%x] read %d chunk",
             p_demux_track->i_track_ID,
            p_demux_track->i_chunk_count );

    return( VLC_SUCCESS );
}
static int TrackCreateSamplesIndex( input_thread_t *p_input,
                                    track_data_mp4_t *p_demux_track )
{
    MP4_Box_t *p_stts; /* makes mapping between sample and decoding time,
                          ctts make same mapping but for composition time,
                          not yet used and probably not usefull */
    MP4_Box_t *p_stsz; /* gives sample size of each samples, there is also stz2
                          that uses a compressed form FIXME make them in libmp4
                          as a unique type */
    /* TODO use also stss and stsh table for seeking */
    /* FIXME use edit table */
    int64_t i_sample;
    int64_t i_chunk;

    int64_t i_index;
    int64_t i_index_sample_used;

    int64_t i_last_dts;

    p_stts = MP4_BoxGet( p_demux_track->p_stbl, "stts" );
    p_stsz = MP4_BoxGet( p_demux_track->p_stbl, "stsz" ); /* FIXME and stz2 */

    if( ( !p_stts )||( !p_stsz ) )
    {
        msg_Warn( p_input, "cannot read sample table" );
        return( VLC_EGENERIC );
    }

    p_demux_track->i_sample_count = p_stsz->data.p_stsz->i_sample_count;


    /* for sample size, there are 2 case */
    if( p_stsz->data.p_stsz->i_sample_size )
    {
        /* 1: all sample have the same size, so no need to construct a table */
        p_demux_track->i_sample_size = p_stsz->data.p_stsz->i_sample_size;
        p_demux_track->p_sample_size = NULL;
    }
    else
    {
        /* 2: each sample can have a different size */
        p_demux_track->i_sample_size = 0;
        p_demux_track->p_sample_size =
            calloc( p_demux_track->i_sample_count, sizeof( uint32_t ) );

        for( i_sample = 0; i_sample < p_demux_track->i_sample_count; i_sample++ )
        {
            p_demux_track->p_sample_size[i_sample] =
                    p_stsz->data.p_stsz->i_entry_size[i_sample];
        }
    }
    /* we have extract all information from stsz,
        now use stts */

    /* if we don't want to waste too much memory, we can't expand
       the box !, so each chunk will contain an "extract" of this table
       for fast research */

    i_last_dts = 0;
    i_index = 0; i_index_sample_used =0;

    /* create and init last data for each chunk */
    for(i_chunk = 0 ; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
    {

        int64_t i_entry, i_sample_count, i;
        /* save last dts */
        p_demux_track->chunk[i_chunk].i_first_dts = i_last_dts;
    /* count how many entries needed for this chunk
       for p_sample_delta_dts and p_sample_count_dts */

        i_sample_count = p_demux_track->chunk[i_chunk].i_sample_count;

        i_entry = 0;
        while( i_sample_count > 0 )
        {
            i_sample_count -= p_stts->data.p_stts->i_sample_count[i_index+i_entry];
            if( i_entry == 0 )
            {
                i_sample_count += i_index_sample_used; /* don't count already used sample
                                                   int this entry */
            }
            i_entry++;
        }

        /* allocate them */
        p_demux_track->chunk[i_chunk].p_sample_count_dts =
            calloc( i_entry, sizeof( uint32_t ) );
        p_demux_track->chunk[i_chunk].p_sample_delta_dts =
            calloc( i_entry, sizeof( uint32_t ) );

        /* now copy */
        i_sample_count = p_demux_track->chunk[i_chunk].i_sample_count;
        for( i = 0; i < i_entry; i++ )
        {
            int64_t i_used;
            int64_t i_rest;

            i_rest = p_stts->data.p_stts->i_sample_count[i_index] - i_index_sample_used;

            i_used = __MIN( i_rest, i_sample_count );

            i_index_sample_used += i_used;
            i_sample_count -= i_used;

            p_demux_track->chunk[i_chunk].p_sample_count_dts[i] = i_used;

            p_demux_track->chunk[i_chunk].p_sample_delta_dts[i] =
                        p_stts->data.p_stts->i_sample_delta[i_index];

            i_last_dts += i_used *
                    p_demux_track->chunk[i_chunk].p_sample_delta_dts[i];

            if( i_index_sample_used >=
                             p_stts->data.p_stts->i_sample_count[i_index] )
            {

                i_index++;
                i_index_sample_used = 0;
            }
        }

    }

    msg_Dbg( p_input,
             "track[Id 0x%x] read %d samples length:"I64Fd"s",
             p_demux_track->i_track_ID,
             p_demux_track->i_sample_count,
             i_last_dts / p_demux_track->i_timescale );

    return( VLC_SUCCESS );
}

/*
 * TrackCreateES:
 *  Create ES and PES to init decoder if needed, for a track starting at i_chunk
 */
static int  TrackCreateES   ( input_thread_t   *p_input,
                              track_data_mp4_t *p_track,
                              unsigned int     i_chunk,
                              es_descriptor_t  **pp_es,
                              pes_packet_t     **pp_pes )
{
    MP4_Box_t *  p_sample;
    unsigned int i;

    unsigned int i_decoder_specific_info_len;
    uint8_t *    p_decoder_specific_info;

    es_descriptor_t *p_es;
    pes_packet_t    *p_pes_init;

    uint8_t             *p_init;
    BITMAPINFOHEADER    *p_bih;
    WAVEFORMATEX        *p_wf;

    MP4_Box_t   *p_esds;

    if( !p_track->chunk[i_chunk].i_sample_description_index )
    {
        msg_Warn( p_input,
                  "invalid SampleEntry index (track[Id 0x%x])",
                  p_track->i_track_ID );
        return( VLC_EGENERIC );
    }

    p_sample = MP4_BoxGet(  p_track->p_stsd,
                            "[%d]",
                p_track->chunk[i_chunk].i_sample_description_index - 1 );

    if( !p_sample || !p_sample->data.p_data )
    {
        msg_Warn( p_input,
                  "cannot find SampleEntry (track[Id 0x%x])",
                  p_track->i_track_ID );
        return( VLC_EGENERIC );
    }

    p_track->p_sample = p_sample;

    if( p_track->i_sample_size == 1 )
    {
        MP4_Box_data_sample_soun_t *p_soun;

        p_soun = p_sample->data.p_sample_soun;

        if( p_soun->i_qt_version == 0 )
        {
            switch( p_sample->i_type )
            {
                case VLC_FOURCC( 'i', 'm', 'a', '4' ):
                    p_soun->i_qt_version = 1;
                    p_soun->i_sample_per_packet = 64;
                    p_soun->i_bytes_per_packet  = 34;
                    p_soun->i_bytes_per_frame   = 34 * p_soun->i_channelcount;
                    p_soun->i_bytes_per_sample  = 2;
                    break;
                default:
                    break;
            }
        }
    }


    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_es = input_AddES( p_input,
                        p_input->stream.p_selected_program,
                        p_track->i_track_ID,
                        0 );
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    /* Initialise ES, first language as description */
    for( i = 0; i < 3; i++ )
    {
        p_es->psz_desc[i] = p_track->i_language[i];
    }
    p_es->psz_desc[3] = '\0';

    p_es->i_stream_id = p_track->i_track_ID;

    /* It's a little ugly but .. there are special cases */
    switch( p_sample->i_type )
    {
        case( VLC_FOURCC( '.', 'm', 'p', '3' ) ):
        case( VLC_FOURCC( 'm', 's', 0x00, 0x55 ) ):
            p_es->i_fourcc = VLC_FOURCC( 'm', 'p', 'g', 'a' );
            break;
        case( VLC_FOURCC( 'r', 'a', 'w', ' ' ) ):
            p_es->i_fourcc = VLC_FOURCC( 'a', 'r', 'a', 'w' );
            break;
        default:
            p_es->i_fourcc = p_sample->i_type;
            break;
    }

    p_es->i_cat = p_track->i_cat;

    i_decoder_specific_info_len = 0;
    p_decoder_specific_info = NULL;
    p_pes_init = NULL;

    /* now see if esds is present and if so create a data packet
        with decoder_specific_info  */
#define p_decconfig p_esds->data.p_esds->es_descriptor.p_decConfigDescr
    if( ( p_esds = MP4_BoxGet( p_sample, "esds" ) )&&
        ( p_esds->data.p_esds )&&
        ( p_decconfig ) )
    {
        /* First update information based on i_objectTypeIndication */
        switch( p_decconfig->i_objectTypeIndication )
        {
            case( 0x20 ): /* MPEG4 VIDEO */
                p_es->i_fourcc = VLC_FOURCC( 'm','p','4','v' );
                break;
            case( 0x40):
                p_es->i_fourcc = VLC_FOURCC( 'm','p','4','a' );
                break;
            case( 0x60):
            case( 0x61):
            case( 0x62):
            case( 0x63):
            case( 0x64):
            case( 0x65): /* MPEG2 video */
                p_es->i_fourcc = VLC_FOURCC( 'm','p','g','v' );
                break;
            /* Theses are MPEG2-AAC */
            case( 0x66): /* main profile */
            case( 0x67): /* Low complexity profile */
            case( 0x68): /* Scaleable Sampling rate profile */
                p_es->i_fourcc = VLC_FOURCC( 'm','p','4','a' );
                break;
            /* true MPEG 2 audio */
            case( 0x69):
                p_es->i_fourcc = VLC_FOURCC( 'm','p','g','a' );
                break;
            case( 0x6a): /* MPEG1 video */
                p_es->i_fourcc = VLC_FOURCC( 'm','p','g','v' );
                break;
            case( 0x6b): /* MPEG1 audio */
                p_es->i_fourcc = VLC_FOURCC( 'm','p','g','a' );
                break;
            case( 0x6c ): /* jpeg */
                p_es->i_fourcc = VLC_FOURCC( 'j','p','e','g' );
                break;
            default:
                /* Unknown entry, but don't touch i_fourcc */
                msg_Warn( p_input,
                          "unknown objectTypeIndication(0x%x) (Track[ID 0x%x])",
                          p_decconfig->i_objectTypeIndication,
                          p_track->i_track_ID );
                break;
        }
        i_decoder_specific_info_len =
                p_decconfig->i_decoder_specific_info_len;
        p_decoder_specific_info =
                p_decconfig->p_decoder_specific_info;
    }
    else
    {
        switch( p_sample->i_type )
        {
            /* qt decoder, send the complete chunk */
            case VLC_FOURCC( 'S', 'V', 'Q', '3' ):
            case VLC_FOURCC( 'Z', 'y', 'G', 'o' ):
                i_decoder_specific_info_len = p_sample->data.p_sample_vide->i_qt_image_description;
                p_decoder_specific_info     = p_sample->data.p_sample_vide->p_qt_image_description;
                break;
            case VLC_FOURCC( 'Q', 'D', 'M', 'C' ):
            case VLC_FOURCC( 'Q', 'D', 'M', '2' ):
            case VLC_FOURCC( 'Q', 'c', 'l', 'p' ):
                i_decoder_specific_info_len = p_sample->data.p_sample_soun->i_qt_description;
                p_decoder_specific_info     = p_sample->data.p_sample_soun->p_qt_description;
                break;
            default:
                break;
        }
    }

#undef p_decconfig

    /* some last initialisation */
    /* XXX I create a bitmapinfoheader_t or
       waveformatex_t for each stream, up to now it's the best thing
       I've found but it could exist a better solution :) as something
       like adding some new fields in p_es ...

       XXX I don't set all values, only thoses that are interesting or known
        --> bitmapinfoheader_t : width and height
        --> waveformatex_t : channels, samplerate, bitspersample
        and at the end I add p_decoder_specific_info

        TODO set more values

     */

    switch( p_track->i_cat )
    {
        case( VIDEO_ES ):
            /* now create a bitmapinfoheader_t for decoder and
               add information found in p_esds */
            /* XXX XXX + 16 are for avoid segfault when ffmpeg access beyong the data */
            p_init = malloc( sizeof( BITMAPINFOHEADER ) + i_decoder_specific_info_len + 16 );
            p_bih = (BITMAPINFOHEADER*)p_init;

            p_bih->biSize     = sizeof( BITMAPINFOHEADER ) + i_decoder_specific_info_len;
            p_bih->biWidth    = p_sample->data.p_sample_vide->i_width;
            p_bih->biHeight   = p_sample->data.p_sample_vide->i_height;
            p_bih->biPlanes   = 1;      // FIXME
            p_bih->biBitCount = 0;      // FIXME
            p_bih->biCompression   = 0; // FIXME
            p_bih->biSizeImage     = 0; // FIXME
            p_bih->biXPelsPerMeter = 0; // FIXME
            p_bih->biYPelsPerMeter = 0; // FIXME
            p_bih->biClrUsed       = 0; // FIXME
            p_bih->biClrImportant  = 0; // FIXME

            if( p_bih->biWidth == 0 )
            {
                // fall on display size
                p_bih->biWidth = p_track->i_width;
            }
            if( p_bih->biHeight == 0 )
            {
                // fall on display size
                p_bih->biHeight = p_track->i_height;
            }

            if( i_decoder_specific_info_len )
            {
                data_packet_t   *p_data;

                memcpy( p_init + sizeof( BITMAPINFOHEADER ),
                        p_decoder_specific_info,
                        i_decoder_specific_info_len);

                /* If stream is mpeg4 video we send specific_info,
                   as it's needed to decode it (vol) */
                switch( p_es->i_fourcc )
                {
                    case VLC_FOURCC( 'm','p','4','v' ):
                    case VLC_FOURCC( 'D','I','V','X' ):
                    case VLC_FOURCC( 'd','i','v','x' ):
                        p_pes_init = input_NewPES( p_input->p_method_data );
                        p_data = input_NewPacket( p_input->p_method_data,
                                                  i_decoder_specific_info_len);
                        p_data->p_payload_end = p_data->p_payload_start + i_decoder_specific_info_len;

                        memcpy( p_data->p_payload_start,
                                p_decoder_specific_info,
                                i_decoder_specific_info_len );
                        p_pes_init->i_dts = p_pes_init->i_pts = 0;
                        p_pes_init->p_first = p_pes_init->p_last = p_data;
                        p_pes_init->i_nb_data = 1;
                        p_pes_init->i_pes_size = i_decoder_specific_info_len;
                        break;
                    default:
                        break;
                }

            }
            break;

        case( AUDIO_ES ):
            p_init = malloc( sizeof( WAVEFORMATEX ) + i_decoder_specific_info_len + 16 );
            p_wf = (WAVEFORMATEX*)p_init;

            p_wf->wFormatTag = 1;
            p_wf->nChannels = p_sample->data.p_sample_soun->i_channelcount;
            p_wf->nSamplesPerSec = p_sample->data.p_sample_soun->i_sampleratehi;
            p_wf->nAvgBytesPerSec = p_sample->data.p_sample_soun->i_channelcount *
                                    p_sample->data.p_sample_soun->i_sampleratehi *
                                    p_sample->data.p_sample_soun->i_samplesize / 8;
            p_wf->nBlockAlign = 0;
            p_wf->wBitsPerSample = p_sample->data.p_sample_soun->i_samplesize;
            p_wf->cbSize = i_decoder_specific_info_len;

            if( i_decoder_specific_info_len )
            {
                memcpy( p_init + sizeof( WAVEFORMATEX ),
                        p_decoder_specific_info,
                        i_decoder_specific_info_len);
            }

            break;

        default:
            p_init = NULL;
            break;
    }
    if( p_es->i_cat == AUDIO_ES )
    {
        p_es->p_bitmapinfoheader = NULL;
        p_es->p_waveformatex     = (void*)p_init;
    }
    else if( p_es->i_cat == VIDEO_ES )
    {
        p_es->p_bitmapinfoheader = (void*)p_init;
        p_es->p_waveformatex     = NULL;
    }

    *pp_es = p_es;
    *pp_pes = p_pes_init;
    return( VLC_SUCCESS );
}

/* given a time it return sample/chunk */
static int  TrackTimeToSampleChunk( input_thread_t *p_input,
                                    track_data_mp4_t *p_track,
                                    uint64_t i_start,
                                    uint32_t *pi_chunk,
                                    uint32_t *pi_sample )
{
    MP4_Box_t    *p_stss;
    uint64_t     i_dts;
    unsigned int i_sample;
    unsigned int i_chunk;
    int          i_index;

    /* convert absolute time to in timescale unit */
    i_start = i_start * (mtime_t)p_track->i_timescale / (mtime_t)1000000;

    /* FIXME see if it's needed to check p_track->i_chunk_count */
    if( !p_track->b_ok || p_track->i_chunk_count == 0 )
    {
        return( VLC_EGENERIC );
    }

    /* we start from sample 0/chunk 0, hope it won't take too much time */
    /* *** find good chunk *** */
    for( i_chunk = 0; ; i_chunk++ )
    {
        if( i_chunk + 1 >= p_track->i_chunk_count )
        {
            /* at the end and can't check if i_start in this chunk,
               it will be check while searching i_sample */
            i_chunk = p_track->i_chunk_count - 1;
            break;
        }

        if( i_start >= p_track->chunk[i_chunk].i_first_dts &&
            i_start <  p_track->chunk[i_chunk + 1].i_first_dts )
        {
            break;
        }
    }

    /* *** find sample in the chunk *** */
    i_sample = p_track->chunk[i_chunk].i_sample_first;
    i_dts    = p_track->chunk[i_chunk].i_first_dts;
    for( i_index = 0; i_sample < p_track->chunk[i_chunk].i_sample_count; )
    {
        if( i_dts +
            p_track->chunk[i_chunk].p_sample_count_dts[i_index] *
            p_track->chunk[i_chunk].p_sample_delta_dts[i_index] < i_start )
        {
            i_dts    +=
                p_track->chunk[i_chunk].p_sample_count_dts[i_index] *
                p_track->chunk[i_chunk].p_sample_delta_dts[i_index];

            i_sample += p_track->chunk[i_chunk].p_sample_count_dts[i_index];
            i_index++;
        }
        else
        {
            if( p_track->chunk[i_chunk].p_sample_delta_dts[i_index] <= 0 )
            {
                break;
            }
            i_sample += ( i_start - i_dts ) / p_track->chunk[i_chunk].p_sample_delta_dts[i_index];
            break;
        }
    }

    if( i_sample >= p_track->i_sample_count )
    {
        msg_Warn( p_input,
                  "track[Id 0x%x] will be disabled (seeking too far) chunk=%d sample=%d",
                  p_track->i_track_ID, i_chunk, i_sample );
        return( VLC_EGENERIC );
    }


    /* *** Try to find nearest sync points *** */
    if( ( p_stss = MP4_BoxGet( p_track->p_stbl, "stss" ) ) )
    {
        unsigned int i_index;
        msg_Dbg( p_input,
                    "track[Id 0x%x] using Sync Sample Box (stss)",
                    p_track->i_track_ID );
        for( i_index = 0; i_index < p_stss->data.p_stss->i_entry_count; i_index++ )
        {
            if( p_stss->data.p_stss->i_sample_number[i_index] >= i_sample )
            {
                if( i_index > 0 )
                {
                    msg_Dbg( p_input, "stts gives %d --> %d (sample number)",
                            i_sample,
                            p_stss->data.p_stss->i_sample_number[i_index-1] );
                    i_sample = p_stss->data.p_stss->i_sample_number[i_index-1];
                    /* new i_sample is less than old so i_chunk can only decreased */
                    while( i_chunk > 0 &&
                            i_sample < p_track->chunk[i_chunk].i_sample_first )
                    {
                        i_chunk--;
                    }
                }
                else
                {
                    msg_Dbg( p_input, "stts gives %d --> %d (sample number)",
                            i_sample,
                            p_stss->data.p_stss->i_sample_number[i_index] );
                    i_sample = p_stss->data.p_stss->i_sample_number[i_index];
                    /* new i_sample is more than old so i_chunk can only increased */
                    while( i_chunk < p_track->i_chunk_count - 1 &&
                           i_sample >= p_track->chunk[i_chunk].i_sample_first +
                                                p_track->chunk[i_chunk].i_sample_count )
                    {
                        i_chunk++;
                    }
                }
                break;
            }
        }
    }
    else
    {
        msg_Dbg( p_input,
                    "track[Id 0x%x] does not provide Sync Sample Box (stss)",
                    p_track->i_track_ID );
    }

    if( pi_chunk  ) *pi_chunk  = i_chunk;
    if( pi_sample ) *pi_sample = i_sample;
    return( VLC_SUCCESS );
}

static int  TrackGotoChunkSample( input_thread_t   *p_input,
                                  track_data_mp4_t *p_track,
                                  unsigned int     i_chunk,
                                  unsigned int     i_sample )
{
    /* now see if actual es is ok */
    if( p_track->i_chunk < 0 ||
        p_track->i_chunk >= p_track->i_chunk_count ||
        p_track->chunk[p_track->i_chunk].i_sample_description_index !=
            p_track->chunk[i_chunk].i_sample_description_index )
    {
        msg_Warn( p_input, "Recreate ES" );

        /* no :( recreate es */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        input_DelES( p_input, p_track->p_es );
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        if( p_track->p_pes_init )
        {
            input_DeletePES( p_input->p_method_data, p_track->p_pes_init );
        }

        if( TrackCreateES( p_input,
                           p_track, i_chunk,
                           &p_track->p_es,
                           &p_track->p_pes_init ) )
        {
            msg_Err( p_input, "cannot create es for track[Id 0x%x]",
                     p_track->i_track_ID );

            p_track->b_ok       = VLC_FALSE;
            p_track->b_selected = VLC_FALSE;
            return( VLC_EGENERIC );
        }
    }

    /* select again the new decoder */
    if( p_track->b_selected && p_track->p_es && p_track->p_es->p_decoder_fifo == NULL )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
        input_SelectES( p_input, p_track->p_es );
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        if( p_track->p_es->p_decoder_fifo )
        {
            if( p_track->p_pes_init != NULL )
            {
                input_DecodePES( p_track->p_es->p_decoder_fifo, p_track->p_pes_init );
                p_track->p_pes_init = NULL;
            }
            p_track->b_selected = VLC_TRUE;
        }
        else
        {
            msg_Dbg( p_input, "Argg cannot select this stream" );
            if( p_track->p_pes_init != NULL )
            {
                input_DeletePES( p_input->p_method_data, p_track->p_pes_init );
                p_track->p_pes_init = NULL;
            }
            p_track->b_selected = VLC_FALSE;
        }
    }

    p_track->i_chunk    = i_chunk;
    p_track->i_sample   = i_sample;

    return( VLC_SUCCESS );
}

/****************************************************************************
 * MP4_TrackCreate:
 ****************************************************************************
 * Parse track information and create all needed data to run a track
 * If it succeed b_ok is set to 1 else to 0
 ****************************************************************************/
static void MP4_TrackCreate( input_thread_t *p_input,
                             track_data_mp4_t *p_track,
                             MP4_Box_t  * p_box_trak )
{
    unsigned int i;

    MP4_Box_t *p_tkhd = MP4_BoxGet( p_box_trak, "tkhd" );
    MP4_Box_t *p_tref = MP4_BoxGet( p_box_trak, "tref" );
    MP4_Box_t *p_elst;

    MP4_Box_t *p_mdhd;
    MP4_Box_t *p_hdlr;

    MP4_Box_t *p_vmhd;
    MP4_Box_t *p_smhd;

    /* hint track unsuported */

    /* set default value (-> track unusable) */
    p_track->b_ok       = VLC_FALSE;
    p_track->b_enable   = VLC_FALSE;
    p_track->b_selected = VLC_FALSE;

    p_track->i_cat = UNKNOWN_ES;

    if( !p_tkhd )
    {
        return;
    }

    /* do we launch this track by default ? */
    p_track->b_enable =
        ( ( p_tkhd->data.p_tkhd->i_flags&MP4_TRACK_ENABLED ) != 0 );

    p_track->i_track_ID = p_tkhd->data.p_tkhd->i_track_ID;
    p_track->i_width = p_tkhd->data.p_tkhd->i_width / 65536;
    p_track->i_height = p_tkhd->data.p_tkhd->i_height / 65536;

    if( ( p_elst = MP4_BoxGet( p_box_trak, "edts/elst" ) ) )
    {
/*        msg_Warn( p_input, "unhandled box: edts --> FIXME" ); */
    }

    if( p_tref )
    {
/*        msg_Warn( p_input, "unhandled box: tref --> FIXME" ); */
    }

    p_mdhd = MP4_BoxGet( p_box_trak, "mdia/mdhd" );
    p_hdlr = MP4_BoxGet( p_box_trak, "mdia/hdlr" );

    if( ( !p_mdhd )||( !p_hdlr ) )
    {
        return;
    }

    p_track->i_timescale = p_mdhd->data.p_mdhd->i_timescale;

    for( i = 0; i < 3; i++ )
    {
        p_track->i_language[i] = p_mdhd->data.p_mdhd->i_language[i];
    }
    p_mdhd->data.p_mdhd->i_language[3] = 0;

    switch( p_hdlr->data.p_hdlr->i_handler_type )
    {
        case( FOURCC_soun ):
            if( !( p_smhd = MP4_BoxGet( p_box_trak, "mdia/minf/smhd" ) ) )
            {
                return;
            }
            p_track->i_cat = AUDIO_ES;
            break;

        case( FOURCC_vide ):
            if( !( p_vmhd = MP4_BoxGet( p_box_trak, "mdia/minf/vmhd" ) ) )
            {
                return;
            }
            p_track->i_cat = VIDEO_ES;
            break;

        default:
            return;
    }
/*  TODO
    add support for:
    p_dinf = MP4_BoxGet( p_minf, "dinf" );
*/
    if( !( p_track->p_stbl = MP4_BoxGet( p_box_trak,"mdia/minf/stbl" ) ) )
    {
        return;
    }

    if( !( p_track->p_stsd = MP4_BoxGet( p_box_trak,"mdia/minf/stbl/stsd") ) )
    {
        return;
    }

    /* Create chunk  index table */
    if( TrackCreateChunksIndex( p_input,p_track  ) )
    {
        return; /* cannot create chunks index */
    }

    /* create sample index table needed for reading and seeking */
    if( TrackCreateSamplesIndex( p_input, p_track ) )
    {
        return; /* cannot create samples index */
    }

    p_track->i_chunk  = 0;
    p_track->i_sample = 0;
    /* now create es but does not select it */
    /* XXX needed else vlc won't know this track exist */
    if( TrackCreateES( p_input,
                       p_track, p_track->i_chunk,
                       &p_track->p_es,
                       &p_track->p_pes_init ) )
    {
        msg_Err( p_input, "cannot create es for track[Id 0x%x]",
                 p_track->i_track_ID );
        return;
    }
#if 0
    {
        int i;

        for( i = 0; i < p_track->i_chunk_count; i++ )
        {
            fprintf( stderr, "%-5d sample_count=%d pts=%lld\n", i, p_track->chunk[i].i_sample_count, p_track->chunk[i].i_first_dts );

        }
    }
#endif
    p_track->b_ok = VLC_TRUE;
}

/****************************************************************************
 * MP4_TrackDestroy:
 ****************************************************************************
 * Destroy a track created by MP4_TrackCreate.
 ****************************************************************************/
static void MP4_TrackDestroy( input_thread_t *p_input,
                              track_data_mp4_t *p_track )
{
    unsigned int i_chunk;

    p_track->b_ok = VLC_FALSE;
    p_track->b_enable   = VLC_FALSE;
    p_track->b_selected = VLC_FALSE;

    p_track->i_cat = UNKNOWN_ES;

    if( p_track->p_pes_init )
    {
        input_DeletePES( p_input->p_method_data, p_track->p_pes_init );
    }

    for( i_chunk = 0; i_chunk < p_track->i_chunk_count; i_chunk++ )
    {
        if( p_track->chunk )
        {
           FREE(p_track->chunk[i_chunk].p_sample_count_dts);
           FREE(p_track->chunk[i_chunk].p_sample_delta_dts );
        }
    }
    FREE( p_track->chunk );

    if( !p_track->i_sample_size )
    {
        FREE( p_track->p_sample_size );
    }
}

static int  MP4_TrackSelect ( input_thread_t    *p_input,
                              track_data_mp4_t  *p_track,
                              mtime_t           i_start )
{
    uint32_t i_chunk;
    uint32_t i_sample;

    if( !p_track->b_ok )
    {
        return( VLC_EGENERIC );
    }

    if( p_track->b_selected )
    {
        msg_Warn( p_input,
                  "track[Id 0x%x] already selected",
                  p_track->i_track_ID );
        return( VLC_SUCCESS );
    }

    if( TrackTimeToSampleChunk( p_input,
                                p_track, i_start,
                                &i_chunk, &i_sample ) )
    {
        msg_Warn( p_input,
                  "cannot select track[Id 0x%x]",
                  p_track->i_track_ID );
        return( VLC_EGENERIC );
    }

    p_track->b_selected = VLC_TRUE;

    if( TrackGotoChunkSample( p_input, p_track, i_chunk, i_sample ) )
    {
        p_track->b_selected = VLC_FALSE;
    }

    return( p_track->b_selected ? VLC_SUCCESS : VLC_EGENERIC );
}

static void MP4_TrackUnselect(input_thread_t    *p_input,
                              track_data_mp4_t  *p_track )
{
    if( !p_track->b_ok )
    {
        return;
    }

    if( !p_track->b_selected )
    {
        msg_Warn( p_input,
                  "track[Id 0x%x] already unselected",
                  p_track->i_track_ID );
        return;
    }

    if( p_track->p_es->p_decoder_fifo )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
        input_UnselectES( p_input, p_track->p_es );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

    p_track->b_selected = VLC_FALSE;
}

static int  MP4_TrackSeek   ( input_thread_t    *p_input,
                              track_data_mp4_t  *p_track,
                              mtime_t           i_start )
{
    uint32_t i_chunk;
    uint32_t i_sample;

    if( !p_track->b_ok )
    {
        return( VLC_EGENERIC );
    }

    if( TrackTimeToSampleChunk( p_input,
                                p_track, i_start,
                                &i_chunk, &i_sample ) )
    {
        msg_Warn( p_input,
                  "cannot select track[Id 0x%x]",
                  p_track->i_track_ID );
        return( VLC_EGENERIC );
    }

    p_track->b_selected = VLC_TRUE;

    TrackGotoChunkSample( p_input, p_track, i_chunk, i_sample );

    return( p_track->b_selected ? VLC_SUCCESS : VLC_EGENERIC );
}





/*
 * 3 types: for audio
 * 
 */

static int  MP4_TrackSampleSize( track_data_mp4_t   *p_track )
{
    int i_size;
    MP4_Box_data_sample_soun_t *p_soun;

    if( p_track->i_sample_size == 0 )
    {
        /* most simple case */
        return( p_track->p_sample_size[p_track->i_sample] );
    }
    if( p_track->i_cat != AUDIO_ES )
    {
        return( p_track->i_sample_size );
    }

    if( p_track->i_sample_size != 1 )
    {
        //msg_Warn( p_input, "SampleSize != 1" );
        return( p_track->i_sample_size );
    }

    p_soun = p_track->p_sample->data.p_sample_soun;

    if( p_soun->i_qt_version == 1 )
    {
        i_size = p_track->chunk[p_track->i_chunk].i_sample_count / p_soun->i_sample_per_packet * p_soun->i_bytes_per_frame;
    }
    else
    {
        //msg_Warn( p_input, "i_qt_version == 0" );
        /* FIXME */

        i_size = p_track->chunk[p_track->i_chunk].i_sample_count * p_soun->i_channelcount * p_soun->i_samplesize / 8;
    }

    //fprintf( stderr, "size=%d\n", i_size );
    return( i_size );
}


static uint64_t MP4_GetTrackPos( track_data_mp4_t *p_track )
{
    unsigned int i_sample;
    uint64_t i_pos;


    i_pos = p_track->chunk[p_track->i_chunk].i_offset;

    if( p_track->i_sample_size )
    {
#if 0
        i_pos += ( p_track->i_sample -
                        p_track->chunk[p_track->i_chunk].i_sample_first ) *
                                MP4_TrackSampleSize( p_track );
#endif
        /* we read chunk by chunk */
        i_pos += 0;
    }
    else
    {
        for( i_sample = p_track->chunk[p_track->i_chunk].i_sample_first;
                i_sample < p_track->i_sample; i_sample++ )
        {
            i_pos += p_track->p_sample_size[i_sample];
        }

    }
    return( i_pos );
}

static int  MP4_TrackNextSample( input_thread_t     *p_input,
                                 track_data_mp4_t   *p_track )
{

    if( p_track->i_cat == AUDIO_ES &&
        p_track->i_sample_size != 0 )
    {
        MP4_Box_data_sample_soun_t *p_soun;

        p_soun = p_track->p_sample->data.p_sample_soun;

        if( p_soun->i_qt_version == 1 )
        {
            /* chunk by chunk */
            p_track->i_sample =
                p_track->chunk[p_track->i_chunk].i_sample_first +
                p_track->chunk[p_track->i_chunk].i_sample_count;
        }
        else
        {
            /* FIXME */
            p_track->i_sample =
                p_track->chunk[p_track->i_chunk].i_sample_first +
                p_track->chunk[p_track->i_chunk].i_sample_count;
        }
    }
    else
    {
        p_track->i_sample++;
    }

    if( p_track->i_sample >= p_track->i_sample_count )
    {
        /* we have reach end of the track so free decoder stuff */
        msg_Warn( p_input, "track[0x%x] will be disabled", p_track->i_track_ID );
        MP4_TrackUnselect( p_input, p_track );
        return( VLC_EGENERIC );
    }

    /* Have we changed chunk ? */
    if( p_track->i_sample >=
            p_track->chunk[p_track->i_chunk].i_sample_first +
            p_track->chunk[p_track->i_chunk].i_sample_count )
    {
        if( TrackGotoChunkSample( p_input,
                                  p_track,
                                  p_track->i_chunk + 1,
                                  p_track->i_sample ) )
        {
            msg_Warn( p_input, "track[0x%x] will be disabled (cannot restart decoder)", p_track->i_track_ID );
            MP4_TrackUnselect( p_input, p_track );
            return( VLC_EGENERIC );
        }
    }

    return( VLC_SUCCESS );
}


