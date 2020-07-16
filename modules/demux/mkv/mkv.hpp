/*****************************************************************************
 * mkv.hpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2005, 2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#ifndef VLC_MKV_MKV_HPP_
#define VLC_MKV_MKV_HPP_

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <inttypes.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <time.h>

#include <vlc_meta.h>
#include <vlc_charset.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_aout.h> /* For reordering */

#include <iostream>
#include <cassert>
#include <typeinfo>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <stdexcept>

/* libebml and matroska */
#include "ebml/EbmlHead.h"
#include "ebml/EbmlSubHead.h"
#include "ebml/EbmlStream.h"
#include "ebml/EbmlContexts.h"
#include "ebml/EbmlVoid.h"
#include "ebml/EbmlVersion.h"

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
#include "matroska/KaxTracks.h"
#include "matroska/KaxTrackAudio.h"
#include "matroska/KaxTrackVideo.h"
#include "matroska/KaxTrackEntryData.h"
#include "matroska/KaxContentEncoding.h"
#include "matroska/KaxVersion.h"

#include "stream_io_callback.hpp"

#ifdef HAVE_ZLIB_H
#   include <zlib.h>
#endif

#ifndef NDEBUG
//# define MKV_DEBUG 0
#endif

namespace mkv {

#define MATROSKA_COMPRESSION_NONE  -1
#define MATROSKA_COMPRESSION_ZLIB   0
#define MATROSKA_COMPRESSION_BLIB   1
#define MATROSKA_COMPRESSION_LZOX   2
#define MATROSKA_COMPRESSION_HEADER 3

enum
{
    MATROSKA_ENCODING_SCOPE_ALL_FRAMES = 1,
    MATROSKA_ENCODING_SCOPE_PRIVATE = 2,
    MATROSKA_ENCODING_SCOPE_NEXT = 4 /* unsupported */
};

#define MKVD_TIMECODESCALE 1000000

#define MKV_IS_ID( el, C ) ( el != NULL && (el->operator const EbmlId&()) == (C::ClassInfos.ClassId()) )
#define MKV_CHECKED_PTR_DECL( name, type, src ) type * name = MKV_IS_ID(src, type) ? static_cast<type*>(src) : NULL
#define MKV_CHECKED_PTR_DECL_CONST( name, type, src ) const type * name = MKV_IS_ID(src, type) ? static_cast<const type*>(src) : NULL


using namespace LIBMATROSKA_NAMESPACE;

void BlockDecode( demux_t *p_demux, KaxBlock *block, KaxSimpleBlock *simpleblock,
                  KaxBlockAdditions *additions,
                  vlc_tick_t i_pts, vlc_tick_t i_duration, bool b_key_picture,
                  bool b_discardable_picture );

class attachment_c
{
public:
    attachment_c( const std::string& _str_file_name, const std::string& _str_mime_type, int _i_size )
        :i_size(_i_size)
        ,str_file_name( _str_file_name)
        ,str_mime_type( _str_mime_type)
    {
        p_data = NULL;
    }
    ~attachment_c() { free( p_data ); }

    /* Allocs the data space. Returns true if allocation went ok */
    bool init()
    {
        p_data = malloc( i_size );
        return (p_data != NULL);
    }

    const char* fileName() const { return str_file_name.c_str(); }
    const char* mimeType() const { return str_mime_type.c_str(); }
    int         size() const    { return i_size; }

    void          *p_data;
private:
    int            i_size;
    std::string    str_file_name;
    std::string    str_mime_type;
};

class matroska_segment_c;
struct matroska_stream_c
{
    matroska_stream_c(stream_t *s, bool owner);
    ~matroska_stream_c() {}

    bool isUsed() const;

    vlc_stream_io_callback io_callback;
    EbmlStream         estream;

    std::vector<matroska_segment_c*> segments;
};


/*****************************************************************************
 * definitions of structures and functions used by this plugins
 *****************************************************************************/
class PrivateTrackData
{
public:
    virtual ~PrivateTrackData() {}
    virtual int32_t Init() { return 0; }
};

class mkv_track_t
{
    public:
        mkv_track_t(enum es_format_category_e es_cat);
        ~mkv_track_t();

        typedef unsigned int track_id_t;

        bool         b_default;
        bool         b_enabled;
        bool         b_forced;
        track_id_t   i_number;

        unsigned int i_extra_data;
        uint8_t      *p_extra_data;

        std::string  codec;
        bool         b_dts_only;
        bool         b_pts_only;

        bool         b_no_duration;
        vlc_tick_t   i_default_duration;
        float        f_timecodescale;
        vlc_tick_t   i_last_dts;
        uint64_t     i_skip_until_fpos; /*< any block before this fpos should be ignored */

        /* video */
        es_format_t fmt;
        float       f_fps;
        es_out_id_t *p_es;

        /* audio */
        unsigned int i_original_rate;
        uint8_t i_chans_to_reorder;            /* do we need channel reordering */
        uint8_t pi_chan_table[AOUT_CHAN_MAX];


        /* Private track paramters */
        PrivateTrackData *p_sys;

        bool            b_discontinuity;

        /* informative */
        std::string str_codec_name;

        /* encryption/compression */
        int                    i_compression_type;
        uint32_t               i_encoding_scope;
        KaxContentCompSettings *p_compression_data;

        /* Matroska 4 new elements used by Opus */
        vlc_tick_t i_seek_preroll;
        vlc_tick_t i_codec_delay;
};

} // namespace

#endif /* _MKV_HPP_ */
