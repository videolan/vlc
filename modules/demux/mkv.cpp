/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#ifdef HAVE_TIME_H
#   include <time.h>                                               /* time() */
#endif

#include <vlc/input.h>

#include <codecs.h>                        /* BITMAPINFOHEADER, WAVEFORMATEX */
#include "iso_lang.h"
#include "vlc_meta.h"

#include <iostream>
#include <cassert>
#include <typeinfo>
#include <string>
#include <vector>
#include <algorithm>

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif

/* libebml and matroska */
#include "ebml/EbmlHead.h"
#include "ebml/EbmlSubHead.h"
#include "ebml/EbmlStream.h"
#include "ebml/EbmlContexts.h"
#include "ebml/EbmlVoid.h"
#include "ebml/EbmlVersion.h"
#include "ebml/StdIOCallback.h"

#include "matroska/KaxAttachments.h"
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
#include "matroska/KaxTags.h"
#include "matroska/KaxTagMulti.h"
#include "matroska/KaxTracks.h"
#include "matroska/KaxTrackAudio.h"
#include "matroska/KaxTrackVideo.h"
#include "matroska/KaxTrackEntryData.h"
#include "matroska/KaxContentEncoding.h"

#include "ebml/StdIOCallback.h"

extern "C" {
   #include "mp4/libmp4.h"
}
#ifdef HAVE_ZLIB_H
#   include <zlib.h>
#endif

#define MATROSKA_COMPRESSION_NONE 0
#define MATROSKA_COMPRESSION_ZLIB 1

#define MKVD_TIMECODESCALE 1000000

/**
 * What's between a directory and a filename?
 */
#if defined( WIN32 )
    #define DIRECTORY_SEPARATOR '\\'
#else
    #define DIRECTORY_SEPARATOR '/'
#endif

using namespace LIBMATROSKA_NAMESPACE;
using namespace std;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( _("Matroska") );
    set_description( _("Matroska stream demuxer" ) );
    set_capability( "demux2", 50 );
    set_callbacks( Open, Close );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );

    add_bool( "mkv-use-ordered-chapters", 1, NULL,
            N_("Ordered chapters"),
            N_("Play chapters in the specified order as specified in the file"), VLC_TRUE );

    add_bool( "mkv-use-chapter-codec", 1, NULL,
            N_("Chapter codecs"),
            N_("Use chapter codecs found in the file"), VLC_TRUE );

    add_bool( "mkv-seek-percent", 0, NULL,
            N_("Seek based on percent not time"),
            N_("Seek based on percent not time"), VLC_TRUE );

    add_shortcut( "mka" );
    add_shortcut( "mkv" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_ZLIB_H
block_t *block_zlib_decompress( vlc_object_t *p_this, block_t *p_in_block ) {
    int result, dstsize, n;
    unsigned char *dst;
    block_t *p_block;
    z_stream d_stream;

    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;
    result = inflateInit(&d_stream);
    if( result != Z_OK )
    {
        msg_Dbg( p_this, "inflateInit() failed. Result: %d", result );
        return NULL;
    }

    d_stream.next_in = (Bytef *)p_in_block->p_buffer;
    d_stream.avail_in = p_in_block->i_buffer;
    n = 0;
    p_block = block_New( p_this, 0 );
    dst = NULL;
    do
    {
        n++;
        p_block = block_Realloc( p_block, 0, n * 1000 );
        dst = (unsigned char *)p_block->p_buffer;
        d_stream.next_out = (Bytef *)&dst[(n - 1) * 1000];
        d_stream.avail_out = 1000;
        result = inflate(&d_stream, Z_NO_FLUSH);
        if( ( result != Z_OK ) && ( result != Z_STREAM_END ) )
        {
            msg_Dbg( p_this, "Zlib decompression failed. Result: %d", result );
            return NULL;
        }
    }
    while( ( d_stream.avail_out == 0 ) && ( d_stream.avail_in != 0 ) &&
           ( result != Z_STREAM_END ) );

    dstsize = d_stream.total_out;
    inflateEnd( &d_stream );

    p_block = block_Realloc( p_block, 0, dstsize );
    p_block->i_buffer = dstsize;
    block_Release( p_in_block );

    return p_block;
}
#endif

/**
 * Helper function to print the mkv parse tree
 */
static void MkvTree( demux_t & demuxer, int i_level, char *psz_format, ... )
{
    va_list args;
    if( i_level > 9 )
    {
        msg_Err( &demuxer, "too deep tree" );
        return;
    }
    va_start( args, psz_format );
    static char *psz_foo = "|   |   |   |   |   |   |   |   |   |";
    char *psz_foo2 = (char*)malloc( ( i_level * 4 + 3 + strlen( psz_format ) ) * sizeof(char) );
    strncpy( psz_foo2, psz_foo, 4 * i_level );
    psz_foo2[ 4 * i_level ] = '+';
    psz_foo2[ 4 * i_level + 1 ] = ' ';
    strcpy( &psz_foo2[ 4 * i_level + 2 ], psz_format );
    __msg_GenericVa( VLC_OBJECT(&demuxer), VLC_MSG_DBG, "mkv", psz_foo2, args );
    free( psz_foo2 );
    va_end( args );
}
    
/*****************************************************************************
 * Stream managment
 *****************************************************************************/
class vlc_stream_io_callback: public IOCallback
{
  private:
    stream_t       *s;
    vlc_bool_t     mb_eof;

  public:
    vlc_stream_io_callback( stream_t * );

    virtual uint32   read            ( void *p_buffer, size_t i_size);
    virtual void     setFilePointer  ( int64_t i_offset, seek_mode mode = seek_beginning );
    virtual size_t   write           ( const void *p_buffer, size_t i_size);
    virtual uint64   getFilePointer  ( void );
    virtual void     close           ( void );
};

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
    void Reset( void );
    EbmlElement *Get( void );
    void        Keep( void );

    int GetLevel( void );

  private:
    EbmlStream  *m_es;
    int         mi_level;
    EbmlElement *m_el[10];

    EbmlElement *m_got;

    int         mi_user_level;
    vlc_bool_t  mb_keep;
};


/*****************************************************************************
 * Some functions to manipulate memory
 *****************************************************************************/
#define GetFOURCC( p )  __GetFOURCC( (uint8_t*)p )
static vlc_fourcc_t __GetFOURCC( uint8_t *p )
{
    return VLC_FOURCC( p[0], p[1], p[2], p[3] );
}

/*****************************************************************************
 * definitions of structures and functions used by this plugins
 *****************************************************************************/
typedef struct
{
    vlc_bool_t   b_default;
    vlc_bool_t   b_enabled;
    unsigned int i_number;

    int          i_extra_data;
    uint8_t      *p_extra_data;

    char         *psz_codec;

    uint64_t     i_default_duration;
    float        f_timecodescale;

    /* video */
    es_format_t fmt;
    float       f_fps;
    es_out_id_t *p_es;

    vlc_bool_t      b_inited;
    /* data to be send first */
    int             i_data_init;
    uint8_t         *p_data_init;

    /* hack : it's for seek */
    vlc_bool_t      b_search_keyframe;
    vlc_bool_t      b_silent;

    /* informative */
    char         *psz_codec_name;
    char         *psz_codec_settings;
    char         *psz_codec_info_url;
    char         *psz_codec_download_url;
    
    /* encryption/compression */
    int           i_compression_type;

} mkv_track_t;

typedef struct
{
    int     i_track;
    int     i_block_number;

    int64_t i_position;
    int64_t i_time;

    vlc_bool_t b_key;
} mkv_index_t;

class chapter_item_t
{
public:
    chapter_item_t()
    :i_start_time(0)
    ,i_end_time(-1)
    ,i_user_start_time(-1)
    ,i_user_end_time(-1)
    ,i_seekpoint_num(-1)
    ,b_display_seekpoint(true)
    ,psz_parent(NULL)
    {}
    
    int64_t RefreshChapters( bool b_ordered, int64_t i_prev_user_time, input_title_t & title );
    const chapter_item_t * FindTimecode( mtime_t i_timecode ) const;
    
    int64_t                     i_start_time, i_end_time;
    int64_t                     i_user_start_time, i_user_end_time; /* the time in the stream when an edition is ordered */
    std::vector<chapter_item_t> sub_chapters;
    int                         i_seekpoint_num;
    int64_t                     i_uid;
    bool                        b_display_seekpoint;
    std::string                 psz_name;
    chapter_item_t              *psz_parent;
    
    bool operator<( const chapter_item_t & item ) const
    {
        return ( i_user_start_time < item.i_user_start_time || (i_user_start_time == item.i_user_start_time && i_user_end_time < item.i_user_end_time) );
    }

protected:
    bool Enter();
    bool Leave();
};

class chapter_edition_t 
{
public:
    chapter_edition_t()
    :i_uid(-1)
    ,b_ordered(false)
    {}
    
    void RefreshChapters( input_title_t & title );
    double Duration() const;
    const chapter_item_t * FindTimecode( mtime_t i_timecode ) const;
    
    std::vector<chapter_item_t> chapters;
    int64_t                     i_uid;
    bool                        b_ordered;
};

class demux_sys_t;

class matroska_segment_t
{
public:
    matroska_segment_t( demux_sys_t & demuxer, EbmlStream & estream )
        :segment(NULL)
        ,es(estream)
        ,i_timescale(MKVD_TIMECODESCALE)
        ,f_duration(-1.0)
        ,f_start_time(0.0)
        ,i_cues_position(-1)
        ,i_chapters_position(-1)
        ,i_tags_position(-1)
        ,cluster(NULL)
        ,i_start_pos(0)
        ,b_cues(VLC_FALSE)
        ,i_index(0)
        ,i_index_max(1024)
        ,psz_muxing_application(NULL)
        ,psz_writing_application(NULL)
        ,psz_segment_filename(NULL)
        ,psz_title(NULL)
        ,psz_date_utc(NULL)
        ,i_default_edition(0)
        ,sys(demuxer)
        ,ep(NULL)
        ,b_preloaded(false)
    {
        index = (mkv_index_t*)malloc( sizeof( mkv_index_t ) * i_index_max );
    }

    ~matroska_segment_t()
    {
        for( size_t i_track = 0; i_track < tracks.size(); i_track++ )
        {
#define tk  tracks[i_track]
            if( tk->fmt.psz_description )
            {
                free( tk->fmt.psz_description );
            }
            if( tk->psz_codec )
            {
                free( tk->psz_codec );
            }
            if( tk->fmt.psz_language )
            {
                free( tk->fmt.psz_language );
            }
            delete tk;
#undef tk
        }
        
        if( psz_writing_application )
        {
            free( psz_writing_application );
        }
        if( psz_muxing_application )
        {
            free( psz_muxing_application );
        }
        if( psz_segment_filename )
        {
            free( psz_segment_filename );
        }
        if( psz_title )
        {
            free( psz_title );
        }
        if( psz_date_utc )
        {
            free( psz_date_utc );
        }
        if ( index )
            free( index );

        delete ep;
    }

    KaxSegment              *segment;
    EbmlStream              & es;

    /* time scale */
    uint64_t                i_timescale;

    /* duration of the segment */
    float                   f_duration;
    float                   f_start_time;

    /* all tracks */
    std::vector<mkv_track_t*> tracks;

    /* from seekhead */
    int64_t                 i_cues_position;
    int64_t                 i_chapters_position;
    int64_t                 i_tags_position;

    KaxCluster              *cluster;
    int64_t                 i_start_pos;
    KaxSegmentUID           segment_uid;
    KaxPrevUID              prev_segment_uid;
    KaxNextUID              next_segment_uid;

    vlc_bool_t              b_cues;
    int                     i_index;
    int                     i_index_max;
    mkv_index_t             *index;

    /* info */
    char                    *psz_muxing_application;
    char                    *psz_writing_application;
    char                    *psz_segment_filename;
    char                    *psz_title;
    char                    *psz_date_utc;

    std::vector<chapter_edition_t> stored_editions;
    int                            i_default_edition;

    std::vector<KaxSegmentFamily>  families;
    
    demux_sys_t                    & sys;
    EbmlParser                     *ep;
    bool                           b_preloaded;

    bool Preload( );
    bool PreloadFamily( const matroska_segment_t & segment );
    void ParseInfo( EbmlElement *info );
    void ParseChapters( EbmlElement *chapters );
    void ParseSeekHead( EbmlElement *seekhead );
    void ParseTracks( EbmlElement *tracks );
    void ParseChapterAtom( int i_level, EbmlMaster *ca, chapter_item_t & chapters );
    void ParseTrackEntry( EbmlMaster *m );
    void ParseCluster( );
    void IndexAppendCluster( KaxCluster *cluster );
    void LoadCues( );
    void LoadTags( );
    void InformationCreate( );
    void Seek( mtime_t i_date, mtime_t i_time_offset );
    int BlockGet( KaxBlock **pp_block, int64_t *pi_ref1, int64_t *pi_ref2, int64_t *pi_duration );
    bool Select( mtime_t i_start_time );
    void UnSelect( );
    static bool CompareSegmentUIDs( const matroska_segment_t * item_a, const matroska_segment_t * item_b );
};

// class holding hard-linked segment together in the playback order
class virtual_segment_t
{
public:
    virtual_segment_t( matroska_segment_t *p_segment )
        :i_current_segment(0)
        ,i_current_edition(-1)
        ,psz_current_chapter(NULL)
    {
        linked_segments.push_back( p_segment );

        AppendUID( p_segment->segment_uid );
        AppendUID( p_segment->prev_segment_uid );
        AppendUID( p_segment->next_segment_uid );
    }

    void Sort();
    size_t AddSegment( matroska_segment_t *p_segment );
    void PreloadLinked( );
    float Duration( ) const;
    void LoadCues( );
    void Seek( demux_t & demuxer, mtime_t i_date, mtime_t i_time_offset, const chapter_item_t *psz_chapter );

    inline chapter_edition_t *Edition()
    {
        if ( i_current_edition >= 0 && size_t(i_current_edition) < editions.size() )
            return &editions[i_current_edition];
        return NULL;
    }
    
    inline bool EditionIsOrdered() const
    {
        return (editions.size() != 0 && i_current_edition >= 0 && editions[i_current_edition].b_ordered);
    }

    matroska_segment_t * Segment() const
    {
        if ( linked_segments.size() == 0 || i_current_segment >= linked_segments.size() )
            return NULL;
        return linked_segments[i_current_segment];
    }

    inline const chapter_item_t *CurrentChapter() const {
        return psz_current_chapter;
    }

    bool SelectNext()
    {
        if ( i_current_segment < linked_segments.size()-1 )
        {
            i_current_segment++;
            return true;
        }
        return false;
    }

/* TODO handle/merge chapters here */
    void UpdateCurrentToChapter( demux_t & demux );

protected:
    std::vector<matroska_segment_t*> linked_segments;
    std::vector<KaxSegmentUID>       linked_uids;
    size_t                           i_current_segment;

    std::vector<chapter_edition_t>   editions;
    int                              i_current_edition;
    const chapter_item_t             *psz_current_chapter;

    void                             AppendUID( const EbmlBinary & UID );
};

class matroska_stream_t
{
public:
    matroska_stream_t( demux_sys_t & demuxer )
        :p_in(NULL)
        ,p_es(NULL)
        ,sys(demuxer)
    {}

    ~matroska_stream_t()
    {
        delete p_in;
        delete p_es;
    }

    IOCallback         *p_in;
    EbmlStream         *p_es;

    std::vector<matroska_segment_t*> segments;

    demux_sys_t                      & sys;

    void PreloadFamily( const matroska_segment_t & segment );
};

class demux_sys_t
{
public:
    demux_sys_t( demux_t & demux )
        :demuxer(demux)
        ,i_pts(0)
        ,i_start_pts(0)
        ,i_chapter_time(0)
        ,meta(NULL)
        ,title(NULL)
        ,p_current_segment(NULL)
        ,f_duration(-1.0)
    {}

    ~demux_sys_t()
    {
        for (size_t i=0; i<streams.size(); i++)
            delete streams[i];
        for ( size_t i=0; i<opened_segments.size(); i++ )
            delete opened_segments[i];
    }

    /* current data */
    demux_t                 & demuxer;

    mtime_t                 i_pts;
    mtime_t                 i_start_pts;
    mtime_t                 i_chapter_time;

    vlc_meta_t              *meta;

    input_title_t           *title;

    std::vector<matroska_stream_t*>  streams;
    std::vector<matroska_segment_t*> opened_segments;
    virtual_segment_t                *p_current_segment;

    /* duration of the stream */
    float                   f_duration;

    matroska_segment_t *FindSegment( const EbmlBinary & uid ) const;
    void PreloadFamily( );
    void PreloadLinked( matroska_segment_t *p_segment );
    void PreparePlayback( );
    matroska_stream_t *AnalyseAllSegmentsFound( EbmlStream *p_estream );
};

static int  Demux  ( demux_t * );
static int  Control( demux_t *, int, va_list );
static void Seek   ( demux_t *, mtime_t i_date, double f_percent, const chapter_item_t *psz_chapter );

#define MKV_IS_ID( el, C ) ( EbmlId( (*el) ) == C::ClassInfos.GlobalId )

static char *UTF8ToStr          ( const UTFstring &u );

/*****************************************************************************
 * Open: initializes matroska demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t            *p_demux = (demux_t*)p_this;
    demux_sys_t        *p_sys;
    matroska_stream_t  *p_stream;
    matroska_segment_t *p_segment;
    uint8_t            *p_peek;
    std::string        s_path, s_filename;
    vlc_stream_io_callback *p_io_callback;
    EbmlStream         *p_io_stream;

    /* peek the begining */
    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 ) return VLC_EGENERIC;

    /* is a valid file */
    if( p_peek[0] != 0x1a || p_peek[1] != 0x45 ||
        p_peek[2] != 0xdf || p_peek[3] != 0xa3 ) return VLC_EGENERIC;

    /* Set the demux function */
    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys = new demux_sys_t( *p_demux );

    p_io_callback = new vlc_stream_io_callback( p_demux->s );
    p_io_stream = new EbmlStream( *p_io_callback );

    if( p_io_stream == NULL )
    {
        msg_Err( p_demux, "failed to create EbmlStream" );
        delete p_io_callback;
        delete p_sys;
        return VLC_EGENERIC;
    }

    p_stream = p_sys->AnalyseAllSegmentsFound( p_io_stream );
    if( p_stream == NULL )
    {
        msg_Err( p_demux, "cannot find KaxSegment" );
        goto error;
    }
    p_sys->streams.push_back( p_stream );

    p_stream->p_in = p_io_callback;
    p_stream->p_es = p_io_stream;

    for (size_t i=0; i<p_stream->segments.size(); i++)
    {
        p_stream->segments[i]->Preload();
    }

    p_segment = p_stream->segments[0];
    if( p_segment->cluster == NULL )
    {
        msg_Err( p_demux, "cannot find any cluster, damaged file ?" );
        goto error;
    }
    // reset the stream reading to the first cluster of the segment used
    p_stream->p_in->setFilePointer( p_segment->cluster->GetElementPosition() );

    /* get the files from the same dir from the same family (based on p_demux->psz_path) */
    if (p_demux->psz_path[0] != '\0' && !strcmp(p_demux->psz_access, ""))
    {
        // assume it's a regular file
        // get the directory path
        s_path = p_demux->psz_path;
        if (s_path.at(s_path.length() - 1) == DIRECTORY_SEPARATOR)
        {
            s_path = s_path.substr(0,s_path.length()-1);
        }
        else
        {
            if (s_path.find_last_of(DIRECTORY_SEPARATOR) > 0) 
            {
                s_path = s_path.substr(0,s_path.find_last_of(DIRECTORY_SEPARATOR));
            }
        }

        struct dirent *p_file_item;
        DIR *p_src_dir = opendir(s_path.c_str());

        if (p_src_dir != NULL)
        {
            while ((p_file_item = (dirent *) readdir(p_src_dir)))
            {
                if (strlen(p_file_item->d_name) > 4)
                {
                    s_filename = s_path + DIRECTORY_SEPARATOR + p_file_item->d_name;

                    if (!s_filename.compare(p_demux->psz_path))
                        continue; // don't reuse the original opened file

#if defined(__GNUC__) && (__GNUC__ < 3)
                    if (!s_filename.compare("mkv", s_filename.length() - 3, 3) || 
                        !s_filename.compare("mka", s_filename.length() - 3, 3))
#else
                    if (!s_filename.compare(s_filename.length() - 3, 3, "mkv") || 
                        !s_filename.compare(s_filename.length() - 3, 3, "mka"))
#endif
                    {
                        // test wether this file belongs to the our family
                        StdIOCallback *p_file_io = new StdIOCallback(s_filename.c_str(), MODE_READ);
                        EbmlStream *p_estream = new EbmlStream(*p_file_io);

                        p_stream = p_sys->AnalyseAllSegmentsFound( p_estream );
                        if ( p_stream == NULL )
                        {
                            msg_Dbg( p_demux, "the file '%s' will not be used", s_filename.c_str() );
                            delete p_estream;
                            delete p_file_io;
                        }
                        else
                        {
                            p_stream->p_in = p_file_io;
                            p_stream->p_es = p_estream;
                            p_sys->streams.push_back( p_stream );
                        }
                    }
                }
            }
            closedir( p_src_dir );
        }
    }

    p_sys->PreloadFamily( );
    p_sys->PreloadLinked( p_segment );
    p_sys->PreparePlayback( );

    /* add information */
    p_segment->InformationCreate( );

    if ( !p_segment->Select( 0 ) )
    {
        msg_Err( p_demux, "cannot use the segment" );
        goto error;
    }
    
    return VLC_SUCCESS;

error:
    delete p_sys;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    delete p_sys;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    int64_t     *pi64;
    double      *pf, f;
    int         i_skp;

    vlc_meta_t **pp_meta;

    switch( i_query )
    {
        case DEMUX_GET_META:
            pp_meta = (vlc_meta_t**)va_arg( args, vlc_meta_t** );
            *pp_meta = vlc_meta_Duplicate( p_sys->meta );
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->f_duration > 0.0 )
            {
                *pi64 = (int64_t)(p_sys->f_duration * 1000);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if ( p_sys->f_duration > 0.0 )
                *pf = (double)(p_sys->i_pts >= p_sys->i_start_pts ? p_sys->i_pts : p_sys->i_start_pts ) / (1000.0 * p_sys->f_duration);
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            Seek( p_demux, -1, f, NULL );
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_pts;
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
            if( p_sys->title && p_sys->title->i_seekpoint > 0 )
            {
                input_title_t ***ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
                int *pi_int    = (int*)va_arg( args, int* );

                *pi_int = 1;
                *ppp_title = (input_title_t**)malloc( sizeof( input_title_t**) );

                (*ppp_title)[0] = vlc_input_title_Duplicate( p_sys->title );

                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TITLE:
            /* TODO handle editions as titles & DVD titles as well */
            if( p_sys->title && p_sys->title->i_seekpoint > 0 )
            {
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_SEEKPOINT:
            /* FIXME do a better implementation */
            i_skp = (int)va_arg( args, int );

            if( p_sys->title && i_skp < p_sys->title->i_seekpoint)
            {
                Seek( p_demux, (int64_t)p_sys->title->seekpoint[i_skp]->i_time_offset, -1, NULL);
                p_demux->info.i_seekpoint |= INPUT_UPDATE_SEEKPOINT;
                p_demux->info.i_seekpoint = i_skp;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
        case DEMUX_GET_FPS:
        default:
            return VLC_EGENERIC;
    }
}

int matroska_segment_t::BlockGet( KaxBlock **pp_block, int64_t *pi_ref1, int64_t *pi_ref2, int64_t *pi_duration )
{
    *pp_block = NULL;
    *pi_ref1  = -1;
    *pi_ref2  = -1;

    for( ;; )
    {
        EbmlElement *el;
        int         i_level;

        if( sys.demuxer.b_die )
        {
            return VLC_EGENERIC;
        }

        el = ep->Get();
        i_level = ep->GetLevel();

        if( el == NULL && *pp_block != NULL )
        {
            /* update the index */
#define idx index[i_index - 1]
            if( i_index > 0 && idx.i_time == -1 )
            {
                idx.i_time        = (*pp_block)->GlobalTimecode() / (mtime_t)1000;
                idx.b_key         = *pi_ref1 == -1 ? VLC_TRUE : VLC_FALSE;
            }
#undef idx
            return VLC_SUCCESS;
        }

        if( el == NULL )
        {
            if( ep->GetLevel() > 1 )
            {
                ep->Up();
                continue;
            }
            msg_Warn( &sys.demuxer, "EOF" );
            return VLC_EGENERIC;
        }

        /* do parsing */
        if( i_level == 1 )
        {
            if( MKV_IS_ID( el, KaxCluster ) )
            {
                cluster = (KaxCluster*)el;

                /* add it to the index */
                if( i_index == 0 ||
                    ( i_index > 0 && index[i_index - 1].i_position < (int64_t)cluster->GetElementPosition() ) )
                {
                    IndexAppendCluster( cluster );
                }

                // reset silent tracks
                for (size_t i=0; i<tracks.size(); i++)
                {
                    tracks[i]->b_silent = VLC_FALSE;
                }

                ep->Down();
            }
            else if( MKV_IS_ID( el, KaxCues ) )
            {
                msg_Warn( &sys.demuxer, "find KaxCues FIXME" );
                return VLC_EGENERIC;
            }
            else
            {
                msg_Dbg( &sys.demuxer, "unknown (%s)", typeid( el ).name() );
            }
        }
        else if( i_level == 2 )
        {
            if( MKV_IS_ID( el, KaxClusterTimecode ) )
            {
                KaxClusterTimecode &ctc = *(KaxClusterTimecode*)el;

                ctc.ReadData( es.I_O(), SCOPE_ALL_DATA );
                cluster->InitTimecode( uint64( ctc ), i_timescale );
            }
            else if( MKV_IS_ID( el, KaxClusterSilentTracks ) )
            {
                ep->Down();
            }
            else if( MKV_IS_ID( el, KaxBlockGroup ) )
            {
                ep->Down();
            }
        }
        else if( i_level == 3 )
        {
            if( MKV_IS_ID( el, KaxBlock ) )
            {
                *pp_block = (KaxBlock*)el;

                (*pp_block)->ReadData( es.I_O() );
                (*pp_block)->SetParent( *cluster );

                ep->Keep();
            }
            else if( MKV_IS_ID( el, KaxBlockDuration ) )
            {
                KaxBlockDuration &dur = *(KaxBlockDuration*)el;

                dur.ReadData( es.I_O() );
                *pi_duration = uint64( dur );
            }
            else if( MKV_IS_ID( el, KaxReferenceBlock ) )
            {
                KaxReferenceBlock &ref = *(KaxReferenceBlock*)el;

                ref.ReadData( es.I_O() );
                if( *pi_ref1 == -1 )
                {
                    *pi_ref1 = int64( ref );
                }
                else
                {
                    *pi_ref2 = int64( ref );
                }
            }
            else if( MKV_IS_ID( el, KaxClusterSilentTrackNumber ) )
            {
                KaxClusterSilentTrackNumber &track_num = *(KaxClusterSilentTrackNumber*)el;
                track_num.ReadData( es.I_O() );
                // find the track
                for (size_t i=0; i<tracks.size(); i++)
                {
                    if ( tracks[i]->i_number == uint32(track_num))
                    {
                        tracks[i]->b_silent = VLC_TRUE;
                        break;
                    }
                }
            }
        }
        else
        {
            msg_Err( &sys.demuxer, "invalid level = %d", i_level );
            return VLC_EGENERIC;
        }
    }
}

static block_t *MemToBlock( demux_t *p_demux, uint8_t *p_mem, int i_mem)
{
    block_t *p_block;
    if( !(p_block = block_New( p_demux, i_mem ) ) ) return NULL;
    memcpy( p_block->p_buffer, p_mem, i_mem );
    //p_block->i_rate = p_input->stream.control.i_rate;
    return p_block;
}

static void BlockDecode( demux_t *p_demux, KaxBlock *block, mtime_t i_pts,
                         mtime_t i_duration )
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    matroska_segment_t *p_segment = p_sys->p_current_segment->Segment();

    size_t          i_track;
    unsigned int    i;
    vlc_bool_t      b;

#define tk  p_segment->tracks[i_track]
    for( i_track = 0; i_track < p_segment->tracks.size(); i_track++ )
    {
        if( tk->i_number == block->TrackNum() )
        {
            break;
        }
    }

    if( i_track >= p_segment->tracks.size() )
    {
        msg_Err( p_demux, "invalid track number=%d", block->TrackNum() );
        return;
    }
    if( tk->p_es == NULL )
    {
        msg_Err( p_demux, "unknown track number=%d", block->TrackNum() );
        return;
    }
    if( i_pts < p_sys->i_start_pts && tk->fmt.i_cat == AUDIO_ES )
    {
        return; /* discard audio packets that shouldn't be rendered */
    }

    es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );
    if( !b )
    {
        tk->b_inited = VLC_FALSE;
        return;
    }

    /* First send init data */
    if( !tk->b_inited && tk->i_data_init > 0 )
    {
        block_t *p_init;

        msg_Dbg( p_demux, "sending header (%d bytes)", tk->i_data_init );
        p_init = MemToBlock( p_demux, tk->p_data_init, tk->i_data_init );
        if( p_init ) es_out_Send( p_demux->out, tk->p_es, p_init );
    }
    tk->b_inited = VLC_TRUE;


    for( i = 0; i < block->NumberFrames(); i++ )
    {
        block_t *p_block;
        DataBuffer &data = block->GetBuffer(i);

        p_block = MemToBlock( p_demux, data.Buffer(), data.Size() );

        if( p_block == NULL )
        {
            break;
        }

#if defined(HAVE_ZLIB_H)
        if( tk->i_compression_type )
        {
            p_block = block_zlib_decompress( VLC_OBJECT(p_demux), p_block );
        }
#endif

        // TODO implement correct timestamping when B frames are used
        if( tk->fmt.i_cat != VIDEO_ES )
        {
            p_block->i_dts = p_block->i_pts = i_pts;
        }
        else
        {
            p_block->i_dts = i_pts;
            p_block->i_pts = 0;
        }

        if( tk->fmt.i_cat == SPU_ES && strcmp( tk->psz_codec, "S_VOBSUB" ) )
        {
            p_block->i_length = i_duration * 1000;
        }

        es_out_Send( p_demux->out, tk->p_es, p_block );

        /* use time stamp only for first block */
        i_pts = 0;
    }

#undef tk
}

matroska_stream_t *demux_sys_t::AnalyseAllSegmentsFound( EbmlStream *p_estream )
{
    int i_upper_lvl = 0;
    size_t i;
    EbmlElement *p_l0, *p_l1, *p_l2;
    bool b_keep_stream = false, b_keep_segment;

    // verify the EBML Header
    p_l0 = p_estream->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (p_l0 == NULL)
    {
        return NULL;
    }
    p_l0->SkipData(*p_estream, EbmlHead_Context);
    delete p_l0;

    // find all segments in this file
    p_l0 = p_estream->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFL);
    if (p_l0 == NULL)
    {
        return NULL;
    }

    matroska_stream_t *p_stream1 = new matroska_stream_t( *this );

    while (p_l0 != 0)
    {
        if (EbmlId(*p_l0) == KaxSegment::ClassInfos.GlobalId)
        {
            EbmlParser  *ep;
            matroska_segment_t *p_segment1 = new matroska_segment_t( *this, *p_estream );
            b_keep_segment = false;

            ep = new EbmlParser(p_estream, p_l0);
            p_segment1->ep = ep;
            p_segment1->segment = (KaxSegment*)p_l0;

            while ((p_l1 = ep->Get()))
            {
                if (MKV_IS_ID(p_l1, KaxInfo))
                {
                    // find the families of this segment
                    KaxInfo *p_info = static_cast<KaxInfo*>(p_l1);

                    p_info->Read(*p_estream, KaxInfo::ClassInfos.Context, i_upper_lvl, p_l2, true);
                    for( i = 0; i < p_info->ListSize(); i++ )
                    {
                        EbmlElement *l = (*p_info)[i];

                        if( MKV_IS_ID( l, KaxSegmentUID ) )
                        {
                            KaxSegmentUID *p_uid = static_cast<KaxSegmentUID*>(l);
                            b_keep_segment = (FindSegment( *p_uid ) == NULL);
                            if ( !b_keep_segment )
                                break; // this segment is already known
                            opened_segments.push_back( p_segment1 );
                            p_segment1->segment_uid = *( new KaxSegmentUID(*p_uid) );
                        }
                        else if( MKV_IS_ID( l, KaxPrevUID ) )
                        {
                            p_segment1->prev_segment_uid = *( new KaxPrevUID( *static_cast<KaxPrevUID*>(l) ) );
                        }
                        else if( MKV_IS_ID( l, KaxNextUID ) )
                        {
                            p_segment1->next_segment_uid = *( new KaxNextUID( *static_cast<KaxNextUID*>(l) ) );
                        }
                        else if( MKV_IS_ID( l, KaxSegmentFamily ) )
                        {
                            KaxSegmentFamily *p_fam = new KaxSegmentFamily( *static_cast<KaxSegmentFamily*>(l) );
                            std::vector<KaxSegmentFamily>::iterator iter;
                            p_segment1->families.push_back( *p_fam );
                        }
                    }
                    break;
                }
            }
            if ( b_keep_segment )
            {
                b_keep_stream = true;
                p_stream1->segments.push_back( p_segment1 );
            }
            else
                delete p_segment1;
        }

        p_l0->SkipData(*p_estream, EbmlHead_Context);
        p_l0 = p_estream->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFL);
    }

    if ( !b_keep_stream )
    {
        delete p_stream1;
        p_stream1 = NULL;
    }

    return p_stream1;
}

bool matroska_segment_t::Select( mtime_t i_start_time )
{
    size_t i_track;

    /* add all es */
    msg_Dbg( &sys.demuxer, "found %d es", tracks.size() );
    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
#define tk  tracks[i_track]
        if( tk->fmt.i_cat == UNKNOWN_ES )
        {
            msg_Warn( &sys.demuxer, "invalid track[%d, n=%d]", i_track, tk->i_number );
            tk->p_es = NULL;
            continue;
        }

        if( !strcmp( tk->psz_codec, "V_MS/VFW/FOURCC" ) )
        {
            if( tk->i_extra_data < (int)sizeof( BITMAPINFOHEADER ) )
            {
                msg_Err( &sys.demuxer, "missing/invalid BITMAPINFOHEADER" );
                tk->fmt.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
            }
            else
            {
                BITMAPINFOHEADER *p_bih = (BITMAPINFOHEADER*)tk->p_extra_data;

                tk->fmt.video.i_width = GetDWLE( &p_bih->biWidth );
                tk->fmt.video.i_height= GetDWLE( &p_bih->biHeight );
                tk->fmt.i_codec       = GetFOURCC( &p_bih->biCompression );

                tk->fmt.i_extra       = GetDWLE( &p_bih->biSize ) - sizeof( BITMAPINFOHEADER );
                if( tk->fmt.i_extra > 0 )
                {
                    tk->fmt.p_extra = malloc( tk->fmt.i_extra );
                    memcpy( tk->fmt.p_extra, &p_bih[1], tk->fmt.i_extra );
                }
            }
        }
        else if( !strcmp( tk->psz_codec, "V_MPEG1" ) ||
                 !strcmp( tk->psz_codec, "V_MPEG2" ) )
        {
            tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'v' );
        }
        else if( !strncmp( tk->psz_codec, "V_MPEG4", 7 ) )
        {
            if( !strcmp( tk->psz_codec, "V_MPEG4/MS/V3" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'D', 'I', 'V', '3' );
            }
            else if( !strcmp( tk->psz_codec, "V_MPEG4/ISO/AVC" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 'a', 'v', 'c', '1' );
                tk->fmt.b_packetized = VLC_FALSE;
                tk->fmt.i_extra = tk->i_extra_data;
                tk->fmt.p_extra = malloc( tk->i_extra_data );
                memcpy( tk->fmt.p_extra,tk->p_extra_data, tk->i_extra_data );
            }
            else
            {
                tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'v' );
            }
        }
        else if( !strcmp( tk->psz_codec, "V_QUICKTIME" ) )
        {
            MP4_Box_t *p_box = (MP4_Box_t*)malloc( sizeof( MP4_Box_t ) );
            stream_t *p_mp4_stream = stream_MemoryNew( VLC_OBJECT(&sys.demuxer),
                                                       tk->p_extra_data,
                                                       tk->i_extra_data );
            MP4_ReadBoxCommon( p_mp4_stream, p_box );
            MP4_ReadBox_sample_vide( p_mp4_stream, p_box );
            tk->fmt.i_codec = p_box->i_type;
            tk->fmt.video.i_width = p_box->data.p_sample_vide->i_width;
            tk->fmt.video.i_height = p_box->data.p_sample_vide->i_height;
            tk->fmt.i_extra = p_box->data.p_sample_vide->i_qt_image_description;
            tk->fmt.p_extra = malloc( tk->fmt.i_extra );
            memcpy( tk->fmt.p_extra, p_box->data.p_sample_vide->p_qt_image_description, tk->fmt.i_extra );
            MP4_FreeBox_sample_vide( p_box );
            stream_MemoryDelete( p_mp4_stream, VLC_TRUE );
        }
        else if( !strcmp( tk->psz_codec, "A_MS/ACM" ) )
        {
            if( tk->i_extra_data < (int)sizeof( WAVEFORMATEX ) )
            {
                msg_Err( &sys.demuxer, "missing/invalid WAVEFORMATEX" );
                tk->fmt.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
            }
            else
            {
                WAVEFORMATEX *p_wf = (WAVEFORMATEX*)tk->p_extra_data;

                wf_tag_to_fourcc( GetWLE( &p_wf->wFormatTag ), &tk->fmt.i_codec, NULL );

                tk->fmt.audio.i_channels   = GetWLE( &p_wf->nChannels );
                tk->fmt.audio.i_rate = GetDWLE( &p_wf->nSamplesPerSec );
                tk->fmt.i_bitrate    = GetDWLE( &p_wf->nAvgBytesPerSec ) * 8;
                tk->fmt.audio.i_blockalign = GetWLE( &p_wf->nBlockAlign );;
                tk->fmt.audio.i_bitspersample = GetWLE( &p_wf->wBitsPerSample );

                tk->fmt.i_extra            = GetWLE( &p_wf->cbSize );
                if( tk->fmt.i_extra > 0 )
                {
                    tk->fmt.p_extra = malloc( tk->fmt.i_extra );
                    memcpy( tk->fmt.p_extra, &p_wf[1], tk->fmt.i_extra );
                }
            }
        }
        else if( !strcmp( tk->psz_codec, "A_MPEG/L3" ) ||
                 !strcmp( tk->psz_codec, "A_MPEG/L2" ) ||
                 !strcmp( tk->psz_codec, "A_MPEG/L1" ) )
        {
            tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'a' );
        }
        else if( !strcmp( tk->psz_codec, "A_AC3" ) )
        {
            tk->fmt.i_codec = VLC_FOURCC( 'a', '5', '2', ' ' );
        }
        else if( !strcmp( tk->psz_codec, "A_DTS" ) )
        {
            tk->fmt.i_codec = VLC_FOURCC( 'd', 't', 's', ' ' );
        }
        else if( !strcmp( tk->psz_codec, "A_FLAC" ) )
        {
            tk->fmt.i_codec = VLC_FOURCC( 'f', 'l', 'a', 'c' );
            tk->fmt.i_extra = tk->i_extra_data;
            tk->fmt.p_extra = malloc( tk->i_extra_data );
            memcpy( tk->fmt.p_extra,tk->p_extra_data, tk->i_extra_data );
        }
        else if( !strcmp( tk->psz_codec, "A_VORBIS" ) )
        {
            int i, i_offset = 1, i_size[3], i_extra;
            uint8_t *p_extra;

            tk->fmt.i_codec = VLC_FOURCC( 'v', 'o', 'r', 'b' );

            /* Split the 3 headers */
            if( tk->p_extra_data[0] != 0x02 )
                msg_Err( &sys.demuxer, "invalid vorbis header" );

            for( i = 0; i < 2; i++ )
            {
                i_size[i] = 0;
                while( i_offset < tk->i_extra_data )
                {
                    i_size[i] += tk->p_extra_data[i_offset];
                    if( tk->p_extra_data[i_offset++] != 0xff ) break;
                }
            }

            i_size[0] = __MIN(i_size[0], tk->i_extra_data - i_offset);
            i_size[1] = __MIN(i_size[1], tk->i_extra_data -i_offset -i_size[0]);
            i_size[2] = tk->i_extra_data - i_offset - i_size[0] - i_size[1];

            tk->fmt.i_extra = 3 * 2 + i_size[0] + i_size[1] + i_size[2];
            tk->fmt.p_extra = malloc( tk->fmt.i_extra );
            p_extra = (uint8_t *)tk->fmt.p_extra; i_extra = 0;
            for( i = 0; i < 3; i++ )
            {
                *(p_extra++) = i_size[i] >> 8;
                *(p_extra++) = i_size[i] & 0xFF;
                memcpy( p_extra, tk->p_extra_data + i_offset + i_extra,
                        i_size[i] );
                p_extra += i_size[i];
                i_extra += i_size[i];
            }
        }
        else if( !strncmp( tk->psz_codec, "A_AAC/MPEG2/", strlen( "A_AAC/MPEG2/" ) ) ||
                 !strncmp( tk->psz_codec, "A_AAC/MPEG4/", strlen( "A_AAC/MPEG4/" ) ) )
        {
            int i_profile, i_srate;
            static unsigned int i_sample_rates[] =
            {
                    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                        16000, 12000, 11025, 8000,  7350,  0,     0,     0
            };

            tk->fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'a' );
            /* create data for faad (MP4DecSpecificDescrTag)*/

            if( !strcmp( &tk->psz_codec[12], "MAIN" ) )
            {
                i_profile = 0;
            }
            else if( !strcmp( &tk->psz_codec[12], "LC" ) )
            {
                i_profile = 1;
            }
            else if( !strcmp( &tk->psz_codec[12], "SSR" ) )
            {
                i_profile = 2;
            }
            else
            {
                i_profile = 3;
            }

            for( i_srate = 0; i_srate < 13; i_srate++ )
            {
                if( i_sample_rates[i_srate] == tk->fmt.audio.i_rate )
                {
                    break;
                }
            }
            msg_Dbg( &sys.demuxer, "profile=%d srate=%d", i_profile, i_srate );

            tk->fmt.i_extra = 2;
            tk->fmt.p_extra = malloc( tk->fmt.i_extra );
            ((uint8_t*)tk->fmt.p_extra)[0] = ((i_profile + 1) << 3) | ((i_srate&0xe) >> 1);
            ((uint8_t*)tk->fmt.p_extra)[1] = ((i_srate & 0x1) << 7) | (tk->fmt.audio.i_channels << 3);
        }
        else if( !strcmp( tk->psz_codec, "A_PCM/INT/BIG" ) ||
                 !strcmp( tk->psz_codec, "A_PCM/INT/LIT" ) ||
                 !strcmp( tk->psz_codec, "A_PCM/FLOAT/IEEE" ) )
        {
            if( !strcmp( tk->psz_codec, "A_PCM/INT/BIG" ) )
            {
                tk->fmt.i_codec = VLC_FOURCC( 't', 'w', 'o', 's' );
            }
            else
            {
                tk->fmt.i_codec = VLC_FOURCC( 'a', 'r', 'a', 'w' );
            }
            tk->fmt.audio.i_blockalign = ( tk->fmt.audio.i_bitspersample + 7 ) / 8 * tk->fmt.audio.i_channels;
        }
        else if( !strcmp( tk->psz_codec, "A_TTA1" ) )
        {
            /* FIXME: support this codec */
            msg_Err( &sys.demuxer, "TTA not supported yet[%d, n=%d]", i_track, tk->i_number );
            tk->fmt.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
        }
        else if( !strcmp( tk->psz_codec, "A_WAVPACK4" ) )
        {
            /* FIXME: support this codec */
            msg_Err( &sys.demuxer, "Wavpack not supported yet[%d, n=%d]", i_track, tk->i_number );
            tk->fmt.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
        }
        else if( !strcmp( tk->psz_codec, "S_TEXT/UTF8" ) )
        {
            tk->fmt.i_codec = VLC_FOURCC( 's', 'u', 'b', 't' );
            tk->fmt.subs.psz_encoding = strdup( "UTF-8" );
        }
        else if( !strcmp( tk->psz_codec, "S_TEXT/SSA" ) ||
                 !strcmp( tk->psz_codec, "S_TEXT/ASS" ) ||
                 !strcmp( tk->psz_codec, "S_SSA" ) ||
                 !strcmp( tk->psz_codec, "S_ASS" ))
        {
            tk->fmt.i_codec = VLC_FOURCC( 's', 's', 'a', ' ' );
            tk->fmt.subs.psz_encoding = strdup( "UTF-8" );
        }
        else if( !strcmp( tk->psz_codec, "S_VOBSUB" ) )
        {
            tk->fmt.i_codec = VLC_FOURCC( 's','p','u',' ' );
            if( tk->i_extra_data )
            {
                char *p_start;
                char *p_buf = (char *)malloc( tk->i_extra_data + 1);
                memcpy( p_buf, tk->p_extra_data , tk->i_extra_data );
                p_buf[tk->i_extra_data] = '\0';
                
                p_start = strstr( p_buf, "size:" );
                if( sscanf( p_start, "size: %dx%d",
                        &tk->fmt.subs.spu.i_original_frame_width, &tk->fmt.subs.spu.i_original_frame_height ) == 2 )
                {
                    msg_Dbg( &sys.demuxer, "original frame size vobsubs: %dx%d", tk->fmt.subs.spu.i_original_frame_width, tk->fmt.subs.spu.i_original_frame_height );
                }
                else
                {
                    msg_Warn( &sys.demuxer, "reading original frame size for vobsub failed" );
                }
                free( p_buf );
            }
        }
        else if( !strcmp( tk->psz_codec, "B_VOBBTN" ) )
        {
            /* FIXME: support this codec */
            msg_Err( &sys.demuxer, "Vob Buttons not supported yet[%d, n=%d]", i_track, tk->i_number );
            tk->fmt.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
        }
        else
        {
            msg_Err( &sys.demuxer, "unknow codec id=`%s'", tk->psz_codec );
            tk->fmt.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
        }
        if( tk->b_default )
        {
            tk->fmt.i_priority = 1000;
        }

        tk->p_es = es_out_Add( sys.demuxer.out, &tk->fmt );

        es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, tk->p_es, i_start_time );
#undef tk
    }
    
    sys.i_start_pts = i_start_time;
    ep->Reset();

    // reset the stream reading to the first cluster of the segment used
    es.I_O().setFilePointer( i_start_pos );

    return true;
}

void matroska_segment_t::UnSelect( )
{
    size_t i_track;

    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
#define tk  tracks[i_track]
        if ( tk->p_es != NULL )
        {
            es_out_Del( sys.demuxer.out, tk->p_es );
            tk->p_es = NULL;
        }
#undef tk
    }
}

void virtual_segment_t::UpdateCurrentToChapter( demux_t & demux )
{
    demux_sys_t & sys = *demux.p_sys;
    const chapter_item_t *psz_curr_chapter;

    /* update current chapter/seekpoint */
    if ( editions.size() )
    {
        /* 1st, we need to know in which chapter we are */
        psz_curr_chapter = editions[i_current_edition].FindTimecode( sys.i_pts );

        /* we have moved to a new chapter */
        if (psz_curr_chapter != NULL && psz_current_chapter != psz_curr_chapter)
        {
            if (psz_current_chapter->i_seekpoint_num != psz_curr_chapter->i_seekpoint_num && psz_curr_chapter->i_seekpoint_num > 0)
            {
                demux.info.i_update |= INPUT_UPDATE_SEEKPOINT;
                demux.info.i_seekpoint = psz_curr_chapter->i_seekpoint_num - 1;
            }

            if ( editions[i_current_edition].b_ordered )
            {
                /* TODO check if we need to silently seek to a new location in the stream (switch to another chapter) */
                if (psz_current_chapter->i_end_time != psz_curr_chapter->i_start_time)
                    Seek( demux, sys.i_pts, 0, psz_curr_chapter );
                /* count the last duration time found for each track in a table (-1 not found, -2 silent) */
                /* only seek after each duration >= end timecode of the current chapter */
            }

//            i_user_time = psz_curr_chapter->i_user_start_time - psz_curr_chapter->i_start_time;
//            i_start_pts = psz_curr_chapter->i_user_start_time;
        }
        psz_current_chapter = psz_curr_chapter;
    }
}

static void Seek( demux_t *p_demux, mtime_t i_date, double f_percent, const chapter_item_t *psz_chapter )
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    virtual_segment_t  *p_vsegment = p_sys->p_current_segment;
    matroska_segment_t *p_segment = p_vsegment->Segment();
    mtime_t            i_time_offset = 0;

    int         i_index;

    msg_Dbg( p_demux, "seek request to "I64Fd" (%f%%)", i_date, f_percent );
    if( i_date < 0 && f_percent < 0 )
    {
        msg_Warn( p_demux, "cannot seek nowhere !" );
        return;
    }
    if( f_percent > 1.0 )
    {
        msg_Warn( p_demux, "cannot seek so far !" );
        return;
    }

    /* seek without index or without date */
    if( f_percent >= 0 && (config_GetInt( p_demux, "mkv-seek-percent" ) || !p_segment->b_cues || i_date < 0 ))
    {
        if (p_sys->f_duration >= 0)
        {
            i_date = int64_t( f_percent * p_sys->f_duration * 1000.0 );
        }
        else
        {
            int64_t i_pos = int64_t( f_percent * stream_Size( p_demux->s ) );

            msg_Dbg( p_demux, "inacurate way of seeking" );
            for( i_index = 0; i_index < p_segment->i_index; i_index++ )
            {
                if( p_segment->index[i_index].i_position >= i_pos)
                {
                    break;
                }
            }
            if( i_index == p_segment->i_index )
            {
                i_index--;
            }

            i_date = p_segment->index[i_index].i_time;

#if 0
            if( p_segment->index[i_index].i_position < i_pos )
            {
                EbmlElement *el;

                msg_Warn( p_demux, "searching for cluster, could take some time" );

                /* search a cluster */
                while( ( el = p_sys->ep->Get() ) != NULL )
                {
                    if( MKV_IS_ID( el, KaxCluster ) )
                    {
                        KaxCluster *cluster = (KaxCluster*)el;

                        /* add it to the index */
                        p_segment->IndexAppendCluster( cluster );

                        if( (int64_t)cluster->GetElementPosition() >= i_pos )
                        {
                            p_sys->cluster = cluster;
                            p_sys->ep->Down();
                            break;
                        }
                    }
                }
            }
#endif
        }
    }

    p_vsegment->Seek( *p_demux, i_date, i_time_offset, psz_chapter );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux)
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    virtual_segment_t  *p_vsegment = p_sys->p_current_segment;
    matroska_segment_t *p_segmet = p_vsegment->Segment();
    if ( p_segmet == NULL ) return 0;
    int                i_block_count = 0;

    KaxBlock *block;
    int64_t i_block_duration;
    int64_t i_block_ref1;
    int64_t i_block_ref2;

    for( ;; )
    {
        if( p_sys->i_pts >= p_sys->i_start_pts  )
            p_vsegment->UpdateCurrentToChapter( *p_demux );
        
        if ( p_vsegment->EditionIsOrdered() && p_vsegment->CurrentChapter() == NULL )
        {
            /* nothing left to read in this ordered edition */
            if ( !p_vsegment->SelectNext() )
                return 0;
            p_segmet->UnSelect( );
            
            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

            /* switch to the next segment */
            p_segmet = p_vsegment->Segment();
            if ( !p_segmet->Select( 0 ) )
            {
                msg_Err( p_demux, "Failed to select new segment" );
                return 0;
            }
            continue;
        }

        if( p_segmet->BlockGet( &block, &i_block_ref1, &i_block_ref2, &i_block_duration ) )
        {
            if ( p_vsegment->EditionIsOrdered() )
            {
                // check if there are more chapters to read
                if ( p_vsegment->CurrentChapter() != NULL )
                {
                    p_sys->i_pts = p_vsegment->CurrentChapter()->i_user_end_time;
                    return 1;
                }

                return 0;
            }
            msg_Warn( p_demux, "cannot get block EOF?" );
            p_segmet->UnSelect( );
            
            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

            /* switch to the next segment */
            if ( !p_vsegment->SelectNext() )
                // no more segments in this stream
                return 0;
            p_segmet = p_vsegment->Segment();
            if ( !p_segmet->Select( 0 ) )
            {
                msg_Err( p_demux, "Failed to select new segment" );
                return 0;
            }

            continue;
        }

        p_sys->i_pts = p_sys->i_chapter_time + block->GlobalTimecode() / (mtime_t) 1000;

        if( p_sys->i_pts >= p_sys->i_start_pts  )
        {
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_pts );
        }

        BlockDecode( p_demux, block, p_sys->i_pts, i_block_duration );

        delete block;
        i_block_count++;

        // TODO optimize when there is need to leave or when seeking has been called
        if( i_block_count > 5 )
        {
            return 1;
        }
    }
}



/*****************************************************************************
 * Stream managment
 *****************************************************************************/
vlc_stream_io_callback::vlc_stream_io_callback( stream_t *s_ )
{
    s = s_;
    mb_eof = VLC_FALSE;
}

uint32 vlc_stream_io_callback::read( void *p_buffer, size_t i_size )
{
    if( i_size <= 0 || mb_eof )
    {
        return 0;
    }

    return stream_Read( s, p_buffer, i_size );
}
void vlc_stream_io_callback::setFilePointer(int64_t i_offset, seek_mode mode )
{
    int64_t i_pos;

    switch( mode )
    {
        case seek_beginning:
            i_pos = i_offset;
            break;
        case seek_end:
            i_pos = stream_Size( s ) - i_offset;
            break;
        default:
            i_pos= stream_Tell( s ) + i_offset;
            break;
    }

    if( i_pos < 0 || i_pos >= stream_Size( s ) )
    {
        mb_eof = VLC_TRUE;
        return;
    }

    mb_eof = VLC_FALSE;
    if( stream_Seek( s, i_pos ) )
    {
        mb_eof = VLC_TRUE;
    }
    return;
}
size_t vlc_stream_io_callback::write( const void *p_buffer, size_t i_size )
{
    return 0;
}
uint64 vlc_stream_io_callback::getFilePointer( void )
{
    return stream_Tell( s );
}
void vlc_stream_io_callback::close( void )
{
    return;
}


/*****************************************************************************
 * Ebml Stream parser
 *****************************************************************************/
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

void EbmlParser::Reset( void )
{
    while ( mi_level > 0)
    {
        delete m_el[mi_level];
        m_el[mi_level] = NULL;
        mi_level--;
    }
    mi_user_level = mi_level = 1;
#if LIBEBML_VERSION >= 0x000704
    // a little faster and cleaner
    m_es->I_O().setFilePointer( static_cast<KaxSegment*>(m_el[0])->GetGlobalPosition(0) );
#else
    m_es->I_O().setFilePointer( m_el[0]->GetElementPosition() + m_el[0]->ElementSize(true) - m_el[0]->GetSize() );
#endif
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
 * Tools
 *  * LoadCues : load the cues element and update index
 *
 *  * LoadTags : load ... the tags element
 *
 *  * InformationCreate : create all information, load tags if present
 *
 *****************************************************************************/
void matroska_segment_t::LoadCues( )
{
    int64_t     i_sav_position = es.I_O().getFilePointer();
    EbmlParser  *ep;
    EbmlElement *el, *cues;

    /* *** Load the cue if found *** */
    if( i_cues_position < 0 )
    {
        msg_Warn( &sys.demuxer, "no cues/empty cues found->seek won't be precise" );

//        IndexAppendCluster( cluster );
    }

    vlc_bool_t b_seekable;

    stream_Control( sys.demuxer.s, STREAM_CAN_FASTSEEK, &b_seekable );
    if( !b_seekable )
        return;

    msg_Dbg( &sys.demuxer, "loading cues" );
    es.I_O().setFilePointer( i_cues_position, seek_beginning );
    cues = es.FindNextID( KaxCues::ClassInfos, 0xFFFFFFFFL);

    if( cues == NULL )
    {
        msg_Err( &sys.demuxer, "cannot load cues (broken seekhead or file)" );
        es.I_O().setFilePointer( i_sav_position, seek_beginning );
        return;
    }

    ep = new EbmlParser( &es, cues );
    while( ( el = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( el, KaxCuePoint ) )
        {
#define idx index[i_index]

            idx.i_track       = -1;
            idx.i_block_number= -1;
            idx.i_position    = -1;
            idx.i_time        = 0;
            idx.b_key         = VLC_TRUE;

            ep->Down();
            while( ( el = ep->Get() ) != NULL )
            {
                if( MKV_IS_ID( el, KaxCueTime ) )
                {
                    KaxCueTime &ctime = *(KaxCueTime*)el;

                    ctime.ReadData( es.I_O() );

                    idx.i_time = uint64( ctime ) * i_timescale / (mtime_t)1000;
                }
                else if( MKV_IS_ID( el, KaxCueTrackPositions ) )
                {
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        if( MKV_IS_ID( el, KaxCueTrack ) )
                        {
                            KaxCueTrack &ctrack = *(KaxCueTrack*)el;

                            ctrack.ReadData( es.I_O() );
                            idx.i_track = uint16( ctrack );
                        }
                        else if( MKV_IS_ID( el, KaxCueClusterPosition ) )
                        {
                            KaxCueClusterPosition &ccpos = *(KaxCueClusterPosition*)el;

                            ccpos.ReadData( es.I_O() );
                            idx.i_position = segment->GetGlobalPosition( uint64( ccpos ) );
                        }
                        else if( MKV_IS_ID( el, KaxCueBlockNumber ) )
                        {
                            KaxCueBlockNumber &cbnum = *(KaxCueBlockNumber*)el;

                            cbnum.ReadData( es.I_O() );
                            idx.i_block_number = uint32( cbnum );
                        }
                        else
                        {
                            msg_Dbg( &sys.demuxer, "         * Unknown (%s)", typeid(*el).name() );
                        }
                    }
                    ep->Up();
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "     * Unknown (%s)", typeid(*el).name() );
                }
            }
            ep->Up();

#if 0
            msg_Dbg( &sys.demuxer, " * added time="I64Fd" pos="I64Fd
                     " track=%d bnum=%d", idx.i_time, idx.i_position,
                     idx.i_track, idx.i_block_number );
#endif

            i_index++;
            if( i_index >= i_index_max )
            {
                i_index_max += 1024;
                index = (mkv_index_t*)realloc( index, sizeof( mkv_index_t ) * i_index_max );
            }
#undef idx
        }
        else
        {
            msg_Dbg( &sys.demuxer, " * Unknown (%s)", typeid(*el).name() );
        }
    }
    delete ep;
    delete cues;

    b_cues = VLC_TRUE;

    msg_Dbg( &sys.demuxer, "loading cues done." );
    es.I_O().setFilePointer( i_sav_position, seek_beginning );
}

void matroska_segment_t::LoadTags( )
{
    int64_t     i_sav_position = es.I_O().getFilePointer();
    EbmlParser  *ep;
    EbmlElement *el, *tags;

    msg_Dbg( &sys.demuxer, "loading tags" );
    es.I_O().setFilePointer( i_tags_position, seek_beginning );
    tags = es.FindNextID( KaxTags::ClassInfos, 0xFFFFFFFFL);

    if( tags == NULL )
    {
        msg_Err( &sys.demuxer, "cannot load tags (broken seekhead or file)" );
        es.I_O().setFilePointer( i_sav_position, seek_beginning );
        return;
    }

    msg_Dbg( &sys.demuxer, "Tags" );
    ep = new EbmlParser( &es, tags );
    while( ( el = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( el, KaxTag ) )
        {
            msg_Dbg( &sys.demuxer, "+ Tag" );
            ep->Down();
            while( ( el = ep->Get() ) != NULL )
            {
                if( MKV_IS_ID( el, KaxTagTargets ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Targets" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() );
                    }
                    ep->Up();
                }
                else if( MKV_IS_ID( el, KaxTagGeneral ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + General" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() );
                    }
                    ep->Up();
                }
                else if( MKV_IS_ID( el, KaxTagGenres ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Genres" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() );
                    }
                    ep->Up();
                }
                else if( MKV_IS_ID( el, KaxTagAudioSpecific ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Audio Specific" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() );
                    }
                    ep->Up();
                }
                else if( MKV_IS_ID( el, KaxTagImageSpecific ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Images Specific" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() );
                    }
                    ep->Up();
                }
                else if( MKV_IS_ID( el, KaxTagMultiComment ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Comment" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiCommercial ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Commercial" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiDate ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Date" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiEntity ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Entity" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiIdentifier ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Identifier" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiLegal ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Legal" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiTitle ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Title" );
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   + Unknown (%s)", typeid( *el ).name() );
                }
            }
            ep->Up();
        }
        else
        {
            msg_Dbg( &sys.demuxer, "+ Unknown (%s)", typeid( *el ).name() );
        }
    }
    delete ep;
    delete tags;

    msg_Dbg( &sys.demuxer, "loading tags done." );
    es.I_O().setFilePointer( i_sav_position, seek_beginning );
}

/*****************************************************************************
 * ParseSeekHead:
 *****************************************************************************/
void matroska_segment_t::ParseSeekHead( EbmlElement *seekhead )
{
    EbmlElement *el;
    EbmlMaster  *m;
    unsigned int i;
    int i_upper_level = 0;

    msg_Dbg( &sys.demuxer, "|   + Seek head" );

    /* Master elements */
    m = static_cast<EbmlMaster *>(seekhead);
    m->Read( es, seekhead->Generic().Context, i_upper_level, el, true );

    for( i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxSeek ) )
        {
            EbmlMaster *sk = static_cast<EbmlMaster *>(l);
            EbmlId id = EbmlVoid::ClassInfos.GlobalId;
            int64_t i_pos = -1;

            unsigned int j;

            for( j = 0; j < sk->ListSize(); j++ )
            {
                EbmlElement *l = (*sk)[j];

                if( MKV_IS_ID( l, KaxSeekID ) )
                {
                    KaxSeekID &sid = *(KaxSeekID*)l;
                    id = EbmlId( sid.GetBuffer(), sid.GetSize() );
                }
                else if( MKV_IS_ID( l, KaxSeekPosition ) )
                {
                    KaxSeekPosition &spos = *(KaxSeekPosition*)l;
                    i_pos = uint64( spos );
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   + Unknown (%s)", typeid(*l).name() );
                }
            }

            if( i_pos >= 0 )
            {
                if( id == KaxCues::ClassInfos.GlobalId )
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   = cues at "I64Fd, i_pos );
                    i_cues_position = segment->GetGlobalPosition( i_pos );
                }
                else if( id == KaxChapters::ClassInfos.GlobalId )
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   = chapters at "I64Fd, i_pos );
                    i_chapters_position = segment->GetGlobalPosition( i_pos );
                }
                else if( id == KaxTags::ClassInfos.GlobalId )
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   = tags at "I64Fd, i_pos );
                    i_tags_position = segment->GetGlobalPosition( i_pos );
                }
            }
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
        }
    }
}

/*****************************************************************************
 * ParseTrackEntry:
 *****************************************************************************/
void matroska_segment_t::ParseTrackEntry( EbmlMaster *m )
{
    unsigned int i;

    mkv_track_t *tk;

    msg_Dbg( &sys.demuxer, "|   |   + Track Entry" );

    tk = new mkv_track_t();
    tracks.push_back( tk );

    /* Init the track */
    memset( tk, 0, sizeof( mkv_track_t ) );

    es_format_Init( &tk->fmt, UNKNOWN_ES, 0 );
    tk->fmt.psz_language = strdup("English");
    tk->fmt.psz_description = NULL;

    tk->b_default = VLC_TRUE;
    tk->b_enabled = VLC_TRUE;
    tk->b_silent = VLC_FALSE;
    tk->i_number = tracks.size() - 1;
    tk->i_extra_data = 0;
    tk->p_extra_data = NULL;
    tk->psz_codec = NULL;
    tk->i_default_duration = 0;
    tk->f_timecodescale = 1.0;

    tk->b_inited = VLC_FALSE;
    tk->i_data_init = 0;
    tk->p_data_init = NULL;

    tk->psz_codec_name = NULL;
    tk->psz_codec_settings = NULL;
    tk->psz_codec_info_url = NULL;
    tk->psz_codec_download_url = NULL;
    
    tk->i_compression_type = MATROSKA_COMPRESSION_NONE;

    for( i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxTrackNumber ) )
        {
            KaxTrackNumber &tnum = *(KaxTrackNumber*)l;

            tk->i_number = uint32( tnum );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Number=%u", uint32( tnum ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackUID ) )
        {
            KaxTrackUID &tuid = *(KaxTrackUID*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track UID=%u",  uint32( tuid ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackType ) )
        {
            char *psz_type;
            KaxTrackType &ttype = *(KaxTrackType*)l;

            switch( uint8(ttype) )
            {
                case track_audio:
                    psz_type = "audio";
                    tk->fmt.i_cat = AUDIO_ES;
                    break;
                case track_video:
                    psz_type = "video";
                    tk->fmt.i_cat = VIDEO_ES;
                    break;
                case track_subtitle:
                    psz_type = "subtitle";
                    tk->fmt.i_cat = SPU_ES;
                    break;
                default:
                    psz_type = "unknown";
                    tk->fmt.i_cat = UNKNOWN_ES;
                    break;
            }

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Type=%s", psz_type );
        }
//        else  if( EbmlId( *l ) == KaxTrackFlagEnabled::ClassInfos.GlobalId )
//        {
//            KaxTrackFlagEnabled &fenb = *(KaxTrackFlagEnabled*)l;

//            tk->b_enabled = uint32( fenb );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Enabled=%u",
//                     uint32( fenb )  );
//        }
        else  if( MKV_IS_ID( l, KaxTrackFlagDefault ) )
        {
            KaxTrackFlagDefault &fdef = *(KaxTrackFlagDefault*)l;

            tk->b_default = uint32( fdef );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Default=%u", uint32( fdef )  );
        }
        else  if( MKV_IS_ID( l, KaxTrackFlagLacing ) )
        {
            KaxTrackFlagLacing &lac = *(KaxTrackFlagLacing*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Lacing=%d", uint32( lac ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackMinCache ) )
        {
            KaxTrackMinCache &cmin = *(KaxTrackMinCache*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track MinCache=%d", uint32( cmin ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackMaxCache ) )
        {
            KaxTrackMaxCache &cmax = *(KaxTrackMaxCache*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track MaxCache=%d", uint32( cmax ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackDefaultDuration ) )
        {
            KaxTrackDefaultDuration &defd = *(KaxTrackDefaultDuration*)l;

            tk->i_default_duration = uint64(defd);
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Default Duration="I64Fd, uint64(defd) );
        }
        else  if( MKV_IS_ID( l, KaxTrackTimecodeScale ) )
        {
            KaxTrackTimecodeScale &ttcs = *(KaxTrackTimecodeScale*)l;

            tk->f_timecodescale = float( ttcs );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track TimeCodeScale=%f", tk->f_timecodescale );
        }
        else if( MKV_IS_ID( l, KaxTrackName ) )
        {
            KaxTrackName &tname = *(KaxTrackName*)l;

            tk->fmt.psz_description = UTF8ToStr( UTFstring( tname ) );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Name=%s", tk->fmt.psz_description );
        }
        else  if( MKV_IS_ID( l, KaxTrackLanguage ) )
        {
            KaxTrackLanguage &lang = *(KaxTrackLanguage*)l;

            tk->fmt.psz_language = strdup( string( lang ).c_str() );
            msg_Dbg( &sys.demuxer,
                     "|   |   |   + Track Language=`%s'", tk->fmt.psz_language );
        }
        else  if( MKV_IS_ID( l, KaxCodecID ) )
        {
            KaxCodecID &codecid = *(KaxCodecID*)l;

            tk->psz_codec = strdup( string( codecid ).c_str() );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track CodecId=%s", string( codecid ).c_str() );
        }
        else  if( MKV_IS_ID( l, KaxCodecPrivate ) )
        {
            KaxCodecPrivate &cpriv = *(KaxCodecPrivate*)l;

            tk->i_extra_data = cpriv.GetSize();
            if( tk->i_extra_data > 0 )
            {
                tk->p_extra_data = (uint8_t*)malloc( tk->i_extra_data );
                memcpy( tk->p_extra_data, cpriv.GetBuffer(), tk->i_extra_data );
            }
            msg_Dbg( &sys.demuxer, "|   |   |   + Track CodecPrivate size="I64Fd, cpriv.GetSize() );
        }
        else if( MKV_IS_ID( l, KaxCodecName ) )
        {
            KaxCodecName &cname = *(KaxCodecName*)l;

            tk->psz_codec_name = UTF8ToStr( UTFstring( cname ) );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Name=%s", tk->psz_codec_name );
        }
        else if( MKV_IS_ID( l, KaxContentEncodings ) )
        {
            EbmlMaster *cencs = static_cast<EbmlMaster*>(l);
            MkvTree( sys.demuxer, 3, "Content Encodings" );
            for( unsigned int i = 0; i < cencs->ListSize(); i++ )
            {
                EbmlElement *l2 = (*cencs)[i];
                if( MKV_IS_ID( l2, KaxContentEncoding ) )
                {
                    MkvTree( sys.demuxer, 4, "Content Encoding" );
                    EbmlMaster *cenc = static_cast<EbmlMaster*>(l2);
                    for( unsigned int i = 0; i < cenc->ListSize(); i++ )
                    {
                        EbmlElement *l3 = (*cenc)[i];
                        if( MKV_IS_ID( l3, KaxContentEncodingOrder ) )
                        {
                            KaxContentEncodingOrder &encord = *(KaxContentEncodingOrder*)l3;
                            MkvTree( sys.demuxer, 5, "Order: %i", uint32( encord ) );
                        }
                        else if( MKV_IS_ID( l3, KaxContentEncodingScope ) )
                        {
                            KaxContentEncodingScope &encscope = *(KaxContentEncodingScope*)l3;
                            MkvTree( sys.demuxer, 5, "Scope: %i", uint32( encscope ) );
                        }
                        else if( MKV_IS_ID( l3, KaxContentEncodingType ) )
                        {
                            KaxContentEncodingType &enctype = *(KaxContentEncodingType*)l3;
                            MkvTree( sys.demuxer, 5, "Type: %i", uint32( enctype ) );
                        }
                        else if( MKV_IS_ID( l3, KaxContentCompression ) )
                        {
                            EbmlMaster *compr = static_cast<EbmlMaster*>(l3);
                            MkvTree( sys.demuxer, 5, "Content Compression" );
                            for( unsigned int i = 0; i < compr->ListSize(); i++ )
                            {
                                EbmlElement *l4 = (*compr)[i];
                                if( MKV_IS_ID( l4, KaxContentCompAlgo ) )
                                {
                                    KaxContentCompAlgo &compalg = *(KaxContentCompAlgo*)l4;
                                    MkvTree( sys.demuxer, 6, "Compression Algorithm: %i", uint32(compalg) );
                                    if( uint32( compalg ) == 0 )
                                    {
                                        tk->i_compression_type = MATROSKA_COMPRESSION_ZLIB;
                                    }
                                }
                                else
                                {
                                    MkvTree( sys.demuxer, 6, "Unknown (%s)", typeid(*l4).name() );
                                }
                            }
                        }

                        else
                        {
                            MkvTree( sys.demuxer, 5, "Unknown (%s)", typeid(*l3).name() );
                        }
                    }
                    
                }
                else
                {
                    MkvTree( sys.demuxer, 4, "Unknown (%s)", typeid(*l2).name() );
                }
            }
                
        }
//        else if( EbmlId( *l ) == KaxCodecSettings::ClassInfos.GlobalId )
//        {
//            KaxCodecSettings &cset = *(KaxCodecSettings*)l;

//            tk->psz_codec_settings = UTF8ToStr( UTFstring( cset ) );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Settings=%s", tk->psz_codec_settings );
//        }
//        else if( EbmlId( *l ) == KaxCodecInfoURL::ClassInfos.GlobalId )
//        {
//            KaxCodecInfoURL &ciurl = *(KaxCodecInfoURL*)l;

//            tk->psz_codec_info_url = strdup( string( ciurl ).c_str() );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Info URL=%s", tk->psz_codec_info_url );
//        }
//        else if( EbmlId( *l ) == KaxCodecDownloadURL::ClassInfos.GlobalId )
//        {
//            KaxCodecDownloadURL &cdurl = *(KaxCodecDownloadURL*)l;

//            tk->psz_codec_download_url = strdup( string( cdurl ).c_str() );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Info URL=%s", tk->psz_codec_download_url );
//        }
//        else if( EbmlId( *l ) == KaxCodecDecodeAll::ClassInfos.GlobalId )
//        {
//            KaxCodecDecodeAll &cdall = *(KaxCodecDecodeAll*)l;

//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Decode All=%u <== UNUSED", uint8( cdall ) );
//        }
//        else if( EbmlId( *l ) == KaxTrackOverlay::ClassInfos.GlobalId )
//        {
//            KaxTrackOverlay &tovr = *(KaxTrackOverlay*)l;

//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Overlay=%u <== UNUSED", uint32( tovr ) );
//        }
        else  if( MKV_IS_ID( l, KaxTrackVideo ) )
        {
            EbmlMaster *tkv = static_cast<EbmlMaster*>(l);
            unsigned int j;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Video" );
            tk->f_fps = 0.0;

            for( j = 0; j < tkv->ListSize(); j++ )
            {
                EbmlElement *l = (*tkv)[j];
//                if( EbmlId( *el4 ) == KaxVideoFlagInterlaced::ClassInfos.GlobalId )
//                {
//                    KaxVideoFlagInterlaced &fint = *(KaxVideoFlagInterlaced*)el4;

//                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Track Video Interlaced=%u", uint8( fint ) );
//                }
//                else if( EbmlId( *el4 ) == KaxVideoStereoMode::ClassInfos.GlobalId )
//                {
//                    KaxVideoStereoMode &stereo = *(KaxVideoStereoMode*)el4;

//                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Track Video Stereo Mode=%u", uint8( stereo ) );
//                }
//                else
                if( MKV_IS_ID( l, KaxVideoPixelWidth ) )
                {
                    KaxVideoPixelWidth &vwidth = *(KaxVideoPixelWidth*)l;

                    tk->fmt.video.i_width = uint16( vwidth );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + width=%d", uint16( vwidth ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelHeight ) )
                {
                    KaxVideoPixelWidth &vheight = *(KaxVideoPixelWidth*)l;

                    tk->fmt.video.i_height = uint16( vheight );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + height=%d", uint16( vheight ) );
                }
                else if( MKV_IS_ID( l, KaxVideoDisplayWidth ) )
                {
                    KaxVideoDisplayWidth &vwidth = *(KaxVideoDisplayWidth*)l;

                    tk->fmt.video.i_visible_width = uint16( vwidth );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + display width=%d", uint16( vwidth ) );
                }
                else if( MKV_IS_ID( l, KaxVideoDisplayHeight ) )
                {
                    KaxVideoDisplayWidth &vheight = *(KaxVideoDisplayWidth*)l;

                    tk->fmt.video.i_visible_height = uint16( vheight );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + display height=%d", uint16( vheight ) );
                }
                else if( MKV_IS_ID( l, KaxVideoFrameRate ) )
                {
                    KaxVideoFrameRate &vfps = *(KaxVideoFrameRate*)l;

                    tk->f_fps = float( vfps );
                    msg_Dbg( &sys.demuxer, "   |   |   |   + fps=%f", float( vfps ) );
                }
//                else if( EbmlId( *l ) == KaxVideoDisplayUnit::ClassInfos.GlobalId )
//                {
//                     KaxVideoDisplayUnit &vdmode = *(KaxVideoDisplayUnit*)l;

//                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Track Video Display Unit=%s",
//                             uint8( vdmode ) == 0 ? "pixels" : ( uint8( vdmode ) == 1 ? "centimeters": "inches" ) );
//                }
//                else if( EbmlId( *l ) == KaxVideoAspectRatio::ClassInfos.GlobalId )
//                {
//                    KaxVideoAspectRatio &ratio = *(KaxVideoAspectRatio*)l;

//                    msg_Dbg( &sys.demuxer, "   |   |   |   + Track Video Aspect Ratio Type=%u", uint8( ratio ) );
//                }
//                else if( EbmlId( *l ) == KaxVideoGamma::ClassInfos.GlobalId )
//                {
//                    KaxVideoGamma &gamma = *(KaxVideoGamma*)l;

//                    msg_Dbg( &sys.demuxer, "   |   |   |   + fps=%f", float( gamma ) );
//                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Unknown (%s)", typeid(*l).name() );
                }
            }
            if ( tk->fmt.video.i_visible_height && tk->fmt.video.i_visible_width )
                tk->fmt.video.i_aspect = VOUT_ASPECT_FACTOR * tk->fmt.video.i_visible_width / tk->fmt.video.i_visible_height;
        }
        else  if( MKV_IS_ID( l, KaxTrackAudio ) )
        {
            EbmlMaster *tka = static_cast<EbmlMaster*>(l);
            unsigned int j;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Audio" );

            for( j = 0; j < tka->ListSize(); j++ )
            {
                EbmlElement *l = (*tka)[j];

                if( MKV_IS_ID( l, KaxAudioSamplingFreq ) )
                {
                    KaxAudioSamplingFreq &afreq = *(KaxAudioSamplingFreq*)l;

                    tk->fmt.audio.i_rate = (int)float( afreq );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + afreq=%d", tk->fmt.audio.i_rate );
                }
                else if( MKV_IS_ID( l, KaxAudioChannels ) )
                {
                    KaxAudioChannels &achan = *(KaxAudioChannels*)l;

                    tk->fmt.audio.i_channels = uint8( achan );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + achan=%u", uint8( achan ) );
                }
                else if( MKV_IS_ID( l, KaxAudioBitDepth ) )
                {
                    KaxAudioBitDepth &abits = *(KaxAudioBitDepth*)l;

                    tk->fmt.audio.i_bitspersample = uint8( abits );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + abits=%u", uint8( abits ) );
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Unknown (%s)", typeid(*l).name() );
                }
            }
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   |   + Unknown (%s)",
                     typeid(*l).name() );
        }
    }
}

/*****************************************************************************
 * ParseTracks:
 *****************************************************************************/
void matroska_segment_t::ParseTracks( EbmlElement *tracks )
{
    EbmlElement *el;
    EbmlMaster  *m;
    unsigned int i;
    int i_upper_level = 0;

    msg_Dbg( &sys.demuxer, "|   + Tracks" );

    /* Master elements */
    m = static_cast<EbmlMaster *>(tracks);
    m->Read( es, tracks->Generic().Context, i_upper_level, el, true );

    for( i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxTrackEntry ) )
        {
            ParseTrackEntry( static_cast<EbmlMaster *>(l) );
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
        }
    }
}

/*****************************************************************************
 * ParseInfo:
 *****************************************************************************/
void matroska_segment_t::ParseInfo( EbmlElement *info )
{
    EbmlElement *el;
    EbmlMaster  *m;
    unsigned int i;
    int i_upper_level = 0;

    msg_Dbg( &sys.demuxer, "|   + Information" );

    /* Master elements */
    m = static_cast<EbmlMaster *>(info);
    m->Read( es, info->Generic().Context, i_upper_level, el, true );

    for( i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxSegmentUID ) )
        {
            segment_uid = *(new KaxSegmentUID(*static_cast<KaxSegmentUID*>(l)));

            msg_Dbg( &sys.demuxer, "|   |   + UID=%d", *(uint32*)segment_uid.GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxPrevUID ) )
        {
            prev_segment_uid = *(new KaxPrevUID(*static_cast<KaxPrevUID*>(l)));

            msg_Dbg( &sys.demuxer, "|   |   + PrevUID=%d", *(uint32*)prev_segment_uid.GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxNextUID ) )
        {
            next_segment_uid = *(new KaxNextUID(*static_cast<KaxNextUID*>(l)));

            msg_Dbg( &sys.demuxer, "|   |   + NextUID=%d", *(uint32*)next_segment_uid.GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxTimecodeScale ) )
        {
            KaxTimecodeScale &tcs = *(KaxTimecodeScale*)l;

            i_timescale = uint64(tcs);

            msg_Dbg( &sys.demuxer, "|   |   + TimecodeScale="I64Fd,
                     i_timescale );
        }
        else if( MKV_IS_ID( l, KaxDuration ) )
        {
            KaxDuration &dur = *(KaxDuration*)l;

            f_duration = float(dur);

            msg_Dbg( &sys.demuxer, "|   |   + Duration=%f",
                     f_duration );
        }
        else if( MKV_IS_ID( l, KaxMuxingApp ) )
        {
            KaxMuxingApp &mapp = *(KaxMuxingApp*)l;

            psz_muxing_application = UTF8ToStr( UTFstring( mapp ) );

            msg_Dbg( &sys.demuxer, "|   |   + Muxing Application=%s",
                     psz_muxing_application );
        }
        else if( MKV_IS_ID( l, KaxWritingApp ) )
        {
            KaxWritingApp &wapp = *(KaxWritingApp*)l;

            psz_writing_application = UTF8ToStr( UTFstring( wapp ) );

            msg_Dbg( &sys.demuxer, "|   |   + Writing Application=%s",
                     psz_writing_application );
        }
        else if( MKV_IS_ID( l, KaxSegmentFilename ) )
        {
            KaxSegmentFilename &sfn = *(KaxSegmentFilename*)l;

            psz_segment_filename = UTF8ToStr( UTFstring( sfn ) );

            msg_Dbg( &sys.demuxer, "|   |   + Segment Filename=%s",
                     psz_segment_filename );
        }
        else if( MKV_IS_ID( l, KaxTitle ) )
        {
            KaxTitle &title = *(KaxTitle*)l;

            psz_title = UTF8ToStr( UTFstring( title ) );

            msg_Dbg( &sys.demuxer, "|   |   + Title=%s", psz_title );
        }
        else if( MKV_IS_ID( l, KaxSegmentFamily ) )
        {
            KaxSegmentFamily *uid = static_cast<KaxSegmentFamily*>(l);

            families.push_back(*uid);

            msg_Dbg( &sys.demuxer, "|   |   + family=%d", *(uint32*)uid->GetBuffer() );
        }
#if defined( HAVE_GMTIME_R ) && !defined( SYS_DARWIN )
        else if( MKV_IS_ID( l, KaxDateUTC ) )
        {
            KaxDateUTC &date = *(KaxDateUTC*)l;
            time_t i_date;
            struct tm tmres;
            char   buffer[256];

            i_date = date.GetEpochDate();
            memset( buffer, 0, 256 );
            if( gmtime_r( &i_date, &tmres ) &&
                asctime_r( &tmres, buffer ) )
            {
                buffer[strlen( buffer)-1]= '\0';
                psz_date_utc = strdup( buffer );
                msg_Dbg( &sys.demuxer, "|   |   + Date=%s", psz_date_utc );
            }
        }
#endif
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
        }
    }

    f_duration *= i_timescale / 1000000.0;
}


/*****************************************************************************
 * ParseChapterAtom
 *****************************************************************************/
void matroska_segment_t::ParseChapterAtom( int i_level, EbmlMaster *ca, chapter_item_t & chapters )
{
    unsigned int i;

    if( sys.title == NULL )
    {
        sys.title = vlc_input_title_New();
    }

    msg_Dbg( &sys.demuxer, "|   |   |   + ChapterAtom (level=%d)", i_level );
    for( i = 0; i < ca->ListSize(); i++ )
    {
        EbmlElement *l = (*ca)[i];

        if( MKV_IS_ID( l, KaxChapterUID ) )
        {
            chapters.i_uid = uint64_t(*(KaxChapterUID*)l);
            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterUID: %lld", chapters.i_uid );
        }
        else if( MKV_IS_ID( l, KaxChapterFlagHidden ) )
        {
            KaxChapterFlagHidden &flag =*(KaxChapterFlagHidden*)l;
            chapters.b_display_seekpoint = uint8( flag ) == 0;

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterFlagHidden: %s", chapters.b_display_seekpoint ? "no":"yes" );
        }
        else if( MKV_IS_ID( l, KaxChapterTimeStart ) )
        {
            KaxChapterTimeStart &start =*(KaxChapterTimeStart*)l;
            chapters.i_start_time = uint64( start ) / I64C(1000);

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterTimeStart: %lld", chapters.i_start_time );
        }
        else if( MKV_IS_ID( l, KaxChapterTimeEnd ) )
        {
            KaxChapterTimeEnd &end =*(KaxChapterTimeEnd*)l;
            chapters.i_end_time = uint64( end ) / I64C(1000);

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterTimeEnd: %lld", chapters.i_end_time );
        }
        else if( MKV_IS_ID( l, KaxChapterDisplay ) )
        {
            EbmlMaster *cd = static_cast<EbmlMaster *>(l);
            unsigned int j;

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterDisplay" );
            for( j = 0; j < cd->ListSize(); j++ )
            {
                EbmlElement *l= (*cd)[j];

                if( MKV_IS_ID( l, KaxChapterString ) )
                {
                    int k;

                    KaxChapterString &name =*(KaxChapterString*)l;
                    for (k = 0; k < i_level; k++)
                        chapters.psz_name += '+';
                    chapters.psz_name += ' ';
                    chapters.psz_name += UTF8ToStr( UTFstring( name ) );

                    msg_Dbg( &sys.demuxer, "|   |   |   |   |    + ChapterString '%s'", UTF8ToStr(UTFstring(name)) );
                }
                else if( MKV_IS_ID( l, KaxChapterLanguage ) )
                {
                    KaxChapterLanguage &lang =*(KaxChapterLanguage*)l;
                    const char *psz = string( lang ).c_str();

                    msg_Dbg( &sys.demuxer, "|   |   |   |   |    + ChapterLanguage '%s'", psz );
                }
                else if( MKV_IS_ID( l, KaxChapterCountry ) )
                {
                    KaxChapterCountry &ct =*(KaxChapterCountry*)l;
                    const char *psz = string( ct ).c_str();

                    msg_Dbg( &sys.demuxer, "|   |   |   |   |    + ChapterCountry '%s'", psz );
                }
            }
        }
        else if( MKV_IS_ID( l, KaxChapterAtom ) )
        {
            chapter_item_t new_sub_chapter;
            ParseChapterAtom( i_level+1, static_cast<EbmlMaster *>(l), new_sub_chapter );
            new_sub_chapter.psz_parent = &chapters;
            chapters.sub_chapters.push_back( new_sub_chapter );
        }
    }
}

/*****************************************************************************
 * ParseChapters:
 *****************************************************************************/
void matroska_segment_t::ParseChapters( EbmlElement *chapters )
{
    EbmlElement *el;
    EbmlMaster  *m;
    unsigned int i;
    int i_upper_level = 0;
    float f_dur;

    /* Master elements */
    m = static_cast<EbmlMaster *>(chapters);
    m->Read( es, chapters->Generic().Context, i_upper_level, el, true );

    for( i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxEditionEntry ) )
        {
            chapter_edition_t edition;
            
            EbmlMaster *E = static_cast<EbmlMaster *>(l );
            unsigned int j;
            msg_Dbg( &sys.demuxer, "|   |   + EditionEntry" );
            for( j = 0; j < E->ListSize(); j++ )
            {
                EbmlElement *l = (*E)[j];

                if( MKV_IS_ID( l, KaxChapterAtom ) )
                {
                    chapter_item_t new_sub_chapter;
                    ParseChapterAtom( 0, static_cast<EbmlMaster *>(l), new_sub_chapter );
                    edition.chapters.push_back( new_sub_chapter );
                }
                else if( MKV_IS_ID( l, KaxEditionUID ) )
                {
                    edition.i_uid = uint64(*static_cast<KaxEditionUID *>( l ));
                }
                else if( MKV_IS_ID( l, KaxEditionFlagOrdered ) )
                {
                    edition.b_ordered = config_GetInt( &sys.demuxer, "mkv-use-ordered-chapters" ) ? (uint8(*static_cast<KaxEditionFlagOrdered *>( l )) != 0) : 0;
                }
                else if( MKV_IS_ID( l, KaxEditionFlagDefault ) )
                {
                    if (uint8(*static_cast<KaxEditionFlagDefault *>( l )) != 0)
                        i_default_edition = stored_editions.size();
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   + Unknown (%s)", typeid(*l).name() );
                }
            }
            stored_editions.push_back( edition );
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
        }
    }

    for( i = 0; i < stored_editions.size(); i++ )
    {
        stored_editions[i].RefreshChapters( *sys.title );
    }
    
    if ( stored_editions[i_default_edition].b_ordered )
    {
        /* update the duration of the segment according to the sum of all sub chapters */
        f_dur = stored_editions[i_default_edition].Duration() / I64C(1000);
        if (f_dur > 0.0)
            f_duration = f_dur;
    }
}

void matroska_segment_t::ParseCluster( )
{
    EbmlElement *el;
    EbmlMaster  *m;
    unsigned int i;
    int i_upper_level = 0;

    /* Master elements */
    m = static_cast<EbmlMaster *>( cluster );
    m->Read( es, cluster->Generic().Context, i_upper_level, el, true );

    for( i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxClusterTimecode ) )
        {
            KaxClusterTimecode &ctc = *(KaxClusterTimecode*)l;

            cluster->InitTimecode( uint64( ctc ), i_timescale );
            break;
        }
    }

    f_start_time = cluster->GlobalTimecode() / 1000000.0;
}

/*****************************************************************************
 * InformationCreate:
 *****************************************************************************/
void matroska_segment_t::InformationCreate( )
{
    size_t      i_track;

    sys.meta = vlc_meta_New();

    if( psz_title )
    {
        vlc_meta_Add( sys.meta, VLC_META_TITLE, psz_title );
    }
    if( psz_date_utc )
    {
        vlc_meta_Add( sys.meta, VLC_META_DATE, psz_date_utc );
    }
    if( psz_segment_filename )
    {
        vlc_meta_Add( sys.meta, _("Segment filename"), psz_segment_filename );
    }
    if( psz_muxing_application )
    {
        vlc_meta_Add( sys.meta, _("Muxing application"), psz_muxing_application );
    }
    if( psz_writing_application )
    {
        vlc_meta_Add( sys.meta, _("Writing application"), psz_writing_application );
    }

    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
        mkv_track_t *tk = tracks[i_track];
        vlc_meta_t *mtk = vlc_meta_New();

        sys.meta->track = (vlc_meta_t**)realloc( sys.meta->track,
                                                    sizeof( vlc_meta_t * ) * ( sys.meta->i_track + 1 ) );
        sys.meta->track[sys.meta->i_track++] = mtk;

        if( tk->fmt.psz_description )
        {
            vlc_meta_Add( sys.meta, VLC_META_DESCRIPTION, tk->fmt.psz_description );
        }
        if( tk->psz_codec_name )
        {
            vlc_meta_Add( sys.meta, VLC_META_CODEC_NAME, tk->psz_codec_name );
        }
        if( tk->psz_codec_settings )
        {
            vlc_meta_Add( sys.meta, VLC_META_SETTING, tk->psz_codec_settings );
        }
        if( tk->psz_codec_info_url )
        {
            vlc_meta_Add( sys.meta, VLC_META_CODEC_DESCRIPTION, tk->psz_codec_info_url );
        }
        if( tk->psz_codec_download_url )
        {
            vlc_meta_Add( sys.meta, VLC_META_URL, tk->psz_codec_download_url );
        }
    }

    if( i_tags_position >= 0 )
    {
        vlc_bool_t b_seekable;

        stream_Control( sys.demuxer.s, STREAM_CAN_FASTSEEK, &b_seekable );
        if( b_seekable )
        {
            LoadTags( );
        }
    }
}


/*****************************************************************************
 * Divers
 *****************************************************************************/

void matroska_segment_t::IndexAppendCluster( KaxCluster *cluster )
{
#define idx index[i_index]
    idx.i_track       = -1;
    idx.i_block_number= -1;
    idx.i_position    = cluster->GetElementPosition();
    idx.i_time        = -1;
    idx.b_key         = VLC_TRUE;

    i_index++;
    if( i_index >= i_index_max )
    {
        i_index_max += 1024;
        index = (mkv_index_t*)realloc( index, sizeof( mkv_index_t ) * i_index_max );
    }
#undef idx
}

static char * UTF8ToStr( const UTFstring &u )
{
    int     i_src;
    const wchar_t *src;
    char *dst, *p;

    i_src = u.length();
    src   = u.c_str();

    p = dst = (char*)malloc( i_src + 1);
    while( i_src > 0 )
    {
        if( *src < 255 )
        {
            *p++ = (char)*src;
        }
        else
        {
            *p++ = '?';
        }
        src++;
        i_src--;
    }
    *p++= '\0';

    return dst;
}

void chapter_edition_t::RefreshChapters( input_title_t & title )
{
    int64_t i_prev_user_time = 0;
    std::vector<chapter_item_t>::iterator index = chapters.begin();

    while ( index != chapters.end() )
    {
        i_prev_user_time = (*index).RefreshChapters( b_ordered, i_prev_user_time, title );
        index++;
    }
}

int64_t chapter_item_t::RefreshChapters( bool b_ordered, int64_t i_prev_user_time, input_title_t & title )
{
    int64_t i_user_time = i_prev_user_time;
    
    // first the sub-chapters, and then ourself
    std::vector<chapter_item_t>::iterator index = sub_chapters.begin();
    while ( index != sub_chapters.end() )
    {
        i_user_time = (*index).RefreshChapters( b_ordered, i_user_time, title );
        index++;
    }

    if ( b_ordered )
    {
        i_user_start_time = i_prev_user_time;
        if ( i_end_time != -1 && i_user_time == i_prev_user_time )
        {
            i_user_end_time = i_user_start_time - i_start_time + i_end_time;
        }
        else
        {
            i_user_end_time = i_user_time;
        }
    }
    else
    {
        std::sort( sub_chapters.begin(), sub_chapters.end() );
        i_user_start_time = i_start_time;
        i_user_end_time = i_end_time;
    }

    if (b_display_seekpoint)
    {
        seekpoint_t *sk = vlc_seekpoint_New();

//        sk->i_level = i_level;
        sk->i_time_offset = i_start_time;
        sk->psz_name = strdup( psz_name.c_str() );

        // A start time of '0' is ok. A missing ChapterTime element is ok, too, because '0' is its default value.
        title.i_seekpoint++;
        title.seekpoint = (seekpoint_t**)realloc( title.seekpoint, title.i_seekpoint * sizeof( seekpoint_t* ) );
        title.seekpoint[title.i_seekpoint-1] = sk;
    }

    i_seekpoint_num = title.i_seekpoint;

    return i_user_end_time;
}

double chapter_edition_t::Duration() const
{
    double f_result = 0.0;
    
    if ( chapters.size() )
    {
        std::vector<chapter_item_t>::const_iterator index = chapters.end();
        index--;
        f_result = (*index).i_user_end_time;
    }
    
    return f_result;
}

const chapter_item_t *chapter_item_t::FindTimecode( mtime_t i_user_timecode ) const
{
    const chapter_item_t *psz_result = NULL;

    if (i_user_timecode >= i_user_start_time && i_user_timecode < i_user_end_time)
    {
        std::vector<chapter_item_t>::const_iterator index = sub_chapters.begin();
        while ( index != sub_chapters.end() && psz_result == NULL )
        {
            psz_result = (*index).FindTimecode( i_user_timecode );
            index++;
        }
        
        if ( psz_result == NULL )
            psz_result = this;
    }

    return psz_result;
}

const chapter_item_t *chapter_edition_t::FindTimecode( mtime_t i_user_timecode ) const
{
    const chapter_item_t *psz_result = NULL;

    std::vector<chapter_item_t>::const_iterator index = chapters.begin();
    while ( index != chapters.end() && psz_result == NULL )
    {
        psz_result = (*index).FindTimecode( i_user_timecode );
        index++;
    }

    return psz_result;
}

void demux_sys_t::PreloadFamily( )
{
/* TODO enable family handling again
    matroska_stream_t *p_stream = Stream();
    if ( p_stream )
    {
        matroska_segment_t *p_segment = p_stream->Segment();
        if ( p_segment )
        {
            for (size_t i=0; i<streams.size(); i++)
            {
                streams[i]->PreloadFamily( *p_segment );
            }
        }
    }
*/
}

void matroska_stream_t::PreloadFamily( const matroska_segment_t & of_segment )
{
    for (size_t i=0; i<segments.size(); i++)
    {
        segments[i]->PreloadFamily( of_segment );
    }
}

bool matroska_segment_t::PreloadFamily( const matroska_segment_t & of_segment )
{
    if ( b_preloaded )
        return false;

    for (size_t i=0; i<families.size(); i++)
    {
        for (size_t j=0; j<of_segment.families.size(); j++)
        {
            if ( families[i] == of_segment.families[j] )
                return Preload( );
        }
    }

    return false;
}

// preload all the linked segments for all preloaded segments
void demux_sys_t::PreloadLinked( matroska_segment_t *p_segment )
{
    size_t i_preloaded, i;

    delete p_current_segment;
    p_current_segment = new virtual_segment_t( p_segment );

    // fill our current virtual segment with all hard linked segments
    do {
        i_preloaded = 0;
        for ( i=0; i< opened_segments.size(); i++ )
        {
            i_preloaded += p_current_segment->AddSegment( opened_segments[i] );
        }
    } while ( i_preloaded ); // worst case: will stop when all segments are found as linked

    p_current_segment->Sort( );

    p_current_segment->PreloadLinked( );
}

void demux_sys_t::PreparePlayback( )
{
    p_current_segment->LoadCues();
    f_duration = p_current_segment->Duration();
}

bool matroska_segment_t::CompareSegmentUIDs( const matroska_segment_t * p_item_a, const matroska_segment_t * p_item_b )
{
    EbmlBinary * p_itema = (EbmlBinary *)(&p_item_a->segment_uid);
    if ( *p_itema == p_item_b->prev_segment_uid )
        return true;

    p_itema = (EbmlBinary *)(&p_item_a->next_segment_uid);
    if ( *p_itema == p_item_b->segment_uid )
        return true;

    if ( *p_itema == p_item_b->prev_segment_uid )
        return true;

    return false;
}

bool matroska_segment_t::Preload( )
{
    if ( b_preloaded )
        return false;

    EbmlElement *el = NULL;

    ep->Reset();

    while( ( el = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( el, KaxInfo ) )
        {
            ParseInfo( el );
        }
        else if( MKV_IS_ID( el, KaxTracks ) )
        {
            ParseTracks( el );
        }
        else if( MKV_IS_ID( el, KaxSeekHead ) )
        {
            ParseSeekHead( el );
        }
        else if( MKV_IS_ID( el, KaxCues ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Cues" );
        }
        else if( MKV_IS_ID( el, KaxCluster ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Cluster" );

            cluster = (KaxCluster*)el;

            i_start_pos = cluster->GetElementPosition();
            ParseCluster( );

            ep->Down();
            /* stop parsing the stream */
            break;
        }
        else if( MKV_IS_ID( el, KaxAttachments ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Attachments FIXME TODO (but probably never supported)" );
        }
        else if( MKV_IS_ID( el, KaxChapters ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Chapters" );
            ParseChapters( el );
        }
        else if( MKV_IS_ID( el, KaxTag ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Tags FIXME TODO" );
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   + Unknown (%s)", typeid(*el).name() );
        }
    }

    b_preloaded = true;

    return true;
}

matroska_segment_t *demux_sys_t::FindSegment( const EbmlBinary & uid ) const
{
    for (size_t i=0; i<opened_segments.size(); i++)
    {
        if ( opened_segments[i]->segment_uid == uid )
            return opened_segments[i];
    }
    return NULL;
}

void virtual_segment_t::Sort()
{
    // keep the current segment index
    matroska_segment_t *p_segment = linked_segments[i_current_segment];

    std::sort( linked_segments.begin(), linked_segments.end(), matroska_segment_t::CompareSegmentUIDs );

    for ( i_current_segment=0; i_current_segment<linked_segments.size(); i_current_segment++)
        if ( linked_segments[i_current_segment] == p_segment )
            break;
}

size_t virtual_segment_t::AddSegment( matroska_segment_t *p_segment )
{
    size_t i;
    // check if it's not already in here
    for ( i=0; i<linked_segments.size(); i++ )
    {
        if ( p_segment->segment_uid == linked_segments[i]->segment_uid )
            return 0;
    }

    // find possible mates
    for ( i=0; i<linked_uids.size(); i++ )
    {
        if (   p_segment->segment_uid == linked_uids[i] 
            || p_segment->prev_segment_uid == linked_uids[i] 
            || p_segment->next_segment_uid == linked_uids[i] )
        {
            linked_segments.push_back( p_segment );

            AppendUID( p_segment->prev_segment_uid );
            AppendUID( p_segment->next_segment_uid );

            return 1;
        }
    }
    return 0;
}

void virtual_segment_t::PreloadLinked( )
{
    for ( size_t i=0; i<linked_segments.size(); i++ )
    {
        linked_segments[i]->Preload( );
    }
}

float virtual_segment_t::Duration() const
{
    float f_duration;
    if ( linked_segments.size() == 0 )
        f_duration = 0.0;
    else {
        matroska_segment_t *p_last_segment = linked_segments[linked_segments.size()-1];
//        p_last_segment->ParseCluster( );

        f_duration = p_last_segment->f_start_time + p_last_segment->f_duration;
    }
    return f_duration;
}

void virtual_segment_t::LoadCues( )
{
    for ( size_t i=0; i<linked_segments.size(); i++ )
    {
        linked_segments[i]->LoadCues();
    }
}

void virtual_segment_t::AppendUID( const EbmlBinary & UID )
{
    if ( UID.GetBuffer() == NULL )
        return;

    for (size_t i=0; i<linked_uids.size(); i++)
    {
        if ( UID == linked_uids[i] )
            return;
    }
    linked_uids.push_back( *(KaxSegmentUID*)(&UID) );
}

void matroska_segment_t::Seek( mtime_t i_date, mtime_t i_time_offset )
{
    KaxBlock    *block;
    int         i_track_skipping;
    int64_t     i_block_duration;
    int64_t     i_block_ref1;
    int64_t     i_block_ref2;
    size_t      i_track;
    int64_t     i_seek_position = i_start_pos;
    int64_t     i_seek_time = f_start_time * 1000;

    if ( i_index > 0 )
    {
        int i_idx = 0;

        for( ; i_idx < i_index; i_idx++ )
        {
            if( index[i_idx].i_time + i_time_offset > i_date )
            {
                break;
            }
        }

        if( i_idx > 0 )
        {
            i_idx--;
        }

        i_seek_position = index[i_idx].i_position;
        i_seek_time = index[i_idx].i_time;
    }

    msg_Dbg( &sys.demuxer, "seek got "I64Fd" (%d%%)",
                i_seek_time, (int)( 100 * i_seek_position / stream_Size( sys.demuxer.s ) ) );

    delete ep;
    ep = new EbmlParser( &es, segment );
    cluster = NULL;

    es.I_O().setFilePointer( i_seek_position, seek_beginning );

    sys.i_start_pts = i_date;

    es_out_Control( sys.demuxer.out, ES_OUT_RESET_PCR );

    /* now parse until key frame */
#define tk  tracks[i_track]
    i_track_skipping = 0;
    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
        if( tk->fmt.i_cat == VIDEO_ES )
        {
            tk->b_search_keyframe = VLC_TRUE;
            i_track_skipping++;
        }
        es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, tk->p_es, i_date );
    }


    while( i_track_skipping > 0 )
    {
        if( BlockGet( &block, &i_block_ref1, &i_block_ref2, &i_block_duration ) )
        {
            msg_Warn( &sys.demuxer, "cannot get block EOF?" );

            return;
        }

        for( i_track = 0; i_track < tracks.size(); i_track++ )
        {
            if( tk->i_number == block->TrackNum() )
            {
                break;
            }
        }

        sys.i_pts = sys.i_chapter_time + block->GlobalTimecode() / (mtime_t) 1000;

        if( i_track < tracks.size() )
        {
            if( sys.i_pts >= sys.i_start_pts )
            {
                BlockDecode( &sys.demuxer, block, sys.i_pts, 0 );
                i_track_skipping = 0;
            }
            else if( tk->fmt.i_cat == VIDEO_ES )
            {
                if( i_block_ref1 == -1 && tk->b_search_keyframe )
                {
                    tk->b_search_keyframe = VLC_FALSE;
                    i_track_skipping--;
                }
                if( !tk->b_search_keyframe )
                {
                    BlockDecode( &sys.demuxer, block, sys.i_pts, 0 );
                }
            } 
        }

        delete block;
    }
#undef tk
}

void virtual_segment_t::Seek( demux_t & demuxer, mtime_t i_date, mtime_t i_time_offset, const chapter_item_t *psz_chapter )
{
    demux_sys_t *p_sys = demuxer.p_sys;
    size_t i;

    // find the actual time for an ordered edition
    if ( psz_chapter == NULL )
    {
        if ( EditionIsOrdered() )
        {
            /* 1st, we need to know in which chapter we are */
            psz_chapter = editions[i_current_edition].FindTimecode( i_date );
        }
    }

    if ( psz_chapter != NULL )
    {
        psz_current_chapter = psz_chapter;
        p_sys->i_chapter_time = i_time_offset = psz_chapter->i_user_start_time - psz_chapter->i_start_time;
        demuxer.info.i_update |= INPUT_UPDATE_SEEKPOINT;
        demuxer.info.i_seekpoint = psz_chapter->i_seekpoint_num - 1;
    }

    // find the best matching segment
    for ( i=0; i<linked_segments.size(); i++ )
    {
        if ( i_date < linked_segments[i]->f_start_time * 1000.0 )
            break;
    }

    if ( i > 0 )
        i--;

    if ( i_current_segment != i  )
    {
        linked_segments[i_current_segment]->UnSelect();
        linked_segments[i]->Select( i_date );
        i_current_segment = i;
    }

    linked_segments[i]->Seek( i_date, i_time_offset );
}
