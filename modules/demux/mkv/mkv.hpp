/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2005, 2008 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _MKV_H_
#define _MKV_H_

/*****************************************************************************
 * Preamble
 *****************************************************************************/


/* config.h may include inttypes.h, so make sure we define that option
 * early enough. */
#define __STDC_FORMAT_MACROS 1
#define __STDC_CONSTANT_MACROS 1
#define __STDC_LIMIT_MACROS 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <inttypes.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#ifdef HAVE_TIME_H
#   include <time.h>                                               /* time() */
#endif

#include <vlc_meta.h>
#include <vlc_charset.h>
#include <vlc_input.h>
#include <vlc_demux.h>

#include <iostream>
#include <cassert>
#include <typeinfo>
#include <string>
#include <vector>
#include <algorithm>

/* libebml and matroska */
#include "ebml/EbmlHead.h"
#include "ebml/EbmlSubHead.h"
#include "ebml/EbmlStream.h"
#include "ebml/EbmlContexts.h"
#include "ebml/EbmlVoid.h"
#include "ebml/EbmlVersion.h"
#include "ebml/StdIOCallback.h"

#include "matroska/KaxAttachments.h"
#include "matroska/KaxAttached.h"
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
//#include "matroska/KaxTagMulti.h"
#include "matroska/KaxTracks.h"
#include "matroska/KaxTrackAudio.h"
#include "matroska/KaxTrackVideo.h"
#include "matroska/KaxTrackEntryData.h"
#include "matroska/KaxContentEncoding.h"
#include "matroska/KaxVersion.h"

#include "ebml/StdIOCallback.h"

#include <vlc_keys.h>

extern "C" {
   #include "../mp4/libmp4.h"
}
#ifdef HAVE_ZLIB_H
#   include <zlib.h>
#endif

#define MKV_DEBUG 0

#define MATROSKA_COMPRESSION_NONE  -1
#define MATROSKA_COMPRESSION_ZLIB   0
#define MATROSKA_COMPRESSION_BLIB   1
#define MATROSKA_COMPRESSION_LZOX   2
#define MATROSKA_COMPRESSION_HEADER 3

#define MKVD_TIMECODESCALE 1000000

#define MKV_IS_ID( el, C ) ( el != NULL && typeid( *el ) == typeid( C ) )


using namespace LIBMATROSKA_NAMESPACE;
using namespace std;

void BlockDecode( demux_t *p_demux, KaxBlock *block, KaxSimpleBlock *simpleblock,
                         mtime_t i_pts, mtime_t i_duration, bool f_mandatory );

class attachment_c
{
public:
    attachment_c( const std::string& _psz_file_name, const std::string& _psz_mime_type, int _i_size )
        :i_size(_i_size)
        ,psz_file_name( _psz_file_name)
        ,psz_mime_type( _psz_mime_type)
    {
        p_data = NULL;
    }
    virtual ~attachment_c() { free( p_data ); }

    /* Allocs the data space. Returns true if allocation went ok */
    bool init()
    {
        p_data = malloc( i_size );
        return (p_data != NULL);
    }

    const char* fileName() const { return psz_file_name.c_str(); }
    const char* mimeType() const { return psz_mime_type.c_str(); }
    int         size() const    { return i_size; }

    void          *p_data;
private:
    int            i_size;
    std::string    psz_file_name;
    std::string    psz_mime_type;
};

class matroska_segment_c;
class matroska_stream_c
{
public:
    matroska_stream_c() :p_io_callback(NULL) ,p_estream(NULL) {}
    virtual ~matroska_stream_c()
    {
        delete p_io_callback;
        delete p_estream;
    }

    IOCallback         *p_io_callback;
    EbmlStream         *p_estream;

    std::vector<matroska_segment_c*> segments;
};


/*****************************************************************************
 * definitions of structures and functions used by this plugins
 *****************************************************************************/
typedef struct
{
//    ~mkv_track_t();

    bool         b_default;
    bool         b_enabled;
    unsigned int i_number;

    unsigned int i_extra_data;
    uint8_t      *p_extra_data;

    char         *psz_codec;
    bool         b_dts_only;
    bool         b_pts_only;

    uint64_t     i_default_duration;
    float        f_timecodescale;
    mtime_t      i_last_dts;

    /* video */
    es_format_t fmt;
    float       f_fps;
    es_out_id_t *p_es;

    /* audio */
    unsigned int i_original_rate;

    bool            b_inited;
    /* data to be send first */
    int             i_data_init;
    uint8_t         *p_data_init;

    /* hack : it's for seek */
    bool            b_search_keyframe;
    bool            b_silent;

    /* informative */
    const char   *psz_codec_name;
    const char   *psz_codec_settings;
    const char   *psz_codec_info_url;
    const char   *psz_codec_download_url;

    /* encryption/compression */
    int                    i_compression_type;
    KaxContentCompSettings *p_compression_data;

} mkv_track_t;

typedef struct
{
    int     i_track;
    int     i_block_number;

    int64_t i_position;
    int64_t i_time;

    bool       b_key;
} mkv_index_t;


#endif /* _MKV_HPP_ */
