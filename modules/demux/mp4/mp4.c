/*****************************************************************************
 * mp4.c : MP4 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: mp4.c,v 1.12 2003/01/08 10:46:30 fenrir Exp $
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
    set_description( "MP4 demuxer" );
    set_capability( "demux", 242 );
    set_callbacks( MP4Init, __MP4End );
vlc_module_end();

/*****************************************************************************
 * Declaration of local function
 *****************************************************************************/

static int MP4_TrackSynchro( input_thread_t *p_input, track_data_mp4_t *p_track );

static void MP4_ParseTrack();

static int MP4_CreateChunksIndex();
static int MP4_CreateSamplesIndex();

static void MP4_StartDecoder();
static void MP4_StopDecoder();

static int  MP4_ReadSample();
static int  MP4_DecodeSample();

#define MP4_Set4BytesLE( p, dw ) \
    *((uint8_t*)p)   = ( (dw)&0xff ); \
    *((uint8_t*)p+1) = ( ((dw)>> 8)&0xff ); \
    *((uint8_t*)p+2) = ( ((dw)>>16)&0xff ); \
    *((uint8_t*)p+3) = ( ((dw)>>24)&0xff )

#define MP4_Set2BytesLE( p, dw ) \
    *((uint8_t*)p) = ( (dw)&0xff ); \
    *((uint8_t*)p+1) = ( ((dw)>> 8)&0xff )

    
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
                msg_Info( p_input, 
                          "ISO Media file (isom) version %d.",
                          p_ftyp->data.p_ftyp->i_minor_version );
                break;
            default:
                msg_Info( p_input,
                          "Unrecognize major file specification (%c%c%c%c).",
                           p_ftyp->data.p_ftyp->i_major_brand&0xff,
                           ( p_ftyp->data.p_ftyp->i_major_brand >>  8)&0xff,
                           ( p_ftyp->data.p_ftyp->i_major_brand >> 16 )&0xff,
                           ( p_ftyp->data.p_ftyp->i_major_brand >> 24 )&0xff );
                break;
        }
    }
    else
    {
        msg_Info( p_input, "File Type box missing(assume ISO Media file)" );
    }

    /* the file need to have one moov box */
    if( MP4_BoxCount( &p_demux->box_root, "/moov" ) != 1 )
    {
        msg_Err( p_input, 
                 "MP4 plugin discarded (%d moov box)",
                 MP4_BoxCount( &p_demux->box_root, "/moov" ) );
//        MP4End( p_input );
//        return( -1 );
    }

    if( !(p_mvhd = MP4_BoxGet( &p_demux->box_root, "/moov/mvhd" ) ) )
    {
        msg_Err( p_input, "cannot find /moov/mvhd !" );
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
        msg_Err( p_input, "cannot find any /moov/trak !" );
        MP4End( p_input );
        return( -1 );
    }
    msg_Dbg( p_input, "find %d track%c",
                        p_demux->i_tracks,
                        p_demux->i_tracks ? 's':' ' );

    /* allocate memory */
    p_demux->track = calloc( p_demux->i_tracks, sizeof( track_data_mp4_t ) );

    /* now process each track and extract all usefull informations */
    for( i = 0; i < p_demux->i_tracks; i++ )
    {
        p_trak = MP4_BoxGet( &p_demux->box_root, "/moov/trak[%d]", i );
        MP4_ParseTrack( p_input, &p_demux->track[i], p_trak );

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

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        MP4End( p_input );
        return( -1 );
    }
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


    for( i = 0; i < p_demux->i_tracks; i++ )
    {
        /* start decoder for this track if enable by default*/
        if( p_demux->track[i].b_enable )
        {
            MP4_StartDecoder( p_input, &p_demux->track[i] );
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
    

    for( i_track = 0; i_track < p_demux->i_tracks; i_track++ )
    {
        if( ( !p_demux->track[i_track].b_ok )||
            ( !p_demux->track[i_track].p_es )||
            ( !p_demux->track[i_track].p_es->p_decoder_fifo )||
            ( MP4_GetTrackPTS( &p_demux->track[i_track] ) >=
                        MP4_GetMoviePTS( p_demux ) ) )
        {
            continue; /* no need to read something */
        }
        while( MP4_GetTrackPTS( &p_demux->track[i_track] ) <
                        MP4_GetMoviePTS( p_demux ) )
        {

            pes_packet_t *p_pes;

            /* read a sample */
            if( !MP4_ReadSample( p_input ,
                                 &p_demux->track[i_track],
                                 &p_pes ) )
            {
                break;
            }

            /* send it to decoder and update time of this track 
                 it also launch a new decoder if needed */
            MP4_DecodeSample( p_input ,
                              &p_demux->track[i_track],
                              p_pes );
        }

    }
    
    /* now check if all tracks are finished or unhandled*/
    
    for( i_track = 0; i_track < p_demux->i_tracks; i_track++ )
    {
        if( ( p_demux->track[i_track].b_ok )&&
            ( p_demux->track[i_track].i_sample < p_demux->track[i_track].i_sample_count )&&
            ( p_demux->track[i_track].p_es )&&
            ( p_demux->track[i_track].p_es->p_decoder_fifo ) )
        {
            return( 1 );
        }
    }

    return( 0 ); /* EOF */
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
    p_demux->i_pcr = __MAX( MP4_GetMoviePTS( p_demux ) - DEFAULT_PTS_DELAY,
                            0 ) * 9 / 100; 
    
    /* Now for each stream try to go to this time */
    for( i_track = 0; i_track < p_demux->i_tracks; i_track++ )
    {
        MP4_TrackSynchro( p_input, &p_demux->track[i_track] );
    }
    return( 1 );
}

/*****************************************************************************
 * MP4End: frees unused data
 *****************************************************************************/
static void __MP4End ( vlc_object_t * p_this )
{
#define FREE( p ) \
    if( p ) { free( p ); }
    unsigned int i_track;
    input_thread_t *  p_input = (input_thread_t *)p_this;
    demux_sys_t *p_demux = p_input->p_demux_data;

    msg_Dbg( p_input, "Freeing all memory" );
    MP4_BoxFree( p_input, &p_demux->box_root );
    for( i_track = 0; i_track < p_demux->i_tracks; i_track++ )
    {
        unsigned int i_chunk;
        for( i_chunk = 0;
                i_chunk < p_demux->track[i_track].i_chunk_count; i_chunk++ )
        {
            if( p_demux->track[i_track].chunk )
            {
               FREE(p_demux->track[i_track].chunk[i_chunk].p_sample_count_dts);
               FREE(p_demux->track[i_track].chunk[i_chunk].p_sample_delta_dts );
            }
        }

        if( !p_demux->track[i_track].i_sample_size )
        {
            FREE( p_demux->track[i_track].p_sample_size );
        }
    }
    FREE( p_demux->track );
#undef FREE
}


/****************************************************************************
 * Local functions, specific to vlc
 ****************************************************************************/

/****************************************************************************
 * MP4_TrackSynchro : synchronize a track with movie time after seek or 
 *                    for newly selected track
 *****************************************************************************
 * TODO add support of Edit List (edts/elts) and Shadow Sync Sample(stsh)
 ****************************************************************************/
static int MP4_TrackSynchro( input_thread_t *p_input, track_data_mp4_t *p_track )
{
    demux_sys_t *p_demux = p_input->p_demux_data;
    unsigned int i_chunk_last;
    MP4_Box_t   *p_stss;

    if( !p_track->b_ok ||
        !p_track->p_es ||
        !p_track->p_es->p_decoder_fifo )
    {
        return( 0 );
    }
    p_track->i_sample = 0;
    i_chunk_last = p_track->i_chunk;
    p_track->i_chunk = 0;
    for( ;; )
    {
        if( p_track->i_sample >= p_track->i_sample_count )
        {
            msg_Warn( p_input, 
                        "track[Id 0x%x] will be disabled (seeking too far)",
                        p_track->i_track_ID );
            MP4_StopDecoder( p_input, p_track );
            break;
        }
        if( MP4_GetTrackPTS( p_track ) >= MP4_GetMoviePTS( p_demux ) )
        {
            break;
        }
        /* go one sample after */
        p_track->i_sample++;
        if( p_track->i_sample >= p_track->chunk[p_track->i_chunk].i_sample_first +
                p_track->chunk[p_track->i_chunk].i_sample_count )
        {
            p_track->i_chunk++;
        }
        
    }
    if( p_track->i_sample >= p_track->i_sample_count )
    {
        return( 0 );
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
            if( p_stss->data.p_stss->i_sample_number[i_index] >= p_track->i_sample )
            {
                if( i_index > 0 )
                {
                    msg_Dbg( p_input, "stts gives %d --> %d (sample number)", 
                            p_track->i_sample, 
                            p_stss->data.p_stss->i_sample_number[i_index-1] );
                    p_track->i_sample = p_stss->data.p_stss->i_sample_number[i_index-1];
                    /* new track.i_sample is less than old so i_chunk can only decreased */
                    while( p_track->i_chunk > 0 && 
                            p_track->i_sample < p_track->chunk[p_track->i_chunk].i_sample_first )
                    {
                        p_track->i_chunk--;
                    }
                }
                else
                {
                    msg_Dbg( p_input, "stts gives %d --> %d (sample number)", 
                            p_track->i_sample, 
                            p_stss->data.p_stss->i_sample_number[i_index-1] );
                    p_track->i_sample = p_stss->data.p_stss->i_sample_number[i_index];
                    /* new track.i_sample is more than old so i_chunk can only increased */
                    while( p_track->i_chunk < p_track->i_chunk_count - 1 &&
                           p_track->i_sample >= p_track->chunk[p_track->i_chunk].i_sample_first +
                                                p_track->chunk[p_track->i_chunk].i_sample_count )
                    {
                        p_track->i_chunk++;
                    }
                }
                break;
            }
        }
    }
    else
    {
        msg_Dbg( p_input, 
                    "track[Id 0x%x] doesn't provided Sync Sample Box (stss)", 
                    p_track->i_track_ID );
    }

    /* *** If i_sample_description_index has changed restart decoder *** */
    if( p_track->chunk[i_chunk_last].i_sample_description_index !=
            p_track->chunk[p_track->i_chunk].i_sample_description_index )
    {
        msg_Warn( p_input,
                "SampleEntry has changed, restarting decoder" );
        MP4_StopDecoder( p_input, p_track );
        MP4_StartDecoder( p_input, p_track );
    }
    return( 1 );
}

/****************************************************************************
 * Parse track information and create all needed data to run a track
 * If it succeed b_ok is set to 1 else to 0
 ****************************************************************************/
static void MP4_ParseTrack( input_thread_t *p_input,
                     track_data_mp4_t *p_demux_track,
                     MP4_Box_t  * p_trak )
{
    unsigned int i;

    MP4_Box_t *p_tkhd = MP4_BoxGet( p_trak, "tkhd" );
    MP4_Box_t *p_tref = MP4_BoxGet( p_trak, "tref" );
    MP4_Box_t *p_elst;

    MP4_Box_t *p_mdhd;
    MP4_Box_t *p_hdlr;

    MP4_Box_t *p_vmhd;
    MP4_Box_t *p_smhd; 

    /* hint track unsuported */

    /* by default, track isn't usable */
    p_demux_track->b_ok = 0;

    /* by default, we don't known the categorie */
    p_demux_track->i_cat = UNKNOWN_ES;
    
    if( !p_tkhd )
    {
        return;
    }

    /* do we launch this track by default ? */
    p_demux_track->b_enable = 
        ( ( p_tkhd->data.p_tkhd->i_flags&MP4_TRACK_ENABLED ) != 0 );

    p_demux_track->i_track_ID = p_tkhd->data.p_tkhd->i_track_ID;
    p_demux_track->i_width = p_tkhd->data.p_tkhd->i_width / 65536;
    p_demux_track->i_height = p_tkhd->data.p_tkhd->i_height / 65536;
    
    if( ( p_elst = MP4_BoxGet( p_trak, "edts/elst" ) ) )
    {
/*        msg_Warn( p_input, "Unhandled box: edts --> FIXME" ); */
    }

    if( p_tref )
    {
/*        msg_Warn( p_input, "Unhandled box: tref --> FIXME" ); */
    } 

    p_mdhd = MP4_BoxGet( p_trak, "mdia/mdhd" );
    p_hdlr = MP4_BoxGet( p_trak, "mdia/hdlr" );
    
    if( ( !p_mdhd )||( !p_hdlr ) )
    {
        return;
    }

    p_demux_track->i_timescale = p_mdhd->data.p_mdhd->i_timescale;

    for( i = 0; i < 3; i++ ) 
    {
        p_demux_track->i_language[i] = p_mdhd->data.p_mdhd->i_language[i];
    }
    p_mdhd->data.p_mdhd->i_language[3] = 0;
    
    switch( p_hdlr->data.p_hdlr->i_handler_type )
    {
        case( FOURCC_soun ):
            if( !( p_smhd = MP4_BoxGet( p_trak, "mdia/minf/smhd" ) ) )
            {
                return;
            }
            p_demux_track->i_cat = AUDIO_ES;
            break;

        case( FOURCC_vide ):
            if( !( p_vmhd = MP4_BoxGet( p_trak, "mdia/minf/vmhd" ) ) )
            {
                return;
            }
            p_demux_track->i_cat = VIDEO_ES;
            break;
            
        default:
            return;
    }
/*  TODO
    add support for:
    p_dinf = MP4_BoxGet( p_minf, "dinf" );
*/
    if( !( p_demux_track->p_stbl = MP4_BoxGet( p_trak,"mdia/minf/stbl" ) ) )
    {
        return;
    }
    
    if( !( p_demux_track->p_stsd = MP4_BoxGet( p_trak,"mdia/minf/stbl/stsd") ) )
    {
        return;
    }
    
    /* Create chunk  index table */
    if( !MP4_CreateChunksIndex( p_input,p_demux_track  ) )
    {
        return; /* cannot create chunks index */
    }
    
    /* create sample index table needed for reading and seeking */
    if( !MP4_CreateSamplesIndex( p_input, p_demux_track ) )
    {
        return; /* cannot create samples index */
    }
     
    p_demux_track->b_ok = 1;        
}
                     


/* now create basic chunk data, the rest will be filled by MP4_CreateSamplesIndex */
static int MP4_CreateChunksIndex( input_thread_t *p_input,
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
        return( 0 );
    }
     
    p_demux_track->i_chunk_count = p_co64->data.p_co64->i_entry_count;
    if( !p_demux_track->i_chunk_count )
    {
        msg_Warn( p_input, "No chunk defined" );
        return( 0 );
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
        return( 0 );
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

    p_demux_track->chunk[i_chunk].i_sample_first = 0;
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
    return( 1 );

}



static int MP4_CreateSamplesIndex( input_thread_t *p_input,
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
        return( 0 ); 
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

    return( 1 );
}

static void MP4_StartDecoder( input_thread_t *p_input,
                                 track_data_mp4_t *p_demux_track )
{
    MP4_Box_t *  p_sample;
    unsigned int i;
    unsigned int i_chunk;

    unsigned int i_decoder_specific_info_len;
    uint8_t *    p_decoder_specific_info;
    pes_packet_t *p_pes_init;

    uint8_t             *p_init;
    BITMAPINFOHEADER    *p_bih;
    WAVEFORMATEX        *p_wf;

    MP4_Box_t   *p_esds;


    if( (!p_demux_track->b_ok )||( p_demux_track->i_cat == UNKNOWN_ES ) )
    {
        return;
    }

    msg_Dbg( p_input, "Starting decoder for track[Id 0x%x]",
                      p_demux_track->i_track_ID );

    /* launch decoder according in chunk we are */
    i_chunk = p_demux_track->i_chunk;

    if( !p_demux_track->chunk[i_chunk].i_sample_description_index )
    {
        msg_Warn( p_input, 
                  "invalid SampleEntry index (track[Id 0x%x])",
                  p_demux_track->i_track_ID );
        return;
    } 
    p_sample = MP4_BoxGet(  p_demux_track->p_stsd, 
                            "[%d]", 
                p_demux_track->chunk[i_chunk].i_sample_description_index - 1 );

    if( ( !p_sample )||( !p_sample->data.p_data ) )
    {
        msg_Warn( p_input, 
                  "cannot find SampleEntry (track[Id 0x%x])",
                  p_demux_track->i_track_ID );
        return;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_demux_track->p_es = input_AddES( p_input,
                                       p_input->stream.p_selected_program, 
                                       p_demux_track->i_track_ID,
                                       0 );
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    /* Initialise ES, first language as description */
    for( i = 0; i < 3; i++ )
    {
        p_demux_track->p_es->psz_desc[i] = p_demux_track->i_language[i];
    }
    p_demux_track->p_es->psz_desc[3] = '\0';

    p_demux_track->p_es->i_stream_id = p_demux_track->i_track_ID;

    /* It's a little ugly but .. there are special cases */
    switch( p_sample->i_type )
    {
        case( VLC_FOURCC( '.', 'm', 'p', '3' ) ):
        case( VLC_FOURCC( 'm', 's', 0x00, 0x55 ) ):
            p_demux_track->p_es->i_fourcc = VLC_FOURCC( 'm', 'p', 'g', 'a' );
            break;
        default:
            p_demux_track->p_es->i_fourcc = p_sample->i_type;
            break;
    }

    p_demux_track->p_es->i_cat = p_demux_track->i_cat;

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
                p_demux_track->p_es->i_fourcc = VLC_FOURCC( 'm','p','4','v' );
                break;
            case( 0x40):
                p_demux_track->p_es->i_fourcc = VLC_FOURCC( 'm','p','4','a' );
                break;
            case( 0x60):
            case( 0x61):
            case( 0x62):
            case( 0x63):
            case( 0x64):
            case( 0x65): /* MPEG2 video */
                p_demux_track->p_es->i_fourcc = VLC_FOURCC( 'm','p','g','v' );
                break;
            /* Theses are MPEG2-AAC */
            case( 0x66): /* main profile */
            case( 0x67): /* Low complexity profile */
            case( 0x68): /* Scaleable Sampling rate profile */
                p_demux_track->p_es->i_fourcc = VLC_FOURCC( 'm','p','4','a' );
                break;
            /* true MPEG 2 audio */
            case( 0x69): 
                p_demux_track->p_es->i_fourcc = VLC_FOURCC( 'm','p','g','a' );
                break;
            case( 0x6a): /* MPEG1 video */
                p_demux_track->p_es->i_fourcc = VLC_FOURCC( 'm','p','g','v' );
                break;
            case( 0x6b): /* MPEG1 audio */
                p_demux_track->p_es->i_fourcc = VLC_FOURCC( 'm','p','g','a' );
                break;
            case( 0x6c ): /* jpeg */
                p_demux_track->p_es->i_fourcc = VLC_FOURCC( 'j','p','e','g' );
                break;
            default:
                /* Unknown entry, but don't touch i_fourcc */
                msg_Warn( p_input, 
                          "objectTypeIndication(0x%x) unknow (Track[ID 0x%x])",
                          p_decconfig->i_objectTypeIndication,
                          p_demux_track->i_track_ID );
                break;
        }
        i_decoder_specific_info_len = 
                p_decconfig->i_decoder_specific_info_len;
        p_decoder_specific_info = 
                p_decconfig->p_decoder_specific_info;
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

    switch( p_demux_track->i_cat )
    {
        case( VIDEO_ES ):    
            /* now create a bitmapinfoheader_t for decoder and 
               add information found in p_esds */
            p_init = malloc( sizeof( BITMAPINFOHEADER ) + i_decoder_specific_info_len );
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
                p_bih->biWidth = p_demux_track->i_width;
            }
            if( p_bih->biHeight == 0 )
            {
                // fall on display size
                p_bih->biHeight = p_demux_track->i_height;
            }

            if( i_decoder_specific_info_len )
            {
                data_packet_t   *p_data;

                memcpy( p_init + sizeof( BITMAPINFOHEADER ), 
                        p_decoder_specific_info,
                        i_decoder_specific_info_len);

                /* If stream is mpeg4 video we send specific_info,
                   as it's needed to decode it (vol) */
                switch( p_demux_track->p_es->i_fourcc )
                {
                    case VLC_FOURCC( 'm','p','4','v' ):
                    case VLC_FOURCC( 'D','I','V','X' ):
                    case VLC_FOURCC( 'd','i','v','x' ):
                        p_pes_init = input_NewPES( p_input->p_method_data );
                        p_data = input_NewPacket( p_input->p_method_data,
                                                  i_decoder_specific_info_len);
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
            p_init = malloc( sizeof( WAVEFORMATEX ) + i_decoder_specific_info_len);
            p_wf = (WAVEFORMATEX*)p_init;

            p_wf->wFormatTag = 0;
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
    if( p_demux_track->i_cat == AUDIO_ES )
    {
        p_demux_track->p_es->p_waveformatex = (void*)p_init;
    }
    else if( p_demux_track->i_cat == VIDEO_ES )
    {
        p_demux_track->p_es->p_bitmapinfoheader = (void*)p_init;
    }
    vlc_mutex_lock( &p_input->stream.stream_lock );
    input_SelectES( p_input, p_demux_track->p_es );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( p_pes_init != NULL )
    {
        input_DecodePES( p_demux_track->p_es->p_decoder_fifo, p_pes_init );
    }
    p_demux_track->b_ok = 1;
    p_demux_track->b_selected = 1;
}

static void MP4_StopDecoder( input_thread_t *p_input,
                             track_data_mp4_t *p_demux_track )
{
    msg_Dbg( p_input, "Stopping decoder (track[Id 0x%x])",
                      p_demux_track->i_track_ID );

    input_UnselectES( p_input, p_demux_track->p_es );
    p_demux_track->p_es = NULL;

    p_demux_track->b_selected = 0;
}

static int  MP4_ReadSample( input_thread_t *p_input,
                            track_data_mp4_t *p_demux_track,
                            pes_packet_t **pp_pes )
{
    size_t i_size;
    off_t i_pos;

    data_packet_t *p_data;


    /* this track have already reach the end */
    if( p_demux_track->i_sample >= p_demux_track->i_sample_count )
    {
        *pp_pes = NULL;
        return( 0 );
    }
    /* caculate size and position for this sample */
    i_size = p_demux_track->i_sample_size ? 
                    p_demux_track->i_sample_size : 
                    p_demux_track->p_sample_size[p_demux_track->i_sample];

    i_pos  = MP4_GetTrackPos( p_demux_track );

    /* go,go go ! */
    if( ! MP4_SeekAbsolute( p_input, i_pos ) )
    {
        return( 0 );
    }

    /* now create a pes */
    if( !(*pp_pes = input_NewPES( p_input->p_method_data ) ) )
    {
        return( 0 );
    }
    /* and a data packet for the data */
    if( !(p_data = input_NewPacket( p_input->p_method_data, i_size ) ) )
    {
        input_DeletePES( p_input->p_method_data, *pp_pes );
        *pp_pes = NULL;
        return( 0 );
    }
    
    /* initialisation of all the field */
    (*pp_pes)->i_dts =
        (*pp_pes)->i_pts = MP4_GetTrackPTS( p_demux_track );
    (*pp_pes)->p_first = (*pp_pes)->p_last  = p_data;
    (*pp_pes)->i_nb_data = 1;
    (*pp_pes)->i_pes_size = i_size;

    if( !i_size )    
    {
        return( 1 );
    }
    
/*    msg_Dbg( p_input, "will read %d bytes", i_size ); */
    if( !MP4_ReadData( p_input, p_data->p_payload_start, i_size ) )
    {
        input_DeletePES( p_input->p_method_data, *pp_pes );
        input_DeletePacket( p_input->p_method_data, p_data );
        return( 0 );
    }

	return( 1 );
}


static int  MP4_DecodeSample( input_thread_t *p_input,
                              track_data_mp4_t *p_demux_track,
                              pes_packet_t *p_pes )
{

    if( !p_pes )
    {
        return( 0 );
    }

    /* don't forget to convert in mpeg clock */
    /* FIXME correct ffmpeg to use dts instead of pts that it incorrect 
       and, set it here ( and correct avi demux ) */
    p_pes->i_dts =
        p_pes->i_pts = input_ClockGetTS( p_input,
                                         p_input->stream.p_selected_program,
                                         p_pes->i_pts * 9/100);

    
    input_DecodePES( p_demux_track->p_es->p_decoder_fifo, p_pes );
    
    /* now update sample position */
    p_demux_track->i_sample++; /* easy ;) */
    if( p_demux_track->i_sample >= p_demux_track->i_sample_count )
    {
        /* we have reach end of the track so free decoder stuff */
        MP4_StopDecoder( p_input, p_demux_track );
        return( 1 );
    }
    /* Have we changed chunk ? */
    if( p_demux_track->i_sample >=
            p_demux_track->chunk[p_demux_track->i_chunk].i_sample_first +
                p_demux_track->chunk[p_demux_track->i_chunk].i_sample_count )
    {
        /* we haven't reached the end of the track, so see if we 
           have to change the decoder for the next frame because 
           i_sample_description_index has changed */

        p_demux_track->i_chunk++;
        if( p_demux_track->chunk[p_demux_track->i_chunk-1].i_sample_description_index 
              != p_demux_track->chunk[p_demux_track->i_chunk].i_sample_description_index  )
        {
            /* FIXME */
            msg_Warn( p_input, 
                      "SampleEntry has changed, starting a new decoder" );
            MP4_StopDecoder( p_input, p_demux_track );
            MP4_StartDecoder( p_input, p_demux_track );
        }
    }

    
    return( 1 );
}



