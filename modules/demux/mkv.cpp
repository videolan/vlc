/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: mkv.cpp,v 1.5 2003/06/22 23:22:11 fenrir Exp $
 *
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

#include <vlc/vlc.h>

#include <vlc/input.h>

#include <codecs.h>                        /* BITMAPINFOHEADER, WAVEFORMATEX */

#include <iostream>
#include <cassert>
#include <typeinfo>

/* libebml and matroska */
#include "ebml/EbmlHead.h"
#include "ebml/EbmlSubHead.h"
#include "ebml/EbmlStream.h"
#include "ebml/EbmlContexts.h"
#include "ebml/EbmlVersion.h"
#include "ebml/EbmlVoid.h"

#include "matroska/FileKax.h"
#include "matroska/KaxAttachements.h"
#include "matroska/KaxBlock.h"
#include "matroska/KaxBlockData.h"
#include "matroska/KaxChapters.h"
#include "matroska/KaxCluster.h"
#include "matroska/KaxClusterData.h"
#include "matroska/KaxContexts.h"
#include "matroska/KaxCues.h"
#include "matroska/KaxCuesData.h"
#include "matroska/KaxInfo.h"
#include "matroska/KaxInfoData.h"
#include "matroska/KaxSeekHead.h"
#include "matroska/KaxSegment.h"
#include "matroska/KaxTag.h"
#include "matroska/KaxTracks.h"
#include "matroska/KaxTrackAudio.h"
#include "matroska/KaxTrackVideo.h"

#include "ebml/StdIOCallback.h"

using namespace LIBMATROSKA_NAMESPACE;
using namespace std;


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate  ( vlc_object_t * );
static void Deactivate( vlc_object_t * );
static int  Demux     ( input_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( N_("mkv-demuxer"), NULL, VLC_TRUE );
        add_bool( "mkv-index", 0, NULL,
                  N_("Create index if no cues found"),
                  N_("Create index if no cues found"), VLC_TRUE );
    set_description( _("mka/mkv stream demuxer" ) );
    set_capability( "demux", 50 );
    set_callbacks( Activate, Deactivate );
    add_shortcut( "mka" );
    add_shortcut( "mkv" );
vlc_module_end();


/*****************************************************************************
 * Stream managment
 *****************************************************************************/
class vlc_stream_io_callback: public IOCallback
{
  private:
    input_thread_t *p_input;

  public:
    vlc_stream_io_callback( input_thread_t * );

    virtual uint32_t read            ( void *p_buffer, size_t i_size);
    virtual void     setFilePointer  ( int64_t i_offset, seek_mode mode = seek_beginning );
    virtual size_t   write           ( const void *p_buffer, size_t i_size);
    virtual uint64_t getFilePointer  ( void );
    virtual void     close           ( void );
};


vlc_stream_io_callback::vlc_stream_io_callback( input_thread_t *p_input_ )
{
    p_input = p_input_;
}

uint32_t vlc_stream_io_callback::read( void *p_buffer, size_t i_size )
{
    data_packet_t *p_data;

    int i_count;
    int i_read = 0;


    if( !i_size )
    {
        return 0;
    }

    do
    {
        i_count = input_SplitBuffer(p_input, &p_data, __MIN( i_size, 10240 ) );
        if( i_count <= 0 )
        {
            return i_read;
        }
        memcpy( p_buffer, p_data->p_payload_start, i_count );
        input_DeletePacket( p_input->p_method_data, p_data );

        (uint8_t*)p_buffer += i_count;
        i_size            -= i_count;
        i_read            += i_count;

    } while( i_size );

    return i_read;
}


void vlc_stream_io_callback::setFilePointer(int64_t i_offset, seek_mode mode )
{
    int64_t i_pos;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    switch( mode )
    {
        case seek_beginning:
            i_pos = i_offset;
            break;
        case seek_end:
            i_pos = p_input->stream.p_selected_area->i_size - i_offset;
            break;
        default:
            i_pos= p_input->stream.p_selected_area->i_tell + i_offset;
            break;
    }
    if( i_pos < 0 || i_pos > p_input->stream.p_selected_area->i_size )
    {
        msg_Err( p_input, "seeking to wrong place (i_pos=%lld)", i_pos );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    input_AccessReinit( p_input );
    p_input->pf_seek( p_input, i_pos );
}

size_t vlc_stream_io_callback::write( const void *p_buffer, size_t i_size )
{
    return 0;
}

uint64_t vlc_stream_io_callback::getFilePointer( void )
{
    uint64_t i_pos;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    i_pos= p_input->stream.p_selected_area->i_tell;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return i_pos;
}

void vlc_stream_io_callback::close( void )
{
    return;
}

/*****************************************************************************
 * Ebml Stream parser
 *****************************************************************************/
class EbmlParser
{
    public:
        EbmlParser( EbmlStream *es, EbmlElement *el_start );
        ~EbmlParser( void );
        void Up( void );
        void Down( void );
        EbmlElement *Get( void );
        void        Keep( void );

        int GetLevel( void );

    private:

    EbmlStream  *m_es;
    int         mi_level;
    EbmlElement *m_el[6];

    EbmlElement *m_got;

    int         mi_user_level;
    vlc_bool_t  mb_keep;

};

EbmlParser::EbmlParser( EbmlStream *es, EbmlElement *el_start )
{
    int i;

    m_es = es;
    m_got = NULL;
    m_el[0] = el_start;

    for( i = 1; i < 6; i++ )
    {
        m_el[i] = NULL;
    }
    mi_level = 1;
    mi_user_level = 1;
    mb_keep = VLC_FALSE;
}

EbmlParser::~EbmlParser( void )
{
    int i;

    for( i = 1; i < mi_level; i++ )
    {
        if( !mb_keep )
        {
            delete m_el[i];
        }
        mb_keep = VLC_FALSE;
    }
}

void EbmlParser::Up( void )
{
    if( mi_user_level == mi_level )
    {
        fprintf( stderr," arrrrrrrrrrrrrg Up cannot escape itself\n" );
    }

    mi_user_level--;
}

void EbmlParser::Down( void )
{
    mi_user_level++;
    mi_level++;
}

void EbmlParser::Keep( void )
{
    mb_keep = VLC_TRUE;
}

int EbmlParser::GetLevel( void )
{
    return mi_user_level;
}

EbmlElement *EbmlParser::Get( void )
{
    int i_ulev = 0;

    if( mi_user_level != mi_level )
    {
        return NULL;
    }
    if( m_got )
    {
        EbmlElement *ret = m_got;
        m_got = NULL;

        return ret;
    }

    if( m_el[mi_level] )
    {
        m_el[mi_level]->SkipData( *m_es, m_el[mi_level]->Generic().Context );
        if( !mb_keep )
        {
            delete m_el[mi_level];
        }
        mb_keep = VLC_FALSE;
    }

    m_el[mi_level] = m_es->FindNextElement( m_el[mi_level - 1]->Generic().Context, i_ulev, 0xFFFFFFFFL, true, 1 );
    if( i_ulev > 0 )
    {
        while( i_ulev > 0 )
        {
            if( mi_level == 1 )
            {
                mi_level = 0;
                return NULL;
            }

            delete m_el[mi_level - 1];
            m_got = m_el[mi_level -1] = m_el[mi_level];
            m_el[mi_level] = NULL;

            mi_level--;
            i_ulev--;
        }
        return NULL;
    }
    else if( m_el[mi_level] == NULL )
    {
        fprintf( stderr," m_el[mi_level] == NULL\n" );
    }

    return m_el[mi_level];
}

/*****************************************************************************
 * Some functions to manipulate memory
 *****************************************************************************/
#define GetWLE( p )     __GetWLE( (uint8_t*)p )
#define GetDWLE( p )    __GetDWLE( (uint8_t*)p )
#define GetFOURCC( p )  __GetFOURCC( (uint8_t*)p )
static uint16_t __GetWLE( uint8_t *p )
{
    return (uint16_t)p[0] | ( ((uint16_t)p[1]) << 8 );
}
static uint32_t __GetDWLE( uint8_t *p )
{
    return (uint32_t)p[0] | ( ((uint32_t)p[1]) << 8 ) |
            ( ((uint32_t)p[2]) << 16 ) | ( ((uint32_t)p[3]) << 24 );
}
static vlc_fourcc_t __GetFOURCC( uint8_t *p )
{
    return VLC_FOURCC( p[0], p[1], p[2], p[3] );
}


/*****************************************************************************
 * definitions of structures and functions used by this plugins
 *****************************************************************************/
typedef struct
{
    int         i_cat;
    vlc_bool_t  b_default;
    int         i_number;

    int         i_extra_data;
    uint8_t     *p_extra_data;

    char         *psz_language;

    char         *psz_codec;
    vlc_fourcc_t i_codec;

    uint64_t     i_default_duration;
    /* video */
    int         i_width;
    int         i_height;
    int         i_display_width;
    int         i_display_height;
    float       f_fps;


    /* audio */
    int         i_channels;
    int         i_samplerate;
    int         i_bitspersample;

    es_descriptor_t *p_es;



    vlc_bool_t      b_inited;
    /* data to be send first */
    int             i_data_init;
    uint8_t         *p_data_init;

    /* hack : it's for seek */
    vlc_bool_t      b_search_keyframe;
} mkv_track_t;

typedef struct
{
    int     i_track;
    int     i_block_number;

    int64_t i_position;
    int64_t i_time;

    vlc_bool_t b_key;
} mkv_index_t;

struct demux_sys_t
{
    vlc_stream_io_callback  *in;
    EbmlStream              *es;
    EbmlParser              *ep;

    /* time scale */
    uint64_t                i_timescale;

    /* duration of the segment */
    float                   f_duration;

    /* all tracks */
    int                     i_track;
    mkv_track_t             *track;

    /* from seekhead */
    int64_t                 i_cues_position;
    int64_t                 i_chapters_position;

    /* current data */
    KaxSegment              *segment;
    KaxCluster              *cluster;

    mtime_t                 i_pts;

    int                     i_index;
    int                     i_index_max;
    mkv_index_t             *index;
};

#define MKVD_TIMECODESCALE 1000000

/*****************************************************************************
 * Activate: initializes matroska demux structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;
    uint8_t        *p_peek;

    int             i_track;
    vlc_bool_t      b_audio_selected;
    int             i_spu_channel, i_audio_channel;

    EbmlElement     *el = NULL, *el1 = NULL, *el2 = NULL, *el3 = NULL, *el4 = NULL;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    /* Set the demux function */
    p_input->pf_demux = Demux;

    /* peek the begining */
    if( input_Peek( p_input, &p_peek, 4 ) < 4 )
    {
        msg_Warn( p_input, "cannot peek" );
        return VLC_EGENERIC;
    }

    /* is a valid file */
    if( p_peek[0] != 0x1a || p_peek[1] != 0x45 || p_peek[2] != 0xdf || p_peek[3] != 0xa3 )
    {
        msg_Warn( p_input, "matroska module discarded (invalid header)" );
        return VLC_EGENERIC;
    }

    if( p_input->stream.i_method != INPUT_METHOD_FILE || !p_input->stream.b_seekable )
    {
        msg_Err( p_input, "can only read matroska over seekable file yet" );
        return VLC_EGENERIC;
    }

    p_input->p_demux_data = p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    p_sys->in = new vlc_stream_io_callback( p_input );
    p_sys->es = new EbmlStream( *p_sys->in );
    p_sys->f_duration   = -1;
    p_sys->i_timescale     = MKVD_TIMECODESCALE;
    p_sys->i_track      = 0;
    p_sys->track        = (mkv_track_t*)malloc( sizeof( mkv_track_t ) );
    p_sys->i_pts   = 0;
    p_sys->i_cues_position = -1;
    p_sys->i_chapters_position = -1;

    p_sys->i_index      = 0;
    p_sys->i_index_max  = 1024;
    p_sys->index        = (mkv_index_t*)malloc( sizeof( mkv_index_t ) * p_sys->i_index_max );

    if( p_sys->es == NULL )
    {
        msg_Err( p_input, "failed to create EbmlStream" );
        delete p_sys->in;
        free( p_sys );
        return VLC_EGENERIC;
    }
    /* Find the EbmlHead element */
    el = p_sys->es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if( el == NULL )
    {
        msg_Err( p_input, "cannot find EbmlHead" );
        goto error;
    }
    msg_Dbg( p_input, "EbmlHead" );
    /* skip it */
    el->SkipData( *p_sys->es, el->Generic().Context );
    delete el;

    /* Find a segment */
    el = p_sys->es->FindNextID( KaxSegment::ClassInfos, 0xFFFFFFFFL);
    if( el == NULL )
    {
        msg_Err( p_input, "cannot find KaxSegment" );
        goto error;
    }
    msg_Dbg( p_input, "+ Segment" );
    p_sys->segment = (KaxSegment*)el;
    p_sys->cluster = NULL;

    p_sys->ep = new EbmlParser( p_sys->es, el );

    while( ( el1 = p_sys->ep->Get() ) != NULL )
    {
        if( EbmlId( *el1 ) == KaxInfo::ClassInfos.GlobalId )
        {
            msg_Dbg( p_input, "|   + Informations" );

            p_sys->ep->Down();
            while( ( el2 = p_sys->ep->Get() ) != NULL )
            {
                if( EbmlId( *el2 ) == KaxTimecodeScale::ClassInfos.GlobalId )
                {
                    KaxTimecodeScale &tcs = *(KaxTimecodeScale*)el2;

                    tcs.ReadData( p_sys->es->I_O() );
                    p_sys->i_timescale = uint64(tcs);

                    msg_Dbg( p_input, "|   |   + TimecodeScale=%lld", p_sys->i_timescale );
                }
                else if( EbmlId( *el2 ) == KaxDuration::ClassInfos.GlobalId )
                {
                    KaxDuration &dur = *(KaxDuration*)el2;

                    dur.ReadData( p_sys->es->I_O() );
                    p_sys->f_duration = float(dur);

                    msg_Dbg( p_input, "|   |   + Duration=%f", p_sys->f_duration );
                }
                else
                {
                    msg_Dbg( p_input, "|   |   + Unknow (%s)", typeid(*el2).name() );
                }
            }
            p_sys->ep->Up();
        }
        else if( EbmlId( *el1 ) == KaxTracks::ClassInfos.GlobalId )
        {
            msg_Dbg( p_input, "|   + Tracks" );

            p_sys->ep->Down();
            while( ( el2 = p_sys->ep->Get() ) != NULL )
            {
                if( EbmlId( *el2 ) == KaxTrackEntry::ClassInfos.GlobalId )
                {
                    msg_Dbg( p_input, "|   |   + Track Entry" );

                    p_sys->i_track++;
                    p_sys->track = (mkv_track_t*)realloc( p_sys->track, sizeof( mkv_track_t ) * (p_sys->i_track + 1 ) );
#define tk  p_sys->track[p_sys->i_track - 1]
                    memset( &tk, 0, sizeof( mkv_track_t ) );
                    tk.i_cat = UNKNOWN_ES;
                    tk.b_default = VLC_FALSE;
                    tk.i_number = p_sys->i_track - 1;
                    tk.i_extra_data = 0;
                    tk.p_extra_data = NULL;
                    tk.i_codec = 0;
                    tk.psz_codec = NULL;
                    tk.psz_language = NULL;
                    tk.i_default_duration = 0;

                    tk.b_inited = VLC_FALSE;
                    tk.i_data_init = 0;
                    tk.p_data_init = NULL;

                    p_sys->ep->Down();

                    while( ( el3 = p_sys->ep->Get() ) != NULL )
                    {
                        if( EbmlId( *el3 ) == KaxTrackNumber::ClassInfos.GlobalId )
                        {
                            KaxTrackNumber &tnum = *(KaxTrackNumber*)el3;
                            tnum.ReadData( p_sys->es->I_O() );

                            tk.i_number = uint32( tnum );
                            msg_Dbg( p_input, "|   |   |   + Track Number=%u", uint32( tnum ) );
                        }
                        else  if( EbmlId( *el3 ) == KaxTrackUID::ClassInfos.GlobalId )
                        {
                            KaxTrackUID &tuid = *(KaxTrackUID*)el3;
                            tuid.ReadData( p_sys->es->I_O() );

                            msg_Dbg( p_input, "|   |   |   + Track UID=%u", uint32( tuid ) );
                        }
                        else  if( EbmlId( *el3 ) == KaxTrackDefaultDuration::ClassInfos.GlobalId )
                        {
                            KaxTrackDefaultDuration &defd = *(KaxTrackDefaultDuration*)el3;
                            defd.ReadData( p_sys->es->I_O() );

                            tk.i_default_duration = uint64(defd);
                            msg_Dbg( p_input, "|   |   |   + Track Default Duration=%lld", uint64(defd) );
                        }
                        else  if( EbmlId( *el3 ) == KaxTrackType::ClassInfos.GlobalId )
                        {
                            char *psz_type;
                            KaxTrackType &ttype = *(KaxTrackType*)el3;
                            ttype.ReadData( p_sys->es->I_O() );
                            switch( uint8(ttype) )
                            {
                                case track_audio:
                                    psz_type = "audio";
                                    tk.i_cat = AUDIO_ES;
                                    break;
                                case track_video:
                                    psz_type = "video";
                                    tk.i_cat = VIDEO_ES;
                                    break;
                                case track_subtitle:
                                    psz_type = "subtitle";
                                    tk.i_cat = SPU_ES;
                                    break;
                                default:
                                    psz_type = "unknown";
                                    tk.i_cat = UNKNOWN_ES;
                                    break;
                            }

                            msg_Dbg( p_input, "|   |   |   + Track Type=%s", psz_type );
                        }
                        else  if( EbmlId( *el3 ) == KaxTrackAudio::ClassInfos.GlobalId )
                        {
                            msg_Dbg( p_input, "|   |   |   + Track Audio" );
                            tk.i_channels = 0;
                            tk.i_samplerate = 0;
                            tk.i_bitspersample = 0;

                            p_sys->ep->Down();

                            while( ( el4 = p_sys->ep->Get() ) != NULL )
                            {
                                if( EbmlId( *el4 ) == KaxAudioSamplingFreq::ClassInfos.GlobalId )
                                {
                                    KaxAudioSamplingFreq &afreq = *(KaxAudioSamplingFreq*)el4;
                                    afreq.ReadData( p_sys->es->I_O() );

                                    tk.i_samplerate = (int)float( afreq );
                                    msg_Dbg( p_input, "|   |   |   |   + afreq=%d", tk.i_samplerate );
                                }
                                else if( EbmlId( *el4 ) == KaxAudioChannels::ClassInfos.GlobalId )
                                {
                                    KaxAudioChannels &achan = *(KaxAudioChannels*)el4;
                                    achan.ReadData( p_sys->es->I_O() );

                                    tk.i_channels = uint8( achan );
                                    msg_Dbg( p_input, "|   |   |   |   + achan=%u", uint8( achan ) );
                                }
                                else if( EbmlId( *el4 ) == KaxAudioBitDepth::ClassInfos.GlobalId )
                                {
                                    KaxAudioBitDepth &abits = *(KaxAudioBitDepth*)el4;
                                    abits.ReadData( p_sys->es->I_O() );

                                    tk.i_bitspersample = uint8( abits );
                                    msg_Dbg( p_input, "|   |   |   |   + abits=%u", uint8( abits ) );
                                }
                                else
                                {
                                    msg_Dbg( p_input, "|   |   |   |   + Unknow (%s)", typeid(*el4).name() );
                                }
                            }
                            p_sys->ep->Up();
                        }
                        else  if( EbmlId( *el3 ) == KaxTrackVideo::ClassInfos.GlobalId )
                        {
                            msg_Dbg( p_input, "|   |   |   + Track Video" );
                            tk.i_width  = 0;
                            tk.i_height = 0;
                            tk.i_display_width  = 0;
                            tk.i_display_height = 0;
                            tk.f_fps = 0.0;

                            p_sys->ep->Down();

                            while( ( el4 = p_sys->ep->Get() ) != NULL )
                            {
                                if( EbmlId( *el4 ) == KaxVideoPixelWidth::ClassInfos.GlobalId )
                                {
                                    KaxVideoPixelWidth &vwidth = *(KaxVideoPixelWidth*)el4;
                                    vwidth.ReadData( p_sys->es->I_O() );

                                    tk.i_width = uint16( vwidth );
                                    msg_Dbg( p_input, "|   |   |   |   + width=%d", uint16( vwidth ) );
                                }
                                else if( EbmlId( *el4 ) == KaxVideoPixelHeight::ClassInfos.GlobalId )
                                {
                                    KaxVideoPixelWidth &vheight = *(KaxVideoPixelWidth*)el4;
                                    vheight.ReadData( p_sys->es->I_O() );

                                    tk.i_height = uint16( vheight );
                                    msg_Dbg( p_input, "|   |   |   |   + height=%d", uint16( vheight ) );
                                }
                                else if( EbmlId( *el4 ) == KaxVideoDisplayWidth::ClassInfos.GlobalId )
                                {
                                    KaxVideoDisplayWidth &vwidth = *(KaxVideoDisplayWidth*)el4;
                                    vwidth.ReadData( p_sys->es->I_O() );

                                    tk.i_display_width = uint16( vwidth );
                                    msg_Dbg( p_input, "|   |   |   |   + display width=%d", uint16( vwidth ) );
                                }
                                else if( EbmlId( *el4 ) == KaxVideoDisplayHeight::ClassInfos.GlobalId )
                                {
                                    KaxVideoDisplayWidth &vheight = *(KaxVideoDisplayWidth*)el4;
                                    vheight.ReadData( p_sys->es->I_O() );

                                    tk.i_display_height = uint16( vheight );
                                    msg_Dbg( p_input, "|   |   |   |   + display height=%d", uint16( vheight ) );
                                }
                                else if( EbmlId( *el4 ) == KaxVideoFrameRate::ClassInfos.GlobalId )
                                {
                                    KaxVideoFrameRate &vfps = *(KaxVideoFrameRate*)el4;
                                    vfps.ReadData( p_sys->es->I_O() );

                                    tk.f_fps = float( vfps );
                                    msg_Dbg( p_input, "   |   |   |   + fps=%f", float( vfps ) );
                                }
                                else
                                {
                                    msg_Dbg( p_input, "|   |   |   |   + Unknow (%s)", typeid(*el4).name() );
                                }
                            }
                            p_sys->ep->Up();
                        }
                        else  if( EbmlId( *el3 ) == KaxCodecID::ClassInfos.GlobalId )
                        {
                            KaxCodecID &codecid = *(KaxCodecID*)el3;
                            codecid.ReadData( p_sys->es->I_O() );

                            tk.psz_codec = strdup( string( codecid ).c_str() );
                            msg_Dbg( p_input, "|   |   |   + Track CodecId=%s", string( codecid ).c_str() );
                        }
                        else  if( EbmlId( *el3 ) == KaxCodecPrivate::ClassInfos.GlobalId )
                        {
                            KaxCodecPrivate &cpriv = *(KaxCodecPrivate*)el3;
                            cpriv.ReadData( p_sys->es->I_O() );

                            tk.i_extra_data = cpriv.GetSize();
                            if( tk.i_extra_data > 0 )
                            {
                                tk.p_extra_data = (uint8_t*)malloc( tk.i_extra_data );
                                memcpy( tk.p_extra_data, cpriv.GetBuffer(), tk.i_extra_data );
                            }
                            msg_Dbg( p_input, "|   |   |   + Track CodecPrivate size=%lld", cpriv.GetSize() );
                        }
                        else  if( EbmlId( *el3 ) == KaxTrackFlagDefault::ClassInfos.GlobalId )
                        {
                            KaxTrackFlagDefault &fdef = *(KaxTrackFlagDefault*)el3;
                            fdef.ReadData( p_sys->es->I_O() );

                            tk.b_default = uint32( fdef );
                            msg_Dbg( p_input, "|   |   |   + Track Default=%u", uint32( fdef )  );
                        }
                        else  if( EbmlId( *el3 ) == KaxTrackLanguage::ClassInfos.GlobalId )
                        {
                            KaxTrackLanguage &lang = *(KaxTrackLanguage*)el3;

                            lang.ReadData( p_sys->es->I_O() );

                            tk.psz_language = strdup( string( lang ).c_str() );
                            msg_Dbg( p_input, "|   |   |   + Track Language=`%s'", string( lang ).c_str() );
                        }
                        else  if( EbmlId( *el3 ) == KaxTrackFlagLacing::ClassInfos.GlobalId )
                        {
                            KaxTrackFlagLacing &lac = *(KaxTrackFlagLacing*)el3;
                            lac.ReadData( p_sys->es->I_O() );

                            msg_Dbg( p_input, "|   |   |   + Track Lacing=%d", uint32( lac ) );
                        }
                        else  if( EbmlId( *el3 ) == KaxTrackMinCache::ClassInfos.GlobalId )
                        {
                            KaxTrackMinCache &cmin = *(KaxTrackMinCache*)el3;
                            cmin.ReadData( p_sys->es->I_O() );

                            msg_Dbg( p_input, "|   |   |   + Track MinCache=%d", uint32( cmin ) );
                        }
                        else  if( EbmlId( *el3 ) == KaxTrackMaxCache::ClassInfos.GlobalId )
                        {
                            KaxTrackMaxCache &cmax = *(KaxTrackMaxCache*)el3;
                            cmax.ReadData( p_sys->es->I_O() );

                            msg_Dbg( p_input, "|   |   |   + Track MaxCache=%d", uint32( cmax ) );
                        }
                        else
                        {
                            msg_Dbg( p_input, "|   |   |   + Unknow (%s)", typeid(*el3).name() );
                        }
                    }
                    p_sys->ep->Up();
                }
                else
                {
                    msg_Dbg( p_input, "|   |   + Unknow (%s)", typeid(*el2).name() );
                }
#undef tk
            }
            p_sys->ep->Up();
        }
        else if( EbmlId( *el1 ) == KaxSeekHead::ClassInfos.GlobalId )
        {
            msg_Dbg( p_input, "|   + Seek head" );
            p_sys->ep->Down();
            while( ( el = p_sys->ep->Get() ) != NULL )
            {
                if( EbmlId( *el ) == KaxSeek::ClassInfos.GlobalId )
                {
                    EbmlId id = EbmlVoid::ClassInfos.GlobalId;
                    int64_t i_pos = -1;

                    //msg_Dbg( p_input, "|   |   + Seek" );
                    p_sys->ep->Down();
                    while( ( el = p_sys->ep->Get() ) != NULL )
                    {
                        if( EbmlId( *el ) == KaxSeekID::ClassInfos.GlobalId )
                        {
                            KaxSeekID &sid = *(KaxSeekID*)el;

                            sid.ReadData( p_sys->es->I_O() );

                            id = EbmlId( sid.GetBuffer(), sid.GetSize() );
                        }
                        else  if( EbmlId( *el ) == KaxSeekPosition::ClassInfos.GlobalId )
                        {
                            KaxSeekPosition &spos = *(KaxSeekPosition*)el;

                            spos.ReadData( p_sys->es->I_O() );

                            i_pos = uint64( spos );
                        }
                        else
                        {
                            msg_Dbg( p_input, "|   |   |   + Unknow (%s)", typeid(*el).name() );
                        }
                    }
                    p_sys->ep->Up();

                    if( i_pos >= 0 )
                    {
                        if( id == KaxCues::ClassInfos.GlobalId )
                        {
                            msg_Dbg( p_input, "|   |   |   = cues at %lld", i_pos );
                            p_sys->i_cues_position = p_sys->segment->GetGlobalPosition( i_pos );
                        }
                        else if( id == KaxChapters::ClassInfos.GlobalId )
                        {
                            msg_Dbg( p_input, "|   |   |   = chapters at %lld", i_pos );
                            p_sys->i_chapters_position = p_sys->segment->GetGlobalPosition( i_pos );
                        }
                    }
                }
                else
                {
                    msg_Dbg( p_input, "|   |   + Unknow (%s)", typeid(*el).name() );
                }
            }
            p_sys->ep->Up();
        }
        else if( EbmlId( *el1 ) == KaxCues::ClassInfos.GlobalId )
        {
            msg_Dbg( p_input, "|   + Cues" );
        }
        else if( EbmlId( *el1 ) == KaxCluster::ClassInfos.GlobalId )
        {
            msg_Dbg( p_input, "|   + Cluster" );
            p_sys->cluster = (KaxCluster*)el1;
            p_sys->ep->Down();

            /* stop parsing the stream */
            break;
        }
        else if( EbmlId( *el1 ) == KaxAttachements::ClassInfos.GlobalId )
        {
            msg_Dbg( p_input, "|   + Attachements FIXME TODO (but probably never supported)" );
        }
        else if( EbmlId( *el1 ) == KaxChapters::ClassInfos.GlobalId )
        {
            msg_Dbg( p_input, "|   + Chapters FIXME TODO" );
        }
        else if( EbmlId( *el1 ) == KaxTag::ClassInfos.GlobalId )
        {
            msg_Dbg( p_input, "|   + Tags FIXME TODO" );
        }
        else
        {
            msg_Dbg( p_input, "|   + Unknow (%s)", typeid(*el1).name() );
        }
    }

    if( p_sys->cluster == NULL )
    {
        msg_Err( p_input, "cannot find any cluster, damaged file ?" );
        goto error;
    }

    if( p_sys->i_chapters_position >= 0 )
    {
        msg_Warn( p_input, "chapters unsupported" );
    }

#if 0
    if( p_sys->i_cues_position == -1 && config_GetInt( p_input, "mkv-index" ) != 0 )
    {
        int64_t i_sav_position = p_sys->in->getFilePointer();
        EbmlParser *ep;

        /* parse to find cues gathering info on all clusters */
        msg_Dbg( p_input, "no cues referenced, looking for one" );

        /* ugly but like easier */
        p_sys->in->setFilePointer( p_sys->cluster->GetElementPosition(), seek_beginning );

        ep = new EbmlParser( p_sys->es, p_sys->segment );
        while( ( el = ep->Get() ) != NULL )
        {
            if( EbmlId( *el ) == KaxCluster::ClassInfos.GlobalId )
            {
                int64_t i_pos = el->GetElementPosition();

                msg_Dbg( p_input, "found a cluster at %lld", i_pos );
            }
            else if( EbmlId( *el ) == KaxCues::ClassInfos.GlobalId )
            {
                msg_Dbg( p_input, "found a Cues !" );
            }
        }
        delete ep;
        msg_Dbg( p_input, "lookind done." );

        p_sys->in->setFilePointer( i_sav_position, seek_beginning );
    }
#endif

    /* *** Load the cue if found *** */
    if( p_sys->i_cues_position >= 0 )
    {
        int64_t i_sav_position = p_sys->in->getFilePointer();
        EbmlParser *ep;
        EbmlElement *cues;

        msg_Dbg( p_input, "loading cues" );
        p_sys->in->setFilePointer( p_sys->i_cues_position, seek_beginning );
        cues = p_sys->es->FindNextID( KaxCues::ClassInfos, 0xFFFFFFFFL);

        ep = new EbmlParser( p_sys->es, cues );
        while( ( el = ep->Get() ) != NULL )
        {
            if( EbmlId( *el ) == KaxCuePoint::ClassInfos.GlobalId )
            {
#define idx p_sys->index[p_sys->i_index]

                idx.i_track       = -1;
                idx.i_block_number= -1;
                idx.i_position    = -1;
                idx.i_time        = -1;
                idx.b_key         = VLC_TRUE;

                ep->Down();
                while( ( el = ep->Get() ) != NULL )
                {
                    if( EbmlId( *el ) == KaxCueTime::ClassInfos.GlobalId )
                    {
                        KaxCueTime &ctime = *(KaxCueTime*)el;

                        ctime.ReadData( p_sys->es->I_O() );

                        idx.i_time = uint64( ctime ) * (mtime_t)1000000000 / p_sys->i_timescale;
                    }
                    else if( EbmlId( *el ) == KaxCueTrackPositions::ClassInfos.GlobalId )
                    {
                        ep->Down();
                        while( ( el = ep->Get() ) != NULL )
                        {
                            if( EbmlId( *el ) == KaxCueTrack::ClassInfos.GlobalId )
                            {
                                KaxCueTrack &ctrack = *(KaxCueTrack*)el;

                                ctrack.ReadData( p_sys->es->I_O() );
                                idx.i_track = uint16( ctrack );
                            }
                            else if( EbmlId( *el ) == KaxCueClusterPosition::ClassInfos.GlobalId )
                            {
                                KaxCueClusterPosition &ccpos = *(KaxCueClusterPosition*)el;

                                ccpos.ReadData( p_sys->es->I_O() );
                                idx.i_position = p_sys->segment->GetGlobalPosition( uint64( ccpos ) );
                            }
                            else if( EbmlId( *el ) == KaxCueBlockNumber::ClassInfos.GlobalId )
                            {
                                KaxCueBlockNumber &cbnum = *(KaxCueBlockNumber*)el;

                                cbnum.ReadData( p_sys->es->I_O() );
                                idx.i_block_number = uint32( cbnum );
                            }
                            else
                            {
                                msg_Dbg( p_input, "         * Unknow (%s)", typeid(*el).name() );
                            }
                        }
                        ep->Up();
                    }
                    else
                    {
                        msg_Dbg( p_input, "     * Unknow (%s)", typeid(*el).name() );
                    }
                }
                ep->Up();

                msg_Dbg( p_input, " * added time=%lld pos=%lld track=%d bnum=%d",
                         idx.i_time, idx.i_position, idx.i_track, idx.i_block_number );

                p_sys->i_index++;
                if( p_sys->i_index >= p_sys->i_index_max )
                {
                    p_sys->i_index_max += 1024;
                    p_sys->index = (mkv_index_t*)realloc( p_sys->index, sizeof( mkv_index_t ) * p_sys->i_index_max );
                }
#undef idx
            }
            else
            {
                msg_Dbg( p_input, " * Unknow (%s)", typeid(*el).name() );
            }
        }
        delete ep;
        delete cues;

        msg_Dbg( p_input, "loading cues done." );
        p_sys->in->setFilePointer( i_sav_position, seek_beginning );
    }

    if( p_sys->i_index <= 0 )
    {
        /* FIXME FIXME FIXME */
        msg_Warn( p_input, "no cues found -> unseekable stream for now FIXME" );
        p_input->stream.b_seekable = VLC_FALSE;
        /* FIXME FIXME FIXME */
    }

    /* Create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        goto error;
    }
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    if( p_sys->f_duration > 1001.0 )
    {
        mtime_t i_duration = (mtime_t)( p_sys->f_duration / 1000.0 );
        p_input->stream.i_mux_rate = p_input->stream.p_selected_area->i_size / 50 / i_duration;
    }
    else
    {
        p_input->stream.i_mux_rate = 0;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* add all es */
    msg_Dbg( p_input, "found %d es", p_sys->i_track );
    for( i_track = 0; i_track < p_sys->i_track; i_track++ )
    {
#define tk  p_sys->track[i_track]
        if( tk.i_cat == UNKNOWN_ES )
        {
            msg_Warn( p_input, "invalid track[%d, n=%d]", i_track, tk.i_number );
            tk.p_es = NULL;
            continue;
        }
        tk.p_es = input_AddES( p_input,
                               p_input->stream.p_selected_program,
                               i_track + 1,
                               tk.i_cat,
                               tk.psz_language, 0 );
        if( !strcmp( tk.psz_codec, "V_MS/VFW/FOURCC" ) )
        {
            if( tk.i_extra_data < (int)sizeof( BITMAPINFOHEADER ) )
            {
                msg_Err( p_input, "missing/invalid BITMAPINFOHEADER" );
                tk.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
            }
            else
            {
                BITMAPINFOHEADER *p_bih = (BITMAPINFOHEADER*)tk.p_extra_data;

                p_bih->biSize           = GetDWLE( &p_bih->biSize );
                p_bih->biWidth          = GetDWLE( &p_bih->biWidth );
                p_bih->biHeight         = GetDWLE( &p_bih->biHeight );
                p_bih->biPlanes         = GetWLE( &p_bih->biPlanes );
                p_bih->biBitCount       = GetWLE( &p_bih->biBitCount );
                p_bih->biCompression    = GetFOURCC( &p_bih->biCompression );
                p_bih->biSizeImage      = GetDWLE( &p_bih->biSizeImage );
                p_bih->biXPelsPerMeter  = GetDWLE( &p_bih->biXPelsPerMeter );
                p_bih->biYPelsPerMeter  = GetDWLE( &p_bih->biYPelsPerMeter );
                p_bih->biClrUsed        = GetDWLE( &p_bih->biClrUsed );
                p_bih->biClrImportant   = GetDWLE( &p_bih->biClrImportant );


                tk.i_codec = p_bih->biCompression;
                tk.p_es->p_bitmapinfoheader = p_bih;
            }
        }
        else if( !strcmp( tk.psz_codec, "V_MPEG1" ) ||
                 !strcmp( tk.psz_codec, "V_MPEG2" ) )
        {
            tk.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'v' );
        }
        else if( !strncmp( tk.psz_codec, "V_MPEG4", 7 ) )
        {
            BITMAPINFOHEADER *p_bih;

            tk.i_extra_data = sizeof( BITMAPINFOHEADER );
            tk.p_extra_data = (uint8_t*)malloc( tk.i_extra_data );

            p_bih = (BITMAPINFOHEADER*)tk.p_extra_data;
            memset( p_bih, 0, sizeof( BITMAPINFOHEADER ) );
            p_bih->biSize  = sizeof( BITMAPINFOHEADER );
            p_bih->biWidth = tk.i_width;
            p_bih->biHeight= tk.i_height;

            if( !strcmp( tk.psz_codec, "V_MPEG4/MS/V3" ) )
            {
                tk.i_codec = VLC_FOURCC( 'D', 'I', 'V', '3' );
            }
            else
            {
                tk.i_codec = VLC_FOURCC( 'm', 'p', '4', 'v' );
            }
        }
        else if( !strcmp( tk.psz_codec, "A_MS/ACM" ) )
        {
            if( tk.i_extra_data < (int)sizeof( WAVEFORMATEX ) )
            {
                msg_Err( p_input, "missing/invalid WAVEFORMATEX" );
                tk.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
            }
            else
            {
                WAVEFORMATEX *p_wf = (WAVEFORMATEX*)tk.p_extra_data;

                p_wf->wFormatTag        = GetWLE( &p_wf->wFormatTag );
                p_wf->nChannels         = GetWLE( &p_wf->nChannels );
                p_wf->nSamplesPerSec    = GetDWLE( &p_wf->nSamplesPerSec );
                p_wf->nAvgBytesPerSec   = GetDWLE( &p_wf->nAvgBytesPerSec );
                p_wf->nBlockAlign       = GetWLE( &p_wf->nBlockAlign );
                p_wf->wBitsPerSample    = GetWLE( &p_wf->wBitsPerSample );
                p_wf->cbSize            = GetWLE( &p_wf->cbSize );

                switch( p_wf->wFormatTag )
                {
                    case WAVE_FORMAT_PCM:
                        tk.i_codec = VLC_FOURCC( 'a', 'r', 'a', 'w' );
                        break;
                    case WAVE_FORMAT_ADPCM:
                        tk.i_codec = VLC_FOURCC( 'm', 's', 0x00, 0x02 );
                        break;
                    case WAVE_FORMAT_ALAW:
                        tk.i_codec = VLC_FOURCC( 'a', 'l', 'a', 'w' );
                        break;
                    case WAVE_FORMAT_MULAW:
                        tk.i_codec = VLC_FOURCC( 'm', 'l', 'a', 'w' );
                        break;
                    case WAVE_FORMAT_IMA_ADPCM:
                        tk.i_codec = VLC_FOURCC( 'm', 's', 0x00, 0x11 );
                        break;
                    case WAVE_FORMAT_MPEG:
                    case WAVE_FORMAT_MPEGLAYER3:
                        tk.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'a' );
                        break;
                    case WAVE_FORMAT_A52:
                        tk.i_codec = VLC_FOURCC( 'a', '5', '2', ' ' );
                        break;
                    case WAVE_FORMAT_WMA1:
                        tk.i_codec = VLC_FOURCC( 'w', 'm', 'a', '1' );
                        break;
                    case WAVE_FORMAT_WMA2:
                        tk.i_codec = VLC_FOURCC( 'w', 'm', 'a', '2' );
                        break;
                    case WAVE_FORMAT_WMA3:
                        tk.i_codec = VLC_FOURCC( 'w', 'm', 'a', '3' );
                        break;
                    default:
                        msg_Err( p_input, "unknown wFormatTag=0x%x", p_wf->wFormatTag );
                        tk.i_codec = VLC_FOURCC( 'm', 's', p_wf->wFormatTag >> 8, p_wf->wFormatTag&0xff );
                        break;
                }
                tk.p_es->p_waveformatex = p_wf;
            }
        }
        else if( !strcmp( tk.psz_codec, "A_MPEG/L3" ) ||
                 !strcmp( tk.psz_codec, "A_MPEG/L2" ) ||
                 !strcmp( tk.psz_codec, "A_MPEG/L1" ) )
        {
            tk.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'a' );
        }
        else if( !strcmp( tk.psz_codec, "A_AC3" ) )
        {
            tk.i_codec = VLC_FOURCC( 'a', '5', '2', ' ' );
        }
        else if( !strcmp( tk.psz_codec, "A_DTS" ) )
        {
            tk.i_codec = VLC_FOURCC( 'd', 't', 's', ' ' );
        }
        else if( !strcmp( tk.psz_codec, "A_VORBIS" ) )
        {
            tk.i_codec = VLC_FOURCC( 'v', 'o', 'r', 'b' );
            tk.i_data_init = tk.i_extra_data;
            tk.p_data_init = tk.p_extra_data;
        }
        else if( !strncmp( tk.psz_codec, "A_AAC/MPEG2/", strlen( "A_AAC/MPEG2/" ) ) ||
                 !strncmp( tk.psz_codec, "A_AAC/MPEG4/", strlen( "A_AAC/MPEG4/" ) ) )
        {
            int i_profile, i_srate;
            static int i_sample_rates[] =
            {
                    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                        16000, 12000, 11025, 8000,  7350,  0,     0,     0
            };
            WAVEFORMATEX *p_wf;

            tk.i_codec = VLC_FOURCC( 'm', 'p', '4', 'a' );
            /* create data for faad (MP4DecSpecificDescrTag)*/

            if( !strcmp( &tk.psz_codec[12], "MAIN" ) )
            {
                i_profile = 0;
            }
            else if( !strcmp( &tk.psz_codec[12], "LC" ) )
            {
                i_profile = 1;
            }
            else if( !strcmp( &tk.psz_codec[12], "SSR" ) )
            {
                i_profile = 2;
            }
            else
            {
                i_profile = 3;
            }

            for( i_srate = 0; i_srate < 13; i_srate++ )
            {
                if( i_sample_rates[i_srate] == tk.i_samplerate )
                {
                    break;
                }
            }
            msg_Dbg( p_input, "profile=%d srate=%d", i_profile, i_srate );

            tk.i_extra_data = sizeof( WAVEFORMATEX ) + 2;
            tk.p_extra_data = (uint8_t*)malloc( tk.i_extra_data );
            p_wf = (WAVEFORMATEX*)tk.p_extra_data;

            p_wf->wFormatTag = WAVE_FORMAT_UNKNOWN;
            p_wf->nChannels  = tk.i_channels;
            p_wf->nSamplesPerSec = tk.i_samplerate;
            p_wf->nAvgBytesPerSec = 0;
            p_wf->nBlockAlign = 0;
            p_wf->wBitsPerSample = 0;
            p_wf->cbSize = 2;

            tk.p_extra_data[sizeof( WAVEFORMATEX )+ 0] = ((i_profile + 1) << 3) | ((i_srate&0xe) >> 1);
            tk.p_extra_data[sizeof( WAVEFORMATEX )+ 1] = ((i_srate & 0x1) << 7) | (tk.i_channels << 3);

            tk.p_es->p_waveformatex = p_wf;
        }
        else if( !strcmp( tk.psz_codec, "A_PCM/INT/BIG" ) ||
                 !strcmp( tk.psz_codec, "A_PCM/INT/LIT" ) ||
                 !strcmp( tk.psz_codec, "A_PCM/FLOAT/IEEE" ) )
        {
            WAVEFORMATEX *p_wf;

            tk.i_extra_data = sizeof( WAVEFORMATEX );
            tk.p_extra_data = (uint8_t*)malloc( tk.i_extra_data );

            p_wf = (WAVEFORMATEX*)tk.p_extra_data;

            if( !strncmp( &tk.psz_codec[6], "INT", 3 ) )
            {
                p_wf->wFormatTag = WAVE_FORMAT_PCM;
            }
            else
            {
                p_wf->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
            }
            p_wf->nChannels  = tk.i_channels;
            p_wf->nSamplesPerSec = tk.i_samplerate;
            p_wf->nAvgBytesPerSec = 0;
            p_wf->nBlockAlign = ( tk.i_bitspersample + 7 ) / 8 * tk.i_channels;
            p_wf->wBitsPerSample = tk.i_bitspersample;
            p_wf->cbSize = 0;

            tk.p_es->p_waveformatex = p_wf;

            if( !strcmp( tk.psz_codec, "A_PCM/INT/BIG" ) )
            {
                tk.i_codec = VLC_FOURCC( 't', 'w', 'o', 's' );
            }
            else
            {
                tk.i_codec = VLC_FOURCC( 'a', 'r', 'a', 'w' );
            }
        }
        else if( !strcmp( tk.psz_codec, "S_TEXT/UTF8" ) )
        {
            tk.i_codec = VLC_FOURCC( 's', 'u', 'b', 't' );
        }
        else
        {
            msg_Err( p_input, "unknow codec id=`%s'", tk.psz_codec );
            tk.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
        }

        tk.p_es->i_fourcc = tk.i_codec;
#undef tk
    }

    /* select track all video, one audio, no spu TODO : improve */
    b_audio_selected = VLC_FALSE;
    i_audio_channel = 0;
    i_spu_channel = 0;
    for( i_track = 0; i_track < p_sys->i_track; i_track++ )
    {
#define tk  p_sys->track[i_track]
        switch( tk.i_cat )
        {
            case VIDEO_ES:
                vlc_mutex_lock( &p_input->stream.stream_lock );
                input_SelectES( p_input, tk.p_es );
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                break;

            case AUDIO_ES:
                if( ( !b_audio_selected && config_GetInt( p_input, "audio-channel" ) < 0 ) ||
                    i_audio_channel == config_GetInt( p_input, "audio-channel" ) )
                {
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    input_SelectES( p_input, tk.p_es );
                    vlc_mutex_unlock( &p_input->stream.stream_lock );

                    b_audio_selected = tk.p_es->p_decoder_fifo ? VLC_TRUE : VLC_FALSE;
                }
                i_audio_channel++;
                break;
            case SPU_ES:
                if( i_spu_channel == config_GetInt( p_input, "spu-channel" ) )
                {
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    input_SelectES( p_input, tk.p_es );
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                }
                i_spu_channel++;
                break;
        }
#undef tk
    }

    if( !b_audio_selected )
    {
        msg_Warn( p_input, "cannot find/select audio track" );
    }

    return VLC_SUCCESS;

error:
    delete p_sys->es;
    delete p_sys->in;
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys   = p_input->p_demux_data;

    int             i_track;

    for( i_track = 0; i_track < p_sys->i_track; i_track++ )
    {
#define tk  p_sys->track[i_track]
        if( tk.psz_codec )
        {
            free( tk.psz_codec );
        }
        if( tk.psz_language )
        {
            free( tk.psz_language );
        }
#undef tk
    }
    free( p_sys->track );

    delete p_sys->ep;
    delete p_sys->es;
    delete p_sys->in;

    free( p_sys );
}

static int BlockGet( input_thread_t *p_input, KaxBlock **pp_block, int64_t *pi_ref1, int64_t *pi_ref2, int64_t *pi_duration )
{
    demux_sys_t    *p_sys   = p_input->p_demux_data;

    *pp_block = NULL;
    *pi_ref1  = -1;
    *pi_ref2  = -1;

    for( ;; )
    {
        EbmlElement *el;
        int         i_level;

        if( p_input->b_die )
        {
            return VLC_EGENERIC;
        }

        el = p_sys->ep->Get();
        i_level = p_sys->ep->GetLevel();

        if( el == NULL && *pp_block != NULL )
        {
            return VLC_SUCCESS;
        }

        if( el == NULL )
        {
            if( p_sys->ep->GetLevel() > 1 )
            {
                p_sys->ep->Up();
                continue;
            }
            msg_Warn( p_input, "EOF" );
            return VLC_EGENERIC;
        }

        /* do parsing */
        if( i_level == 1 )
        {
            if( EbmlId( *el ) == KaxCluster::ClassInfos.GlobalId )
            {
                p_sys->cluster = (KaxCluster*)el;
                p_sys->ep->Down();
            }
            else if( EbmlId( *el ) == KaxCues::ClassInfos.GlobalId )
            {
                msg_Warn( p_input, "find KaxCues FIXME" );
                return VLC_EGENERIC;
            }
            else
            {
                msg_Dbg( p_input, "unknown (%s)", typeid( el ).name() );
            }
        }
        else if( i_level == 2 )
        {
            if( EbmlId( *el ) == KaxClusterTimecode::ClassInfos.GlobalId )
            {
                KaxClusterTimecode &ctc = *(KaxClusterTimecode*)el;

                ctc.ReadData( p_sys->es->I_O() );
                p_sys->cluster->InitTimecode( uint64( ctc ), p_sys->i_timescale );
            }
            else if( EbmlId( *el ) == KaxBlockGroup::ClassInfos.GlobalId )
            {
                p_sys->ep->Down();
            }
        }
        else if( i_level == 3 )
        {
            if( EbmlId( *el ) == KaxBlock::ClassInfos.GlobalId )
            {
                *pp_block = (KaxBlock*)el;

                (*pp_block)->ReadData( p_sys->es->I_O() );
                (*pp_block)->SetParent( *p_sys->cluster );

                p_sys->ep->Keep();
            }
            else if( EbmlId( *el ) == KaxBlockDuration::ClassInfos.GlobalId )
            {
                KaxBlockDuration &dur = *(KaxBlockDuration*)el;

                dur.ReadData( p_sys->es->I_O() );
                *pi_duration = uint64( dur );
            }
            else if( EbmlId( *el ) == KaxReferenceBlock::ClassInfos.GlobalId )
            {
                KaxReferenceBlock &ref = *(KaxReferenceBlock*)el;

                ref.ReadData( p_sys->es->I_O() );
                if( *pi_ref1 == -1 )
                {
                    *pi_ref1 = int64( ref );
                }
                else
                {
                    *pi_ref2 = int64( ref );
                }
            }
        }
        else
        {
            msg_Err( p_input, "invalid level = %d", i_level );
            return VLC_EGENERIC;
        }
    }
}

static pes_packet_t *MemToPES( input_thread_t *p_input, uint8_t *p_mem, int i_mem )
{
    pes_packet_t *p_pes;
    data_packet_t *p_data;

    if( ( p_pes = input_NewPES( p_input->p_method_data ) ) == NULL )
    {
        return NULL;
    }

    p_data = input_NewPacket( p_input->p_method_data, i_mem);

    memcpy( p_data->p_payload_start, p_mem, i_mem );
    p_data->p_payload_end = p_data->p_payload_start + i_mem;

    p_pes->p_first = p_pes->p_last = p_data;
    p_pes->i_nb_data = 1;
    p_pes->i_pes_size = i_mem;

    return p_pes;
}

static void BlockDecode( input_thread_t *p_input, KaxBlock *block, mtime_t i_pts, mtime_t i_duration )
{
    demux_sys_t    *p_sys   = p_input->p_demux_data;

    int             i_track;
    pes_packet_t    *p_pes;
    unsigned int    i;

#define tk  p_sys->track[i_track]
    for( i_track = 0; i_track < p_sys->i_track; i_track++ )
    {
        if( tk.i_number == block->TrackNum() )
        {
            break;
        }
    }

    if( i_track >= p_sys->i_track )
    {
        msg_Err( p_input, "invalid track number=%d", block->TrackNum() );
        return;
    }

    if( tk.p_es->p_decoder_fifo == NULL )
    {
        return;
    }

    if( ( p_pes = input_NewPES( p_input->p_method_data ) ) == NULL )
    {
        msg_Err( p_input, "cannot allocate PES" );
    }


    p_pes->i_pts = i_pts;
    p_pes->i_dts = i_pts;
    if( tk.i_cat == SPU_ES )
    {
        /* special case : dts mean end of display */
        if( i_duration > 0 )
        {
            /* FIXME not sure about that */
            p_pes->i_dts += i_duration * 1000;// * (mtime_t) 1000 / p_sys->i_timescale;
        }
        else
        {
            /* unknown */
            p_pes->i_dts = 0;
        }
    }

    p_pes->p_first = p_pes->p_last = NULL;
    p_pes->i_nb_data = 0;
    p_pes->i_pes_size = 0;

    for( i = 0; i < block->NumberFrames(); i++ )
    {
        data_packet_t *p_data = NULL;
        DataBuffer &data = block->GetBuffer(i);

        if( ( p_data = input_NewPacket( p_input->p_method_data, data.Size() ) ) != NULL )
        {
            memcpy( p_data->p_payload_start, data.Buffer(), data.Size() );

            p_data->p_payload_end = p_data->p_payload_start + data.Size();

            if( p_pes->p_first == NULL )
            {
                p_pes->p_first = p_data;
            }
            else
            {
                p_pes->p_last->p_next  = p_data;
            }
            p_pes->i_nb_data++;
            p_pes->i_pes_size += data.Size();
            p_pes->p_last  = p_data;
        }
    }
    if( tk.i_cat == SPU_ES && p_pes->p_first && p_pes->i_pes_size > 0 )
    {
        p_pes->p_first->p_payload_end[-1] = '\0';
    }

    if( !tk.b_inited && tk.i_data_init > 0 )
    {
        pes_packet_t *p_init;

        msg_Dbg( p_input, "sending header (%d bytes)", tk.i_data_init );

        if( tk.i_codec == VLC_FOURCC( 'v', 'o', 'r', 'b' ) )
        {
            int i;
            int i_offset = 1;
            int i_size[3];

            /* XXX hack split the 3 headers */
            if( tk.p_data_init[0] != 0x02 )
            {
                msg_Err( p_input, "invalid vorbis header" );
            }

            for( i = 0; i < 2; i++ )
            {
                i_size[i] = 0;
                while( i_offset < tk.i_data_init )
                {
                    i_size[i] += tk.p_data_init[i_offset];
                    if( tk.p_data_init[i_offset++] != 0xff )
                    {
                        break;
                    }
                }
            }
            i_size[0] = __MIN( i_size[0], tk.i_data_init - i_offset );
            i_size[1] = __MIN( i_size[1], tk.i_data_init - i_offset - i_size[0] );
            i_size[2] = tk.i_data_init - i_offset - i_size[0] - i_size[1];

            p_init = MemToPES( p_input, &tk.p_data_init[i_offset], i_size[0] );
            if( p_init )
            {
                input_DecodePES( tk.p_es->p_decoder_fifo, p_init );
            }
            p_init = MemToPES( p_input, &tk.p_data_init[i_offset+i_size[0]], i_size[1] );
            if( p_init )
            {
                input_DecodePES( tk.p_es->p_decoder_fifo, p_init );
            }
            p_init = MemToPES( p_input, &tk.p_data_init[i_offset+i_size[0]+i_size[1]], i_size[2] );
            if( p_init )
            {
                input_DecodePES( tk.p_es->p_decoder_fifo, p_init );
            }
        }
        else
        {
            p_init = MemToPES( p_input, tk.p_data_init, tk.i_data_init );
            if( p_init )
            {
                input_DecodePES( tk.p_es->p_decoder_fifo, p_init );
            }
        }
    }
    tk.b_inited = VLC_TRUE;

    input_DecodePES( tk.p_es->p_decoder_fifo, p_pes );

#undef tk
}

static void Seek( input_thread_t *p_input, mtime_t i_date, int i_percent)
{
    demux_sys_t    *p_sys   = p_input->p_demux_data;

    KaxBlock    *block;
    int64_t     i_block_duration;
    int64_t     i_block_ref1;
    int64_t     i_block_ref2;

    int         i_index;
    int         i_track_skipping;
    int         i_track;

    msg_Dbg( p_input, "seek request to %lld (%d%%)", i_date, i_percent );

    for( i_index = 0; i_index < p_sys->i_index; i_index++ )
    {
        if( p_sys->index[i_index].i_time >= i_date )
        {
            break;
        }
    }

    if( i_index > 0 )
    {
        i_index--;
    }

    msg_Dbg( p_input, "seek got %lld (%d%%)",
             p_sys->index[i_index].i_time, (int)(100 * p_sys->index[i_index].i_position /p_input->stream.p_selected_area->i_size ) );

    delete p_sys->ep;

    p_sys->in->setFilePointer( p_sys->index[i_index].i_position, seek_beginning );

    p_sys->ep = new EbmlParser( p_sys->es, p_sys->segment );

    /* now parse until key frame */
#define tk  p_sys->track[i_track]
    i_track_skipping = 0;
    for( i_track = 0; i_track < p_sys->i_track; i_track++ )
    {
        if( tk.i_cat == VIDEO_ES )
        {
            tk.b_search_keyframe = VLC_TRUE;
            i_track_skipping++;
        }
    }


    while( i_track_skipping > 0 )
    {
        if( BlockGet( p_input, &block, &i_block_ref1, &i_block_ref2, &i_block_duration ) )
        {
            msg_Warn( p_input, "cannot get block EOF?" );

            return;
        }

        p_sys->i_pts = block->GlobalTimecode() * (mtime_t) 1000 / p_sys->i_timescale;

        for( i_track = 0; i_track < p_sys->i_track; i_track++ )
        {
            if( tk.i_number == block->TrackNum() )
            {
                break;
            }
        }

        if( i_track < p_sys->i_track )
        {
            if( tk.i_cat == VIDEO_ES && i_block_ref1 == -1 && tk.b_search_keyframe )
            {
                tk.b_search_keyframe = VLC_FALSE;
                i_track_skipping--;
            }
            if( tk.i_cat == VIDEO_ES && !tk.b_search_keyframe )
            {
                BlockDecode( p_input, block, 0, 0 );
            }
        }

        delete block;
    }
#undef tk
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    demux_sys_t    *p_sys   = p_input->p_demux_data;
    mtime_t        i_start_pts;
    KaxBlock *block;
    int64_t i_block_duration;
    int64_t i_block_ref1;
    int64_t i_block_ref2;

    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        mtime_t i_duration = (mtime_t)( p_sys->f_duration / 1000 );
        mtime_t i_date;
        int i_percent;

        i_date = (mtime_t)1000000 *
                 (mtime_t)i_duration*
                 (mtime_t)p_sys->in->getFilePointer() /
                 (mtime_t)p_input->stream.p_selected_area->i_size;

        i_percent = 100 * p_sys->in->getFilePointer() /
                        p_input->stream.p_selected_area->i_size;

        Seek( p_input, i_date, i_percent);
    }

    i_start_pts = p_sys->i_pts;

    for( ;; )
    {
        mtime_t i_pts;

        if( BlockGet( p_input, &block, &i_block_ref1, &i_block_ref2, &i_block_duration ) )
        {
            msg_Warn( p_input, "cannot get block EOF?" );

            return 0;
        }

        p_sys->i_pts = block->GlobalTimecode() * (mtime_t) 1000 / p_sys->i_timescale;

        if( p_sys->i_pts > 0 )
        {
            input_ClockManageRef( p_input,
                                  p_input->stream.p_selected_program,
                                  p_sys->i_pts * 9 / 100 );
        }

        i_pts = input_ClockGetTS( p_input,
                                  p_input->stream.p_selected_program,
                                  p_sys->i_pts * 9 / 100 );

        BlockDecode( p_input, block, i_pts, i_block_duration );

        delete block;

        if( i_start_pts == -1 )
        {
            i_start_pts = p_sys->i_pts;
        }
        else if( p_sys->i_pts > i_start_pts + (mtime_t)100000)
        {
            return 1;
        }
    }
}


